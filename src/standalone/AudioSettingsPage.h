// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"

#include <juce_audio_devices/juce_audio_devices.h>

namespace ohpe
{
/**
    Standalone audio settings: audio system, output device, sample rate, buffer size with the
    resulting latency, MIDI inputs, and the multi-out routing (every mixer channel to Main or
    to one of eight stereo outputs, with the device channels each output lands on).
*/
class AudioSettingsPage final : public juce::Component,
                                private juce::ChangeListener,
                                private juce::Timer
{
public:
    AudioSettingsPage (juce::AudioDeviceManager& dm, OhpeProcessor& p);
    ~AudioSettingsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** p86: called after every change the page made to the device setup (the app saves it at once, so a chosen
        input survives a crash or a forced quit, not only a clean exit). */
    std::function<void()> onSetupChanged;

    /** p86: the input channels to open on a device with `deviceInputs` inputs: the first two (a mono device: its
        one channel - the player gives it to both sides). Empty for none. */
    static juce::BigInteger inputChannelsFor (int deviceInputs);

    /** Latency text "5.3 ms (256 samples @ 48 kHz)" for a device configuration. */
    static juce::String latencyText (int bufferSamples, int extraOutputLatency, double sampleRate);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override { updateLatency(); }
    void refreshDeviceLists();
    void updateLatency();
    void applySetup (std::function<void (juce::AudioDeviceManager::AudioDeviceSetup&)> change);
    int inputCount (const juce::String& deviceName); ///< p86: 0 = unknown
    void enableAllOutputs();

    juce::AudioDeviceManager& dm;
    OhpeProcessor& proc;
    juce::Label inputLabel; ///< p68: audio input (Voice Control, tape, bounce)
    juce::ComboBox inputBox;
    juce::Label typeLabel, deviceLabel, rateLabel, bufferLabel, latencyLabel, latencyValue, routingLabel, routingInfo, midiLabel;
    juce::ComboBox typeBox, deviceBox, rateBox, bufferBox;
    juce::OwnedArray<juce::ToggleButton> midiInputs;
    juce::OwnedArray<juce::Label> channelLabels, deviceChannelLabels;
    juce::OwnedArray<juce::ComboBox> outputBoxes;
    juce::OwnedArray<juce::ComboBoxParameterAttachment> outputAttachments;
    bool updating = false;
};
} // namespace ohpe
