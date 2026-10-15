// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "engines/ISynthEngine.h"

#include <cmath>
#include <vector>

namespace ohpe::test
{
constexpr double sampleRate = 48000.0;

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
    size_t size() const { return l.size(); }
};

inline bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x)) return false;
    return true;
}

inline bool noDenormals (const std::vector<float>& v)
{
    for (float x : v)
        if (x != 0.0f && std::fpclassify (x) == FP_SUBNORMAL) return false;
    return true;
}

inline float peak (const std::vector<float>& v, size_t from = 0, size_t to = (size_t) -1)
{
    float p = 0.0f;
    for (size_t i = from; i < std::min (to, v.size()); ++i) p = std::max (p, std::abs (v[i]));
    return p;
}

inline float rms (const std::vector<float>& v, size_t from = 0, size_t to = (size_t) -1)
{
    double s = 0.0;
    size_t n = 0;
    for (size_t i = from; i < std::min (to, v.size()); ++i, ++n) s += (double) v[i] * v[i];
    return n ? (float) std::sqrt (s / (double) n) : 0.0f;
}

/** Fundamental frequency from upward zero crossings (linear interpolation) after DC removal. */
inline double measureFrequency (const std::vector<float>& v, double sr, size_t from, size_t to)
{
    double mean = 0.0;
    for (size_t i = from; i < to; ++i) mean += v[i];
    mean /= (double) (to - from);
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = from + 1; i < to; ++i)
    {
        const double a = v[i - 1] - mean, b = v[i] - mean;
        if (a < 0.0 && b >= 0.0)
        {
            const double t = (double) (i - 1) + a / (a - b);
            if (first < 0.0) first = t;
            last = t;
            ++count;
        }
    }
    if (count < 2) return 0.0;
    return (count - 1) * sr / (last - first);
}

inline double centsBetween (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

/** Renders an engine in blocks of `block` samples into a stereo buffer. */
inline Stereo renderEngine (ISynthEngine& e, size_t numSamples, int block = 256)
{
    Stereo s (numSamples);
    for (size_t pos = 0; pos < numSamples; pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, numSamples - pos);
        e.render (s.l.data() + pos, s.r.data() + pos, n);
    }
    return s;
}

/** Engine-friendly common settings: instant envelopes, no modulation. */
inline VoiceCommon plainCommon()
{
    VoiceCommon c;
    c.amp = { 0.001f, 0.1f, 1.0f, 0.05f };
    c.mod = { 0.001f, 0.1f, 1.0f, 0.05f };
    c.modEnvAmount = 0.0f;
    c.lfoDepth = 0.0f;
    c.velocitySens = 0.0f;
    return c;
}
} // namespace ohpe::test
