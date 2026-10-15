// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ohpe
{
/** MIDI system real-time / common status bytes used for clock sync. */
namespace midiclock
{
constexpr uint8_t tick = 0xf8, start = 0xfa, cont = 0xfb, stop = 0xfc, songPosition = 0xf2;
constexpr int ticksPerQuarter = 24;

inline bool isClockMessage (const MidiEvent& e) noexcept
{
    return e.status == tick || e.status == start || e.status == cont || e.status == stop || e.status == songPosition;
}
} // namespace midiclock

/**
    Follows an incoming MIDI clock (24 ticks per quarter note, start / stop / continue, song
    position pointer) and turns it into a transport: tempo from the smoothed tick interval,
    position locked to the ticks. Ticks arrive with jitter (USB, drivers, the audio block
    grid); smoothing trades reaction time for a steadier tempo. Audio thread, no allocation.
*/
class MidiClockFollower
{
public:
    static constexpr int maxWindow = 48; ///< tick intervals averaged at most (two quarter notes)

    void prepare (double sampleRate) noexcept { sr = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() noexcept;

    /** 0 = raw tick intervals and positions, 1 = heavy smoothing (tempo over two quarter notes). */
    void setSmoothing (float amount) noexcept;

    /** Feeds one clock message at an absolute sample time (monotonic). Other messages are ignored. */
    void handle (const MidiEvent& e, int64_t absoluteTime) noexcept;

    /**
        The transport for a block starting at absoluteStart (call after feeding the block's
        messages). Until two ticks have arrived the tempo is fallbackBpm.
    */
    Transport transportAt (int64_t absoluteStart, double fallbackBpm) const noexcept;

    bool isRunning() const noexcept { return running; }
    bool hasTempo() const noexcept { return samplesPerTick > 0.0; }
    double bpm() const noexcept { return hasTempo() ? 60.0 * sr / (samplesPerTick * midiclock::ticksPerQuarter) : 0.0; }

private:
    double sr = 48000.0;
    int window = 24;          ///< tick intervals averaged for the tempo
    double phaseGain = 0.6;   ///< how far a tick pulls the position (1 = jump to every tick)
    double samplesPerTick = 0.0;
    std::array<int64_t, maxWindow + 1> history {}; ///< recent tick times (ring buffer)
    int historyCount = 0, historyPos = 0;
    int outliers = 0;
    bool running = false;
    int64_t positionTicks = 0; ///< song position of the next tick, in ticks
    double anchorPpq = 0.0;
    double anchorTime = 0.0;   ///< smoothed time of the last tick
    bool anchored = false;
};

/**
    Generates MIDI clock output from a transport: a tick at every 1/24 quarter note while the
    transport runs, start (or song position + continue when starting mid-song) and stop.
*/
class MidiClockGenerator
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() noexcept { wasPlaying = false; lastTick = -1.0; }

    template <int Cap>
    void process (const Transport& tr, int numSamples, EventList<Cap>& out) noexcept
    {
        if (! tr.playing || tr.bpm <= 0.0)
        {
            if (wasPlaying) out.add ({ 0, midiclock::stop, 0, 0 });
            wasPlaying = false;
            return;
        }
        const double samplesPerTick = sr * 60.0 / tr.bpm / midiclock::ticksPerQuarter;
        const double startTick = tr.ppqAtBlockStart * midiclock::ticksPerQuarter;
        const double endTick = startTick + numSamples / samplesPerTick;
        const bool jumped = wasPlaying && (startTick < lastTick - 0.5 || startTick > lastTick + 2.0 + numSamples / samplesPerTick);
        if (! wasPlaying || jumped)
        {
            lastTick = std::ceil (startTick - 1.0e-6) - 1.0;
            if (jumped) out.add ({ 0, midiclock::stop, 0, 0 });
            if (startTick < 1.0e-3 && startTick > -1.0e-3)
                out.add ({ 0, midiclock::start, 0, 0 });
            else
            {
                // song position in 16th notes (6 ticks), then continue
                const int spp = std::clamp ((int) std::ceil (std::max (0.0, startTick) / 6.0 - 1.0e-6), 0, 16383);
                out.add ({ 0, midiclock::songPosition, (uint8_t) (spp & 0x7f), (uint8_t) ((spp >> 7) & 0x7f) });
                out.add ({ 0, midiclock::cont, 0, 0 });
                lastTick = spp * 6.0 - 1.0; // ticks resume at the announced position
            }
        }
        wasPlaying = true;
        for (double k = lastTick + 1.0; k < endTick - 1.0e-9; k += 1.0)
        {
            lastTick = k;
            if (k < startTick - 1.0e-6) continue;
            const int offset = std::clamp ((int) std::floor ((k - startTick) * samplesPerTick + 0.5), 0, numSamples - 1);
            out.add ({ offset, midiclock::tick, 0, 0 });
        }
    }

private:
    double sr = 48000.0;
    bool wasPlaying = false;
    double lastTick = -1.0;
};
} // namespace ohpe
