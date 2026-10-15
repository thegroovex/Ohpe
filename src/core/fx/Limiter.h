// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"

namespace ohpe
{
/** Master safety limiter: instant attack, smooth release, ceiling -0.3 dBFS, soft knee. */
class Limiter
{
public:
    void prepare (float sampleRate) noexcept
    {
        release = std::exp (-1.0f / (0.12f * sampleRate));
        gain = 1.0f;
    }
    void reset() noexcept { gain = 1.0f; }

    void process (float* l, float* r, int n) noexcept
    {
        constexpr float ceiling = 0.966f; // -0.3 dBFS
        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
            const float target = peak > ceiling ? ceiling / peak : 1.0f;
            gain = target < gain ? target : target + (gain - target) * release;
            l[i] *= gain;
            r[i] *= gain;
        }
    }

    float currentGain() const noexcept { return gain; }

private:
    float release = 0.999f, gain = 1.0f;
};
} // namespace ohpe
