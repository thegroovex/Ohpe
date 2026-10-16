// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "UserTable.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
    Audio to wavetable analysis (issue #16). Pure functions, no JUCE, never on the audio thread
    (the plug-in runs them on a worker thread).

    Wavetable frames: the sound's pitch is detected (YIN difference function, refined per frame),
    then one period is cut at `frames` evenly spaced points of the sound (period-synchronous
    slicing; the end of each period is crossfaded into the period before it so the cycle loops
    without a click), resampled to 2048 samples, phase-aligned (fundamental starts as a sine, so
    neighbouring frames morph without phase cancellation) and optionally smoothed across frames
    in the spectral domain. UserTable::create removes DC, band-limits and builds the mipmaps.
    Unpitched sounds are cut into 2048-sample windows instead.

    Formant material (Vocal engine): the sound is decimated to about 11 kHz, every frame is
    analysed by linear prediction (order 12, autocorrelation method, Levinson-Durbin) and the
    five strongest peaks of the LPC envelope between 150 Hz and 5 kHz become the frame's five
    formants (frequency, level relative to the strongest, -3 dB bandwidth).
*/
namespace ohpe::usertable
{
struct AnalysisOptions
{
    int frames = 64;          ///< wavetable frames 8 .. 256
    float smoothing = 0.0f;   ///< 0 = off .. 1 = strong (spectral smoothing across neighbouring frames)
    bool alignPhase = true;
    bool formants = true;     ///< also extract the formant material (min (frames, 64) formant frames)
    double maxSeconds = 60.0; ///< longer sounds are analysed over their first maxSeconds
};

struct AnalysisResult
{
    bool ok = false;
    std::string error;          ///< English, translated by the UI ("The sound is silent." ...)
    bool pitched = false;
    float pitchHz = 0.0f;       ///< detected pitch (0 = unpitched)
    int numFrames = 0;
    std::vector<float> frames;  ///< numFrames * frameSize raw frames (before UserTable::create)
    std::vector<FormantFrame> formants;
};

/** Progress 0 .. 1; return false to cancel. */
using Progress = std::function<bool (float)>;

/** YIN pitch of x[0 .. n) in Hz (0 = no clear pitch). `clarity` (optional) 0 .. 1. */
float detectPitch (const float* x, int64_t n, double sampleRate, float minHz = 30.0f, float maxHz = 2000.0f, float* clarity = nullptr);

/** Full analysis of a mono sound. */
AnalysisResult analyse (const float* mono, int64_t n, double sampleRate, const AnalysisOptions& options, const Progress& progress = {});

/** Formant frames evenly spaced over the sound (LPC). */
std::vector<FormantFrame> extractFormants (const float* mono, int64_t n, double sampleRate, int numFrames, const Progress& progress = {});

/** Spectral smoothing across frames (frames must be phase-aligned): amount 0 .. 1. */
void smoothFrames (std::vector<float>& frames, int numFrames, float amount);

/** Rotates every frame so its fundamental (or strongest harmonic) starts as a sine. */
void alignFrames (std::vector<float>& frames, int numFrames);
} // namespace ohpe::usertable
