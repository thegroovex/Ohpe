// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SyncSettingsPage.h"

#include "plugin/Localisation.h"
#include "ui/Palette.h"

namespace ohpe
{
using i18n::tr;

SyncSettingsPage::SyncSettingsPage (juce::AudioDeviceManager& d, OhpeProcessor& p, LinkSync& l, std::function<void()> midiOutputChanged)
    : dm (d), proc (p), link (l), onMidiOutputChanged (std::move (midiOutputChanged))
{
    auto setupLabel = [this] (juce::Label& lb, const juce::String& text) {
        lb.setText (text, juce::dontSendNotification);
        lb.setFont (ui::labelFont (14.0f));
        lb.setColour (juce::Label::textColourId, ui::colour::engraving);
        addAndMakeVisible (lb);
    };
    setupLabel (title, tr ("Sync & MIDI"));
    title.setFont (ui::brandFont (16.0f));
    title.setColour (juce::Label::textColourId, ui::colour::ink);
    setupLabel (sourceLabel, tr ("Sync source"));
    setupLabel (smoothLabel, tr ("Clock smoothing"));
    setupLabel (outputLabel, tr ("MIDI output"));
    setupLabel (statusLabel, tr ("Tempo"));
    setupLabel (statusValue, "-");
    statusValue.setFont (ui::monoFont (14.0f));
    statusValue.setColour (juce::Label::textColourId, ui::colour::ink);

    sourceBox.addItem (tr ("Internal clock"), 1);
    sourceBox.addItem (tr ("MIDI clock"), 2);
    sourceBox.addItem (LinkSync::available() ? juce::String ("Link") : tr ("Link (not built)"), 3);
    sourceBox.setItemEnabled (3, LinkSync::available());
    addAndMakeVisible (sourceBox);
    if (auto* prm = proc.parameter (Core::Ids::syncSource))
        sourceAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*prm, sourceBox);

    smoothSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    smoothSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (smoothSlider);
    if (auto* prm = proc.parameter (Core::Ids::syncSmoothing))
        smoothAttachment = std::make_unique<juce::SliderParameterAttachment> (*prm, smoothSlider);

    clockOutButton.setButtonText (tr ("Send MIDI clock"));
    notesOutButton.setButtonText (tr ("Send sequencer notes"));
    for (auto* b : { &clockOutButton, &notesOutButton }) addAndMakeVisible (*b);
    if (auto* prm = proc.parameter (Core::Ids::clockOut))
        clockOutAttachment = std::make_unique<juce::ButtonParameterAttachment> (*prm, clockOutButton);
    if (auto* prm = proc.parameter (Core::Ids::notesOut))
        notesOutAttachment = std::make_unique<juce::ButtonParameterAttachment> (*prm, notesOutButton);

    addAndMakeVisible (outputBox);
    outputBox.onChange = [this] {
        const int i = outputBox.getSelectedId() - 2;
        dm.setDefaultMidiOutputDevice (i >= 0 && i < outputs.size() ? outputs[i].identifier : juce::String());
        if (onMidiOutputChanged) onMidiOutputChanged();
    };
    refreshOutputs();

    setSize (520, 300);
    startTimerHz (4);
}

SyncSettingsPage::~SyncSettingsPage() { stopTimer(); }

void SyncSettingsPage::refreshOutputs()
{
    outputs = juce::MidiOutput::getAvailableDevices();
    outputBox.clear (juce::dontSendNotification);
    outputBox.addItem (tr ("None"), 1);
    int selected = 1;
    for (int i = 0; i < outputs.size(); ++i)
    {
        outputBox.addItem (outputs[i].name, i + 2);
        if (outputs[i].identifier == dm.getDefaultMidiOutputIdentifier()) selected = i + 2;
    }
    outputBox.setSelectedId (selected, juce::dontSendNotification);
}

void SyncSettingsPage::timerCallback()
{
    auto& c = proc.core();
    juce::String s = juce::String (c.transportBpm(), 1) + " BPM";
    s << "  " << (c.transportPlaying() ? tr ("running") : tr ("stopped"));
    if (link.isEnabled())
        s << "  |  Link: " << link.numPeers() << " " << tr ("peers");
    statusValue.setText (s, juce::dontSendNotification);
}

void SyncSettingsPage::paint (juce::Graphics& g) { g.fillAll (ui::colour::body); }

void SyncSettingsPage::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto row = [&r] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (8); return x; };
    title.setBounds (row (24));
    auto line = [&] (juce::Label& lb, juce::Component& c) {
        auto x = row (26);
        lb.setBounds (x.removeFromLeft (170));
        c.setBounds (x);
    };
    line (sourceLabel, sourceBox);
    line (smoothLabel, smoothSlider);
    line (outputLabel, outputBox);
    clockOutButton.setBounds (row (24).withTrimmedLeft (170));
    notesOutButton.setBounds (row (24).withTrimmedLeft (170));
    line (statusLabel, statusValue);
}
} // namespace ohpe
