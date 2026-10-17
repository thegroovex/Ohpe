// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "BounceToEngine.h"
#include "OhpeCore.h"
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

    // ---- Ohpe ----
    Core& core() noexcept { return ohpeCore; }
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
    /** Handles pending remote-control presses (called by the processor's timer; public for tests). */
    void pollRemoteControls();

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
    std::vector<MidiEvent> midiScratch;
    juce::AudioBuffer<float> scratch;
    juce::AudioBuffer<float> inputScratch; ///< 0.2: input bus copy for tape recording
    int syncSourceParam = -1, tempoParam = -1;

    // 0.3: remote controls
    static constexpr int numTriggers = 20; ///< p11: + tape.tx.play / tplay / stop / rec / loop / arm, p18: + lib.breed*, p23: + bounce.go
    void setPlainFromUi (const char* id, float plain);
    juce::CriticalSection transportLock;
    juce::String transportMessage;
    std::atomic<int> transportMessages { 0 };
    std::array<int, numTriggers> triggerParam {};
    std::array<float, numTriggers> triggerLast {};
    std::array<std::atomic<int>, numTriggers> triggerPresses {};
    std::atomic<int> libraryToggles { 0 };
    std::array<std::atomic<int>, 3> breedRequests {}; ///< p18
    std::unique_ptr<PresetManager> presetManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeProcessor)
};
} // namespace ohpe
