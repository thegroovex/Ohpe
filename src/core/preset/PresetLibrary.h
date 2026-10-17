// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Preset.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ohpe::preset
{
/** One preset in the library browser (factory bank or user folder). */
struct Entry
{
    std::string uid;          ///< "factory:<resource name>" or "user:<path relative to the user folder>"
    Meta meta;
    bool factory = false;
    std::string location;     ///< resource name (factory) or absolute file path (user)
    int64_t modified = 0;     ///< seconds since 1970 (user files), 0 for factory presets
    bool favourite = false;
    int rating = 0;           ///< 0 (none) .. 5 stars
    std::string pack;         ///< id of the content pack the preset comes from (read-only, factory = true); empty otherwise
};

enum class Bank { All = 0, Factory, User };
enum class Sort { Name = 0, Rating, Engine, Newest };

struct Filter
{
    std::string text;                 ///< words, all must match name / author / tags / engine / comment (case-insensitive)
    std::vector<std::string> tags;    ///< any of these tags (empty = all)
    std::string engine;               ///< engine id, empty = all
    Bank bank = Bank::All;
    int kind = -1;                    ///< -1 all, 0 part, 1 project
    bool favouritesOnly = false;
    int minRating = 0;
    Sort sort = Sort::Name;
    std::string pack;                 ///< content pack id: only that pack's presets (empty = no pack filter)
};

/** Case-insensitive (ASCII + German umlauts) text search over an entry. */
bool matchesText (const Entry& e, const std::string& text);
/** True if the entry passes every criterion of the filter. */
bool matches (const Entry& e, const Filter& f);
/** Indices of `entries` that pass `f`, sorted by `f.sort` (ties: name, then factory before user). */
std::vector<int> filterAndSort (const std::vector<Entry>& entries, const Filter& f);

/**
    User data of the library: favourites and ratings by uid, the last author name. Stored as
    {"format":"ohpe-library","version":1,"author":"...","items":{"<uid>":{"fav":true,"rating":4}}}
    in the user preset folder (library.json).
*/
struct UserData
{
    std::string author;
    std::map<std::string, std::pair<bool, int>> items; ///< uid -> (favourite, rating)

    std::string toJson() const;
    bool fromJson (const std::string& json);
    void applyTo (std::vector<Entry>& entries) const;
    void set (const std::string& uid, bool favourite, int rating);
};

/** The content pack ids the entries come from (for the pack chips), sorted, each once. */
std::vector<std::string> usedPacks (const std::vector<Entry>& entries);

/** The categories that are in use by the entries (for the tag chips), in category order. */
std::vector<std::string> usedTags (const std::vector<Entry>& entries);
} // namespace ohpe::preset
