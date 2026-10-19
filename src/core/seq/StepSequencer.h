// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"
#include "../model/ParamRegistry.h"
#include "ISequencer.h"

#include <array>
#include <cstdint>
#include <atomic>

#include <nlohmann/json_fwd.hpp>

namespace ohpe
{
/**
    64-step sequencer with per-step note, velocity, gate, probability, ratchets (1-8) and up
    to four parameter locks per step. Playback settings are parameters ("seq.step.*").

    16 patterns: "seq.step.pattern" picks the playing pattern (a change waits for the end of
    the pattern playing); with "seq.step.song" on, the chain (song) plays instead: up to 64
    entries of pattern + repeats, looping. Each pattern may have its own length (0 = the
    "seq.step.length" parameter). The editing API without a pattern number works on the edit
    pattern (setEditPattern), pattern 1 by default.

    Pattern storage is lock-free: every step is packed into atomics, so the UI may edit while
    the audio thread plays without tearing a step.
*/
class StepSequencer final : public ISequencer
{
public:
    static constexpr int maxSteps = 64;
    static constexpr int maxLocksPerStep = 4;
    static constexpr int numPatterns = 16;
    static constexpr int maxChain = 64;

    struct ChainEntry
    {
        int pattern = 0; ///< 0-based
        int repeats = 1; ///< 1..16
    };

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
        float gate = 0.5f;     ///< fraction of the step length, 0.05..1 (1 = tie: into the next step when it plays the same note, 0.5)
        int probability = 100; ///< 0..100 %
        int ratchet = 1;       ///< 1..8 hits inside the step
        std::array<Lock, maxLocksPerStep> locks {};
    };

    enum Param { Length = 0, Rate, Swing, Transpose, Channel, numParams, Pattern = numParams, Song, numAllParams };
    enum RateChoice { R1_4 = 0, R1_8, R1_16, R1_32, R1_8T, R1_16T };

    explicit StepSequencer (const ParamRegistry* registry = nullptr) : reg (registry) {}

    const char* id() const noexcept override { return "step"; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    /** Pattern / song parameters (0.2, registered after the 0.1 list; local indices Pattern, Song). */
    std::vector<ParamDescriptor> getChainParamDescriptors() const;
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
    void clearPattern() noexcept; ///< clears every pattern, the pattern lengths and the chain

    Step getStep (int pattern, int index) const noexcept;
    void setStep (int pattern, int index, const Step& step) noexcept;
    void setEditPattern (int pattern) noexcept { editPattern.store (std::clamp (pattern, 0, numPatterns - 1)); }
    int getEditPattern() const noexcept { return editPattern.load(); }
    /** Own length of a pattern (1..64) or 0 = follow "seq.step.length". */
    void setPatternLength (int pattern, int length) noexcept;
    /** p59: the length a pattern plays (its own, or "seq.step.length"). */
    int effectiveLength (int pattern) const noexcept { return lengthOf (pattern); }
    /** p59 (controller Double): copies steps 0 .. len-1 of `pattern` to len .. 2 len-1 (len = effectiveLength) and
        returns the doubled length to set; 0 = it would not fit (more than maxSteps), nothing changed. */
    int doubleSteps (int pattern) noexcept;
    /** p59 (controller Duplicate): pattern `from` (steps, locks, own length) into pattern `to`. */
    void copyPattern (int from, int to) noexcept;
    int getPatternLength (int pattern) const noexcept;
    void setChain (const std::vector<ChainEntry>& entries) noexcept;
    std::vector<ChainEntry> getChain() const;
    /** Pattern playing now (0-based, UI), -1 when stopped. */
    int playingPatternIndex() const noexcept { return playPattern.load (std::memory_order_relaxed); }
    /** Chain entry playing now (song mode, UI), -1 otherwise. */
    int playingChainEntry() const noexcept { return playEntry.load (std::memory_order_relaxed); }

    void setRegistry (const ParamRegistry* r) noexcept { reg = r; }
    /** 0.5: the core tells the sequencer when it plays the drum kit (notes to the kit are never tied). */
    void setDrumsMode (bool drums) noexcept { drumsMode = drums; }
    /** p45: Scale Lock - the transpose moves in degrees of the key (mask relative to root), the step notes snap to it. */
    void setKeyLock (bool on, int root, uint16_t mask) noexcept
    {
        keyLock = on;
        keyLockRoot = root;
        keyLockMask = mask;
    }

    static double stepsPerQuarter (int rateChoice) noexcept;

private:
    nlohmann::json patternToJson (int pattern) const;
    void patternFromJson (int pattern, const nlohmann::json& arr);
    static uint64_t pack (const Step& s) noexcept;
    static void unpack (uint64_t v, Step& s) noexcept;
    static uint32_t packLock (const Lock& l) noexcept;
    static Lock unpackLock (uint32_t v) noexcept;

    int lengthOf (int pattern) const noexcept;
    void locate (long long k, int& pattern, int& stepIndex) noexcept;
    void triggerStep (int pattern, int stepIndex, int offset, double samplesPerStep, int64_t blockStart, SequencerOutput& out) noexcept;
    void schedule (int64_t time, const MidiEvent& e) noexcept;

    const ParamRegistry* reg = nullptr;
    std::array<std::array<std::atomic<uint64_t>, maxSteps>, numPatterns> steps {};
    std::array<std::array<std::array<std::atomic<uint32_t>, maxLocksPerStep>, maxSteps>, numPatterns> locks {};
    std::array<std::atomic<int>, numPatterns> patternLength {};
    std::array<std::atomic<uint32_t>, maxChain> chain {};
    std::atomic<int> chainLength { 0 };
    std::atomic<int> editPattern { 0 };
    int requestedPattern = 0, playingPattern = 0;
    long long patternStart = 0;
    bool songMode = false;
    std::atomic<int> playPattern { -1 }, playEntry { -1 };

    int length = 16, rate = R1_16, transpose = 0, channel = 1;
    float swing = 0.0f;
    bool drumsMode = false;
    bool keyLock = false;    ///< p45
    int keyLockRoot = 0;
    uint16_t keyLockMask = 0xfff;
    int shiftNote (int note, int by) const noexcept; ///< p45: + transpose (semitones, or degrees with Scale Lock)
    bool extendOff (int channel, int note, int64_t time) noexcept;
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
