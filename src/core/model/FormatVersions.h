// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>

/**
    p62 (1.0 format freeze): the version of every file and document Ohpe writes, in one place.

    Every persisted format carries an explicit version field. The rules (docs/FORMATS.md, docs/de/FORMATE.md):
      * Older files always load: a reader accepts every version from 1 (or 0 = "written before the field existed")
        up to its own and migrates them on load. The migrations are tested against frozen fixture files
        (tests/formats/fixtures).
      * Newer JSON documents load too: unknown fields, sections and parameter IDs are ignored (or kept unchanged where
        the format says so, e.g. settings.json); values a build knows keep their meaning forever.
      * Binary formats change their layout only with a version bump; a reader refuses a version whose layout it cannot
        read instead of reading it wrongly (.ohpetape), or reads the fixed leading part (.ohpewt, append-only layout).
      * Host parameter IDs are append-only (tests/core/golden/param_ids_v1.txt).

    A version here only ever goes up, and only together with a migration, a new fixture of the old shape, an entry in
    tests/formats/FORMATS.lock and the format documents (scripts/ci/check-formats.py and FormatTests enforce this).
*/
namespace ohpe::formats
{
// documents / files the user keeps
constexpr int state = 1;          ///< plug-in / standalone state and host sessions: {"format":"ohpe-state"}
constexpr int preset = 2;         ///< .ohpepreset (part and project presets): {"format":"ohpe-preset"}
constexpr int modSchema = 1;      ///< meaning of the modulation matrix routings in presets ("modSchema")
constexpr int tapeFile = 1;       ///< .ohpetape header {"format":"ohpe-tape"}
constexpr int tapeAudio = 1;      ///< tape audio blob ("OHPETAPE", in .ohpetape and appended to the plug-in state)
constexpr int wavetable = 1;      ///< .ohpewt ("OHPEWT" + uint16 version)
constexpr int pack = 1;           ///< .ohpepack pack.json "format"
constexpr int midiMap = 1;        ///< .ohpemidi and state section "midimap": {"format":"ohpe-midimap"}
constexpr int settings = 1;       ///< <user data>/Settings/settings.json ("version"; absent = 0, before 1.0)
constexpr int updateSettings = 1; ///< <user data>/Settings/update.json ("version"; absent = 0, before 1.0)
constexpr int library = 1;        ///< library.json (favourites, ratings): {"format":"ohpe-library"}
constexpr int installedPacks = 1; ///< Content/installed.json "format"

// sections inside a state / project preset (each its own "version")
constexpr int sequencer = 1;      ///< "sequencer"
constexpr int orbit = 1;          ///< "orbit"
constexpr int morph = 1;          ///< "morph" (and a part preset's "morph" block)
constexpr int motion = 1;         ///< "motion"
constexpr int harmony = 1;        ///< "harmony"
constexpr int samples = 1;        ///< "samples": {"format":"ohpe-samples"}
constexpr int userTables = 1;     ///< "usertables": {"format":"ohpe-usertables"}
constexpr int genome = 1;         ///< sound genome (breed)

struct Entry
{
    const char* key; ///< as in tests/formats/FORMATS.lock
    int version;
};

/** Every constant above, in the order of tests/formats/FORMATS.lock (append-only). */
constexpr std::array<Entry, 20> all { { { "state", state },
                                        { "preset", preset },
                                        { "modSchema", modSchema },
                                        { "tapeFile", tapeFile },
                                        { "tapeAudio", tapeAudio },
                                        { "wavetable", wavetable },
                                        { "pack", pack },
                                        { "midiMap", midiMap },
                                        { "settings", settings },
                                        { "updateSettings", updateSettings },
                                        { "library", library },
                                        { "installedPacks", installedPacks },
                                        { "sequencer", sequencer },
                                        { "orbit", orbit },
                                        { "morph", morph },
                                        { "motion", motion },
                                        { "harmony", harmony },
                                        { "samples", samples },
                                        { "userTables", userTables },
                                        { "genome", genome } } };
} // namespace ohpe::formats
