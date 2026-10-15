// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "DspUtil.h"

namespace ohpe::dsp
{
/** PolyBLEP residual for band-limited discontinuities. t = phase 0..1, dt = phase increment. */
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/**
    Band-limited oscillator with a continuous shape:
    0.0 = saw, 0.5 = square, 1.0 = narrow pulse (pulse width 50 % -> 8 %).
*/
class MorphOsc
{
public:
    void reset (float phase0 = 0.0f) noexcept { phase = phase0; }

    float next (float freqHz, float sampleRate, float shape) noexcept
    {
        const float dt = std::clamp (freqHz / sampleRate, 0.0f, 0.49f);
        shape = clamp01 (shape);

        float saw = 2.0f * phase - 1.0f;
        saw -= polyBlep (phase, dt);

        const float pw = shape <= 0.5f ? 0.5f : 0.5f - (shape - 0.5f) * 0.84f;
        float sq = phase < pw ? 1.0f : -1.0f;
        sq += polyBlep (phase, dt);
        float t2 = phase - pw;
        if (t2 < 0.0f) t2 += 1.0f;
        sq -= polyBlep (t2, dt);

        const float mix = std::min (1.0f, shape * 2.0f);
        const float out = saw + (sq - saw) * mix;

        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        return out;
    }

    float getPhase() const noexcept { return phase; }

private:
    float phase = 0.0f;
};

/** Plain sine oscillator via phase accumulator (sub oscillator, FM operators). */
class SineOsc
{
public:
    void reset (float phase0 = 0.0f) noexcept { phase = phase0; }
    float next (float freqHz, float sampleRate, float phaseMod = 0.0f) noexcept
    {
        const float out = std::sin (twoPi * phase + phaseMod);
        phase += freqHz / sampleRate;
        phase -= std::floor (phase);
        return out;
    }

private:
    float phase = 0.0f;
};
} // namespace ohpe::dsp
