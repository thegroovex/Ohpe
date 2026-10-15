// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "DspUtil.h"

namespace ohpe::dsp
{
struct AdsrParams
{
    float attack = 0.005f;  ///< seconds (linear rise to 1.0)
    float decay = 0.2f;     ///< seconds (exponential fall to sustain, -60 dB of the distance)
    float sustain = 0.8f;   ///< 0..1
    float release = 0.3f;   ///< seconds (exponential fall to silence, reaches -60 dB)
};

/**
    ADSR envelope. Attack is linear and reaches 1.0 after exactly `attack` seconds; decay and
    release are exponential and cover 60 dB in their time. Real-time safe, per-sample.
*/
class Adsr
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare (float sampleRate) noexcept { sr = sampleRate > 0.0f ? sampleRate : 48000.0f; }

    void setParams (const AdsrParams& p) noexcept
    {
        params = p;
        attackStep = 1.0f / std::max (1.0f, p.attack * sr);
        decayCoeff = coeffFor (p.decay);
        releaseCoeff = coeffFor (p.release);
    }

    void noteOn() noexcept { stage = Stage::Attack; }
    void noteOff() noexcept
    {
        if (stage != Stage::Idle) stage = Stage::Release;
    }
    void reset() noexcept
    {
        stage = Stage::Idle;
        level = 0.0f;
    }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::Idle: return 0.0f;
            case Stage::Attack:
                level += attackStep;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level = params.sustain + (level - params.sustain) * decayCoeff;
                if (level - params.sustain < 1.0e-4f) { level = params.sustain; stage = Stage::Sustain; }
                break;
            case Stage::Sustain: level = params.sustain; break;
            case Stage::Release:
                level *= releaseCoeff;
                if (level < 1.0e-4f) { level = 0.0f; stage = Stage::Idle; }
                break;
        }
        return level;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

private:
    float coeffFor (float seconds) const noexcept
    {
        const float n = std::max (1.0f, seconds * sr);
        return std::exp (std::log (0.001f) / n); // -60 dB after n samples
    }

    AdsrParams params;
    Stage stage = Stage::Idle;
    float sr = 48000.0f, level = 0.0f, attackStep = 1.0f, decayCoeff = 0.0f, releaseCoeff = 0.0f;
};
} // namespace ohpe::dsp
