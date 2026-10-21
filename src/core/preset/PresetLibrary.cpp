// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PresetLibrary.h"
#include "model/JsonParse.h" // p63: depth-limited parse
#include "../model/FormatVersions.h"

#include "../engines/EngineFactory.h"

#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>

namespace ohpe::preset
{
using nlohmann::json;

namespace
{
/** Lower case for ASCII and the German umlauts (UTF-8), so "ÄRA" finds "ära". */
std::string fold (const std::string& s)
{
    std::string r;
    r.reserve (s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        const auto c = (unsigned char) s[i];
        if (c == 0xc3 && i + 1 < s.size())
        {
            const auto n = (unsigned char) s[i + 1];
            r += (char) c;
            r += (char) ((n == 0x84 || n == 0x96 || n == 0x9c) ? n + 0x20 : n); // Ä Ö Ü -> ä ö ü
            ++i;
            continue;
        }
        r += (char) ((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    return r;
}

std::string engineName (const std::string& id)
{
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto& info = engineTypeInfo ((EngineType) t);
        if (id == info.id) return std::string (info.nameEn) + " " + info.nameDe;
    }
    return {};
}
} // namespace

std::string foldForSearch (const std::string& s)
{
    std::string r;
    r.reserve (s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        const auto c = (unsigned char) s[i];
        if (c == 0xc3 && i + 1 < s.size())
        {
            const auto n = (unsigned char) s[i + 1];
            switch (n)
            {
                case 0x84: case 0xa4: r += 'a'; ++i; continue; // Ä ä
                case 0x96: case 0xb6: r += 'o'; ++i; continue; // Ö ö
                case 0x9c: case 0xbc: r += 'u'; ++i; continue; // Ü ü
                case 0x9f: r += "ss"; ++i; continue;           // ß
                default: break;
            }
        }
        const char lc = (char) ((c >= 'A' && c <= 'Z') ? c + 32 : c);
        // "ae" / "oe" / "ue" spell the umlauts too ("Flaeche" = "Fläche"); folded the same way on both sides
        if (lc == 'e' && ! r.empty() && (r.back() == 'a' || r.back() == 'o' || r.back() == 'u')) continue;
        r += lc;
    }
    return r;
}

const std::vector<Category>& quickTags()
{
    // append-only: free tags of the factory presets with their own chip
    static const std::vector<Category> list = {
        { "MPE", "MPE" },
        { "Fingerboard", "Fingerboard" },
        { "Morph", "Morph" },
    };
    return list;
}

std::vector<std::string> usedQuickTags (const std::vector<Entry>& entries)
{
    std::vector<std::string> r;
    for (const auto& q : quickTags())
        for (const auto& e : entries)
            if (std::find (e.meta.tags.begin(), e.meta.tags.end(), q.key) != e.meta.tags.end())
            {
                r.push_back (q.key);
                break;
            }
    return r;
}

namespace
{
/** p54: extra search words for tags (English / German / spellings), so "pad", "fläche", "expressive" find them. */
const char* tagAliases (const std::string& tag)
{
    static const std::map<std::string, const char*> aliases = {
        { "Pads", "pad fläche flaechen area" },   { "Keys", "key taste tasteninstrument" }, { "Plucks", "pluck gezupft zupf" },
        { "Leads", "lead solo" },                 { "Textures", "texture textur atmo" },    { "Vocal", "voice vox stimme vokal" },
        { "Drums", "drum perc percussion schlagzeug trommel" }, { "FX", "effect effekt sfx" }, { "Arp", "arpeggio arpeggiator" },
        { "Piano", "klavier e-piano epiano" },    { "3D", "binaural spatial raum" },        { "Boom", "kick sub" },
        { "MPE", "mpe expressive expression ausdruck polyphonic aftertouch" },
        { "Fingerboard", "fingerboard mpe-hd mpe hd griffbrett" },
        { "Morph", "morph morphing" },
    };
    const auto it = aliases.find (tag);
    return it != aliases.end() ? it->second : "";
}
} // namespace

std::string searchText (const Entry& e)
{
    std::string hay = e.meta.name + " " + e.meta.author + " " + e.meta.comment + " " + e.meta.engine + " " + engineName (e.meta.engine);
    for (const auto& t : e.meta.tags) hay += " " + t + " " + tagAliases (t);
    for (const auto& c : categories())
        if (std::find (e.meta.tags.begin(), e.meta.tags.end(), c.key) != e.meta.tags.end()) hay += std::string (" ") + c.nameDe;
    for (const auto& c : quickTags())
        if (std::find (e.meta.tags.begin(), e.meta.tags.end(), c.key) != e.meta.tags.end()) hay += std::string (" ") + c.nameDe;
    if (! e.pack.empty()) hay += " pack paket " + e.pack + " " + e.packName;
    hay += e.factory ? (e.pack.empty() ? " factory werk" : "") : " user eigene benutzer";
    hay += e.meta.kind == Kind::Project ? " project projekt" : " part";
    if (e.favourite) hay += " favourite favorite favorit fav \xe2\x98\x85";
    if (e.rating > 0)
    {
        hay += " ";
        for (int s = 0; s < e.rating; ++s) hay += "*";
        hay += " " + std::to_string (e.rating) + "star " + std::to_string (e.rating) + "stern";
    }
    return foldForSearch (hay);
}

bool matchesText (const Entry& e, const std::string& text)
{
    std::istringstream words (foldForSearch (text));
    const auto hay = searchText (e);
    std::string w;
    while (words >> w)
        if (hay.find (w) == std::string::npos) return false;
    return true;
}

bool matches (const Entry& e, const Filter& f)
{
    if (f.bank == Bank::Factory && ! e.factory) return false;
    if (f.bank == Bank::User && e.factory) return false;
    if (! f.pack.empty() && e.pack != f.pack) return false;
    if (f.kind >= 0 && (int) e.meta.kind != f.kind) return false;
    if (f.favouritesOnly && ! e.favourite) return false;
    if (e.rating < f.minRating) return false;
    if (! f.engine.empty() && e.meta.engine != f.engine) return false;
    if (! f.tags.empty())
    {
        bool any = false;
        for (const auto& t : f.tags)
            any = any || std::find (e.meta.tags.begin(), e.meta.tags.end(), t) != e.meta.tags.end();
        if (! any) return false;
    }
    for (const auto& t : f.required)
        if (std::find (e.meta.tags.begin(), e.meta.tags.end(), t) == e.meta.tags.end()) return false;
    return f.text.empty() || matchesText (e, f.text);
}

std::vector<std::string> usedPacks (const std::vector<Entry>& entries)
{
    std::vector<std::string> out;
    for (const auto& e : entries)
        if (! e.pack.empty() && std::find (out.begin(), out.end(), e.pack) == out.end()) out.push_back (e.pack);
    std::sort (out.begin(), out.end());
    return out;
}

int indexOfUid (const std::vector<Entry>& entries, const std::string& uid)
{
    if (uid.empty()) return -1;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].uid == uid) return (int) i;
    return -1;
}

std::vector<int> filterAndSort (const std::vector<Entry>& entries, const Filter& f)
{
    std::vector<int> idx;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (matches (entries[(size_t) i], f)) idx.push_back (i);
    auto byName = [&] (int a, int b) {
        const auto na = fold (entries[(size_t) a].meta.name), nb = fold (entries[(size_t) b].meta.name);
        if (na != nb) return na < nb;
        if (entries[(size_t) a].factory != entries[(size_t) b].factory) return entries[(size_t) a].factory;
        return entries[(size_t) a].uid < entries[(size_t) b].uid;
    };
    std::stable_sort (idx.begin(), idx.end(), [&] (int a, int b) {
        const auto& ea = entries[(size_t) a];
        const auto& eb = entries[(size_t) b];
        switch (f.sort)
        {
            case Sort::Rating:
                if (ea.rating != eb.rating) return ea.rating > eb.rating;
                if (ea.favourite != eb.favourite) return ea.favourite;
                break;
            case Sort::Engine:
                if (ea.meta.engine != eb.meta.engine) return ea.meta.engine < eb.meta.engine;
                break;
            case Sort::Newest:
                if (ea.modified != eb.modified) return ea.modified > eb.modified;
                break;
            case Sort::Name: break;
        }
        return byName (a, b);
    });
    return idx;
}

std::string UserData::toJson() const
{
    json j;
    j["format"] = "ohpe-library";
    j["version"] = formats::library;
    j["author"] = author;
    json list = json::object();
    for (const auto& [uid, v] : items)
        if (v.first || v.second > 0) list[uid] = { { "fav", v.first }, { "rating", v.second } };
    j["items"] = list;
    return j.dump (2);
}

bool UserData::fromJson (const std::string& text)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    const auto j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-library") return false;
    author = j.value ("author", "");
    items.clear();
    if (j.contains ("items") && j["items"].is_object())
        for (const auto& [uid, v] : j["items"].items())
            if (v.is_object())
                items[uid] = { v.value ("fav", false), std::clamp (v.value ("rating", 0), 0, 5) };
    return true;
}
catch (const std::exception&)
{
    return false;
}

void UserData::applyTo (std::vector<Entry>& entries) const
{
    for (auto& e : entries)
    {
        const auto it = items.find (e.uid);
        e.favourite = it != items.end() && it->second.first;
        e.rating = it != items.end() ? it->second.second : 0;
    }
}

void UserData::set (const std::string& uid, bool favourite, int rating)
{
    rating = std::clamp (rating, 0, 5);
    if (! favourite && rating == 0) items.erase (uid);
    else items[uid] = { favourite, rating };
}

std::vector<std::string> usedTags (const std::vector<Entry>& entries)
{
    std::set<std::string> used, free;
    for (const auto& e : entries)
        for (const auto& t : e.meta.tags)
            (isCategory (t) ? used : free).insert (t);
    std::vector<std::string> r;
    for (const auto& c : categories())
        if (used.count (c.key) > 0) r.push_back (c.key);
    r.insert (r.end(), free.begin(), free.end());
    return r;
}
} // namespace ohpe::preset
