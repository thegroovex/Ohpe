// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "IEffect.h"

#include <array>

namespace ohpe
{
/** Eight-line feedback-delay-network reverb with damping. 100 % wet (send effect). */
class ReverbEffect final : public IEffect
{
public:
    enum Param { Size = 0, Damping, PreDelay, numParams };
    static constexpr int numLines = 8;

    EffectInfo info() const noexcept override { return { "reverb", "Reverb", "Hall", "reverb" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 12.0; }

private:
    void updateCoefficients() noexcept;

    std::array<std::vector<float>, numLines> lines;
    std::array<int, numLines> lengths {}, pos {};
    std::array<float, numLines> gains {};
    std::array<dsp::OnePole, numLines> damp;
    std::vector<float> pre;
    int prePos = 0, preLen = 1;
    float sr = 48000.0f, size = 0.6f, damping = 0.4f, preDelay = 0.01f;
};
} // namespace ohpe
