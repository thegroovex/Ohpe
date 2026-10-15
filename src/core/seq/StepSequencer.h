// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"
#include "../model/ParamRegistry.h"
#include "ISequencer.h"

#include <array>
#include <atomic>

namespace ohpe
{
/**
    64-step sequencer with per-step note, velocity, gate, probability, ratchets (1-8) and up
    to four parameter locks per step. Playback settings are parameters ("seq.step.*").

    Pattern storage is lock-free: every step is packed into atomics, so the UI may edit while
    the audio thread plays without tearing a step.
*/
class StepSequencer final : public ISequencer
{
public:
    static constexpr int maxSteps = 64;
    static constexpr int maxLocksPerStep = 4;

    struct Lock
    {
        int paramIndex = -1;   ///< registry index, -1 = empty slot
        float normValue = 0.0f;
    };

    struct Step
    {
        bool on = false;
        int note = 60;         ///< 0..127
        int velocity = 100;    ///< 1..127
        float gate = 0.5f;     ///< fraction of the step length, 0.05..1 (1 = tie length)
        int probability = 100; ///< 0..100 %
        int ratchet = 1;       ///< 1..8 hits inside the step
        std::array<Lock, maxLocksPerStep> locks {};
    };

    enum Param { Length = 0, Rate, Swing, Transpose, Channel, numParams };
    enum RateChoice { R1_4 = 0, R1_8, R1_16, R1_32, R1_8T, R1_16T };

    explicit StepSequencer (const ParamRegistry* registry = nullptr) : reg (registry) {}

    const char* id() const noexcept override { return "step"; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (const Transport& transport, int numSamples, SequencerOutput& out) noexcept override;
    int playPosition() const noexcept override { return position.load (std::memory_order_relaxed); }

    std::string toJson() const override;
    bool fromJson (const std::string& json) override;

    // ---- pattern editing (message thread; lock-free towards the audio thread) ----
    Step getStep (int index) const noexcept;
    void setStep (int index, const Step& step) noexcept;
    void toggleStep (int index, int note = 60) noexcept;
    void clearPattern() noexcept;
    void setRegistry (const ParamRegistry* r) noexcept { reg = r; }

    static double stepsPerQuarter (int rateChoice) noexcept;

private:
    static uint64_t pack (const Step& s) noexcept;
    static void unpack (uint64_t v, Step& s) noexcept;
    static uint32_t packLock (const Lock& l) noexcept;
    static Lock unpackLock (uint32_t v) noexcept;

    void triggerStep (int stepIndex, int offset, double samplesPerStep, int64_t blockStart, SequencerOutput& out) noexcept;
    void schedule (int64_t time, const MidiEvent& e) noexcept;

    const ParamRegistry* reg = nullptr;
    std::array<std::atomic<uint64_t>, maxSteps> steps {};
    std::array<std::array<std::atomic<uint32_t>, maxLocksPerStep>, maxSteps> locks {};

    int length = 16, rate = R1_16, transpose = 0, channel = 1;
    float swing = 0.0f;
    double sr = 48000.0;
    double lastStepPos = -1.0;
    int64_t sampleClock = 0;
    bool wasPlaying = false;
    std::atomic<int> position { -1 };
    dsp::Random rng { 0x5eed };

    struct Pending
    {
        int64_t time = 0;
        MidiEvent event;
        bool used = false;
    };
    std::array<Pending, 256> pending {};
    std::array<int, 128> noteRefs {};
};
} // namespace ohpe
