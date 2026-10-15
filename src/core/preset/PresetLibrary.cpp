// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PresetLibrary.h"

#include "../engines/EngineFactory.h"

#include <algorithm>
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

bool matchesText (const Entry& e, const std::string& text)
{
    std::istringstream words (fold (text));
    std::string hay = e.meta.name + " " + e.meta.author + " " + e.meta.comment + " " + e.meta.engine + " " + engineName (e.meta.engine);
    for (const auto& t : e.meta.tags) hay += " " + t;
    for (const auto& c : categories())
        if (std::find (e.meta.tags.begin(), e.meta.tags.end(), c.key) != e.meta.tags.end()) hay += std::string (" ") + c.nameDe;
    hay = fold (hay);
    std::string w;
    while (words >> w)
        if (hay.find (w) == std::string::npos) return false;
    return true;
}

bool matches (const Entry& e, const Filter& f)
{
    if (f.bank == Bank::Factory && ! e.factory) return false;
    if (f.bank == Bank::User && e.factory) return false;
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
    return f.text.empty() || matchesText (e, f.text);
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
    j["version"] = 1;
    j["author"] = author;
    json items = json::object();
    for (const auto& [uid, v] : this->items)
        if (v.first || v.second > 0) items[uid] = { { "fav", v.first }, { "rating", v.second } };
    j["items"] = items;
    return j.dump (2);
}

bool UserData::fromJson (const std::string& text)
{
    const auto j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-library") return false;
    author = j.value ("author", "");
    items.clear();
    if (j.contains ("items") && j["items"].is_object())
        for (const auto& [uid, v] : j["items"].items())
            if (v.is_object())
                items[uid] = { v.value ("fav", false), std::clamp (v.value ("rating", 0), 0, 5) };
    return true;
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
