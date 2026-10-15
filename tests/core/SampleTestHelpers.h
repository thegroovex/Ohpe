// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Synthetic test audio for the sample-based engines (no files, no copyrighted material).

#include "TestHelpers.h"
#include "sample/SampleData.h"

#include <cmath>
#include <memory>
#include <vector>

namespace ohpe::test
{
inline std::shared_ptr<SampleData> makeSine (double freq, double rate, double seconds, float amp = 0.5f, std::string path = {})
{
    std::vector<float> x ((size_t) (rate * seconds));
    for (size_t i = 0; i < x.size(); ++i) x[i] = amp * (float) std::sin (2.0 * 3.14159265358979323846 * freq * (double) i / rate);
    return SampleData::fromMono (std::move (x), rate, std::move (path));
}

/** Silence with decaying-sine hits at the given frames (plus a faint noise floor). */
inline std::shared_ptr<SampleData> makeHits (double rate, double seconds, const std::vector<int64_t>& hitFrames,
                                             const std::vector<float>& amps)
{
    std::vector<float> x ((size_t) (rate * seconds), 0.0f);
    uint32_t s = 12345u;
    for (auto& v : x)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        v = ((float) (s >> 8) / 16777216.0f - 0.5f) * 0.0005f; // about -66 dB
    }
    for (size_t h = 0; h < hitFrames.size(); ++h)
        for (int64_t i = 0; i < (int64_t) (0.15 * rate) && hitFrames[h] + i < (int64_t) x.size(); ++i)
            x[(size_t) (hitFrames[h] + i)] += amps[h] * (float) (std::sin (2.0 * 3.14159265358979323846 * 800.0 * (double) i / rate)
                                                                * std::exp (-(double) i / (0.02 * rate)));
    return SampleData::fromMono (std::move (x), rate);
}

/** Pitch by normalised autocorrelation (robust for formant-rich signals). */
inline double measurePitchAutocorr (const std::vector<float>& v, double sr, size_t from, size_t to, double fMin = 60.0, double fMax = 1200.0)
{
    const int minLag = (int) (sr / fMax), maxLag = (int) (sr / fMin);
    const size_t n = to - from - (size_t) maxLag;
    std::vector<double> r ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
    {
        double num = 0.0, e0 = 0.0, e1 = 0.0;
        for (size_t i = from; i < from + n; ++i)
        {
            num += (double) v[i] * v[i + (size_t) lag];
            e0 += (double) v[i] * v[i];
            e1 += (double) v[i + (size_t) lag] * v[i + (size_t) lag];
        }
        r[(size_t) lag] = num / std::sqrt (e0 * e1 + 1.0e-20);
    }
    double best = -1.0;
    for (int lag = minLag; lag <= maxLag; ++lag) best = std::max (best, r[(size_t) lag]);
    for (int lag = minLag; lag <= maxLag; ++lag) // first peak close to the best one (avoids octave errors)
        if (r[(size_t) lag] >= 0.9 * best && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1])
        {
            const double a = r[(size_t) lag - 1], b = r[(size_t) lag], c = r[(size_t) lag + 1];
            const double denom = a - 2.0 * b + c;
            const double shift = std::abs (denom) > 1.0e-12 ? 0.5 * (a - c) / denom : 0.0;
            return sr / ((double) lag + shift);
        }
    return 0.0;
}

inline double autocorrPeak (const std::vector<float>& v, size_t from, size_t to, int lag)
{
    double num = 0.0, e0 = 0.0, e1 = 0.0;
    for (size_t i = from; i + (size_t) lag < to; ++i)
    {
        num += (double) v[i] * v[i + (size_t) lag];
        e0 += (double) v[i] * v[i];
        e1 += (double) v[i + (size_t) lag] * v[i + (size_t) lag];
    }
    return num / std::sqrt (e0 * e1 + 1.0e-20);
}

/** Energy of one frequency (Goertzel). */
inline double toneEnergy (const std::vector<float>& v, double sr, size_t from, size_t to, double freq)
{
    const double w = 2.0 * 3.14159265358979323846 * freq / sr, c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        const double s0 = v[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
} // namespace ohpe::test
