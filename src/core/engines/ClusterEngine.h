// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "../dsp/Oscillators.h"
#include "PolyEngine.h"

namespace ohpe
{
/**
    Cluster engine: every note plays a stack of up to seven band-limited oscillators, detuned
    against each other and spread across the stereo field. The stack can be a unison or a chord
    shape (octaves, fifths, triads, sevenths, fourths ...). Stereo resonant low-pass per voice.
*/
class ClusterEngine;

namespace cluster
{
/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Detune = 0, Spread, Tone, Chord, // macros 1-4
    StackVoices, Shape, SideLevel, Resonance, KeyTrack, Drift,
    numParams
};

/** Chord shapes (append-only, same order as the "chord" choice). */
enum ChordShape
{
    Unison = 0, Octave, Fifth, Major, Minor, Sus2, Sus4, Major7, Minor7, Dominant7, Add9, Fourths,
    numShapes
};

constexpr int maxStack = 7;

/** Semitone offset of each stack slot for a chord shape (slot 0 is always the played note). */
int chordInterval (int shape, int slot) noexcept;

struct Shared
{
    float detune = 0.3f, spread = 0.6f, tone = 0.7f;
    int chord = Unison, stackVoices = 7;
    float shape = 0.0f, sideLevel = 0.7f, resonance = 0.1f, keyTrack = 0.5f, drift = 0.2f;
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
    dsp::MorphOsc osc[maxStack];
    float driftValue[maxStack] {}, driftTarget[maxStack] {};
    dsp::Svf filterL, filterR;
    float cutoffOct = -1.0f;
    dsp::Random rnd;
};
} // namespace cluster

class ClusterEngine final : public PolyEngine<cluster::Voice>
{
public:
    ClusterEngine();
    EngineInfo info() const noexcept override { return { "cluster", "Cluster", "Cluster", "cluster" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void setParam (int localIndex, float value) noexcept override;

    const cluster::Shared& getShared() const noexcept { return shared; }

private:
    cluster::Shared shared;
};
} // namespace ohpe
