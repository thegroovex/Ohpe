// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <string>

namespace ohpe::character
{
/**
    p21: the four character macros of a part ("pN.char.bright", "pN.char.hard", "pN.char.alive", "pN.char.wide",
    -1 .. +1, 0 = neutral). Each one moves several real parameters of the part at once, musically:

      * BRIGHT - filter cutoff / tone / brightness, FM index, pluck damping (less), vocal formant, a touch of resonance;
      * HARD   - shorter amp attack, drive, FM feedback, pluck excitation, crackle, vocal tension;
      * ALIVE  - the part's life amount ("pN.life", p15), drift, jitter, vibrato depth, grain spray / scan;
      * WIDE   - stereo width: unison width, cluster / grain spread and side, the layer's width.

    The offsets are added to the normalised value of each parameter inside the part (after the modulation matrix) -
    host automation and the UI keep the stored values, all four at 0 leave the sound exactly as it is.
    Audio thread safe (no allocation).
*/
enum Macro { Bright = 0, Hard, Alive, Wide, numMacros };

/** Stable key of a macro ("bright", "hard", "alive", "wide"): the parameter suffix after "char.". */
const char* key (int macro) noexcept;
const char* nameEn (int macro) noexcept; ///< "Bright" ...
const char* nameDe (int macro) noexcept; ///< "Hell" ...

/** How far one unit of each macro moves a parameter (normalised offset per unit). */
struct Weights
{
    std::array<float, numMacros> w {};
    bool any() const noexcept { return w[0] != 0.0f || w[1] != 0.0f || w[2] != 0.0f || w[3] != 0.0f; }
};

/** Weights of an engine parameter by its local ID ("cutoff", "drive" ...; the same for every engine). */
Weights engineWeights (const std::string& localId) noexcept;

/** Part-level targets (not engine parameters). */
constexpr float ampAttackHard = -0.30f; ///< amp attack (normalised) per unit of HARD
constexpr float lifeAlive = 0.60f;      ///< "pN.life" (0..1) per unit of ALIVE
constexpr float layerWidthWide = 0.40f; ///< "pN.layer.width" per unit of WIDE

/** The summed offset of `w` for the macro values `m` (each -1..1). */
inline float offset (const Weights& w, const std::array<float, numMacros>& m) noexcept
{
    return w.w[0] * m[0] + w.w[1] * m[1] + w.w[2] * m[2] + w.w[3] * m[3];
}

/** A normalised value moved by an offset and clamped to 0..1. */
inline float moved (float norm, float off) noexcept
{
    const float v = norm + off;
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
} // namespace ohpe::character
