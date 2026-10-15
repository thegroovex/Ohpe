// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

#include "Palette.h"
#include "plugin/Localisation.h"

namespace ohpe
{
using i18n::tr;
using namespace ui;

OhpeEditor::OhpeEditor (OhpeProcessor& p)
    : AudioProcessorEditor (p), proc (p), display (p), keyboard (p.keyboardState)
{
    setLookAndFeel (&lnf);

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
        key->onSecondaryClick = [this, k = key.get()] { showControlMenu (proc, "p1.engine", k); };
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
    octDownKey.onClick = [this] { keyboard.setBaseOctave (keyboard.getBaseOctave() - 1); saveUiState(); };
    octUpKey.onClick = [this] { keyboard.setBaseOctave (keyboard.getBaseOctave() + 1); saveUiState(); };
    midiKey.onClick = [this] { showMidiMenu(); };
    for (auto* k : { &playKey, &octDownKey, &octUpKey, &midiKey }) addAndMakeVisible (*k);

    for (auto* b : { &langEn, &langDe })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    langEn.onClick = [this] { setLanguage (i18n::Language::English); };
    langDe.onClick = [this] { setLanguage (i18n::Language::German); };

    display.onPageChange = [this] (int d) {
        const auto ids = modeParams (mode, proc.core(), mixerChannel);
        auto& pg = pageOf[(size_t) mode];
        pg = (pg + d + numPages (ids)) % numPages (ids);
        rebindEncoders();
        saveUiState();
    };
    display.onStepClicked = [this] (int step) {
        const int note = proc.core().lastNote();
        proc.core().stepSequencer().toggleStep (step, note >= 0 ? note : 60);
        display.repaint();
    };
    display.onStepPageClicked = [this] (int pg) { stepPage = pg; display.setStepPage (pg); };
    display.onChannelClicked = [this] (int ch) { mixerChannel = ch; display.setMixerChannel (ch); rebindEncoders(); saveUiState(); };
    addAndMakeVisible (display);
    addAndMakeVisible (keyboard);

    loadUiState();
    proc.addChangeListener (this);

    setResizable (true, true);
    setResizeLimits (designWidth * 2 / 3, designHeight * 2 / 3, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designWidth / designHeight);
    setSize (designWidth, designHeight);

    shownEngine = ui::selectedEngine (proc.core());
    rebindEncoders();
    refreshKeys();
    startTimerHz (30);
}

OhpeEditor::~OhpeEditor()
{
    stopTimer();
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
    langEn.setBounds (scaled (1070, 26, 42, 26));
    langDe.setBounds (scaled (1116, 26, 42, 26));
    keyboard.setBounds (scaled (40, 610, 1120, 166));

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
    g.drawText (tr ("designed by groovex"), juce::Rectangle<float> (142 * s, 30 * s, 220 * s, 20 * s), juce::Justification::centredLeft);
    g.setColour (colour::signalRed);
    g.fillEllipse (128 * s, 36 * s, 7 * s, 7 * s);

    // engraved section legends + grid lines
    auto legend = [&] (const juce::String& text, float x, float y, float w) {
        g.setColour (colour::engraving);
        g.setFont (labelFont (11.0f * s));
        g.drawText (i18n::upper (text), juce::Rectangle<float> (x * s, y * s, w * s, 14 * s), juce::Justification::centredLeft);
        g.setColour (colour::bodyShade);
        g.fillRect (juce::Rectangle<float> (x * s, (y + 15) * s, w * s, 1.0f * s));
    };
    legend (tr ("Engine"), 40, 74, 190);
    legend (tr ("Mode"), 970, 74, 190);
    legend (tr ("Sequencer") + " / MIDI", 60, 444, 190);
    legend (tr ("Octave") + " " + juce::String (keyboard.getBaseOctave()), 970, 444, 190);

    // encoder wells
    g.setColour (colour::bodyShade.withAlpha (0.5f));
    g.fillRoundedRectangle (juce::Rectangle<float> (280 * s, 430 * s, 640 * s, 166 * s), 14 * s);
    // keyboard bed
    g.setColour (colour::bodyShade.withAlpha (0.7f));
    g.fillRoundedRectangle (juce::Rectangle<float> (30 * s, 602 * s, 1140 * s, 186 * s), 12 * s);
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
    auto* p = proc.parameter ("p1.engine");
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) t));
    p->endChangeGesture();
    // the core picks the change up on the next block; the UI follows at once
    proc.core().params().set ("p1.engine", (float) t);
    if (mode != Mode::Synth) mode = Mode::Synth;
    pageOf[(size_t) Mode::Synth] = 0;
    shownEngine = t;
    rebindEncoders();
    refreshKeys();
    saveUiState();
}

void OhpeEditor::setLanguage (i18n::Language lang)
{
    i18n::setCurrent (lang);
    saveUiState();
    refreshKeys();
    repaint();
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

void OhpeEditor::rebindEncoders()
{
    const auto ids = modeParams (mode, proc.core(), mixerChannel);
    auto& pg = pageOf[(size_t) mode];
    pg = juce::jlimit (0, numPages (ids) - 1, pg);
    std::vector<juce::String> shown;
    for (int i = 0; i < 4; ++i)
    {
        const size_t k = (size_t) (pg * 4 + i);
        const juce::String id = k < ids.size() ? juce::String (ids[k]) : juce::String();
        encoders[(size_t) i]->bind (id);
        shown.push_back (id);
    }
    display.setMode (mode, pg, numPages (ids), shown);
    display.setMixerChannel (mixerChannel);
}

void OhpeEditor::refreshKeys()
{
    for (int t = 0; t < numEngineTypes; ++t) engineKeys[(size_t) t]->setActive ((EngineType) t == shownEngine);
    for (int m = 0; m < numModes; ++m) modeKeys[(size_t) m]->setActive ((Mode) m == mode);
    playKey.setActive (proc.core().params().get (Core::Ids::seqRun) >= 0.5f);
    langEn.setToggleState (i18n::current() == i18n::Language::English, juce::dontSendNotification);
    langDe.setToggleState (i18n::current() == i18n::Language::German, juce::dontSendNotification);
    for (auto& e : encoders) e->repaint();
}

void OhpeEditor::timerCallback()
{
    display.tick (1.0f / 30.0f);
    const auto engine = ui::selectedEngine (proc.core());
    if (engine != shownEngine)
    {
        shownEngine = engine;
        if (mode == Mode::Synth) rebindEncoders();
        refreshKeys();
    }
    const bool running = proc.core().params().get (Core::Ids::seqRun) >= 0.5f;
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
}

void OhpeEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
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
    juce::Component::SafePointer<OhpeEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&midiKey), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        if (r == 3) { self.proc.clearAllMidiMappings(); return; }
        const bool save = r == 1;
        self.chooser = std::make_unique<juce::FileChooser> (save ? tr ("Export MIDI map ...") : tr ("Import MIDI map ..."),
                                                            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.ohpemidi");
        const int flags = (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                                : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles;
        self.chooser->launchAsync (flags, [safe, save] (const juce::FileChooser& fc) {
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
    obj->setProperty ("channel", mixerChannel);
    obj->setProperty ("octave", keyboard.getBaseOctave());
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
    if (s.hasProperty ("channel")) mixerChannel = juce::jlimit (0, 7, (int) s["channel"]);
    if (s.hasProperty ("octave")) keyboard.setBaseOctave ((int) s["octave"]);
    if (auto* arr = s["pages"].getArray())
        for (int i = 0; i < juce::jmin (numModes, arr->size()); ++i) pageOf[(size_t) i] = (int) (*arr)[i];
}
} // namespace ohpe
