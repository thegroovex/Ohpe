// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "IEffect.h"

namespace ohpe
{
/** Stereo / ping-pong delay with tone-filtered feedback. 100 % wet (send effect). */
class DelayEffect final : public IEffect
{
public:
    enum Param { Time = 0, Feedback, Tone, PingPong, numParams };

    EffectInfo info() const noexcept override { return { "delay", "Delay", "Delay", "delay" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 10.0; }

private:
    std::vector<float> bufL, bufR;
    int writePos = 0;
    float sr = 48000.0f, timeSec = 0.375f, feedback = 0.4f, tone = 0.6f;
    bool pingPong = true;
    dsp::Smoother delaySmoother;
    dsp::OnePole toneL, toneR;
};
} // namespace ohpe
