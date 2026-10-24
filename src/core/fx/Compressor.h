// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"

#include <algorithm>
#include <cmath>

namespace ohpe
{
/** Soft-knee downward static curve: gain change in dB (<= 0) for an input level in dB. */
inline float compStaticGainDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float over = levelDb - thresholdDb;
    const float slope = 1.0f / std::max (1.0f, ratio) - 1.0f;
    if (kneeDb > 0.0f && std::abs (over) <= 0.5f * kneeDb)
    {
        const float x = over + 0.5f * kneeDb;
        return slope * x * x / (2.0f * kneeDb);
    }
    return over > 0.0f ? slope * over : 0.0f;
}

/**
    p88: the shared detector / gain computer of the compressors (rack Compressor, channel compressor-lite, master).

    Zero latency (no lookahead: the part chains, the mixer and the master keep their timing). Per sample, stereo-linked:
      - held peak detector (12 ms hold, 20 ms fall, 0.8): no gain ripple inside a bass cycle - the sustained sound;
      - main gain computer: the static curve through the user's attack and a program-dependent release (single hits
        release up to 3.3x faster, sustained compression with the set release - less pumping after a lone transient);
      - onset guard (p88): a fast peak path with instant attack (on the held peak) and the same release follows the static curve. The applied
        reduction is never less than the guard's minus a small transient allowance (0.3 dB per ms of attack, at most
        3 dB); both paths release alike. Released after silence, the first hit of a note or chord is caught instead of passing with the full
        make-up gain on top (p87: +24 dB make-up let chords through at up to +14 dBTP). Once the attack has settled
        the guard lies above the main curve, so the sustained sound is unchanged.
    Real-time safe, no allocation.
*/
class CompDetector
{
public:
    void prepare (float sampleRate) noexcept
    {
        sr = sampleRate > 0.0f ? sampleRate : 48000.0f;
        holdSamples = (int) (0.012f * sr);
        envFall = std::exp (-1.0f / (0.02f * sr));
        guardAtk = 0.0f; // instant: a lagging guard lets a fast onset through by its slope in dB x the lag
        sustainC = std::exp (-1.0f / (0.15f * sr));
        setTimes (attackMs, releaseMs);
        reset();
    }

    void reset() noexcept { grDb = 0.0f; guardDb = 0.0f; peakEnv = 0.0f; holdLeft = 0; sustain = 0.0f; }

    void setTimes (float attackMsIn, float releaseMsIn) noexcept
    {
        attackMs = std::max (0.01f, attackMsIn);
        releaseMs = std::max (1.0f, releaseMsIn);
        atkC = std::exp (-1.0f / (sr * attackMs * 0.001f));
        relSlow = std::exp (-1.0f / (sr * releaseMs * 0.001f));
        relFast = std::exp (-1.0f / (sr * releaseMs * 0.0003f));
        allowanceDb = std::min (3.0f, 0.3f * attackMs);
    }

    /** One stereo-linked sample: returns the applied gain change in dB (<= 0 for a downward curve). The curve may
        also lift (> 0, the multiband compressor's upward half): a falling target (more reduction or less lift) is the
        attack, a rising one the release - so the lift also lets go at once (the guard) at a note onset. */
    template <typename Curve>
    float next (float absPeak, Curve&& curve) noexcept
    {
        if (absPeak >= peakEnv) { peakEnv = absPeak; holdLeft = holdSamples; }
        else if (holdLeft > 0) --holdLeft;
        else peakEnv = std::max (absPeak, peakEnv * envFall);
        const float target = curve (dsp::gainToDb (peakEnv)); // gainToDb: -100 dB for silence
        // program-dependent release: sustain -> 1 while compression goes on (set release), -> 0 for lone hits (faster)
        sustain = (grDb < -1.0f ? 1.0f : 0.0f) + (sustain - (grDb < -1.0f ? 1.0f : 0.0f)) * sustainC;
        const float rel = relFast + (relSlow - relFast) * sustain;
        grDb = target + (grDb - target) * (target < grDb ? atkC : rel);
        guardDb = target + (guardDb - target) * (target < guardDb ? guardAtk : rel);
        if (std::abs (grDb) < 1.0e-6f) grDb = 0.0f;
        if (std::abs (guardDb) < 1.0e-6f) guardDb = 0.0f;
        return std::min (grDb, guardDb + allowanceDb);
    }

    float attack() const noexcept { return attackMs; }

private:
    float sr = 48000.0f, attackMs = 10.0f, releaseMs = 150.0f, allowanceDb = 3.0f;
    float atkC = 0.0f, relSlow = 0.0f, relFast = 0.0f, guardAtk = 0.0f, sustainC = 0.0f;
    float grDb = 0.0f, guardDb = 0.0f, peakEnv = 0.0f, envFall = 0.999f, sustain = 0.0f;
    int holdSamples = 576, holdLeft = 0;
};

/**
    Stereo-linked feed-forward compressor (CompDetector: held peak, onset guard, soft knee, gain smoothing in dB).
    Used for the per-channel "compressor-lite" and the master compressor. Real-time safe.
*/
class Compressor
{
public:
    void prepare (float sampleRate) noexcept
    {
        det.prepare (sampleRate);
        set (threshold, ratio, attackMs, releaseMs, makeup, kneeDb);
        reset();
    }

    void reset() noexcept { det.reset(); lastGr = 0.0f; }

    void set (float thresholdDb, float ratioIn, float attackMsIn, float releaseMsIn, float makeupDb, float kneeDbIn = 6.0f) noexcept
    {
        threshold = thresholdDb;
        ratio = std::max (1.0f, ratioIn);
        attackMs = std::max (0.01f, attackMsIn);
        releaseMs = std::max (1.0f, releaseMsIn);
        makeup = makeupDb;
        kneeDb = std::max (0.0f, kneeDbIn);
        det.setTimes (attackMs, releaseMs);
        makeupGain = dsp::dbToGain (makeup);
    }

    /** Static curve: gain change in dB (<= 0) for an input level in dB. */
    float staticGainDb (float levelDb) const noexcept { return compStaticGainDb (levelDb, threshold, ratio, kneeDb); }

    void process (float* l, float* r, int n) noexcept
    {
        const float th = threshold, ra = ratio, kn = kneeDb;
        auto curve = [th, ra, kn] (float db) { return compStaticGainDb (db, th, ra, kn); };
        float gr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            gr = det.next (std::max (std::abs (l[i]), std::abs (r[i])), curve);
            const float g = (gr >= 0.0f ? 1.0f : std::pow (10.0f, gr * 0.05f)) * makeupGain;
            l[i] *= g;
            r[i] *= g;
        }
        if (n > 0) lastGr = gr;
    }

    /** Current gain reduction in dB (positive number, 0 = none). */
    float gainReductionDb() const noexcept { return -lastGr; }

private:
    CompDetector det;
    float threshold = -12.0f, ratio = 3.0f, attackMs = 10.0f, releaseMs = 150.0f, makeup = 0.0f, kneeDb = 6.0f;
    float makeupGain = 1.0f, lastGr = 0.0f;
};
} // namespace ohpe
