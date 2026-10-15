// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "LinkSync.h"
#include "plugin/PluginProcessor.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace ohpe
{
/**
    Standalone "Sync & MIDI" page: sync source (internal clock, MIDI clock, Link), clock
    smoothing, MIDI output device, sending MIDI clock and sequencer notes, and the live
    tempo / Link session status.
*/
class SyncSettingsPage final : public juce::Component,
                               private juce::Timer
{
public:
    SyncSettingsPage (juce::AudioDeviceManager& dm, OhpeProcessor& p, LinkSync& link, std::function<void()> midiOutputChanged);
    ~SyncSettingsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshOutputs();

    juce::AudioDeviceManager& dm;
    OhpeProcessor& proc;
    LinkSync& link;
    std::function<void()> onMidiOutputChanged;

    juce::Label title, sourceLabel, smoothLabel, outputLabel, statusLabel, statusValue;
    juce::ComboBox sourceBox, outputBox;
    juce::Slider smoothSlider;
    juce::ToggleButton clockOutButton, notesOutButton;
    std::unique_ptr<juce::ComboBoxParameterAttachment> sourceAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> smoothAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> clockOutAttachment, notesOutAttachment;
    juce::Array<juce::MidiDeviceInfo> outputs;
};
} // namespace ohpe
