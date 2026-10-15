// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiClock.h"

#include <algorithm>
#include <cmath>

namespace ohpe
{
void MidiClockFollower::reset() noexcept
{
    samplesPerTick = 0.0;
    historyCount = historyPos = 0;
    outliers = 0;
    running = false;
    positionTicks = 0;
    anchorPpq = 0.0;
    anchorTime = 0.0;
    anchored = false;
}

void MidiClockFollower::setSmoothing (float amount) noexcept
{
    const float a = std::clamp (amount, 0.0f, 1.0f);
    window = 1 + (int) std::lround (a * (maxWindow - 1)); // 1 .. 48 tick intervals
    phaseGain = 1.0 - 0.8 * a;                            // 1 .. 0.2
}

void MidiClockFollower::handle (const MidiEvent& e, int64_t t) noexcept
{
    switch (e.status)
    {
        case midiclock::start:
            running = true;
            positionTicks = 0;
            anchored = false;
            return;
        case midiclock::cont:
            running = true;
            anchored = false;
            return;
        case midiclock::stop:
            running = false;
            return;
        case midiclock::songPosition:
            positionTicks = (int64_t) (((int) e.data1 & 0x7f) | (((int) e.data2 & 0x7f) << 7)) * 6;
            anchorPpq = (double) positionTicks / midiclock::ticksPerQuarter;
            anchored = false;
            return;
        case midiclock::tick:
            break;
        default:
            return;
    }

    // tempo: average interval over the last `window` ticks (jitter of single ticks cancels out);
    // a single wild interval (dropped / doubled tick) is ignored, three in a row reset the history
    constexpr int cap = maxWindow + 1;
    if (historyCount > 0)
    {
        const int64_t prev = history[(size_t) ((historyPos + cap - 1) % cap)];
        const double dt = (double) (t - prev);
        if (dt <= 0.0) return;
        if (samplesPerTick > 0.0 && (dt > samplesPerTick * 1.8 || dt < samplesPerTick * 0.55))
        {
            if (++outliers < 3) return;
            historyCount = 0; // a real tempo change or a restart of the clock
            samplesPerTick = 0.0;
        }
        outliers = 0;
    }
    history[(size_t) historyPos] = t;
    historyPos = (historyPos + 1) % cap;
    historyCount = std::min (historyCount + 1, cap);
    if (historyCount >= 2)
    {
        const int n = std::min (historyCount - 1, window);
        const int64_t oldest = history[(size_t) ((historyPos + cap - 1 - n) % cap)];
        samplesPerTick = (double) (t - oldest) / n;
    }

    if (! running) return;
    // position: the first tick after start / continue is the song position; later ticks pull the
    // smoothed position towards themselves
    const double ppq = (double) positionTicks / midiclock::ticksPerQuarter;
    if (! anchored || samplesPerTick <= 0.0)
        anchorTime = (double) t;
    else
    {
        const double predicted = anchorTime + samplesPerTick * (ppq - anchorPpq) * midiclock::ticksPerQuarter;
        const double err = (double) t - predicted;
        anchorTime = std::abs (err) > samplesPerTick * 0.5 ? (double) t : predicted + phaseGain * err;
    }
    anchorPpq = ppq;
    anchored = true;
    ++positionTicks;
}

Transport MidiClockFollower::transportAt (int64_t start, double fallbackBpm) const noexcept
{
    Transport tr;
    tr.hostDriven = false;
    tr.bpm = hasTempo() ? bpm() : fallbackBpm;
    tr.playing = running && anchored && tr.bpm > 0.0;
    if (! tr.playing)
    {
        tr.ppqAtBlockStart = anchorPpq;
        return tr;
    }
    const double spt = 60.0 * sr / (tr.bpm * midiclock::ticksPerQuarter);
    // extrapolate from the last tick, but never more than 1.5 ticks ahead (clock stalled)
    const double ticks = std::min (((double) start - anchorTime) / spt, 1.5);
    tr.ppqAtBlockStart = anchorPpq + ticks / midiclock::ticksPerQuarter;
    return tr;
}
} // namespace ohpe
