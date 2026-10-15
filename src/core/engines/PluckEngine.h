// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "PolyEngine.h"

#include <vector>

namespace ohpe
{
/**
    Pluck engine: a plucked waveguide string (delay line with a fractional all-pass tuner and a
    loss filter, excited by a shaped noise burst with pick-position comb) crossfaded with a modal
    mallet (five tuned two-pole resonators struck by a raised-cosine mallet), followed by a small
    resonant body. Tuning compensates the phase delay of the loop filters exactly, so pitch stays
    accurate across the keyboard.
*/
class PluckEngine;

namespace pluck
{
/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Material = 0, Damping, Brightness, Body, // macros 1-4
    Excite, PickPosition, BodySize,
    numParams
};

struct Shared
{
    float material = 0.0f, damping = 0.35f, brightness = 0.55f, body = 0.3f;
    float excite = 0.6f, pickPosition = 0.18f, bodySize = 0.5f;
};

class Voice : public VoiceBase
{
public:
    static constexpr int numModes = 5;
    static constexpr float minFreq = 20.0f;

    const Shared* shared = nullptr;

    void prepare (float sampleRate);
    void start (int note, float vel, bool legato, const VoiceCommon& c) noexcept;
    void release() noexcept { releaseBase(); }
    void kill() noexcept;
    void render (float* l, float* r, int n, const VoiceCommon& c) noexcept;

private:
    void exciteString (float freq, float hardness) noexcept;
    void clearState() noexcept;

    // string
    std::vector<float> line, burst;
    int mask = 0, writePos = 0;
    float apX1 = 0.0f, apY1 = 0.0f, lpState = 0.0f, hpState = 0.0f;
    // mallet
    float modeY1[numModes] {}, modeY2[numModes] {};
    int malletPos = 0, malletLength = 0;
    // body
    dsp::Svf body1, body2, body3;
    dsp::Random noise;
};
} // namespace pluck

class PluckEngine final : public PolyEngine<pluck::Voice>
{
public:
    PluckEngine();
    EngineInfo info() const noexcept override { return { "pluck", "Pluck", "Pluck", "pluck" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void setParam (int localIndex, float value) noexcept override;

    const pluck::Shared& getShared() const noexcept { return shared; }

private:
    pluck::Shared shared;
};
} // namespace ohpe
