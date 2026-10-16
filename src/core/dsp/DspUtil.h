// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ohpe::dsp
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

inline float midiToHz (float note) noexcept { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
inline float dbToGain (float db) noexcept { return db <= -96.0f ? 0.0f : std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g) noexcept { return g <= 1.0e-5f ? -100.0f : 20.0f * std::log10 (g); }
inline float semitonesToRatio (float st) noexcept { return std::pow (2.0f, st / 12.0f); }

/** Flushes denormal-range values to zero (filters call it on their state). */
inline float flush (float x) noexcept { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

inline float clamp01 (float x) noexcept { return std::min (1.0f, std::max (0.0f, x)); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

/** Fast tanh-style soft clipper (Pade approximation, exact limit at +-3). */
inline float softClip (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Equal-power pan, pan -1..+1 -> left/right gains. */
inline void panGains (float pan, float& l, float& r) noexcept
{
    const float a = (std::clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * pi;
    l = std::cos (a);
    r = std::sin (a);
}

/** One-pole parameter smoother (per-sample). */
class Smoother
{
public:
    void prepare (float sampleRate, float timeMs = 20.0f) noexcept
    {
        coeff = 1.0f - std::exp (-1.0f / (std::max (1.0f, sampleRate) * timeMs * 0.001f));
    }
    void reset (float v) noexcept { current = target = v; }
    void setTarget (float v) noexcept { target = v; }
    float next() noexcept
    {
        current += (target - current) * coeff;
        return current;
    }
    float value() const noexcept { return current; }
    /** Jumps to the target once within `tolerance` of it (0.7). */
    void settle (float tolerance) noexcept
    {
        if (std::abs (target - current) <= tolerance) current = target;
    }

private:
    float current = 0.0f, target = 0.0f, coeff = 1.0f;
};

/** Small deterministic PRNG (xorshift32) - real-time safe, reproducible in tests. */
class Random
{
public:
    explicit Random (uint32_t seed = 0x1234567u) noexcept : state (seed ? seed : 1u) {}
    void seed (uint32_t s) noexcept { state = s ? s : 1u; }
    uint32_t nextU32() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float next01() noexcept { return (float) (nextU32() >> 8) * (1.0f / 16777216.0f); }
    float nextBipolar() noexcept { return next01() * 2.0f - 1.0f; }

private:
    uint32_t state;
};
} // namespace ohpe::dsp
