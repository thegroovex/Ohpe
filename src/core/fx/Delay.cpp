// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Delay.h"

namespace ohpe
{
std::vector<ParamDescriptor> DelayEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("time", "Delay Time", "Delay-Zeit", 0.01f, 2.0f, 0.375f, ParamUnit::Seconds, 0.5f),
        continuous ("feedback", "Feedback", "Feedback", 0.0f, 0.95f, 0.4f),
        continuous ("tone", "Tone", "Klang", 0.0f, 1.0f, 0.6f),
        toggle ("pingpong", "Ping-Pong", "Ping-Pong", true),
    };
}

void DelayEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    const size_t len = (size_t) (sr * 2.1f) + 4;
    bufL.assign (len, 0.0f);
    bufR.assign (len, 0.0f);
    delaySmoother.prepare (sr, 80.0f);
    delaySmoother.reset (timeSec * sr);
    reset();
}

void DelayEffect::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    toneL.reset();
    toneR.reset();
}

void DelayEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Time: timeSec = v; break;
        case Feedback: feedback = v; break;
        case Tone: tone = v; break;
        case PingPong: pingPong = v >= 0.5f; break;
        default: break;
    }
}

void DelayEffect::process (float* l, float* r, int n) noexcept
{
    if (bufL.empty()) return;
    const int len = (int) bufL.size();
    const float cutoff = 500.0f * std::pow (2.0f, tone * 5.0f); // 500 Hz .. 16 kHz
    toneL.setCutoff (cutoff, sr);
    toneR.setCutoff (cutoff, sr);
    delaySmoother.setTarget (std::clamp (timeSec * sr, 1.0f, (float) len - 3.0f));
    for (int i = 0; i < n; ++i)
    {
        const float d = delaySmoother.next();
        float rp = (float) writePos - d;
        if (rp < 0.0f) rp += (float) len;
        // a read position a hair below 0 rounds to exactly len when wrapped (float): index the start, never past the end
        int i0 = (int) rp;
        const float frac = rp - (float) i0;
        if (i0 >= len) i0 -= len;
        const int i1 = (i0 + 1) % len;
        const float dl = bufL[(size_t) i0] + (bufL[(size_t) i1] - bufL[(size_t) i0]) * frac;
        const float dr = bufR[(size_t) i0] + (bufR[(size_t) i1] - bufR[(size_t) i0]) * frac;
        const float inL = l[i], inR = r[i];
        const float fbL = toneL.lowpass (dl) * feedback, fbR = toneR.lowpass (dr) * feedback;
        if (pingPong)
        {
            bufL[(size_t) writePos] = dsp::flush (0.5f * (inL + inR) + fbR);
            bufR[(size_t) writePos] = dsp::flush (fbL);
        }
        else
        {
            bufL[(size_t) writePos] = dsp::flush (inL + fbL);
            bufR[(size_t) writePos] = dsp::flush (inR + fbR);
        }
        l[i] = dl;
        r[i] = dr;
        if (++writePos >= len) writePos = 0;
    }
}
} // namespace ohpe
