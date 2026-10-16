// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Display.h"
#include "LibraryBrowser.h"
#include "OhpeLookAndFeel.h"
#include "ProView.h"
#include "Widgets.h"
#include "plugin/Localisation.h"

#include <algorithm>
#include <array>
#include <memory>

namespace ohpe
{
namespace ui
{
/** Part selector 1-4 (header). */
class PartSelector : public juce::Component, public juce::TooltipClient
{
public:
    void setPart (int p) { part = p; repaint(); }
    /** 1.0: per part 0 = no 3D, 1 = binaural 3D (headphones pictogram lit), 2 = 3D for speakers; `t` drives the pulse. */
    void setSpatial (const std::array<int, 4>& states, float t)
    {
        const bool pulse = std::find (states.begin(), states.end(), 1) != states.end();
        if (states != spatial || pulse) { spatial = states; time = t; repaint(); }
    }
    juce::String getTooltip() override;
    std::function<void (int)> onSelect;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    juce::Rectangle<float> cell (int i) const;

private:
    int part = 0;
    std::array<int, 4> spatial {};
    float time = 0.0f;
};

/** Preset name with previous / next (header); a click on the name opens the library. */
class PresetStrip : public juce::Component
{
public:
    void setName (const juce::String& n) { if (n != name) { name = n; repaint(); } }
    std::function<void (int delta)> onStep;
    std::function<void()> onOpen;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> arrow (bool right) const;
    juce::String name;
};
} // namespace ui

/**
    The Ohpe editor: engine keys (left), display (centre), mode keys (right), four encoders,
    play / octave keys and a two-octave keyboard. Header (0.3): part selector, preset strip,
    LIBRARY and PRO keys. Designed on a 1200 x 800 grid and scaled uniformly (resizable, fixed
    aspect ratio).
*/
class OhpeEditor final : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer,
                         private juce::ChangeListener
{
public:
    explicit OhpeEditor (OhpeProcessor&);
    ~OhpeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // FileDragAndDropTarget: audio files -> sampler / grain of the edit part, or a drum pad;
    // .ohpepreset files -> load
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    static constexpr int designWidth = 1200, designHeight = 800;

    // used by tests and the screenshot tool
    void selectMode (ui::Mode m);
    void selectEngine (EngineType t);
    void selectPart (int part);                 ///< 0..3
    void selectSeqView (ui::SeqView v);
    void selectView (int v);          ///< 0.7: view tab of the LFO / Effects mode
    void selectModRouting (int r);    ///< 0.7
    void selectFxSlot (int s);        ///< 0.7
    void moveFxSlot (int from, int to);
    void selectTapeTrack (int track);
    void selectDrumPad (int pad, bool audition = false);
    void selectLockStep (int step);             ///< -1 = leave lock editing
    void setLanguage (i18n::Language lang);
    void showLibrary (bool show);
    void showProView (bool show);
    bool libraryVisible() const { return library.isVisible(); }
    bool proViewVisible() const { return proView.isVisible(); }
    ui::Mode currentMode() const noexcept { return mode; }
    int currentPart() const noexcept { return ctx.part; }
    int lockStep() const noexcept { return lockStepIndex; }
    ui::Encoder& encoder (int i) { return *encoders[(size_t) i]; }
    ui::Display& displayComponent() { return display; }
    ui::ProView& proViewComponent() { return proView; }
    ui::LibraryBrowser& libraryComponent() { return library; }
    void advanceAnimation (float seconds);
    void softKey (int id) { onSoftKey (id); }
    bool loadAudioFile (const juce::File& f, int pad = -1);

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebindEncoders();
    void refreshKeys();
    void refreshSoftKeys();
    void refreshProView();
    void showMidiMenu();
    void saveUiState();
    void loadUiState();
    void setParamPlain (const juce::String& id, float plain);
    void onSoftKey (int id);
    void exportBounce();
    void chooseSampleFile();
    ui::VirtualPtr makeLockControl (const juce::String& paramId);
    ui::VirtualPtr makeStepField (int field);
    void updateVirtualFlags();
    int& currentPage();
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
    ui::PictoKey libraryKey { ui::Picto::Library, "Library" };
    ui::PictoKey proKey { ui::Picto::Pro, "Pro" };
    ui::PartSelector partSelector;
    float spatialTime = 0.0f;
    juce::TooltipWindow tooltips { this, 600 }; ///< 1.0: tooltips (3D badge, part selector)
    ui::PresetStrip presetStrip;
    juce::TextButton langEn { "EN" }, langDe { "DE" };
    ui::Keyboard keyboard;
    ui::ProView proView;
    ui::LibraryBrowser library;

    ui::Mode mode = ui::Mode::Synth;
    ui::PageContext ctx;
    std::array<int, ui::numModes> pageOf {};
    std::array<int, ui::numSeqViews> seqPageOf {};
    int stepPage = 0, lockStepIndex = -1;
    EngineType shownEngine = EngineType::Analog;
    juce::Image bodyTexture;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastLearning;
    int libraryRequestsSeen = 0;
    int lastKeyboardChannel = 1, lastFxType = -1;
    int modPage = 0, rackPage = 0, lastModSignature = -1; ///< 0.7
    std::array<ui::VirtualPtr, 4> virtuals {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeEditor)
};
} // namespace ohpe
