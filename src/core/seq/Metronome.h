// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"

#include <array>
#include <cstdint>

namespace ohpe
{
/**
    p39: the metronome - Ohpe's own synthesised click (no samples): a short sine "tick" with a fast pitch drop and
    a few milliseconds of filtered noise on the attack; the downbeat (beat 1 of a 4/4 bar) can be accented (higher,
    louder). The core decides when it clicks (sequencer / tape running, the count-in); this class only renders.

    Parameters ("metro.", group "metronome", appended at the end of the layout; not part of presets - they belong to
    the player like the monitoring switch):
      metro.on      click on / off while the sequencer or the tape runs
      metro.level   click level (0 .. 1, squared to gain; 1 = -6 dBFS peak)
      metro.accent  accent on the downbeat
      metro.countin count-in for recording: Off, 1 Bar, 2 Bars (the click plays during the count-in even when off)
      metro.plugin  in a plug-in host the click is off unless this is on (the standalone always clicks when on)
      metro.tap     trigger: tap tempo (MIDI-mappable; the plug-in measures the taps)

    Audio thread: trigger(), render() (no allocation). Everything else on the set-up thread.
*/
class Metronome
{
public:
    struct Ids
    {
        static constexpr const char* on = "metro.on";
        static constexpr const char* level = "metro.level";
        static constexpr const char* accent = "metro.accent";
        static constexpr const char* countIn = "metro.countin";
        static constexpr const char* plugin = "metro.plugin";
        static constexpr const char* tap = "metro.tap";
    };
    static constexpr int beatsPerBar = 4; ///< Ohpe counts in 4/4 (bar.beat everywhere)

    static void registerParams (ParamRegistry& reg);
    void bind (const ParamRegistry& reg);
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Reads metro.* from the block's snapshot. */
    void pull (const ParamSnapshot& snap) noexcept;
    bool enabled() const noexcept { return on; }
    bool accentOn() const noexcept { return accent; }
    float levelValue() const noexcept { return level; }
    int countInBars() const noexcept { return countIn; }
    bool clickInPlugin() const noexcept { return plugin; }

    /** Starts a click at `offset` samples into the next render() (later triggers in one block replace earlier ones
        from their offset on). `downbeat` = beat 1 of the bar (accented when accent is on). */
    void trigger (int offset, bool downbeat) noexcept;
    /** Adds the clicks of this block to left / right (either may be null); numSamples <= any size. */
    void render (float* left, float* right, int numSamples) noexcept;
    /** True while a click is still sounding (tests, idle check). */
    bool sounding() const noexcept { return amp > 1.0e-5f || pendingCount > 0; }

private:
    void startClick (bool downbeat) noexcept;

    int idxOn = -1, idxLevel = -1, idxAccent = -1, idxCountIn = -1, idxPlugin = -1;
    bool on = false, accent = true, plugin = false;
    float level = 0.6f;
    int countIn = 0;

    double sr = 48000.0;
    // the voice
    double phase = 0.0;
    float freq = 0.0f, freqTarget = 0.0f, freqCoef = 0.0f;
    float amp = 0.0f, ampDecay = 0.0f, attack = 0.0f, attackStep = 0.0f;
    float noiseAmp = 0.0f, noiseDecay = 0.0f, noiseLp = 0.0f;
    float gain = 0.0f;
    uint32_t noiseState = 0x1234567u;
    // triggers of the current block
    struct Pending { int offset; bool downbeat; };
    std::array<Pending, 16> pending {};
    int pendingCount = 0;
};

/**
    p39: tap tempo - the tempo from the intervals between taps (seconds on any monotonic clock). A gap longer than
    2 seconds starts over; the tempo is the average of the last (up to) four intervals, 20 .. 300 BPM.
*/
class TapTempo
{
public:
    /** Returns the new tempo, or 0 when there is not one yet (the first tap). */
    double tap (double seconds) noexcept;
    void reset() noexcept { count = 0; last = -1.0; }
    int taps() const noexcept { return count; }

private:
    std::array<double, 4> intervals {};
    int count = 0, numIntervals = 0, next = 0;
    double last = -1.0;
};
} // namespace ohpe
