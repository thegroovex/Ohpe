// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleData.h"

#include "../dsp/DspUtil.h"

#include <cmath>

namespace ohpe
{
SampleData::SampleData (std::vector<std::vector<float>> channelData, double sampleRateHz, std::string sourcePath, std::string name)
    : data (std::move (channelData)), rate (sampleRateHz > 0.0 ? sampleRateHz : 48000.0), filePath (std::move (sourcePath)),
      displayName (std::move (name))
{
    if (data.empty()) data.emplace_back();
    if (data.size() > 2) data.resize (2);
    frames = (int64_t) data[0].size();
    for (auto& c : data) c.resize ((size_t) frames, 0.0f); // equal lengths
    for (auto& c : data)
        for (auto& x : c)
        {
            if (! std::isfinite (x)) x = 0.0f;
            peakValue = std::max (peakValue, std::abs (x));
        }
    if (displayName.empty() && ! filePath.empty())
    {
        const auto slash = filePath.find_last_of ("/\\");
        displayName = slash == std::string::npos ? filePath : filePath.substr (slash + 1);
    }
}

std::shared_ptr<SampleData> SampleData::fromMono (std::vector<float> mono, double sampleRateHz, std::string sourcePath)
{
    std::vector<std::vector<float>> ch;
    ch.push_back (std::move (mono));
    return std::make_shared<SampleData> (std::move (ch), sampleRateHz, std::move (sourcePath));
}

namespace sample
{
SamplePtr makeHarmonicTone (double rate, float rootNote, float seconds, float decaySeconds, int numHarmonics)
{
    const auto n = (size_t) std::max (1.0, rate * seconds);
    std::vector<float> x (n, 0.0f);
    const double f0 = dsp::midiToHz (rootNote);
    const double fadeIn = 0.002 * rate;
    for (int h = 1; h <= numHarmonics; ++h)
    {
        const double f = f0 * h;
        if (f > rate * 0.45) break;
        const double amp = 1.0 / h;
        const double hDecay = decaySeconds / (1.0 + 0.35 * (h - 1)); // upper partials fade first
        const double w = 2.0 * 3.14159265358979323846 * f / rate;
        for (size_t i = 0; i < n; ++i)
            x[i] += (float) (amp * std::sin (w * (double) i) * std::exp (-(double) i / (hDecay * rate)));
    }
    float pk = 0.0f;
    for (float v : x) pk = std::max (pk, std::abs (v));
    const float norm = pk > 0.0f ? 0.8f / pk : 0.0f;
    for (size_t i = 0; i < n; ++i)
    {
        float g = norm;
        if ((double) i < fadeIn) g *= (float) ((double) i / fadeIn);
        const double tail = (double) (n - 1 - i);
        if (tail < 0.01 * rate) g *= (float) (tail / (0.01 * rate)); // ends at exactly zero
        x[i] *= g;
    }
    auto s = SampleData::fromMono (std::move (x), rate);
    return s;
}
SamplePtr makeKeysTone (double rate, float rootNote, float seconds)
{
    const auto n = (size_t) std::max (1.0, rate * seconds);
    std::vector<float> x (n, 0.0f);
    const double f0 = dsp::midiToHz (rootNote);
    const double twoPi = 2.0 * 3.14159265358979323846;
    struct Partial { double ratio, amp, decay; };
    // warm body, a tine "bell" partial that dies fast, a little inharmonic shimmer
    const Partial partials[] = { { 1.0, 1.0, 2.6 }, { 2.0, 0.42, 1.5 }, { 3.0, 0.16, 0.9 }, { 4.0, 0.08, 0.55 },
                                 { 5.0, 0.04, 0.4 }, { 7.02, 0.30, 0.10 }, { 14.1, 0.10, 0.04 }, { 1.003, 0.25, 2.2 } };
    for (const auto& p : partials)
    {
        if (f0 * p.ratio > rate * 0.45) continue;
        const double w = twoPi * f0 * p.ratio / rate;
        for (size_t i = 0; i < n; ++i)
            x[i] += (float) (p.amp * std::sin (w * (double) i) * std::exp (-(double) i / (p.decay * rate)));
    }
    // hammer: a few milliseconds of filtered noise
    uint32_t s = 0x9e3779b9u;
    float lp = 0.0f;
    const auto hammer = (size_t) (0.004 * rate);
    for (size_t i = 0; i < std::min (n, hammer * 3); ++i)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        const float white = (float) (s >> 8) / 8388608.0f - 1.0f;
        lp += (white - lp) * 0.35f;
        x[i] += 0.25f * lp * (float) std::exp (-(double) i / (double) hammer);
    }
    float pk = 0.0f;
    for (float v : x) pk = std::max (pk, std::abs (v));
    const float norm = pk > 0.0f ? 0.8f / pk : 0.0f;
    const double fadeIn = 0.0015 * rate, fadeOut = 0.05 * rate;
    for (size_t i = 0; i < n; ++i)
    {
        float g = norm;
        if ((double) i < fadeIn) g *= (float) ((double) i / fadeIn);
        const double tail = (double) (n - 1 - i);
        if (tail < fadeOut) g *= (float) (tail / fadeOut);
        x[i] *= g;
    }
    return SampleData::fromMono (std::move (x), rate);
}

SamplePtr makePadTexture (double rate, float rootNote, float seconds)
{
    const auto n = (size_t) std::max (1.0, rate * seconds);
    std::vector<std::vector<float>> ch (2, std::vector<float> (n, 0.0f));
    const double f0 = dsp::midiToHz (rootNote);
    const double twoPi = 2.0 * 3.14159265358979323846;
    const double detune[3] = { -0.07, 0.0, 0.06 }; // semitones: a gentle chorus
    for (int v = 0; v < 3; ++v)
    {
        const double fv = f0 * std::pow (2.0, detune[v] / 12.0);
        for (int h = 1; h <= 16; ++h)
        {
            if (fv * h > rate * 0.45) break;
            const double w = twoPi * fv * h / rate;
            const double phase = 0.37 * h * (v + 1);
            // upper partials swell and fade slowly at their own rates: the texture keeps moving
            const double moveRate = 0.11 + 0.037 * h + 0.05 * v;
            const double baseAmp = 1.0 / std::pow ((double) h, 1.15);
            const double pan = std::sin (1.7 * h + v); // spread partials across the stereo field
            const double gl = std::sqrt (0.5 * (1.0 - 0.6 * pan)), gr = std::sqrt (0.5 * (1.0 + 0.6 * pan));
            for (size_t i = 0; i < n; ++i)
            {
                const double t = (double) i / rate;
                const double a = baseAmp * (h == 1 ? 1.0 : 0.55 + 0.45 * std::sin (twoPi * moveRate * t + phase));
                const double y = a * std::sin (w * (double) i + phase);
                ch[0][i] += (float) (y * gl);
                ch[1][i] += (float) (y * gr);
            }
        }
    }
    float pk = 0.0f;
    for (const auto& c : ch)
        for (float v : c) pk = std::max (pk, std::abs (v));
    const float norm = pk > 0.0f ? 0.7f / pk : 0.0f;
    for (auto& c : ch)
        for (auto& v : c) v *= norm;
    return std::make_shared<SampleData> (std::move (ch), rate);
}
} // namespace sample
} // namespace ohpe
