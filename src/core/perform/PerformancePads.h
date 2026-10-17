// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"

#include <array>
#include <atomic>
#include <vector>

namespace ohpe
{
/**
    p25: performance pads ("Performance-Pads") - four throws on the main mix for live sets.

    * Stutter ("pads.stutter"): from the next grid line it catches one slice of "pads.stutter.div" (1/4 .. 1/32,
      triplets) and repeats it while held.
    * Tape stop ("pads.stop"): the mix slows down to a standstill over "pads.stop.len" (1/8 .. 2 bars), pitch and
      speed falling together like a stopping tape; silent once stopped.
    * Filter sweep ("pads.sweep"): a resonant low-pass closes (or a high-pass opens, "pads.sweep.mode") over
      "pads.sweep.len" (1/8 .. 4 bars), resonance "pads.sweep.res"; on release it sweeps back in a quarter of the time.
    * Reverb freeze ("pads.freeze"): throws the next moment of the mix (about 0.2 s) into a reverb and holds it
      endlessly while held; on release the wash decays (about 3 s). Level "pads.freeze.mix"; the dry mix plays on.
    * Engaging waits for the next line of "pads.quant" (Off, 1/16 .. 1 bar) while the transport runs (at once while
      it stops); releasing is immediate with a short fade. Each pad is a host parameter: MIDI-learnable, automatable.
    * Matrix: "Pad Sweep" (the sweep position 0 .. 1) and "Pads Held" (1 while a pad is engaged) are Part sources.
    * Nothing held, nothing ringing: the mix passes untouched (bit-identical, isActive() == false).

    Place: the master chain (after the master FX rack, before compressor, saturation and limiter), so the limiter
    guards what the pads do. The aux outputs are not affected. Audio thread except the ui* readers (any thread).
*/
class PerformancePads
{
public:
    enum Pad { Stutter = 0, TapeStop, Sweep, Freeze, numPads };
    enum State { Idle = 0, Pending, Engaged, Releasing }; ///< ui: what a pad does right now

    static constexpr double maxSliceSeconds = 4.0;
    static constexpr double maxStopSeconds = 6.0;

    /** Registers the "pads.*" block (global, append-only). */
    static void registerParams (ParamRegistry& reg);
    /** Grid of a "pads.quant" choice in quarter notes (0 = off). */
    static double quantBeats (int choice) noexcept;
    static double stutterBeats (int choice) noexcept;
    static double lengthBeats (int choice) noexcept; ///< "pads.stop.len" / "pads.sweep.len"
    /** The parameter IDs of the four pads ("pads.stutter" ...). */
    static const char* padId (int pad) noexcept;

    void bind (const ParamRegistry& reg);
    void prepare (double sampleRate);
    void reset() noexcept;
    void pullParams (const ParamSnapshot& snap) noexcept;
    /** Before every process(): tempo and position at the first sample (quarter notes). */
    void setTransport (double bpm, double ppq, bool playing) noexcept;
    /** Something to do (a pad held, pending, fading or ringing). False = process() would not change the signal. */
    bool isActive() const noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

    /** Matrix sources (audio thread, after process()). */
    float sweepSource() const noexcept { return sweepPos; }
    float heldSource() const noexcept;

    // ---- any thread (display)
    State uiState (int pad) const noexcept { return (State) uiStates[(size_t) pad].load (std::memory_order_relaxed); }
    float uiSweep() const noexcept { return uiSweepPos.load (std::memory_order_relaxed); }
    float uiStopSpeed() const noexcept { return uiSpeed.load (std::memory_order_relaxed); }
    float uiStutterPhase() const noexcept { return uiPhase.load (std::memory_order_relaxed); }
    float uiFreezeLevel() const noexcept { return uiFreeze.load (std::memory_order_relaxed); }

private:
    bool onGrid (double ppqAtSample, double grid) const noexcept;
    void engage (int pad) noexcept;
    float freezeTick (float inL, float inR, float& outR) noexcept;

    std::array<int, numPads> padIdx { -1, -1, -1, -1 };
    int quantIdx = -1, divIdx = -1, stopLenIdx = -1, sweepModeIdx = -1, sweepLenIdx = -1, sweepResIdx = -1, freezeMixIdx = -1;

    std::array<bool, numPads> want {};
    std::array<State, numPads> state {};
    double sr = 48000.0, bpm = 120.0, ppq = 0.0;
    bool playing = false;
    int quant = 1, div = 2, stopLen = 2, sweepMode = 0, sweepLen = 3;
    float sweepRes = 0.45f, freezeMix = 0.6f;
    float fadeStep = 0.0f; ///< per sample: 1 / fade length (4 ms)

    // stutter
    std::vector<float> sliceL, sliceR;
    int sliceLen = 0, capPos = 0, playPos = 0;
    bool capturing = false;
    float stMix = 0.0f;
    // tape stop
    std::vector<float> ringL, ringR;
    long long writeCount = 0;
    double readPos = 0.0, stopElapsed = 0.0, stopDuration = 1.0;
    float stopMix = 0.0f, speed = 1.0f;
    // filter sweep
    std::array<dsp::Svf, 2> svf;
    float sweepPos = 0.0f;
    int coeffCountdown = 0;
    // reverb freeze: four-line feedback network, Hadamard mixing
    static constexpr int numLines = 4;
    std::array<std::vector<float>, numLines> lines;
    std::array<int, numLines> lineLen {}, linePos {};
    std::array<float, numLines> damp {};
    float inGate = 0.0f, feedback = 0.0f, freezeEnv = 0.0f;
    double throwElapsed = 0.0;
    bool ringing = false;

    std::array<std::atomic<int>, numPads> uiStates {};
    std::atomic<float> uiSweepPos { 0.0f }, uiSpeed { 1.0f }, uiPhase { 0.0f }, uiFreeze { 0.0f };
};
} // namespace ohpe
