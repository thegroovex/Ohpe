// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "../dsp/Oscillators.h"
#include "PolyEngine.h"

namespace ohpe
{
/** Virtual-analog engine: two morphing oscillators + sub + noise into a ladder or state-variable filter. */
class AnalogEngine;

namespace analog
{
/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Shape = 0, Detune, Cutoff, Resonance, // macros 1-4
    Osc2Octave, OscMix, Sub, Noise, FilterType, Drive, KeyTrack,
    numParams
};

enum FilterType { LadderLP = 0, SvfLP, SvfBP, SvfHP };

struct Shared
{
    float shape = 0.2f, detune = 0.15f, cutoff = 3000.0f, resonance = 0.2f;
    float osc2Octave = 0.0f, oscMix = 0.5f, sub = 0.0f, noise = 0.0f, drive = 0.2f, keyTrack = 0.5f;
    int filterType = LadderLP;
};

class Voice : public VoiceBase
{
public:
    const Shared* shared = nullptr;

    void prepare (float sampleRate) noexcept;
    void start (int note, float vel, bool legato, const VoiceCommon& c) noexcept;
    void release() noexcept { releaseBase(); }
    void kill() noexcept;
    void render (float* l, float* r, int n, const VoiceCommon& c) noexcept;

private:
    dsp::MorphOsc osc1, osc2;
    dsp::SineOsc subOsc;
    dsp::Ladder ladder;
    dsp::Svf svf;
    dsp::Random noise;
};
} // namespace analog

class AnalogEngine final : public PolyEngine<analog::Voice>
{
public:
    AnalogEngine();
    EngineInfo info() const noexcept override { return { "analog", "Analog", "Analog", "analog" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void setParam (int localIndex, float value) noexcept override;

    const analog::Shared& getShared() const noexcept { return shared; }

private:
    analog::Shared shared;
};
} // namespace ohpe
