// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "wavetable/AudioAnalysis.h"
#include "wavetable/InputCapture.h"
#include "wavetable/UserTableState.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>
#include <optional>

namespace ohpe
{
/**
    p20 (issue #16): audio to wavetable. Imports user material into the Wavetable engine (user
    wavetable) and the Vocal engine (user formant tracks) of a part:

    * audio files (WAV, AIFF, FLAC, Ogg) are analysed (pitch, period-synchronous frames, formants),
    * single-cycle wavetable sets (2048 samples per frame or a "clm " / cue frame size) are read as
      they are, Ohpe tables (.ohpewt) are loaded,
    * a tape track or a few seconds of the live input can be analysed too.

    Analysis and file I/O run on a worker thread, never on the audio thread; the finished table is
    saved into the user wavetable library (<user application data>/groovex/Ohpe/Wavetables,
    environment variable OHPE_WAVETABLE_DIR overrides it) and published to the engine on the
    message thread. The references (path, library file name, embedded copy for tables up to 64
    frames) live in the state section "usertables" and in part presets. Message thread unless noted.
*/
class UserTableManager : private juce::Thread
{
public:
    using Target = usertable::Target;

    explicit UserTableManager (Core& core);
    ~UserTableManager() override;

    static juce::File defaultLibraryFolder();
    void setLibraryFolder (const juce::File& folder) { library = folder; }
    const juce::File& libraryFolder() const noexcept { return library; }
    /** The *.ohpewt files of the library, sorted by name. */
    juce::Array<juce::File> libraryFiles() const;

    /** Analysis settings for the next import (frames 8 .. 256, smoothing 0 .. 1). */
    int frameCount = 64;
    float smoothing = 0.0f;

    /** File patterns for the file chooser (audio files + .ohpewt). */
    juce::String filePatterns() const;
    static bool isImportableFile (const juce::File& f);

    /** Starts an import into part `part` (0-based). False when busy or the file type is unknown. */
    bool importFile (const juce::File& file, int part, Target target);
    /** Analyses tape track `track` (0-based, the recorded part up to 30 s). False when busy or the track is empty. */
    bool importTapeTrack (int track, int part, Target target);
    /** Records `seconds` of the live input, then analyses it. False when busy. */
    bool recordInput (double seconds, int part, Target target);
    void cancel();
    /** Removes the part's user material (the toggle stays: the built-in material plays). */
    void clear (int part, Target target);
    /** Loads an .ohpewt file (any thread but the audio thread); nullptr + error when it is not valid. */
    usertable::UserTablePtr loadTableFile (const juce::File& file, juce::String* error = nullptr) const;

    struct Status
    {
        bool busy = false;      ///< analysing / loading / recording
        bool recording = false; ///< recording the live input
        float progress = 0.0f;  ///< 0 .. 1
        int part = 0;
        Target target = Target::Wavetable;
    };
    Status status() const;

    /** Messages for the display (English, translated by the UI; "Wavetable ready" ...), counted. */
    int messageCount() const noexcept { return messages.load(); }
    juce::String lastMessage() const { const juce::ScopedLock sl (lock); return message; }
    /** Extra text of the last message that is not translated (a table name, frames, pitch). */
    juce::String lastDetail() const { const juce::ScopedLock sl (lock); return detail; }

    /** Called after a table was published (the processor switches "pN.usertable.*" on). */
    std::function<void (int part, Target target)> onPublished;

    /** Audio thread: the live input capture (the processor pushes the input bus into it). */
    usertable::InputCapture& capture() noexcept { return inputCapture; }

    /** Periodic message-thread work: finished imports, the end of an input recording. */
    void timerTick();

    /** The current references into the core's "usertables" section (before saving). */
    void storeInState();
    /** Reloads the tables named in the core's "usertables" section (after loading a state). */
    usertable::RestoreResult restoreFromState();
    /** Part presets: the part's references (empty = none) / loading them (empty = clear the part). */
    std::string partJson (int part);
    usertable::RestoreResult applyPartJson (int part, const std::string& json);

    /** Content packs (the wavetables/ folder of each Content/<pack>/, .wav files, read-only): where they are (default: the updater's folder). */
    void setContentFolder (const juce::File& folder) { contentDir = folder; }
    const juce::File& contentFolder() const noexcept { return contentDir; }
    /** Loads the pack table a reference "<pack>/<table>" points at (a single-cycle set, frames of 2048 samples,
        read like an imported set). nullptr + `state` PackMissing / ItemMissing when it is not there. */
    usertable::UserTablePtr loadPackTable (const update::ContentRef& ref, update::RefState& state) const;
    /** Packs the last restore / part preset referred to that are not installed ("content pack missing"), each once. */
    const juce::StringArray& missingPacks() const noexcept { return missing; }

private:
    void run() override;
    struct Job
    {
        juce::File file;
        std::vector<float> audio;
        double rate = 48000.0;
        juce::String name;
        int part = 0;
        Target target = Target::Wavetable;
        usertable::AnalysisOptions options;
    };
    bool start (Job job);
    void process (Job& job);
    void finish (bool ok, const juce::String& msg, const juce::String& det, usertable::UserTablePtr t);
    void post (const juce::String& msg, const juce::String& det);
    juce::File saveToLibrary (const usertable::UserTable& t) const;
    usertable::Resolver resolver() const;
    usertable::ContentResolver contentResolver() const;
    void report (const usertable::RestoreResult& r);

    Core& core;
    juce::File library, contentDir;
    juce::StringArray missing;
    juce::AudioFormatManager formats; ///< used by the worker only
    usertable::InputCapture inputCapture;
    juce::CriticalSection lock;
    juce::WaitableEvent wake;
    std::optional<Job> pending;
    std::atomic<bool> busy { false }, cancelFlag { false };
    std::atomic<float> progressValue { 0.0f };
    bool recordingInput = false;
    Job recordJob;
    // result (worker -> message thread)
    bool resultReady = false;
    usertable::UserTablePtr result;
    int resultPart = 0;
    Target resultTarget = Target::Wavetable;
    Target busyTarget = Target::Wavetable;
    int busyPart = 0;
    juce::String message, detail;
    std::atomic<int> messages { 0 };
};
} // namespace ohpe
