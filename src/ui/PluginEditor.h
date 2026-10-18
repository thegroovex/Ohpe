// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "BreedView.h"
#include "ConfirmDialog.h"
#include "Display.h"
#include "GridController.h"
#include "GridKeyboard.h"
#include "LibraryBrowser.h"
#include "OhpeLookAndFeel.h"
#include "ProView.h"
#include "SettingsView.h"
#include "PackManagerView.h"
#include "UpdateView.h"
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
/** p27: the version legend above the wordmark ("V0.6.1", "UPDATE 0.7.0" in yellow when one is offered); a click on it
    or on the wordmark opens the update view. */
class VersionBadge : public juce::Component, public juce::TooltipClient
{
public:
    void setTexts (const juce::String& version, const juce::String& update)
    {
        if (version != versionText || update != updateText) { versionText = version; updateText = update; repaint(); }
    }
    bool showsUpdate() const { return updateText.isNotEmpty(); }
    std::function<void()> onClick;
    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String versionText, updateText;
};
/**
    p35: the CPU meter on the legend line of the mode column, right-aligned ("CPU 12 %"): the average share of each
    audio block's time the processing takes (the slowest block in the tooltip). Engraved like the legends (engraving / alertInk follow the light and night skins); red from
    80 % average or a block over 100 % (the audio may break up). Display only - not a parameter.
*/
class CpuMeter : public juce::Component, public juce::TooltipClient
{
public:
    /** Loads as fractions (1 = all of a block's time). Repaints only when the shown whole percent changes. */
    void setLoad (float average, float peak);
    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    int shownPercent() const noexcept { return avgPercent; }

private:
    int avgPercent = -1, peakPercent = -1;
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
    void paintOverChildren (juce::Graphics&) override; ///< p32: display brightness
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
    /** p18: the breeding view (RANDOM held / right-clicked, the library's Breed button, MIDI "lib.breed").
        `libraryEntry` >= 0: that preset becomes parent B (A = the current sound) and a brood is bred at once. */
    void showBreeding (bool show, int libraryEntry = -1);
    bool breedingVisible() const { return breedView.isVisible(); }
    ui::BreedView& breedingComponent() { return breedView; }
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
    bool overlayOpen() const
    {
        return library.isVisible() || proView.isVisible() || breedView.isVisible() || updateView.isVisible() || settingsView.isVisible();
    }
    /** p27: the update view (a click on the version legend / wordmark). */
    void showUpdates (bool show);
    bool updatesVisible() const { return updateView.isVisible(); }
    ui::UpdateView& updateComponent() { return updateView; }
    ui::VersionBadge& versionBadgeComponent() { return versionBadge; }
    ui::CpuMeter& cpuMeterComponent() { return cpuMeter; } ///< p35
    Updater& updaterInstance() { return *updater; }
    /** p32: the settings view (gear key in the header, the wordmark opens its About page). `page` -1 = the page shown last. */
    void showSettings (bool show, int page = -1);
    bool settingsVisible() const { return settingsView.isVisible(); }
    ui::SettingsView& settingsComponent() { return settingsView; }
    ui::PackManagerView& packComponent() { return packView; }
    /** Content packs: the settings page of the pack manager (an extra page after About). */
    int packPage() const { return ui::SettingsView::numPages; }
    /** Installs a pack file (picked or dropped); the result is flashed on the display. */
    void installPack (const juce::File& pack);
    ui::PictoKey& settingsKeyComponent() { return settingsKey; }
    ui::HelpOverlay& helpOverlayComponent() { return helpOverlay; }
    UserSettingsStore& userSettingsStore() { return *userSettings; }
    /** p32: applies the user settings (language, skin, tooltips, help overlay, brightness, animations, window size). */
    void applyUserSettings();
    bool tooltipsEnabled() const noexcept { return tooltipsOn; }
    float animationRate() const noexcept { return animRate; }
    ui::PictoKey& recordKey() { return recKey; }
    ui::PictoKey& playTransportKey() { return playKey; }
    juce::String lastFlash() const { return lastFlashText; }
    // p14: keyboard row and header
    ui::Wheel& pitchWheelComponent() { return pitchWheel; }
    ui::Wheel& modWheelComponent() { return modWheel; }
    ui::SlideSwitch& glideModeSwitch() { return glideSwitch; }
    ui::MiniKnob& glideKnobComponent() { return glideKnob; }
    ui::LifeBar& lifeBarComponent() { return lifeBar; } // p15
    ui::Keyboard& keyboardComponent() { return keyboard; }
    ui::PictoKey& midiKeyComponent() { return midiKey; }
    // p24: the isomorphic grid keyboard (swapped in for the piano keys by the GRID key between OCTAVE + and MIDI)
    ui::GridKeyboard& gridKeyboard() { return grid; }
    ui::PictoKey& gridKeyComponent() { return gridKey; }
    void showGridKeyboard (bool show);
    bool gridKeyboardShown() const noexcept { return grid.isVisible(); }
    /** p46: the Grid-Controller (Profil 1) of the standalone app (nullptr in plug-ins and while no profile was ever
        chosen). gridControllerForTests() creates it in any build (tests, screenshots: a mock device). */
    ui::GridController* gridController() noexcept { return gridCtl.get(); }
    ui::GridController& gridControllerForTests();
    /** p46: what the encoders of the controller and the screen show: the page of the shown mode. */
    int shownPage() { return currentPage(); }
    ui::PageContext pageContext() const noexcept { return ctx; }
    // p14 rev 3: the bottom panels - transport keys with light bars, the readouts, the octave keys
    ui::PictoKey& stopTransportKey() { return stopKey; }
    ui::PictoKey& toStartTransportKey() { return toStartKey; }
    ui::PictoKey& loopTransportKey() { return loopKey; }
    ui::PictoKey& octaveDownKey() { return octDownKey; }
    ui::PictoKey& octaveUpKey() { return octUpKey; }
    ui::PictoKey& engineKey (EngineType t) { return *engineKeys[(size_t) t]; }
    ui::PictoKey& modeKey (ui::Mode m) { return *modeKeys[(size_t) m]; }
    ui::Readout& tapeReadoutComponent() { return tapeReadout; }
    ui::TempoReadout& tempoPanelComponent() { return tempoPanel; } ///< p39 (tests / screenshots)
    ui::ClickStrip& clickStripComponent() { return clickStrip; }   ///< p39
    ui::Readout& keysReadoutComponent() { return keysReadout; }
    /** RANDOM: new values for the sound parameters of the edit part's engine (its Synth pages). */
    void randomiseSound (juce::int64 seed = 0);
    /** SAVE: opens the library with the save panel for the edit part. */
    void openSave();
    /** The design-unit rectangles of the plate's panels and legends (layout tests). */
    static juce::Rectangle<float> designArea (const char* name);
    void softKey (int id) { onSoftKey (id); }
    ui::ConfirmDialog& confirmDialogComponent() { return confirmDialog; } ///< p38
    /** p38: loads a tape file the way the LOAD TAPE key does after the file dialog (asks first over material). */
    void openTapeFile (const juce::File& f) { loadTapeFile (f); }
    /** p42: the loop keys of the tape page instead of the edit keys (the loop readout toggles them). */
    void showTapeLoopKeys (bool show);
    bool tapeLoopKeysShown() const noexcept { return tapeLoopKeys; }
    /** p42 tests: the loop keys act as with Shift held (off the grid). */
    void setLoopKeysFreeForTests (bool f) noexcept { loopFreeForTests = f; }
    /** p16: assigns morph corner `corner` (0..3) of the edit part: `presetIndex` = an entry of the library (its sound),
        -1 = the current sound, -2 = clear the corner, -3 = clear every corner. Switches the morph on when a corner
        gets a sound. */
    void assignMorphCorner (int corner, int presetIndex);
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
    void showCornerMenu (int corner); ///< p16
    void saveUiState();
    void loadUiState();
    void setParamPlain (const juce::String& id, float plain);
    void refreshVersionBadge(); ///< p27: "V0.6.1" / "UPDATE 0.7.0" above the wordmark
    void applyAccents();        ///< p32: key / encoder accent colours of the current skin
    void applySkinNow();        ///< p32: palette, look and feel, cached images
    void refreshHelpOverlay();  ///< p32: texts and visibility of the help overlay
    void refreshPadKeys(); ///< p25: the Pads view's soft keys follow the pad parameters
    void onSoftKey (int id);
    void showBounceMenu();                      ///< p23: the BOUNCE soft key of the Grain / Sampler page (source, loop)
    void showTableImportMenu();                 ///< p20: the IMPORT soft key (file, tape track, input, library, options)
    void importTable (const juce::File& file);  ///< p20: audio / wavetable file into the shown engine's user material
    void exportBounce();
    // p38: clear the whole tape (asks first), save / load the whole tape as an ".ohpetape" file
    void askClearTape();
    void saveTape();
    void loadTape();
    void loadTapeFile (const juce::File& f); ///< asks first when the tape holds material
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
    double gridHintUntil = 0.0; ///< p47: the grid's first-use hint in the keyboard readout shows until then (ms counter)
    std::array<unsigned, numEngineTypes> engineTipParts {}; ///< p14: part sets the engine tooltips were written for
    bool engineTipsValid = false;
    ui::PictoKey octDownKey { ui::Picto::OctaveDown, "Octave" };
    ui::PictoKey octUpKey { ui::Picto::OctaveUp, "Octave" };
    ui::PictoKey midiKey { ui::Picto::Midi, "MIDI" };
    ui::PictoKey gridKey { ui::Picto::Grid, "Grid" }; ///< p24
    ui::PictoKey libraryKey { ui::Picto::Library, "Library" };
    ui::PictoKey proKey { ui::Picto::Pro, "Pro" };
    // p14: header round keys, performance wheels (left of the keyboard), glide (right of it)
    ui::PictoKey randomKey { ui::Picto::Random, "Random" };
    ui::PictoKey saveKey { ui::Picto::Save, "Save" };
    ui::Wheel pitchWheel, modWheel;
    ui::SlideSwitch glideSwitch;
    ui::MiniKnob glideKnob;
    ui::LifeBar lifeBar; ///< p15: the edit part's life amount (foot of the engine column)
    ui::Readout tapeReadout, keysReadout; ///< p14 rev 3
    ui::TempoReadout tempoPanel { proc, ui::TempoReadout::Style::Panel }; ///< p39: the tempo in the transport panel
    ui::ClickStrip clickStrip { proc };                                    ///< p39: metronome, count-in
    juce::Image background; ///< p14: the static housing, panels and recesses (rendered once per size)
    ui::PartSelector partSelector;
    float spatialTime = 0.0f;
    juce::TooltipWindow tooltips { this, 600 }; ///< 1.0: tooltips (3D badge, part selector)
    ui::PresetStrip presetStrip;
    ui::PictoKey settingsKey { ui::Picto::Settings, "Settings" }; ///< p32: replaces the language pills
    ui::Keyboard keyboard;
    ui::GridKeyboard grid; ///< p24
    std::unique_ptr<ui::GridController> gridCtl; ///< p46
    void makeGridController();
    ui::ProView proView;
    ui::LibraryBrowser library;
    ui::BreedView breedView; ///< p18
    juce::SharedResourcePointer<Updater> updater; ///< p27: one per process
    ui::UpdateView updateView;
    juce::SharedResourcePointer<UserSettingsStore> userSettings; ///< p32: per user, shared by every open editor
    ui::SettingsView settingsView;
    ui::PackManagerView packView; ///< content packs: the pack manager (a settings page)
    bool packDropPending = false; ///< an .ohpepack was dropped / chosen: flash the result when the install ends
    ui::HelpOverlay helpOverlay;
    ui::ConfirmDialog confirmDialog; ///< p38
    bool tooltipsOn = true;
    float animRate = 1.0f;
    int skinPoll = 0;
    int shownSkin = -1;
    int shownLanguage = -1; ///< the language setting applied last (-1: none yet)
    float shownWindowScale = 0.0f;
    ui::VersionBadge versionBadge;
    ui::CpuMeter cpuMeter; ///< p35
    void refreshCpuMeter();
    int contentGenerationSeen = 0;
    juce::uint32 randomPressedAt = 0; ///< p18: RANDOM held >= 450 ms opens the breeding view
    std::unique_ptr<breed::Audition> randomAudition; ///< p37: offline check / level of RANDOM sounds (created on first use)
    std::array<int, 3> breedRequestsSeen {};
    int tapeClearAllSeen = 0; ///< p38: MIDI / host presses of "tape.edit.clearall"
    bool tapeLoopKeys = false, loopFreeForTests = false; ///< p42
    int tapeLoopSignature = -1;
    int loopKeySignature() const;

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
    int userTableMessagesSeen = 0; ///< p20
    bool userTableBusy = false;
    int bounceMessagesSeen = 0; ///< p23
    bool bounceRecording = false;
    int modPage = 0, rackPage = 0, lastModSignature = -1; ///< 0.7
    int morphPage = 0; ///< p16
    int motionPage = 0; ///< p22
    int lastMotionSignature = -1; ///< p22: soft key / status refresh of the Motion view
    int lastPadsSignature = -1;   ///< p25: soft key refresh of the Pads view
    int harmonyPage = 0, lastHarmonySignature = -1; ///< p24: the Harmony view
    std::array<ui::VirtualPtr, 4> virtuals {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OhpeEditor)
};
} // namespace ohpe
