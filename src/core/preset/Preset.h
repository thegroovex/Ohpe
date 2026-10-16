// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"

#include <cstdint>
#include <map>
#include <optional>
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
          "arp": { "on": 1, "mode": 0, "oct": 2, "gate": 0.5, "rate": 2, "latch": 1 },  // part only, optional
          "morph": { "corners": [ { "name": "...", "params": {...} }, null, ... ] }, // part only, optional (p16)
          "sections": { "sequencer": {...}, "orbit": {...} }   // project only, optional
        }

    * kind "part": parameter IDs without the part prefix ("engine", "fm.algo", "amp.a"), so a
      preset loads into any of the four parts. Parameters of the part that the preset does not
      list return to their defaults (presets may be sparse).
    * kind "project": full parameter IDs of the whole instrument except the remote controls
      ("lib.*", "ui.*", "tape.edit.*", "tape.tx.*") and the sync setup ("sync.*"), plus the pattern sections.
      The MIDI map and the tape audio are never part of a preset.
    * "arp" (part presets only, optional, 0.7): arpeggiator settings that travel with the sound.
      Loading such a preset into part N switches the global arpeggiator on and points it at part N
      (seq.arp.on = 1, seq.arp.part = N) and sets the listed keys (on, mode, oct, gate, rate,
      latch; plain values like "params"); keys it does not list return to their defaults. Part
      presets without "arp" leave the arpeggiator alone, except that a preset loaded into the part
      whose arp a preset switched on (and that is still unchanged) switches it off again.
    * "modSchema" (0.7, optional): schema version of the modulation matrix routings ("mod.sK.*" / "pN.mod.sK.*")
      the preset contains; presets of older schemas are migrated when they are read.
    * "morph" (part presets only, optional, p16): the four corners of the part's morph pad (A top left, B top right,
      C bottom left, D bottom right; null = the part's own sound). Each corner lists its name and its sound
      parameters like "params" (local IDs, sparse: missing = default). Project presets and states keep the corners
      of all parts in the section "morph".
    * "usertables" (part presets only, optional, p20): the part's user wavetable and user formant tracks
      ({"wavetable": {"name", "path", "file", "data"?}, "vocal": {...}}). Projects and states keep them in the
      section "usertables" ({"format": "ohpe-usertables", "parts": {"p1": {...}}}). See docs/en/engines/wavetable.md.
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
    /** 0.7: schema of the modulation matrix parameters ("modSchema", 0 = none; see mod::schemaVersion). Written when
        the preset contains matrix routings; older schemas are migrated on load (mod::migrate). */
    int modSchema = 0;
};

struct Preset
{
    Meta meta;
    std::map<std::string, float> params;           ///< part: local IDs; project: full IDs
    std::map<std::string, std::string> sections;   ///< project: section name -> JSON text
    /** part only: arpeggiator settings ("on", "mode", "oct", "gate", "rate", "latch"; local keys of
        "seq.arp.*"), absent if the preset does not carry an arp block */
    std::optional<std::map<std::string, float>> arp;
    /** part only, p16: the morph corners of the part (JSON of MorphSpace::partToJson), absent if no corner is set */
    std::optional<std::string> morph;
    /** part only, p20: the user wavetable / formant material of the part (JSON of usertable::partToJson: name, path,
        library file name and, for tables up to 64 frames, the table itself), absent when the part has none */
    std::optional<std::string> userTables;
};

/** The keys an arp block may carry (local IDs below "seq.arp."). */
const std::vector<std::string>& arpKeys();

/** Prefixes a project preset neither saves nor loads. */
bool isExcludedFromProject (const std::string& id) noexcept;

/** Part preset of part `partIndex` (0-based) with the current values. If the arpeggiator is on and
    plays this part, its settings are captured as the preset's arp block. */
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
    bool arpApplied = false;   ///< part: the preset's arp block was applied (arp on and retargeted)
    bool arpReleased = false;  ///< part: a preset without arp block switched the part's arp off
};

struct PartOptions
{
    /** apply the preset's arp block (false: the sound only, e.g. level measurements) */
    bool arp = true;
    /** for presets without arp block: switch the arpeggiator off if it is on and plays this part.
        The caller sets it when a preset switched that arp on and it has not been changed since. */
    bool releaseArp = false;
};

/** p16: the part's morph corners into the preset's "morph" block (none when no corner is set). */
void captureMorph (const Core& core, int partIndex, Preset& p);
/** p16: a part preset's morph corners into part `partIndex`; a preset without a "morph" block clears the part's
    corners (a new sound replaces the morph space too). */
void applyPartMorph (const Preset& p, Core& core, int partIndex);

/** Loads a part preset into part `partIndex` (0-based). Message thread. */
ApplyResult applyPart (const Preset& p, ParamRegistry& reg, int partIndex, const PartOptions& options = {});

/** Current values of the arpeggiator parameters (seq.arp.*, registry order), to tell whether the
    user changed the arp since a preset set it. */
std::vector<float> arpSnapshot (const ParamRegistry& reg);
/** Loads a project preset: all parameters (except remote / sync), step + orbit patterns. The MIDI
    map stays. Other sections (e.g. "samples") are handed to the core's extra state sections. */
ApplyResult applyProject (const Preset& p, Core& core);

/** Engine id of a part's current engine ("analog" ...). */
std::string engineIdOfPart (const ParamRegistry& reg, int partIndex);

/** A safe file name for a preset name (no path separators, no reserved characters). */
std::string safeFileName (const std::string& name);
} // namespace ohpe::preset
