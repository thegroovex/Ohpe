// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p39: the tempo is editable where it is shown (header and transport panel: drag, type, wheel), tap tempo, the
// metronome / count-in keys, REC with a count-in, a tempo that follows MIDI clock is read-only.

#include <doctest/doctest.h>

#include "plugin/PluginProcessor.h"
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

float plain (OhpeProcessor& p, const char* id) { return p.core().params().get (id); }

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos, juce::ModifierKeys mods = {})
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPos, now, 1, pos != downPos);
}

struct Fixture
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;
    Fixture()
    {
        p.prepareToPlay (48000.0, 256);
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        REQUIRE (e != nullptr);
    }
};
} // namespace

TEST_CASE ("tempo: the transport panel readout - drag up / down, Shift fine, type a value")
{
    Fixture f;
    auto& t = f.e->tempoPanelComponent();
    REQUIRE (t.isVisible());
    REQUIRE (t.getWidth() > 10);
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (120.0f));
    const juce::Point<float> c (t.getWidth() * 0.5f, t.getHeight() * 0.5f);
    t.mouseDown (mouseEvent (t, c, c));
    t.mouseDrag (mouseEvent (t, c.translated (0.0f, -40.0f), c)); // up 40 px = +10 BPM
    t.mouseUp (mouseEvent (t, c.translated (0.0f, -40.0f), c));
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (130.0f));
    t.mouseDown (mouseEvent (t, c, c));
    t.mouseDrag (mouseEvent (t, c.translated (0.0f, 20.0f), c, juce::ModifierKeys::shiftModifier)); // fine: -0.5
    t.mouseUp (mouseEvent (t, c, c));
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (129.5f));
    CHECK (t.commitText ("92,5"));
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (92.5f));
    CHECK_FALSE (t.commitText ("500"));
    CHECK_FALSE (t.commitText ("abc"));
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (92.5f));
    CHECK (f.p.parameter (Core::Ids::tempo)->convertFrom0to1 (f.p.parameter (Core::Ids::tempo)->getValue()) == doctest::Approx (92.5f).epsilon (0.01)); // the host sees it
    CHECK (t.shownText() == "92.5");
}

TEST_CASE ("tempo: the header readout is a control too; TAP sets the tempo from the second tap")
{
    Fixture f;
    auto& d = f.e->displayComponent();
    auto& h = d.headerTempo();
    REQUIRE (h.isVisible());
    REQUIRE (d.headerTap().isVisible());
    const juce::Point<float> c (h.getWidth() * 0.8f, h.getHeight() * 0.5f);
    h.mouseDown (mouseEvent (h, c, c));
    h.mouseDrag (mouseEvent (h, c.translated (0.0f, -8.0f), c));
    h.mouseUp (mouseEvent (h, c, c));
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (122.0f));
    h.mouseWheelMove (mouseEvent (h, c, c), juce::MouseWheelDetails { 0.0f, 1.0f, false, false, false });
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (123.0f));

    f.p.tapTempo();
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (123.0f)); // one tap: nothing yet
    juce::Thread::sleep (300);
    f.p.tapTempo();
    juce::Thread::sleep (300);
    f.p.tapTempo();
    CHECK (f.p.tapCount() == 3);
    const float tapped = plain (f.p, Core::Ids::tempo);
    CHECK (tapped > 120.0f); // ~200 BPM (sleep is not exact on every system)
    CHECK (tapped <= 300.0f);
}

TEST_CASE ("tempo: following MIDI clock it is read-only")
{
    Fixture f;
    f.p.core().params().set (Core::Ids::syncSource, (float) Core::SyncMidiClock);
    CHECK (f.p.tempoFollowsHost());
    auto& t = f.e->tempoPanelComponent();
    CHECK_FALSE (t.commitText ("140"));
    f.p.setTempoFromUi (140.0f);
    CHECK (plain (f.p, Core::Ids::tempo) == doctest::Approx (120.0f));
    f.p.core().params().set (Core::Ids::syncSource, (float) Core::SyncInternal);
    CHECK_FALSE (f.p.tempoFollowsHost());
}

TEST_CASE ("metronome keys: click on / off, count-in Off -> 1 -> 2 -> Off")
{
    Fixture f;
    auto& s = f.e->clickStripComponent();
    REQUIRE (s.isVisible());
    CHECK (s.fieldAt (s.fieldArea (ui::ClickStrip::ClickField).getCentre()) == ui::ClickStrip::ClickField);
    s.clickField (ui::ClickStrip::ClickField);
    CHECK (plain (f.p, Metronome::Ids::on) == 1.0f);
    s.clickField (ui::ClickStrip::ClickField);
    CHECK (plain (f.p, Metronome::Ids::on) == 0.0f);
    for (int expect : { 1, 2, 0 })
    {
        s.clickField (ui::ClickStrip::CountInField);
        CHECK ((int) plain (f.p, Metronome::Ids::countIn) == expect);
    }
    // the readout row does not overlap the transport keys
    CHECK_FALSE (s.getBounds().intersects (f.e->recordKey().getBounds()));
    CHECK_FALSE (f.e->tempoPanelComponent().getBounds().intersects (f.e->tapeReadoutComponent().getBounds()));
    CHECK_FALSE (f.e->tempoPanelComponent().getBounds().intersects (s.getBounds()));
}

TEST_CASE ("count-in: REC waits for the count-in, REC again cancels it")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.core().params().set (Metronome::Ids::countIn, 1.0f);
    const auto msg = p.runTransport (T::Record);
    CHECK (msg.isNotEmpty());
    p.core().tape().housekeeping();
    process (p, 4);
    CHECK (p.core().countInActive());
    CHECK (p.core().tape().positionFrames() == 0);
    CHECK (p.core().clickCount() >= 1); // the count-in clicks even with the metronome off
    const auto cancel = p.runTransport (T::Record);
    CHECK (cancel.isNotEmpty());
    process (p, 2);
    CHECK_FALSE (p.core().countInActive());
    CHECK_FALSE (p.tapeRolling());
    CHECK (p.core().tape().positionFrames() == 0);

    p.runTransport (T::Record); // again, to the end: 1 bar at 120 BPM = 96000 samples = 375 blocks of 256
    for (int k = 0; k < 40; ++k) { process (p, 10); p.core().tape().housekeeping(); }
    CHECK_FALSE (p.core().countInActive());
    CHECK (p.core().lastCountInRelease() >= 0);
    CHECK (p.core().tape().positionFrames() == 400 * 256 - 96000);
}
