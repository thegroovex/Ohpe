// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "PolyEngine.h"

#include <array>

namespace ohpe
{
/**
    Vocal engine: formant synthesis. A glottal pulse source (Rosenberg flow derivative, its open
    phase set by "tension") plus breath noise drives five parallel formant resonators. "Vowel"
    morphs continuously through A - E - I - O - U, "formant" shifts all formants (smaller or
    larger vocal tract), vibrato and jitter make it sing.
*/
namespace vocal
{
/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Vowel = 0, Formant, Breath, Tension, // macros 1-4
    VibratoDepth, VibratoRate, Jitter,
    numParams
};

constexpr int numFormants = 5;
constexpr int numVowels = 5; ///< A, E, I, O, U

struct FormantSet
{
    float freq[numFormants];
    float gainDb[numFormants];
    float bandwidth[numFormants];
};

/** The vowel table (A, E, I, O, U). */
const FormantSet& vowelFormants (int vowel) noexcept;
/** Formants for a continuous vowel position 0 (A) .. 4 (U). */
FormantSet morphVowel (float position) noexcept;

struct Shared
{
    float vowel = 0.0f, formantShift = 0.0f, breath = 0.15f, tension = 0.5f;
    float vibratoDepth = 0.2f, vibratoRate = 5.5f, jitter = 0.1f;
};

/** Constant-peak-gain band-pass biquad (RBJ), transposed direct form II. */
struct Resonator
{
    float b0 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
    void set (float freq, float bandwidth, float sr) noexcept;
    float process (float x) noexcept;
    void reset() noexcept { z1 = z2 = 0.0f; }
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
    static float glottalFlow (float phase, float openQuotient) noexcept;

    double phase = 0.0;
    float lastFlow = 0.0f;
    float vibPhase = 0.0f, vibFade = 0.0f, jitterValue = 0.0f, noiseLp = 0.0f;
    std::array<Resonator, numFormants> res {};
    std::array<float, numFormants> gains {};
    dsp::Random rng;
    dsp::OnePole tilt, air;
};
} // namespace vocal

class VocalEngine final : public PolyEngine<vocal::Voice>
{
public:
    VocalEngine();
    EngineInfo info() const noexcept override { return { "vocal", "Vocal", "Vokal", "vocal" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void setParam (int localIndex, float value) noexcept override;

    const vocal::Shared& getShared() const noexcept { return shared; }

private:
    vocal::Shared shared;
};
} // namespace ohpe
