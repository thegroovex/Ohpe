// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "SeqCommon.h"

#include <array>
#include <atomic>

namespace ohpe
{
/**
    Orbit: a polymetric sequencer. Four lanes of up to 16 steps, each with its own length and
    rate, all playing into one part. Lanes of different lengths drift against each other and
    meet again after the least common multiple of their lengths. Every lane position is derived
    from the absolute transport position, so the lanes stay aligned through loops and jumps.

    Lane settings are parameters ("seq.orbit.*"), the steps are pattern data (state section
    "orbit"), packed into atomics for lock-free editing.
*/
class OrbitSequencer final : public ISequencer
{
public:
    static constexpr int numLanes = 4;
    static constexpr int maxSteps = 16;

    enum Param { On = 0, Part, Gate, firstLaneParam };
    enum LaneParam { Length = 0, Rate, Transpose, numLaneParams };
    static constexpr int numParams = firstLaneParam + numLanes * numLaneParams;
    /** 0.5: parameters registered later (local indices continue after numParams). */
    enum LateParam { Swing = numParams, Human, LanePart1, numAllParams = LanePart1 + numLanes };
    /** 0.5: lanes with their own target part send on channel part + this offset (the core routes them
        to that part, never to the drum kit). */
    static constexpr int lanePartChannelOffset = 8;

    struct Step
    {
        bool on = false;
        int note = 60;
        int velocity = 100;
    };

    const char* id() const noexcept override { return "orbit"; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    /** 0.5: swing, humanise and the lane targets ("seq.orbit.swing", "human", "l1.part" .. "l4.part"; 0 = the
        Orbit target part or the drum kit), registered at the end of the layout. */
    std::vector<ParamDescriptor> getLateParamDescriptors() const;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float plainValue) noexcept override;
    void process (const Transport& transport, int numSamples, SequencerOutput& out) noexcept override;
    int playPosition() const noexcept override { return lanePosition (0); }
    std::string toJson() const override;
    bool fromJson (const std::string& json) override;

    // ---- pattern editing (any thread, lock-free) ----
    Step getStep (int lane, int index) const noexcept;
    void setStep (int lane, int index, const Step& s) noexcept;
    void clearPattern() noexcept;

    bool enabled() const noexcept { return on; }
    /** 0.5: the core tells Orbit when it plays the drum kit (notes to the kit are never tied). */
    void setDrumsMode (bool drums) noexcept { drumsMode = drums; }
    int targetPart() const noexcept { return part; }
    int lanePosition (int lane) const noexcept { return positions[(size_t) lane].load (std::memory_order_relaxed); }
    int laneLength (int lane) const noexcept { return lanes[(size_t) lane].length; }

private:
    static uint32_t pack (const Step& s) noexcept;
    static Step unpack (uint32_t v) noexcept;

    struct Lane
    {
        int length = maxSteps, rate = seq::R1_16, transpose = 0, part = 0; ///< part 0 = the Orbit target
        seq::GridClock clock;
    };
    std::array<Lane, numLanes> lanes {};
    std::array<std::array<std::atomic<uint32_t>, maxSteps>, numLanes> steps {};
    bool on = false, wasPlaying = false;
    int part = 4;
    float gate = 0.5f;
    float swing = 0.0f, human = 0.0f;
    bool drumsMode = false;
    seq::Humaniser humaniser;
    double sr = 48000.0;
    int64_t sampleClock = 0;
    seq::NoteQueue queue;
    std::array<std::atomic<int>, numLanes> positions {};
};
} // namespace ohpe
