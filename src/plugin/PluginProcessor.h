// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "PresetManager.h"
#include "SampleLoader.h"

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
    /** Runs a tape edit on the edit track ("tape.edit.track"); lift / cut use loop in .. out,
        drop / splice the play position. Returns false if the tape refused it. */
    bool runTapeEdit (const char* triggerId);

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
    juce::AudioProcessorValueTreeState apvts;
    std::vector<std::atomic<float>*> rawValues;
    std::vector<juce::RangedAudioParameter*> params;
    std::unique_ptr<std::atomic<float>[]> lastPushed; ///< plain value last copied host -> registry (audio + message thread)
    std::vector<MidiEvent> midiScratch;
    juce::AudioBuffer<float> scratch;
    juce::AudioBuffer<float> inputScratch; ///< 0.2: input bus copy for tape recording
    int syncSourceParam = -1, tempoParam = -1;

    // 0.3: remote controls
    void pollRemoteTriggers();
    static constexpr int numTriggers = 10;
    std::array<int, numTriggers> triggerParam {};
    std::array<float, numTriggers> triggerLast {};
    std::array<std::atomic<int>, numTriggers> triggerPresses {};
    std::atomic<int> libraryToggles { 0 };
    std::unique_ptr<PresetManager> presetManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeProcessor)
};
} // namespace ohpe
