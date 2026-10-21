// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "ParamRegistry.h"
#include "FormatVersions.h"

#include <array>
#include <map>
#include <string>

namespace ohpe::state
{
/**
    Ohpe state / preset documents (JSON, UTF-8).

    State:  {"format":"ohpe-state","version":1,"app":"Ohpe","params":{"<id>":<plain value>,...},
             "sections":{"midimap":{...},"sequencer":{...},"ui":{...}}}
    Preset: {"format":"ohpe-preset","version":1,"name":"...","prefix":"p1.","params":{"<id without prefix>":<value>}}

    Compatibility rules: parameters are stored by stable string ID with plain values. Unknown
    IDs (from newer versions) are ignored, missing IDs keep their defaults, so old documents
    load in new versions and vice versa. Sections are opaque JSON owned by their module.
*/
using Sections = std::map<std::string, std::string>; ///< section name -> JSON text

constexpr int currentVersion = formats::state;

std::string saveState (const ParamRegistry& reg, const Sections& sections);

struct LoadResult
{
    bool ok = false;
    int applied = 0;   ///< parameters set
    int unknown = 0;   ///< parameters in the document this build does not know
    Sections sections;
};

/** Loads a state. resetFirst = parameters not in the document return to their defaults. */
LoadResult loadState (const std::string& json, ParamRegistry& reg, bool resetFirst = true);

/** Saves all parameters whose ID starts with `prefix` (e.g. "p1.") as a preset. */
std::string savePreset (const ParamRegistry& reg, const std::string& prefix, const std::string& name);

/** p62 (moved from the plug-in, p40 / p50): the preset uid of each of the four parts from a state's "ui" section.
    Current shape: "presets" = [uid part 1 .. uid part 4] ("preset" = the edit part's, kept for older builds). Older
    sessions (before p50) only have "preset": that uid is given to every part. Empty strings = no preset ("Init"). */
std::array<std::string, 4> sessionPresetUids (const std::string& uiJson);

/** Loads a preset into `targetPrefix` (e.g. a part-1 preset into part 2 with "p2."). */
LoadResult loadPreset (const std::string& json, ParamRegistry& reg, const std::string& targetPrefix);
} // namespace ohpe::state
