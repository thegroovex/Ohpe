// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "DspUtil.h"

namespace ohpe::dsp
{
/** RBJ-cookbook biquad (transposed direct form II). */
class Biquad
{
public:
    void reset() noexcept { z1 = z2 = 0.0f; }

    void setLowShelf (float sr, float freq, float gainDb, float q = 0.707f) noexcept { design (sr, freq, gainDb, q, 0); }
    void setPeak (float sr, float freq, float gainDb, float q = 0.9f) noexcept { design (sr, freq, gainDb, q, 1); }
    void setHighShelf (float sr, float freq, float gainDb, float q = 0.707f) noexcept { design (sr, freq, gainDb, q, 2); }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = flush (b1 * x - a1 * y + z2);
        z2 = flush (b2 * x - a2 * y);
        return y;
    }

    bool isFlat() const noexcept { return flat; }

private:
    void design (float sr, float freq, float gainDb, float q, int type) noexcept
    {
        flat = std::abs (gainDb) < 0.01f;
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = twoPi * std::clamp (freq, 10.0f, sr * 0.45f) / sr;
        const float cw = std::cos (w0), sw = std::sin (w0);
        const float alpha = sw / (2.0f * q);
        float B0, B1, B2, A0, A1, A2;
        if (type == 1)
        {
            B0 = 1.0f + alpha * A; B1 = -2.0f * cw; B2 = 1.0f - alpha * A;
            A0 = 1.0f + alpha / A; A1 = -2.0f * cw; A2 = 1.0f - alpha / A;
        }
        else
        {
            const float sq = 2.0f * std::sqrt (A) * alpha;
            if (type == 0)
            {
                B0 = A * ((A + 1) - (A - 1) * cw + sq); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sq);
                A0 = (A + 1) + (A - 1) * cw + sq; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sq;
            }
            else
            {
                B0 = A * ((A + 1) + (A - 1) * cw + sq); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sq;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
    bool flat = true;
};
} // namespace ohpe::dsp
