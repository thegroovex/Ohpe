// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace ohpe
{
/**
    p35: the CPU meter - how much of the time an audio block stands for the processing of that block took.

    1.0 = the whole block (the audio thread had no time left; above that the output breaks up). The plug-in measures
    every processBlock() and hands the result to add(); the UI reads average() and peak() at its own pace.
    * average: follows the load with a time constant of 0.5 s of audio (calm enough to read),
    * peak: the slowest block, held for 2 s, then falling with 0.5 s.
    Audio thread: add(), reset(). Any thread: average(), peak(). No allocation, no locks. Not a parameter (not saved,
    not automatable).
*/
class CpuLoad
{
public:
    static constexpr double averageSeconds = 0.5, peakHoldSeconds = 2.0;

    void add (double processSeconds, double blockSeconds) noexcept
    {
        if (! (blockSeconds > 0.0) || ! (processSeconds >= 0.0) || ! std::isfinite (processSeconds)) return;
        const double load = processSeconds / blockSeconds;
        const double k = 1.0 - std::exp (-blockSeconds / averageSeconds);
        avg = first ? load : avg + (load - avg) * k;
        first = false;
        if (load >= pk) { pk = load; held = 0.0; }
        else if ((held += blockSeconds) > peakHoldSeconds) pk = std::max (load, pk * (1.0 - k));
        avgOut.store ((float) avg, std::memory_order_relaxed);
        peakOut.store ((float) pk, std::memory_order_relaxed);
    }

    void reset() noexcept
    {
        avg = pk = held = 0.0;
        first = true;
        avgOut.store (0.0f, std::memory_order_relaxed);
        peakOut.store (0.0f, std::memory_order_relaxed);
    }

    float average() const noexcept { return avgOut.load (std::memory_order_relaxed); }
    float peak() const noexcept { return peakOut.load (std::memory_order_relaxed); }

private:
    double avg = 0.0, pk = 0.0, held = 0.0;
    bool first = true;
    std::atomic<float> avgOut { 0.0f }, peakOut { 0.0f };
};
} // namespace ohpe
