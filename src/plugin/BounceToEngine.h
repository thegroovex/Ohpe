// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "bounce/Bounce.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>

namespace ohpe
{
/**
    p23 (issue #15): bounce to engine. One button moves a tape track (or the tape mix, or the loop
    range while the tape loop is on) or a few bars of the live input into the Grain or Sampler engine
    of a part - as a one-shot or as an endless loop (see core/bounce/Bounce.h).

    The bounce is written as a 24-bit WAV into the user bounce library (<user application data>/
    groovex/Ohpe/Bounces, environment variable OHPE_BOUNCE_DIR overrides it); the engines keep the
    file's path, so the "samples" state section and the session restore it like any loaded sample.
    An endless loop carries a "smpl" loop chunk (loop [1, frames - 3), see sample::hasEndlessLayout).
    When the library cannot be written the bounce still plays, for this session only.

    Settings: "bounce.src", "bounce.to", "bounce.loop", "bounce.len"; "bounce.go" triggers it (the
    processor's remote controls). Message thread unless noted.
*/
class BounceManager
{
public:
    explicit BounceManager (Core& core);

    static juce::File defaultLibraryFolder();
    void setLibraryFolder (const juce::File& folder) { library = folder; }
    const juce::File& libraryFolder() const noexcept { return library; }

    /** Sets a parameter on host and registry (the processor keeps automation in sync). */
    bounce::Setter setParam;

    /**
        Bounces with the "bounce.*" settings. part -1 = the edit part ("ui.part"), target -1 = "bounce.to",
        source -1 = "bounce.src". Tape sources bounce at once; the input starts a recording (the bounce
        follows in timerTick()). False when nothing could be bounced (empty track, busy, silent).
    */
    bool bounce (int part = -1, int target = -1, int source = -1);
    /** Ends an input recording early and bounces what was recorded (false = nothing recorded). */
    bool finishRecording();
    void cancel();

    bool recording() const noexcept { return capture.recording(); }
    float progress() const noexcept { return capture.progress(); }
    int recordingPart() const noexcept { return pendingPart; }

    /** Audio thread: the live input (the processor pushes the input bus into it). */
    void pushInput (const float* l, const float* r, int n) noexcept { capture.push (l, r, n); }

    /** Periodic message-thread work: the end of an input recording. */
    void timerTick();

    /** Messages for the display (translated), counted. */
    int messageCount() const noexcept { return messages.load(); }
    juce::String lastMessage() const { const juce::ScopedLock sl (lock); return message; }
    /** True when the last message reports a finished bounce (the editor jumps to the engine page). */
    bool lastSucceeded() const noexcept { return succeeded.load(); }
    int lastPart() const noexcept { return donePart.load(); }
    int lastTarget() const noexcept { return doneTarget.load(); }

    /** The file written for the last bounce (empty = session only). */
    juce::File lastFile() const { const juce::ScopedLock sl (lock); return written; }

private:
    bool finish (bounce::Audio audio, int part, int target, bool endless, const std::string& name);
    juce::File writeWav (const SampleData& s, const std::string& name);
    void post (const juce::String& msg, bool ok);

    Core& core;
    juce::File library;
    bounce::StereoCapture capture;
    int pendingPart = -1, pendingTarget = 0;
    bool pendingEndless = true;
    juce::CriticalSection lock;
    juce::String message;
    juce::File written;
    std::atomic<int> messages { 0 }, donePart { -1 }, doneTarget { 0 };
    std::atomic<bool> succeeded { false };
};
} // namespace ohpe
