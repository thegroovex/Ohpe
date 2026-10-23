// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UpdateManifest.h"
#include "model/JsonParse.h" // p63: depth-limited parse

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

using nlohmann::json;

namespace ohpe::update
{
namespace
{
std::string getString (const json& o, const char* key)
{
    if (! o.is_object()) return {};
    const auto it = o.find (key);
    return it != o.end() && it->is_string() ? it->get<std::string>() : std::string();
}

uint64_t getSize (const json& o, const char* key)
{
    if (! o.is_object()) return 0;
    const auto it = o.find (key);
    if (it == o.end()) return 0;
    if (it->is_number_unsigned()) return it->get<uint64_t>();
    if (it->is_number_integer()) { const auto v = it->get<int64_t>(); return v > 0 ? (uint64_t) v : 0; }
    return 0;
}

std::string lower (std::string s)
{
    for (auto& c : s) c = (char) std::tolower ((unsigned char) c);
    return s;
}

bool contains (const std::string& s, const char* what) { return lower (s).find (lower (what)) != std::string::npos; }

bool endsWith (const std::string& s, const char* tail)
{
    const std::string t = lower (tail), l = lower (s);
    return l.size() >= t.size() && l.compare (l.size() - t.size(), t.size(), t) == 0;
}

std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}

/** Platform / kind / part of a release asset from its file name (releases without a manifest). */
bool classify (const std::string& name, Asset& a)
{
    if (endsWith (name, ".dmg") || endsWith (name, ".pkg"))
    {
        a.platform = "macos"; a.kind = "installer"; a.part = "all";
        return true;
    }
    if (contains (name, "windows") || endsWith (name, ".exe") || endsWith (name, ".msi"))
    {
        a.platform = "windows";
        a.kind = endsWith (name, ".exe") || endsWith (name, ".msi") ? "installer" : "archive";
        a.part = contains (name, "vst3") ? "plugin" : contains (name, "-app") ? "app" : "all";
        return true;
    }
    if (contains (name, "linux"))
    {
        a.platform = "linux"; a.kind = "archive"; a.part = "all";
        return true;
    }
    return false;
}
} // namespace

const char* channelName (Channel c) noexcept { return c == Channel::Preview ? "preview" : "stable"; }
Channel channelFromName (const std::string& s) noexcept { return lower (s) == "preview" ? Channel::Preview : Channel::Stable; }

const char* currentPlatform() noexcept
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

const Asset* Release::assetFor (const std::string& platform, const std::string& part) const
{
    const Asset* best = nullptr;
    int bestScore = -1;
    for (const auto& a : assets)
    {
        if (a.platform != platform || a.url.empty()) continue;
        int score = -1;
        if (a.part == part) score = 2;
        else if (a.part == "all" || a.part.empty()) score = 1;
        if (score < 0) continue;
        score = score * 2 + (a.kind == "installer" ? 1 : 0);
        if (score > bestScore) { best = &a; bestScore = score; }
    }
    return best;
}

bool parseManifest (const std::string& text, Release& out, std::string* error)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    auto fail = [error] (const char* msg) { if (error != nullptr) *error = msg; return false; };
    const json j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object()) return fail ("not JSON");
    if (! j.contains ("format") || ! j["format"].is_number_integer() || j["format"].get<int>() != 1) return fail ("unknown manifest format");
    if (getString (j, "app") != "Ohpe") return fail ("not an Ohpe manifest");
    Release r;
    if (! Version::parse (getString (j, "version"), r.version)) return fail ("no valid version");
    r.tag = getString (j, "tag");
    r.date = getString (j, "date");
    r.page = getString (j, "page");
    r.channel = channelFromName (getString (j, "channel"));
    if (j.contains ("notes"))
    {
        r.notesEn = getString (j["notes"], "en");
        r.notesDe = getString (j["notes"], "de");
    }
    if (r.notesDe.empty()) r.notesDe = r.notesEn;
    if (j.contains ("assets") && j["assets"].is_array())
        for (const auto& o : j["assets"])
        {
            Asset a;
            a.platform = lower (getString (o, "platform"));
            a.kind = lower (getString (o, "kind"));
            a.part = lower (getString (o, "part"));
            if (a.part.empty()) a.part = "all";
            if (a.kind.empty()) a.kind = "archive";
            a.name = getString (o, "name");
            a.url = getString (o, "url");
            a.sha256 = lower (getString (o, "sha256"));
            a.size = getSize (o, "size");
            if (a.platform.empty() || a.url.empty()) continue;
            r.assets.push_back (std::move (a));
        }
    if (j.contains ("content") && j["content"].is_array())
        for (const auto& o : j["content"])
        {
            ContentPack p;
            p.id = getString (o, "id");
            p.version = getString (o, "version");
            p.kind = getString (o, "kind");
            if (o.contains ("name"))
            {
                p.nameEn = getString (o["name"], "en");
                p.nameDe = getString (o["name"], "de");
            }
            if (p.nameEn.empty()) p.nameEn = p.id;
            if (p.nameDe.empty()) p.nameDe = p.nameEn;
            p.minApp = getString (o, "minApp");
            p.url = getString (o, "url");
            p.sha256 = lower (getString (o, "sha256"));
            p.size = getSize (o, "size");
            if (p.id.empty() || ! Version::of (p.version).valid() || p.url.empty()) continue;
            r.content.push_back (std::move (p));
        }
    out = std::move (r);
    return true;
}
catch (const std::exception&)
{
    if (error != nullptr) *error = "The update manifest is damaged";
    return false;
}

std::string manifestToJson (const Release& r)
{
    json j;
    j["format"] = 1;
    j["app"] = "Ohpe";
    j["version"] = r.version.toString();
    j["tag"] = r.tag;
    j["channel"] = channelName (r.channel);
    j["date"] = r.date;
    j["page"] = r.page;
    j["notes"] = { { "en", r.notesEn }, { "de", r.notesDe } };
    j["assets"] = json::array();
    for (const auto& a : r.assets)
        j["assets"].push_back ({ { "platform", a.platform }, { "kind", a.kind }, { "part", a.part }, { "name", a.name },
                                 { "url", a.url }, { "size", a.size }, { "sha256", a.sha256 } });
    j["content"] = json::array();
    for (const auto& p : r.content)
        j["content"].push_back ({ { "id", p.id }, { "version", p.version }, { "kind", p.kind },
                                  { "name", { { "en", p.nameEn }, { "de", p.nameDe } } }, { "minApp", p.minApp },
                                  { "url", p.url }, { "size", p.size }, { "sha256", p.sha256 } });
    return j.dump (2);
}

void splitNotes (const std::string& body, std::string& en, std::string& de)
{
    std::string text = body;
    text.erase (std::remove (text.begin(), text.end(), '\r'), text.end());
    // find the headings at line starts
    auto findHeading = [&text] (const char* word) -> size_t {
        size_t pos = 0;
        while ((pos = text.find ("## ", pos)) != std::string::npos)
        {
            if ((pos == 0 || text[pos - 1] == '\n') && (pos + 3 >= text.size() || text[pos + 3] != '#'))
            {
                const auto eol = text.find ('\n', pos);
                const auto line = lower (trim (text.substr (pos + 3, eol == std::string::npos ? std::string::npos : eol - pos - 3)));
                if (line == word) return pos;
            }
            pos += 3;
        }
        return std::string::npos;
    };
    const auto pe = findHeading ("english"), pd = findHeading ("deutsch");
    auto section = [&text] (size_t from, size_t to) {
        const auto eol = text.find ('\n', from);
        if (eol == std::string::npos) return std::string();
        return trim (text.substr (eol + 1, to == std::string::npos ? std::string::npos : to - eol - 1));
    };
    if (pe == std::string::npos && pd == std::string::npos)
    {
        en = de = trim (text);
        return;
    }
    const std::string preamble = trim (text.substr (0, std::min (pe, pd)));
    auto join = [&preamble] (const std::string& s) { return preamble.empty() ? s : (s.empty() ? preamble : preamble + "\n\n" + s); };
    if (pe != std::string::npos) en = join (section (pe, pd > pe ? pd : std::string::npos));
    if (pd != std::string::npos) de = join (section (pd, pe > pd ? pe : std::string::npos));
    if (pe == std::string::npos) en = de;
    if (pd == std::string::npos) de = en;
}

std::map<std::string, std::string> parseSha256Sums (const std::string& text)
{
    std::map<std::string, std::string> sums;
    size_t pos = 0;
    while (pos < text.size())
    {
        auto eol = text.find ('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        const auto line = trim (text.substr (pos, eol - pos));
        pos = eol + 1;
        if (line.size() < 66) continue;
        const auto digest = lower (line.substr (0, 64));
        if (! std::all_of (digest.begin(), digest.end(), [] (char c) { return std::isxdigit ((unsigned char) c) != 0; })) continue;
        if (! std::isspace ((unsigned char) line[64])) continue;
        auto name = trim (line.substr (65));
        if (! name.empty() && name[0] == '*') name = name.substr (1);
        if (! name.empty()) sums[name] = digest;
    }
    return sums;
}

void applySums (Release& r, const std::string& sumsText)
{
    const auto sums = parseSha256Sums (sumsText);
    for (auto& a : r.assets)
        if (a.sha256.empty())
            if (const auto it = sums.find (a.name); it != sums.end()) a.sha256 = it->second;
}

std::vector<ReleaseRef> parseReleaseList (const std::string& text, bool* ok)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    std::vector<ReleaseRef> list;
    if (ok != nullptr) *ok = false;
    const json j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_array()) return list;
    if (ok != nullptr) *ok = true;
    for (const auto& o : j)
    {
        if (! o.is_object()) continue;
        if (o.contains ("draft") && o["draft"].is_boolean() && o["draft"].get<bool>()) continue;
        ReleaseRef ref;
        const auto tag = getString (o, "tag_name");
        if (! Version::parse (tag, ref.version)) continue;
        ref.prerelease = (o.contains ("prerelease") && o["prerelease"].is_boolean() && o["prerelease"].get<bool>()) || ref.version.isPreRelease();
        auto& r = ref.fallback;
        r.version = ref.version;
        r.tag = tag;
        r.page = getString (o, "html_url");
        const auto published = getString (o, "published_at");
        r.date = published.substr (0, std::min<size_t> (10, published.size()));
        r.channel = ref.prerelease ? Channel::Preview : Channel::Stable;
        splitNotes (getString (o, "body"), r.notesEn, r.notesDe);
        if (o.contains ("assets") && o["assets"].is_array())
            for (const auto& a : o["assets"])
            {
                const auto name = getString (a, "name");
                const auto url = getString (a, "browser_download_url");
                if (name == manifestAssetName) { ref.manifestUrl = url; continue; }
                if (name == sumsAssetName) { ref.sumsUrl = url; continue; }
                Asset asset;
                if (! classify (name, asset)) continue;
                asset.name = name;
                asset.url = url;
                asset.size = getSize (a, "size");
                if (! asset.url.empty()) r.assets.push_back (std::move (asset));
            }
        list.push_back (std::move (ref));
    }
    return list;
}
catch (const std::exception&)
{
    if (ok != nullptr) *ok = false;
    return {};
}

int pickUpdate (const std::vector<ReleaseRef>& list, Channel channel, const Version& current, const std::string& skipped)
{
    int best = -1;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& r = list[(size_t) i];
        if (r.prerelease && channel == Channel::Stable) continue;
        if (! (r.version > current)) continue;
        if (best < 0 || r.version > list[(size_t) best].version) best = i;
    }
    if (best >= 0 && ! skipped.empty())
    {
        Version s;
        if (Version::parse (skipped, s) && s == list[(size_t) best].version) return -1;
    }
    return best;
}

int pickPrevious (const std::vector<ReleaseRef>& list, const Version& current)
{
    auto newestOlder = [&] (bool stableOnly) {
        int best = -1;
        for (int i = 0; i < (int) list.size(); ++i)
        {
            const auto& r = list[(size_t) i];
            if (stableOnly && r.prerelease) continue;
            if (! (r.version < current)) continue;
            if (best < 0 || r.version > list[(size_t) best].version) best = i;
        }
        return best;
    };
    // a stable build goes back to the previous stable release; pre-releases only when there is none
    if (! current.isPreRelease() && current.major >= 1)
        if (const int s = newestOlder (true); s >= 0) return s;
    return newestOlder (false);
}
} // namespace ohpe::update
