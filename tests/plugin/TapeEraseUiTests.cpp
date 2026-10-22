// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p38: the tape page's CLEAR TRACK / CLEAR ALL / SAVE TAPE / LOAD TAPE keys, the confirmation before the whole tape is
// cleared or replaced, the MIDI triggers "tape.edit.clear" / "tape.edit.clearall" and the ".ohpetape" files.

#include <doctest/doctest.h>

#include <cstdlib>

#include "plugin/PluginProcessor.h"
#include "tape/TapeFile.h"
#include "ui/PluginEditor.h"

using namespace ohpe;
using T = OhpeProcessor::TransportKey;

namespace
{
struct TempFolder
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("ohpe-test-tapeerase-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
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

void setHost (OhpeProcessor& p, const juce::String& id, float plainValue)
{
    auto* q = p.parameter (id);
    REQUIRE (q != nullptr);
    q->setValueNotifyingHost (q->convertTo0to1 (plainValue));
}

/** One-touch records a note on `track` (0-based) as the edit track, then stops and disarms. */
void recordOn (OhpeProcessor& p, int track)
{
    setHost (p, Core::Ids::tapeEditTrack, (float) (track + 1));
    process (p, 1);
    p.runTransport (T::Record);
    REQUIRE (p.tapeRecording());
    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    p.core().tape().housekeeping();
    for (int k = 0; k < 4; ++k)
    {
        process (p, 10, k == 0 ? note : juce::MidiBuffer());
        p.core().tape().housekeeping();
    }
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    process (p, 1, off);
    p.runTransport (T::Record); // punch out
    process (p, 4);
    p.runTransport (T::Stop);
    p.runTransport (T::Stop); // to the start
    setHost (p, "tape.tr" + juce::String (track + 1) + ".arm", 0.0f);
    process (p, 4);
    p.core().tape().housekeeping();
    REQUIRE (p.core().tape().contentEndFrames (track) > 0);
}
} // namespace

TEST_CASE ("tape erase: twelve keys in two rows; CLEAR TRACK at once, CLEAR ALL after the confirmation, both undoable")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    recordOn (p, 0);
    recordOn (p, 1);
    auto& tape = p.core().tape();

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();
    e.setLanguage (i18n::Language::English);
    e.selectMode (ui::Mode::Tape);
    CHECK (d.softKeyRows() == 2);
    for (int k = 0; k < 14; ++k) CHECK_FALSE (d.softKeyArea (k).isEmpty()); // p67: + CAPTURE (row 1), MEMORY (row 2)
    CHECK (d.softKeyArea (14).isEmpty());
    CHECK (d.softKeyArea (7).getY() > d.softKeyArea (0).getBottom() - 1.0f); // CLEAR TRACK opens the second row
    CHECK (d.softKeyArea (13).getRight() == doctest::Approx (d.softKeyArea (6).getRight()));
    for (int t = 0; t < TapeMachine::numTracks; ++t) // the lanes stay above the keys
        CHECK (d.trackLane (t).getBottom() <= d.softKeyArea (0).getY());

    // CLEAR TRACK: the edit track (2) only, no question, one undo step
    e.selectTapeTrack (1);
    const int steps = tape.undoSteps();
    e.softKey (130);
    CHECK_FALSE (e.confirmDialogComponent().isOpen());
    CHECK (tape.contentEndFrames (1) == 0);
    CHECK (tape.contentEndFrames (0) > 0);
    CHECK (tape.undoSteps() == steps + 1);
    REQUIRE (tape.undo());
    CHECK (tape.contentEndFrames (1) > 0);

    // CLEAR ALL: asks first; CANCEL (and Esc) leave the tape alone
    e.softKey (131);
    auto& dlg = e.confirmDialogComponent();
    REQUIRE (dlg.isOpen());
    CHECK (dlg.focusedKey() == 0); // CANCEL is the default
    CHECK (dlg.options().confirmText == "Delete");
    dlg.cancel();
    CHECK_FALSE (dlg.isOpen());
    CHECK (tape.contentEndFrames() > 0);
    e.softKey (131);
    REQUIRE (dlg.isOpen());
    dlg.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    CHECK_FALSE (dlg.isOpen());
    e.softKey (131);
    dlg.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)); // Return presses the focused key: CANCEL
    CHECK_FALSE (dlg.isOpen());
    CHECK (tape.contentEndFrames() > 0);
    // confirmed: every track empty, still undoable
    e.softKey (131);
    REQUIRE (dlg.isOpen());
    dlg.confirm();
    CHECK (tape.contentEndFrames() == 0);
    REQUIRE (tape.undo());
    CHECK (tape.contentEndFrames (0) > 0);
    CHECK (tape.contentEndFrames (1) > 0);

    // the dialog paints in both skins, the destructive key in the alert ink
    dlg.setBounds (e.getLocalBounds());
    e.softKey (131);
    for (auto skin : { ui::Skin::Light, ui::Skin::Dark })
    {
        ui::applySkin (skin);
        auto img = dlg.createComponentSnapshot (dlg.getLocalBounds());
        const auto ink = ui::colour::alertInk;
        auto near = [&] (juce::Rectangle<float> area) {
            const auto k = area.toNearestInt();
            for (int y = k.getY(); y < k.getBottom(); ++y)
                for (int x = k.getX(); x < k.getRight(); ++x)
                {
                    const auto px = img.getPixelAt (x, y);
                    if (std::abs (px.getRed() - ink.getRed()) + std::abs (px.getGreen() - ink.getGreen()) + std::abs (px.getBlue() - ink.getBlue()) < 40)
                        return true;
                }
            return false;
        };
        CHECK (near (dlg.keyArea (1)));       // DELETE in the alert ink
        CHECK_FALSE (near (dlg.keyArea (0))); // CANCEL is not red
    }
    ui::applySkin (ui::Skin::Light);
    dlg.cancel();

    // an empty tape: CLEAR ALL and SAVE TAPE are off, CLEAR ALL does not ask
    REQUIRE (tape.clearAll());
    e.softKey (131);
    CHECK_FALSE (dlg.isOpen());
}

TEST_CASE ("tape erase: refused while recording")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    recordOn (p, 0);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectMode (ui::Mode::Tape);
    e.selectTapeTrack (0);
    p.runTransport (T::Record);
    p.core().tape().housekeeping();
    process (p, 4);
    REQUIRE (p.tapeRecording());
    e.softKey (130);
    e.softKey (131);
    CHECK_FALSE (e.confirmDialogComponent().isOpen());
    TempFolder tmp;
    juce::String msg;
    CHECK_FALSE (p.saveTapeFile (tmp.dir.getChildFile ("x.ohpetape"), msg));
    CHECK (msg == "Not while recording");
    p.runTransport (T::Stop);
    process (p, 4);
    p.core().tape().housekeeping();
    CHECK (p.core().tape().contentEndFrames (0) > 0);
}

TEST_CASE ("tape erase: MIDI triggers - clear the edit track at once, clear all asks on screen")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    recordOn (p, 0);
    recordOn (p, 2);
    auto& tape = p.core().tape();
    for (auto [id, cc] : { std::pair<const char*, int> { Core::Ids::tapeClearTrack, 50 }, { Core::Ids::tapeClearAll, 51 } })
    {
        MidiMapping m;
        m.paramId = id;
        m.number = cc;
        m.channel = 1;
        p.core().midiMap().add (m);
    }
    p.core().midiMapChanged();
    setHost (p, Core::Ids::tapeEditTrack, 3.0f);
    process (p, 1);
    juce::MidiBuffer clear;
    clear.addEvent (juce::MidiMessage::controllerEvent (1, 50, 127), 0);
    clear.addEvent (juce::MidiMessage::controllerEvent (1, 50, 0), 10);
    process (p, 2, clear);
    p.pollRemoteControls();
    CHECK (tape.contentEndFrames (2) == 0);
    CHECK (tape.contentEndFrames (0) > 0);

    const int before = p.tapeClearAllRequestCount();
    juce::MidiBuffer all;
    all.addEvent (juce::MidiMessage::controllerEvent (1, 51, 127), 0);
    all.addEvent (juce::MidiMessage::controllerEvent (1, 51, 0), 10);
    process (p, 2, all);
    p.pollRemoteControls();
    CHECK (p.tapeClearAllRequestCount() == before + 1);
    CHECK (tape.contentEndFrames (0) > 0); // nothing is cleared without the confirmation
    CHECK (p.runTapeEdit (Core::Ids::tapeClearAll)); // what the confirmation runs
    CHECK (tape.contentEndFrames() == 0);
}

TEST_CASE ("tape erase: SAVE TAPE / LOAD TAPE round trip with loop range and edit track; loading over material asks")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    recordOn (p, 0);
    recordOn (p, 5);
    setHost (p, "tape.loopin", 0.5f);
    setHost (p, "tape.loopout", 1.5f);
    setHost (p, "tape.loop", 1.0f);
    setHost (p, Core::Ids::tapeEditTrack, 6.0f);
    process (p, 2);
    const auto end0 = p.core().tape().contentEndFrames (0), end5 = p.core().tape().contentEndFrames (5);

    TempFolder tmp;
    juce::String msg;
    REQUIRE (p.saveTapeFile (tmp.dir.getChildFile ("Session"), msg)); // the extension is added
    const auto file = tmp.dir.getChildFile ("Session.ohpetape");
    REQUIRE (file.existsAsFile());
    CHECK (msg.contains ("Session.ohpetape"));

    OhpeProcessor q;
    q.prepareToPlay (48000.0, 256);
    REQUIRE (q.loadTapeFile (file, msg));
    CHECK (q.core().tape().contentEndFrames (0) == end0);
    CHECK (q.core().tape().contentEndFrames (5) == end5);
    CHECK (q.core().params().get ("tape.loopin") == doctest::Approx (0.5f));
    CHECK (q.core().params().get ("tape.loopout") == doctest::Approx (1.5f));
    CHECK (q.core().params().get ("tape.loop") >= 0.5f);
    CHECK (q.core().params().get (Core::Ids::tapeEditTrack) == 6.0f);
    CHECK (q.core().tape().canUndo()); // loading is one undo step
    CHECK_FALSE (q.loadTapeFile (tmp.dir.getChildFile ("missing.ohpetape"), msg));
    CHECK (msg == "Could not read the tape file");

    // the editor: loading over recorded material asks first
    OhpeProcessor r;
    r.prepareToPlay (48000.0, 256);
    recordOn (r, 3);
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.setLanguage (i18n::Language::English);
    e.selectMode (ui::Mode::Tape);
    e.openTapeFile (file);
    auto& dlg = e.confirmDialogComponent();
    REQUIRE (dlg.isOpen());
    CHECK (dlg.options().confirmText == "Load");
    dlg.cancel();
    CHECK (r.core().tape().contentEndFrames (3) > 0);
    CHECK (r.core().tape().contentEndFrames (0) == 0);
    e.openTapeFile (file);
    REQUIRE (dlg.isOpen());
    dlg.confirm();
    CHECK (r.core().tape().contentEndFrames (0) == end0);
    CHECK (r.core().tape().contentEndFrames (3) == 0);
    REQUIRE (r.core().tape().undo());
    CHECK (r.core().tape().contentEndFrames (3) > 0);
}

TEST_CASE ("tape erase: English and German labels")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.setLanguage (i18n::Language::German);
    CHECK (i18n::tr ("Clear Track") == juce::String::fromUTF8 ("Spur löschen"));
    CHECK (i18n::tr ("Clear All") == juce::String::fromUTF8 ("Alles löschen"));
    CHECK (i18n::tr ("Save Tape") == "Band speichern");
    CHECK (i18n::tr ("Load Tape") == "Band laden");
    CHECK (i18n::tr ("Clear the whole tape?") == juce::String::fromUTF8 ("Gesamte Bandaufnahme löschen?"));
    CHECK (i18n::tr ("Delete") == juce::String::fromUTF8 ("Löschen"));
    CHECK (i18n::tr ("Cancel") == "Abbrechen");
    e.setLanguage (i18n::Language::English);
}
