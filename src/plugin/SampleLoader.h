// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "sample/SampleData.h"
#include "sample/SampleState.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace ohpe
{
/**
    Loads sample files (WAV, AIFF, FLAC, Ogg Vorbis) from disk into the core's sample engines
    and keeps their paths in the state ("samples" section). Message thread only.

    The core never touches files: this class decodes a file into an immutable SampleData and
    hands it to the Sampler / Grain engine or a drum pad, which publish it lock-free to the
    audio thread.
*/
class SampleLoader
{
public:
    explicit SampleLoader (Core& core);

    /** File patterns for a file chooser, e.g. "*.wav;*.aif;*.aiff;*.flac;*.ogg". */
    juce::String filePatterns() const;

    /** Decodes a file (at most `maxSeconds`, mono or the first two channels). nullptr on failure. */
    SamplePtr decode (const juce::File& file, juce::String* error = nullptr, double maxSeconds = 600.0);

    /** One sample over the whole keyboard of part `part`'s Sampler (rootKey -1 = "root" parameter). */
    bool loadSamplerSample (const juce::File& file, int rootKey = -1, int part = 0);
    /** Adds a multisample zone to part `part`'s Sampler. */
    bool addSamplerZone (const juce::File& file, int keyLo, int keyHi, int rootKey, int velLo = 1, int velHi = 127, int part = 0);
    /** The sample the Grain engine of part `part` granulates. */
    bool loadGrainSample (const juce::File& file, int part = 0);
    /** A sample for drum pad `pad` (0-based); switches the pad's type to Sample. */
    bool loadDrumPad (int pad, const juce::File& file);

    void clearSampler (int part = 0);
    void clearGrain (int part = 0);
    void clearDrumPad (int pad);

    /** Stores the current sample paths in the core's "samples" state section (before saving). */
    void storeInState();
    /** Reloads the samples named in the core's "samples" section (after loading a state). */
    sample::RestoreResult restoreFromState();

    /** Periodic message-thread work: re-slicing after the slice sensitivity moved. */
    void timerTick();

    /** Text of the last error (English; the UI translates it via i18n::tr). */
    const juce::String& lastError() const noexcept { return error; }

private:
    Core& core;
    juce::AudioFormatManager formats;
    juce::String error;
};
} // namespace ohpe
