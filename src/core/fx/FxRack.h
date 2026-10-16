// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "InsertSlot.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace ohpe
{
/**
    0.7: the FX rack - serial insert slots, each an InsertSlot with its own type and parameter blocks.

    Part racks have six slots: slot 1 is the 0.2 FX slot ("pN.fx.*", so every older preset and project
    loads into slot 1 unchanged), slots 2-6 are "pN.fx2.*" ... "pN.fx6.*". The master rack has four slots
    "mfx1.*" ... "mfx4.*" (between the send returns and the master dynamics). Every slot adds "mix"
    (dry / wet of the whole slot, 1 = the effect's own output) and "bypass" ("pN.fx.mix", "pN.fx2.bypass").

    Slots run in order 1, 2, ... ; reordering moves the parameters of the slots (moveSlot, message thread),
    so the order is part of every preset and automatable like any other value. A slot with type Off or
    bypass on is skipped: an empty rack is an exact bypass, and so is a slot at mix 1 with the effect's
    own parameters (bit-identical to the 0.2 single slot).

    Real-time: process() and pullParams() run on the audio thread without allocation. Slots 2-6 and the
    master slots prepare effect types lazily (see InsertSlot); prepareSelected() on the message thread.
*/
class FxRack
{
public:
    static constexpr int numPartSlots = 6;
    static constexpr int numMasterSlots = 4;
    static constexpr int maxSlots = 6;

    /** Prefix of slot `slot` (0-based) of a part rack, e.g. ("p2.", 0) -> "p2.fx.", ("p2.", 2) -> "p2.fx3.". */
    static std::string partSlotPrefix (const std::string& partPrefix, int slot);
    /** Prefix of slot `slot` (0-based) of the master rack: "mfx1." ... */
    static std::string masterSlotPrefix (int slot);
    /** Every slot prefix of every rack (parts 1-4, then master) - for registering later type blocks. */
    static std::vector<std::string> allSlotPrefixes (int numParts);
    /** Parameter group of a slot prefix ("part2/fx", "master/fx"). */
    static std::string groupOf (const std::string& slotPrefix);

    // ---- layout (append-only; ParameterLayout.cpp) ----
    /** Slots 2..numSlots of a part rack: type + 0.2 type blocks (slot 1 is registered by the 0.2 layout). */
    static void registerExtraSlots (ParamRegistry& reg, const std::string& partPrefix, const std::string& group);
    /** "mix" and "bypass" of every slot of a part rack. */
    static void registerSlotControls (ParamRegistry& reg, const std::string& partPrefix, const std::string& group);
    /** The master rack: per slot type + 0.2 blocks, then mix / bypass of every slot. */
    static void registerMaster (ParamRegistry& reg);
    /** Parameter blocks of the types [fromType, toType) for every slot of every rack (types added later). */
    static void registerTypeBlocks (ParamRegistry& reg, int numParts, int fromType, int toType);

    explicit FxRack (int numSlots = numPartSlots);

    /** Binds to slot prefixes (partSlotPrefix / masterSlotPrefix). Slot 1 of a part rack prepares eagerly. */
    void bindPart (const ParamRegistry& reg, const std::string& partPrefix);
    void bindMaster (const ParamRegistry& reg);
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void pullParams (const ParamSnapshot& snap) noexcept;
    /** In place; numSamples <= maxBlockSize. */
    void process (float* left, float* right, int numSamples) noexcept;

    /** Message thread: prepares the selected types of all slots; true when something was prepared. */
    bool prepareSelected (const ParamRegistry& reg);
    /** Any slot is waiting for prepareSelected(). */
    bool isWaiting() const noexcept;
    /** True when any slot would process something (type set, not bypassed). Audio thread. */
    bool isActive() const noexcept { return activeSlots > 0; }

    int numSlots() const noexcept { return slotCount; }
    InsertSlot& slot (int i) noexcept { return slots[(size_t) i]; }
    const InsertSlot& slot (int i) const noexcept { return slots[(size_t) i]; }
    const std::string& slotPrefix (int i) const noexcept { return prefixes[(size_t) i]; }
    int mixParamIndex (int i) const noexcept { return mixIndex[(size_t) i]; }
    int bypassParamIndex (int i) const noexcept { return bypassIndex[(size_t) i]; }
    /** Output peak of slot `i` during the last block (UI meters / visuals, any thread). */
    float slotPeak (int i) const noexcept { return peaks[(size_t) i].load (std::memory_order_relaxed); }

    /**
        Message thread: moves slot `from` to position `to` (0-based), shifting the slots in between, by
        rewriting the parameters of the slots concerned (type, mix, bypass and every type block value).
        Returns false when nothing moved. Hosts see ordinary parameter changes.
    */
    static bool moveSlot (const ParamRegistry& reg, const std::vector<std::string>& slotPrefixes, int from, int to,
                          const std::function<void (int index, float plainValue)>& setParam);
    /** The slot prefixes of a part rack / the master rack. */
    static std::vector<std::string> partPrefixes (const std::string& partPrefix);
    static std::vector<std::string> masterPrefixes();

private:
    void bindSlots (const ParamRegistry& reg);

    int slotCount;
    std::array<InsertSlot, maxSlots> slots;
    std::array<std::string, maxSlots> prefixes;
    std::array<int, maxSlots> mixIndex {}, bypassIndex {};
    std::array<float, maxSlots> mix {}, mixSmooth {};
    std::array<bool, maxSlots> bypass {};
    std::array<std::atomic<float>, maxSlots> peaks {};
    std::vector<float> dryL, dryR;
    float smoothCoeff = 0.01f;
    int activeSlots = 0;
};
} // namespace ohpe
