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

    // p11: tape transport
    /** Runs a transport action (keys, tape page, shortcuts) and shows its message on the display. */
    void transport (OhpeProcessor::TransportKey action);
    /** Standalone shortcuts: Space = play / stop, R = record, Enter / Home = to the start, L = loop,
        A = arm the edit track. Returns false for other keys and in plug-in hosts (the host owns them). */
    bool handleShortcut (const juce::KeyPress& key);
    /** The library or the pro view lies on the plate: the plate's controls are hidden. */
    bool overlayOpen() const { return library.isVisible() || proView.isVisible(); }
    ui::PictoKey& recordKey() { return recKey; }
    ui::PictoKey& playTransportKey() { return playKey; }
    juce::String lastFlash() const { return lastFlashText; }
    // p14: keyboard row and header
    ui::Wheel& pitchWheelComponent() { return pitchWheel; }
    ui::Wheel& modWheelComponent() { return modWheel; }
    ui::SlideSwitch& glideModeSwitch() { return glideSwitch; }
    ui::MiniKnob& glideKnobComponent() { return glideKnob; }
    ui::Keyboard& keyboardComponent() { return keyboard; }
    ui::PictoKey& midiKeyComponent() { return midiKey; }
    // p14 rev 3: the bottom panels - transport keys with light bars, the readouts, the octave keys
    ui::PictoKey& stopTransportKey() { return stopKey; }
    ui::PictoKey& toStartTransportKey() { return toStartKey; }
    ui::PictoKey& loopTransportKey() { return loopKey; }
    ui::PictoKey& octaveDownKey() { return octDownKey; }
    ui::PictoKey& octaveUpKey() { return octUpKey; }
    ui::PictoKey& engineKey (EngineType t) { return *engineKeys[(size_t) t]; }
    ui::PictoKey& modeKey (ui::Mode m) { return *modeKeys[(size_t) m]; }
    ui::Readout& tapeReadoutComponent() { return tapeReadout; }
    ui::Readout& keysReadoutComponent() { return keysReadout; }
    /** RANDOM: new values for the sound parameters of the edit part's engine (its Synth pages). */
    void randomiseSound (juce::int64 seed = 0);
    /** SAVE: opens the library with the save panel for the edit part. */
    void openSave();
    /** The design-unit rectangles of the plate's panels and legends (layout tests). */
    static juce::Rectangle<float> designArea (const char* name);
    void softKey (int id) { onSoftKey (id); }
    bool loadAudioFile (const juce::File& f, int pad = -1);

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebindEncoders();
    void renderBackground (float physicalScale);
    void bindPartControls(); ///< p14: glide switch / knob follow the edit part
    void refreshKeys();
    void refreshTransportKeys(); ///< p11: key states (p14: REC / PLAY breathe while running)
    void refreshLeds();          ///< p14: engine / mode key LEDs and their tooltips
    void updateOverlayVisibility(); ///< p11: hides every plate control under the library / pro view
    std::vector<juce::Component*> plateComponents();
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
    // p11: tape transport keys (with Play: the TRANSPORT cluster)
    ui::PictoKey recKey { ui::Picto::Record, "Rec" };
    ui::PictoKey stopKey { ui::Picto::Stop, "Stop" };
    ui::PictoKey toStartKey { ui::Picto::ToStart, "To start" };
    ui::PictoKey loopKey { ui::Picto::Loop, "Loop" };
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    juce::String lastFlashText;
    int transportMessagesSeen = 0;
    float blinkClock = 0.0f;
    std::array<unsigned, numEngineTypes> engineTipParts {}; ///< p14: part sets the engine tooltips were written for
    bool engineTipsValid = false;
    ui::PictoKey octDownKey { ui::Picto::OctaveDown, "Octave" };
    ui::PictoKey octUpKey { ui::Picto::OctaveUp, "Octave" };
    ui::PictoKey midiKey { ui::Picto::Midi, "MIDI" };
    ui::PictoKey libraryKey { ui::Picto::Library, "Library" };
    ui::PictoKey proKey { ui::Picto::Pro, "Pro" };
    // p14: header round keys, performance wheels (left of the keyboard), glide (right of it)
    ui::PictoKey randomKey { ui::Picto::Random, "Random" };
    ui::PictoKey saveKey { ui::Picto::Save, "Save" };
    ui::Wheel pitchWheel, modWheel;
    ui::SlideSwitch glideSwitch;
    ui::MiniKnob glideKnob;
    ui::Readout tapeReadout, keysReadout; ///< p14 rev 3
    juce::Image background; ///< p14: the static housing, panels and recesses (rendered once per size)
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
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastLearning;
    int libraryRequestsSeen = 0;
    int lastKeyboardChannel = 1, lastFxType = -1;
    int modPage = 0, rackPage = 0, lastModSignature = -1; ///< 0.7
    std::array<ui::VirtualPtr, 4> virtuals {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeEditor)
};
} // namespace ohpe
