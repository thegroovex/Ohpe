// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioSettingsPage.h"

#include "plugin/Localisation.h"
#include "plugin/MicAccess.h"
#include "ui/Palette.h"

namespace ohpe
{
using i18n::tr;

juce::String AudioSettingsPage::latencyText (int bufferSamples, int extra, double sr)
{
    if (sr <= 0.0) return "-";
    const double ms = 1000.0 * (double) (bufferSamples + extra) / sr;
    return juce::String (ms, 1) + " ms  (" + juce::String (bufferSamples) + " " + tr ("samples") + " @ " + juce::String (sr / 1000.0, 1) + " kHz)";
}

AudioSettingsPage::AudioSettingsPage (juce::AudioDeviceManager& d, OhpeProcessor& p) : dm (d), proc (p)
{
    auto setupLabel = [this] (juce::Label& l, const juce::String& text) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (ui::labelFont (14.0f));
        l.setColour (juce::Label::textColourId, ui::colour::engraving);
        addAndMakeVisible (l);
    };
    setupLabel (typeLabel, tr ("Audio system"));
    setupLabel (deviceLabel, tr ("Output device"));
    setupLabel (inputLabel, tr ("Input device")); // p68
    setupLabel (rateLabel, tr ("Sample rate"));
    setupLabel (bufferLabel, tr ("Buffer size"));
    setupLabel (latencyLabel, tr ("Output latency"));
    setupLabel (latencyValue, "-");
    latencyValue.setColour (juce::Label::textColourId, ui::colour::ink);
    latencyValue.setFont (ui::monoFont (15.0f));
    setupLabel (routingLabel, tr ("Multi-out routing"));
    routingLabel.setFont (ui::brandFont (16.0f));
    routingLabel.setColour (juce::Label::textColourId, ui::colour::ink);
    setupLabel (routingInfo, tr ("Every mixer channel can go to the main output or to its own stereo output."));
    setupLabel (midiLabel, tr ("MIDI inputs"));

    for (auto* b : { &typeBox, &deviceBox, &inputBox, &rateBox, &bufferBox }) addAndMakeVisible (*b);
    inputBox.onChange = [this] { // p68: none by default; macOS asks for the microphone the first time one opens
        if (updating) return;
        const bool none = inputBox.getSelectedId() <= 1;
        const auto name = none ? juce::String() : inputBox.getText();
        auto open = [this, name] {
            applySetup ([this, name] (auto& s) {
                s.inputDeviceName = name;
                s.useDefaultInputChannels = false;
                // p86: a mono device opens its one channel (asking for two of them is fine with every driver
                // tried, but the count is known here - no channel the device does not have is asked for)
                s.inputChannels = name.isEmpty() ? juce::BigInteger() : inputChannelsFor (inputCount (name));
            });
        };
        // p86: ask for the microphone first (macOS) - the device opens when the answer is there, so the first
        // blocks are not silent because the question was still open
        if (! none && mic::access() == mic::Access::notDetermined)
            mic::request ([safe = juce::Component::SafePointer<AudioSettingsPage> (this), open] (bool) { if (safe != nullptr) open(); });
        else
            open();
    };
    typeBox.onChange = [this] {
        if (updating) return;
        dm.setCurrentAudioDeviceType (typeBox.getText(), true);
        enableAllOutputs();
        refreshDeviceLists();
    };
    deviceBox.onChange = [this] {
        if (updating) return;
        applySetup ([this] (auto& s) { s.outputDeviceName = deviceBox.getText(); s.useDefaultOutputChannels = false; s.outputChannels.setRange (0, 64, true); });
    };
    rateBox.onChange = [this] {
        if (updating) return;
        applySetup ([this] (auto& s) { s.sampleRate = (double) rateBox.getSelectedId(); });
    };
    bufferBox.onChange = [this] {
        if (updating) return;
        applySetup ([this] (auto& s) { s.bufferSize = bufferBox.getSelectedId(); });
    };

    // routing rows: one per mixer channel, bound to the host parameter mix.chN.out
    for (int ch = 0; ch < Mixer::numChannels; ++ch)
    {
        auto* l = channelLabels.add (new juce::Label());
        setupLabel (*l, tr ("Channel") + " " + juce::String (ch + 1));
        l->setColour (juce::Label::textColourId, ui::colour::ink);
        auto* box = outputBoxes.add (new juce::ComboBox());
        box->addItem (tr ("Main"), 1);
        for (int o = 1; o <= numAuxOutputs; ++o) box->addItem (tr ("Out") + " " + juce::String (o), o + 1);
        addAndMakeVisible (box);
        if (auto* param = proc.parameter ("mix.ch" + juce::String (ch + 1) + ".out"))
            outputAttachments.add (new juce::ComboBoxParameterAttachment (*param, *box));
        auto* dl = deviceChannelLabels.add (new juce::Label());
        setupLabel (*dl, {});
        dl->setFont (ui::monoFont (13.0f));
        box->onChange = [this] { updateLatency(); };
    }

    dm.addChangeListener (this);
    refreshDeviceLists();
    startTimerHz (2);
    setSize (640, 674); // p68: + input row
}

AudioSettingsPage::~AudioSettingsPage()
{
    dm.removeChangeListener (this);
    outputAttachments.clear();
}

void AudioSettingsPage::enableAllOutputs()
{
    applySetup ([] (auto& s) { s.useDefaultOutputChannels = false; s.outputChannels.setRange (0, 64, true); });
}

void AudioSettingsPage::applySetup (std::function<void (juce::AudioDeviceManager::AudioDeviceSetup&)> change)
{
    auto setup = dm.getAudioDeviceSetup();
    change (setup);
    const auto err = dm.setAudioDeviceSetup (setup, true);
    if (err.isNotEmpty()) juce::Logger::writeToLog ("Ohpe audio settings: " + err);
    refreshDeviceLists();
    if (onSetupChanged) onSetupChanged(); // p86
}

juce::BigInteger AudioSettingsPage::inputChannelsFor (int deviceInputs)
{
    juce::BigInteger chans;
    if (deviceInputs > 0) chans.setRange (0, juce::jmin (2, deviceInputs), true);
    else chans.setRange (0, 2, true); // count unknown: the first two (the driver opens what exists)
    return chans;
}

int AudioSettingsPage::inputCount (const juce::String& name)
{
    // p86: the device's input channel count when it is already the input (no second device instance is created to
    // probe one: some drivers allow only one); unknown otherwise - the driver then opens the channels that exist
    if (auto* dev = dm.getCurrentAudioDevice(); dev != nullptr && dm.getAudioDeviceSetup().inputDeviceName == name)
        return dev->getInputChannelNames().size();
    return 0;
}

void AudioSettingsPage::changeListenerCallback (juce::ChangeBroadcaster*) { refreshDeviceLists(); }

void AudioSettingsPage::refreshDeviceLists()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    typeBox.clear (juce::dontSendNotification);
    int id = 1;
    for (auto* t : dm.getAvailableDeviceTypes()) typeBox.addItem (t->getTypeName(), id++);
    typeBox.setText (dm.getCurrentAudioDeviceType(), juce::dontSendNotification);

    deviceBox.clear (juce::dontSendNotification);
    if (auto* type = dm.getCurrentDeviceTypeObject())
    {
        id = 1;
        for (const auto& name : type->getDeviceNames (false)) deviceBox.addItem (name, id++);
    }
    auto* dev = dm.getCurrentAudioDevice();
    deviceBox.setText (dev != nullptr ? dev->getName() : tr ("No device"), juce::dontSendNotification);

    inputBox.clear (juce::dontSendNotification); // p68
    inputBox.addItem (tr ("No input"), 1);
    if (auto* type = dm.getCurrentDeviceTypeObject())
    {
        id = 2;
        for (const auto& name : type->getDeviceNames (true)) inputBox.addItem (name, id++);
    }
    const auto inName = dm.getAudioDeviceSetup().inputDeviceName;
    if (inName.isEmpty() || dm.getAudioDeviceSetup().inputChannels.isZero()) inputBox.setSelectedId (1, juce::dontSendNotification);
    else inputBox.setText (inName, juce::dontSendNotification);

    rateBox.clear (juce::dontSendNotification);
    bufferBox.clear (juce::dontSendNotification);
    if (dev != nullptr)
    {
        for (auto r : dev->getAvailableSampleRates()) rateBox.addItem (juce::String (r, 0) + " Hz", (int) r);
        rateBox.setSelectedId ((int) dev->getCurrentSampleRate(), juce::dontSendNotification);
        for (auto b : dev->getAvailableBufferSizes())
            bufferBox.addItem (juce::String (b) + " " + tr ("samples") + "  -  " + juce::String (1000.0 * b / juce::jmax (1.0, dev->getCurrentSampleRate()), 1) + " ms", b);
        bufferBox.setSelectedId (dev->getCurrentBufferSizeSamples(), juce::dontSendNotification);
    }

    // MIDI inputs
    for (auto* b : midiInputs) removeChildComponent (b);
    midiInputs.clear();
    for (const auto& in : juce::MidiInput::getAvailableDevices())
    {
        auto* t = midiInputs.add (new juce::ToggleButton (in.name));
        t->setToggleState (dm.isMidiInputDeviceEnabled (in.identifier), juce::dontSendNotification);
        t->onClick = [this, ident = in.identifier, t] { dm.setMidiInputDeviceEnabled (ident, t->getToggleState()); };
        addAndMakeVisible (t);
    }
    updateLatency();
    resized();
}

void AudioSettingsPage::updateLatency()
{
    auto* dev = dm.getCurrentAudioDevice();
    int outChannels = 0;
    juce::StringArray names;
    if (dev != nullptr)
    {
        latencyValue.setText (latencyText (dev->getCurrentBufferSizeSamples(), dev->getOutputLatencyInSamples(), dev->getCurrentSampleRate()),
                              juce::dontSendNotification);
        outChannels = dev->getActiveOutputChannels().countNumberOfSetBits();
        names = dev->getOutputChannelNames();
    }
    else
        latencyValue.setText ("-", juce::dontSendNotification);
    proc.deviceOutputChannels.store (dev != nullptr ? outChannels : 2);

    for (int ch = 0; ch < deviceChannelLabels.size(); ++ch)
    {
        const int out = outputBoxes[ch]->getSelectedId() - 1; // 0 = main
        const int first = out * 2;
        juce::String text;
        if (out > 0 && first + 2 > outChannels)
            text = tr ("not available on this device - plays on Main");
        else
        {
            text = tr ("Device channels") + " " + juce::String (first + 1) + "/" + juce::String (first + 2);
            if (first + 1 < names.size()) text << "  " << names[first] << " / " << names[first + 1];
        }
        deviceChannelLabels[ch]->setText (text, juce::dontSendNotification);
    }
}

void AudioSettingsPage::paint (juce::Graphics& g)
{
    g.fillAll (ui::colour::body);
    g.setColour (ui::colour::bodyShade);
    g.fillRect (20, routingLabel.getY() - 10, getWidth() - 40, 1);
}

void AudioSettingsPage::resized()
{
    auto r = getLocalBounds().reduced (20);
    auto row = [&r] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (6); return x; };
    auto pair = [&] (juce::Label& l, juce::Component& c) {
        auto x = row (28);
        l.setBounds (x.removeFromLeft (170));
        c.setBounds (x);
    };
    pair (typeLabel, typeBox);
    pair (deviceLabel, deviceBox);
    pair (inputLabel, inputBox); // p68
    pair (rateLabel, rateBox);
    pair (bufferLabel, bufferBox);
    pair (latencyLabel, latencyValue);
    if (! midiInputs.isEmpty())
    {
        auto x = row (24);
        midiLabel.setBounds (x.removeFromLeft (170));
        midiInputs[0]->setBounds (x);
        for (int i = 1; i < midiInputs.size(); ++i) midiInputs[i]->setBounds (row (24).withTrimmedLeft (170));
    }
    else
        midiLabel.setBounds ({});
    r.removeFromTop (14);
    routingLabel.setBounds (row (24));
    routingInfo.setBounds (row (20));
    for (int ch = 0; ch < channelLabels.size(); ++ch)
    {
        auto x = row (26);
        channelLabels[ch]->setBounds (x.removeFromLeft (110));
        outputBoxes[ch]->setBounds (x.removeFromLeft (130));
        x.removeFromLeft (10);
        deviceChannelLabels[ch]->setBounds (x);
    }
}
} // namespace ohpe
