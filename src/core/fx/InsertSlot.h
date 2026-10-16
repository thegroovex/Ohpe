// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "IEffect.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace ohpe
{
/**
    The FX slot of one part (the engine's insert effect), between the part and the mixer.

    Parameters (prefix e.g. "p1.fx."): "type" (Off, Chorus, Phaser, Bitcrush, Filter Drive,
    Delay, Reverb - append-only list) and one parameter block per effect type
    ("p1.fx.chorus.rate", ...). Delay and reverb are 100 % wet effects; the slot adds a
    "mix" parameter for them ("p1.fx.delay.mix"). Type Off is an exact bypass.

    0.8: every part has its slot (parts 2-4 appended), and the type list grew (append-only) by Shaper,
    Squash, Comb, Formant, Flanger, Wobble, Tremolo and Width. Every effect is a self-contained IEffect,
    so other hosts of effects (e.g. a larger rack) can use the same classes.
*/
class InsertSlot
{
public:
    enum Type
    {
        Off = 0, Chorus, Phaser, Bitcrush, FilterDrive, Delay, Reverb,
        Shaper, Squash, Comb, Formant, Flanger, Wobble, Tremolo, Width, // 0.8
        numTypes
    };
    static constexpr int numLegacyTypes = Reverb + 1; ///< the 0.2 types (their blocks are registered in the 0.2 layout)

    InsertSlot();

    /** Registers the slot parameters for a prefix such as "p1.fx." (append-only): the type choice (every type)
        and the parameter blocks of the 0.2 types. */
    static void registerParams (ParamRegistry& reg, const std::string& prefix, const std::string& group);
    /** 0.8: the parameter blocks of the types added in 0.8 (registered at the end of the layout). */
    static void registerLateParams (ParamRegistry& reg, const std::string& prefix, const std::string& group);

    void bind (const ParamRegistry& reg, const std::string& prefix);
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void pullParams (const ParamSnapshot& snap) noexcept;

    /** In place; numSamples <= maxBlockSize. */
    void process (float* left, float* right, int numSamples) noexcept;

    int currentType() const noexcept { return type; }
    /** 0.8: tempo for the synced types, note starts of the part (LFO retrigger). Audio thread. */
    void setTempo (double bpm) noexcept;
    void noteOn() noexcept;
    IEffect* effect (int t) noexcept { return t > Off && t < numTypes ? effects[(size_t) t].get() : nullptr; }

    static bool isWetOnly (int t) noexcept { return t == Delay || t == Reverb; }

private:
    static std::unique_ptr<IEffect> create (int t);
    static void registerBlocks (ParamRegistry& reg, const std::string& prefix, const std::string& group, int from, int to);

    std::array<std::unique_ptr<IEffect>, numTypes> effects;
    std::array<std::vector<int>, numTypes> paramIndex;
    std::array<std::vector<float>, numTypes> lastValue;
    std::array<int, numTypes> mixIndex {};
    int typeIndex = -1, type = Off;
    float wetMix = 0.35f;
    std::vector<float> dryL, dryR;
};
} // namespace ohpe
