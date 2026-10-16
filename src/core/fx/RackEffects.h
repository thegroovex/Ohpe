// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// 0.7: the FX rack types - hyper / dimension, distortion, flanger, compressor, multiband, EQ, filter, echo
// (modern delay) and hall (modern plate / hall reverb). Every one is a self-contained IEffect; parameters that
// sweep (cutoffs, times, gains) glide sample by sample, so automation and the modulation matrix never zipper.

#include "../dsp/DspUtil.h"
#include "IEffect.h"

#include <array>
#include <atomic>
#include <vector>

namespace ohpe
{
namespace rackdsp
{
/** Zero-delay-feedback state-variable filter with free damping k (k = 1 / Q); all outputs at once. */
struct Zdf
{
    float g = 0.0f, k = 1.41421356f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    float lp = 0.0f, bp = 0.0f, hp = 0.0f;

    void set (float hz, float q, float sr) noexcept
    {
        g = std::tan (dsp::pi * std::clamp (hz, 5.0f, sr * 0.49f) / sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = lp = bp = hp = 0.0f; }
    void tick (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = dsp::flush (2.0f * v1 - ic1);
        ic2 = dsp::flush (2.0f * v2 - ic2);
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
};

/** RBJ biquad (transposed direct form II). */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
    enum Type { Peak = 0, LowShelf, HighShelf, LowPass, HighPass };
    void set (int type, float hz, float q, float gainDb, float sr) noexcept;
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = dsp::flush (b1 * x - a1 * y + z2);
        z2 = dsp::flush (b2 * x - a2 * y);
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
    /** Magnitude in dB at `hz` (UI curves). */
    float magnitudeDb (float hz, float sr) const noexcept;
};

/** 4x oversampler (8th-order elliptic-like Butterworth cascade up and down) for nonlinear stages. */
class Oversampler4
{
public:
    void prepare (float sampleRate) noexcept;
    void reset() noexcept;
    /** Upsamples x into out[4]. */
    void up (float x, float* out) noexcept;
    /** Downsamples 4 samples to one. */
    float down (const float* in) noexcept;

private:
    std::array<Biquad, 4> upF, downF;
};

/** Fractional delay line (cubic interpolation), power-of-two length. */
class DelayLine
{
public:
    void prepare (int maxSamples);
    void reset() noexcept;
    void push (float x) noexcept
    {
        buf[(size_t) w] = x;
        w = (w + 1) & mask;
    }
    /** Sample `d` samples ago (d >= 1). */
    float read (float d) const noexcept;
    int capacity() const noexcept { return mask; }

private:
    std::vector<float> buf;
    int w = 0, mask = 0;
};

/** One-pole glide toward a target value (per sample). */
struct Glide
{
    float v = 0.0f, t = 0.0f, a = 0.01f;
    void prepare (float sr, float seconds) noexcept { a = 1.0f - std::exp (-1.0f / (std::max (1.0e-4f, seconds) * sr)); }
    void snap (float x) noexcept { v = t = x; }
    float next() noexcept
    {
        v += (t - v) * a;
        return v;
    }
    bool settled() const noexcept { return std::abs (t - v) < 1.0e-6f * (1.0f + std::abs (t)); }
};

/** Tempo divisions of the synced rack effects (append-only; 0 = free). */
double syncBeats (int division) noexcept;
std::vector<std::string> syncNamesEn();
std::vector<std::string> syncNamesDe();
} // namespace rackdsp

/**
    Hyper: a multi-voice unison widener. Up to seven modulated copies of the signal, each detuned by a slow
    pitch drift (Doppler of a moving delay tap) and placed across the stereo field, then a "dimension" stage of
    cross-fed short delays that widens further. Retrigger restarts the voices' motion with every note.
*/
class HyperEffect final : public IEffect
{
public:
    enum Param { Rate = 0, Detune, Voices, Spread, Dimension, Retrigger, Mix, numParams };
    static constexpr int maxVoices = 7;
    EffectInfo info() const noexcept override { return { "hyper", "Hyper", "Hyper", "hyper" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Rate, Detune, Spread, Dimension };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.06; }
    void noteOn() noexcept override;

private:
    rackdsp::DelayLine lineL, lineR;
    std::array<rackdsp::DelayLine, 2> dim;
    std::array<float, maxVoices> phase {}, rateMul {}, panL {}, panR {};
    float sr = 48000.0f, rate = 0.6f, detune = 0.35f, spread = 0.8f, dimension = 0.4f, mix = 0.5f;
    int voices = 5;
    bool retrigger = false;
    rackdsp::Glide depthG, mixG;
};

/**
    Distortion with eight modes (tube, soft clip, hard clip, diode, linear fold, sine fold, asymmetric, downsample),
    a filter before (off / low / band / high-pass) and a tone low-pass after, 4x oversampled (except the
    intentionally aliasing downsample mode), DC-free, with output trim and mix.
*/
class DistortionEffect final : public IEffect
{
public:
    enum Param { Mode = 0, Drive, Bias, PreFilter, PreFreq, PostTone, Output, Mix, numParams };
    enum Curve { Tube = 0, SoftClip, HardClip, Diode, LinearFold, SineFold, Asymmetric, Downsample, numCurves };
    EffectInfo info() const noexcept override { return { "distortion", "Distortion", "Verzerrung", "distortion" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Drive, Bias, PreFreq, PostTone };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;

    /** The static transfer curve of a mode at a drive (0..1) and bias: output for input x (-1..1) - UI. */
    static float transfer (int mode, float drive, float bias, float x) noexcept;

private:
    struct Channel
    {
        rackdsp::Zdf pre, post;
        rackdsp::Oversampler4 os;
        float dcX = 0.0f, dcY = 0.0f, held = 0.0f, holdPhase = 0.0f;
    };
    std::array<Channel, 2> ch;
    float sr = 48000.0f, preFreq = 800.0f, postTone = 1.0f, bias = 0.0f;
    int mode = Tube, preFilter = 0;
    rackdsp::Glide driveG, outG, mixG, preG, postG;
};

/** Stereo flanger: modulated short delay with positive / negative feedback, free or tempo-synced. */
class FlangerRackEffect final : public IEffect
{
public:
    enum Param { Rate = 0, Sync, Depth, Delay, Feedback, Stereo, Mix, numParams };
    EffectInfo info() const noexcept override { return { "flange", "Flanger", "Flanger", "flanger" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Rate, Depth, Delay, Feedback };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.2; }
    void setTempo (double bpm) noexcept override { tempo = bpm; }
    void noteOn() noexcept override {}

private:
    rackdsp::DelayLine lineL, lineR;
    float sr = 48000.0f, rate = 0.25f, phase = 0.0f, stereo = 0.25f, fbL = 0.0f, fbR = 0.0f;
    int sync = 0;
    double tempo = 120.0;
    rackdsp::Glide depthG, delayG, fbG, mixG;
};

/** Compressor: stereo-linked, soft knee, attack / release, makeup, mix (parallel compression). */
class CompressorEffect final : public IEffect
{
public:
    enum Param { Threshold = 0, Ratio, Attack, Release, Knee, Makeup, Mix, numParams };
    EffectInfo info() const noexcept override { return { "compressor", "Compressor", "Kompressor", "compressor" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Threshold, Ratio, Attack, Makeup };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    /** Current gain reduction in dB (>= 0) - UI meter, any thread. */
    float gainReductionDb() const noexcept { return grUi.load (std::memory_order_relaxed); }
    /** Static curve (gain change in dB) for an input level - UI. */
    static float staticGainDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept;

private:
    float sr = 48000.0f, threshold = -18.0f, ratio = 4.0f, attackMs = 10.0f, releaseMs = 120.0f, knee = 6.0f, env = -120.0f, grDb = 0.0f;
    float atkC = 0.0f, relC = 0.0f;
    rackdsp::Glide makeupG, mixG;
    std::atomic<float> grUi { 0.0f };
};

/**
    Multiband: three bands (Linkwitz-Riley 4th-order crossovers with phase-compensated sum, so depth 0 is flat),
    each compressed downward above and lifted upward below its own level window - the "squash" sound with one
    depth knob, plus balance of upward / downward, time, band gains, output and mix.
*/
class MultibandEffect final : public IEffect
{
public:
    enum Param { Depth = 0, Upward, Downward, Time, LowGain, MidGain, HighGain, LowSplit, HighSplit, Output, Mix, numParams };
    static constexpr int numBands = 3;
    EffectInfo info() const noexcept override { return { "multiband", "Multiband", "Multiband", "multiband" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Depth, LowGain, MidGain, HighGain };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    /** Gain change of a band in dB (UI, any thread; negative = pulled down). */
    float bandGainDb (int band) const noexcept { return bandUi[(size_t) band].load (std::memory_order_relaxed); }
    /** Static gain (dB) of a band at a level for depth / upward / downward (tests, UI). */
    static float staticGainDb (float levelDb, float depth, float upward, float downward) noexcept;
    /** The two halves of staticGainDb: downward (level envelope) and upward lift (fast envelope, see process). */
    static float downGainDb (float levelDb, float depth, float downward) noexcept;
    static float upGainDb (float levelDb, float depth, float upward) noexcept;

private:
    struct Split
    {
        rackdsp::Zdf a, b; // two cascaded Butterworth sections = LR4
    };
    void setSplits() noexcept;
    std::array<Split, 2> lowLp, lowHp, highLp, highHp; ///< [channel]
    std::array<Split, 2> lowAp;                        ///< low band through the high split's all-pass (phase match)
    std::array<float, numBands> env {}, gain {};
    std::array<float, numBands> envFast {}; ///< instant attack, 40 ms release: the upward lift lets go at a note onset
    float fastRel = 0.0f;
    float sr = 48000.0f, depth = 0.5f, upward = 1.0f, downward = 1.0f, time = 0.5f, lowSplit = 120.0f, highSplit = 2500.0f;
    std::array<float, numBands> bandLevel { 1.0f, 1.0f, 1.0f };
    float atk = 0.0f, rel = 0.0f, gainA = 0.0f;
    rackdsp::Glide outG, mixG;
    std::array<std::atomic<float>, numBands> bandUi {};
};

/** EQ: low shelf, three parametric bands, high shelf; coefficients glide (zipper-free sweeps); output trim. */
class EqEffect final : public IEffect
{
public:
    enum Param
    {
        LowFreq = 0, LowGain, P1Freq, P1Gain, P1Q, P2Freq, P2Gain, P2Q, P3Freq, P3Gain, P3Q, HighFreq, HighGain, Output, numParams
    };
    static constexpr int numBands = 5;
    EffectInfo info() const noexcept override { return { "eq", "EQ", "EQ", "eq" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { P1Freq, P1Gain, P2Freq, P2Gain };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    /** Response in dB at `hz` of the given settings (UI curve; values = the 14 plain parameter values). */
    static float responseDb (const float* values, float hz, float sampleRate) noexcept;

private:
    void update (bool snap) noexcept;
    std::array<float, numParams> target {}, current {};
    std::array<std::array<rackdsp::Biquad, numBands>, 2> bands;
    float sr = 48000.0f, outGain = 1.0f;
    bool dirty = true;
    int countdown = 0;
};

/**
    Filter: low-pass / high-pass (12 or 24 dB), band-pass, notch, comb (+ / -) and vowel formant, with drive in front.
    The cutoff glides in octaves per sample, so sweeps and modulation stay smooth even with high resonance.
*/
class FilterRackEffect final : public IEffect
{
public:
    enum Param { Type = 0, Cutoff, Resonance, Drive, Mix, numParams };
    enum Kind { LP12 = 0, LP24, HP12, HP24, BP, Notch, CombPlus, CombMinus, Formant, numKinds };
    EffectInfo info() const noexcept override { return { "filter", "Filter", "Filter", "filter" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Cutoff, Resonance, Drive, Mix };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.3; }

private:
    struct Channel
    {
        rackdsp::Zdf a, b;
        std::array<rackdsp::Zdf, 3> f;
        rackdsp::DelayLine comb;
        float damp = 0.0f;
    };
    std::array<Channel, 2> ch;
    float sr = 48000.0f, reso = 0.3f;
    int kind = LP24;
    rackdsp::Glide octG, driveG, mixG;
};

/**
    Echo: modern stereo delay - stereo, ping-pong or tap (two taps, right at a ratio of the left), free time or synced
    to the tempo, feedback with low-cut / high-cut in the loop and a gentle saturation, smooth time changes (tape-like
    glide instead of clicks).
*/
class EchoEffect final : public IEffect
{
public:
    enum Param { Mode = 0, Sync, Time, Ratio, Feedback, LowCut, HighCut, Width, Mix, numParams };
    enum Modes { Stereo = 0, PingPong, Tap, numModes };
    EffectInfo info() const noexcept override { return { "echo", "Echo", "Echo", "echo" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Feedback, Time, HighCut, Width };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 8.0; }
    void setTempo (double bpm) noexcept override { tempo = bpm; }

private:
    float delaySamples() const noexcept;
    rackdsp::DelayLine lineL, lineR;
    std::array<rackdsp::Zdf, 2> lowCut, highCut;
    float sr = 48000.0f, timeMs = 375.0f, ratio = 0.75f, lowHz = 120.0f, highHz = 8000.0f, width = 1.0f;
    int mode = Stereo, sync = 0;
    double tempo = 120.0;
    rackdsp::Glide timeG, fbG, mixG;
};

/**
    Hall: modern plate / hall reverb - input diffusion, an eight-line feedback delay network with gently modulated
    lines (no metallic ringing), decay time in seconds with damping in the loop, pre-delay and stereo width.
*/
class HallEffect final : public IEffect
{
public:
    enum Param { Mode = 0, Size, Decay, Damping, PreDelay, Width, Mix, numParams };
    static constexpr int numLines = 8;
    EffectInfo info() const noexcept override { return { "hall", "Hall", "Raum", "hall" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Size, Decay, Damping, PreDelay };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return std::min (30.0, (double) decay * 1.5 + 0.3); }

private:
    void updateLoop() noexcept;
    std::array<rackdsp::DelayLine, numLines> lines;
    std::array<float, numLines> baseLen {}, gainL {}, lpState {}, modPhase {};
    std::array<rackdsp::DelayLine, 4> diffuse;
    std::array<float, 4> diffuseLen {};
    rackdsp::DelayLine pre;
    float sr = 48000.0f, size = 0.6f, decay = 2.5f, damping = 0.4f, preMs = 12.0f, width = 1.0f, lpCoeff = 0.3f;
    int mode = 1;
    bool loopDirty = true;
    rackdsp::Glide mixG, sizeG;
};
} // namespace ohpe
