// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ContentPacks.h"
#include "../model/FormatVersions.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

using nlohmann::json;

namespace ohpe::update
{
const InstalledPack* ContentRegistry::find (const std::string& id) const
{
    for (const auto& p : packs)
        if (p.id == id) return &p;
    return nullptr;
}

void ContentRegistry::installed (const std::string& id, const std::string& version)
{
    for (auto& p : packs)
        if (p.id == id)
        {
            if (p.version != version) p.previousVersion = p.version;
            p.version = version;
            return;
        }
    packs.push_back ({ id, version, {} });
}

bool ContentRegistry::rolledBack (const std::string& id)
{
    for (auto& p : packs)
        if (p.id == id && ! p.previousVersion.empty())
        {
            p.version = p.previousVersion;
            p.previousVersion.clear();
            return true;
        }
    return false;
}

void ContentRegistry::removed (const std::string& id)
{
    packs.erase (std::remove_if (packs.begin(), packs.end(), [&id] (const InstalledPack& p) { return p.id == id; }), packs.end());
}

std::string ContentRegistry::toJson() const
{
    json j;
    j["format"] = formats::installedPacks;
    j["packs"] = json::array();
    for (const auto& p : packs) j["packs"].push_back ({ { "id", p.id }, { "version", p.version }, { "previous", p.previousVersion } });
    return j.dump (2);
}

ContentRegistry ContentRegistry::fromJson (const std::string& text)
{
    ContentRegistry r;
    const json j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || ! j.contains ("packs") || ! j["packs"].is_array()) return r;
    for (const auto& o : j["packs"])
    {
        if (! o.is_object()) continue;
        InstalledPack p;
        p.id = o.value ("id", std::string());
        p.version = o.value ("version", std::string());
        p.previousVersion = o.value ("previous", std::string());
        if (isSafePackId (p.id) && Version::of (p.version).valid() && r.find (p.id) == nullptr) r.packs.push_back (std::move (p));
    }
    return r;
}

bool isSafePackId (const std::string& id)
{
    if (id.empty() || id.size() > 64 || id[0] == '.') return false;
    return std::all_of (id.begin(), id.end(), [] (char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    });
}

bool isSafeEntryPath (const std::string& pathIn)
{
    if (pathIn.empty() || pathIn.size() > 512) return false;
    std::string path = pathIn;
    std::replace (path.begin(), path.end(), '\\', '/');
    if (path[0] == '/') return false;
    if (path.size() >= 2 && path[1] == ':') return false; // C:...
    if (path.find ('\0') != std::string::npos) return false;
    size_t start = 0;
    while (start <= path.size())
    {
        auto end = path.find ('/', start);
        if (end == std::string::npos) end = path.size();
        const auto part = path.substr (start, end - start);
        if (part == "..") return false;
        if (part.empty() && end != path.size()) return false; // "a//b"
        if (part == "." ) return false;
        for (char c : part)
            if ((unsigned char) c < 32 || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
        start = end + 1;
    }
    return true;
}

PackState packState (const ContentPack& offered, const ContentRegistry& reg, const Version& app)
{
    if (! offered.minApp.empty())
    {
        const auto need = Version::of (offered.minApp);
        if (need.valid() && app < need) return PackState::NeedsNewerApp;
    }
    const auto* inst = reg.find (offered.id);
    if (inst == nullptr) return PackState::NotInstalled;
    return Version::of (offered.version) > Version::of (inst->version) ? PackState::UpdateAvailable : PackState::Installed;
}
} // namespace ohpe::update
