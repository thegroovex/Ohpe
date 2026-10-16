// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"

namespace ohpe
{
/**
    Stereo-linked feed-forward compressor (peak detector, soft knee, gain smoothing in dB).
    Used for the per-channel "compressor-lite" and the master compressor. Real-time safe.
*/
class Compressor
{
public:
    void prepare (float sampleRate) noexcept
    {
        sr = sampleRate > 0.0f ? sampleRate : 48000.0f;
        holdSamples = (int) (0.012f * sr);
        envFall = std::exp (-1.0f / (0.02f * sr));
        set (threshold, ratio, attackMs, releaseMs, makeup);
        reset();
    }

    void reset() noexcept { grDb = 0.0f; peakEnv = 0.0f; holdLeft = 0; }

    void set (float thresholdDb, float ratioIn, float attackMsIn, float releaseMsIn, float makeupDb, float kneeDbIn = 6.0f) noexcept
    {
        threshold = thresholdDb;
        ratio = std::max (1.0f, ratioIn);
        attackMs = std::max (0.01f, attackMsIn);
        releaseMs = std::max (1.0f, releaseMsIn);
        makeup = makeupDb;
        kneeDb = std::max (0.0f, kneeDbIn);
        attackCoeff = std::exp (-1.0f / (sr * attackMs * 0.001f));
        releaseCoeff = std::exp (-1.0f / (sr * releaseMs * 0.001f));
        makeupGain = dsp::dbToGain (makeup);
    }

    /** Static curve: gain change in dB (<= 0) for an input level in dB. */
    float staticGainDb (float levelDb) const noexcept
    {
        const float over = levelDb - threshold;
        const float slope = 1.0f / ratio - 1.0f;
        if (kneeDb > 0.0f && std::abs (over) <= 0.5f * kneeDb)
        {
            const float x = over + 0.5f * kneeDb;
            return slope * x * x / (2.0f * kneeDb);
        }
        return over > 0.0f ? slope * over : 0.0f;
    }

    void process (float* l, float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            // 0.8: held peak detector (12 ms hold, 20 ms fall) - no gain ripple inside a bass cycle
            const float x = std::max (std::abs (l[i]), std::abs (r[i]));
            if (x >= peakEnv) { peakEnv = x; holdLeft = holdSamples; }
            else if (holdLeft > 0) --holdLeft;
            else peakEnv = std::max (x, peakEnv * envFall);
            const float peak = peakEnv;
            const float target = staticGainDb (dsp::gainToDb (peak));
            const float c = target < grDb ? attackCoeff : releaseCoeff;
            grDb = target + (grDb - target) * c;
            if (grDb > -1.0e-6f) grDb = 0.0f;
            const float g = (grDb >= 0.0f ? 1.0f : std::pow (10.0f, grDb * 0.05f)) * makeupGain;
            l[i] *= g;
            r[i] *= g;
        }
    }

    /** Current gain reduction in dB (positive number, 0 = none). */
    float gainReductionDb() const noexcept { return -grDb; }

private:
    float sr = 48000.0f, threshold = -12.0f, ratio = 3.0f, attackMs = 10.0f, releaseMs = 150.0f, makeup = 0.0f, kneeDb = 6.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f, makeupGain = 1.0f, grDb = 0.0f;
    float peakEnv = 0.0f, envFall = 0.999f;
    int holdSamples = 576, holdLeft = 0;
};
} // namespace ohpe
