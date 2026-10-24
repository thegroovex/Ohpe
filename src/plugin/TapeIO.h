// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Tape audio in the plug-in state, and the offline bounce (master + stems as 24-bit WAV).

#include "OhpeCore.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <string>

namespace ohpe::tapeio
{
/**
    Plug-in state layout (0.2):  <state JSON> '\0' <tape audio, TapeMachine::saveAudio format>
    The JSON part is the same document as before; the tape part is only written when the tape
    holds audio. States without a tape part (0.1, or an empty tape) load as before and leave
    an empty tape.
*/
void appendTapeAudio (Core& core, juce::MemoryBlock& state);

/** Splits a state into its JSON text and the optional tape part (nullptr / 0 if none). */
void splitState (const void* data, int size, std::string& json, const uint8_t*& tape, size_t& tapeSize);

/** Restores the tape from the tape part of a state (empty tape if there is none) and rewinds it to the start. Message thread. */
void restoreTape (Core& core, const uint8_t* tape, size_t tapeSize);

struct BounceOptions
{
    juce::File folder;                 ///< created if missing
    juce::String baseName { "Ohpe" };  ///< files: <base> Master.wav, <base> Track 1.wav ...
    double tailSeconds = 2.0;          ///< rendered after the end of the tape content (reverb / delay tails)
    bool master = true;                ///< the main output (Main bus), as the mixer is set up
    bool stems = true;                 ///< the eight tape tracks, each its own stereo file
    int bitDepth = 24;
};

struct BounceResult
{
    bool ok = false;
    juce::String error;                ///< localised
    juce::Array<juce::File> files;
    juce::int64 frames = 0;
    double sampleRate = 0.0;
};

/**
    Offline bounce (message thread, faster than real time, the live audio is not touched).
    Renders a copy of the instrument with the current parameters and the committed tape
    content from the start of the tape to the end of the content plus the tail: transport at
    speed 1 forward, no recording, sequencer stopped. Master = Main bus (channels routed to
    Out 1-8 are not in it, as in a host with those outputs enabled); stems = the eight tape
    tracks after level and tape character, before the mixer.
*/
BounceResult bounce (Core& live, const BounceOptions& options);
} // namespace ohpe::tapeio
