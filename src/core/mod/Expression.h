// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p33: expressive playing - the per-part Expression page. Velocity and aftertouch each drive up to two musical
// functions ("pN.expr.vel.dst1" ...), with a bipolar amount and a curve. Every slot is one more routing of the part's
// modulation matrix (same sources, curves, scaling and smoothing as "pN.mod.sK.*"), so the page is a simple face of
// the matrix, not a second modulation system.

#include "ModVoice.h"

#include <array>
#include <string>
#include <vector>

namespace ohpe::mod::expr
{
/** The functions an expression slot can drive ("pN.expr.*.dstK"; append-only - presets store the index). */
enum Target
{
    Off = 0, Cutoff, Resonance, Level, Timbre, Attack, Decay, Release, ModEnv, LfoRate, LfoDepth, Vibrato, Glide, FmAmount,
    Position, Spray, MorphX, MorphY, Drive, FxMix, SubLevel, Pan, Pitch, Life,
    numTargets
};

/** The two expression sources. */
enum Input { Velocity = 0, Aftertouch, numInputs };
static constexpr int slotsPerInput = 2;
static_assert (numInputs * slotsPerInput == mod::numExprSlots, "the matrix keeps one routing per expression slot");

/** Vibrato: the 5.5 Hz vibrato source into Pitch; amount 100 % = +-1 semitone (the matrix's pitch range is 12). */
static constexpr float vibratoScale = 1.0f / 12.0f;

struct TargetInfo
{
    const char* key;       ///< stable key (tools, preset generators)
    const char* nameEn;
    const char* nameDe;
    const char* matrixKey; ///< matrix target key (destTable) or nullptr
    std::array<const char*, 4> engineIds; ///< engine parameter ids tried in this order (nullptr ends the list)
    bool engineFirst;      ///< engine parameter before the matrix target (else the matrix target first)
};

/**
    The curated list. A function resolves per engine: the first engine parameter of `engineIds` the active engine has,
    else the matrix target (e.g. Cutoff = the analog filter, the tone of other engines, else the BRIGHT macro; Drive =
    the analog drive, else the HARD macro). A function that resolves to nothing on an engine is inert there.
*/
inline const std::array<TargetInfo, numTargets>& targets()
{
    static const std::array<TargetInfo, numTargets> t { {
        { "off", "Off", "Aus", nullptr, {}, true },
        { "cutoff", "Cutoff", "Cutoff", "charbright", { "cutoff", "tone", "bright", "formant" }, true },
        { "resonance", "Resonance", "Resonanz", nullptr, { "reso", "body", nullptr, nullptr }, true },
        { "level", "Level", "Pegel", "level", {}, false },
        { "timbre", "Timbre", "Klangfarbe", "timbre", {}, false },
        { "attack", "Amp Attack", "Amp Attack", "ampattack", {}, false },
        { "decay", "Amp Decay", "Amp Decay", "ampdecay", {}, false },
        { "release", "Amp Release", "Amp Release", "amprelease", {}, false },
        { "modenv", "Mod Env Amount", "Mod-Hüllkurve Menge", "modenvamt", {}, false },
        { "lforate", "LFO Rate", "LFO Tempo", "lfo1rate", {}, false },
        { "lfodepth", "LFO Depth", "LFO Tiefe", "lfo1depth", {}, false },
        { "vibrato", "Vibrato", "Vibrato", "pitch", {}, false },
        { "glide", "Glide Time", "Glide-Zeit", "glide", {}, false },
        { "fm", "FM Amount", "FM-Menge", nullptr, { "index", "fm", nullptr, nullptr }, true },
        { "position", "Position", "Position", nullptr, { "pos", "vowel", "start", "pickpos" }, true },
        { "spray", "Grain Spray", "Grain-Streuung", "charalive", { "spray", "jitter", "drift", nullptr }, true },
        { "morphx", "Morph X", "Morph X", "morphx", {}, false },
        { "morphy", "Morph Y", "Morph Y", "morphy", {}, false },
        { "drive", "Drive", "Drive", "charhard", { "drive", "feedback", "excite", "tension" }, true },
        { "fxmix", "FX Mix", "FX-Mischung", "fx1mix", {}, false },
        { "sublevel", "Sub Level", "Sub-Pegel", "sublevel", { "sub", nullptr, nullptr, nullptr }, false },
        { "pan", "Pan", "Panorama", "pan", {}, false },
        { "pitch", "Pitch", "Tonhöhe", "pitch", {}, false },
        { "life", "Life", "Leben", "life", {}, false },
    } };
    return t;
}

inline std::vector<std::string> targetNamesEn()
{
    std::vector<std::string> v;
    for (const auto& t : targets()) v.emplace_back (t.nameEn);
    return v;
}
inline std::vector<std::string> targetNamesDe()
{
    std::vector<std::string> v;
    for (const auto& t : targets()) v.emplace_back (t.nameDe);
    return v;
}
inline int targetIndexOfKey (const std::string& key) noexcept
{
    for (int i = 0; i < numTargets; ++i)
        if (key == targets()[(size_t) i].key) return i;
    return -1;
}

/** Parameter suffix of an expression slot: input 0 / 1, slot 0 / 1, field "dst" / "amt" / "curve" -> "expr.vel.dst1". */
inline std::string slotSuffix (int input, int slot, const char* field)
{
    return std::string ("expr.") + (input == Velocity ? "vel." : "at.") + field + std::to_string (slot + 1);
}

/** The defaults of the page: the musical first choice of each slot, amount 0 (neutral until it is turned up). */
inline int defaultTarget (int input, int slot) noexcept
{
    if (input == Velocity) return slot == 0 ? Cutoff : Attack;
    return slot == 0 ? Vibrato : Cutoff;
}
} // namespace ohpe::mod::expr
