// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Slicer.h"

#include <cmath>

namespace ohpe::sample
{
std::vector<int64_t> detectTransients (const IAudioRegion& audio, const SliceSettings& settings)
{
    std::vector<int64_t> slices { 0 };
    const int64_t n = audio.numFrames();
    if (n <= 0) return slices;

    const double sr = audio.sampleRate();
    const int chans = audio.numChannels();
    std::vector<float> mono ((size_t) n);
    float peak = 0.0f;
    for (int64_t i = 0; i < n; ++i)
    {
        float v = 0.0f;
        for (int c = 0; c < chans; ++c) v = std::max (v, std::abs (audio.channel (c)[i]));
        mono[(size_t) i] = v;
        peak = std::max (peak, v);
    }
    if (peak <= 0.0f) return slices;

    const float sens = std::clamp (settings.sensitivity, 0.0f, 1.0f);
    const float ratio = 2.0f + (1.0f - sens) * 14.0f;                         // 16 .. 2
    const float floorLevel = peak * std::pow (10.0f, (-14.0f - 40.0f * sens) / 20.0f); // -14 .. -54 dB
    const float quiet = floorLevel * 0.25f;
    const float slowCoeff = 1.0f - std::exp (-1.0f / (float) (0.030 * sr));        // 30 ms average
    const float peakDecay = std::exp (-1.0f / (float) (0.005 * sr));               // 5 ms peak hold-off
    const auto minGap = (int64_t) (settings.minGapMs * 0.001 * sr);
    const auto lookBack = (int64_t) (0.010 * sr);

    float slow = 0.0f, fast = 0.0f;
    int64_t last = 0;
    for (int64_t i = 0; i < n; ++i)
    {
        const float x = mono[(size_t) i];
        fast = std::max (x, fast * peakDecay);
        const bool onset = fast > floorLevel && fast > ratio * slow + 1.0e-9f;
        slow += (x - slow) * slowCoeff;
        if (! onset || i - last < minGap) continue;

        // move back to the last quiet sample (at most 10 ms)
        int64_t start = i;
        while (start > 0 && i - start < lookBack && mono[(size_t) start - 1] > quiet) --start;
        if (start - last < minGap) continue;
        if (start > 0)
        {
            slices.push_back (start);
            if ((int) slices.size() >= settings.maxSlices) break;
        }
        last = start;
        // the new hit becomes the reference: no re-trigger on its own decay
        slow = std::max (slow, fast / ratio);
    }
    return slices;
}
} // namespace ohpe::sample
