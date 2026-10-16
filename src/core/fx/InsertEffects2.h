// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// 0.8 (sound revision 3): FX types for every rack slot - comb, formant, tempo-synced wobble filter,
// tremolo / autopan and stereo width with a mono low end. Each one is a complete insert.

#include "../dsp/Filters.h"
#include "IEffect.h"

#include <array>
#include <vector>

namespace ohpe
{
namespace fxsync
{
/** Tempo divisions of the synced effects (append-only; 0 = free running in Hz). */
std::vector<std::string> namesEn();
std::vector<std::string> namesDe();
/** Cycle length in beats of division `d` (> 0), 0 for "Free". */
double beats (int d) noexcept;
/** Rate in Hz: `hz` when free, else from the tempo. */
float rateHz (int division, float hz, double bpm) noexcept;
} // namespace fxsync

/** Shared shape of the LFOs of these effects (phase 0..1 -> -1..+1). */
float fxLfoShape (int shape, float phase, float held) noexcept;

    enum Curve { Soft = 0, Hard, Fold, Tube, Rectify, Sine, Crush, numCurves };
    EffectInfo info() const noexcept override { return { "shaper", "Shaper", "Waveshaper", "shaper" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;

    static float shape (int curve, float x) noexcept;

private:
    struct Channel
    {
        dsp::Svf up1, up2, down1, down2;
        dsp::OnePole tone;
        float last = 0.0f, dcX = 0.0f, dcY = 0.0f;
    };
    float run (Channel& c, float x) noexcept;
    std::array<Channel, 2> ch;
    float sr = 48000.0f, drive = 0.3f, bias = 0.0f, toneHz = 12000.0f, mix = 1.0f, outGain = 1.0f;
    int curve = Soft;
};

    static constexpr int numBands = 3;
    EffectInfo info() const noexcept override { return { "squash", "Squash", "Squash", "squash" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;

    /** Static gain (dB) of one band at level `levelDb` for the depth / upward settings (tests). */
    static float bandGainDb (float levelDb, float depth, float upward) noexcept;

private:
    void updateTimes() noexcept;
    std::array<dsp::Svf, 4> lo1, lo2; ///< [channel*2 + stage]: low split (two cascaded Butterworth = 4th order)
    std::array<dsp::Svf, 4> mi1;      ///< mid / high split
    std::array<float, numBands> env {}, gainDb {};
    float sr = 48000.0f, depth = 0.5f, time = 0.5f, upward = 0.5f, mix = 1.0f, outGain = 1.0f;
    std::array<float, numBands> bandGain { 1.0f, 1.0f, 1.0f };
    float atk = 0.0f, rel = 0.0f, envAtk = 0.0f, envRel = 0.0f;
    bool monoLow = false; ///< the low band (below 120 Hz) summed to mono: a mono-safe bass
};

/** Modulated comb filter (stereo offset, damped feedback; negative feedback = hollow, odd harmonics). */
class CombEffect final : public IEffect
{
public:
    enum Param { Freq = 0, Feedback, Damp, Rate, Depth, Spread, Mix, numParams };
    EffectInfo info() const noexcept override { return { "comb", "Comb", "Kammfilter", "comb" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.5; }

private:
    std::vector<float> bufL, bufR;
    int writePos = 0;
    float zl = 0.0f, zr = 0.0f, sr = 48000.0f, freq = 300.0f, feedback = 0.6f, damp = 0.3f, rate = 0.2f, depth = 0.3f,
          spread = 0.3f, mix = 0.5f, phase = 0.0f;
};

/** Three-formant vowel filter (a e i o u), shiftable, with a (tempo-synced) LFO on the vowel. */
class FormantEffect final : public IEffect
{
public:
    enum Param { Vowel = 0, Shift, Resonance, Rate, Sync, Depth, Mix, numParams };
    EffectInfo info() const noexcept override { return { "formant", "Formant", "Formant", "formant" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    void setTempo (double bpm) noexcept override { tempo = bpm; }
    void noteOn() noexcept override { phase = 0.0f; }

private:
    std::array<dsp::Svf, 6> bp; ///< [channel * 3 + formant]
    float sr = 48000.0f, vowel = 0.0f, shift = 0.0f, reso = 0.5f, rate = 1.0f, depth = 0.0f, mix = 1.0f, phase = 0.0f;
    int sync = 0;
    double tempo = 120.0;
};

    EffectInfo info() const noexcept override { return { "flanger", "Flanger", "Flanger", "flanger" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.1; }

private:
    std::vector<float> bufL, bufR;
    int writePos = 0;
    float fbL = 0.0f, fbR = 0.0f, sr = 48000.0f, rate = 0.2f, depth = 0.7f, feedback = 0.5f, delayMs = 2.0f, mix = 0.5f,
          phase = 0.0f;
};

/**
    Wobble: a resonant filter swept between two cutoffs by a tempo-synced LFO (restarted by every
    new note when "Retrigger" is on), with drive in front.
*/
class WobbleEffect final : public IEffect
{
public:
    enum Param { Rate = 0, Sync, Shape, Low, High, Resonance, Mode, Drive, Retrigger, Mix, numParams };
    EffectInfo info() const noexcept override { return { "wobble", "Wobble", "Wobble", "wobble" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    void setTempo (double bpm) noexcept override { tempo = bpm; }
    void noteOn() noexcept override { if (retrigger) { phase = 0.0f; held = 1.0f; } }

private:
    dsp::Svf fl, fr;
    dsp::Random rnd { 0x51f7u };
    float sr = 48000.0f, rate = 2.0f, lowHz = 150.0f, highHz = 3000.0f, reso = 0.5f, drive = 0.2f, mix = 1.0f, phase = 0.0f,
          held = 0.0f, smoothOct = -1.0f;
    int sync = 4, shape = 0, mode = 0;
    bool retrigger = true;
    double tempo = 120.0;
};

/** Tremolo or autopan (left / right opposite), free or tempo-synced. */
class TremoloEffect final : public IEffect
{
public:
    enum Param { Rate = 0, Sync, Depth, Mode, Shape, numParams };
    EffectInfo info() const noexcept override { return { "tremolo", "Tremolo", "Tremolo", "tremolo" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    void setTempo (double bpm) noexcept override { tempo = bpm; }

private:
    float sr = 48000.0f, rate = 4.0f, depth = 0.5f, phase = 0.0f, smooth = 0.0f;
    int sync = 0, mode = 0, shape = 0;
    double tempo = 120.0;
};

/**
    Stereo width: Haas-style spread that stays mono-compatible (the added side signal cancels in
    the mono sum), side gain 0..200 % and a mono low end (the side signal below "Mono Below" is
    removed, 4th order; 20 Hz = off).
*/
class WidthEffect final : public IEffect
{
public:
    enum Param { Width = 0, Spread, Haas, MonoBelow, Output, numParams };
    EffectInfo info() const noexcept override { return { "width", "Width", "Stereobreite", "width" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.03; }

private:
    std::vector<float> buf;
    int writePos = 0;
    dsp::Svf lp1, lp2;
    float sr = 48000.0f, width = 1.0f, spread = 0.0f, haasMs = 12.0f, monoHz = 20.0f, outGain = 1.0f;
};
} // namespace ohpe
