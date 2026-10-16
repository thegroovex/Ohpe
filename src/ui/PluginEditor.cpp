// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

#include "ModFxViews.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"
#include "plugin/TapeIO.h"
#include "engines/GrainEngine.h"
#include "engines/SamplerEngine.h"

#include <cmath>

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
    skSlotLeft = 60, skSlotRight, skSlotClear        // 0.7: FX rack
};

bool isAudioFile (const juce::String& path)
{
    const auto ext = juce::File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg";
}

bool isPresetFile (const juce::String& path) { return juce::File (path).hasFileExtension (preset::fileExtension); }

/** Parameters a step may lock: sound parameters, not transport / sequencer / remote controls. */
bool lockable (const juce::String& id)
{
    for (const char* p : { "seq.", "tempo", "sync.", "lib.", "ui.", "tape.", "key." })
        if (id.startsWith (p)) return false;
    return id.isNotEmpty();
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
        g.fillRoundedRectangle (c.translated (0, 1.5f), c.getHeight() * 0.2f);
        g.setColour (sel ? colour::keyDark : colour::bodyLight);
        g.fillRoundedRectangle (c, c.getHeight() * 0.2f);
        g.setColour (sel ? colour::sunYellow : colour::engraving);
        g.setFont (brandFont (c.getHeight() * 0.55f));
        g.drawText (juce::String (i + 1), c, juce::Justification::centred);
    }
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
    g.fillRoundedRectangle (b, b.getHeight() * 0.22f);
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
    : AudioProcessorEditor (p), proc (p), display (p), keyboard (p.keyboardState), proView (p), library (p)
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
        key->onClick = [this, m] { selectMode ((Mode) m); };
        addAndMakeVisible (*key);
        modeKeys[(size_t) m] = std::move (key);
    }

    playKey.setAccent (colour::signalRed);
    playKey.onClick = [this] { setParamPlain (Core::Ids::seqRun, proc.core().params().get (Core::Ids::seqRun) >= 0.5f ? 0.0f : 1.0f); };
    playKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::seqRun, &playKey); };
    octDownKey.onClick = [this] { keyboard.setBaseOctave (keyboard.getBaseOctave() - 1); saveUiState(); repaint(); };
    octUpKey.onClick = [this] { keyboard.setBaseOctave (keyboard.getBaseOctave() + 1); saveUiState(); repaint(); };
    midiKey.onClick = [this] { showMidiMenu(); };
    libraryKey.setAccent (colour::sunYellow);
    libraryKey.onClick = [this] { showLibrary (! library.isVisible()); };
    libraryKey.onSecondaryClick = [this] { showControlMenu (proc, Core::Ids::libOpen, &libraryKey); };
    proKey.setAccent (colour::ultraviolet);
    proKey.onClick = [this] { showProView (! proView.isVisible()); };
    for (auto* k : { &playKey, &octDownKey, &octUpKey, &midiKey, &libraryKey, &proKey }) addAndMakeVisible (*k);

    partSelector.onSelect = [this] (int part) { selectPart (part); };
    presetStrip.onStep = [this] (int d) { proc.presets().step (d); };
    presetStrip.onOpen = [this] { showLibrary (true); };
    addAndMakeVisible (partSelector);
    addAndMakeVisible (presetStrip);

    for (auto* b : { &langEn, &langDe })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    langEn.onClick = [this] { setLanguage (i18n::Language::English); };
    langDe.onClick = [this] { setLanguage (i18n::Language::German); };

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
    display.onFxSlotClicked = [this] (int s) { selectFxSlot (s); };
    display.onFxSlotMoved = [this] (int from, int to) { moveFxSlot (from, to); };
    display.onTrackClicked = [this] (int t) { selectTapeTrack (t); };
    display.onPadClicked = [this] (int pad) { selectDrumPad (pad, true); };
    display.onSoftKey = [this] (int id) { onSoftKey (id); };
    addAndMakeVisible (display);
    addAndMakeVisible (keyboard);

    proView.onClose = [this] { showProView (false); };
    proView.onModeSelected = [this] (Mode m) { selectMode (m); };
    addChildComponent (proView);
    library.onClosed = [this] { libraryKey.setActive (false); rebindEncoders(); };
    addChildComponent (library);

    loadUiState();
    proc.addChangeListener (this);
    proc.presets().addChangeListener (this);
    libraryRequestsSeen = proc.libraryToggleRequests();

    setResizable (true, true);
    setResizeLimits (designWidth * 2 / 3, designHeight * 2 / 3, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designWidth / designHeight);
    setSize (designWidth, designHeight);

    ctx.part = juce::jlimit (0, Core::numParts - 1, (int) std::lround (proc.core().params().get (Core::Ids::editPart)) - 1);
    shownEngine = ui::selectedEngine (proc.core(), ctx.part);
    rebindEncoders();
    refreshKeys();
    startTimerHz (30);
}

OhpeEditor::~OhpeEditor()
{
    stopTimer();
    proc.presets().removeChangeListener (this);
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> OhpeEditor::scaled (float x, float y, float w, float h) const
{
    const float s = (float) getWidth() / (float) designWidth;
    return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt();
}

void OhpeEditor::resized()
{
    // strict 10-unit grid on a 1200 x 800 canvas
    for (int t = 0; t < numEngineTypes; ++t)
        engineKeys[(size_t) t]->setBounds (scaled (40.0f + 100.0f * (float) (t % 2), 100.0f + 82.0f * (float) (t / 2), 90, 78));
    display.setBounds (scaled (260, 90, 680, 330));
    for (int m = 0; m < numModes; ++m)
        modeKeys[(size_t) m]->setBounds (scaled (970.0f + 100.0f * (float) (m % 2), 100.0f + 82.0f * (float) (m / 2), 90, 78));
    for (int i = 0; i < 4; ++i)
        encoders[(size_t) i]->setBounds (scaled (300.0f + 160.0f * (float) i, 440, 120, 150));
    playKey.setBounds (scaled (60, 470, 90, 78));
    midiKey.setBounds (scaled (160, 470, 90, 78));
    octDownKey.setBounds (scaled (970, 470, 90, 78));
    octUpKey.setBounds (scaled (1070, 470, 90, 78));
    // header (0.3): part selector, preset strip, library + pro keys
    partSelector.setBounds (scaled (300, 30, 150, 30));
    presetStrip.setBounds (scaled (470, 30, 290, 30));
    libraryKey.setBounds (scaled (776, 8, 78, 70));
    proKey.setBounds (scaled (862, 8, 78, 70));
    langEn.setBounds (scaled (1070, 26, 42, 26));
    langDe.setBounds (scaled (1116, 26, 42, 26));
    keyboard.setBounds (scaled (40, 610, 1120, 166));
    proView.setBounds (scaled (30, 86, 1140, 516));
    library.setBounds (scaled (30, 86, 1140, 516));

    // brushed aluminium texture, regenerated for the size
    const int w = juce::jmax (1, getWidth()), h = juce::jmax (1, getHeight());
    bodyTexture = juce::Image (juce::Image::RGB, w, h, false);
    juce::Graphics g (bodyTexture);
    g.setGradientFill (juce::ColourGradient (colour::bodyLight, 0, 0, colour::body.darker (0.03f), 0, (float) h, false));
    g.fillAll();
    juce::Random rnd (1971);
    for (int y = 0; y < h; ++y)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + 0.02f * rnd.nextFloat()));
        g.fillRect (0, y, w, 1);
    }
}

void OhpeEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (bodyTexture, 0, 0);
    const float s = (float) getWidth() / (float) designWidth;

    // wordmark + tagline
    g.setColour (colour::ink);
    g.setFont (brandFont (34.0f * s));
    g.drawText ("OHPE", juce::Rectangle<float> (40 * s, 18 * s, 200 * s, 40 * s), juce::Justification::centredLeft);
    g.setColour (colour::engraving);
    g.setFont (labelFont (12.0f * s));
    g.drawText (tr ("designed by groovex"), juce::Rectangle<float> (142 * s, 30 * s, 150 * s, 20 * s), juce::Justification::centredLeft, true);
    g.setColour (colour::signalRed);
    g.fillEllipse (128 * s, 36 * s, 7 * s, 7 * s);

    // engraved section legends + grid lines
    auto legend = [&] (const juce::String& text, float x, float y, float w) {
        g.setColour (colour::engraving);
        g.setFont (labelFont (11.0f * s));
        g.drawText (i18n::upper (text), juce::Rectangle<float> (x * s, y * s, w * s, 14 * s), juce::Justification::centredLeft, true);
        g.setColour (colour::bodyShade);
        g.fillRect (juce::Rectangle<float> (x * s, (y + 15) * s, w * s, 1.0f * s));
    };
    legend (tr ("Engine") + " / P" + juce::String (ctx.part + 1), 40, 74, 190);
    legend (tr ("Mode"), 970, 74, 190);
    legend (tr ("Sequencer") + " / MIDI", 60, 444, 190);
    legend (tr ("Octave") + " " + juce::String (keyboard.getBaseOctave()), 970, 444, 190);
    legend (tr ("Part"), 300, 10, 150);
    legend (tr ("Preset"), 470, 10, 290);

    // encoder wells
    g.setColour (colour::bodyShade.withAlpha (0.5f));
    g.fillRoundedRectangle (juce::Rectangle<float> (280 * s, 430 * s, 640 * s, 166 * s), 14 * s);
    // keyboard bed
    g.setColour (colour::bodyShade.withAlpha (0.7f));
    g.fillRoundedRectangle (juce::Rectangle<float> (30 * s, 602 * s, 1140 * s, 186 * s), 12 * s);
}

int& OhpeEditor::currentPage()
{
    if (mode == Mode::Sequencer) return seqPageOf[(size_t) ctx.seqView];
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMod) return modPage;       // 0.7
    if (mode == Mode::Effects && ctx.fxView != modfx::FxViewSends) return rackPage;  // 0.7
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
    shownEngine = ui::selectedEngine (proc.core(), part);
    pageOf[(size_t) Mode::Synth] = juce::jmin (pageOf[(size_t) Mode::Synth], numPages (modeParams (Mode::Synth, proc.core(), ctx)) - 1);
    rebindEncoders();
    refreshKeys();
    repaint();
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
    repaint();
}

void OhpeEditor::showLibrary (bool show)
{
    if (show == library.isVisible()) return;
    if (show)
    {
        proView.setVisible (false);
        proKey.setActive (false);
        library.open();
    }
    else
        library.close (true);
    libraryKey.setActive (show);
}

void OhpeEditor::showProView (bool show)
{
    if (show && library.isVisible()) showLibrary (false);
    proView.setVisible (show);
    proKey.setActive (show);
    if (show)
    {
        refreshProView();
        proView.toFront (false);
    }
    saveUiState();
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
    proView.setContent (mode, proGroups (mode, proc.core(), ctx), i18n::upper (sub));
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
    repaint (scaled (30, 60, 220, 40)); // engine legend shows the part
}

void OhpeEditor::advanceAnimation (float seconds)
{
    display.tick (seconds);
    // headless renders call this instead of the timer: keep the preset name current after a load
    presetStrip.setName (proc.presets().currentName());
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
    }
    else if (mode == Mode::Tape)
    {
        auto& tape = core.tape();
        keys = { { skLift, tr ("Lift") },     { skCut, tr ("Cut") },   { skDrop, tr ("Drop"), tape.clipboardFrames() > 0 },
                 { skSplice, tr ("Splice"), tape.clipboardFrames() > 0 },
                 { skUndo, tr ("Undo"), tape.canUndo() }, { skRedo, tr ("Redo"), tape.canRedo() },
                 { skRtz, juce::String::fromUTF8 ("|\xe2\x97\x80") }, { skExport, tr ("Export") } };
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
    else if (mode == Mode::Effects && ctx.fxView != modfx::FxViewSends) // 0.7
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

bool OhpeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile (f) || isPresetFile (f)) return true;
    return false;
}

void OhpeEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    for (const auto& path : files)
    {
        const juce::File f (path);
        if (isPresetFile (path))
        {
            preset::Preset p;
            if (preset::fromJson (f.loadFileAsString().toStdString(), p) && proc.presets().apply (p))
                display.flash (tr ("Loaded") + ": " + juce::String::fromUTF8 (p.meta.name.c_str()));
            continue;
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

void OhpeEditor::refreshKeys()
{
    for (int t = 0; t < numEngineTypes; ++t) engineKeys[(size_t) t]->setActive ((EngineType) t == shownEngine);
    for (int m = 0; m < numModes; ++m) modeKeys[(size_t) m]->setActive ((Mode) m == mode);
    playKey.setActive (proc.core().params().get (Core::Ids::seqRun) >= 0.5f);
    libraryKey.setActive (library.isVisible());
    proKey.setActive (proView.isVisible());
    langEn.setToggleState (i18n::current() == i18n::Language::English, juce::dontSendNotification);
    langDe.setToggleState (i18n::current() == i18n::Language::German, juce::dontSendNotification);
    for (auto& e : encoders) e->repaint();
}

bool OhpeEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (library.isVisible()) { library.close (false); return true; }
        if (proView.isVisible()) { showProView (false); return true; }
        if (lockStepIndex >= 0) { selectLockStep (-1); return true; }
    }
    return false;
}

void OhpeEditor::timerCallback()
{
    auto& core = proc.core();
    display.tick (1.0f / 30.0f);

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
        const auto pre = ctx.fxView == modfx::FxViewSends ? "p" + std::to_string (ctx.part + 1) + ".fx."
                                                          : modfx::rackPrefixes (ctx.fxView, ctx.part)[(size_t) juce::jlimit (0, (ctx.fxView == modfx::FxViewMaster ? FxRack::numMasterSlots : FxRack::numPartSlots) - 1, ctx.fxSlot)];
        const int fx = (int) std::lround (core.params().get (pre + "type"));
        if (fx != lastFxType) { lastFxType = fx; rebindEncoders(); }
        if (ctx.fxView != modfx::FxViewSends) refreshSoftKeys();
    }
    if (mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMod)
    {
        // the routing's target name follows the engine / FX type; soft key states follow the routing
        const auto r = "p" + std::to_string (ctx.part + 1) + ".mod.s" + std::to_string (ctx.modRouting + 1) + ".";
        const int sig = (int) std::lround (core.params().get (r + "src")) * 1000 + (int) std::lround (core.params().get (r + "dst"));
        if (sig != lastModSignature) { lastModSignature = sig; refreshSoftKeys(); }
    }
    if (core.effectsWaiting()) core.prepareEffects(); // 0.7: a rack slot got a new type (UI, host, preset)
    // 0.7: modulation rings on the encoders
    for (auto& e : encoders)
    {
        const auto r = modfx::modulationRange (core, e->boundId().toStdString());
        e->setModulation (r.any, r.lo, r.hi, r.live);
    }
    const bool running = core.params().get (Core::Ids::seqRun) >= 0.5f;
    if (running != playKey.getToggleState())
    {
        playKey.setToggleState (running, juce::dontSendNotification);
        refreshKeys();
    }
    const auto learning = proc.learningParamId();
    if (learning != lastLearning)
    {
        lastLearning = learning;
        for (auto& e : encoders) e->repaint();
    }
    if (lockStepIndex >= 0) updateVirtualFlags();
    if (proView.isVisible()) proView.refresh();
    if (mode == Mode::Tape) refreshSoftKeys(); // undo / redo / clipboard state

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
    for (const char* id : { Core::Ids::libOpen, Core::Ids::libPrev, Core::Ids::libNext, Core::Ids::editPart })
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

void OhpeEditor::saveUiState()
{
    auto* obj = proc.uiState.getDynamicObject();
    if (obj == nullptr) { proc.uiState = juce::var (new juce::DynamicObject()); obj = proc.uiState.getDynamicObject(); }
    obj->setProperty ("lang", i18n::current() == i18n::Language::German ? "de" : "en");
    obj->setProperty ("mode", (int) mode);
    obj->setProperty ("channel", ctx.mixerChannel);
    obj->setProperty ("octave", keyboard.getBaseOctave());
    obj->setProperty ("seqView", (int) ctx.seqView);
    obj->setProperty ("drumPad", ctx.drumPad);
    obj->setProperty ("lfoView", ctx.lfoView);    // 0.7
    obj->setProperty ("fxView", ctx.fxView);
    obj->setProperty ("modRouting", ctx.modRouting);
    obj->setProperty ("fxSlot", ctx.fxSlot);
    obj->setProperty ("pro", proView.isVisible());
    juce::Array<juce::var> pages;
    for (int pg : pageOf) pages.add (pg);
    obj->setProperty ("pages", pages);
}

void OhpeEditor::loadUiState()
{
    const auto& s = proc.uiState;
    if (! s.isObject()) return;
    if (s.hasProperty ("lang")) i18n::setCurrent (s["lang"].toString() == "de" ? i18n::Language::German : i18n::Language::English);
    if (s.hasProperty ("mode"))
    {
        const auto m = (Mode) juce::jlimit (0, numModes - 1, (int) s["mode"]);
        if (modeInfo (m).available) mode = m;
    }
    if (s.hasProperty ("channel")) ctx.mixerChannel = juce::jlimit (0, 7, (int) s["channel"]);
    if (s.hasProperty ("octave")) keyboard.setBaseOctave ((int) s["octave"]);
    if (s.hasProperty ("seqView")) ctx.seqView = (SeqView) juce::jlimit (0, numSeqViews - 1, (int) s["seqView"]);
    if (s.hasProperty ("drumPad")) ctx.drumPad = juce::jlimit (0, drum::DrumKit::numPads - 1, (int) s["drumPad"]);
    if (s.hasProperty ("lfoView")) ctx.lfoView = juce::jlimit (0, modfx::numLfoViews - 1, (int) s["lfoView"]); // 0.7
    if (s.hasProperty ("fxView")) ctx.fxView = juce::jlimit (0, modfx::numFxViews - 1, (int) s["fxView"]);
    if (s.hasProperty ("modRouting")) ctx.modRouting = juce::jlimit (0, mod::numSlots - 1, (int) s["modRouting"]);
    if (s.hasProperty ("fxSlot")) ctx.fxSlot = juce::jlimit (0, FxRack::numPartSlots - 1, (int) s["fxSlot"]);
    if (auto* arr = s["pages"].getArray())
        for (int i = 0; i < juce::jmin (numModes, arr->size()); ++i) pageOf[(size_t) i] = (int) (*arr)[i];
    ctx.tapeTrack = juce::jlimit (0, TapeMachine::numTracks - 1, (int) std::lround (proc.core().params().get (Core::Ids::tapeEditTrack)) - 1);
    if (s.hasProperty ("pro") && (bool) s["pro"] != proView.isVisible())
    {
        proView.setVisible ((bool) s["pro"]);
        proKey.setActive (proView.isVisible());
    }
}
} // namespace ohpe
