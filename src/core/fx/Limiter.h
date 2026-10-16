// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"

#include <vector>

namespace ohpe
{
/**
    Master limiter (0.8 punch pass): 1.5 ms look-ahead, true-peak aware, ceiling -0.3 dBFS by default.

    The required gain of every sample (ceiling / peak, the peak including the inter-sample peaks of a cubic
    4x interpolation) is held as a running minimum over the look-ahead window, released smoothly (two stages:
    fast for short peaks, slower once the limiter works continuously - no pumping on dense mixes), then averaged
    over the window: the gain ramps down before a peak arrives (no hard gain step = no clipping distortion) and is
    at or below the required gain when it does. A final clamp at the ceiling catches what the estimate misses.
    The audio is delayed by the window (71 samples at 48 kHz). The delay stays when the limiter is off
    (passThrough): the plug-in reports one constant latency to the host and switching never jumps in time.
*/
class Limiter
{
public:
    void prepare (float sampleRate) noexcept
    {
        sr = sampleRate > 0.0f ? sampleRate : 48000.0f;
        look = std::max (8, (int) (0.0015f * sr));
        delayL.assign ((size_t) look, 0.0f);
        delayR.assign ((size_t) look, 0.0f);
        reqRing.assign ((size_t) look, 1.0f);
        avgRing.assign ((size_t) look, 1.0f);
        relFast = std::exp (-1.0f / (0.06f * sr));
        relSlow = std::exp (-1.0f / (0.35f * sr));
        slowFollow = std::exp (-1.0f / (0.4f * sr));
        reset();
    }

    void reset() noexcept
    {
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        std::fill (reqRing.begin(), reqRing.end(), 1.0f);
        std::fill (avgRing.begin(), avgRing.end(), 1.0f);
        pos = 0;
        held = 1.0f;
        sum = (double) look;
        gain = 1.0f;
        workDb = 0.0f;
        for (auto& h : histL) h = 0.0f;
        for (auto& h : histR) h = 0.0f;
    }

    /** Gain state back to unity; the delay line keeps its audio (on / off switching without a gap). */
    void resetGain() noexcept
    {
        std::fill (reqRing.begin(), reqRing.end(), 1.0f);
        std::fill (avgRing.begin(), avgRing.end(), 1.0f);
        held = 1.0f;
        sum = (double) look;
        gain = 1.0f;
        workDb = 0.0f;
    }

    /** Limiter off: only the look-ahead delay (constant latency). Keeps the true-peak history current. */
    void passThrough (float* l, float* r, int n) noexcept
    {
        if (delayL.empty()) return;
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r[i];
            histL[0] = histL[1]; histL[1] = histL[2]; histL[2] = xl;
            histR[0] = histR[1]; histR[1] = histR[2]; histR[2] = xr;
            delayL[(size_t) pos] = xl;
            delayR[(size_t) pos] = xr;
            if (++pos >= look) pos = 0;
            l[i] = delayL[(size_t) pos];
            r[i] = delayR[(size_t) pos];
        }
    }

    /** Ceiling as linear gain (default 0.966 = -0.3 dBFS). */
    void setCeiling (float linear) noexcept { ceiling = std::clamp (linear, 0.05f, 1.0f); }
    float getCeiling() const noexcept { return ceiling; }
    /** Look-ahead delay in samples (on and off: constant). */
    int latencySamples() const noexcept { return look - 1; }

    void process (float* l, float* r, int n) noexcept
    {
        if (delayL.empty()) return; // not prepared
        const float target = ceiling * 0.97f; // headroom for the inter-sample estimate (true peak stays below the ceiling)
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r[i];
            // true-peak estimate: the sample and three cubic (Catmull-Rom) points before it
            const float pk = std::max (truePeak (histL, xl), truePeak (histR, xr));
            const float req = pk > target ? target / pk : 1.0f;

            // running minimum over the window (window is short: a plain scan)
            reqRing[(size_t) pos] = req;
            float m = 1.0f;
            for (float v : reqRing) m = std::min (m, v);

            // release: fast for an isolated peak, slower while the limiter keeps working (program dependent)
            const float grDb = -dsp::gainToDb (std::max (held, 1.0e-6f));
            workDb = grDb + (workDb - grDb) * slowFollow;
            // 0.8: the release glides from fast to slow between 1 and 4 dB of working reduction - a hard switch at 2 dB
            // was a cliff: a little more drive locked the slow release in and the output got quieter, not louder
            const float w = std::clamp ((workDb - 1.0f) * (1.0f / 3.0f), 0.0f, 1.0f);
            const float rel = relFast + (relSlow - relFast) * w;
            held = m < held ? m : m + (held - m) * rel;

            // average over the window: the gain is down before the peak leaves the delay line
            sum += (double) held - (double) avgRing[(size_t) pos];
            avgRing[(size_t) pos] = held;
            gain = std::min (1.0f, (float) (sum / (double) look));

            // the delay is look - 1 samples: the averaging window of the oldest sample still contains its own minimum
            delayL[(size_t) pos] = xl;
            delayR[(size_t) pos] = xr;
            if (++pos >= look) pos = 0;
            const float dl = delayL[(size_t) pos], dr = delayR[(size_t) pos];
            l[i] = std::clamp (dl * gain, -ceiling, ceiling);
            r[i] = std::clamp (dr * gain, -ceiling, ceiling);
        }
    }

    float currentGain() const noexcept { return gain; }

private:
    static float truePeak (float* h, float x) noexcept
    {
        // h[0..2] = the three previous samples; interpolate between h[1] and h[2] (needs x as the next point)
        const float y0 = h[0], y1 = h[1], y2 = h[2], y3 = x;
        float pk = std::abs (x);
        for (float t : { 0.25f, 0.5f, 0.75f })
        {
            const float t2 = t * t, t3 = t2 * t;
            const float v = 0.5f * ((2.0f * y1) + (-y0 + y2) * t + (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t2
                                    + (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t3);
            pk = std::max (pk, std::abs (v));
        }
        h[0] = y1;
        h[1] = y2;
        h[2] = y3;
        return pk;
    }

    float sr = 48000.0f, ceiling = 0.966f; // -0.3 dBFS
    int look = 72, pos = 0;
    std::vector<float> delayL, delayR, reqRing, avgRing;
    float histL[3] {}, histR[3] {};
    double sum = 72.0;
    float held = 1.0f, gain = 1.0f, workDb = 0.0f, relFast = 0.999f, relSlow = 0.9999f, slowFollow = 0.9999f;
};
} // namespace ohpe
