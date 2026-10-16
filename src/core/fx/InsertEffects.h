// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Insert effects for the per-part FX slot: chorus, phaser, bitcrush, filter drive.
// Each one is a complete insert (it carries its own dry / wet "mix" parameter).

#include "../dsp/Filters.h"
#include "IEffect.h"

#include <array>

namespace ohpe
{
/** Stereo chorus: two modulated delay lines (quadrature LFO), dry / wet mix. */
class ChorusEffect final : public IEffect
{
public:
    /** Rate, Depth, Mix: 0.2. Voices, Drift, Spread, LowCut: 0.8 punch pass (appended; their defaults give the 0.2
        chorus). Voices 2-4 = ensemble: staggered taps with phase-offset LFOs, panned across the stereo field. */
    enum Param { Rate = 0, Depth, Mix, Voices, Drift, Spread, LowCut, numParams };
    static constexpr int maxVoices = 4;
    EffectInfo info() const noexcept override { return { "chorus", "Chorus", "Chorus", "chorus" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.05; }

private:
    std::vector<float> bufL, bufR;
    int writePos = 0;
    float sr = 48000.0f, rate = 0.6f, depth = 0.5f, mix = 0.5f, phase = 0.0f;
    int voices = 1;
    float drift = 0.0f, spread = 0.0f, lowCutHz = 20.0f;
    std::array<float, maxVoices> driftPos {}, driftVel {};
    uint32_t seed = 0x2545f491u, driftTick = 0;
    float hpC = 0.0f, hpL1 = 0.0f, hpL2 = 0.0f, hpR1 = 0.0f, hpR2 = 0.0f, hpXL1 = 0.0f, hpXL2 = 0.0f, hpXR1 = 0.0f, hpXR2 = 0.0f;
};

/** Six-stage stereo phaser with feedback, exponential sweep 150 Hz .. 5 kHz. */
class PhaserEffect final : public IEffect
{
public:
    enum Param { Rate = 0, Depth, Feedback, Mix, numParams };
    static constexpr int numStages = 6;
    EffectInfo info() const noexcept override { return { "phaser", "Phaser", "Phaser", "phaser" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;

private:
    std::array<float, numStages> zl {}, zr {};
    float fbL = 0.0f, fbR = 0.0f;
    float sr = 48000.0f, rate = 0.4f, depth = 0.7f, feedback = 0.5f, mix = 0.5f, phase = 0.0f;
};

/** Bit depth and sample-rate reduction. */
class BitcrushEffect final : public IEffect
{
public:
    enum Param { Bits = 0, Downsample, Mix, numParams };
    EffectInfo info() const noexcept override { return { "bitcrush", "Bitcrush", "Bitcrusher", "bitcrush" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;

private:
    float bits = 8.0f, downsample = 1.0f, mix = 1.0f;
    float holdL = 0.0f, holdR = 0.0f, counter = 1.0f;
};

/** Overdriven state-variable filter: drive -> soft clip -> SVF (low / band / high pass). */
class FilterDriveEffect final : public IEffect
{
public:
    enum Param { Cutoff = 0, Resonance, Drive, Mode, Mix, numParams };
    EffectInfo info() const noexcept override { return { "filterdrive", "Filter Drive", "Filter-Drive", "filterdrive" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;

private:
    void update() noexcept;
    dsp::Svf svfL, svfR;
    dsp::Smoother cutoffSmooth;
    float sr = 48000.0f, cutoff = 2000.0f, resonance = 0.3f, drive = 0.3f, mix = 1.0f;
    int mode = 0;
    float dcC = 0.99935f, dcX[2] = { 0.0f, 0.0f }, dcY[2] = { 0.0f, 0.0f }; ///< 0.8: DC blocker (~5 Hz) after the clipper
    float dcBlock (float x, int c) noexcept
    {
        const float y = x - dcX[c] + dcC * dcY[c];
        dcX[c] = x;
        dcY[c] = std::abs (y) < 1.0e-20f ? 0.0f : y;
        return y;
    }
};
} // namespace ohpe
