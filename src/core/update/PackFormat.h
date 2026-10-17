// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "ContentPacks.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ohpe::update
{
/**
    Content packs: the shared loader base (no JUCE) - what a pack zip may hold, where each file goes, how big it may
    be, and how presets point at pack content. Used by the updater (downloaded packs) and "Install pack ..." (a local
    zip); both unpack through the same plan, so the rules are the same.

    Layout of a pack zip (format 1, see packaging/content/README.md):

        pack.json                    {"format":1,"id","version","kind","name":{"en","de"},"description":{"en","de"},"minApp"}
        <kind folder>/...            the content, one fixed folder per kind:
                                       kind "presets"    -> presets/    (*.ohpepreset)
                                       kind "wavetables" -> wavetables/ (*.wav, frames of 2048 samples)
        readme.txt, licence.txt ...  optional notes at the root (*.txt, *.md), kept as they are

    Older packs (format 0) are "flat": the *.ohpepreset files sit at the root of the zip and there is no pack.json.
    They still install: their content files move into the kind folder (presets/), id and version come from the
    manifest entry or the file name "Ohpe-content-<id>-<version>.zip". Library entries keep their uid
    ("content:<id>/<path inside the kind folder>"), so favourites and ratings survive the move.

    Every entry path is checked (isSafeEntryPath) - one unsafe path refuses the whole pack. Files of another kind or
    another type are skipped, never unpacked. A pack lands in Content/<id>/.
*/
enum class PackKind { Unknown = 0, Presets, Wavetables };

PackKind kindFromName (const std::string& name) noexcept;
const char* kindName (PackKind k) noexcept;
/** The fixed sub folder of a kind inside a pack ("presets", "wavetables"); empty for Unknown. */
const char* kindFolder (PackKind k) noexcept;
/** File extensions (lower case, with dot) of a kind's content. */
std::vector<std::string> kindExtensions (PackKind k);

/** pack.json */
struct PackInfo
{
    std::string id, version, kind = "presets";
    std::string nameEn, nameDe, descriptionEn, descriptionDe;
    std::string minApp;
};

constexpr const char* packInfoFile = "pack.json";
constexpr int packFormat = 1;

/** Reads pack.json; false (with an English `error`) when an id / version / kind is missing or not valid.
    Names fall back to the id (EN) and the EN name (DE). */
bool parsePackInfo (const std::string& json, PackInfo& out, std::string* error = nullptr);
std::string packInfoToJson (const PackInfo& info);

/** "Ohpe-content-<id>-<version>.zip" -> id + version (false if the name does not have that shape). */
bool parsePackFileName (const std::string& fileName, std::string& id, std::string& version);

/** Size limits of one pack of a kind. */
struct KindLimits
{
    uint64_t maxPackBytes = 0;  ///< all files of the pack, unpacked
    uint64_t maxFileBytes = 0;  ///< one file, unpacked
    int maxFiles = 0;
};
KindLimits limitsFor (PackKind k) noexcept;
/** Everything in Content/ (installed packs, without backups) may use at most this much. */
constexpr uint64_t contentQuotaBytes = 2ull * 1024 * 1024 * 1024;
/** Root notes (*.txt / *.md) of a pack: at most this big each. */
constexpr uint64_t maxNoteBytes = 256 * 1024;

/** One entry of the zip (as listed by the zip directory). */
struct ZipItem
{
    std::string path;
    uint64_t size = 0; ///< uncompressed
    bool directory = false;
};

/** Where each file of a zip goes, or why the pack is refused. */
struct PackPlan
{
    bool ok = false;
    std::string error;  ///< English, for the UI ("The content pack is damaged" ...)
    std::string detail; ///< not translated (a path, a size)
    PackInfo info;
    PackKind kind = PackKind::Unknown;
    bool flat = false;      ///< old layout: content at the root, moved into the kind folder
    bool hasPackInfo = false;
    struct File
    {
        int index = -1;     ///< into the zip items
        std::string target; ///< path inside Content/<id>/
        uint64_t size = 0;
    };
    std::vector<File> files;
    int skipped = 0;        ///< entries that are not content of this kind (left out)
    uint64_t totalBytes = 0;
};

/**
    Plans the unpacking of a pack.

    `items`: the zip directory. `packJson`: the text of the zip's pack.json (empty = none). `expected`: what the
    caller knows already (manifest entry: id, version, kind; local install: from the file name, may be empty) - a
    pack.json must agree with a given id. `usedBytes`: what the other installed packs use (quota check; the pack's
    own installed version is not counted, it becomes the backup).
*/
PackPlan planPack (const std::vector<ZipItem>& items, const std::string& packJson, const PackInfo& expected, uint64_t usedBytes = 0);

/** uid path of a pack file for the library: "<id>/<path inside the kind folder>" (root files of old packs keep their name). */
std::string contentUidPath (const std::string& packId, const std::string& pathInPack);

/**
    A reference from a preset to pack content: "<pack>/<item>", e.g. "atmo/glass-choir" = the wavetable
    Content/atmo/wavetables/glass-choir.wav. The item is the path inside the kind folder without its extension
    (sub folders allowed: "atmo/vowels/aah").
*/
struct ContentRef
{
    std::string pack, item;

    bool valid() const;
    std::string toString() const { return pack + "/" + item; }
    static ContentRef parse (const std::string& text); ///< invalid (empty) if the text is not a reference
};

/** What resolving a reference found. */
enum class RefState { Found = 0, PackMissing, ItemMissing, Invalid };
const char* refStateName (RefState s) noexcept;

/** The content of the installed packs, by kind: pack id -> items (paths in the kind folder without extension). */
struct ContentIndex
{
    struct Pack
    {
        std::string id, version, nameEn, nameDe;
        std::vector<std::pair<PackKind, std::string>> items;
    };
    std::vector<Pack> packs;

    const Pack* find (const std::string& id) const;
    RefState resolve (const ContentRef& ref, PackKind kind) const;
};

/** Content download URLs the updater accepts (https release downloads of the project and its pack repository, their
    asset host); SHA-256 stays mandatory. `allowLocal`: file:// as well (tests). */
bool isAllowedContentUrl (const std::string& url, bool allowLocal = false);
} // namespace ohpe::update
