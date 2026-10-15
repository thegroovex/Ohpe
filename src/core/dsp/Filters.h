// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "DspUtil.h"

namespace ohpe::dsp
{
/** Zero-delay-feedback state-variable filter (trapezoidal integration). */
class Svf
{
public:
    enum Mode { LowPass = 0, BandPass, HighPass, Notch };

    void prepare (float sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    /** cutoff in Hz, resonance 0..1 */
    void set (float cutoff, float resonance) noexcept
    {
        const float fc = std::clamp (cutoff, 10.0f, sr * 0.45f);
        g = std::tan (pi * fc / sr);
        k = 2.0f - 1.96f * clamp01 (resonance);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float x, int mode) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flush (2.0f * v1 - ic1);
        ic2 = flush (2.0f * v2 - ic2);
        switch (mode)
        {
            case BandPass: return v1;
            case HighPass: return x - k * v1 - v2;
            case Notch: return x - k * v1;
            default: return v2;
        }
    }

private:
    float sr = 48000.0f, g = 0.0f, k = 2.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
};

/** Four-pole zero-delay-feedback ladder low-pass with soft-saturated feedback path. */
class Ladder
{
public:
    void prepare (float sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0.0f; }

    /** cutoff in Hz, resonance 0..1 (self-oscillation near 1) */
    void set (float cutoff, float resonance) noexcept
    {
        const float fc = std::clamp (cutoff, 10.0f, sr * 0.45f);
        const float g = std::tan (pi * fc / sr);
        G = g / (1.0f + g);
        inv1g = 1.0f / (1.0f + g);
        k = 3.95f * clamp01 (resonance);
        G2 = G * G; G3 = G2 * G; G4 = G3 * G;
    }

    float process (float x, float drive = 1.0f) noexcept
    {
        const float sigma = G3 * s[0] * inv1g + G2 * s[1] * inv1g + G * s[2] * inv1g + s[3] * inv1g;
        const float y4 = (G4 * x + sigma) / (1.0f + k * G4);
        float u = softClip (drive * (x - k * y4)) * (1.0f + 0.5f * k / 3.95f);
        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - s[i]) * G;
            const float y = v + s[i];
            s[i] = flush (y + v);
            u = y;
        }
        return u;
    }

private:
    float sr = 48000.0f, G = 0.0f, G2 = 0.0f, G3 = 0.0f, G4 = 0.0f, inv1g = 1.0f, k = 0.0f;
    float s[4] {};
};

/** Simple one-pole low/high-pass (tone controls, DC blocking). */
class OnePole
{
public:
    void setCutoff (float hz, float sampleRate) noexcept
    {
        a = 1.0f - std::exp (-twoPi * std::clamp (hz, 1.0f, sampleRate * 0.49f) / sampleRate);
    }
    float lowpass (float x) noexcept { z = flush (z + a * (x - z)); return z; }
    float highpass (float x) noexcept { return x - lowpass (x); }
    void reset() noexcept { z = 0.0f; }

private:
    float a = 1.0f, z = 0.0f;
};
} // namespace ohpe::dsp
