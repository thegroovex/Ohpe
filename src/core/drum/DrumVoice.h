// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"
#include "../dsp/Filters.h"
#include "../sample/SampleData.h"

#include <array>

namespace ohpe::drum
{
/** Synth drum models in their permanent order (value of "drum.padN.type", append-only). */
enum class PadType
{
    Kick = 0,
    Snare,
    Hat,
    Clap,
    Tom,
    Perc,
    numTypes
};

/** What a pad plays (value of "drum.padN.src"): its sample (falls back to the model), or the model. */
enum class PadSource
{
    Sample = 0,
    Synth,
    numSources
};

/** One pad's settings (plain values, from the "drum.padN.*" parameters). */
struct PadSettings
{
    PadSource source = PadSource::Sample;
    PadType type = PadType::Kick;
    float tune = 0.0f;   ///< semitones, relative to the pad's own pitch
    float decay = 0.5f;  ///< 0..1, 0.5 = natural (samples: >= 0.5 plays the whole sample)
    float level = 0.8f;  ///< 0..1
    float pan = 0.0f;    ///< -1..+1
    int choke = 0;       ///< 0 = none, 1..8 = choke group
    float tone = 0.5f;   ///< 0..1, 0.5 = natural; samples: below 0.5 darker

    // voicing of the synth model on this pad (factory kit, not parameters)
    float modelTune = 0.0f;  ///< semitones
    float modelDecay = 0.4f; ///< 0..1 base decay of the model
    float modelTone = 0.5f;  ///< 0..1 base tone of the model
};

/**
    One sounding drum hit: a synthetic drum model (kick, snare, hat, clap, tom, perc) or a
    sample. Real-time safe; everything is computed per sample from a few state variables.
*/
class DrumVoice
{
public:
    void prepare (float sampleRate) noexcept;

    /** Starts a hit. `sample` is the pad's published sample (may be nullptr). */
    void trigger (int padIndex, const PadSettings& s, float velocity, const SampleData* sample, uint64_t order) noexcept;
    /** Fast fade-out (choke group / retrigger), about 3 ms. */
    void choke() noexcept;
    void kill() noexcept { active = false; pad = -1; }

    /** Adds into l/r. `currentSample` = the pad's sample now; a different one ends a sample hit. */
    void render (float* l, float* r, int numSamples, const SampleData* currentSample) noexcept;

    bool isActive() const noexcept { return active; }
    bool isPlayingSample() const noexcept { return sampleMode; }
    bool isChoking() const noexcept { return chokeGain < 1.0f; }
    int padIndex() const noexcept { return pad; }
    int chokeGroup() const noexcept { return settings.choke; }
    uint64_t startOrder() const noexcept { return order; }

private:
    float coeffFor (float seconds) const noexcept { return std::exp (std::log (0.001f) / std::max (1.0f, seconds * sr)); }
    float nextSynth() noexcept;

    float sr = 48000.0f;
    bool active = false;
    int pad = -1;
    uint64_t order = 0;
    PadSettings settings;
    float gainL = 0.0f, gainR = 0.0f;
    float chokeGain = 1.0f, chokeStep = 0.0f;
    int64_t t = 0; ///< samples since trigger

    // model state
    float amp = 0.0f, ampCoeff = 0.0f;     ///< main envelope
    float amp2 = 0.0f, amp2Coeff = 0.0f;   ///< secondary envelope (noise / tail / body)
    float pitchEnv = 0.0f, pitchCoeff = 0.0f;
    float baseFreq = 100.0f;
    double ph[6] {};
    dsp::Random noise;
    dsp::Svf bp, hp;
    dsp::OnePole lp, lp2;

    // sample state
    const SampleData* sample = nullptr;
    bool sampleMode = false;
    bool sampleFilter = false;
    double pos = 0.0, rate = 1.0;
};
} // namespace ohpe::drum
