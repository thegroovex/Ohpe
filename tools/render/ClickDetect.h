// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p85: click / crackle detection for offline renders (tools/render/OhpeClickScan.cpp, tests/core/ClickRegressionTests.cpp).
// Material: realistic part playing (held note + release, held 4-note chord + release, legato line, velocities, loud chord),
// optional silence before it and before the last chord (the sleeping stages wake up). Rendering at a fixed host block size
// or a variable host pattern. Detector: second difference (steps and kinks) per 16-sample window against the median of the
// windows around it (+-13 ms): more than 8x (18 dB) above and above an absolute floor = a click; within -1..+8 ms of a MIDI
// event it counts as "at event" (attack / release), elsewhere as "free" (crackle). Exactly repeated non-zero samples
// (>= 24 in a row) count as holds (dropouts).

#include "OhpeCore.h"
#include "render/PresetRender.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace ohpe::clickscan
{
struct Ev
{
    int64_t at;
    MidiEvent e;
};

inline std::vector<Ev> partMaterial (double sr, double lead, double gap)
{
    std::vector<Ev> ev;
    // lead: silence before the material (stages fall asleep); gap: silence inserted before the loud chord
    auto s = [&] (double t) { return (int64_t) std::llround ((t + lead + (t >= 7.8 ? gap : 0.0)) * sr); };
    auto note = [&] (double on, double off, int n, int v) {
        ev.push_back ({ s (on), MidiEvent::noteOn (1, n, v) });
        ev.push_back ({ s (off), MidiEvent::noteOff (1, n) });
    };
    note (0.00, 1.20, 60, 100);
    for (int n : { 48, 55, 63, 67 }) note (1.80, 3.60, n, 90);
    note (4.20, 4.55, 60, 96); note (4.50, 4.85, 62, 96); note (4.80, 5.15, 63, 96); note (5.10, 5.45, 65, 96); note (5.40, 6.20, 67, 96);
    note (6.60, 6.85, 72, 30); note (7.00, 7.25, 72, 70); note (7.40, 7.65, 72, 127);
    for (int n : { 53, 60, 65, 68 }) note (7.90, 9.40, n, 110);
    std::stable_sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    return ev;
}

inline double worstLoad = 0.0, sumUs = 0.0; ///< worst block time / block duration, total processing time (last run)

struct Rng
{
    uint32_t s = 12345u;
    uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
};

/** Renders with a fixed block size (block > 0) or the variable host pattern (block == 0). */
inline render::Audio run (Core& core, const std::vector<Ev>& events, int64_t total, int block, std::vector<int64_t>& boundaries)
{
    render::Audio a;
    worstLoad = sumUs = 0.0;
    a.sampleRate = core.sampleRate();
    a.l.assign ((size_t) total, 0.0f);
    a.r.assign ((size_t) total, 0.0f);
    std::vector<MidiEvent> be;
    size_t next = 0;
    Rng rng;
    for (int64_t pos = 0; pos < total;)
    {
        int b = block;
        if (b <= 0)
        {
            const uint32_t r = rng.next() % 100;
            b = r < 55 ? 512 : r < 75 ? 16 + (int) (rng.next() % 497) : r < 90 ? 128 * (1 + (int) (rng.next() % 4)) : 1 + (int) (rng.next() % 31);
        }
        const int n = (int) std::min<int64_t> (b, total - pos);
        be.clear();
        while (next < events.size() && events[next].at < pos + n)
        {
            auto e = events[next].e;
            e.sampleOffset = (int) std::max<int64_t> (0, events[next].at - pos);
            be.push_back (e);
            ++next;
        }
        OutputBuffers out;
        out.left[0] = a.l.data() + pos;
        out.right[0] = a.r.data() + pos;
        Transport tr;
        const auto t0 = std::chrono::steady_clock::now();
        core.process (out, n, be.empty() ? nullptr : be.data(), (int) be.size(), tr);
        const double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count();
        const double budget = 1.0e6 * n / core.sampleRate();
        worstLoad = std::max (worstLoad, us / budget);
        sumUs += us;
        pos += n;
        boundaries.push_back (pos);
    }
    return a;
}

struct Result
{
    int freeClicks = 0, eventClicks = 0, holds = 0, onBoundary = 0, nonFinite = 0;
    double worstRatio = 0.0, firstFree = -1.0, peak = 0.0;
    std::vector<double> freeTimes;
};

inline Result detect (const render::Audio& a, const std::vector<int64_t>& eventTimes, const std::vector<int64_t>& boundaries, int latency)
{
    Result r;
    constexpr int W = 16, R = 40;
    const size_t n = a.size();
    const size_t nw = n / W;
    std::vector<char> nearEvent (nw, 0), nearBoundary (nw, 0);
    for (auto t : eventTimes)
    {
        const int64_t from = t + latency - (int64_t) (0.001 * a.sampleRate), to = t + latency + (int64_t) (0.008 * a.sampleRate);
        for (int64_t k = std::max<int64_t> (0, from / W); k <= to / W && k < (int64_t) nw; ++k) nearEvent[(size_t) k] = 1;
    }
    for (auto b : boundaries)
        for (int64_t k = (b - 2) / W; k <= (b + 2) / W; ++k)
            if (k >= 0 && k < (int64_t) nw) nearBoundary[(size_t) k] = 1;
    for (const auto* ch : { &a.l, &a.r })
    {
        const auto& x = *ch;
        std::vector<float> p (nw, 0.0f);
        for (size_t i = 2; i < n; ++i)
        {
            if (! std::isfinite (x[i])) { ++r.nonFinite; continue; }
            r.peak = std::max (r.peak, (double) std::abs (x[i]));
            const float h = std::abs (x[i] - 2.0f * x[i - 1] + x[i - 2]);
            auto& w = p[i / W < nw ? i / W : nw - 1];
            w = std::max (w, h);
        }
        std::vector<float> tmp;
        for (size_t k = 0; k < nw; ++k)
        {
            tmp.clear();
            for (int d = -R; d <= R; ++d)
            {
                if (std::abs (d) <= 2) continue;
                const int64_t j = (int64_t) k + d;
                if (j >= 0 && j < (int64_t) nw) tmp.push_back (p[(size_t) j]);
            }
            if (tmp.empty()) continue;
            std::nth_element (tmp.begin(), tmp.begin() + (long) tmp.size() / 2, tmp.end());
            const float bg = tmp[tmp.size() / 2];
            if (p[k] > 8.0f * bg && p[k] > 2.0e-3f)
            {
                const double ratio = p[k] / std::max (bg, 1.0e-7f);
                if (nearEvent[k]) { ++r.eventClicks; continue; }
                ++r.freeClicks;
                if (nearBoundary[k]) ++r.onBoundary;
                r.worstRatio = std::max (r.worstRatio, ratio);
                if (r.freeTimes.size() < 8) r.freeTimes.push_back ((double) (k * W) / a.sampleRate);
            }
        }
        // repeated samples inside a signal (dropouts / held buffers)
        int run = 0;
        for (size_t i = 1; i < n; ++i)
        {
            if (x[i] == x[i - 1] && x[i] != 0.0f) ++run;
            else
            {
                if (run >= 24) ++r.holds;
                run = 0;
            }
        }
    }
    if (! r.freeTimes.empty()) r.firstFree = r.freeTimes.front();
    return r;
}

} // namespace ohpe::clickscan
