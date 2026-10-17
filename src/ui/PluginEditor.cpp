// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "UserTableView.h"
#include "wavetable/TableFile.h"

#include "ModFxViews.h"
#include "MorphView.h"
#include "MotionView.h"
#include "PadsView.h"
#include "HarmonyView.h"

#include "Palette.h"
#include "Brand.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"
#include "plugin/TapeIO.h"
#include "tape/TapeFile.h"
#include "engines/GrainEngine.h"
#include "engines/SamplerEngine.h"
#include "breed/RandomSound.h"

#include <cmath>
#include <cstring>
#include <map>

namespace ohpe
{
using i18n::tr;
using namespace ui;

namespace
{
// soft key ids (display)
enum SoftKeyId
{
    skLoadSample = 10, skClearSample,
    skLift = 20, skCut, skDrop, skSplice, skUndo, skRedo, skRtz, skExport,
    skPadLoad = 30, skPadClear,
    skClearLocks = 40, skLockDone,
    skModPrev = 50, skModNext, skModClear,           // 0.7: matrix
    skSlotLeft = 60, skSlotRight, skSlotClear,       // 0.7: FX rack
    skCornerA = 70, skCornerB, skCornerC, skCornerD, // p16: morph corners
    skTableImport = 80, skTableUser, skTableClear,    // p20: user wavetable / formants
    skBounce = 90,                                    // p23: bounce to engine (tape page: at once; Grain / Sampler: menu)
    skMotionRec = 100, skMotionPlay, skMotionClear,   // p22: motion recorder
    skPadFirst = 110,                                 // p25: performance pads (latching: 110 .. 113)
    skHarmonyTake = 120, skHarmonyHold, skHarmonyClear, // p24: harmony brain
    skTapeClear = 130, skTapeClearAll, skTapeSave, skTapeLoad // p38: clear track / all, save / load the tape
};

juce::String tapefileExtension() { return tapefile::extension; } // p38: ".ohpetape"

bool isAudioFile (const juce::String& path)
{
    const auto ext = juce::File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg";
}

/** p20: the engine pages that import user material (Wavetable: frames, Vocal: formant tracks). */
bool importsTables (EngineType e) { return e == EngineType::Wavetable || e == EngineType::Vocal; }
UserTableManager::Target tableTarget (EngineType e) { return e == EngineType::Vocal ? UserTableManager::Target::Vocal : UserTableManager::Target::Wavetable; }
juce::String userTableParam (int part, EngineType e)
{
    return "p" + juce::String (part + 1) + (e == EngineType::Vocal ? ".usertable.vocal" : ".usertable.wt");
}

bool isPresetFile (const juce::String& path) { return juce::File (path).hasFileExtension (preset::fileExtension); }

/** Parameters a step may lock: sound parameters, not transport / sequencer / remote controls. */
bool lockable (const juce::String& id)
{
    for (const char* p : { "seq.", "tempo", "sync.", "lib.", "ui.", "tape.", "key.", "perf.", "bounce." })
        if (id.startsWith (p)) return false;
    return id.isNotEmpty();
}

/** 0.7: settings of this computer (not of the project): animated 3D visuals. */
juce::PropertiesFile::Options uiSettingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Ohpe";
    o.folderName = "Ohpe";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

bool loadAnimatedVisualsSetting()
{
    juce::PropertiesFile f (uiSettingsOptions());
    return f.getBoolValue ("animatedVisuals", true);
}

void storeAnimatedVisualsSetting (bool on)
{
    juce::PropertiesFile f (uiSettingsOptions());
    f.setValue ("animatedVisuals", on);
    f.saveIfNeeded();
}

juce::String sampleName (const SamplePtr& s)
{
    return s != nullptr ? juce::String::fromUTF8 (s->name().c_str()) : juce::String();
}
} // namespace

// ---------------------------------------------------------------- header widgets
juce::Rectangle<float> PartSelector::cell (int i) const
{
    auto b = getLocalBounds().toFloat();
    const float w = b.getWidth() / (float) Core::numParts;
    return { b.getX() + w * (float) i, b.getY(), w, b.getHeight() };
}

void PartSelector::paint (juce::Graphics& g)
{
    for (int i = 0; i < Core::numParts; ++i)
    {
        auto c = cell (i).reduced (getHeight() * 0.06f);
        const bool sel = i == part;
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        g.fillRoundedRectangle (c.translated (0, 1.5f), radius::inset * (float) getHeight() / 32.0f);
        g.setColour (sel ? colour::keyDark : colour::bodyLight);
        g.fillRoundedRectangle (c, radius::inset * (float) getHeight() / 32.0f);
        g.setColour (sel ? colour::sunYellow : colour::engraving);
        g.setFont (brandFont (c.getHeight() * 0.55f));
        g.drawText (juce::String (i + 1), c, juce::Justification::centred);
        // 1.0: the headphones pictogram in the corner lights up while the part plays binaural 3D
        if (spatial[(size_t) i] != 0)
        {
            const auto icon = c.withSizeKeepingCentre (c.getHeight() * 0.42f, c.getHeight() * 0.42f)
                                  .withPosition (c.getRight() - c.getHeight() * 0.48f, c.getY() + c.getHeight() * 0.06f);
            if (spatial[(size_t) i] == 1)
                ui::space3d::drawHeadphones (g, icon, colour::ultraviolet.brighter (sel ? 0.5f : 0.0f), 0.55f + 0.45f * std::sin (time * 2.4f));
            else
                ui::space3d::drawSpeakers (g, icon, sel ? colour::sunYellow : colour::engraving);
        }
    }
}

juce::String PartSelector::getTooltip()
{
    const auto m = getMouseXYRelative().toFloat();
    for (int i = 0; i < Core::numParts; ++i)
        if (cell (i).contains (m) && spatial[(size_t) i] != 0)
            return tr ("Part") + " " + juce::String (i + 1) + ": " + ui::space3d::badgeTooltip ((ui::space3d::State) spatial[(size_t) i]);
    return {};
}

void PartSelector::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < Core::numParts; ++i)
        if (cell (i).contains (e.position) && onSelect) onSelect (i);
}

juce::Rectangle<float> PresetStrip::arrow (bool right) const
{
    auto b = getLocalBounds().toFloat();
    const float w = b.getHeight() * 1.1f;
    return right ? b.withLeft (b.getRight() - w) : b.withWidth (w);
}

void PresetStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colour::display);
    g.fillRoundedRectangle (b, radius::inset * (float) getHeight() / 32.0f); // 32 design units high
    g.setColour (colour::displayText);
    g.setFont (labelFont (b.getHeight() * 0.5f));
    g.drawText (name, b.withTrimmedLeft (arrow (false).getWidth()).withTrimmedRight (arrow (true).getWidth()), juce::Justification::centred, true);
    for (bool right : { false, true })
    {
        auto a = arrow (right).reduced (b.getHeight() * 0.32f);
        juce::Path tri;
        if (right) tri.addTriangle (a.getX(), a.getY(), a.getX(), a.getBottom(), a.getRight(), a.getCentreY());
        else tri.addTriangle (a.getRight(), a.getY(), a.getRight(), a.getBottom(), a.getX(), a.getCentreY());
        g.setColour (colour::sunYellow);
        g.fillPath (tri);
    }
}

void PresetStrip::mouseDown (const juce::MouseEvent& e)
{
    if (arrow (false).contains (e.position)) { if (onStep) onStep (-1); }
    else if (arrow (true).contains (e.position)) { if (onStep) onStep (1); }
    else if (onOpen) onOpen();
}

// ---------------------------------------------------------------- editor
OhpeEditor::OhpeEditor (OhpeProcessor& p)
    : AudioProcessorEditor (p), proc (p), display (p),
      pitchWheel (p, Core::Ids::perfBend, "Pitch", true), modWheel (p, Core::Ids::perfMod, "Mod", false),
      glideSwitch (p), glideKnob (p, "Glide"), lifeBar (p),
      keyboard (p.keyboardState), grid (p), proView (p), library (p), breedView (p), updateView (*updater),
      settingsView (p, *updater, *userSettings), packView (*updater)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    for (int i = 0; i < 4; ++i)
    {
        encoders[(size_t) i] = std::make_unique<Encoder> (proc, i);
        addAndMakeVisible (*encoders[(size_t) i]);
    }

    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto& info = engineTypeInfo ((EngineType) t);
        auto key = std::make_unique<PictoKey> ((Picto) t, info.nameEn);
        key->setAvailable (info.implemented);
        key->setAccent (colour::sunYellow);
        key->setHasLed (true); // p14
        key->onClick = [this, t] { selectEngine ((EngineType) t); };
        key->onSecondaryClick = [this, k = key.get()] { showControlMenu (proc, "p" + juce::String (ctx.part + 1) + ".engine", k); };
        addAndMakeVisible (*key);
        engineKeys[(size_t) t] = std::move (key);
    }

    for (int m = 0; m < numModes; ++m)
    {
        const auto& info = modeInfo ((Mode) m);
        auto key = std::make_unique<PictoKey> (info.picto, info.nameEn);
        key->setAvailable (info.available);
        key->setAccent (colour::signalRed);
        key->setHasLed (true); // p14
        key->onClick = [this, m] { selectMode ((Mode) m); };
        addAndMakeVisible (*key);
        modeKeys[(size_t) m] = std::move (key);
    }

    // p11: the TRANSPORT cluster - play, record, stop, to the start, loop; right-click = MIDI learn
    playKey.setAccent (colour::signalRed);
    playKey.onClick = [this] { transport (OhpeProcessor::TransportKey::PlayStop); };
    playKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::tapeTxPlay, &playKey); };
    recKey.setAccent (colour::signalRed);
    recKey.setPictoColour (colour::signalRed);
    recKey.onClick = [this] { transport (OhpeProcessor::TransportKey::Record); };
    recKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::tapeTxRec, &recKey); };
    stopKey.onClick = [this] { transport (OhpeProcessor::TransportKey::Stop); };
    stopKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::tapeTxStop, &stopKey); };
    toStartKey.onClick = [this] { transport (OhpeProcessor::TransportKey::ToStart); };
    toStartKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::tapeRtz, &toStartKey); };
    loopKey.setAccent (colour::sunYellow);
    loopKey.onClick = [this] { transport (OhpeProcessor::TransportKey::Loop); };
    loopKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::tapeTxLoop, &loopKey); };
    for (auto* k : { &recKey, &stopKey, &toStartKey, &loopKey }) addAndMakeVisible (*k);
    // p14 rev 3: every key of the two bottom panels carries a light bar (unlit: a faint slot) - PLAY green while
    // running, REC red (breathing while recording), LOOP yellow, MIDI red during MIDI learn, the octave keys
    // yellow while the keyboard is shifted that way
    for (auto* k : { &playKey, &recKey, &stopKey, &toStartKey, &loopKey, &octDownKey, &octUpKey, &midiKey }) k->setHasLed (true);
    for (auto* r : { &tapeReadout, &keysReadout }) addAndMakeVisible (*r);
    addAndMakeVisible (tempoPanel); // p39
    addAndMakeVisible (clickStrip);
    glideSwitch.setCaption ("Portamento");
    octDownKey.onClick = [this] { keyboard.setBaseOctave (keyboard.getBaseOctave() - 1); saveUiState(); repaint(); };
    octUpKey.onClick = [this] { keyboard.setBaseOctave (keyboard.getBaseOctave() + 1); saveUiState(); repaint(); };
    midiKey.onClick = [this] { showMidiMenu(); };
    // p24: GRID swaps the piano keys for the isomorphic grid keyboard (yellow bar while it is shown)
    gridKey.setHasLed (true);
    gridKey.onClick = [this] {
        const bool show = ! grid.isVisible();
        // p47: the first time the grid is switched on (per project), a one-line hint in the keyboard readout for 5 s
        if (show && ! (bool) proc.uiState.getProperty ("gridHintSeen", false))
            gridHintUntil = juce::Time::getMillisecondCounterHiRes() + 5000.0;
        if (! show) gridHintUntil = 0.0;
        showGridKeyboard (show);
        saveUiState();
    };
    addAndMakeVisible (gridKey);
    libraryKey.setAccent (colour::sunYellow);
    libraryKey.onClick = [this] { showLibrary (! library.isVisible()); };
    libraryKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::libOpen, &libraryKey); };
    proKey.setAccent (colour::ultraviolet);
    proKey.onClick = [this] { showProView (! proView.isVisible()); };
    for (auto* k : { &playKey, &octDownKey, &octUpKey, &midiKey, &libraryKey, &proKey }) addAndMakeVisible (*k);
    // p14: header pills and round keys, the keyboard row's wheels and glide controls
    for (auto* k : { &libraryKey, &proKey, &randomKey, &saveKey }) k->setLegendOnTop (true);
    libraryKey.setStyle (PictoKey::Style::Pill);
    proKey.setStyle (PictoKey::Style::Pill);
    randomKey.setStyle (PictoKey::Style::Round);
    saveKey.setStyle (PictoKey::Style::Round);
    octDownKey.setLegend ("Octave -");
    octUpKey.setLegend ("Octave +");
    // p18: RANDOM rolls a sound; held (or right-clicked) it opens the breeding view, where it rolls a new parent B
    randomKey.onStateChange = [this] {
        if (randomKey.isDown() && randomPressedAt == 0) randomPressedAt = juce::Time::getMillisecondCounter();
    };
    randomKey.onClick = [this] {
        const bool held = randomPressedAt != 0 && juce::Time::getMillisecondCounter() - randomPressedAt >= 450;
        randomPressedAt = 0;
        if (breedView.isVisible())
        {
            breedView.randomParent (1);
            breedView.breedNew();
        }
        else if (held) showBreeding (true);
        else randomiseSound();
    };
    randomKey.onSecondaryClick = [this] { showBreeding (! breedView.isVisible()); };
    saveKey.onClick = [this] {
        if (breedView.isVisible() && breedView.selectedChild() >= 0) breedView.saveChild (breedView.selectedChild()); // p18: heart + library
        else openSave();
    };
    for (auto* k : { &randomKey, &saveKey }) addAndMakeVisible (*k);
    for (auto* c : { (juce::Component*) &pitchWheel, (juce::Component*) &modWheel, (juce::Component*) &glideSwitch, (juce::Component*) &glideKnob })
        addAndMakeVisible (*c);
    addAndMakeVisible (lifeBar); // p15: LIFE at the foot of the engine column

    partSelector.onSelect = [this] (int part) { selectPart (part); };
    presetStrip.onStep = [this] (int d) { proc.presets().step (d); };
    presetStrip.onOpen = [this] { showLibrary (true); };
    addAndMakeVisible (partSelector);
    addAndMakeVisible (presetStrip);

    // p32: the gear key (where the language pills were) opens the settings; the language lives there now
    settingsKey.setStyle (PictoKey::Style::Round);
    settingsKey.setLegendOnTop (true);
    settingsKey.setFlushRight (true); // p47
    settingsKey.onClick = [this] { showSettings (! settingsView.isVisible()); };
    addAndMakeVisible (settingsKey);

    display.onPageChange = [this] (int d) {
        const bool stepEdit = lockStepIndex >= 0 && mode == Mode::Sequencer && ctx.seqView == SeqView::Step;
        const int n = stepEdit ? 2 : numPages (modeParams (mode, proc.core(), ctx));
        auto& pg = currentPage();
        pg = (pg + d + n) % n;
        rebindEncoders();
        saveUiState();
    };
    display.onStepClicked = [this] (int step) {
        const int note = proc.core().lastNote();
        proc.core().stepSequencer().toggleStep (step, note >= 0 ? note : 60);
        display.repaint();
    };
    display.onStepSelected = [this] (int step) { selectLockStep (step == lockStepIndex ? -1 : step); };
    display.onLockBadgeClicked = [this] { selectLockStep (-1); };
    display.onStepPageClicked = [this] (int pg) { stepPage = pg; display.setStepPage (pg); };
    display.onChannelClicked = [this] (int ch) { ctx.mixerChannel = ch; rebindEncoders(); saveUiState(); };
    display.onSeqViewClicked = [this] (SeqView v) { selectSeqView (v); };
    // 0.7: LFO / Mod and Sends / Rack / Master views, the routing list, the FX chain
    display.onViewTabClicked = [this] (int v) { selectView (v); };
    display.onModRoutingClicked = [this] (int r) { selectModRouting (r); };
    display.onMorphCornerClicked = [this] (int c) { showCornerMenu (c); }; // p16
    display.onFxSlotClicked = [this] (int s) { selectFxSlot (s); };
    display.onFxSlotMoved = [this] (int from, int to) { moveFxSlot (from, to); };
    display.onSpaceBadgeClicked = [this] { // 1.0: the 3D badge opens the Space view of the edit part
        if (mode != Mode::Effects) selectMode (Mode::Effects);
        selectView (modfx::FxViewSpace);
    };
    display.onTrackClicked = [this] (int t) { selectTapeTrack (t); };
    display.onPadClicked = [this] (int pad) { selectDrumPad (pad, true); };
    display.onSoftKey = [this] (int id) { onSoftKey (id); };
    // p11: tape page transport and arm dots
    display.onTransportClicked = [this] (int b) {
        using T = OhpeProcessor::TransportKey;
        static const T actions[] = { T::ToStart, T::Stop, T::TapePlayStop, T::Record, T::Loop };
        transport (actions[juce::jlimit (0, 4, b)]);
    };
    display.onTrackArmClicked = [this] (int t) {
        selectTapeTrack (t);
        transport (OhpeProcessor::TransportKey::Arm);
    };
    display.setShowShortcuts (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
    display.setAnimatedVisuals (loadAnimatedVisualsSetting());
    display.onVisualSettingsChanged = [this] {
        saveUiState();
        storeAnimatedVisualsSetting (display.animatedVisuals());
    };
    addAndMakeVisible (display);
    addAndMakeVisible (keyboard);
    addChildComponent (grid); // p24

    proView.onClose = [this] { showProView (false); };
    proView.onModeSelected = [this] (Mode m) { selectMode (m); };
    proView.onSettingsChanged = [this] { saveUiState(); }; // p21: Simple / All
    addChildComponent (proView);
    library.onClosed = [this] { libraryKey.setActive (false); updateOverlayVisibility(); rebindEncoders(); };
    library.onInstallPack = [this] { packDropPending = true; packView.chooseAndInstall(); };
    library.onManagePacks = [this] { showSettings (true, packPage()); };
    addChildComponent (library);
    library.onBreed = [this] (int entry) { // p18: the library's Breed button
        library.close (false);
        showBreeding (true, entry);
    };
    breedView.onClosed = [this] { randomKey.setActive (false); updateOverlayVisibility(); rebindEncoders(); };
    addChildComponent (breedView);
    // p27: the update view, opened from the version legend / wordmark
    {
        const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        updater->setPart (standalone ? "app" : "plugin");
        updateView.setStandalone (standalone);
        updateView.onClosed = [this] { updateOverlayVisibility(); rebindEncoders(); };
        addChildComponent (updateView);
        versionBadge.setName ("versionBadge");
        // p32: the version legend / wordmark: the update view when an update is offered, otherwise the About page
        versionBadge.onClick = [this] {
            if (versionBadge.showsUpdate()) showUpdates (! updateView.isVisible());
            else if (settingsView.isVisible() && settingsView.page() == ui::SettingsView::About) showSettings (false);
            else showSettings (true, ui::SettingsView::About);
        };
        addAndMakeVisible (versionBadge);
        cpuMeter.setName ("cpuMeter"); // p35
        addAndMakeVisible (cpuMeter);
        contentGenerationSeen = updater->contentGeneration();
        refreshVersionBadge();
    }
    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 700); // p11: tooltips on keys and the display
    // p32: the settings view and the help overlay
    {
        const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        settingsView.setStandalone (standalone);
        settingsView.onClosed = [this] { settingsKey.setActive (false); updateOverlayVisibility(); rebindEncoders(); };
        settingsView.onSettingsChanged = [this] { applyUserSettings(); };
        settingsView.onOpenUpdates = [this] { showUpdates (true); };
        // content packs: the pack manager is a settings page; Updates and the library lead to it
        settingsView.addExtraPage ("Content packs", "Inhaltspakete", &packView);
        settingsView.onInstallPack = [this] { packDropPending = true; packView.chooseAndInstall(); };
        settingsView.onManagePacks = [this] { showSettings (true, packPage()); };
        settingsView.getAnimatedVisuals = [this] { return display.animatedVisuals(); };
        settingsView.setAnimatedVisuals = [this] (bool on) {
            display.setAnimatedVisuals (on);
            storeAnimatedVisualsSetting (on);
            saveUiState();
        };
        addChildComponent (settingsView);
        addChildComponent (helpOverlay);
        addChildComponent (confirmDialog); // p38
        userSettings->addChangeListener (this);
    }

    loadUiState();
    proc.addChangeListener (this);
    proc.presets().addChangeListener (this);
    libraryRequestsSeen = proc.libraryToggleRequests();
    for (int k = 0; k < 3; ++k) breedRequestsSeen[(size_t) k] = proc.breedRequestCount (k);
    tapeClearAllSeen = proc.tapeClearAllRequestCount(); // p38

    setResizable (true, true);
    setResizeLimits (designWidth * 2 / 3, designHeight * 2 / 3, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designWidth / designHeight);
    {   // p32: the default window size of the user settings
        const float ws = userSettings->get().windowScale;
        shownWindowScale = ws;
        setSize (juce::roundToInt ((float) designWidth * ws), juce::roundToInt ((float) designHeight * ws));
    }

    ctx.part = juce::jlimit (0, Core::numParts - 1, (int) std::lround (proc.core().params().get (Core::Ids::editPart)) - 1);
    shownEngine = ui::selectedEngine (proc.core(), ctx.part);
    bindPartControls();
    rebindEncoders();
    refreshKeys();
    refreshLeds();
    updateOverlayVisibility();
    applyUserSettings(); // p32: language, skin, tooltips, help overlay
    transportMessagesSeen = proc.transportMessageCount();
    startTimerHz (30);
}

OhpeEditor::~OhpeEditor()
{
    stopTimer();
    userSettings->removeChangeListener (this);
    proc.presets().removeChangeListener (this);
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> OhpeEditor::scaled (float x, float y, float w, float h) const
{
    const float s = (float) getWidth() / (float) designWidth;
    return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt();
}

// ---------------------------------------------------------------- p14: layout (design units, 8-unit grid)
juce::Rectangle<float> OhpeEditor::designArea (const char* name)
{
    struct Item { const char* name; float x, y, w, h; };
    static const Item items[] = {
        { "housing", 8, 8, 1184, 784 },
        { "partLegend", 304, 20, 152, 12 },     { "partSelector", 304, 40, 152, 32 },
        { "presetLegend", 472, 20, 272, 12 },   { "presetStrip", 472, 40, 272, 32 },
        // p14: the header row - every legend on one baseline (y 20 - 32), every control centred on y 56 (the
        // strips' centre line); 8 units between the keys, the last one ends 32 units from the housing's right
        // edge, as the wordmark starts 32 units from its left edge
        { "libraryKey", 760, 20, 80, 56 },      { "proKey", 848, 20, 80, 56 },
        // p47: the three round keys' rings 80 units apart, the last one flush right (ring centres 981, 1061, 1141)
        { "randomKey", 949, 20, 64, 56 },       { "saveKey", 1029, 20, 64, 56 },
        // p32: the gear key ends 32 units from the housing's right edge (the language pills moved into the settings)
        // p47: flush right - its ring and legend end on x 1160 (as the mode column's content, the CPU meter), mirroring
        // the left edge x 40 of the version legend, the logo mark and the engine caption
        { "settingsKey", 1094, 20, 66, 56 },
        // p27: the version legend on the header's legend line above the wordmark (the click area covers the wordmark)
        { "versionBadge", 40, 20, 248, 56 },
        // p35: the CPU meter, right-aligned on the mode column's legend line
        { "cpuMeter", 1064, 88, 96, 12 },
        { "engineLegend", 40, 88, 200, 12 },    { "modeLegend", 968, 88, 192, 12 },
        { "overlay", 24, 104, 1152, 512 },
        { "engineColumn", 32, 112, 208, 336 },  { "modeColumn", 960, 112, 208, 336 },
        { "display", 256, 112, 688, 336 },
        { "transportPanel", 32, 464, 208, 144 }, { "octavePanel", 960, 464, 208, 144 },
        { "keyboardBed", 24, 624, 1152, 152 },
        { "pitchWheel", 40, 636, 40, 128 },     { "modWheel", 96, 636, 40, 128 },
        { "keyboard", 152, 632, 872, 136 },
        // rev 3: PORTAMENTO (switch) over GLIDE (knob) - a 56-unit control column and one text column at x 1104
        { "glideSwitch", 1040, 632, 128, 72 },  { "glideKnob", 1040, 712, 128, 56 },
        // p15: LIFE - a light bar at the foot of the engine column, under the eight engine keys (64 high, 70 apart)
        { "lifeBar", 44, 404, 184, 32 },
    };
    for (const auto& i : items)
        if (std::strcmp (i.name, name) == 0) return { i.x, i.y, i.w, i.h };
    jassertfalse;
    return {};
}

void OhpeEditor::resized()
{
    auto place = [this] (juce::Component& c, const char* name) {
        const auto r = designArea (name);
        c.setBounds (scaled (r.getX(), r.getY(), r.getWidth(), r.getHeight()));
    };
    // columns: 2 x 4 keys of 88 x 72 on an 80-unit pitch, 12 units inside the recessed panels; p15: the engine keys
    // are 64 high on a 70-unit pitch, so the LIFE bar fits at the foot of the engine column
    const float s = (float) getWidth() / (float) designWidth;
    for (int t = 0; t < numEngineTypes; ++t)
        engineKeys[(size_t) t]->setBounds (scaled (44.0f + 96.0f * (float) (t % 2), 124.0f + 70.0f * (float) (t / 2), 88, 64));
    place (lifeBar, "lifeBar");
    lifeBar.setUiScale (s);
    for (int m = 0; m < numModes; ++m)
        modeKeys[(size_t) m]->setBounds (scaled (972.0f + 96.0f * (float) (m % 2), 124.0f + 80.0f * (float) (m / 2), 88, 72));
    place (display, "display");
    // four encoders centred under the display, 168 units apart
    for (int i = 0; i < 4; ++i)
        encoders[(size_t) i]->setBounds (scaled (280.0f + 168.0f * (float) i, 460, 136, 148));
    // p14 rev 3: the transport block (bottom left) and the octave block (bottom right) mirror each other: the
    // same panel, the same rows and key heights, on the 8-unit grid (12 units inside the panels)
    for (auto* k : { &playKey, &recKey, &stopKey, &toStartKey, &loopKey, &octDownKey, &octUpKey, &gridKey, &midiKey })
    {
        k->setJoined (false, false);
        k->setShowLegend (true);
    }
    auto box = [this] (juce::Component& c, float x, float y, float w, float h) {
        const float sc = (float) getWidth() / (float) designWidth;
        c.setBounds (juce::Rectangle<float> (x * sc, y * sc, w * sc, h * sc).toNearestIntEdges()); // joined keys share an edge
    };
    {
        // the tape-deck layout: a readout (32) over one segmented row of five joined keys (80): TO START STOP
        // PLAY REC LOOP; the octave block: the keyboard readout over OCTAVE - / OCTAVE + (two slots each,
        // joined) and MIDI
        const float slot = 184.0f / 5.0f;
        // p39: the readout row - tape position, the tempo, metronome and count-in (one 32-unit row, 4 units apart)
        box (tapeReadout, 44, 476, 76, 32);
        box (tempoPanel, 124, 476, 48, 32);
        box (clickStrip, 176, 476, 52, 32);
        ui::PictoKey* row[] = { &toStartKey, &stopKey, &playKey, &recKey, &loopKey };
        for (int i = 0; i < 5; ++i)
        {
            box (*row[i], 44.0f + slot * (float) i, 516, slot, 80);
            row[i]->setJoined (i > 0, i < 4);
        }
        box (keysReadout, 972, 476, 184, 32);
        // p24: OCTAVE - / + one and a half slots each, then GRID and MIDI
        box (octDownKey, 972, 516, slot * 1.5f, 80);
        box (octUpKey, 972 + slot * 1.5f, 516, slot * 1.5f, 80);
        box (gridKey, 972 + slot * 3.0f, 516, slot, 80);
        box (midiKey, 972 + slot * 4.0f, 516, slot, 80);
        octDownKey.setJoined (false, true);
        octUpKey.setJoined (true, false);
    }
    // header
    place (partSelector, "partSelector");
    place (presetStrip, "presetStrip");
    place (libraryKey, "libraryKey");
    place (proKey, "proKey");
    place (randomKey, "randomKey");
    place (saveKey, "saveKey");
    place (settingsKey, "settingsKey");
    // keyboard row: wheels left, glide right
    place (pitchWheel, "pitchWheel");
    place (modWheel, "modWheel");
    place (keyboard, "keyboard");
    place (grid, "keyboard"); // p24
    place (glideSwitch, "glideSwitch");
    place (glideKnob, "glideKnob");
    place (proView, "overlay");
    place (library, "overlay");
    place (breedView, "overlay"); // p18
    place (updateView, "overlay"); // p27
    place (settingsView, "overlay"); // p32
    helpOverlay.setBounds (getLocalBounds());
    confirmDialog.setBounds (getLocalBounds()); // p38
    place (versionBadge, "versionBadge");
    place (cpuMeter, "cpuMeter"); // p35
    for (auto& k : engineKeys) k->setUiScale (s);
    for (auto& k : modeKeys) k->setUiScale (s);
    for (auto* k : { &playKey, &recKey, &stopKey, &toStartKey, &loopKey, &octDownKey, &octUpKey, &midiKey, &gridKey, &libraryKey, &proKey, &randomKey, &saveKey, &settingsKey })
        k->setUiScale (s);
    glideSwitch.setUiScale (s);
    glideKnob.setUiScale (s);
    tapeReadout.setUiScale (s);
    tempoPanel.setUiScale (s); // p39
    clickStrip.setUiScale (s);
    keysReadout.setUiScale (s);
    background = {}; // rendered again at the next paint (at the physical pixel scale)
}

namespace
{
/** A panel sunk into the housing: darker fill, shadow under the upper edge, light at the lower lip. */
void recess (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour fill, float s)
{
    g.setColour (colour::sheen.withAlpha (0.8f));
    g.fillRoundedRectangle (r.translated (0, 1.0f * s), radius);
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.10f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getY() + 7.0f * s, false));
    g.fillRect (r.withHeight (7.0f * s));
    g.setColour (juce::Colours::black.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}
} // namespace

void OhpeEditor::renderBackground (float phys)
{
    // p14: the static part of the plate - housing, recessed panels, the display's frame - rendered once per
    // size at the physical pixel scale (crisp on 2x screens, no repaint work while playing)
    phys = juce::jlimit (1.0f, 4.0f, phys);
    const int w = juce::jmax (1, juce::roundToInt ((float) getWidth() * phys)), h = juce::jmax (1, juce::roundToInt ((float) getHeight() * phys));
    background = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (background);
    const float s = (float) w / (float) designWidth;
    auto d = [s] (juce::Rectangle<float> r) { return juce::Rectangle<float> (r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s); };

    g.fillAll (colour::backdrop);
    // housing: a soft shadow on the backdrop, matte warm white, a lit upper edge
    const auto housing = d (designArea ("housing"));
    const float hr = radius::housing * s;
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.018f * (float) (7 - i) * 0.6f));
        g.fillRoundedRectangle (housing.expanded ((float) i * 1.2f * s).translated (0, (float) i * 1.0f * s), hr + (float) i * s);
    }
    g.setGradientFill (juce::ColourGradient (colour::bodyLight, 0, housing.getY(), colour::body.darker (0.015f), 0, housing.getBottom(), false));
    g.fillRoundedRectangle (housing, hr);
    g.setColour (colour::sheen.withAlpha (0.9f));
    g.drawRoundedRectangle (housing.reduced (1.0f * s), hr - s, 1.0f * s);
    g.setColour (colour::bodyShade.darker (0.08f));
    g.drawRoundedRectangle (housing, hr, 1.0f * s);

    // recessed panels
    for (const char* n : { "engineColumn", "modeColumn", "transportPanel", "octavePanel" })
        recess (g, d (designArea (n)), radius::panel * s, colour::panel, s);
    recess (g, d (designArea ("keyboardBed")), radius::panel * s, colour::panel.darker (0.02f), s);
    // header: the part selector's track and the preset strip's slot
    recess (g, d (designArea ("partSelector")).expanded (3.0f * s), radius::panel * s, colour::panel, s);
    recess (g, d (designArea ("presetStrip")).expanded (3.0f * s), radius::panel * s, colour::panel, s);

    // the display sits in a dark frame sunk into the housing
    const auto disp = d (designArea ("display"));
    const auto frame = disp.expanded (7.0f * s);
    g.setColour (colour::sheen.withAlpha (0.85f));
    g.fillRoundedRectangle (frame.translated (0, 1.2f * s), radius::panel * s);
    g.setGradientFill (juce::ColourGradient (colour::bezelLow, 0, frame.getY(), colour::bezel, 0, frame.getBottom(), false));
    g.fillRoundedRectangle (frame, radius::panel * s);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (frame.reduced (0.5f * s), radius::panel * s, 1.0f * s);
}

void OhpeEditor::paint (juce::Graphics& g)
{
    const float phys = juce::jlimit (1.0f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (background.getWidth() != juce::roundToInt ((float) getWidth() * phys)) renderBackground (phys);
    g.drawImage (background, getLocalBounds().toFloat());
    const float s = (float) getWidth() / (float) designWidth;

    // p47: logo mark + wordmark + tagline on one baseline (the caps centred on the header's line, y 56 - like the
    // strips and keys); the mark's ring sits on the caps' middle, the red dot after OHPE like a full stop
    {
        const auto wordFont = brandFont (34.0f * s); // p14
        const float baseline = brand::baselineCentredOn (wordFont, 56.0f * s);
        const float capMid = baseline - wordFont.getHeight() * 0.274f; // cap height 0.549 of the font height
        const float markSize = 22.0f * s;
        // the ring's ink starts on x 40 (round: a hair over the edge, as an O overshoots a stem)
        brand::drawMark (g, juce::Rectangle<float> (39.6f * s, capMid - markSize * 0.5f, markSize, markSize), colour::ink, colour::signalRed);
        const float right = brand::drawWordmark (g, wordFont, 70.0f * s, baseline, colour::ink, colour::signalRed);
        const auto tagFont = labelFont (12.0f * s);
        const float tagX = right + 12.0f * s;
        g.setColour (colour::engraving);
        g.setFont (tagFont);
        g.drawText (tr ("designed by groovex"),
                    juce::Rectangle<float> (tagX, baseline - tagFont.getAscent(), 288.0f * s - tagX, tagFont.getHeight()),
                    juce::Justification::centredLeft, true);
    }

    // engraved section legends (small caps, no rules: the recessed panels carry the structure)
    auto legend = [&] (const juce::String& text, const char* area, bool optical = false) {
        const auto r = designArea (area);
        g.setColour (colour::engraving);
        drawBaselineLegend (g, i18n::upper (text), juce::Rectangle<float> (r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s),
                            11.0f * s, juce::Justification::centredLeft, optical);
    };
    legend (tr ("Part"), "partLegend");
    legend (tr ("Preset"), "presetLegend");
    // p11: the legends of the plate belong to its controls - hidden with them while the library or the
    // pro view lies on the plate
    if (! overlayOpen())
    {
        legend (tr ("Engine") + " / P" + juce::String (ctx.part + 1), "engineLegend", true); // p47: ink on x 40
        legend (tr ("Mode"), "modeLegend");
    }
}

int& OhpeEditor::currentPage()
{
    if (mode == Mode::Sequencer) return seqPageOf[(size_t) ctx.seqView];
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMod) return modPage;       // 0.7
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMorph) return morphPage;   // p16
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMotion) return motionPage; // p22
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewHarmony) return harmonyPage; // p24
    if (mode == Mode::Effects && ctx.fxView != modfx::FxViewSends) return rackPage;  // 0.7 (1.0: also the Space view)
    return pageOf[(size_t) mode];
}

// ---------------------------------------------------------------- 0.7: matrix and FX rack views
void OhpeEditor::selectView (int v)
{
    if (mode == Mode::Lfo) ctx.lfoView = juce::jlimit (0, modfx::numLfoViews - 1, v);
    else if (mode == Mode::Effects)
    {
        if (ctx.fxView != v) rackPage = 0;
        ctx.fxView = juce::jlimit (0, modfx::numFxViews - 1, v);
        const int n = ctx.fxView == modfx::FxViewMaster ? FxRack::numMasterSlots : FxRack::numPartSlots;
        ctx.fxSlot = juce::jlimit (0, n - 1, ctx.fxSlot);
    }
    rebindEncoders();
    refreshKeys();
    saveUiState();
}

void OhpeEditor::selectModRouting (int r)
{
    ctx.modRouting = juce::jlimit (0, mod::numSlots - 1, r);
    modPage = 0;
    rebindEncoders();
    saveUiState();
}

void OhpeEditor::selectFxSlot (int s)
{
    const int n = ctx.fxView == modfx::FxViewMaster ? FxRack::numMasterSlots : FxRack::numPartSlots;
    if (s != ctx.fxSlot) rackPage = 0;
    ctx.fxSlot = juce::jlimit (0, n - 1, s);
    rebindEncoders();
    saveUiState();
}

void OhpeEditor::moveFxSlot (int from, int to)
{
    auto& core = proc.core();
    const auto prefixes = modfx::rackPrefixes (ctx.fxView, ctx.part);
    if (FxRack::moveSlot (core.params(), prefixes, from, to,
                          [this, &core] (int idx, float v) { setParamPlain (juce::String (core.params().descriptor (idx).id), v); }))
    {
        core.prepareEffects();
        ctx.fxSlot = juce::jlimit (0, (int) prefixes.size() - 1, to);
        rebindEncoders();
        saveUiState();
    }
}

void OhpeEditor::selectMode (Mode m)
{
    if (! modeInfo (m).available) return;
    mode = m;
    rebindEncoders();
    refreshKeys();
    saveUiState();
}

void OhpeEditor::selectEngine (EngineType t)
{
    const juce::String id = "p" + juce::String (ctx.part + 1) + ".engine";
    auto* p = proc.parameter (id);
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) t));
    p->endChangeGesture();
    // the core picks the change up on the next block; the UI follows at once
    proc.core().params().set (id.toStdString(), (float) t);
    if (mode != Mode::Synth) mode = Mode::Synth;
    pageOf[(size_t) Mode::Synth] = 0;
    shownEngine = t;
    rebindEncoders();
    refreshKeys();
    saveUiState();
}

void OhpeEditor::selectPart (int part)
{
    part = juce::jlimit (0, Core::numParts - 1, part);
    if (std::lround (proc.core().params().get (Core::Ids::editPart)) != part + 1)
    {
        setParamPlain (Core::Ids::editPart, (float) (part + 1));
        proc.core().params().set (Core::Ids::editPart, (float) (part + 1)); // UI follows at once
    }
    ctx.part = part;
    bindPartControls();
    shownEngine = ui::selectedEngine (proc.core(), part);
    pageOf[(size_t) Mode::Synth] = juce::jmin (pageOf[(size_t) Mode::Synth], numPages (modeParams (Mode::Synth, proc.core(), ctx)) - 1);
    rebindEncoders();
    refreshKeys();
    repaint();
}

// ---------------------------------------------------------------- p14: glide, RANDOM, SAVE
void OhpeEditor::bindPartControls()
{
    const juce::String pre = "p" + juce::String (ctx.part + 1) + ".";
    glideSwitch.bind (pre + "glide.mode");
    glideKnob.bind (pre + "glide");
    lifeBar.bind (pre + "life"); // p15
}

void OhpeEditor::randomiseSound (juce::int64 seed)
{
    // p37: the engine's own sound parameters inside musical ranges, glide / pitch envelope reset, then checked and
    // level-normalised offline (breed::Audition on a private core, never the audio thread) - see core RandomSound.h.
    // Level (volume), tuning, voices, the sample choice, envelopes, effects and matrix stay; the part gain is set.
    const auto useSeed = (uint64_t) (seed != 0 ? seed : juce::Time::currentTimeMillis());
    if (randomAudition == nullptr) randomAudition = std::make_unique<breed::Audition>();
    const auto& schema = randomAudition->schema();
    const auto base = breed::fromRegistry (schema, proc.core().params(), ctx.part);
    const auto result = randomsound::make (*randomAudition, base, (int) shownEngine, useSeed);
    const juce::String prefix = "p" + juce::String (ctx.part + 1) + ".";
    for (int k = 0; k < schema.size() && k < (int) result.genome.size(); ++k)
        if (result.genome[(size_t) k] != base[(size_t) k])
            setParamPlain (prefix + juce::String (schema.gene (k).id), result.genome[(size_t) k]);
    rebindEncoders();
    display.flash (tr ("Random sound"));
    lastFlashText = tr ("Random sound");
}

void OhpeEditor::openSave()
{
    showLibrary (true);
    library.showSavePanel (true);
}

void OhpeEditor::selectSeqView (SeqView v)
{
    ctx.seqView = v;
    if (v != SeqView::Step) lockStepIndex = -1;
    if (mode != Mode::Sequencer) mode = Mode::Sequencer;
    rebindEncoders();
    refreshKeys();
    saveUiState();
}

void OhpeEditor::selectTapeTrack (int track)
{
    ctx.tapeTrack = juce::jlimit (0, TapeMachine::numTracks - 1, track);
    setParamPlain (Core::Ids::tapeEditTrack, (float) (ctx.tapeTrack + 1));
    proc.core().params().set (Core::Ids::tapeEditTrack, (float) (ctx.tapeTrack + 1));
    rebindEncoders();
    saveUiState();
}

void OhpeEditor::selectDrumPad (int pad, bool audition)
{
    ctx.drumPad = juce::jlimit (0, drum::DrumKit::numPads - 1, pad);
    rebindEncoders();
    saveUiState();
    if (audition)
    {
        const int note = juce::jlimit (0, 127, proc.core().drumKit().baseNote() + ctx.drumPad);
        proc.keyboardState.noteOn (Core::drumChannel, note, 0.85f);
        juce::Component::SafePointer<OhpeEditor> safe (this);
        juce::Timer::callAfterDelay (220, [safe, note] {
            if (safe != nullptr) safe->proc.keyboardState.noteOff (Core::drumChannel, note, 0.0f);
        });
    }
}

void OhpeEditor::selectLockStep (int step)
{
    lockStepIndex = step >= 0 ? juce::jlimit (0, StepSequencer::maxSteps - 1, step) : -1;
    if (lockStepIndex >= 0)
    {
        stepPage = lockStepIndex / 16;
        display.setStepPage (stepPage);
        seqPageOf[(size_t) SeqView::Step] = 0;
    }
    rebindEncoders();
}

void OhpeEditor::setLanguage (i18n::Language lang)
{
    i18n::setCurrent (lang);
    library.languageChanged();
    saveUiState();
    refreshKeys();
    rebindEncoders();
    refreshHelpOverlay(); // p32
    repaint();
}

void OhpeEditor::showLibrary (bool show)
{
    if (show == library.isVisible()) return;
    if (show)
    {
        if (breedView.isVisible()) showBreeding (false); // p18
        if (updateView.isVisible()) showUpdates (false); // p27
        if (settingsView.isVisible()) showSettings (false); // p32
        proView.setVisible (false);
        proKey.setActive (false);
        library.open();
    }
    else
        library.close (true);
    libraryKey.setActive (show);
    updateOverlayVisibility();
}

void OhpeEditor::refreshVersionBadge()
{
    const auto us = updater->snapshot();
    versionBadge.setTexts ("V" + juce::String (OHPE_VERSION),
                           us.hasRelease ? i18n::upper (tr ("Update")) + " " + juce::String::fromUTF8 (us.release.version.toString().c_str()) : juce::String());
}

void OhpeEditor::refreshCpuMeter() { cpuMeter.setLoad (proc.cpuLoad().average(), proc.cpuLoad().peak()); }

void OhpeEditor::showUpdates (bool show)
{
    if (show == updateView.isVisible()) return;
    if (show)
    {
        if (library.isVisible()) showLibrary (false);
        if (breedView.isVisible()) showBreeding (false);
        if (proView.isVisible()) showProView (false);
        if (settingsView.isVisible()) showSettings (false); // p32
        updateView.open();
    }
    else
        updateView.close();
    updateOverlayVisibility();
}

void OhpeEditor::showSettings (bool show, int page)
{
    if (show == settingsView.isVisible())
    {
        if (show && page >= 0) settingsView.showPage (page);
        return;
    }
    if (show)
    {
        if (library.isVisible()) showLibrary (false);
        if (breedView.isVisible()) showBreeding (false);
        if (proView.isVisible()) showProView (false);
        if (updateView.isVisible()) showUpdates (false);
        settingsView.open (page);
    }
    else
        settingsView.close();
    settingsKey.setActive (show);
    updateOverlayVisibility();
}

// ---------------------------------------------------------------- p32: user settings
void OhpeEditor::applyUserSettings()
{
    const auto& us = userSettings->get();
    // language (only a fixed choice overrides the process-wide language; System follows the system / OHPE_LANGUAGE)
    {
        // only a change of the setting acts, so a language chosen for the session (setLanguage) stays until then
        const auto before = i18n::current();
        const int lang = (int) us.language;
        if (lang != shownLanguage)
        {
            if (us.language == settings::Language::English) i18n::setCurrent (i18n::Language::English);
            else if (us.language == settings::Language::German) i18n::setCurrent (i18n::Language::German);
            else if (shownLanguage >= 0) i18n::followSystem(); // a change to System in the settings (not at start-up)
            shownLanguage = lang;
        }
        if (i18n::current() != before)
        {
            library.languageChanged();
            refreshKeys();
            rebindEncoders();
        }
    }
    // skin
    const int skin = (int) settings::effectiveSkin (us.skin, us.skin == settings::Skin::Auto && UserSettingsStore::systemDark());
    if (skin != shownSkin)
    {
        shownSkin = skin;
        applySkinNow();
    }
    // tooltips
    if (us.tooltips != tooltipsOn || (us.tooltips && tooltipWindow == nullptr))
    {
        tooltipsOn = us.tooltips;
        if (tooltipsOn)
        {
            if (tooltipWindow == nullptr) tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 700);
            tooltips.setMillisecondsBeforeTipAppears (600);
        }
        else
        {
            tooltipWindow.reset();
            tooltips.setMillisecondsBeforeTipAppears (1 << 30);
            tooltips.hideTip();
        }
    }
    animRate = settings::animationRate (us.animations);
    // window size: only when the setting changed (the user may have resized the window since)
    if (us.windowScale != shownWindowScale)
    {
        shownWindowScale = us.windowScale;
        setSize (juce::roundToInt ((float) designWidth * us.windowScale), juce::roundToInt ((float) designHeight * us.windowScale));
    }
    refreshHelpOverlay();
    repaint();
}

void OhpeEditor::applySkinNow()
{
    ui::applySkin (shownSkin == (int) settings::Skin::Dark ? ui::Skin::Dark : ui::Skin::Light);
    lnf.refreshColours();
    applyAccents();
    keyboard.refreshColours();
    glideKnob.refreshColours();
    display.skinChanged();
    background = {};
    sendLookAndFeelChange();
    repaint();
}

void OhpeEditor::applyAccents()
{
    for (auto& k : engineKeys) k->setAccent (colour::sunYellow);
    for (auto& k : modeKeys) k->setAccent (colour::signalRed);
    playKey.setAccent (colour::signalRed);
    recKey.setAccent (colour::signalRed);
    recKey.setPictoColour (colour::signalRed);
    loopKey.setAccent (colour::sunYellow);
    libraryKey.setAccent (colour::sunYellow);
    proKey.setAccent (colour::ultraviolet);
    settingsKey.setAccent (colour::sunYellow);
    for (auto& e : encoders) e->refreshColours();
}

void OhpeEditor::refreshHelpOverlay()
{
    std::vector<ui::HelpOverlay::Item> items;
    auto add = [&items] (juce::Rectangle<float> r, const char* title, const char* text) { items.push_back ({ r, tr (title), tr (text) }); };
    add (designArea ("partSelector").expanded (4.0f).withTrimmedRight (0), "Parts", "Four parts, each with its own sound");
    add (designArea ("presetStrip").expanded (4.0f), "Preset", "Arrows step, a click opens the library");
    add (designArea ("engineColumn"), "Engines", "The sound engine of the edit part");
    add (designArea ("display"), "Display", "Pages and values - drag the bars");
    add (designArea ("modeColumn"), "Modes", "What the display and encoders edit");
    add ({ 272, 456, 656, 156 }, "Encoders", "Four colours = the four values on the display");
    add (designArea ("transportPanel"), "Transport", "Play, record, loop the tape");
    add (designArea ("octavePanel"), "Keyboard", "Octave, grid keyboard, MIDI");
    add ({ 32, 628, 112, 144 }, "Wheels", "Pitch bend and modulation");
    add ({ 1032, 628, 144, 144 }, "Glide", "Portamento mode and time");
    add (designArea ("settingsKey").expanded (2.0f), "Settings", "Language, help, skin, MIDI");
    helpOverlay.setItems (std::move (items));
    const bool show = userSettings->get().helpOverlay && ! overlayOpen();
    if (helpOverlay.isVisible() != show) helpOverlay.setVisible (show);
    if (show) helpOverlay.toFront (false);
}

void OhpeEditor::paintOverChildren (juce::Graphics& g)
{
    // p32: display brightness - the dark display dimmed (lower glare), the plate untouched
    const float b = userSettings->get().brightness;
    if (b < 0.999f && display.isVisible())
    {
        g.setColour (juce::Colours::black.withAlpha (juce::jlimit (0.0f, 0.75f, (1.0f - b) * 1.1f)));
        g.fillRoundedRectangle (display.getBounds().toFloat(), radius::inset * (float) getWidth() / (float) designWidth);
    }
}

void OhpeEditor::showBreeding (bool show, int libraryEntry)
{
    if (show == breedView.isVisible()) return;
    if (show)
    {
        if (library.isVisible()) showLibrary (false);
        if (proView.isVisible()) showProView (false);
        if (updateView.isVisible()) showUpdates (false); // p27
        if (settingsView.isVisible()) showSettings (false); // p32
        breedView.open (libraryEntry);
    }
    else
        breedView.close (true);
    randomKey.setActive (show);
    updateOverlayVisibility();
}

void OhpeEditor::showProView (bool show)
{
    if (show && breedView.isVisible()) showBreeding (false);
    if (show && library.isVisible()) showLibrary (false);
    if (show && updateView.isVisible()) showUpdates (false); // p27
    if (show && settingsView.isVisible()) showSettings (false); // p32
    proView.setVisible (show);
    proKey.setActive (show);
    if (show)
    {
        refreshProView();
        proView.toFront (false);
    }
    updateOverlayVisibility();
    saveUiState();
}

// ---------------------------------------------------------------- p11: overlays hide the plate
std::vector<juce::Component*> OhpeEditor::plateComponents()
{
    // everything that sits on the plate under the overlay area (p14: 24..1176 x 104..616)
    std::vector<juce::Component*> c { &display, &playKey, &recKey, &stopKey, &toStartKey, &loopKey, &midiKey, &octDownKey, &octUpKey,
                                      &tapeReadout, &keysReadout, &gridKey, &tempoPanel, &clickStrip };
    for (auto& k : engineKeys) c.push_back (k.get());
    c.push_back (&lifeBar); // p15
    for (auto& k : modeKeys) c.push_back (k.get());
    for (auto& e : encoders) c.push_back (e.get());
    return c;
}

void OhpeEditor::updateOverlayVisibility()
{
    // Before: the overlays only covered the plate by z-order. Legends painted by the editor above the
    // lid's top edge ("ENGINE / P1", "MODE") stayed visible, and the controls under the lid stayed live
    // (display animation, tooltips, hit-testing at the lid's edges). Now every plate control is hidden
    // while an overlay is open and the editor paints no plate legends.
    const bool plate = ! overlayOpen();
    for (auto* c : plateComponents())
        if (c->isVisible() != plate) c->setVisible (plate);
    refreshHelpOverlay(); // p32
    repaint();
}

// ---------------------------------------------------------------- p11: tape transport
void OhpeEditor::transport (OhpeProcessor::TransportKey action)
{
    const auto msg = proc.runTransport (action);
    if (msg.isNotEmpty())
    {
        display.flash (msg);
        lastFlashText = msg;
    }
    refreshTransportKeys();
    if (mode == Mode::Tape) rebindEncoders(); // arm / loop values on the encoders
    display.repaint();
}

bool OhpeEditor::handleShortcut (const juce::KeyPress& key)
{
    if (proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone) return false;
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown()) return false;
    using T = OhpeProcessor::TransportKey;
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (key.getKeyCode() == juce::KeyPress::spaceKey) { transport (T::PlayStop); return true; }
    if (c == 'r') { transport (T::Record); return true; }
    if ((key.getKeyCode() == juce::KeyPress::returnKey && ! library.isVisible()) || key.getKeyCode() == juce::KeyPress::homeKey)
    {
        transport (T::ToStart); // Enter keeps its meaning in the library (keep the preset)
        return true;
    }
    if (c == 'l') { transport (T::Loop); return true; }
    if (c == 't') { proc.tapTempo(); return true; }  // p39: tap tempo
    if (c == 'k') { ui::setParamFromScreen (proc, Metronome::Ids::on, proc.core().params().get (Metronome::Ids::on) >= 0.5f ? 0.0f : 1.0f); return true; } // p39: click
    return false;
}

void OhpeEditor::refreshTransportKeys()
{
    const bool seqRunning = proc.core().params().get (Core::Ids::seqRun) >= 0.5f;
    const bool blink = std::fmod (blinkClock, 1.0f) < 0.6f; // MIDI learn only
    const float pulse = ui::breath (blinkClock);            // p14: REC / PLAY breathe while running
    const bool playing = seqRunning || proc.tapeRolling();
    const bool loop = proc.core().params().get ("tape.loop") >= 0.5f;
    const bool recording = proc.tapeRecording(), ready = proc.tapeRecordReady();
    playKey.setActive (playing);
    recKey.setActive (recording || ready);
    loopKey.setActive (loop);
    // p14 rev 3: the transport light bars - PLAY green while running (its triangle lights too), REC red
    // (steady when armed, recording; the circle glows), LOOP yellow; STOP / TO START unlit. While PLAY
    // runs or REC records, bar and sign breathe gently (never off). The level is quantised to 1/48 so a key
    // repaints only when its light visibly changes.
    const float breathing = std::round (pulse * 48.0f) / 48.0f;
    playKey.setLed (colour::playGreen, playing ? breathing : 0.0f);
    recKey.setLed (colour::signalRed, recording ? breathing : (ready ? 1.0f : 0.0f));
    loopKey.setLed (colour::sunYellow, loop ? 1.0f : 0.0f);
    stopKey.setLed (colour::playGreen, 0.0f);
    toStartKey.setLed (colour::playGreen, 0.0f);
    const bool learning = proc.learningParamId().isNotEmpty();
    midiKey.setLed (colour::signalRed, learning ? (blink ? 1.0f : 0.45f) : 0.0f);
    const int oct = keyboard.getBaseOctave();
    octDownKey.setLed (colour::sunYellow, oct < 3 ? 1.0f : 0.0f);
    octUpKey.setLed (colour::sunYellow, oct > 3 ? 1.0f : 0.0f);
    gridKey.setLed (colour::sunYellow, grid.isVisible() ? 1.0f : 0.0f); // p24

    // the tape readout: state, time (m:ss.t) and bar.beat of the tape position
    {
        auto& tape = proc.core().tape();
        const double secs = juce::jmax (0.0, (double) tape.positionFrames() / juce::jmax (1.0, tape.sampleRate()));
        const int tenths = (int) (secs * 10.0);
        const juce::String time = juce::String (tenths / 600) + ":" + juce::String ((tenths / 10) % 60).paddedLeft ('0', 2) + "." + juce::String (tenths % 10);
        const double beats = secs * juce::jmax (20.0, (double) proc.core().params().get (Core::Ids::tempo)) / 60.0;
        const juce::String barBeat = juce::String ((int) (beats / 4.0) + 1) + "." + juce::String ((int) std::fmod (beats, 4.0) + 1);
        juce::String badge = i18n::upper (tr ("Tape"));
        auto badgeColour = colour::displayDim;
        if (proc.core().countInActive()) { badge = i18n::upper (tr ("Count-in")); badgeColour = colour::signalRed; } // p39
        else if (recording) { badge = "REC"; badgeColour = colour::signalRed.withAlpha (0.4f + 0.6f * pulse); }
        else if (ready) { badge = "REC"; badgeColour = colour::signalRed; }
        else if (proc.tapeRolling()) { badge = i18n::upper (tr ("Play")); badgeColour = colour::playGreenLit.withAlpha (0.4f + 0.6f * pulse); }
        tapeReadout.setContent (badge, badgeColour, time, barBeat);
    }
    // the keyboard readout: range of the on-screen keyboard and its octave shift; MIDI learn in red
    {
        const bool gridShown = grid.isVisible(); // p24: the grid's lowest and highest note
        const int lo = gridShown ? grid.lowestNote() : keyboard.getRangeStart(), hi = gridShown ? grid.highestNote() : keyboard.getRangeEnd();
        const auto name = [] (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 4); };
        const juce::String range = name (lo) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + name (hi);
        const juce::String shift = (oct > 3 ? "+" : oct < 3 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")))
                                 + juce::String (std::abs (oct - 3));
        if (learning)
            keysReadout.setContent (i18n::upper (tr ("MIDI Learn")), blink ? colour::signalRed : colour::signalRed.withAlpha (0.45f), range, shift);
        else
            keysReadout.setContent (i18n::upper (tr (gridShown ? "Grid" : "Keys")), colour::displayDim, range, shift);
        keysReadout.setHint (gridShown && ! learning && juce::Time::getMillisecondCounterHiRes() < gridHintUntil
                                 ? tr ("Same chord shape in every key") : juce::String()); // p47
    }
}

void OhpeEditor::refreshLeds()
{
    const auto leds = ui::keyLeds (ui::ledInputs (proc.core(), ctx.part, mode, proc.tapeRecording(), proc.tapeRolling()));
    const float pulse = std::round (ui::breath (blinkClock) * 48.0f) / 48.0f; // p14: recording / playing breathe
    auto apply = [pulse] (ui::PictoKey& k, ui::Led led) {
        switch (led)
        {
            case ui::Led::Focus:  k.setLed (colour::sunYellow, 1.0f); break;
            case ui::Led::Active: k.setLed (colour::ultraviolet, 1.0f); break;
            case ui::Led::Record: k.setLed (colour::signalRed, pulse); break;
            case ui::Led::Play:   k.setLed (colour::playGreen, pulse); break;
            case ui::Led::Off:    k.setLed (k.getLedColour(), 0.0f); break;
        }
    };
    for (int t = 0; t < numEngineTypes; ++t)
    {
        // rev 3: the engine key's bar has a segment per part - yellow the edit part, violet the other parts
        auto& k = *engineKeys[(size_t) t];
        std::array<juce::Colour, 4> colours {};
        std::array<float, 4> levels {};
        for (int p = 0; p < Core::numParts; ++p)
        {
            const auto seg = leds.engineSegments[(size_t) t][(size_t) p];
            colours[(size_t) p] = seg == ui::Led::Focus ? colour::sunYellow : colour::ultraviolet;
            levels[(size_t) p] = seg == ui::Led::Off ? 0.0f : 1.0f;
        }
        k.setBarSegments (colours, levels);
        if (! engineTipsValid || engineTipParts[(size_t) t] != leds.engineParts[(size_t) t])
        {
            engineTipParts[(size_t) t] = leds.engineParts[(size_t) t];
            juce::String parts;
            for (int p = 0; p < Core::numParts; ++p)
                if ((leds.engineParts[(size_t) t] >> p) & 1u) parts << (parts.isEmpty() ? "" : ", ") << (p + 1);
            auto tip = tr (engineTypeInfo ((EngineType) t).nameEn) + " - " + tr ("Bar: one segment per part - yellow = the edit part, violet = another part uses the engine");
            if (parts.isNotEmpty()) tip << "\n" << tr ("Parts") << ": " << parts;
            k.setTooltip (tip);
        }
    }
    engineTipsValid = true;
    for (int m = 0; m < numModes; ++m) apply (*modeKeys[(size_t) m], leds.mode[(size_t) m]);
}

void OhpeEditor::refreshProView()
{
    if (! proView.isVisible()) return;
    juce::String sub = tr (modeInfo (mode).nameEn);
    if (mode == Mode::Synth || mode == Mode::Envelope || mode == Mode::Lfo || mode == Mode::Effects)
    {
        sub << "  /  " << tr ("Part") << " " << (ctx.part + 1);
        if (mode == Mode::Synth) sub << "  /  " << i18n::pick (engineTypeInfo (shownEngine).nameEn, engineTypeInfo (shownEngine).nameDe);
    }
    if (mode == Mode::Mixer) sub << "  /  " << tr ("Channel") << " " << (ctx.mixerChannel + 1);
    if (mode == Mode::Sequencer) sub << "  /  " << tr (seqViewName (ctx.seqView));
    proView.setContent (mode, proGroups (mode, proc.core(), ctx), i18n::upper (sub), ctx.part);
}

void OhpeEditor::setParamPlain (const juce::String& id, float plain)
{
    if (auto* p = proc.parameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

// ---------------------------------------------------------------- lock editing (0.3)
VirtualPtr OhpeEditor::makeLockControl (const juce::String& paramId)
{
    auto& reg = proc.core().params();
    const int idx = reg.indexOf (paramId.toStdString());
    if (idx < 0 || ! lockable (paramId)) return nullptr;
    auto v = std::make_shared<VirtualControl>();
    v->paramId = paramId;
    v->name = i18n::pick (reg.descriptor (idx).nameEn, reg.descriptor (idx).nameDe);
    const int step = lockStepIndex;
    v->get = [this, idx, step, paramId] {
        const auto st = proc.core().stepSequencer().getStep (step);
        for (const auto& l : st.locks)
            if (l.paramIndex == idx) return l.normValue;
        auto* p = proc.parameter (paramId);
        return p != nullptr ? p->getValue() : 0.0f;
    };
    v->set = [this, idx, step] (float norm) {
        auto& seq = proc.core().stepSequencer();
        auto st = seq.getStep (step);
        int slot = -1;
        for (int k = 0; k < StepSequencer::maxLocksPerStep; ++k)
            if (st.locks[(size_t) k].paramIndex == idx) slot = k;
        for (int k = 0; k < StepSequencer::maxLocksPerStep && slot < 0; ++k)
            if (st.locks[(size_t) k].paramIndex < 0) slot = k;
        if (slot < 0)
        {
            display.flash (tr ("All four locks of this step are in use"));
            return;
        }
        st.locks[(size_t) slot] = { idx, juce::jlimit (0.0f, 1.0f, norm) };
        seq.setStep (step, st);
        updateVirtualFlags();
    };
    v->reset = [this, idx, step] {
        auto& seq = proc.core().stepSequencer();
        auto st = seq.getStep (step);
        for (auto& l : st.locks)
            if (l.paramIndex == idx) l = {};
        seq.setStep (step, st);
        updateVirtualFlags();
    };
    v->text = [this, paramId] (float norm) {
        const auto& r = proc.core().params();
        const int i = r.indexOf (paramId.toStdString());
        return i >= 0 ? formatValue (r.descriptor (i), r.descriptor (i).fromNormalised (norm)) : juce::String();
    };
    return v;
}

VirtualPtr OhpeEditor::makeStepField (int field)
{
    static const char* names[] = { "Note", "Velocity", "Gate", "Probability", "Ratchet" };
    if (field < 0 || field > 4) return nullptr;
    auto v = std::make_shared<VirtualControl>();
    v->name = tr (names[field]);
    const int step = lockStepIndex;
    auto toNorm = [field] (const StepSequencer::Step& s) -> float {
        switch (field)
        {
            case 0: return (float) s.note / 127.0f;
            case 1: return (float) (s.velocity - 1) / 126.0f;
            case 2: return (s.gate - 0.05f) / 0.95f;
            case 3: return (float) s.probability / 100.0f;
            default: return (float) (s.ratchet - 1) / 7.0f;
        }
    };
    auto apply = [field] (StepSequencer::Step& s, float n) {
        n = juce::jlimit (0.0f, 1.0f, n);
        switch (field)
        {
            case 0: s.note = (int) std::lround (n * 127.0f); break;
            case 1: s.velocity = 1 + (int) std::lround (n * 126.0f); break;
            case 2: s.gate = 0.05f + 0.95f * n; break;
            case 3: s.probability = (int) std::lround (n * 100.0f); break;
            default: s.ratchet = 1 + (int) std::lround (n * 7.0f); break;
        }
    };
    v->get = [this, step, toNorm] { return toNorm (proc.core().stepSequencer().getStep (step)); };
    v->set = [this, step, apply] (float n) {
        auto& seq = proc.core().stepSequencer();
        auto st = seq.getStep (step);
        apply (st, n);
        seq.setStep (step, st);
        display.repaint();
    };
    v->reset = [this, step, field, apply, toNorm] {
        auto& seq = proc.core().stepSequencer();
        auto st = seq.getStep (step);
        const StepSequencer::Step def;
        apply (st, toNorm (def));
        if (field == 0) st.note = def.note;
        seq.setStep (step, st);
    };
    v->text = [field] (float n) -> juce::String {
        switch (field)
        {
            case 0: return juce::MidiMessage::getMidiNoteName ((int) std::lround (n * 127.0f), true, true, 4);
            case 1: return juce::String (1 + (int) std::lround (n * 126.0f));
            case 2: return juce::String ((int) std::lround ((0.05f + 0.95f * n) * 100.0f)) + " %";
            case 3: return juce::String ((int) std::lround (n * 100.0f)) + " %";
            default: return juce::String (1 + (int) std::lround (n * 7.0f)) + "x";
        }
    };
    return v;
}

void OhpeEditor::updateVirtualFlags()
{
    if (lockStepIndex < 0) return;
    const auto st = proc.core().stepSequencer().getStep (lockStepIndex);
    const auto& reg = proc.core().params();
    for (size_t i = 0; i < virtuals.size(); ++i)
        if (auto& v = virtuals[i]; v != nullptr && v->paramId.isNotEmpty())
        {
            const int idx = reg.indexOf (v->paramId.toStdString());
            bool locked = false;
            for (const auto& l : st.locks) locked = locked || (idx >= 0 && l.paramIndex == idx);
            v->locked = locked;
        }
    for (auto& e : encoders)
    {
        e->refreshVirtual();
        e->repaint();
    }
    display.repaint();
}

// ---------------------------------------------------------------- binding
void OhpeEditor::rebindEncoders()
{
    auto& core = proc.core();
    const bool stepEdit = lockStepIndex >= 0 && mode == Mode::Sequencer && ctx.seqView == SeqView::Step;
    std::vector<std::string> ids = stepEdit ? std::vector<std::string> (8) : modeParams (mode, core, ctx);
    const int n = stepEdit ? 2 : numPages (ids);
    auto& pg = currentPage();
    pg = juce::jlimit (0, n - 1, pg);
    std::vector<juce::String> shown;
    for (int i = 0; i < 4; ++i)
    {
        const size_t k = (size_t) (pg * 4 + i);
        const juce::String id = k < ids.size() ? juce::String (ids[k]) : juce::String();
        VirtualPtr v;
        if (stepEdit) v = makeStepField ((int) k);
        else if (lockStepIndex >= 0) v = makeLockControl (id);
        virtuals[(size_t) i] = v;
        if (v != nullptr) encoders[(size_t) i]->bindVirtual (v);
        else encoders[(size_t) i]->bind (stepEdit ? juce::String() : id);
        shown.push_back (stepEdit ? juce::String() : id);
    }
    juce::String title;
    if (stepEdit) title = tr ("Step");
    else
    {
        const auto titles = pageTitles (mode, core, ctx);
        if (pg < (int) titles.size()) title = tr (titles[(size_t) pg].c_str());
    }
    display.setContext (ctx);
    display.setMode (mode, pg, n, shown, title);
    display.setVirtuals (virtuals);
    display.setLockStep (lockStepIndex);
    updateVirtualFlags();
    refreshSoftKeys();
    refreshProView();
    partSelector.setPart (ctx.part);
    presetStrip.setName (proc.presets().currentName());
    refreshVersionBadge(); // p27
    refreshCpuMeter(); // p35
    repaint (scaled (30, 60, 220, 40)); // engine legend shows the part
}

void OhpeEditor::refreshPadKeys()
{
    if (mode != Mode::Effects || ctx.fxView != modfx::FxViewPads) return; // p25: key states follow the pads (mouse, MIDI, host)
    int sig = 0;
    for (int k = 0; k < PerformancePads::numPads; ++k)
        sig = sig * 2 + (proc.core().params().get (PerformancePads::padId (k)) >= 0.5f ? 1 : 0);
    if (sig != lastPadsSignature) { lastPadsSignature = sig; refreshSoftKeys(); }
}

void OhpeEditor::advanceAnimation (float seconds)
{
    display.tick (seconds);
    refreshPadKeys(); // p25: headless renders show the latched keys too
    blinkClock += seconds;
    refreshTransportKeys();
    refreshLeds();
    {   // p14: the part keys' 3D pictograms as in the timer (headless renders show them too)
        std::array<int, 4> st {};
        for (int p = 0; p < Core::numParts; ++p) st[(size_t) p] = (int) ui::space3d::partState (proc.core(), p);
        spatialTime += seconds;
        partSelector.setSpatial (st, spatialTime);
    }
    if (proView.isVisible()) proView.tick (seconds); // p21: the pro view's pictures (headless renders too)
    // headless renders call this instead of the timer: keep the preset name current after a load
    presetStrip.setName (proc.presets().currentName());
    refreshVersionBadge(); // p27
    refreshCpuMeter(); // p35
}

void OhpeEditor::refreshSoftKeys()
{
    std::vector<Display::SoftKey> keys;
    juce::String status;
    auto& core = proc.core();
    if (mode == Mode::Synth && (shownEngine == EngineType::Sampler || shownEngine == EngineType::Grain))
    {
        keys.push_back ({ skLoadSample, tr ("Load sample") });
        keys.push_back ({ skClearSample, tr ("Clear") });
        keys.push_back ({ skBounce, tr ("Bounce"), true, proc.bounces().recording() }); // p23
        juce::String name;
        auto& part = core.part (ctx.part);
        if (shownEngine == EngineType::Grain)
        {
            if (auto* g = dynamic_cast<GrainEngine*> (part.engine (EngineType::Grain))) name = sampleName (g->sample());
        }
        else if (auto* s = dynamic_cast<SamplerEngine*> (part.engine (EngineType::Sampler)))
        {
            const auto zones = s->zones();
            if (! zones.empty()) name = sampleName (zones.front().sample) + (zones.size() > 1 ? " +" + juce::String ((int) zones.size() - 1) : juce::String());
        }
        status = name.isNotEmpty() ? name : tr ("Built-in tone - drop an audio file here");
        if (proc.bounces().recording()) // p23: the input recording of a bounce
            status = tr ("Recording the input for the bounce") + " " + juce::String (juce::roundToInt (proc.bounces().progress() * 100.0f)) + " %";
    }
    else if (mode == Mode::Synth && importsTables (shownEngine)) // p20
    {
        const bool has = usertable::tableOf (core, ctx.part, tableTarget (shownEngine)) != nullptr;
        const auto* prm = proc.parameter (userTableParam (ctx.part, shownEngine));
        const bool on = prm != nullptr && prm->getValue() > 0.5f;
        keys = { { skTableImport, tr ("Import") }, { skTableUser, tr ("User"), has, on && has }, { skTableClear, tr ("Clear"), has } };
        status = usertableview::statusLine (proc, ctx.part, shownEngine);
    }
    else if (mode == Mode::Tape)
    {
        auto& tape = core.tape();
        // p38: two rows - the edits of the edit track above; clear track / all, save / load the tape, export, bounce below
        const bool any = tape.contentEndFrames() > 0;
        keys = { { skLift, tr ("Lift") },     { skCut, tr ("Cut") },   { skDrop, tr ("Drop"), tape.clipboardFrames() > 0 },
                 { skSplice, tr ("Splice"), tape.clipboardFrames() > 0 },
                 { skUndo, tr ("Undo"), tape.canUndo() }, { skRedo, tr ("Redo"), tape.canRedo() },
                 { skTapeClear, tr ("Clear Track"), tape.contentEndFrames (ctx.tapeTrack) > 0 },
                 { skTapeClearAll, tr ("Clear All"), any },
                 { skTapeSave, tr ("Save Tape"), any },
                 { skTapeLoad, tr ("Load Tape") },
                 { skExport, tr ("Export") },
                 { skBounce, tr ("Bounce"), true, proc.bounces().recording() } }; // p11: "to the start" is a transport button now; p23: bounce
    }
    else if (mode == Mode::Drum)
    {
        keys = { { skPadLoad, tr ("Load") }, { skPadClear, tr ("Clear"), core.drumKit().padSample (ctx.drumPad) != nullptr } };
        if (auto s = core.drumKit().padSample (ctx.drumPad)) status = tr ("Pad") + " " + juce::String (ctx.drumPad + 1) + ": " + sampleName (s);
    }
    else if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMod) // 0.7
    {
        const auto r = "p" + std::to_string (ctx.part + 1) + ".mod.s" + std::to_string (ctx.modRouting + 1) + ".";
        keys = { { skModPrev, juce::String::fromUTF8 ("\xe2\x97\x80 ") + tr ("Mod") },
                 { skModNext, tr ("Mod") + juce::String::fromUTF8 (" \xe2\x96\xb6") },
                 { skModClear, tr ("Clear"), std::lround (core.params().get (r + "src")) != 0 || std::lround (core.params().get (r + "dst")) != 0 } };
    }
    else if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMorph) // p16: one key per corner (assign / clear)
    {
        for (int c = 0; c < MorphSpace::numCorners; ++c)
            keys.push_back ({ skCornerA + c, juce::String (MorphSpace::cornerName (c)) + juce::String::fromUTF8 (" \xc2\xb7 ")
                                                 + morphview::cornerTitle (core, ctx.part, c),
                              true, false });
    }
    else if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewHarmony) // p24: take the key, hold, forget
    {
        const auto* hold = proc.parameter ("harmony.hold");
        keys = { { skHarmonyTake, tr ("Take Key"), core.harmony().heard() },
                 { skHarmonyHold, tr ("Hold"), true, hold != nullptr && hold->getValue() > 0.5f },
                 { skHarmonyClear, tr ("Forget"), core.harmony().heard() } };
    }
    else if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMotion) // p22: record, play, clear the part's lanes
    {
        const auto pre = "p" + juce::String (ctx.part + 1) + ".motion.";
        auto isOn = [&] (const char* k) { const auto* prm = proc.parameter (pre + k); return prm != nullptr && prm->getValue() > 0.5f; };
        keys = { { skMotionRec, tr ("Rec"), true, isOn ("rec") },
                 { skMotionPlay, tr ("Play"), true, isOn ("on") },
                 { skMotionClear, tr ("Clear"), core.motion().anyLane (ctx.part) } };
    }
    else if (mode == Mode::Effects && ctx.fxView == modfx::FxViewPads) // p25: the pads as latching keys
    {
        for (int k = 0; k < PerformancePads::numPads; ++k)
        {
            const auto* prm = proc.parameter (PerformancePads::padId (k));
            keys.push_back ({ skPadFirst + k, tr (padsview::padName (k)), true, prm != nullptr && prm->getValue() > 0.5f });
        }
    }
    else if (mode == Mode::Effects && modfx::isRackView (ctx.fxView)) // 0.7
    {
        const auto prefixes = modfx::rackPrefixes (ctx.fxView, ctx.part);
        const int n = (int) prefixes.size();
        keys = { { skSlotLeft, juce::String::fromUTF8 ("\xe2\x97\x80 ") + tr ("Move"), ctx.fxSlot > 0 },
                 { skSlotRight, tr ("Move") + juce::String::fromUTF8 (" \xe2\x96\xb6"), ctx.fxSlot < n - 1 },
                 { skSlotClear, tr ("Clear"), std::lround (core.params().get (prefixes[(size_t) juce::jlimit (0, n - 1, ctx.fxSlot)] + "type")) != 0 } };
        if (core.effectsWaiting()) status = tr ("Preparing effect...");
    }
    else if (mode == Mode::Sequencer && ctx.seqView == SeqView::Step && lockStepIndex >= 0)
    {
        keys = { { skClearLocks, tr ("Clear locks") }, { skLockDone, tr ("Done"), true, true } };
        status = tr ("Turn any encoder to lock its value on this step");
    }
    display.setSoftKeys (keys);
    display.setStatus (status);
}

void OhpeEditor::onSoftKey (int id)
{
    auto& core = proc.core();
    switch (id)
    {
        case skLoadSample: chooseSampleFile(); break;
        case skClearSample:
            if (shownEngine == EngineType::Grain) proc.sampleLoader().clearGrain (ctx.part);
            else proc.sampleLoader().clearSampler (ctx.part);
            break;
        case skLift: proc.runTapeEdit (Core::Ids::tapeLift); break;
        case skCut: proc.runTapeEdit (Core::Ids::tapeCut); break;
        case skDrop: proc.runTapeEdit (Core::Ids::tapeDrop); break;
        case skSplice: proc.runTapeEdit (Core::Ids::tapeSplice); break;
        case skUndo: proc.runTapeEdit (Core::Ids::tapeUndo); break;
        case skRedo: proc.runTapeEdit (Core::Ids::tapeRedo); break;
        case skRtz: proc.runTapeEdit (Core::Ids::tapeRtz); break;
        case skExport: exportBounce(); break;
        case skTapeClear: // p38: one undo step, no question
            if (proc.tapeRecording()) display.flash (tr ("Not while recording"));
            else if (proc.runTapeEdit (Core::Ids::tapeClearTrack))
                display.flash (tr ("Track cleared") + " (" + juce::String (ctx.tapeTrack + 1) + ") - " + tr ("Undo brings it back"));
            break;
        case skTapeClearAll: askClearTape(); return;
        case skTapeSave: saveTape(); return;
        case skTapeLoad: loadTape(); return;
        case skPadLoad: chooseSampleFile(); break;
        case skPadClear: proc.sampleLoader().clearDrumPad (ctx.drumPad); break;
        case skClearLocks:
            if (lockStepIndex >= 0)
            {
                auto st = core.stepSequencer().getStep (lockStepIndex);
                for (auto& l : st.locks) l = {};
                core.stepSequencer().setStep (lockStepIndex, st);
            }
            break;
        case skLockDone: selectLockStep (-1); return;
        case skModPrev: selectModRouting ((ctx.modRouting + mod::numSlots - 1) % mod::numSlots); return;
        case skModNext: selectModRouting ((ctx.modRouting + 1) % mod::numSlots); return;
        case skModClear:
        {
            const auto r = "p" + juce::String (ctx.part + 1) + ".mod.s" + juce::String (ctx.modRouting + 1) + ".";
            for (const char* k : { "src", "via", "dst", "amt", "curve" })
                if (auto* prm = proc.parameter (r + k)) setParamPlain (r + k, prm->convertFrom0to1 (prm->getDefaultValue()));
            rebindEncoders();
            return;
        }
        case skCornerA:
        case skCornerB:
        case skCornerC:
        case skCornerD: showCornerMenu (id - skCornerA); return; // p16
        case skMotionRec: // p22
        case skMotionPlay:
        {
            const auto prmId = "p" + juce::String (ctx.part + 1) + (id == skMotionRec ? ".motion.rec" : ".motion.on");
            const auto* prm = proc.parameter (prmId); // the host value (the registry follows at the next block)
            setParamPlain (prmId, prm != nullptr && prm->getValue() > 0.5f ? 0.0f : 1.0f);
            break;
        }
        case skMotionClear: core.motion().clearPart (ctx.part); break; // p22
        case skPadFirst:
        case skPadFirst + 1:
        case skPadFirst + 2:
        case skPadFirst + 3: // p25: a soft key latches its pad
        {
            const juce::String prmId (PerformancePads::padId (id - skPadFirst));
            const auto* prm = proc.parameter (prmId);
            setParamPlain (prmId, prm != nullptr && prm->getValue() > 0.5f ? 0.0f : 1.0f);
            break;
        }
        case skHarmonyTake: proc.takeHarmonyKey(); break; // p24: the remembered key -> key.root / key.scale
        case skHarmonyHold:
        {
            const auto* prm = proc.parameter ("harmony.hold");
            setParamPlain ("harmony.hold", prm != nullptr && prm->getValue() > 0.5f ? 0.0f : 1.0f);
            break;
        }
        case skHarmonyClear: core.harmony().requestClear(); break; // p24
        case skTableImport: showTableImportMenu(); return; // p20
        case skBounce: // p23: the tape page bounces at once (the bounce settings), the Grain / Sampler page offers the sources
            if (mode == Mode::Tape)
            {
                if (proc.bounces().recording()) proc.bounces().finishRecording();
                else proc.bounces().bounce();
            }
            else
            {
                showBounceMenu();
                return;
            }
            break;
        case skTableUser:
        {
            const auto id = userTableParam (ctx.part, shownEngine);
            const auto* prm = proc.parameter (id); // the host value (the registry follows at the next block)
            setParamPlain (id, prm != nullptr && prm->getValue() > 0.5f ? 0.0f : 1.0f);
            break;
        }
        case skTableClear:
            proc.userTables().clear (ctx.part, tableTarget (shownEngine));
            setParamPlain (userTableParam (ctx.part, shownEngine), 0.0f);
            break;
        case skSlotLeft: moveFxSlot (ctx.fxSlot, ctx.fxSlot - 1); return;
        case skSlotRight: moveFxSlot (ctx.fxSlot, ctx.fxSlot + 1); return;
        case skSlotClear:
        {
            const auto prefixes = modfx::rackPrefixes (ctx.fxView, ctx.part);
            const auto& pre = prefixes[(size_t) juce::jlimit (0, (int) prefixes.size() - 1, ctx.fxSlot)];
            setParamPlain (pre + "type", 0.0f);
            setParamPlain (pre + "mix", 1.0f);
            setParamPlain (pre + "bypass", 0.0f);
            rebindEncoders();
            return;
        }
        default: break;
    }
    refreshSoftKeys();
    updateVirtualFlags();
}

// ---------------------------------------------------------------- p16: morph corners
void OhpeEditor::showCornerMenu (int corner)
{
    auto& core = proc.core();
    const int part = ctx.part;
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Corner") + " " + MorphSpace::cornerName (corner) + ": " + morphview::cornerTitle (core, part, corner));
    m.addItem (1, tr ("Current sound") + " (" + proc.presets().currentName() + ")");
    // part presets of the library, by their first category
    std::map<juce::String, juce::PopupMenu> byCategory;
    const auto& entries = proc.presets().entries();
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        if (e.meta.kind != preset::Kind::Part) continue;
        juce::String cat = tr ("Other");
        for (const auto& t : e.meta.tags)
            if (preset::isCategory (t))
            {
                for (const auto& c : preset::categories())
                    if (t == c.key) cat = i18n::pick (c.key, c.nameDe);
                break;
            }
        byCategory[cat].addItem (1000 + i, juce::String::fromUTF8 (e.meta.name.c_str()));
    }
    juce::PopupMenu library;
    for (auto& [cat, sub] : byCategory) library.addSubMenu (cat, sub);
    m.addSubMenu (tr ("Preset"), library);
    m.addSeparator();
    m.addItem (2, tr ("Clear corner"), core.morph().cornerSet (part, corner));
    m.addItem (3, tr ("Clear all corners"), core.morph().anyCorner (part));
    juce::Component::SafePointer<OhpeEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&display), [safe, corner] (int r) {
        if (safe == nullptr || r == 0) return;
        safe->assignMorphCorner (corner, r >= 1000 ? r - 1000 : r == 1 ? -1 : r == 2 ? -2 : -3);
    });
}

void OhpeEditor::assignMorphCorner (int corner, int presetIndex)
{
    const int part = ctx.part;
    bool assigned = false;
    if (presetIndex >= 0)
    {
        const auto& entries = proc.presets().entries();
        if (presetIndex >= (int) entries.size()) return;
        preset::Preset p;
        if (! preset::fromJson (proc.presets().read (entries[(size_t) presetIndex]), p) || p.meta.kind != preset::Kind::Part) return;
        proc.editRegistry ([&] (Core& c) { c.morph().setCorner (part, corner, p.meta.name, p.params); });
        assigned = true;
    }
    else if (presetIndex == -1)
    {
        const auto name = proc.presets().currentName().toStdString();
        proc.editRegistry ([&] (Core& c) { c.morph().captureCorner (part, corner, name); });
        assigned = true;
    }
    else if (presetIndex == -2) proc.core().morph().clearCorner (part, corner);
    else proc.core().morph().clearPart (part);
    const auto on = "p" + juce::String (part + 1) + ".morph.on";
    if (assigned && proc.core().params().get (on.toStdString()) < 0.5f) setParamPlain (on, 1.0f);
    refreshSoftKeys();
    display.repaint();
}

void OhpeEditor::chooseSampleFile()
{
    const bool drum = mode == Mode::Drum;
    chooser = std::make_unique<juce::FileChooser> (drum ? tr ("Load a sample onto the pad") : tr ("Load sample"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   proc.sampleLoader().filePatterns());
    juce::Component::SafePointer<OhpeEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc) {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        safe->loadAudioFile (fc.getResult());
    });
}

bool OhpeEditor::loadAudioFile (const juce::File& f, int pad)
{
    auto& loader = proc.sampleLoader();
    bool ok = false;
    if (mode == Mode::Drum || pad >= 0)
    {
        const int target = pad >= 0 ? pad : ctx.drumPad;
        ok = loader.loadDrumPad (target, f);
        if (ok) selectDrumPad (target);
    }
    else if (shownEngine == EngineType::Grain)
        ok = loader.loadGrainSample (f, ctx.part);
    else
    {
        ok = loader.loadSamplerSample (f, -1, ctx.part);
        if (ok && shownEngine != EngineType::Sampler) selectEngine (EngineType::Sampler);
    }
    display.flash (ok ? tr ("Loaded") + ": " + f.getFileName() : (loader.lastError().isNotEmpty() ? tr (loader.lastError()) : tr ("The file could not be loaded.")));
    refreshSoftKeys();
    return ok;
}

void OhpeEditor::importTable (const juce::File& f)
{
    auto& ut = proc.userTables();
    EngineType engine = shownEngine;
    if (mode != Mode::Synth || ! importsTables (engine)) // an .ohpewt dropped elsewhere: into the Wavetable engine
    {
        engine = EngineType::Wavetable;
        if (ui::selectedEngine (proc.core(), ctx.part) != EngineType::Wavetable) selectEngine (EngineType::Wavetable);
    }
    if (ut.status().busy) { display.flash (tr ("An import is running")); return; }
    if (! ut.importFile (f, ctx.part, tableTarget (engine))) display.flash (tr ("The file could not be loaded."));
    refreshSoftKeys();
}

void OhpeEditor::showBounceMenu()
{
    auto& core = proc.core();
    auto& b = proc.bounces();
    const int part = ctx.part;
    const int target = shownEngine == EngineType::Sampler ? bounce::TargetSampler : bounce::TargetGrain;
    enum { idSource = 100, idLoop = 1, idFinish = 2, idCancel = 3, idLen = 200 };
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Bounce into this engine"));
    if (b.recording())
    {
        m.addItem (idFinish, tr ("Finish the recording"));
        m.addItem (idCancel, tr ("Cancel the bounce"));
    }
    else
    {
        const int editTrack = juce::jlimit (0, TapeMachine::numTracks - 1, (int) std::lround (core.params().get (Core::Ids::tapeEditTrack)) - 1);
        const int current = (int) std::lround (core.params().get (Core::Ids::bounceSrc));
        m.addItem (idSource + bounce::SourceEditTrack, tr ("Edit track") + " (" + tr ("Tape") + " " + juce::String (editTrack + 1) + ")",
                   core.tape().contentEndFrames (editTrack) > 0, current == bounce::SourceEditTrack);
        for (int t = 0; t < TapeMachine::numTracks; ++t)
            m.addItem (idSource + bounce::SourceTrack1 + t, tr ("Tape") + " " + juce::String (t + 1), core.tape().contentEndFrames (t) > 0,
                       current == bounce::SourceTrack1 + t);
        m.addItem (idSource + bounce::SourceMix, tr ("Tape Mix"), core.tape().contentEndFrames() > 0, current == bounce::SourceMix);
        const int len = (int) std::lround (core.params().get (Core::Ids::bounceLen));
        m.addItem (idSource + bounce::SourceInput, tr ("Input") + " (" + juce::String (1 << juce::jlimit (0, 3, len)) + " " + tr ("bars") + ")",
                   true, current == bounce::SourceInput);
        juce::PopupMenu lens;
        for (int k = 0; k < bounce::numLengths; ++k) lens.addItem (idLen + k, juce::String (1 << k) + " " + tr ("bars"), true, len == k);
        m.addSubMenu (tr ("Input Length"), lens);
        m.addSeparator();
        m.addItem (idLoop, tr ("Endless loop"), true, core.params().get (Core::Ids::bounceLoop) >= 0.5f);
    }
    juce::Component::SafePointer<OhpeEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&display), [safe, part, target] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        auto& bm = self.proc.bounces();
        if (r == idFinish) bm.finishRecording();
        else if (r == idCancel) bm.cancel();
        else if (r == idLoop) self.setParamPlain (Core::Ids::bounceLoop, self.proc.core().params().get (Core::Ids::bounceLoop) >= 0.5f ? 0.0f : 1.0f);
        else if (r >= idLen && r < idLen + bounce::numLengths) self.setParamPlain (Core::Ids::bounceLen, (float) (r - idLen));
        else if (r >= idSource && r < idSource + bounce::numSources)
        {
            self.setParamPlain (Core::Ids::bounceSrc, (float) (r - idSource));
            bm.bounce (part, target, r - idSource);
        }
        self.refreshSoftKeys();
    });
}

void OhpeEditor::showTableImportMenu()
{
    auto& ut = proc.userTables();
    auto& core = proc.core();
    const auto target = tableTarget (shownEngine);
    const int part = ctx.part;
    enum { idFile = 1, idCancel = 2, idTape = 100, idInput = 200, idFrames = 300, idSmooth = 400, idLib = 1000, idPack = 2000 };
    juce::PopupMenu m;
    const bool busy = ut.status().busy;
    m.addItem (idFile, tr ("Audio or wavetable file ..."), ! busy);
    juce::PopupMenu tape;
    for (int t = 0; t < TapeMachine::numTracks; ++t)
        tape.addItem (idTape + t, tr ("Track") + " " + juce::String (t + 1), ! busy && core.tape().contentEndFrames (t) > 0);
    m.addSubMenu (tr ("From tape track"), tape, ! busy);
    juce::PopupMenu input;
    for (int sec : { 2, 4, 8 }) input.addItem (idInput + sec, juce::String (sec) + " s", ! busy);
    m.addSubMenu (tr ("Record input"), input, ! busy);
    const auto files = ut.libraryFiles();
    juce::PopupMenu lib;
    for (int i = 0; i < juce::jmin (200, files.size()); ++i) lib.addItem (idLib + i, files[i].getFileNameWithoutExtension(), ! busy);
    m.addSubMenu (tr ("Library"), lib, ! busy && ! files.isEmpty());
    // wavetable banks: the tables of the installed content packs (read-only), one sub menu per pack
    const auto packTables = target == usertable::Target::Wavetable ? ut.packTables() : std::vector<UserTableManager::PackTable> {};
    {
        juce::PopupMenu banks;
        juce::String packShown;
        juce::PopupMenu one;
        const auto current = usertable::tableOf (core, part, target);
        const juce::String currentRef = current != nullptr ? juce::String::fromUTF8 (current->contentRef.c_str()) : juce::String();
        for (int i = 0; i < juce::jmin (500, (int) packTables.size()); ++i)
        {
            const auto& t = packTables[(size_t) i];
            if (t.pack != packShown && packShown.isNotEmpty())
            {
                banks.addSubMenu (packTables[(size_t) i - 1].packName, one, ! busy);
                one = {};
            }
            packShown = t.pack;
            one.addItem (idPack + i, t.item, ! busy, t.ref() == currentRef);
        }
        if (packShown.isNotEmpty()) banks.addSubMenu (packTables.back().packName, one, ! busy);
        if (target == usertable::Target::Wavetable) m.addSubMenu (tr ("Content packs"), banks, ! busy && ! packTables.empty());
    }
    m.addSeparator();
    juce::PopupMenu frames;
    for (int n : { 8, 16, 32, 64, 128, 256 }) frames.addItem (idFrames + n / 8, juce::String (n), true, ut.frameCount == n);
    m.addSubMenu (tr ("Frames"), frames);
    juce::PopupMenu smooth;
    static const float levels[] = { 0.0f, 0.25f, 0.5f, 1.0f };
    static const char* const names[] = { "Off", "Light", "Medium", "Strong" };
    for (int i = 0; i < 4; ++i) smooth.addItem (idSmooth + i, tr (names[i]), true, std::abs (ut.smoothing - levels[i]) < 0.01f);
    m.addSubMenu (tr ("Smoothing"), smooth);
    if (busy)
    {
        m.addSeparator();
        m.addItem (idCancel, tr ("Cancel import"));
    }
    juce::Component::SafePointer<OhpeEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&display), [safe, target, part, files, packTables] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        auto& u = self.proc.userTables();
        bool started = true;
        if (r == idFile)
        {
            self.chooser = std::make_unique<juce::FileChooser> (tr ("Import a sound or wavetable"),
                                                                juce::File::getSpecialLocation (juce::File::userMusicDirectory), u.filePatterns());
            self.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc) {
                if (safe != nullptr && fc.getResult() != juce::File()) safe->importTable (fc.getResult());
            });
            return;
        }
        if (r == idCancel) u.cancel();
        else if (r >= idPack && r - idPack < (int) packTables.size())
        {
            if (! u.usePackTable (part, packTables[(size_t) (r - idPack)].ref())) self.display.flash (tr (u.lastMessage()));
            self.refreshSoftKeys();
            return;
        }
        else if (r >= idLib && r < idPack) started = u.importFile (files[r - idLib], part, target);
        else if (r >= idSmooth) u.smoothing = levels[juce::jlimit (0, 3, r - idSmooth)];
        else if (r >= idFrames) u.frameCount = (r - idFrames) * 8;
        else if (r >= idInput) started = u.recordInput ((double) (r - idInput), part, target);
        else if (r >= idTape) started = u.importTapeTrack (r - idTape, part, target);
        if (! started) self.display.flash (tr ("The import could not start"));
        self.refreshSoftKeys();
    });
}

bool OhpeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile (f) || isPresetFile (f) || juce::File (f).hasFileExtension (usertable::fileExtension)
            || juce::File (f).hasFileExtension (update::packFileExtension) || juce::File (f).hasFileExtension (tapefile::extension))
            return true;
    return false;
}

void OhpeEditor::installPack (const juce::File& pack)
{
    if (updater->busy()) { display.flash (tr ("Busy - try again in a moment")); return; }
    packDropPending = true;
    display.flash (tr ("Installing") + ": " + pack.getFileName());
    packView.install (pack);
}

void OhpeEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    for (const auto& path : files)
    {
        const juce::File f (path);
        if (f.hasFileExtension (update::packFileExtension)) // content packs: dropped on the window = Install pack
        {
            installPack (f);
            break;
        }
        if (f.hasFileExtension (tapefile::extension)) // p38: a tape file dropped on the window loads the tape (asks first)
        {
            loadTapeFile (f);
            break;
        }
        if (isPresetFile (path))
        {
            preset::Preset p;
            if (preset::fromJson (f.loadFileAsString().toStdString(), p) && proc.presets().apply (p))
                display.flash (tr ("Loaded") + ": " + juce::String::fromUTF8 (p.meta.name.c_str()));
            continue;
        }
        if (f.hasFileExtension (usertable::fileExtension) || (mode == Mode::Synth && importsTables (shownEngine) && isAudioFile (path)))
        {
            importTable (f); // p20: audio / wavetable files on the Wavetable and Vocal pages
            break;
        }
        if (! isAudioFile (path)) continue;
        int pad = -1;
        if (mode == Mode::Drum)
        {
            const auto local = display.getLocalPoint (this, juce::Point<int> (x, y)).toFloat();
            for (int i = 0; i < drum::DrumKit::numPads; ++i)
                if (display.padCell (i).contains (local)) pad = i;
        }
        loadAudioFile (f, pad);
        break; // one sample per drop
    }
}

void OhpeEditor::exportBounce()
{
    auto& tape = proc.core().tape();
    if (tape.contentEndFrames() <= 0)
    {
        display.flash (tr ("The tape is empty - record something first"));
        return;
    }
    chooser = std::make_unique<juce::FileChooser> (tr ("Export folder (master + 8 stems, 24-bit WAV)"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    juce::Component::SafePointer<OhpeEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe] (const juce::FileChooser& fc) {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        tapeio::BounceOptions o;
        o.folder = fc.getResult();
        o.baseName = "Ohpe " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M");
        const auto r = tapeio::bounce (safe->proc.core(), o);
        const juce::String msg = r.ok ? tr ("Exported") + " " + juce::String (r.files.size()) + " " + tr ("files to") + "\n" + o.folder.getFullPathName()
                                      : r.error;
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (r.ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon)
                                          .withTitle ("Ohpe").withMessage (msg).withButton ("OK"),
                                      nullptr);
    });
}

// ---------------------------------------------------------------- p38: clear the whole tape, tape files
void OhpeEditor::askClearTape()
{
    auto& tape = proc.core().tape();
    if (proc.tapeRecording()) { display.flash (tr ("Not while recording")); return; }
    if (tape.contentEndFrames() <= 0) { display.flash (tr ("The tape is empty")); return; }
    ui::ConfirmDialog::Options o;
    o.title = tr ("Clear the whole tape?");
    o.message = tr ("All 8 tracks will be emptied. Undo brings them back.");
    o.cancelText = tr ("Cancel");
    o.confirmText = tr ("Delete");
    juce::Component::SafePointer<OhpeEditor> safe (this);
    o.onConfirm = [safe] {
        if (safe == nullptr) return;
        if (safe->proc.tapeRecording()) safe->display.flash (tr ("Not while recording"));
        else if (safe->proc.runTapeEdit (Core::Ids::tapeClearAll)) safe->display.flash (tr ("Tape cleared") + " - " + tr ("Undo brings it back"));
        safe->refreshSoftKeys();
    };
    confirmDialog.show (std::move (o));
}

void OhpeEditor::saveTape()
{
    if (proc.tapeRecording()) { display.flash (tr ("Not while recording")); return; }
    if (proc.core().tape().contentEndFrames() <= 0) { display.flash (tr ("The tape is empty")); return; }
    const auto name = "Ohpe Tape " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M") + tapefileExtension();
    chooser = std::make_unique<juce::FileChooser> (tr ("Save the whole tape"), juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (name),
                                                   juce::String ("*") + tapefileExtension());
    juce::Component::SafePointer<OhpeEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File()) return;
                              juce::String msg;
                              safe->proc.saveTapeFile (fc.getResult(), msg);
                              safe->display.flash (msg);
                          });
}

void OhpeEditor::loadTape()
{
    if (proc.tapeRecording()) { display.flash (tr ("Not while recording")); return; }
    chooser = std::make_unique<juce::FileChooser> (tr ("Load a tape"), juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   juce::String ("*") + tapefileExtension());
    juce::Component::SafePointer<OhpeEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc) {
        if (safe != nullptr && fc.getResult() != juce::File()) safe->loadTapeFile (fc.getResult());
    });
}

void OhpeEditor::loadTapeFile (const juce::File& f)
{
    juce::Component::SafePointer<OhpeEditor> safe (this);
    auto doLoad = [safe, f] {
        if (safe == nullptr) return;
        juce::String msg;
        safe->proc.loadTapeFile (f, msg);
        safe->display.flash (msg);
        safe->rebindEncoders();
        safe->refreshSoftKeys();
    };
    if (proc.tapeRecording()) { display.flash (tr ("Not while recording")); return; }
    if (proc.core().tape().contentEndFrames() <= 0) { doLoad(); return; }
    ui::ConfirmDialog::Options o; // material on the tape: ask before it is replaced
    o.title = tr ("Replace the recorded tape?");
    o.message = tr ("The loaded tape replaces all 8 tracks. Undo brings the old ones back.");
    o.cancelText = tr ("Cancel");
    o.confirmText = tr ("Load");
    o.onConfirm = doLoad;
    confirmDialog.show (std::move (o));
}

void OhpeEditor::refreshKeys()
{
    for (int t = 0; t < numEngineTypes; ++t) engineKeys[(size_t) t]->setActive ((EngineType) t == shownEngine);
    for (int m = 0; m < numModes; ++m) modeKeys[(size_t) m]->setActive ((Mode) m == mode);
    refreshTransportKeys();
    // p11: tooltips (translated; the shortcut in brackets in the standalone app)
    const bool sa = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    auto tip = [sa] (const char* text, const char* key) { return tr (text) + (sa ? "  (" + juce::String (key) + ")" : juce::String()); };
    const bool linked = proc.core().params().get (Core::Ids::tapeTxLink) >= 0.5f;
    playKey.setTooltip (tip (linked ? "Play / stop - sequencer and tape" : "Play / stop the sequencer", "Space") + "\n" + tr ("Bar: green = playing (breathing)"));
    recKey.setTooltip (tip ("Record on the tape - arms the selected track when none is armed", "R") + "\n" + tr ("Bar: red = record on, breathing = recording"));
    stopKey.setTooltip (tip ("Stop - press again to go to the start", "Space"));
    toStartKey.setTooltip (tip ("To the start of the tape", "Enter"));
    loopKey.setTooltip (tip ("Loop between loop in and out", "L") + "\n" + tr ("Bar: yellow = loop on"));
    midiKey.setTooltip (tr ("MIDI maps and remote controls") + "\n" + tr ("Bar: red = MIDI learn"));
    gridKey.setTooltip (tr ("Grid keyboard: 4 x 24 pads with MPE - every chord has the same shape in every key (fourths, thirds, whole tones or key notes only); swaps the piano keys")
                        + "\n" + tr ("Bar: yellow = grid shown")); // p24, p47
    grid.setTooltip (tr ("Every chord has the same shape everywhere. With MPE on: slide sideways = bend, up / down = slide"));
    tapeReadout.setTooltip (tr ("Tape position (minutes:seconds) and bar.beat; REC = recording, PLAY = tape rolling"));
    keysReadout.setTooltip (tr ("Range of the on-screen keyboard and its octave shift; red during MIDI learn"));
    libraryKey.setTooltip (tr ("Preset library"));
    proKey.setTooltip (tr ("All parameters of the mode"));
    randomKey.setTooltip (tr ("Random: new values for the sound of the edit part's engine. Hold (or right-click) for breeding"));
    saveKey.setTooltip (tr ("Save the edit part as a user preset"));
    settingsKey.setTooltip (tr ("Settings: language, help, skin, display, MIDI, updates, about")); // p32
    pitchWheel.setTooltip (tr ("Pitch bend - springs back to the centre; right-click: MIDI learn"));
    modWheel.setTooltip (tr ("Mod wheel (modulation matrix source); right-click: MIDI learn"));
    glideSwitch.setTooltip (tr ("Glide mode of the edit part: Auto, Legato or Always"));
    glideKnob.setTooltip (tr ("Glide time of the edit part"));
    lifeBar.setTooltip (tr ("Life of the edit part: 0 = static; more = held notes keep evolving (drift, breathing filter, grain scatter). Right-click: MIDI learn"));
    octDownKey.setTooltip (tr ("Keyboard one octave down") + "\n" + tr ("Bar: yellow = keyboard shifted down"));
    octUpKey.setTooltip (tr ("Keyboard one octave up") + "\n" + tr ("Bar: yellow = keyboard shifted up"));
    for (int m = 0; m < numModes; ++m) // p14: what the mode keys' light bars say
    {
        const auto status = (Mode) m == Mode::Sequencer ? tr ("violet = sequencer running")
                          : (Mode) m == Mode::Tape      ? tr ("green = tape playing, red = recording")
                          : (Mode) m == Mode::Effects   ? tr ("violet = an FX rack slot is on")
                          : (Mode) m == Mode::Drum      ? tr ("violet = a sequencer plays the drum kit")
                          : (Mode) m == Mode::Lfo       ? tr ("violet = LFO or modulation routed")
                                                        : juce::String();
        modeKeys[(size_t) m]->setTooltip (tr (modeInfo ((Mode) m).nameEn) + " - " + tr ("Bar: yellow = shown page")
                                          + (status.isNotEmpty() ? ", " + status : juce::String()));
    }
    libraryKey.setActive (library.isVisible());
    proKey.setActive (proView.isVisible());
    settingsKey.setActive (settingsView.isVisible());
    for (auto& e : encoders) e->repaint();
}

bool OhpeEditor::keyPressed (const juce::KeyPress& key)
{
    if (handleShortcut (key)) return true; // p11: standalone transport shortcuts
    if (key.getKeyCode() == juce::KeyPress::F1Key) // p32: the help overlay on / off
    {
        auto s = userSettings->get();
        s.helpOverlay = ! s.helpOverlay;
        userSettings->set (s);
        applyUserSettings();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (library.isVisible()) { library.close (false); return true; }
        if (breedView.isVisible()) { breedView.close (false); return true; } // p18
        if (updateView.isVisible()) { showUpdates (false); return true; } // p27
        if (settingsView.isVisible()) { showSettings (false); return true; } // p32
        if (proView.isVisible()) { showProView (false); return true; }
        if (lockStepIndex >= 0) { selectLockStep (-1); return true; }
    }
    return false;
}

void OhpeEditor::timerCallback()
{
    auto& core = proc.core();
    display.tick (1.0f / 30.0f);
    {
        std::array<int, 4> st {}; // 1.0: 3D state of every part for the part selector
        for (int p = 0; p < Core::numParts; ++p) st[(size_t) p] = (int) ui::space3d::partState (core, p);
        spatialTime += animRate / 30.0f; // p32: Settings > Animations
        partSelector.setSpatial (st, spatialTime);
    }
    if (userSettings->get().skin == settings::Skin::Auto && ++skinPoll >= 60) // p32: follow the system's appearance
    {
        skinPoll = 0;
        applyUserSettings();
    }

    // p27: the daily update check (opt-in; never while the transport or the tape runs), the version legend, content packs
    updater->maybeAutoCheck (core.transportPlaying() || proc.tapeRolling());
    refreshVersionBadge();
    refreshCpuMeter(); // p35
    if (const int gen = updater->contentGeneration(); gen != contentGenerationSeen)
    {
        contentGenerationSeen = gen;
        proc.presets().rescan();
    }
    if (packDropPending && ! updater->busy() && updater->snapshot().busyPack.isEmpty())
    {
        packDropPending = false;
        const auto us = updater->snapshot();
        if (us.message.isNotEmpty()) display.flash (tr (us.message) + (us.detail.isNotEmpty() ? ": " + us.detail : juce::String()));
    }

    // remote controls / host: edit part, library key
    const int part = juce::jlimit (0, Core::numParts - 1, (int) std::lround (core.params().get (Core::Ids::editPart)) - 1);
    if (part != ctx.part) selectPart (part);
    const int track = juce::jlimit (0, TapeMachine::numTracks - 1, (int) std::lround (core.params().get (Core::Ids::tapeEditTrack)) - 1);
    if (track != ctx.tapeTrack) { ctx.tapeTrack = track; rebindEncoders(); }
    if (const int req = proc.libraryToggleRequests(); req != libraryRequestsSeen)
    {
        const bool toggle = ((req - libraryRequestsSeen) & 1) != 0;
        libraryRequestsSeen = req;
        if (toggle) showLibrary (! library.isVisible());
    }
    for (int k = 0; k < 3; ++k) // p18: MIDI remote "lib.breed" / ".go" / ".next"
        if (const int req = proc.breedRequestCount (k); req != breedRequestsSeen[(size_t) k])
        {
            const int presses = req - breedRequestsSeen[(size_t) k];
            breedRequestsSeen[(size_t) k] = req;
            if (k == 0 && (presses & 1) != 0) showBreeding (! breedView.isVisible());
            if (k == 1) { if (! breedView.isVisible()) showBreeding (true); breedView.breedAgain(); }
            if (k == 2) { if (! breedView.isVisible()) showBreeding (true); breedView.auditionNext(); }
        }

    if (const int req = proc.tapeClearAllRequestCount(); req != tapeClearAllSeen) // p38: MIDI "tape.edit.clearall" asks first
    {
        tapeClearAllSeen = req;
        if (! confirmDialog.isOpen()) askClearTape();
    }

    const auto engine = ui::selectedEngine (core, ctx.part);
    if (engine != shownEngine)
    {
        shownEngine = engine;
        if (mode == Mode::Synth || proView.isVisible()) rebindEncoders();
        refreshKeys();
    }
    // the FX slot page follows the slot type (0.7: the selected slot of the shown rack)
    if (mode == Mode::Effects)
    {
        const auto pre = ! modfx::isRackView (ctx.fxView) ? "p" + std::to_string (ctx.part + 1) + ".fx."
                                                          : modfx::rackPrefixes (ctx.fxView, ctx.part)[(size_t) juce::jlimit (0, (ctx.fxView == modfx::FxViewMaster ? FxRack::numMasterSlots : FxRack::numPartSlots) - 1, ctx.fxSlot)];
        const int fx = (int) std::lround (core.params().get (pre + "type"));
        if (fx != lastFxType) { lastFxType = fx; rebindEncoders(); }
        if (modfx::isRackView (ctx.fxView)) refreshSoftKeys();
    }
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMod)
    {
        // the routing's target name follows the engine / FX type; soft key states follow the routing
        const auto r = "p" + std::to_string (ctx.part + 1) + ".mod.s" + std::to_string (ctx.modRouting + 1) + ".";
        const int sig = (int) std::lround (core.params().get (r + "src")) * 1000 + (int) std::lround (core.params().get (r + "dst"));
        if (sig != lastModSignature) { lastModSignature = sig; refreshSoftKeys(); }
    }
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewHarmony) // p24: chips, bars and keys follow the brain
    {
        const auto& h = core.harmony();
        const auto c = h.chord();
        const int sig = ((((h.keyRoot() * 16 + h.keyScale()) * 200 + (c.root + 1) * 16 + c.type) * 16 + h.historyCount()) * 2 + (h.heard() ? 1 : 0)) * 2
                        + (core.params().get ("harmony.hold") >= 0.5f ? 1 : 0);
        if (sig != lastHarmonySignature) { lastHarmonySignature = sig; refreshSoftKeys(); }
        display.repaint(); // the memory bars move with every block
    }
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMotion) // p22: key states and the status line follow the recorder
    {
        const auto pre = "p" + std::to_string (ctx.part + 1) + ".motion.";
        int sig = (core.params().get (pre + "rec") >= 0.5f ? 1 : 0) + (core.params().get (pre + "on") >= 0.5f ? 2 : 0) + (core.transportPlaying() ? 4 : 0)
                + 8 * (int) std::lround (core.params().get (pre + "len"));
        for (int l = 0; l < MotionRecorder::numLanes; ++l) sig = sig * 3 + (core.motion().laneTarget (ctx.part, l) >= 0 ? 1 : 0);
        if (sig != lastMotionSignature) { lastMotionSignature = sig; refreshSoftKeys(); }
    }
    refreshPadKeys(); // p25
    if (core.effectsWaiting()) core.prepareEffects(); // 0.7: a rack slot got a new type (UI, host, preset)
    // 0.7: modulation rings on the encoders
    for (auto& e : encoders)
    {
        const auto r = modfx::modulationRange (core, e->boundId().toStdString());
        e->setModulation (r.any, r.lo, r.hi, r.live);
    }
    // p11: transport keys (REC / PLAY breathe), messages of MIDI-triggered transport actions
    if (animRate > 0.0f) blinkClock += animRate / 30.0f; // p32: Reduced breathes slower
    else blinkClock = 0.9f;                              // Off: the lights stand still, fully lit
    refreshTransportKeys();
    refreshLeds();
    tempoPanel.refresh(); // p39
    clickStrip.refresh();
    if (const int n = proc.transportMessageCount(); n != transportMessagesSeen)
    {
        transportMessagesSeen = n;
        lastFlashText = proc.lastTransportMessage();
        display.flash (lastFlashText);
        if (mode == Mode::Tape) rebindEncoders();
    }
    {   // p20: import progress and messages
        auto& ut = proc.userTables();
        const bool busy = ut.status().busy;
        if (const int n = ut.messageCount(); n != userTableMessagesSeen)
        {
            userTableMessagesSeen = n;
            const auto det = ut.lastDetail();
            display.flash (tr (ut.lastMessage()) + (det.isNotEmpty() ? ": " + det : juce::String()));
            refreshSoftKeys();
        }
        if (busy || busy != userTableBusy)
        {
            userTableBusy = busy;
            if (mode == Mode::Synth && importsTables (shownEngine)) refreshSoftKeys();
            display.repaint();
        }
    }
    {   // p23: bounce messages and the input recording's progress
        auto& b = proc.bounces();
        if (const int n = b.messageCount(); n != bounceMessagesSeen)
        {
            bounceMessagesSeen = n;
            display.flash (b.lastMessage());
            refreshSoftKeys();
        }
        if (b.recording() || b.recording() != bounceRecording)
        {
            bounceRecording = b.recording();
            refreshSoftKeys();
        }
    }
    const auto learning = proc.learningParamId();
    if (learning != lastLearning)
    {
        lastLearning = learning;
        for (auto& e : encoders) e->repaint();
        for (auto* c : { (juce::Component*) &pitchWheel, (juce::Component*) &modWheel, (juce::Component*) &glideSwitch, (juce::Component*) &glideKnob })
            c->repaint();
    }
    if (lockStepIndex >= 0) updateVirtualFlags();
    pitchWheel.refresh(); // p14: host automation / MIDI-mapped wheels
    modWheel.refresh();
    glideSwitch.refresh();
    {   // p15: the LIFE bar shimmers while the edit part sounds; shows the matrix's live offset
        auto& pt = core.part (ctx.part);
        lifeBar.tick (1.0f / 30.0f, pt.activeVoiceCount() > 0, pt.modulationOf (core.params().indexOf (lifeBar.boundId().toStdString())));
    }
    if (proView.isVisible()) proView.refresh();
    if (mode == Mode::Tape) refreshSoftKeys(); // undo / redo / clipboard state

    // p24: the grid keyboard plays the edit part, shifted by the octave keys; it shows the brain's key and chord
    if (grid.isVisible())
    {
        grid.setPart (ctx.part);
        grid.setBaseOctave (keyboard.getBaseOctave());
        grid.refresh();
    }
    // the on-screen keyboard plays the edit part (its MIDI channel; omni = 1)
    const int ch = juce::jlimit (1, 16, (int) std::lround (core.params().get ("p" + std::to_string (ctx.part + 1) + ".midich")));
    if (ch != lastKeyboardChannel)
    {
        lastKeyboardChannel = ch;
        keyboard.setMidiChannel (ch);
    }
    presetStrip.setName (proc.presets().currentName());
}

void OhpeEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &proc.presets())
    {
        presetStrip.setName (proc.presets().currentName());
        rebindEncoders();
        return;
    }
    if (source == &userSettings.getObject()) // p32: another editor (or this one) changed the user settings
    {
        applyUserSettings();
        return;
    }
    loadUiState();
    rebindEncoders();
    refreshKeys();
    repaint();
}

void OhpeEditor::showMidiMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("MIDI");
    m.addItem (1, tr ("Export MIDI map ..."));
    m.addItem (2, tr ("Import MIDI map ..."));
    m.addSeparator();
    m.addItem (3, tr ("Clear all MIDI mappings"), ! proc.core().midiMap().mappings().empty());
    m.addSeparator();
    juce::PopupMenu remote;
    for (const char* id : { Core::Ids::libOpen, Core::Ids::libPrev, Core::Ids::libNext, Core::Ids::libBreed, Core::Ids::libBreedGo,
                            Core::Ids::libBreedNext, Core::Ids::editPart,
                            // p11: tape transport
                            Core::Ids::tapeTxPlay, Core::Ids::tapeTxTapePlay, Core::Ids::tapeTxStop, Core::Ids::tapeTxRec,
                            Core::Ids::tapeRtz, Core::Ids::tapeTxLoop, Core::Ids::tapeTxArm, Core::Ids::tapeEditTrack,
                            Core::Ids::bounceGo, // p23
                            Core::Ids::tapeClearTrack, Core::Ids::tapeClearAll }) // p38
    {
        const int idx = proc.core().params().indexOf (id);
        if (idx < 0) continue;
        const auto& d = proc.core().params().descriptor (idx);
        const auto mapping = proc.describeMidiMapping (id);
        remote.addItem (100 + idx, tr ("MIDI Learn") + ": " + i18n::pick (d.nameEn, d.nameDe) + (mapping.isNotEmpty() ? " (" + mapping + ")" : juce::String()));
    }
    m.addSubMenu (tr ("Remote controls"), remote);
    juce::Component::SafePointer<OhpeEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&midiKey), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        if (r >= 100)
        {
            self.proc.armMidiLearn (juce::String (self.proc.core().params().descriptor (r - 100).id));
            return;
        }
        if (r == 3) { self.proc.clearAllMidiMappings(); return; }
        const bool save = r == 1;
        self.chooser = std::make_unique<juce::FileChooser> (save ? tr ("Export MIDI map ...") : tr ("Import MIDI map ..."),
                                                            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.ohpemidi");
        const int chooserFlags = (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                                : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles;
        self.chooser->launchAsync (chooserFlags, [safe, save] (const juce::FileChooser& fc) {
            if (safe == nullptr) return;
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            if (save) safe->proc.exportMidiMap (f.withFileExtension ("ohpemidi"));
            else if (! safe->proc.importMidiMap (f))
                juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                  .withTitle ("Ohpe").withMessage (tr ("The file is not an Ohpe MIDI map.")).withButton ("OK"),
                                              nullptr);
        });
    });
}

void OhpeEditor::showGridKeyboard (bool show) // p24
{
    grid.setBaseOctave (keyboard.getBaseOctave());
    grid.setPart (ctx.part);
    grid.setVisible (show);
    keyboard.setVisible (! show);
    gridKey.setActive (show);
}

void OhpeEditor::saveUiState()
{
    auto* obj = proc.uiState.getDynamicObject();
    if (obj == nullptr) { proc.uiState = juce::var (new juce::DynamicObject()); obj = proc.uiState.getDynamicObject(); }
    obj->setProperty ("lang", i18n::current() == i18n::Language::German ? "de" : "en");
    obj->setProperty ("mode", (int) mode);
    obj->setProperty ("channel", ctx.mixerChannel);
    obj->setProperty ("octave", keyboard.getBaseOctave());
    obj->setProperty ("grid", grid.isVisible()); // p24
    if (gridHintUntil > 0.0 || obj->hasProperty ("gridHintSeen")) obj->setProperty ("gridHintSeen", true); // p47
    obj->setProperty ("seqView", (int) ctx.seqView);
    obj->setProperty ("drumPad", ctx.drumPad);
    obj->setProperty ("lfoView", ctx.lfoView);    // 0.7
    obj->setProperty ("fxView", ctx.fxView);
    obj->setProperty ("modRouting", ctx.modRouting);
    obj->setProperty ("fxSlot", ctx.fxSlot);
    obj->setProperty ("pro", proView.isVisible());
    obj->setProperty ("proSimple", proView.isSimple()); // p21
    obj->setProperty ("visual", (int) display.visual());
    juce::Array<juce::var> pages;
    for (int pg : pageOf) pages.add (pg);
    obj->setProperty ("pages", pages);
}

void OhpeEditor::loadUiState()
{
    const auto& s = proc.uiState;
    if (! s.isObject()) return;
    // p32: the language is a user setting now (Settings > General); the session's "lang" is still written for older versions
    if (s.hasProperty ("mode"))
    {
        const auto m = (Mode) juce::jlimit (0, numModes - 1, (int) s["mode"]);
        if (modeInfo (m).available) mode = m;
    }
    if (s.hasProperty ("channel")) ctx.mixerChannel = juce::jlimit (0, 7, (int) s["channel"]);
    if (s.hasProperty ("octave")) keyboard.setBaseOctave ((int) s["octave"]);
    if (s.hasProperty ("grid")) showGridKeyboard ((bool) s["grid"]); // p24
    if (s.hasProperty ("seqView")) ctx.seqView = (SeqView) juce::jlimit (0, numSeqViews - 1, (int) s["seqView"]);
    if (s.hasProperty ("drumPad")) ctx.drumPad = juce::jlimit (0, drum::DrumKit::numPads - 1, (int) s["drumPad"]);
    if (s.hasProperty ("visual")) display.setVisual ((Display::Visual) juce::jlimit (0, (int) Display::Visual::numVisuals - 1, (int) s["visual"]));
    if (s.hasProperty ("lfoView")) ctx.lfoView = juce::jlimit (0, modfx::numLfoViews - 1, (int) s["lfoView"]); // 0.7
    if (s.hasProperty ("fxView")) ctx.fxView = juce::jlimit (0, modfx::numFxViews - 1, (int) s["fxView"]);
    if (s.hasProperty ("modRouting")) ctx.modRouting = juce::jlimit (0, mod::numSlots - 1, (int) s["modRouting"]);
    if (s.hasProperty ("fxSlot")) ctx.fxSlot = juce::jlimit (0, FxRack::numPartSlots - 1, (int) s["fxSlot"]);
    if (auto* arr = s["pages"].getArray())
        for (int i = 0; i < juce::jmin (numModes, arr->size()); ++i) pageOf[(size_t) i] = (int) (*arr)[i];
    ctx.tapeTrack = juce::jlimit (0, TapeMachine::numTracks - 1, (int) std::lround (proc.core().params().get (Core::Ids::tapeEditTrack)) - 1);
    if (s.hasProperty ("proSimple")) proView.setSimple ((bool) s["proSimple"], false); // p21
    if (s.hasProperty ("pro") && (bool) s["pro"] != proView.isVisible())
    {
        proView.setVisible ((bool) s["pro"]);
        proKey.setActive (proView.isVisible());
        updateOverlayVisibility();
    }
}
} // namespace ohpe

// ---------------------------------------------------------------- p27: the version legend
namespace ohpe::ui
{
juce::String VersionBadge::getTooltip() { return i18n::tr ("Version and updates"); }

void VersionBadge::paint (juce::Graphics& g)
{
    const float s = (float) getHeight() / 56.0f; // design units
    const auto legendArea = juce::Rectangle<float> (0, 0, (float) getWidth(), 12.0f * s);
    g.setColour (colour::engraving);
    drawBaselineLegend (g, versionText, legendArea, 11.0f * s, juce::Justification::centredLeft, true); // p47: ink on x 40
    if (updateText.isNotEmpty())
    {
        const float x = juce::GlyphArrangement::getStringWidth (labelFont (11.0f * s), versionText) + 10.0f * s;
        g.setColour (colour::sunYellow);
        g.fillRoundedRectangle (x, 3.0f * s, 12.0f * s, 6.0f * s, 3.0f * s); // a small yellow light bar: something to act on
        g.setColour (colour::engraving);
        drawBaselineLegend (g, updateText, legendArea.withTrimmedLeft (x + 18.0f * s), 11.0f * s, juce::Justification::centredLeft);
    }
}

// ---------------------------------------------------------------- p35: CPU meter
void CpuMeter::setLoad (float average, float peak)
{
    const int a = juce::jlimit (0, 999, juce::roundToInt (juce::jmax (0.0f, average) * 100.0f));
    const int p = juce::jlimit (0, 999, juce::roundToInt (juce::jmax (0.0f, peak) * 100.0f));
    if (a == avgPercent && p == peakPercent) return;
    const bool repaintNeeded = a != avgPercent || (p >= 100) != (peakPercent >= 100);
    avgPercent = a;
    peakPercent = p;
    if (repaintNeeded) repaint();
}

juce::String CpuMeter::getTooltip()
{
    return i18n::tr ("CPU load: share of the audio time the sound takes to compute") + " - " + i18n::tr ("Peak") + " "
           + juce::String (juce::jmax (0, peakPercent)) + " %";
}

void CpuMeter::paint (juce::Graphics& g)
{
    const float s = (float) getHeight() / 12.0f; // design units
    const bool hot = avgPercent >= 80 || peakPercent >= 100;
    g.setColour (hot ? colour::alertInk : colour::engraving); // both follow the skin (light / night)
    drawBaselineLegend (g, "CPU " + juce::String (juce::jmax (0, avgPercent)) + " %", getLocalBounds().toFloat(), 11.0f * s,
                        juce::Justification::centredRight, true); // p47: ink on x 1160
}
} // namespace ohpe::ui
