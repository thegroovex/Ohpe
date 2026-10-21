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
    std::string packName;     ///< p54: the pack's display names (English and German), searched by the text filter
};

enum class Bank { All = 0, Factory, User };
enum class Sort { Name = 0, Rating, Engine, Newest };

struct Filter
{
    /** words, all must match (AND) name / author / comment / category and tags (English and German) / engine / pack /
        bank / kind / favourite / rating (case- and umlaut-insensitive: "flache", "flaeche" and "Fläche" are the same) */
    std::string text;
    std::vector<std::string> tags;    ///< any of these tags (empty = all)
    std::vector<std::string> required; ///< p54: all of these tags (the quick chips MPE / Fingerboard / Morph); empty = all
    std::string engine;               ///< engine id, empty = all
    Bank bank = Bank::All;
    int kind = -1;                    ///< -1 all, 0 part, 1 project
    bool favouritesOnly = false;
    int minRating = 0;
    Sort sort = Sort::Name;
    std::string pack;                 ///< content pack id: only that pack's presets (empty = no pack filter)
};

/** p54: the quick filter tags (append-only; free tags of the factory bank that get their own chips), English key +
    German label. They filter with "all of" (Filter::required), so "Pads" + "MPE" shows the MPE pads. */
const std::vector<Category>& quickTags();
/** The quick tags that are used by the entries (for the quick chips), in quickTags() order. */
std::vector<std::string> usedQuickTags (const std::vector<Entry>& entries);

/** p54: folds text for the search: lower case (ASCII, German umlauts), umlauts to their base letter (ä/ae -> a,
    ö/oe -> o, ü/ue -> u, ß -> ss). */
std::string foldForSearch (const std::string& s);
/** p54: everything the text search looks at for an entry (folded): name, author, comment, engine (id, English and
    German name), tags (with German category names and aliases), pack, bank, kind, favourite and rating words. */
std::string searchText (const Entry& e);
/** Text search over an entry: every whitespace separated word of `text` must occur in searchText(e). */
bool matchesText (const Entry& e, const std::string& text);
/** True if the entry passes every criterion of the filter. */
bool matches (const Entry& e, const Filter& f);
/** Indices of `entries` that pass `f`, sorted by `f.sort` (ties: name, then factory before user). */
std::vector<int> filterAndSort (const std::vector<Entry>& entries, const Filter& f);
/** p65: index of the entry with this uid, -1 if there is none (empty uid, a user preset that was deleted, or a factory
    preset retired by the 1.0 bank curation). A session that names an unknown uid keeps its sound - the parameters are
    stored in the session - and shows no preset name. */
int indexOfUid (const std::vector<Entry>& entries, const std::string& uid);

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
