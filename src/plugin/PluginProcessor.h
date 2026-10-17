// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "BounceToEngine.h"
#include "OhpeCore.h"
#include "fx/SpaceMode.h"
#include "model/CpuLoad.h"
#include "PresetManager.h"
#include "SampleLoader.h"
#include "UserTables.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace ohpe
{
/**
    The JUCE wrapper around ohpe::Core.

    - Parameters: one host parameter per registry entry, same order, ID = registry ID
      (append-only, so host automation stays valid across versions).
    - Buses: main stereo output + 8 aux stereo outputs (multi-out), MIDI in, and (0.2) one
      stereo audio input for tape recording (off by default). In a plug-in the
      aux outputs are off until the host enables them; in the standalone app all are on.
    - MIDI learn: armMidiLearn(id) -> next CC / note is assigned on the audio thread; a timer
      on the message thread stores it in the map (notifies listeners via ChangeBroadcaster).
*/
class OhpeProcessor final : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster,
                            private juce::Timer
{
public:
    OhpeProcessor();
    ~OhpeProcessor() override;

    // ---- juce::AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ohpe"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** p40: a new instance (first start of the standalone app, a new plug-in instance without host
        state) starts with the factory start sound (PresetManager::loadStartSound) instead of the
        plain parameter defaults; a restored session or host state replaces it. Tests and the
        screenshot tool switch this off to start from the plain defaults. */
    static inline std::atomic<bool> startWithFactorySound { true };

    // ---- Ohpe ----
    Core& core() noexcept { return ohpeCore; }
    /** p35: the CPU meter (share of each block's time the processing took; any thread). */
    const CpuLoad& cpuLoad() const noexcept { return cpuMeter; }
    /** Sample files for the Sampler / Grain engines and drum pads (message thread). */
    SampleLoader& sampleLoader() noexcept { return samples; }
    /** p20: user wavetables / formant tracks (audio to wavetable). Message thread. */
    UserTableManager& userTables() noexcept { return userTableManager; }
    /** p23: bounce to engine (tape / live input -> Grain or Sampler). Message thread. */
    BounceManager& bounces() noexcept { return bounceManager; }
    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }
    juce::RangedAudioParameter* parameter (const juce::String& id) const { return apvts.getParameter (id); }
    juce::RangedAudioParameter* parameter (int registryIndex) const;

    // MIDI learn / map (message thread)
    void armMidiLearn (const juce::String& paramId);
    void cancelMidiLearn();
    juce::String learningParamId() const;
    void clearMidiMapping (const juce::String& paramId);
    void clearAllMidiMappings();
    juce::String describeMidiMapping (const juce::String& paramId) const; ///< "CC 74 · Ch 1" or empty
    bool exportMidiMap (const juce::File& file) const;
    bool importMidiMap (const juce::File& file);
    /** p32 (settings, MIDI learn list): every mapping in order; edit / remove one by its index. */
    std::vector<MidiMapping> midiMappings() const { return ohpeCore.midiMap().mappings(); }
    bool setMidiMapping (int index, const MidiMapping& m);
    bool removeMidiMapping (int index);

    /** 0.3: the preset library (factory + user bank). Message thread. */
    PresetManager& presets() noexcept { return *presetManager; }

    /**
        0.3: edits the core's registry on the message thread and publishes the result to the host:
        pending host values are pulled into the registry first, then `fn` runs, then every changed
        value is sent to the host parameters (automation / UI follow).
    */
    void editRegistry (const std::function<void (Core&)>& fn);

    /**
        0.3: remote controls ("lib.*", "tape.edit.*"): MIDI-mapped presses are counted on the audio
        thread, host / UI changes are detected as rising edges on the timer. The library key has
        no processor-side action: the editor polls libraryToggleRequests().
    */
    int libraryToggleRequests() const noexcept { return libraryToggles.load(); }
    /** p18: presses of "lib.breed" (0), "lib.breed.go" (1), "lib.breed.next" (2) - the editor polls them. */
    int breedRequestCount (int which) const noexcept { return breedRequests[(size_t) juce::jlimit (0, 2, which)].load(); }
    /** Runs a tape edit on the edit track ("tape.edit.track"); lift / cut use loop in .. out,
        drop / splice the play position. Returns false if the tape refused it. */
    bool runTapeEdit (const char* triggerId);
    /** p38: presses of "tape.edit.clearall" (MIDI / host) - the editor polls them and asks before it clears. */
    int tapeClearAllRequestCount() const noexcept { return tapeClearAllRequests.load(); }
    /** p38: the whole tape as an ".ohpetape" file (all tracks, length, loop range, edit track; see tape/TapeFile.h).
        Message thread; refused while recording. `message` is a short translated line for the display. Loading is
        one tape undo step. */
    bool saveTapeFile (const juce::File& file, juce::String& message);
    bool loadTapeFile (const juce::File& file, juce::String& message);
    /** Handles pending remote-control presses (called by the processor's timer; public for tests). */
    void pollRemoteControls();
    /** p41: keeps every part's 3D switch (pN.space.mode) and pN.space.on / space.out in step (message thread; the
        timer calls it, the display after a click so the stage follows at once). */
    void syncSpaceModes();
    /** p24: writes the harmony brain's remembered key into "key.root" / "key.scale" (host notified). Message thread. */
    void takeHarmonyKey();

    /**
        Tape transport (p11): the actions of the transport keys, the same for the keys on the body,
        the buttons on the tape page, the standalone shortcuts and the MIDI triggers "tape.tx.*".
        - PlayStop (the PLAY key): starts / stops the sequencer; with "Play starts tape" (tape.tx.link,
          default on) the tape runs with it. Stopping also ends a recording.
        - TapePlayStop (the tape page): starts / stops the tape; linked, the sequencer runs with it.
        - Stop: stops tape, recording and (linked) the sequencer; when already stopped: to the start.
        - Record: one-touch record. Arms the edit track when no track is armed, switches recording on
          and starts the tape (and the linked sequencer). While recording: punch out (keeps playing).
        - ToStart: tape back to 0:00.
        - Loop: loop on / off; without a loop range it loops the recorded length (or 4 bars).
        - Arm: arms / disarms the edit track.
        Message thread. Returns a short, translated message for the display ("Recording track 2").
    */
    enum class TransportKey { PlayStop = 0, TapePlayStop, Stop, Record, ToStart, Loop, Arm };
    juce::String runTransport (TransportKey action);
    bool tapeRolling() const;       ///< the tape plays (tape.play on)
    bool tapeRecording() const;     ///< the tape really records (play + rec + an armed track)
    bool tapeRecordReady() const;   ///< recording is switched on (tape.rec), maybe not rolling yet
    int armedTrackMask() const;     ///< bit t = track t+1 armed
    /** Transport messages from MIDI triggers, for the editor's display (counter + last text). */
    int transportMessageCount() const noexcept { return transportMessages.load(); }
    juce::String lastTransportMessage() const { const juce::ScopedLock sl (transportLock); return transportMessage; }

    /**
        p39: tempo and metronome. The tempo follows the host (plug-in with a host position) or an incoming MIDI clock
        ("sync.src" = MIDI clock): then it is read-only on the screen. setTempoFromUi() clamps to 20 .. 300 BPM and
        notifies the host; tapTempo() measures the taps (screen TAP key, "metro.tap" via MIDI) and sets the tempo
        from the second tap on. Any thread (the MIDI trigger arrives on the audio thread like the other triggers).
    */
    bool tempoFollowsHost() const noexcept;
    bool hostTempoActive() const noexcept { return hostTempo.load (std::memory_order_relaxed); }
    float currentTempo() const noexcept; ///< the tempo in use (host / clock tempo when it follows them)
    void setTempoFromUi (float bpm);
    void tapTempo();
    int tapCount() const noexcept { return taps.load (std::memory_order_relaxed); }

    /** UI state stored with the session (language, mode, page). */
    juce::var uiState;

    /** Standalone: number of output channels the audio device really has (-1 = plug-in, all buses as the host says). */
    std::atomic<int> deviceOutputChannels { -1 };

    juce::MidiKeyboardState keyboardState;

    /**
        Standalone tempo sync: an external transport (network tempo sync, Link) that drives the core while
        "sync.src" is Link and no host provides a position. Set / cleared on the message thread
        by the standalone app; fill() runs on the audio thread and must be real-time safe.
    */
    struct ExternalTransport
    {
        virtual ~ExternalTransport() = default;
        /** Fills bpm / ppq / playing for the block (hostDriven = true); false = not available now. */
        virtual bool fill (Transport& tr, int numSamples, double sampleRate, double tempoParam) noexcept = 0;
    };
    std::atomic<ExternalTransport*> externalTransport { nullptr };

    /** p32: the standalone app's audio / MIDI devices (nullptr in a plug-in host) and its audio settings window - the
        settings view lists and switches the MIDI inputs with them. Message thread; set by the standalone window. */
    juce::AudioDeviceManager* standaloneDevices = nullptr;
    std::function<void()> openStandaloneAudioSettings;

    static BusesProperties makeBuses();

private:
    void timerCallback() override;
    void captureInput (juce::AudioBuffer<float>& buffer, int numSamples); ///< 0.2: input bus -> core
    void syncHostParametersFromRegistry();
    void pullHostParametersIntoRegistry();

    Core ohpeCore;
    SampleLoader samples { ohpeCore };
    UserTableManager userTableManager { ohpeCore }; ///< p20
    BounceManager bounceManager { ohpeCore };       ///< p23
    juce::AudioProcessorValueTreeState apvts;
    std::vector<std::atomic<float>*> rawValues;
    std::vector<juce::RangedAudioParameter*> params;
    std::unique_ptr<std::atomic<float>[]> lastPushed; ///< plain value last copied host -> registry (audio + message thread)
    // p35: host parameters that changed since the last block (one bit each, set by an APVTS listener after the raw
    // value is stored); processBlock copies only those into the registry, plus one word of 64 per block as a sweep
    struct HostParamListener final : juce::AudioProcessorValueTreeState::Listener
    {
        OhpeProcessor* owner = nullptr;
        int index = 0;
        void parameterChanged (const juce::String&, float) override { owner->markHostChanged (index); }
    };
    void markHostChanged (int index) noexcept
    {
        hostChanged[(size_t) index >> 6].fetch_or (uint64_t (1) << (index & 63), std::memory_order_release);
    }
    void pushHostParameter (size_t i) noexcept;
    std::vector<std::unique_ptr<HostParamListener>> hostListeners;
    std::unique_ptr<std::atomic<uint64_t>[]> hostChanged;
    int hostChangedWords = 0, sweepWord = 0;
    CpuLoad cpuMeter; ///< p35
    std::vector<MidiEvent> midiScratch;
    juce::AudioBuffer<float> scratch;
    juce::AudioBuffer<float> inputScratch; ///< 0.2: input bus copy for tape recording
    int syncSourceParam = -1, tempoParam = -1;

    // 0.3: remote controls
    static constexpr int numTriggers = 24; ///< p11: + tape.tx.play / tplay / stop / rec / loop / arm, p18: + lib.breed*, p23: + bounce.go, p24: + harmony.take / clear, p38: + tape.edit.clear / clearall
    void setPlainFromUi (const char* id, float plain);
    juce::CriticalSection transportLock;
    juce::String transportMessage;
    std::atomic<int> transportMessages { 0 };
    std::array<int, numTriggers> triggerParam {};
    std::array<float, numTriggers> triggerLast {};
    std::array<std::atomic<int>, numTriggers> triggerPresses {};
    std::atomic<int> libraryToggles { 0 };
    std::atomic<int> tapeClearAllRequests { 0 }; ///< p38
    std::array<std::atomic<int>, 3> breedRequests {}; ///< p18
    std::unique_ptr<PresetManager> presetManager;
    // p40: the loaded preset's uid for the session (getStateInformation may run off the message thread)
    juce::SpinLock presetUidLock;
    std::string presetUidCache, presetUidPending;
    std::atomic<bool> presetUidRestore { false };
    void applyPendingPresetUid();
    // p39: tap tempo (own edge detector for "metro.tap": the trigger list stays as it is)
    TapTempo tapper;
    juce::SpinLock tapLock;
    std::atomic<int> taps { 0 };
    std::atomic<bool> hostTempo { false };
    int metroTapParam = -1;
    float metroTapLast = 0.0f;
    spacemode::Link<Core::numParts> spaceModeLink; ///< p41: the 3D switch per part <-> space.on / space.out

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeProcessor)
};
} // namespace ohpe
