// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.3 (package E): preset library (factory bank, user folder, favourites, prev / next, preview),
// remote controls (MIDI-mapped triggers), editor: direct manipulation in the display, fine
// control, pro view, part selector, step locks, tape and drum pages.

#include <doctest/doctest.h>

#include "OhpePresets.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
struct TempFolder
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("ohpe-test-presets-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    TempFolder() { dir.createDirectory(); }
    ~TempFolder() { dir.deleteRecursively(); }
};

void process (OhpeProcessor& p, int blocks, const juce::MidiBuffer& first = {})
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi = first;
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos, juce::ModifierKeys mods)
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPos, now, 1, pos != downPos);
}

int countFactory (const std::string& tag)
{
    int n = 0;
    for (int i = 0; i < OhpePresets::namedResourceListSize; ++i)
    {
        int size = 0;
        const char* data = OhpePresets::getNamedResource (OhpePresets::namedResourceList[i], size);
        preset::Preset p;
        if (data != nullptr && preset::fromJson (std::string (data, (size_t) size), p))
            if (tag.empty() || std::find (p.meta.tags.begin(), p.meta.tags.end(), tag) != p.meta.tags.end()) ++n;
    }
    return n;
}
} // namespace

TEST_CASE ("library: the factory bank is embedded and every factory preset is valid")
{
    REQUIRE (OhpePresets::namedResourceListSize >= 8);
    OhpeProcessor p;
    TempFolder tmp;
    auto& pm = p.presets();
    pm.setUserFolder (tmp.dir);
    CHECK ((int) pm.entries().size() == OhpePresets::namedResourceListSize);
    CHECK (countFactory ({}) == OhpePresets::namedResourceListSize);
    const int before = p.core().params().size();
    for (int i = 0; i < (int) pm.entries().size(); ++i)
    {
        const auto& e = pm.entries()[(size_t) i];
        CAPTURE (e.meta.name);
        CHECK (e.factory);
        CHECK (! e.meta.name.empty());
        CHECK (! e.meta.tags.empty());
        for (const auto& t : e.meta.tags) CHECK (preset::isCategory (t)); // factory presets use the fixed categories
        preset::Preset parsed;
        REQUIRE (preset::fromJson (pm.read (e), parsed));
        // every parameter a factory preset sets exists (no typos)
        for (const auto& [id, v] : parsed.params)
        {
            const auto full = parsed.meta.kind == preset::Kind::Part ? "p1." + id : id;
            CAPTURE (full);
            CHECK (p.core().params().indexOf (full) >= 0);
        }
        CHECK (pm.load (i, 0));
    }
    CHECK (p.core().params().size() == before); // presets never change the parameter list
}

TEST_CASE ("library: save / load round-trip in the user folder, favourites, rating, delete")
{
    OhpeProcessor p;
    TempFolder tmp;
    auto& pm = p.presets();
    pm.setUserFolder (tmp.dir);
    const int factory = (int) pm.entries().size();

    p.parameter ("p2.engine")->setValueNotifyingHost (p.parameter ("p2.engine")->convertTo0to1 ((float) EngineType::Cluster));
    p.parameter ("p2.cluster.detune")->setValueNotifyingHost (0.9f);
    preset::Meta meta;
    meta.name = "My Cluster";
    meta.author = "Tester";
    meta.tags = { "Pads", "wide" };
    const auto file = pm.save (meta, false, 1);
    REQUIRE (file.existsAsFile());
    CHECK (file.getParentDirectory().getFileName() == "Parts");
    CHECK ((int) pm.entries().size() == factory + 1);
    CHECK (pm.author() == "Tester");
    const int idx = pm.current();
    REQUIRE (idx >= 0);
    CHECK_FALSE (pm.entries()[(size_t) idx].factory);
    CHECK (pm.entries()[(size_t) idx].meta.engine == "cluster");

    // load it into part 4: the cluster sound arrives there, host parameters follow
    CHECK (pm.load (idx, 3));
    CHECK ((int) std::lround (p.core().params().get ("p4.engine")) == (int) EngineType::Cluster);
    CHECK (p.parameter ("p4.cluster.detune")->getValue() == doctest::Approx (0.9f).epsilon (0.01));

    // favourites and rating persist in library.json
    pm.setFavourite (idx, true);
    pm.setRating (idx, 4);
    {
        OhpeProcessor q;
        q.presets().setUserFolder (tmp.dir);
        const auto& e = q.presets().entries();
        auto it = std::find_if (e.begin(), e.end(), [] (const preset::Entry& x) { return x.meta.name == "My Cluster"; });
        REQUIRE (it != e.end());
        CHECK (it->favourite);
        CHECK (it->rating == 4);
        CHECK (q.presets().author() == "Tester");
    }
    // a second save with the same name does not overwrite
    const auto file2 = pm.save (meta, false, 1);
    CHECK (file2 != file);
    // project presets go to Projects/
    meta.name = "Song";
    const auto proj = pm.save (meta, true);
    CHECK (proj.getParentDirectory().getFileName() == "Projects");
    // filter by tag / engine, step through the view
    pm.filter = {};
    pm.filter.tags = { "wide" };
    CHECK (pm.view().size() == 3);
    pm.filter.engine = "cluster";
    pm.filter.kind = 0;
    CHECK (pm.view().size() == 2);
    pm.filter = {};
    pm.filter.bank = preset::Bank::Factory;
    CHECK (pm.step (1, 0));
    CHECK (pm.entries()[(size_t) pm.current()].factory);
    // delete (user presets only)
    pm.filter = {};
    int user = -1;
    for (int i = 0; i < (int) pm.entries().size(); ++i)
        if (! pm.entries()[(size_t) i].factory) user = i;
    const int total = (int) pm.entries().size();
    CHECK_FALSE (pm.remove (0)); // factory
    CHECK (pm.remove (user));
    CHECK ((int) pm.entries().size() == total - 1);
}

TEST_CASE ("library: preview and cancel restore the sound from before")
{
    OhpeProcessor p;
    TempFolder tmp;
    auto& pm = p.presets();
    pm.setUserFolder (tmp.dir);
    p.parameter ("p1.analog.cutoff")->setValueNotifyingHost (0.123f);
    const float before = p.parameter ("p1.analog.cutoff")->getValue();
    pm.beginPreview (0);
    CHECK (pm.previewing());
    pm.filter = {};
    pm.filter.engine = "analog";
    REQUIRE (pm.step (1, 0));
    CHECK (std::abs (p.parameter ("p1.analog.cutoff")->getValue() - before) > 0.01f);
    pm.revertPreview();
    CHECK_FALSE (pm.previewing());
    CHECK (p.parameter ("p1.analog.cutoff")->getValue() == doctest::Approx (before).epsilon (0.001));
}

TEST_CASE ("remote controls: MIDI-mapped next preset, edit part and tape edits")
{
    OhpeProcessor p;
    TempFolder tmp;
    p.presets().setUserFolder (tmp.dir);
    p.prepareToPlay (48000.0, 256);
    MidiMapping m;
    m.paramId = Core::Ids::libNext;
    m.number = 30;
    m.channel = 1;
    p.core().midiMap().add (m);
    MidiMapping part;
    part.paramId = Core::Ids::editPart;
    part.number = 31;
    part.channel = 1;
    p.core().midiMap().add (part);
    p.core().midiMapChanged();
    CHECK (p.presets().current() == -1);
    juce::MidiBuffer press;
    press.addEvent (juce::MidiMessage::controllerEvent (1, 30, 127), 0);
    press.addEvent (juce::MidiMessage::controllerEvent (1, 30, 0), 10); // press + release in one block
    process (p, 2, press);
    p.pollRemoteControls();
    CHECK (p.presets().current() >= 0);
    const int first = p.presets().current();
    process (p, 2, press);
    p.pollRemoteControls();
    CHECK (p.presets().current() != first);
    // CC 31 at 127 selects part 4
    juce::MidiBuffer sel;
    sel.addEvent (juce::MidiMessage::controllerEvent (1, 31, 127), 0);
    process (p, 2, sel);
    CHECK ((int) std::lround (p.core().params().get (Core::Ids::editPart)) == 4);
    CHECK (p.presets().editPart() == 3);
    // tape edits run on the edit track; undo on an empty tape is refused but safe
    CHECK_FALSE (p.runTapeEdit (Core::Ids::tapeUndo));
    CHECK (p.runTapeEdit (Core::Ids::tapeRtz));
    // host / UI rising edge on a trigger
    const int libBefore = p.libraryToggleRequests();
    p.parameter (Core::Ids::libOpen)->setValueNotifyingHost (1.0f);
    p.pollRemoteControls();
    CHECK (p.libraryToggleRequests() == libBefore + 1);
    p.pollRemoteControls();
    CHECK (p.libraryToggleRequests() == libBefore + 1); // held: no repeat
}

TEST_CASE ("editor 0.3: display bars drag with fine control and reset on double-click")
{
    CHECK (ui::dragToNormalised (0.5f, 100.0f, 200.0f, false) == doctest::Approx (1.0f));
    CHECK (ui::dragToNormalised (0.5f, 100.0f, 200.0f, true) == doctest::Approx (0.55f));
    CHECK (ui::dragToNormalised (0.5f, -1000.0f, 200.0f, false) == 0.0f);
    ui::BarDrag d;
    d.begin (0.2f, { 0, 0 }, false);
    CHECK (d.update ({ 50, 0 }, 100.0f, false) == doctest::Approx (0.7f));
    CHECK (d.update ({ 60, 0 }, 100.0f, true) == doctest::Approx (0.7f)); // switching to fine never jumps
    CHECK (d.update ({ 110, 0 }, 100.0f, true) == doctest::Approx (0.75f));
    CHECK (d.update ({ 110, -10 }, 100.0f, true) == doctest::Approx (0.76f)); // up = more

    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectEngine (EngineType::Analog);
    auto& disp = e.displayComponent();
    auto* prm = p.parameter ("p1.analog.shape");
    prm->setValueNotifyingHost (0.5f);
    const auto bar = disp.barArea (0);
    // drag from the middle of the row's text (not the bar): relative, no jump
    const auto start = juce::Point<float> (bar.getCentreX(), disp.rowArea (0).getY() + 2.0f);
    disp.mouseDown (mouseEvent (disp, start, start, {}));
    CHECK (prm->getValue() == doctest::Approx (0.5f));
    disp.mouseDrag (mouseEvent (disp, start.translated (bar.getWidth() * 0.2f, 0), start, {}));
    disp.mouseUp (mouseEvent (disp, start.translated (bar.getWidth() * 0.2f, 0), start, {}));
    CHECK (prm->getValue() == doctest::Approx (0.7f).epsilon (0.02));
    // Shift: a tenth of the speed
    disp.mouseDown (mouseEvent (disp, start, start, juce::ModifierKeys::shiftModifier));
    disp.mouseDrag (mouseEvent (disp, start.translated (bar.getWidth() * 0.2f, 0), start, juce::ModifierKeys::shiftModifier));
    disp.mouseUp (mouseEvent (disp, start.translated (bar.getWidth() * 0.2f, 0), start, juce::ModifierKeys::shiftModifier));
    CHECK (prm->getValue() == doctest::Approx (0.72f).epsilon (0.02));
    // a click on the bar jumps there
    const auto onBar = juce::Point<float> (bar.getX() + bar.getWidth() * 0.25f, bar.getCentreY());
    disp.mouseDown (mouseEvent (disp, onBar, onBar, {}));
    disp.mouseUp (mouseEvent (disp, onBar, onBar, {}));
    CHECK (prm->getValue() == doctest::Approx (0.25f).epsilon (0.02));
    // double-click resets to the default
    disp.mouseDoubleClick (mouseEvent (disp, start, start, {}));
    CHECK (prm->getValue() == doctest::Approx (prm->getDefaultValue()));
}

TEST_CASE ("editor 0.3: part selector, pages of every mode and sequencer view, pro view groups")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (2);
    CHECK ((int) std::lround (p.core().params().get (Core::Ids::editPart)) == 3);
    e.selectEngine (EngineType::Wavetable);
    CHECK (e.encoder (0).boundId() == "p3.wavetable.pos");
    CHECK ((int) std::lround (p.core().params().get ("p3.engine")) == (int) EngineType::Wavetable);
    CHECK ((int) std::lround (p.core().params().get ("p1.engine")) != (int) EngineType::Wavetable);
    e.selectMode (ui::Mode::Envelope);
    CHECK (e.encoder (0).boundId() == "p3.amp.a");
    for (int m = 0; m < ui::numModes; ++m)
        for (int v = 0; v < (m == (int) ui::Mode::Sequencer ? ui::numSeqViews : 1); ++v)
        {
            ui::PageContext ctx;
            ctx.part = 2;
            ctx.seqView = (ui::SeqView) v;
            const auto ids = ui::modeParams ((ui::Mode) m, p.core(), ctx);
            CHECK (ui::pageTitles ((ui::Mode) m, p.core(), ctx).size() == (size_t) ui::numPages (ids));
            for (const auto& id : ids)
            {
                CAPTURE (id);
                if (! id.empty()) CHECK (p.parameter (id) != nullptr);
            }
            for (const auto& g : ui::proGroups ((ui::Mode) m, p.core(), ctx))
                for (const auto& id : g.ids)
                {
                    CAPTURE (id);
                    CHECK (p.parameter (id) != nullptr);
                }
        }
    e.showProView (true);
    CHECK (e.proViewVisible());
    e.selectMode (ui::Mode::Drum);
    CHECK (e.proViewComponent().numBars() == 2 + 24 * 8);
    e.selectMode (ui::Mode::Tape);
    CHECK (e.proViewComponent().numBars() == 6 + 3 + 4 + 8 * 4);
    e.setSize (1800, 1200);
    CHECK (e.proViewComponent().numColumns() >= 4);
    auto img = e.createComponentSnapshot (e.getLocalBounds());
    CHECK (img.getWidth() == 1800);
    // a pro view bar is a control too
    auto& bar = e.proViewComponent().bar (0);
    auto* prm = p.parameter (bar.paramId());
    const auto bb = bar.barBounds();
    const auto at = juce::Point<float> (bb.getX() + bb.getWidth() * 0.8f, bb.getCentreY());
    bar.mouseDown (mouseEvent (bar, at, at, {}));
    bar.mouseUp (mouseEvent (bar, at, at, {}));
    CHECK (prm->getValue() == doctest::Approx (0.8f).epsilon (0.03));
    e.showProView (false);
    CHECK_FALSE (e.proViewVisible());
}

TEST_CASE ("editor 0.3: step selection edits the step and locks parameters from any mode")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& seq = p.core().stepSequencer();
    seq.toggleStep (5, 60);
    e.selectMode (ui::Mode::Sequencer);
    e.selectLockStep (5);
    REQUIRE (e.encoder (0).boundVirtual() != nullptr);
    e.encoder (0).boundVirtual()->set (64.0f / 127.0f); // note
    CHECK (seq.getStep (5).note == 64);
    e.encoder (1).boundVirtual()->set (0.0f);           // velocity
    CHECK (seq.getStep (5).velocity == 1);

    e.selectEngine (EngineType::Analog); // Synth mode, step stays selected
    CHECK (e.lockStep() == 5);
    auto v = e.encoder (2).boundVirtual();
    REQUIRE (v != nullptr);
    CHECK (v->paramId == "p1.analog.cutoff");
    CHECK_FALSE (v->locked);
    v->set (0.3f);
    auto st = seq.getStep (5);
    const int idx = p.core().params().indexOf ("p1.analog.cutoff");
    CHECK (st.locks[0].paramIndex == idx);
    CHECK (st.locks[0].normValue == doctest::Approx (0.3f));
    CHECK (v->locked);
    // the parameter itself is untouched
    CHECK (p.parameter ("p1.analog.cutoff")->getValue() != doctest::Approx (0.3f));
    // four locks at most
    for (int k = 0; k < 4; ++k)
        if (auto w = e.encoder (k).boundVirtual()) w->set (0.6f);
    e.selectMode (ui::Mode::Envelope);
    e.encoder (0).boundVirtual()->set (0.5f); // 5th lock: refused
    int used = 0;
    for (const auto& l : seq.getStep (5).locks) used += l.paramIndex >= 0 ? 1 : 0;
    CHECK (used == 4);
    // double-click (reset) removes a lock
    e.selectEngine (EngineType::Analog);
    e.encoder (2).boundVirtual()->reset();
    used = 0;
    for (const auto& l : seq.getStep (5).locks) used += l.paramIndex >= 0 ? 1 : 0;
    CHECK (used == 3);
    // leaving lock editing binds the parameters again
    e.selectLockStep (-1);
    CHECK (e.encoder (2).boundVirtual() == nullptr);
    CHECK (e.encoder (2).boundId() == "p1.analog.cutoff");
    e.selectMode (ui::Mode::Sequencer);
    e.advanceAnimation (0.2f);
    auto img = e.createComponentSnapshot (e.getLocalBounds());
    CHECK (img.isValid());
}

TEST_CASE ("editor 0.3: tape and drum pages, library browser")
{
    OhpeProcessor p;
    TempFolder tmp;
    p.presets().setUserFolder (tmp.dir);
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectMode (ui::Mode::Tape);
    CHECK (e.encoder (0).boundId() == "tape.play");
    e.selectTapeTrack (3);
    CHECK ((int) std::lround (p.core().params().get (Core::Ids::tapeEditTrack)) == 4);
    CHECK (ui::modeParams (ui::Mode::Tape, p.core(), { 0, 0, ui::SeqView::Step, 3, 0 })[4] == "tape.tr4.arm");
    e.softKey (26); // return to zero
    e.advanceAnimation (0.6f);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());

    e.selectMode (ui::Mode::Drum);
    e.selectDrumPad (5);
    CHECK (e.encoder (0).boundId() == "drum.pad6.type");
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());

    for (auto m : { ui::Mode::Sequencer })
        for (int v = 0; v < ui::numSeqViews; ++v)
        {
            e.selectSeqView ((ui::SeqView) v);
            CHECK (e.currentMode() == m);
            CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        }

    // library: open, filter by tag, preview, cancel
    const float before = p.parameter ("p1.engine")->getValue();
    e.showLibrary (true);
    auto& lib = e.libraryComponent();
    CHECK (lib.isOpen());
    CHECK (lib.numRows() == OhpePresets::namedResourceListSize);
    lib.toggleTag ("Bass");
    CHECK (lib.numRows() == countFactory ("Bass"));
    lib.toggleTag ("Bass");
    lib.setSearchText ("slap"); // one factory preset: Slap FM Bass
    REQUIRE (lib.numRows() == 1);
    lib.selectRow (0);
    CHECK ((int) std::lround (p.core().params().get ("p1.engine")) == (int) EngineType::FM);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    lib.close (false); // cancel: back to the sound from before
    CHECK (p.parameter ("p1.engine")->getValue() == doctest::Approx (before));
    CHECK_FALSE (e.libraryVisible());
    // save through the browser
    e.showLibrary (true);
    lib.setSearchText ({});
    lib.showSavePanel (true);
    lib.setSaveFields ("Browser Save", "groovex", { "Keys" }, false);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    CHECK (lib.commitSave());
    CHECK (tmp.dir.getChildFile ("Parts").getChildFile ("Browser Save.ohpepreset").existsAsFile());
    e.showLibrary (false);
    e.setLanguage (i18n::Language::German);
    e.showLibrary (true);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.showLibrary (false);
    e.setLanguage (i18n::Language::English);
}

// ------------------------------------------------------------------ 0.7: arp presets
namespace
{
int entryIndex (const PresetManager& pm, const std::string& name)
{
    for (int i = 0; i < (int) pm.entries().size(); ++i)
        if (pm.entries()[(size_t) i].meta.name == name) return i;
    return -1;
}
float plain (OhpeProcessor& p, const char* id) { return p.core().params().get (id); }
} // namespace

TEST_CASE ("library: Arp presets switch the arpeggiator on for their part; plain presets leave it alone")
{
    OhpeProcessor p;
    TempFolder tmp;
    auto& pm = p.presets();
    pm.setUserFolder (tmp.dir);
    const int arp = entryIndex (pm, "Analog Arp"), plainSound = entryIndex (pm, "Round Sub");
    REQUIRE (arp >= 0);
    REQUIRE (plainSound >= 0);
    REQUIRE (plain (p, "seq.arp.on") == 0.0f);

    // every factory part preset tagged Arp carries an arp block
    for (const auto& e : pm.entries())
    {
        if (e.meta.kind != preset::Kind::Part || std::find (e.meta.tags.begin(), e.meta.tags.end(), "Arp") == e.meta.tags.end()) continue;
        CAPTURE (e.meta.name);
        preset::Preset parsed;
        REQUIRE (preset::fromJson (pm.read (e), parsed));
        CHECK (parsed.arp.has_value());
    }

    // "Analog Arp" into part 1: arp on, aimed at part 1, host parameters follow
    REQUIRE (pm.load (arp, 0));
    CHECK (plain (p, "seq.arp.on") == 1.0f);
    CHECK ((int) plain (p, "seq.arp.part") == 1);
    CHECK (plain (p, "seq.arp.latch") == 1.0f);
    CHECK (p.parameter ("seq.arp.on")->getValue() == doctest::Approx (1.0f));
    CHECK (p.parameter ("seq.arp.part")->getValue() == doctest::Approx (0.0f)); // 1 of 1..4
    CHECK (pm.arpPresetPart() == 0);

    // a plain preset into another part: the arp stays
    REQUIRE (pm.load (plainSound, 1));
    CHECK (plain (p, "seq.arp.on") == 1.0f);
    CHECK ((int) plain (p, "seq.arp.part") == 1);

    // a plain preset into the arp's part: the arp the preset switched on goes off again
    REQUIRE (pm.load (plainSound, 0));
    CHECK (plain (p, "seq.arp.on") == 0.0f);
    CHECK (pm.arpPresetPart() == -1);

    // ... but not once the user changed the arp: then it is the user's arp
    REQUIRE (pm.load (arp, 2));
    CHECK ((int) plain (p, "seq.arp.part") == 3);
    p.parameter ("seq.arp.mode")->setValueNotifyingHost (p.parameter ("seq.arp.mode")->convertTo0to1 (1.0f));
    p.editRegistry ([] (Core&) {}); // host value -> registry
    CHECK (pm.arpPresetPart() == -1);
    REQUIRE (pm.load (plainSound, 2));
    CHECK (plain (p, "seq.arp.on") == 1.0f);

    // an arp switched on by hand is never touched by plain presets
    REQUIRE (pm.load (plainSound, 3));
    p.parameter ("seq.arp.part")->setValueNotifyingHost (p.parameter ("seq.arp.part")->convertTo0to1 (4.0f));
    REQUIRE (pm.load (plainSound, 3));
    CHECK (plain (p, "seq.arp.on") == 1.0f);
    CHECK ((int) plain (p, "seq.arp.part") == 4);

    // saving a part the arp plays keeps its arp in the user preset
    REQUIRE (pm.load (arp, 1));
    preset::Meta meta;
    meta.name = "My Arp";
    const auto file = pm.save (meta, false, 1);
    REQUIRE (file.existsAsFile());
    preset::Preset saved;
    REQUIRE (preset::fromJson (file.loadFileAsString().toStdString(), saved));
    REQUIRE (saved.arp.has_value());
    CHECK (saved.arp->at ("on") == 1.0f);

    // preview + revert restores the arp as it was
    pm.load (plainSound, 1); // arp off again (preset-owned)
    REQUIRE (plain (p, "seq.arp.on") == 0.0f);
    pm.beginPreview (0);
    pm.load (arp, 0);
    CHECK (plain (p, "seq.arp.on") == 1.0f);
    pm.revertPreview();
    CHECK (plain (p, "seq.arp.on") == 0.0f);
}

// ------------------------------------------------------------------ 0.7: user presets show under "User"
namespace
{
void setEnv (const char* name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s (name, value.toRawUTF8());
#else
    if (value.isEmpty()) unsetenv (name);
    else setenv (name, value.toRawUTF8(), 1);
#endif
}

int userCount (const PresetManager& pm)
{
    int n = 0;
    for (const auto& e : pm.entries()) n += e.factory ? 0 : 1;
    return n;
}
} // namespace

TEST_CASE ("library: a sound saved under a new name shows under User at once, with filters set, and after a restart")
{
    TempFolder tmp;
    const auto previousDir = juce::SystemStats::getEnvironmentVariable ("OHPE_PRESET_DIR", {});
    setEnv ("OHPE_PRESET_DIR", tmp.dir.getFullPathName()); // the default user folder, as after a restart
    {
        OhpeProcessor p;
        REQUIRE (p.presets().userFolder() == tmp.dir);
        p.prepareToPlay (48000.0, 256);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
        auto& pm = p.presets();
        e.showLibrary (true);
        auto& lib = e.libraryComponent();

        // browse the Bass category with a search, pick a factory sound and edit it
        lib.toggleTag ("Bass");
        lib.setSearchText ("slap");
        REQUIRE (lib.numRows() == 1);
        lib.selectRow (0);
        const int factory = pm.current();
        REQUIRE (factory >= 0);
        REQUIRE (pm.entries()[(size_t) factory].factory);
        p.parameter ("p1.fm.index")->setValueNotifyingHost (0.77f);

        // the save panel starts with the loaded sound's name and tags
        lib.showSavePanel (true);
        CHECK (lib.saveTagSelection().contains ("Bass"));

        // saved under a new name and without any tag (the case from the bug report)
        lib.setSaveFields ("My Own Bass", "Tester", {}, false);
        REQUIRE (lib.commitSave());
        CHECK (lib.statusMessage().contains ("My Own Bass"));
        const int saved = pm.current();
        REQUIRE (saved >= 0);
        CHECK_FALSE (pm.entries()[(size_t) saved].factory);
        CHECK (pm.entries()[(size_t) saved].meta.name == "My Own Bass");
        // ... it is in the list right away, selected, and the bank shows the user presets
        CHECK (pm.filter.bank == preset::Bank::User);
        CHECK (pm.filter.tags.empty());
        CHECK (pm.filter.text.empty());
        REQUIRE (lib.rowOfEntry (saved) >= 0);
        CHECK (lib.selectedListRow() == lib.rowOfEntry (saved));
        CHECK (lib.numRows() == 1);
        // the User / Eigene bank chip on its own also lists it
        pm.filter = {};
        pm.filter.bank = preset::Bank::User;
        lib.setSearchText ({});
        CHECK (lib.numRows() == 1);
        CHECK (lib.entryAtRow (0) == saved);
        // the panel takes the tags of the loaded user preset (none here)
        lib.showSavePanel (true);
        CHECK (lib.saveTagSelection().isEmpty());
        lib.showSavePanel (false);

        // saving with a filter that does not hide it leaves the filter alone
        pm.filter = {};
        pm.filter.bank = preset::Bank::User;
        pm.filter.kind = 0;
        lib.showSavePanel (true);
        lib.setSaveFields ("Second Sound", "Tester", { "Keys" }, false);
        REQUIRE (lib.commitSave());
        CHECK (pm.filter.kind == 0);
        CHECK (lib.numRows() == 2);
        CHECK (lib.rowOfEntry (pm.current()) >= 0);

        // name clash: replace overwrites the file (favourite stays), keep both picks a free name
        pm.setFavourite (pm.current(), true);
        const auto first = tmp.dir.getChildFile ("Parts").getChildFile ("Second Sound.ohpepreset");
        REQUIRE (first.existsAsFile());
        CHECK (pm.userFileFor ("Second Sound", false) == first);
        lib.showSavePanel (true);
        lib.setSaveFields ("Second Sound", "Tester", { "Pads" }, false);
        REQUIRE (lib.commitSave (ui::LibraryBrowser::SaveConflict::Replace));
        CHECK (userCount (pm) == 2);
        CHECK (pm.entries()[(size_t) pm.current()].meta.tags == std::vector<std::string> { "Pads" });
        CHECK (pm.entries()[(size_t) pm.current()].favourite);
        lib.showSavePanel (true);
        lib.setSaveFields ("Second Sound", "Tester", { "Pads" }, false);
        REQUIRE (lib.commitSave (ui::LibraryBrowser::SaveConflict::KeepBoth));
        CHECK (userCount (pm) == 3);
        CHECK (pm.entries()[(size_t) pm.current()].meta.name == "Second Sound 2");
        CHECK (tmp.dir.getChildFile ("Parts").getChildFile ("Second Sound 2.ohpepreset").existsAsFile());
        CHECK (lib.rowOfEntry (pm.current()) >= 0);
        // keep both with a name at the file name limit still finds a free name
        const std::string longName (90, 'x');
        lib.showSavePanel (true);
        lib.setSaveFields (juce::String (longName), "Tester", {}, false);
        REQUIRE (lib.commitSave());
        lib.showSavePanel (true);
        lib.setSaveFields (juce::String (longName), "Tester", {}, false);
        REQUIRE (lib.commitSave (ui::LibraryBrowser::SaveConflict::KeepBoth));
        CHECK (userCount (pm) == 5);
        CHECK (pm.entries()[(size_t) pm.current()].meta.name == std::string (72, 'x') + " 2");
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        e.showLibrary (false);
    }
    {
        // restart: a new instance finds them in the default folder, under User
        OhpeProcessor q;
        auto& pm = q.presets();
        CHECK (userCount (pm) == 5);
        pm.filter.bank = preset::Bank::User;
        CHECK (pm.view().size() == 5);
        CHECK (entryIndex (pm, "My Own Bass") >= 0);
        CHECK (pm.author() == "Tester");
        const int idx = entryIndex (pm, "Second Sound");
        REQUIRE (idx >= 0);
        CHECK (pm.entries()[(size_t) idx].favourite);
    }
    setEnv ("OHPE_PRESET_DIR", previousDir);
}

TEST_CASE ("library: the default user folder")
{
    const auto f = PresetManager::defaultUserFolder();
    CHECK (f.getFileName() == "Presets");
    CHECK (f.getParentDirectory().getFileName() == "Ohpe");
#if JUCE_MAC
    CHECK (f.getFullPathName().contains ("/Library/Application Support/groovex/Ohpe/Presets"));
#endif
}
