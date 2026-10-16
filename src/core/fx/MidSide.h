// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "IEffect.h"

#include <array>
#include <vector>

namespace ohpe
{
/**
    Mid / side stereo processor (0.7): FX rack type "Mid/Side" (index 7), available in every slot of
    every part rack ("pN.fx.midside.*", "pN.fx2.midside.*" ...) and of the master rack ("mfxK.midside.*").

        M = (L + R) / 2,  S = (L - R) / 2
        bass mono:  S -= low part of S below "monofreq" (24 dB/oct.; mono-compatible lows)
        tilt:       one tilt EQ each for M and S (pivot 650 Hz, -6 .. +6 dB)
        levels:     M * mid gain, S * side gain * width (width 0 = mono, 1 = unchanged, 2 = double)
        micro-shift: S += amount * M delayed by 1..30 ms (Haas-style widening that cancels in mono)
        balance:    L / R gain (-1 = left only ... +1 = right only)
        L = M + S,  R = M - S

    With every parameter at its neutral value (width 100 %, levels and tilts 0 dB, bass mono
    off, micro-shift 0, balance centre) the effect is an exact (bit-identical) bypass.
    Gains are smoothed (20 ms); no allocation after prepare().
*/
class MidSideEffect final : public IEffect
{
public:
    /// Descriptor (= registry, page) order. The first four are the modulation-matrix targets "FX k Param 1-4".
    enum Param { Width = 0, Mid, Side, Balance, BassMono, MonoFreq, Haas, HaasTime, MidTilt, SideTilt, numParams };

    EffectInfo info() const noexcept override { return { "midside", "Mid/Side", "Mitte/Seite", "midside" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float plainValue) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 0.03; }

    /** True while the current settings leave the signal untouched (bit-exact bypass). */
    bool isNeutral() const noexcept;

    static constexpr float pivotHz = 650.0f;
    static constexpr float maxHaasSeconds = 0.03f;

private:
    struct Tilt
    {
        dsp::OnePole lp;
        dsp::Smoother low, high;
        float process (float x) noexcept
        {
            const float lo = lp.lowpass (x);
            return lo * low.next() + (x - lo) * high.next();
        }
    };
    static void tiltGains (float db, float& low, float& high) noexcept;
    bool settled() const noexcept;
    void snapSmoothers() noexcept;

    float sr = 48000.0f;
    float width = 1.0f, midDb = 0.0f, sideDb = 0.0f, monoHz = 120.0f, balance = 0.0f, haas = 0.0f, midTilt = 0.0f,
          sideTilt = 0.0f, haasTime = 0.012f;
    bool bassMono = false;
    bool active = false; ///< false = bypassed (filter states are reset when it becomes active again)

    dsp::Smoother midGain, sideGain, balL, balR, monoMix, haasAmt, haasDelay;
    Tilt midTiltF, sideTiltF;
    std::array<dsp::Svf, 2> sideHp; ///< two cascaded 12 dB high-passes = 24 dB/oct.
    std::vector<float> delayBuf;
    int writePos = 0;
};
} // namespace ohpe
