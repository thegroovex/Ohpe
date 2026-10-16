// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "IEffect.h"

#include <array>
#include <atomic>
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

    0.7 (FX rack): the slot is the building block of FxRack (six slots per part, four on the master).
    Effect types whose parameter blocks are not registered for a prefix act as Off. Slots of a rack
    other than slot 1 are "lazy": an effect type allocates its buffers (prepare) only when it is
    selected, on the message thread (prepareSelected); until then the slot passes the signal dry.
    Adding a type: append it to Type, create() and the name lists, and register its parameter block
    for every slot at the end of the layout (FxRack::registerTypeBlocks).

    1.0: Space 3D (the 3D spatialiser, Space3D.h) appended after the 0.8 types.

    0.8 (sound revision 3): Comb, Formant, Wobble, Tremolo and Width appended after the rack types
    (InsertEffects2.h); their blocks are registered for every slot with FxRack::registerTypeBlocks.
*/
class InsertSlot
{
public:
    enum Type
    {
        Off = 0, Chorus, Phaser, Bitcrush, FilterDrive, Delay, Reverb,
        Hyper, Distortion, Flanger, Compressor, Multiband, Eq, Filter, Echo, Hall, // 0.7 (rack types, RackEffects.h)
        MidSide, // 0.7 (stereo type, MidSide.h)
        Comb, Formant, Wobble, Tremolo, Width, // 0.8 (sound revision 3, InsertEffects2.h)
        Space3D, // 1.0 (3D spatialiser, Space3D.h)
        numTypes
    };
    static constexpr int firstRackType = Hyper; ///< the 0.7 rack types (their blocks: FxRack::registerTypeBlocks)
    static constexpr int endRackTypes = Hall + 1; ///< one past the 0.7 rack types (registered before the stereo types)
    static constexpr int firstStereoType = MidSide; ///< 0.7 stereo types (their blocks: FxRack::registerTypeBlocks, after the rack types)
    static constexpr int firstR3Type = Comb;  ///< the 0.8 types (their blocks: FxRack::registerTypeBlocks, after Mid/Side)
    static constexpr int firstSpaceType = Space3D; ///< 1.0: the spatialiser (its blocks: FxRack::registerTypeBlocks, at the end)
    static constexpr int numLegacyTypes = Reverb + 1; ///< the 0.2 types (their blocks: registerParams)

    InsertSlot();

    /** English / German names of the types (append-only, index = "type" value). */
    static std::vector<std::string> typeNamesEn();
    static std::vector<std::string> typeNamesDe();

    /** Registers the slot parameters for a prefix such as "p1.fx." (append-only): the type choice
        (every type) and the parameter blocks of the 0.2 types. */
    static void registerParams (ParamRegistry& reg, const std::string& prefix, const std::string& group);
    /** Registers the parameter blocks of the types [fromType, toType) for one prefix (later types). */
    static void registerBlocks (ParamRegistry& reg, const std::string& prefix, const std::string& group, int fromType, int toType);
    /** Registers the parameters of one type's block from its local index `firstParam` on (parameters a type
        appended to its block later, e.g. the 0.8 chorus ensemble). */
    static void registerBlockTail (ParamRegistry& reg, const std::string& prefix, const std::string& group, int type, int firstParam);
    /** Host name prefix of a slot prefix: "" for the 0.2 slot "p1.fx.", "P2 FX3 " for "p2.fx3.", "Master FX 1 " ... */
    static std::string hostNamePrefix (const std::string& prefix, bool german);
    /** Effect info of a type (Off: nullptr id). Message thread. */
    static EffectInfo typeInfo (int t);

    void bind (const ParamRegistry& reg, const std::string& prefix);
    /** Lazy slots prepare only the selected types (see class comment). Default: every type at prepare(). */
    void setLazy (bool lazyPrepare) noexcept { lazy = lazyPrepare; }
    void prepare (double sampleRate, int maxBlockSize);
    /** Message thread: prepares the type selected in `reg` if it is not ready (lazy slots). Returns true when
        it prepared something. Never call while the audio thread could process that type (it cannot: an
        unready type is not processed). */
    bool prepareSelected (const ParamRegistry& reg);
    /** p16 (morph space), message thread: prepares type `t` if it is not ready (lazy slots), like prepareSelected for
        a type a morph corner selects. Returns true when it prepared something. */
    bool prepareType (int t);
    /** Audio thread: the selected type is waiting for prepareSelected(). */
    bool isWaiting() const noexcept { return waiting.load (std::memory_order_relaxed); }
    bool isReady (int t) const noexcept { return t > Off && t < numTypes && ready[(size_t) t].load (std::memory_order_acquire); }
    /** 0.7: registry index of the parameter modulation target "Param j+1" (j = 0..3) moves for type `t`. */
    int modParamIndexOf (int t, int j) const noexcept
    {
        const IEffect* e = t > Off && t < numTypes ? effects[(size_t) t].get() : nullptr;
        return paramIndexOf (t, e != nullptr ? e->modulationParam (j) : j);
    }
    /** Registry index of parameter `local` of type `t` (-1 when not registered). */
    int paramIndexOf (int t, int local) const noexcept
    {
        return t > Off && t < numTypes && local >= 0 && local < (int) paramIndex[(size_t) t].size() ? paramIndex[(size_t) t][(size_t) local] : -1;
    }
    int typeParamIndex() const noexcept { return typeIndex; }
    void reset() noexcept;
    void pullParams (const ParamSnapshot& snap) noexcept;

    /** In place; numSamples <= maxBlockSize. */
    void process (float* left, float* right, int numSamples) noexcept;
    /** 0.7: tempo for the synced types, note starts of the part (retrigger). Audio thread. */
    void setTempo (double bpm) noexcept;
    void noteOn() noexcept;
    /** 1.0: song position for tempo-synced motion, master monitoring mode (headphones / speakers). Audio thread. */
    void setTransportPosition (double ppq, bool playing) noexcept;
    void setMonitorMode (int mode) noexcept;

    int currentType() const noexcept { return type; }
    IEffect* effect (int t) noexcept { return t > Off && t < numTypes ? effects[(size_t) t].get() : nullptr; }

    static bool isWetOnly (int t) noexcept { return t == Delay || t == Reverb; }

private:
    static std::unique_ptr<IEffect> create (int t);

    std::array<std::unique_ptr<IEffect>, numTypes> effects;
    std::array<std::vector<int>, numTypes> paramIndex;
    std::array<std::vector<float>, numTypes> lastValue;
    std::array<int, numTypes> mixIndex {};
    std::array<std::atomic<bool>, numTypes> ready {};
    std::atomic<bool> waiting { false };
    bool lazy = false;
    double preparedRate = 0.0;
    int preparedBlock = 0;
    int typeIndex = -1, type = Off;
    float wetMix = 0.35f;
    std::vector<float> dryL, dryR;
};
} // namespace ohpe
