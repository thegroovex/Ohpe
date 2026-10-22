// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "tapemem/TapeMemory.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <thread>

namespace ohpe
{
/**
    p67: Tape Memory (Tape-Memory). The core's ring buffer always holds the last seconds of the output
    ("tapemem.src"); capture() takes "tapemem.len" of it and builds a playable Sampler instrument on a worker thread
    (silence trim, onsets, pitch - see core/tapemem/TapeMemory.h); timerTick() puts the result into the part on the
    message thread. The last capture of each part is saved in the state section "tapememory" (only while the part's
    Sampler still plays it) and can be exported as a 24-bit WAV into the user capture library (<user application data>/
    groovex/Ohpe/Captures; environment variable OHPE_CAPTURE_DIR overrides it). Message thread unless noted.
*/
class TapeMemoryManager
{
public:
    explicit TapeMemoryManager (Core& core);
    ~TapeMemoryManager();

    static juce::File defaultLibraryFolder();
    void setLibraryFolder (const juce::File& f) { library = f; }
    const juce::File& libraryFolder() const noexcept { return library; }

    /** Sets a parameter on host and registry (the processor keeps automation in sync). */
    bounce::Setter setParam;

    /** Captures with the "tapemem.*" settings; part / mode -1 = the settings. False when busy or the memory is empty. */
    bool capture (int part = -1, int mode = -1);
    /** Waits for a running analysis and applies it (tests, offline). */
    void finishNow();
    bool busy() const noexcept { return working.load(); }

    /** Periodic message-thread work: applies a finished analysis. */
    void timerTick();

    /** The part's capture while its Sampler still plays it (else empty). */
    const tapemem::Capture* captureOf (int part) const;
    /** p72: the part's last capture, playing or set aside (else nullptr). */
    const tapemem::Capture* storedCapture (int part) const;
    /**
        p72: a preset or project loads into the part. A capture still playing there leaves the part's Sampler (its
        zones are cleared, so the preset's own instrument / sample plays) but stays in this session's memory:
        restore() brings it back, exportWav() still writes it. True if a capture was set aside.
    */
    bool release (int part, bool announce = true);
    /** p72: puts the part's last capture (-1 = the last captured part) back into it (engine Sampler, mode, root). */
    bool restore (int part = -1);
    /** p72: only the audio of the part's stored capture goes back (the parameters are already there; preview revert). */
    bool reattach (int part);
    /** p72: every part's stored capture (playing or set aside) - a library preview keeps them to revert to. */
    const std::array<tapemem::Capture, Core::numParts>& storedCaptures() const noexcept { return captures; }
    void setStoredCaptures (const std::array<tapemem::Capture, Core::numParts>& c) { captures = c; }
    int lastCapturedPart() const noexcept { return lastPart.load(); }
    /** Writes the part's capture (-1 = the last one) as a WAV into the library; the file, or empty on failure. */
    juce::File exportWav (int part = -1);

    void storeInState();
    /** Puts the captures of the state section back; `clearOthers` = forget every other stored capture (session load). */
    int restoreFromState (bool clearOthers = true);

    /** Messages for the display (translated), counted. */
    int messageCount() const noexcept { return messages.load(); }
    juce::String lastMessage() const { const juce::ScopedLock sl (lock); return message; }

    /** Seconds held in the memory now (for the display). */
    double heldSeconds() const noexcept;

private:
    void post (const juce::String& msg);
    void join();

    Core& core;
    juce::File library;
    std::array<tapemem::Capture, Core::numParts> captures {};
    std::thread worker;
    std::atomic<bool> working { false }, ready { false };
    tapemem::Capture pending;
    int pendingPart = 0;
    int counter = 0;
    juce::CriticalSection lock;
    juce::String message;
    std::atomic<int> messages { 0 }, lastPart { -1 };
};
} // namespace ohpe
