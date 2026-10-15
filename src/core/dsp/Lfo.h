// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "DspUtil.h"

namespace ohpe::dsp
{
/** Low-frequency oscillator, bipolar output -1..+1. Shapes: 0 sine, 1 triangle, 2 saw, 3 square, 4 sample & hold. */
class Lfo
{
public:
    enum Shape { Sine = 0, Triangle, Saw, Square, SampleHold, numShapes };

    void prepare (float sampleRate) noexcept { sr = sampleRate > 0.0f ? sampleRate : 48000.0f; }
    void setRate (float hz) noexcept { inc = std::max (0.0f, hz) / sr; }
    void setShape (int s) noexcept { shape = (s >= 0 && s < numShapes) ? s : 0; }
    void reset (float phase0 = 0.0f) noexcept { phase = phase0; }

    /** Advances by n samples and returns the value at the new position (block-rate use). */
    float advance (int n) noexcept
    {
        phase += inc * (float) n;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rng.nextBipolar();
        }
        return value();
    }

    float value() const noexcept
    {
        switch (shape)
        {
            case Triangle: return 1.0f - 4.0f * std::abs (phase - 0.5f);
            case Saw: return 2.0f * phase - 1.0f;
            case Square: return phase < 0.5f ? 1.0f : -1.0f;
            case SampleHold: return held;
            default: return std::sin (twoPi * phase);
        }
    }

private:
    float sr = 48000.0f, inc = 0.0f, phase = 0.0f, held = 0.0f;
    int shape = Sine;
    Random rng { 0xbeef };
};
} // namespace ohpe::dsp
