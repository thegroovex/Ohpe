// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ohpe
{
class Core;
}

/**
    Ohpe preset files (format "ohpe-preset", version 2), documented in docs/en/PRESETS.md.

    A preset is one UTF-8 JSON document, file extension ".ohpepreset":

        {
          "format": "ohpe-preset",
          "version": 2,
          "kind": "part" | "project",
          "name": "Glass Keys",
          "author": "groovex",
          "tags": ["Keys", "Plucks", "bright"],   // categories (fixed list) and free tags
          "engine": "fm",                          // part: the engine; project: engine of part 1
          "comment": "",
          "params": { "<id>": <plain value>, ... },
          "sections": { "sequencer": {...}, "orbit": {...} }   // project only, optional
        }

    * kind "part": parameter IDs without the part prefix ("engine", "fm.algo", "amp.a"), so a
      preset loads into any of the four parts. Parameters of the part that the preset does not
      list return to their defaults (presets may be sparse).
    * kind "project": full parameter IDs of the whole instrument except the remote controls
      ("lib.*", "ui.*", "tape.edit.*") and the sync setup ("sync.*"), plus the pattern sections.
      The MIDI map and the tape audio are never part of a preset.
    * Values are plain values (as shown, e.g. Hz, seconds). Unknown IDs are ignored, so presets
      from newer versions load; version-1 presets (no kind / tags, "prefix" field) load as parts.
    * Loading a preset never adds, removes or reorders parameters.
*/
namespace ohpe::preset
{
constexpr int formatVersion = 2;
constexpr const char* fileExtension = ".ohpepreset";

enum class Kind { Part = 0, Project };

/** The fixed category tags (append-only), English key + German label. */
struct Category
{
    const char* key;
    const char* nameDe;
};
const std::vector<Category>& categories();
bool isCategory (const std::string& tag) noexcept;

struct Meta
{
    std::string name;
    std::string author;
    std::string comment;
    std::vector<std::string> tags;
    std::string engine;           ///< engine id ("analog", "fm" ...), empty if unknown
    Kind kind = Kind::Part;
    int version = formatVersion;  ///< version of the document it was read from
};

struct Preset
{
    Meta meta;
    std::map<std::string, float> params;           ///< part: local IDs; project: full IDs
    std::map<std::string, std::string> sections;   ///< project: section name -> JSON text
};

/** Prefixes a project preset neither saves nor loads. */
bool isExcludedFromProject (const std::string& id) noexcept;

/** Part preset of part `partIndex` (0-based) with the current values. */
Preset capturePart (const ParamRegistry& reg, int partIndex, Meta meta);
/** Project preset of the whole instrument (parameters + step / orbit patterns + `extraSections`). */
Preset captureProject (Core& core, Meta meta, const std::map<std::string, std::string>& extraSections = {});

std::string toJson (const Preset& p);
/** Parses a document; false (and `error`) if it is not an Ohpe preset. */
bool fromJson (const std::string& json, Preset& out, std::string* error = nullptr);

struct ApplyResult
{
    bool ok = false;
    int applied = 0;
    int unknown = 0;
};

/** Loads a part preset into part `partIndex` (0-based). Message thread. */
ApplyResult applyPart (const Preset& p, ParamRegistry& reg, int partIndex);
/** Loads a project preset: all parameters (except remote / sync), step + orbit patterns. The MIDI
    map stays. Other sections (e.g. "samples") are handed to the core's extra state sections. */
ApplyResult applyProject (const Preset& p, Core& core);

/** Engine id of a part's current engine ("analog" ...). */
std::string engineIdOfPart (const ParamRegistry& reg, int partIndex);

/** A safe file name for a preset name (no path separators, no reserved characters). */
std::string safeFileName (const std::string& name);
} // namespace ohpe::preset
