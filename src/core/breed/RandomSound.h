// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "SoundGenome.h"

#include <cstdint>
#include <vector>

/**
    p37: the RANDOM key ("Zufall") - a new sound for the edit part's engine that is always musically usable.

    The roll changes the engine's own sound parameters (as before), but each one inside a musical range of its own
    instead of anywhere on its knob: noise, crackle and breath are off in most rolls and low when they do come,
    cutoff, resonance, FM index / feedback and grain spray stay where the engine still sounds like an instrument,
    ratios and grain pitches land on musical intervals. The part's portamento and pitch envelope are reset with the
    roll (glide off; a short legato glide or a short pitch blip only in a few rolls), so a new sound never slides in.
    Level, tuning, voices, the sample choice, the envelopes, the effects and the matrix stay as they are.

    make() then plays the candidate offline on a private core (breed::Audition: C4 + G4, the factory presets'
    measuring chord) and checks it: a near-silent, broken (non-finite) or noise-like candidate (spectral flatness
    above maxFlatness) is rolled again (a few attempts, the best one is kept). Finally the part gain is set so the
    sound plays at the factory presets' median loudness (targetLufs: loudest 400 ms, K-weighted) with its true peak at or
    below truePeakCeilingDb. The gain sits before the part's effects; when a compressor or the like in the part's own
    chain keeps the level down even at the top of the gain range, the sound ends up as loud as the sound it was rolled
    from. Message / worker thread only - never the audio thread.

    Deterministic: a seed gives the same sound on every platform (breed::Rng).
*/
namespace ohpe::randomsound
{
/** Median loudness of the factory part presets in the audition render: the loudest 400 ms (momentary, K-weighted)
    of the chord, so plucks and pads are compared by how loud a note sounds, not by how long it lasts. Measured by the
    core test "random sound: the loudness target is the factory median". */
constexpr float targetLufs = -14.4f;
constexpr float truePeakCeilingDb = -1.0f;
/** A candidate quieter than this before the gain is set counts as near-silent and is rolled again. */
constexpr float silentLufs = -48.0f;
/** Spectral flatness (100 Hz - 10 kHz, averaged spectrum) above which a candidate counts as noise. */
constexpr float maxFlatness = 0.30f;
/** The part gain never goes above this (dB, the top of its range) to reach the target: a sound that would need more
    is rolled again. */
constexpr float maxGainDb = 24.0f;
constexpr int maxAttempts = 5;

struct Loudness
{
    float lufs = -200.0f;       ///< K-weighted gated (integrated) loudness (LUFS) of the whole render
    float momentaryMax = -200.0f; ///< the loudest 400 ms block (K-weighted, LUFS): what normalisation aims at
    float truePeakDb = -200.0f; ///< 4x oversampled peak (dBTP)
    float flatness = 0.0f;      ///< spectral flatness of the held chord: 0 = tonal .. 1 = white noise
    int nonFinite = 0;
};

/** Measures stereo audio at `sampleRate` (flatness over the held part, `holdSeconds` from the start). */
Loudness measure (const std::vector<float>& left, const std::vector<float>& right, double sampleRate, float holdSeconds = 0.7f);

/** The constrained roll: `base` with new values for the sound parameters of `engineType`, glide / pitch env reset. */
breed::Genome roll (const breed::Schema& s, const breed::Genome& base, int engineType, uint64_t seed);

/** True when a sound slides into its first note: portamento that acts on a new note (Auto / Always) or a pitch
    envelope. */
bool glideAtStart (const breed::Schema& s, const breed::Genome& g);
/** The loudest noise source among the parameters (noise, crackle, breath; 0 .. 1). */
float noiseParamLevel (const breed::Schema& s, const breed::Genome& g, int engineType);

struct Result
{
    breed::Genome genome;
    Loudness loudness;    ///< of the final sound (after the gain)
    float gainDb = 0.0f;  ///< part gain set
    int attempts = 0;
    int renders = 0;
    bool usable = false;  ///< passed every check (false only when every attempt failed: the best one is returned)
    /** Only when the gain reached its top and the sound is still more than 3 dB under the target: the loudness of the
        sound it was rolled from (its effects chain caps the level, e.g. a compressor first in the rack). */
    float baseMomentary = -200.0f;
};

/** Rolls, checks and level-normalises a new sound (see the class comment). */
Result make (breed::Audition& audition, const breed::Genome& base, int engineType, uint64_t seed);
} // namespace ohpe::randomsound
