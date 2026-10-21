// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UpdateSettings.h"
#include "model/JsonParse.h" // p63: depth-limited parse
#include "../model/FormatVersions.h"

#include <nlohmann/json.hpp>

#include <algorithm>

using nlohmann::json;

namespace ohpe::update
{
Settings Settings::defaults (const Version& current)
{
    Settings s;
    s.channel = current.major < 1 || current.isPreRelease() ? Channel::Preview : Channel::Stable;
    return s;
}

std::string Settings::toJson() const
{
    json j;
    j["version"] = formats::updateSettings; // p62 (files before 1.0 have none; nothing to migrate)
    j["autoCheck"] = autoCheck;
    j["channel"] = channelName (channel);
    j["lastCheck"] = lastCheck;
    j["skipped"] = skipped;
    j["remindVersion"] = remindVersion;
    j["remindAfter"] = remindAfter;
    return j.dump (2);
}

Settings Settings::fromJson (const std::string& text, const Settings& base)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    Settings s = base;
    const json j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object()) return s;
    auto num = [&j] (const char* k, int64_t fallback) {
        const auto it = j.find (k);
        return it != j.end() && it->is_number_integer() ? it->get<int64_t>() : fallback;
    };
    auto str = [&j] (const char* k, const std::string& fallback) {
        const auto it = j.find (k);
        return it != j.end() && it->is_string() ? it->get<std::string>() : fallback;
    };
    if (const auto it = j.find ("autoCheck"); it != j.end() && it->is_boolean()) s.autoCheck = it->get<bool>();
    if (const auto it = j.find ("channel"); it != j.end() && it->is_string())
    {
        const auto c = it->get<std::string>();
        if (c == "stable" || c == "preview") s.channel = channelFromName (c);
    }
    s.lastCheck = std::max<int64_t> (0, num ("lastCheck", s.lastCheck));
    s.remindAfter = std::max<int64_t> (0, num ("remindAfter", s.remindAfter));
    s.skipped = str ("skipped", s.skipped);
    s.remindVersion = str ("remindVersion", s.remindVersion);
    return s;
}
catch (const std::exception&)
{
    return base;
}

bool autoCheckDue (const Settings& s, int64_t now, bool playing)
{
    if (! s.autoCheck || playing) return false;
    if (s.lastCheck > now) return true; // the clock went back: check again rather than never
    return now - s.lastCheck >= secondsPerDay;
}

bool shouldOffer (const Settings& s, const Version& version, int64_t now)
{
    if (! version.valid()) return false;
    if (! s.skipped.empty() && Version::of (s.skipped) == version) return false;
    if (! s.remindVersion.empty() && Version::of (s.remindVersion) == version && now < s.remindAfter) return false;
    return true;
}

void remindLater (Settings& s, const Version& version, int64_t now)
{
    s.remindVersion = version.toString();
    s.remindAfter = now + remindDelaySeconds;
}

void skipVersion (Settings& s, const Version& version) { s.skipped = version.toString(); }
} // namespace ohpe::update
