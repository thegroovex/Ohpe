// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p42: the tape loop by hand - loop in / out at the playhead (snapped to the grid, Shift free), also while the tape
// plays; the loop keys behind the loop readout; dragging the loop edges; the loop history; the MIDI triggers
// "tape.edit.loop*"; encoders / host can never leave loop out before loop in.

#include <doctest/doctest.h>

#include "plugin/PluginProcessor.h"
#include "tape/TapeLoop.h"
#include "ui/PluginEditor.h"

using namespace ohpe;
using T = OhpeProcessor::TransportKey;

namespace
{
void process (OhpeProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
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

float plain (OhpeProcessor& p, const char* id) { return p.core().params().get (id); }

/** Puts the (stopped) playhead at `seconds`. */
void at (OhpeProcessor& p, double seconds)
{
    p.core().tape().locate (seconds);
    process (p, 1);
}

void prepare120 (OhpeProcessor& p)
{
    p.prepareToPlay (48000.0, 256);
    setHost (p, Core::Ids::tempo, 120.0f); // a bar = 2 s
    process (p, 2);
    p.watchTapeLoop (0.0);
}
} // namespace

TEST_CASE ("tape loop: set in / out at the playhead - on the bar grid, Shift free, loop out switches the loop on, undo / redo")
{
    OhpeProcessor p;
    prepare120 (p);
    CHECK (p.tapeLoopSnap() == tapeloop::Snap::Bar); // the default grid
    CHECK (plain (p, "tape.loop") < 0.5f);

    at (p, 3.1);
    p.setTapeLoopEdge (true, false);
    CHECK (plain (p, "tape.loopin") == doctest::Approx (4.0f).epsilon (1.0e-3));
    at (p, 9.1);
    const auto msg = p.setTapeLoopEdge (false, false);
    CHECK (msg.isNotEmpty());
    CHECK (plain (p, "tape.loopout") == doctest::Approx (10.0f).epsilon (1.0e-3));
    CHECK (plain (p, "tape.loop") >= 0.5f); // loop out closes the loop
    // Shift: exactly at the playhead
    at (p, 5.25);
    p.setTapeLoopEdge (true, true);
    CHECK (plain (p, "tape.loopin") == doctest::Approx (5.25f).epsilon (1.0e-3));
    // beat grid
    setHost (p, Core::Ids::tapeLoopSnap, 1.0f);
    process (p, 1);
    at (p, 7.3);
    p.setTapeLoopEdge (false, false);
    CHECK (plain (p, "tape.loopout") == doctest::Approx (7.5f).epsilon (1.0e-3));
    CHECK (plain (p, "tape.loopin") < plain (p, "tape.loopout"));

    // undo walks back through every change, redo forward
    REQUIRE (p.canUndoTapeLoop());
    p.tapeLoopAction (Core::Ids::tapeLoopUndo);
    CHECK (plain (p, "tape.loopout") == doctest::Approx (10.0f).epsilon (1.0e-3));
    p.tapeLoopAction (Core::Ids::tapeLoopUndo);
    CHECK (plain (p, "tape.loopin") == doctest::Approx (4.0f).epsilon (1.0e-3));
    REQUIRE (p.canRedoTapeLoop());
    p.tapeLoopAction (Core::Ids::tapeLoopRedo);
    CHECK (plain (p, "tape.loopin") == doctest::Approx (5.25f).epsilon (1.0e-3));

    // half / double on the beat grid
    p.tapeLoopAction (Core::Ids::tapeLoopDouble);
    CHECK (plain (p, "tape.loopout") - plain (p, "tape.loopin") == doctest::Approx (2.0f * (10.0f - 5.25f)).epsilon (1.0e-3));
    p.tapeLoopAction (Core::Ids::tapeLoopHalf);
    CHECK (plain (p, "tape.loopout") > plain (p, "tape.loopin"));
}

TEST_CASE ("tape loop: set while the tape plays - the playhead stays in the new loop; MIDI triggers do the same")
{
    OhpeProcessor p;
    prepare120 (p);
    setHost (p, Core::Ids::tapeLoopSnap, 0.0f); // free: exactly at the playhead
    process (p, 1);
    p.runTransport (T::TapePlayStop);
    REQUIRE (p.tapeRolling());
    process (p, 40); // ~0.21 s
    p.setTapeLoopEdge (true, false);
    const double in = plain (p, "tape.loopin");
    process (p, 100); // ~0.53 s later
    p.setTapeLoopEdge (false, false);
    const double out = plain (p, "tape.loopout");
    CHECK (out - in == doctest::Approx (0.533).epsilon (0.05));
    CHECK (plain (p, "tape.loop") >= 0.5f);
    for (int k = 0; k < 300; ++k)
    {
        process (p, 1);
        const double pos = (double) p.core().tape().positionFrames() / 48000.0;
        REQUIRE (pos >= in - 0.01);
        REQUIRE (pos <= out + 0.01);
    }
    p.runTransport (T::Stop);
    process (p, 2);

    // MIDI / host: a rising edge of the trigger sets loop in at the playhead
    at (p, 1.0);
    setHost (p, Core::Ids::tapeLoopSetIn, 1.0f);
    p.pollRemoteControls();
    CHECK (plain (p, "tape.loopin") == doctest::Approx (1.0f).epsilon (1.0e-3));
    setHost (p, Core::Ids::tapeLoopSetIn, 0.0f);
    p.pollRemoteControls();
    CHECK (p.transportMessageCount() > 0); // the display shows what happened
}

TEST_CASE ("tape loop: encoders / host never leave loop out before loop in; an encoder turn is one undo step")
{
    OhpeProcessor p;
    prepare120 (p);
    at (p, 2.0);
    p.setTapeLoopEdge (true, false);
    at (p, 6.0);
    p.setTapeLoopEdge (false, false);
    p.watchTapeLoop (1.0);
    // loop in turned past loop out
    setHost (p, "tape.loopin", 9.0f);
    process (p, 1);
    p.watchTapeLoop (1.1);
    CHECK (plain (p, "tape.loopin") == doctest::Approx (9.0f).epsilon (1.0e-3));
    CHECK (plain (p, "tape.loopout") > plain (p, "tape.loopin"));
    // a turn of loop out in small steps: one step once it rests
    p.watchTapeLoop (5.0);
    const float in0 = plain (p, "tape.loopin"), out0 = plain (p, "tape.loopout");
    for (int k = 1; k <= 8; ++k)
    {
        setHost (p, "tape.loopout", out0 + 0.25f * (float) k);
        process (p, 1);
        p.watchTapeLoop (5.0 + 0.05 * k);
    }
    p.watchTapeLoop (7.0);
    p.tapeLoopAction (Core::Ids::tapeLoopUndo);
    CHECK (plain (p, "tape.loopout") == doctest::Approx (out0).epsilon (1.0e-3));
    CHECK (plain (p, "tape.loopin") == doctest::Approx (in0).epsilon (1.0e-3));
}

TEST_CASE ("tape loop: the loop readout opens the loop keys (two rows), they act, the encoders show the loop page; dragging an edge")
{
    OhpeProcessor p;
    prepare120 (p);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();
    e.setLanguage (i18n::Language::English);
    e.selectMode (ui::Mode::Tape);
    REQUIRE (d.softKeyList().size() == 14); // the edit keys stay as they were (p67: + CAPTURE, MEMORY)
    const auto readout = d.loopReadoutArea();
    REQUIRE_FALSE (readout.isEmpty());
    CHECK (readout.getRight() <= d.softKeyArea (0).getX());
    CHECK (readout.getRight() <= d.softKeyArea (7).getX());

    REQUIRE (d.onLoopReadoutClicked != nullptr);
    d.onLoopReadoutClicked();
    CHECK (e.tapeLoopKeysShown());
    const auto& keys = d.softKeyList();
    REQUIRE (keys.size() == 11);
    CHECK (keys[0].label == "Set In");
    CHECK (keys[1].label == "Set Out");
    CHECK (d.softKeyRows() == 2);
    CHECK (d.softKeyArea (6).getY() > d.softKeyArea (0).getBottom() - 1.0f);
    for (int k = 0; k < 11; ++k) CHECK (d.softKeyArea (k).getX() >= readout.getRight());
    bool loopPage = false;
    for (int i = 0; i < 4; ++i) loopPage |= e.encoder (i).boundId() == "tape.loopin";
    CHECK (loopPage);

    at (p, 3.9);
    e.softKey (140); // SET IN
    CHECK (plain (p, "tape.loopin") == doctest::Approx (4.0f).epsilon (1.0e-3));
    at (p, 12.2);
    e.softKey (141); // SET OUT
    CHECK (plain (p, "tape.loopout") == doctest::Approx (12.0f).epsilon (1.0e-3));
    CHECK (plain (p, "tape.loop") >= 0.5f);
    e.setLoopKeysFreeForTests (true); // as with Shift held
    at (p, 4.321);
    e.softKey (140);
    CHECK (plain (p, "tape.loopin") == doctest::Approx (4.321f).epsilon (1.0e-3));
    e.setLoopKeysFreeForTests (false);
    e.softKey (143); // the grid: Bar -> Beat
    CHECK (p.tapeLoopSnap() == tapeloop::Snap::Beat);
    e.softKey (146); // UNDO LOOP
    CHECK (plain (p, "tape.loopin") == doctest::Approx (4.0f).epsilon (1.0e-3));
    e.softKey (142); // LOOP off
    CHECK (plain (p, "tape.loop") < 0.5f);
    e.softKey (142);
    CHECK (plain (p, "tape.loop") >= 0.5f);

    // the edges on the lanes: in left of out, both grabbable
    const float xi = d.tapeLoopEdgeX (true), xo = d.tapeLoopEdgeX (false);
    CHECK (xi < xo);
    const auto lanes = d.tapeLanesArea();
    CHECK (d.tapeLoopEdgeAt ({ xi, lanes.getCentreY() }) == 1);
    CHECK (d.tapeLoopEdgeAt ({ xo, lanes.getCentreY() }) == 2);
    CHECK (d.tapeLoopEdgeAt ({ (xi + xo) * 0.5f, lanes.getCentreY() }) == 0);
    // a drag (as the display does it): on the grid, stops at the other edge, one undo step for the whole drag
    const auto before = p.tapeLoopRange();
    for (double s = 4.0; s <= 30.0; s += 0.7) p.dragTapeLoopEdge (true, s, false);
    p.endTapeLoopDrag();
    CHECK (plain (p, "tape.loopin") < plain (p, "tape.loopout"));
    CHECK (plain (p, "tape.loopout") - plain (p, "tape.loopin") == doctest::Approx (0.5f).epsilon (1.0e-3)); // one beat
    e.softKey (146);
    CHECK (plain (p, "tape.loopin") == doctest::Approx ((float) before.in).epsilon (1.0e-3));

    e.softKey (145); // BACK
    CHECK_FALSE (e.tapeLoopKeysShown());
    CHECK (d.softKeyList().size() == 14);
}
