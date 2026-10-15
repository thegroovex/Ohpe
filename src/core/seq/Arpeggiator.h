// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"
#include "SeqCommon.h"

#include <array>
#include <atomic>

namespace ohpe
{
/**
    Arpeggiator for one part: the notes played into its target part (keyboard, MIDI, one-finger
    chords) are held and played one after another in the chosen order over 1-4 octaves.

    Timing: while the transport runs the steps sit on the transport's grid (ppq-locked, also in
    a host); while it is stopped the arpeggiator runs free at the current tempo, starting
    exactly on the first key. Latch keeps the notes after the keys are released; the next key
    pressed with no key held starts a new set.
*/
class Arpeggiator final : public ISequencer
{
public:
    static constexpr int maxHeld = 32;

    enum Param { On = 0, Part, Mode, Octaves, Gate, Rate, Latch, numParams };
    enum ModeChoice { Up = 0, Down, UpDown, Random, Order, Chord, numModes };

    const char* id() const noexcept override { return "arp"; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float plainValue) noexcept override;

    /** Audio thread: a note on / off for the target part, before process() of the same block. */
    void noteInput (const MidiEvent& e) noexcept;
    /** Produces the arpeggio for this block (inputs given with noteInput are consumed). */
    void process (const Transport& transport, int numSamples, SequencerOutput& out) noexcept override;
    /** Releases everything (arp switched off or retargeted). Pending note-offs still go out. */
    void releaseAll() noexcept;

    int playPosition() const noexcept override { return position.load (std::memory_order_relaxed); }
    std::string toJson() const override { return "{\"format\":\"ohpe-arp\",\"version\":1}"; }
    bool fromJson (const std::string&) override { return true; }

    bool enabled() const noexcept { return on; }
    int targetPart() const noexcept { return part; }
    int heldCount() const noexcept { return numHeld; }

private:
    struct Held { int note; int velocity; };
    void applyInput (const MidiEvent& e) noexcept;
    void playStep (int64_t time, double samplesPerStep) noexcept;
    int buildSorted (std::array<Held, maxHeld>& out) const noexcept;

    bool on = false, latch = false;
    int part = 3, mode = Up, octaves = 1, rate = seq::R1_16;
    float gate = 0.5f;
    double sr = 48000.0;

    std::array<Held, maxHeld> held {}; ///< in the order played
    int numHeld = 0;
    std::array<bool, 128> physical {};  ///< keys physically down
    int numPhysical = 0;

    std::array<MidiEvent, 256> inputs {};
    int numInputs = 0;

    int64_t sampleClock = 0;
    int64_t stepCount = 0;
    bool freeRunning = false, wasSynced = false;
    double freePpq = 0.0;
    seq::GridClock clock;
    seq::NoteQueue queue;
    dsp::Random rng { 0xa4d1 };
    std::atomic<int> position { -1 };
};
} // namespace ohpe
