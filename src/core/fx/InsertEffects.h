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
    enum Param { Rate = 0, Depth, Mix, numParams };
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
};
} // namespace ohpe
