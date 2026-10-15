// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "SeqCommon.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace ohpe
{
/**
    Euclidean sequencer: four rings, each distributing `pulses` hits as evenly as possible over
    `steps` (Bjorklund's algorithm), rotated by `rotation` and playing its own note. Rings of
    different lengths run as a polymeter on one shared rate grid. Everything is a parameter
    ("seq.euclid.*"), so there is no pattern data to save.
*/
class EuclidSequencer final : public ISequencer
{
public:
    static constexpr int numRings = 4;
    static constexpr int maxSteps = 32;

    enum Param { On = 0, Part, Rate, Gate, Velocity, firstRingParam };
    enum RingParam { Steps = 0, Pulses, Rotation, Note, numRingParams };
    static constexpr int numParams = firstRingParam + numRings * numRingParams;

    /** Bjorklund pattern as a bit mask (bit i = step i hits), first step always a hit when pulses > 0. */
    static uint32_t pattern (int steps, int pulses) noexcept;
    /** Pattern rotated to the right by `rotation` steps (the hit at step 0 moves to step `rotation`). */
    static uint32_t rotated (uint32_t pattern, int steps, int rotation) noexcept;

    const char* id() const noexcept override { return "euclid"; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float plainValue) noexcept override;
    void process (const Transport& transport, int numSamples, SequencerOutput& out) noexcept override;
    int playPosition() const noexcept override { return position.load (std::memory_order_relaxed); }
    std::string toJson() const override { return "{\"format\":\"ohpe-euclid\",\"version\":1}"; }
    bool fromJson (const std::string&) override { return true; }

    bool enabled() const noexcept { return on; }
    int targetPart() const noexcept { return part; } ///< 1-based
    /** Ring step playing now (UI), -1 when stopped. */
    int ringPosition (int ring) const noexcept { return ringPos[(size_t) ring].load (std::memory_order_relaxed); }
    uint32_t ringPattern (int ring) const noexcept { return rings[(size_t) ring].mask; }
    int ringSteps (int ring) const noexcept { return rings[(size_t) ring].steps; }

private:
    struct Ring
    {
        int steps = 16, pulses = 0, rotation = 0, note = 36;
        uint32_t mask = 0;
    };
    void rebuild (Ring& r) noexcept { r.mask = rotated (pattern (r.steps, r.pulses), r.steps, r.rotation); }

    std::array<Ring, numRings> rings {};
    bool on = false;
    int part = 1, rate = seq::R1_16, velocity = 100;
    float gate = 0.5f;
    double sr = 48000.0;
    int64_t sampleClock = 0;
    bool wasPlaying = false;
    seq::GridClock clock;
    seq::NoteQueue queue;
    std::atomic<int> position { -1 };
    std::array<std::atomic<int>, numRings> ringPos {};
};
} // namespace ohpe
