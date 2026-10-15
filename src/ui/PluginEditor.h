// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Display.h"
#include "OhpeLookAndFeel.h"
#include "Widgets.h"
#include "plugin/Localisation.h"

#include <array>
#include <memory>

namespace ohpe
{
/**
    The Ohpe editor: engine keys (left), display (centre), mode keys (right), four encoders,
    play / octave keys and a two-octave keyboard. Designed on a 1200 x 800 grid and scaled
    uniformly (resizable, fixed aspect ratio).
*/
class OhpeEditor final : public juce::AudioProcessorEditor,
                         private juce::Timer,
                         private juce::ChangeListener
{
public:
    explicit OhpeEditor (OhpeProcessor&);
    ~OhpeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1200, designHeight = 800;

    // used by tests and the screenshot tool
    void selectMode (ui::Mode m);
    void selectEngine (EngineType t);
    void setLanguage (i18n::Language lang);
    ui::Mode currentMode() const noexcept { return mode; }
    ui::Encoder& encoder (int i) { return *encoders[(size_t) i]; }
    void advanceAnimation (float seconds) { display.tick (seconds); }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebindEncoders();
    void refreshKeys();
    void showMidiMenu();
    void saveUiState();
    void loadUiState();
    void setParamPlain (const juce::String& id, float plain);
    juce::Rectangle<int> scaled (float x, float y, float w, float h) const;

    OhpeProcessor& proc;
    ui::OhpeLookAndFeel lnf;
    ui::Display display;
    std::array<std::unique_ptr<ui::Encoder>, 4> encoders;
    std::array<std::unique_ptr<ui::PictoKey>, numEngineTypes> engineKeys;
    std::array<std::unique_ptr<ui::PictoKey>, ui::numModes> modeKeys;
    ui::PictoKey playKey { ui::Picto::Play, "Play" };
    ui::PictoKey octDownKey { ui::Picto::OctaveDown, "Octave" };
    ui::PictoKey octUpKey { ui::Picto::OctaveUp, "Octave" };
    ui::PictoKey midiKey { ui::Picto::Midi, "MIDI" };
    juce::TextButton langEn { "EN" }, langDe { "DE" };
    ui::Keyboard keyboard;

    ui::Mode mode = ui::Mode::Synth;
    std::array<int, ui::numModes> pageOf {};
    int mixerChannel = 0, stepPage = 0;
    EngineType shownEngine = EngineType::Analog;
    juce::Image bodyTexture;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastLearning;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeEditor)
};
} // namespace ohpe
