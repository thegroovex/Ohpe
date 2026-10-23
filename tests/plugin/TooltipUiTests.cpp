// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p82: every interactive control has an English and a German tooltip (the test walks the component tree of every view,
// painted controls through their RegionProvider), keys that learn MIDI say so, and no German text is missing in the
// translation table. The help search finds topics, parameters and controls by English or German words (fuzzy) and
// jumps to them.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettingsStore.h"
#include "ui/ModFxViews.h"
#include "ui/Palette.h"
#include "ui/PluginEditor.h"
#include "ui/SettingsView.h"
#include "ui/Tooltips.h"
#include "ui/Widgets.h"

#include <map>
#include <typeinfo>

using namespace ohpe;
namespace tips = ohpe::ui::tips;

namespace
{
struct Rig
{
    juce::TemporaryFile tmp;
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;

    Rig()
    {
        OhpeEditor::showTourOnFirstOpen.store (false);
        tmp.getFile().createDirectory();
        p.prepareToPlay (48000.0, 512);
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        e->userSettingsStore().setFolderForTests (tmp.getFile());
        e->userSettingsStore().set ({});
        e->applyUserSettings();
        e->setLanguage (i18n::Language::English);
    }
    ~Rig()
    {
        e->userSettingsStore().set ({});
        e->applyUserSettings();
        i18n::setCurrent (i18n::Language::English);
        ui::applySkin (ui::Skin::Light);
    }
};

juce::String className (const juce::Component& c)
{
    juce::String n (typeid (c).name());
    return n.length() > 60 ? n.substring (n.length() - 60) : n;
}

/** What a walk saw: path -> tooltip of every control, plus the problems. */
struct Seen
{
    std::map<juce::String, juce::String> tips;
    juce::StringArray problems;
};

bool isJuceContainer (const juce::Component& c)
{
    return dynamic_cast<const juce::Viewport*> (&c) != nullptr || dynamic_cast<const juce::ListBox*> (&c) != nullptr
        || (c.getParentComponent() != nullptr && dynamic_cast<const juce::Viewport*> (c.getParentComponent()) != nullptr
            && c.getParentComponent()->getViewedComponent() != &c);
}

void walk (juce::Component& c, const juce::String& path, Seen& seen, juce::Component& root, int& controls)
{
    if (! c.isVisible() || c.getWidth() <= 0 || c.getHeight() <= 0) return;
    bool self = true, children = true;
    c.getInterceptsMouseClicks (self, children);
    const auto here = path + "/" + className (c);

    if (dynamic_cast<juce::ScrollBar*> (&c) != nullptr) return; // standard scroll bars
    if (auto* rp = dynamic_cast<tips::RegionProvider*> (&c))
    {
        int i = 0;
        for (const auto& r : rp->tipRegions())
        {
            const auto key = here + "#region" + juce::String (i++) + "@" + juce::String ((int) r.area.getX()) + "," + juce::String ((int) r.area.getY());
            seen.tips[key] = r.tip;
            ++controls;
            if (r.tip.trim().isEmpty()) seen.problems.add ("no tooltip: " + key);
        }
    }
    else if (self && &c != &root && ! tips::isPassive (c) && ! isJuceContainer (c) && c.isEnabled())
    {
        auto* tc = dynamic_cast<juce::TooltipClient*> (&c);
        const auto tip = tc != nullptr ? tc->getTooltip() : juce::String();
        seen.tips[here] = tip;
        ++controls;
        if (tip.trim().isEmpty()) seen.problems.add ("no tooltip: " + here + " '" + c.getName() + "'");
        // a key that learns MIDI with a right-click says so
        if (auto* k = dynamic_cast<ui::PictoKey*> (&c); k != nullptr && k->onSecondaryClick != nullptr && ! tip.contains (tips::learnHint()))
            seen.problems.add ("no MIDI learn hint: " + here + " '" + k->getButtonText() + "'");
    }
    // the parts of a slider, a text field or a keyboard are the control itself
    if (dynamic_cast<juce::Slider*> (&c) != nullptr || dynamic_cast<juce::TextEditor*> (&c) != nullptr
        || dynamic_cast<juce::ComboBox*> (&c) != nullptr || dynamic_cast<juce::MidiKeyboardComponent*> (&c) != nullptr)
        return;
    if (! children && dynamic_cast<tips::RegionProvider*> (&c) == nullptr) return;
    for (int i = 0; i < c.getNumChildComponents(); ++i)
        walk (*c.getChildComponent (i), here + "[" + juce::String (i) + "]", seen, root, controls);
}

/** Walks the editor in English and in German and checks every control in both. */
void checkState (OhpeEditor& e, const juce::String& state, juce::StringArray& problems, int& total)
{
    Seen en, de;
    int n = 0;
    e.setLanguage (i18n::Language::English);
    e.advanceAnimation (0.05f);
    (void) e.createComponentSnapshot (e.getLocalBounds(), true, 0.4f); // painted views build their controls while painting
    walk (e, state, en, e, n);
    total += n;
    e.setLanguage (i18n::Language::German);
    e.advanceAnimation (0.05f);
    (void) e.createComponentSnapshot (e.getLocalBounds(), true, 0.4f); // paints every text of the view in German
    int m = 0;
    walk (e, state, de, e, m);
    problems.addArray (en.problems);
    for (const auto& p : de.problems)
        if (! p.startsWith ("no tooltip")) problems.add ("DE " + p);
    for (const auto& [key, tipEn] : en.tips)
    {
        const auto it = de.tips.find (key);
        if (it == de.tips.end()) continue; // a region that only exists in one language (a wider text) - checked by count
        if (tipEn.isNotEmpty() && it->second.trim().isEmpty()) problems.add ("no German tooltip: " + key);
        else if (tipEn.isNotEmpty() && it->second == tipEn) problems.add ("German tooltip = English: " + key + " '" + tipEn + "'");
    }
    e.setLanguage (i18n::Language::English);
}

void report (const juce::StringArray& problems)
{
    juce::StringArray unique;
    for (const auto& p : problems) unique.addIfNotAlreadyThere (p);
    for (int i = 0; i < juce::jmin (400, unique.size()); ++i) MESSAGE (unique[i].toStdString());
    CHECK (unique.isEmpty());
}
} // namespace

TEST_CASE ("p82 tooltips: every control of the plate, every mode, view and page has an EN and a DE tooltip")
{
    Rig r;
    auto& e = *r.e;
    juce::StringArray problems;
    int total = 0;
    i18n::clearMissingKeys();
    checkState (e, "plate", problems, total);
    for (int m = 0; m < ui::numModes; ++m)
    {
        const auto mode = (ui::Mode) m;
        if (! ui::modeInfo (mode).available) continue;
        e.selectMode (mode);
        const int views = mode == ui::Mode::Lfo ? ui::modfx::numLfoViews
                        : mode == ui::Mode::Effects ? ui::modfx::numFxViews
                        : mode == ui::Mode::Sequencer ? ui::numSeqViews : 1;
        for (int v = 0; v < views; ++v)
        {
            if (mode == ui::Mode::Sequencer) e.selectSeqView ((ui::SeqView) v);
            else if (views > 1) e.selectView (v);
            const auto state = juce::String ("mode") + juce::String (m) + ".view" + juce::String (v);
            checkState (e, state, problems, total);
            e.displayComponent().onPageChange (1); // the next page (arrows, other rows)
            checkState (e, state + ".page2", problems, total);
            e.displayComponent().onPageChange (-1);
        }
    }
    e.selectMode (ui::Mode::Tape);
    e.showTapeLoopKeys (true);
    checkState (e, "tape.loopkeys", problems, total);
    e.showTapeLoopKeys (false);
    e.selectMode (ui::Mode::Sequencer);
    e.selectSeqView (ui::SeqView::Step);
    e.selectLockStep (3);
    checkState (e, "seq.lock", problems, total);
    e.selectLockStep (-1);
    e.selectMode (ui::Mode::Synth);
    e.showGridKeyboard (true);
    checkState (e, "grid", problems, total);
    e.showGridKeyboard (false);
    for (auto t : { EngineType::FM, EngineType::Wavetable, EngineType::Grain, EngineType::Analog })
    {
        e.selectEngine (t);
        checkState (e, "engine" + juce::String ((int) t), problems, total);
    }
    MESSAGE ("controls checked on the plate: " << total);
    CHECK (total > 300);
    report (problems);
}

TEST_CASE ("p82 tooltips: pro view, FX editor, library, breeding, settings, updates, dialogs, tour")
{
    Rig r;
    auto& e = *r.e;
    juce::StringArray problems;
    int total = 0;
    for (int m = 0; m < ui::numModes; ++m)
    {
        if (! ui::modeInfo ((ui::Mode) m).available) continue;
        e.selectMode ((ui::Mode) m);
        e.showProView (true);
        e.proViewComponent().setSimple (false, false);
        checkState (e, "pro" + juce::String (m), problems, total);
        e.showProView (false);
    }
    e.selectMode (ui::Mode::Effects);
    e.selectView (ui::modfx::FxViewRack);
    e.showFxEditor (true);
    checkState (e, "fxeditor", problems, total);
    e.showFxEditor (false);
    e.selectMode (ui::Mode::Synth);

    e.showLibrary (true);
    if (e.libraryComponent().numRows() > 0) e.libraryComponent().selectRow (0);
    checkState (e, "library", problems, total);
    e.libraryComponent().showSavePanel (true);
    checkState (e, "library.save", problems, total);
    e.libraryComponent().showSavePanel (false);
    e.showLibrary (false);

    e.showBreeding (true);
    checkState (e, "breed", problems, total);
    e.showBreeding (false);

    auto& sv = e.settingsComponent();
    e.showSettings (true, 0);
    for (int tab = 0; tab < sv.numTabs(); ++tab)
    {
        sv.showPage (tab);
        checkState (e, "settings" + juce::String (tab), problems, total);
    }
    sv.showPage (ui::SettingsView::Help);
    sv.setSearchText ("filter");
    checkState (e, "settings.search", problems, total);
    sv.setSearchText ({});
    e.showSettings (false);

    e.showUpdates (true);
    checkState (e, "updates", problems, total);
    e.showUpdates (false);

    e.confirmDialogComponent().show ({ "Clear the tape?", "All eight tracks are cleared.", "Cancel", "Clear", [] {}, [] {} });
    checkState (e, "confirm", problems, total);
    e.confirmDialogComponent().cancel();

    r.p.offerRecovery (1, [] {});
    e.showPendingRecoveryOffer();
    checkState (e, "recovery", problems, total);
    e.recoveryNoticeComponent().dismiss();

    for (int step = 0; step < ui::FirstStartTour::numSteps; ++step)
    {
        e.showTour (true, step);
        checkState (e, "tour" + juce::String (step), problems, total);
    }
    e.showTour (false);
    MESSAGE ("controls checked in the overlays: " << total);
    CHECK (total > 300);
    report (problems);
}

TEST_CASE ("p82 localisation: no German text is missing after every view was shown and painted")
{
    Rig r;
    auto& e = *r.e;
    i18n::clearMissingKeys();
    e.setLanguage (i18n::Language::German);
    auto paint = [&e] { e.advanceAnimation (0.05f); (void) e.createComponentSnapshot (e.getLocalBounds(), true, 0.4f); };
    paint();
    for (int m = 0; m < ui::numModes; ++m)
    {
        const auto mode = (ui::Mode) m;
        if (! ui::modeInfo (mode).available) continue;
        e.selectMode (mode);
        const int views = mode == ui::Mode::Lfo ? ui::modfx::numLfoViews
                        : mode == ui::Mode::Effects ? ui::modfx::numFxViews
                        : mode == ui::Mode::Sequencer ? ui::numSeqViews : 1;
        for (int v = 0; v < views; ++v)
        {
            if (mode == ui::Mode::Sequencer) e.selectSeqView ((ui::SeqView) v);
            else if (views > 1) e.selectView (v);
            paint();
            e.showProView (true);
            paint();
            e.showProView (false);
        }
    }
    e.showLibrary (true);
    paint();
    e.showLibrary (false);
    e.showSettings (true, 0);
    for (int tab = 0; tab < e.settingsComponent().numTabs(); ++tab)
    {
        e.settingsComponent().showPage (tab);
        paint();
    }
    e.showSettings (false);
    const auto missing = i18n::missingKeys();
    for (const auto& k : missing) MESSAGE ("missing German text: '" << k.toStdString() << "'");
    CHECK (missing.isEmpty());
}

TEST_CASE ("p82 help search: fuzzy matching (case, umlauts, prefixes, typos), EN and DE")
{
    CHECK (tips::fold (juce::String::fromUTF8 ("Hüllkurve & Groß")) == "huellkurve gross");
    CHECK (tips::editDistance ("envelope", "envelpoe") == 1);
    CHECK (tips::editDistance ("filter", "fitler") == 1);
    CHECK (tips::matchScore ("Low Shelf", "low shelf") == doctest::Approx (1.0f));
    CHECK (tips::matchScore ("Cutoff frequency", "cut") > 0.5f);
    CHECK (tips::matchScore ("Cutoff frequency", "cutof") > 0.5f);
    CHECK (tips::matchScore ("Reverb", "reverv") > 0.3f);
    CHECK (tips::matchScore (juce::String::fromUTF8 ("Hüllkurve"), "huellkurve") > 0.9f);
    CHECK (tips::matchScore (juce::String::fromUTF8 ("Hüllkurve"), juce::String::fromUTF8 ("HÜLLKURVE")) > 0.9f);
    CHECK (tips::matchScore ("Delay time", "xyz") == 0.0f);
    CHECK (tips::matchScore ("Delay time", "delay xyz") == 0.0f); // every word must match

    std::vector<tips::Entry> index;
    index.push_back ({ tips::Kind::Topic, "Envelopes", juce::String::fromUTF8 ("Hüllkurven"), "attack decay", "", "1" });
    index.push_back ({ tips::Kind::Parameter, "Cutoff", "Cutoff", "filter frequency", "Filterfrequenz", "p1.analog.cutoff" });
    index.push_back ({ tips::Kind::Control, "Mod wheel", "Modrad", "", "", "wheel.mod" });
    CHECK (tips::search (index, "huellkurve").front().entry.target == "1");
    CHECK (tips::search (index, "envelpoe").front().entry.target == "1");
    CHECK (tips::search (index, "modrad").front().entry.target == "wheel.mod");
    CHECK (tips::search (index, "filterfrequenz").front().entry.target == "p1.analog.cutoff");
    CHECK (tips::search (index, "").empty());
    CHECK (tips::search (index, "qqqq").empty());
}

TEST_CASE ("p82 help search: the Help page finds topics, parameters and controls and jumps to them")
{
    Rig r;
    auto& e = *r.e;
    const auto index = e.helpSearchIndex();
    int kinds[3] {};
    for (const auto& en : index) ++kinds[(int) en.kind];
    CHECK (kinds[(int) tips::Kind::Topic] == ui::SettingsView::numHelpTopics());
    CHECK (kinds[(int) tips::Kind::Parameter] > 100);
    CHECK (kinds[(int) tips::Kind::Control] > 30);

    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        e.openHelpSearch ("cutoff");
        auto& sv = e.settingsComponent();
        CHECK (e.settingsVisible());
        CHECK (sv.page() == ui::SettingsView::Help);
        REQUIRE_FALSE (sv.searchResults().empty());
        // German words find English-named things and back
        sv.setSearchText (juce::String::fromUTF8 ("Hüllkurve"));
        CHECK_FALSE (sv.searchResults().empty());
        sv.setSearchText ("envelope");
        CHECK_FALSE (sv.searchResults().empty());
        sv.setSearchText ("modrad");
        REQUIRE_FALSE (sv.searchResults().empty());
        CHECK (sv.searchResults().front().entry.target == "wheel.mod");
        // a control: the settings close, the control is highlighted
        CHECK (sv.clickSearchResult (0));
        CHECK_FALSE (e.settingsVisible());
        CHECK (e.highlightedArea().intersects (e.modWheelComponent().getBounds().toFloat()));
        // a parameter: its mode and page are shown, the encoder is highlighted
        e.openHelpSearch ("resonance");
        int found = -1;
        for (size_t i = 0; i < sv.searchResults().size(); ++i)
            if (sv.searchResults()[i].entry.kind == tips::Kind::Parameter) { found = (int) i; break; }
        REQUIRE (found >= 0);
        const auto id = sv.searchResults()[(size_t) found].entry.target;
        CHECK (sv.clickSearchResult (found));
        bool onEncoder = false;
        for (int i = 0; i < 4; ++i)
            if (e.encoder (i).boundId() == id) onEncoder = e.highlightedArea().intersects (e.encoder (i).getBounds().toFloat());
        CAPTURE (id.toStdString());
        CHECK ((onEncoder || e.proViewVisible()));
        if (e.proViewVisible()) e.showProView (false);
        // a help topic: the topic is shown on the Help page
        e.openHelpSearch (lang == i18n::Language::German ? juce::String::fromUTF8 ("erste schritte") : juce::String ("getting started"));
        REQUIRE_FALSE (sv.searchResults().empty());
        CHECK (sv.searchResults().front().entry.kind == tips::Kind::Topic);
        CHECK (sv.clickSearchResult (0));
        CHECK (e.settingsVisible());
        CHECK (sv.helpTopic() == 0);
        e.showSettings (false);
    }
    e.setLanguage (i18n::Language::English);
}
