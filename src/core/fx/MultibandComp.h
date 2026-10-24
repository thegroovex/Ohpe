// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p88: Multiband Comp - an aggressive three-band upward + downward compressor for punch and density ("mehr Druck") at
// a controlled level. Low / Mid / High between two phase-coherent Linkwitz-Riley crossovers (4th order; the bands sum
// to an all-pass, so DEPTH 0 or a neutral setting is flat in level). Each band: downward compression above its DOWN
// threshold and upward compression (a lift) below its UP threshold, both with very high ratios, its own attack /
// release (scaled together by TIME) and an output gain; any band can be soloed. Globals: DEPTH (wet / dry, applied as
// a per-band gain blend so the crossover phase stays coherent), TIME, UPWARD and DOWNWARD amount, IN and OUT gain.
// Every band detector is the zero-latency CompDetector (Compressor.h) with its onset guard: the first hit after a
// quiet passage is caught (no make-up / lift spike). The output passes a zero-latency peak safety stage (instant
// attack, 50 ms release, ceiling -1.5 dBFS sample peak, about -1 dBTP) that only acts when the output would go over.

#include "Compressor.h"
#include "IEffect.h"
#include "RackEffects.h"

#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace ohpe
{
class MultibandCompEffect final : public IEffect
{
public:
    static constexpr int numBands = 3;
    enum BandParam { DownThresh = 0, UpThresh, DownRatio, UpRatio, Attack, Release, Gain, Solo, numBandParams };
    enum Param
    {
        Depth = 0, Time, Upward, Downward, Input, Output, LowSplit, HighSplit,
        firstBandParam, // + band * numBandParams + BandParam
        numParams = firstBandParam + numBands * numBandParams
    };
    static constexpr int bandParam (int band, int k) noexcept { return firstBandParam + band * numBandParams + k; }
    static constexpr float safetyCeilingDb = -1.5f;

    MultibandCompEffect();

    EffectInfo info() const noexcept override { return { "mbcomp", "Multiband Comp", "Multiband-Kompressor", "mbcomp" }; }
    int modulationParam (int j) const noexcept override
    {
        static constexpr int p[4] = { Depth, Time, Downward, Upward };
        return j >= 0 && j < 4 ? p[j] : j;
    }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float value) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;

    /** Static gain change (dB) of a band at a detector level: downward above `downTh`, upward lift below `upTh`
        (soft knees, lift at most 24 dB and faded out towards the noise floor below -78 dBFS). */
    static float bandCurveDb (float levelDb, float downTh, float downRatio, float upTh, float upRatio, float downAmt, float upAmt) noexcept;

    /** UI meters (any thread): a band's input peak (dBFS) and its applied gain change (dB, + = lifted, before DEPTH). */
    float bandLevelDb (int band) const noexcept { return levelUi[(size_t) band].load (std::memory_order_relaxed); }
    float bandGainDb (int band) const noexcept { return gainUi[(size_t) band].load (std::memory_order_relaxed); }
    /** Gain reduction of the output safety stage now (dB, >= 0). */
    float safetyDb() const noexcept { return safetyUi.load (std::memory_order_relaxed); }

    /** The editor's starting points (plain values, index = Param). Message thread. */
    struct Style
    {
        const char* nameEn;
        const char* nameDe;
        std::array<float, numParams> values;
    };
    static const std::vector<Style>& styles();
    static std::array<float, numParams> defaults();

private:
    struct Band
    {
        CompDetector det;
        float downTh = -30.0f, upTh = -42.0f, downRatio = 60.0f, upRatio = 4.0f, attackMs = 1.0f, releaseMs = 100.0f;
        bool solo = false;
        rackdsp::Glide gain;
    };
    void setSplits() noexcept;
    void setTimes (int band) noexcept;

    std::array<rackdsp::Zdf, 2> lowA, lowB, lowHpA, lowHpB; ///< [channel]: LP4 / HP4 at the low split
    std::array<rackdsp::Zdf, 2> midA, midB, highA, highB;   ///< [channel]: LP4 / HP4 of the rest at the high split
    std::array<rackdsp::Zdf, 2> lowAp;                      ///< [channel]: the low band through the high split's all-pass
    std::array<Band, numBands> bands;
    float sr = 48000.0f, time = 1.0f, upward = 1.0f, downward = 1.0f, lowSplit = 120.0f, highSplit = 2500.0f;
    rackdsp::Glide depthG, inG, outG;
    float safetyEnv = 0.0f, safetyRel = 0.999f, ceiling = 0.84f;
    std::array<std::atomic<float>, numBands> levelUi {}, gainUi {};
    std::atomic<float> safetyUi { 0.0f };
};
} // namespace ohpe
