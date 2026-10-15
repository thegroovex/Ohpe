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
} // namespace sample
} // namespace ohpe
