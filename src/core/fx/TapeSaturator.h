// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"

namespace ohpe
{
/**
    Tape-style saturation: soft compression of peaks plus a gentle high-frequency loss that
    grows with the amount. Amount 0 is an exact bypass. Used by the tape tracks and by the
    master "tape" stage. Real-time safe.
*/
class TapeSaturator
{
public:
    void prepare (float sampleRate) noexcept
    {
        sr = sampleRate > 0.0f ? sampleRate : 48000.0f;
        setAmount (amount);
        reset();
    }
    void reset() noexcept { lpL.reset(); lpR.reset(); }

    void setAmount (float a) noexcept
    {
        amount = dsp::clamp01 (a);
        drive = 1.0f + 5.0f * amount;
        outGain = 1.0f / std::pow (drive, 0.6f);
        lpL.setCutoff (20000.0f - 9000.0f * amount, sr);
        lpR.setCutoff (20000.0f - 9000.0f * amount, sr);
    }
    float getAmount() const noexcept { return amount; }
    bool isBypassed() const noexcept { return amount <= 0.0f; }

    float processSample (float x, bool left) noexcept
    {
        const float y = dsp::softClip (x * drive) * outGain;
        return left ? lpL.lowpass (y) : lpR.lowpass (y);
    }

    void process (float* l, float* r, int n) noexcept
    {
        if (isBypassed()) return;
        for (int i = 0; i < n; ++i)
        {
            l[i] = processSample (l[i], true);
            r[i] = processSample (r[i], false);
        }
    }

private:
    float sr = 48000.0f, amount = 0.0f, drive = 1.0f, outGain = 1.0f;
    dsp::OnePole lpL, lpR;
};
} // namespace ohpe
