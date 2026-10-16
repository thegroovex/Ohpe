// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"

#include <array>
#include <cmath>
#include <vector>

namespace ohpe
{
/**
    Master limiter (0.8 punch pass): 1.5 ms look-ahead, true-peak aware, ceiling -0.3 dBFS by default.

    The required gain of every sample (ceiling / peak, the peak including the inter-sample peaks of a 4x
    windowed-sinc interpolation over 24 samples - a cubic estimate missed up to 3 dB on near-Nyquist content such as
    a saturated hi-hat) is held as a running minimum over the look-ahead window, released smoothly (two stages:
    fast for short peaks, slower once the limiter works continuously - no pumping on dense mixes), then averaged
    over the window: the gain ramps down before a peak arrives (no hard gain step = no clipping distortion) and is
    at or below the required gain when it does. A final clamp at the ceiling catches what the estimate misses.
    The audio is delayed by the window plus the interpolator's 12 samples (83 samples at 48 kHz). The delay stays when the limiter is off
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
        // 4x true-peak interpolator: windowed sinc over taps x[m-11] .. x[m+12] for the points m + 1/4, 1/2, 3/4
        for (int ph = 0; ph < 3; ++ph)
        {
            const double frac = (double) (ph + 1) * 0.25;
            double total = 0.0;
            for (int j = 0; j < taps; ++j)
            {
                const double t = frac - (double) (j - (half - 1)); // distance from tap j (x[m - 11 + j]) to the point
                const double pi = 3.14159265358979323846;
                const double sinc = std::abs (t) < 1.0e-9 ? 1.0 : std::sin (pi * t) / (pi * t);
                const double u = t / ((double) half + 0.5); // Hann window over the span
                const double w = std::abs (u) >= 1.0 ? 0.0 : 0.5 + 0.5 * std::cos (pi * u);
                coef[(size_t) ph][(size_t) j] = sinc * w;
                total += sinc * w;
            }
            for (auto& c : coef[(size_t) ph]) c /= total;
        }
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
        histL.fill (0.0f);
        histR.fill (0.0f);
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
            push (histL, l[i]);
            push (histR, r[i]);
            const float xl = histL[(size_t) (half - 1)], xr = histR[(size_t) (half - 1)];
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
    int latencySamples() const noexcept { return look - 1 + half; }

    void process (float* l, float* r, int n) noexcept
    {
        if (delayL.empty()) return; // not prepared
        const float target = ceiling * 0.97f; // headroom for the inter-sample estimate (true peak stays below the ceiling)
        for (int i = 0; i < n; ++i)
        {
            push (histL, l[i]);
            push (histR, r[i]);
            // the sample 12 back and its true peak towards the next one (the interpolator needs 12 samples ahead)
            const float xl = histL[(size_t) (half - 1)], xr = histR[(size_t) (half - 1)];
            const float pk = std::max (truePeak (histL), truePeak (histR));
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
    static constexpr int half = 12, taps = 24;
    using History = std::array<float, (size_t) taps>;

    static void push (History& h, float x) noexcept
    {
        for (size_t j = 0; j + 1 < h.size(); ++j) h[j] = h[j + 1];
        h[h.size() - 1] = x;
    }

    float truePeak (const History& h) const noexcept
    {
        float pk = std::abs (h[(size_t) (half - 1)]);
        for (const auto& c : coef)
        {
            double v = 0.0;
            for (size_t j = 0; j < h.size(); ++j) v += (double) c[j] * (double) h[j];
            pk = std::max (pk, (float) std::abs (v));
        }
        return pk;
    }

    float sr = 48000.0f, ceiling = 0.966f; // -0.3 dBFS
    int look = 72, pos = 0;
    std::vector<float> delayL, delayR, reqRing, avgRing;
    History histL {}, histR {};
    std::array<std::array<double, (size_t) taps>, 3> coef {};
    double sum = 72.0;
    float held = 1.0f, gain = 1.0f, workDb = 0.0f, relFast = 0.999f, relSlow = 0.9999f, slowFollow = 0.9999f;
};
} // namespace ohpe
