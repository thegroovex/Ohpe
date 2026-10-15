// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Reverb.h"

namespace ohpe
{
std::vector<ParamDescriptor> ReverbEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("size", "Size", "Größe", 0.0f, 1.0f, 0.6f),
        continuous ("damping", "Damping", "Dämpfung", 0.0f, 1.0f, 0.4f),
        continuous ("predelay", "Pre-Delay", "Pre-Delay", 0.0f, 0.2f, 0.01f, ParamUnit::Seconds),
    };
}

void ReverbEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    static const float baseMs[numLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.1f };
    for (int i = 0; i < numLines; ++i)
    {
        lengths[(size_t) i] = std::max (8, (int) (baseMs[i] * 0.001f * sr * 1.6f));
        lines[(size_t) i].assign ((size_t) lengths[(size_t) i], 0.0f);
    }
    pre.assign ((size_t) (0.21f * sr) + 2, 0.0f);
    reset();
    updateCoefficients();
}

void ReverbEffect::reset() noexcept
{
    for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
    std::fill (pre.begin(), pre.end(), 0.0f);
    pos.fill (0);
    prePos = 0;
    for (auto& d : damp) d.reset();
}

void ReverbEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Size: size = v; break;
        case Damping: damping = v; break;
        case PreDelay: preDelay = v; break;
        default: break;
    }
    updateCoefficients();
}

void ReverbEffect::updateCoefficients() noexcept
{
    const float rt60 = 0.3f + size * size * 8.0f; // seconds
    for (int i = 0; i < numLines; ++i)
    {
        const float lenSec = (float) lengths[(size_t) i] / sr;
        gains[(size_t) i] = std::pow (10.0f, -3.0f * lenSec / rt60);
        damp[(size_t) i].setCutoff (18000.0f * std::pow (0.05f, damping), sr);
    }
    preLen = std::clamp ((int) (preDelay * sr), 1, (int) std::max<size_t> (1, pre.size() - 1));
}

void ReverbEffect::process (float* l, float* r, int n) noexcept
{
    if (pre.empty()) return;
    const int preSize = (int) pre.size();
    for (int s = 0; s < n; ++s)
    {
        pre[(size_t) prePos] = 0.5f * (l[s] + r[s]);
        int rp = prePos - preLen;
        if (rp < 0) rp += preSize;
        const float in = pre[(size_t) rp];
        if (++prePos >= preSize) prePos = 0;

        float o[numLines];
        for (int i = 0; i < numLines; ++i)
            o[i] = lines[(size_t) i][(size_t) pos[(size_t) i]];

        // Householder feedback matrix: y = x - 2/N * sum(x)
        float sum = 0.0f;
        for (float v : o) sum += v;
        sum *= 2.0f / (float) numLines;

        float outL = 0.0f, outR = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            const float fb = damp[(size_t) i].lowpass ((o[i] - sum) * gains[(size_t) i]);
            lines[(size_t) i][(size_t) pos[(size_t) i]] = dsp::flush (fb + in * 0.35f);
            if (++pos[(size_t) i] >= lengths[(size_t) i]) pos[(size_t) i] = 0;
            if (i & 1) outR += o[i]; else outL += o[i];
        }
        l[s] = outL * 0.35f;
        r[s] = outR * 0.35f;
    }
}
} // namespace ohpe
