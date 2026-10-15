// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "SampleLoader.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace ohpe
{
/**
    The JUCE wrapper around ohpe::Core.

    - Parameters: one host parameter per registry entry, same order, ID = registry ID
      (append-only, so host automation stays valid across versions).
    - Buses: main stereo output + 8 aux stereo outputs (multi-out), MIDI in. In a plug-in the
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

    /** UI state stored with the session (language, mode, page). */
    juce::var uiState;

    /** Standalone: number of output channels the audio device really has (-1 = plug-in, all buses as the host says). */
    std::atomic<int> deviceOutputChannels { -1 };

    juce::MidiKeyboardState keyboardState;

    static BusesProperties makeBuses();

private:
    void timerCallback() override;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeProcessor)
};
} // namespace ohpe
