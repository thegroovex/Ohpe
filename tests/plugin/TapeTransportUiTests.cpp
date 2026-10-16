// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p11: tape transport (one-touch record, punch out, stop / to the start, loop, arm, "Play starts
// tape", MIDI triggers), the tape page's transport panel, and overlays (library, pro view) that
// hide every control and legend of the plate.

#include <doctest/doctest.h>

#include "preset/Preset.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace ohpe;
using T = OhpeProcessor::TransportKey;

namespace
{
struct TempFolder
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("ohpe-test-transport-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
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

float plain (OhpeProcessor& p, const char* id) { return p.core().params().get (id); }

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos, juce::ModifierKeys mods = {})
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, pos, now, 1, false);
}

/** Darkest perceived brightness inside a design-unit rectangle of the editor (1200 x 800 design). */
float darkest (juce::Component& e, juce::Rectangle<float> design)
{
    const float s = (float) e.getWidth() / 1200.0f;
    const auto r = juce::Rectangle<float> (design.getX() * s, design.getY() * s, design.getWidth() * s, design.getHeight() * s).toNearestInt();
    auto img = e.createComponentSnapshot (e.getLocalBounds());
    float lo = 1.0f;
    for (int y = r.getY(); y < r.getBottom(); ++y)
        for (int x = r.getX(); x < r.getRight(); ++x)
            lo = juce::jmin (lo, img.getPixelAt (x, y).getPerceivedBrightness());
    return lo;
}

/** No visible child except the overlay itself may overlap the overlay. */
void checkNothingUnder (OhpeEditor& e, juce::Component& overlay)
{
    for (auto* c : e.getChildren())
    {
        if (c == &overlay || ! c->isVisible()) continue;
        CAPTURE (c->getName());
        CAPTURE (c->getBounds().toString());
        CHECK_FALSE (c->getBounds().intersects (overlay.getBounds()));
    }
}
} // namespace

TEST_CASE ("tape transport: one-touch record, punch out, stop twice = to the start")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    REQUIRE (p.armedTrackMask() == 0);
    CHECK (plain (p, Core::Ids::tapeTxLink) >= 0.5f); // "Play starts tape" is on by default

    // REC with nothing armed: arms the edit track (1), records, starts the tape and the sequencer
    p.runTransport (T::Record);
    CHECK (p.armedTrackMask() == 1);
    CHECK (p.tapeRecording());
    CHECK (plain (p, Core::Ids::seqRun) >= 0.5f);
    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    p.core().tape().housekeeping(); // the block pool for recording (the plug-in's timer does this)
    for (int k = 0; k < 4; ++k)
    {
        process (p, 10, k == 0 ? note : juce::MidiBuffer());
        p.core().tape().housekeeping();
    }
    CHECK (p.core().tape().positionFrames() > 0);

    // REC again: punch out, the tape keeps playing
    p.runTransport (T::Record);
    CHECK_FALSE (p.tapeRecordReady());
    CHECK (p.tapeRolling());
    process (p, 4);
    p.core().tape().housekeeping();
    CHECK (p.core().tape().contentEndFrames (0) > 0);

    // STOP: everything stops; STOP again: back to the start
    p.runTransport (T::Stop);
    CHECK_FALSE (p.tapeRolling());
    CHECK (plain (p, Core::Ids::seqRun) < 0.5f);
    process (p, 2);
    CHECK (p.core().tape().positionFrames() > 0);
    p.runTransport (T::Stop);
    process (p, 2);
    CHECK (p.core().tape().positionFrames() == 0);

    // a second REC keeps the armed track (no new arming)
    p.runTransport (T::Arm); // disarm track 1
    CHECK (p.armedTrackMask() == 0);
    p.core().params().set (Core::Ids::tapeEditTrack, 3.0f);
    p.runTransport (T::Arm);
    CHECK (p.armedTrackMask() == 4);
    p.runTransport (T::Record);
    CHECK (p.armedTrackMask() == 4);
    p.runTransport (T::Stop);
    CHECK_FALSE (p.tapeRecordReady());
}

TEST_CASE ("tape transport: Play starts tape link, tape play, loop without a range")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.runTransport (T::PlayStop);
    CHECK (p.tapeRolling());
    CHECK (plain (p, Core::Ids::seqRun) >= 0.5f);
    p.runTransport (T::PlayStop);
    CHECK_FALSE (p.tapeRolling());
    CHECK (plain (p, Core::Ids::seqRun) < 0.5f);

    // link off: PLAY runs the sequencer alone, the tape page's play the tape alone
    p.parameter (Core::Ids::tapeTxLink)->setValueNotifyingHost (0.0f);
    p.core().params().set (Core::Ids::tapeTxLink, 0.0f);
    p.runTransport (T::PlayStop);
    CHECK (plain (p, Core::Ids::seqRun) >= 0.5f);
    CHECK_FALSE (p.tapeRolling());
    p.runTransport (T::TapePlayStop);
    CHECK (p.tapeRolling());
    p.runTransport (T::Stop); // stops both
    CHECK_FALSE (p.tapeRolling());
    CHECK (plain (p, Core::Ids::seqRun) < 0.5f);

    // loop without a range: four bars at the tempo
    p.runTransport (T::Loop);
    CHECK (plain (p, "tape.loop") >= 0.5f);
    CHECK (plain (p, "tape.loopout") > plain (p, "tape.loopin") + 1.0f);
    p.runTransport (T::Loop);
    CHECK (plain (p, "tape.loop") < 0.5f);

    // transport settings and triggers are not part of presets
    CHECK (preset::isExcludedFromProject ("tape.tx.link"));
    CHECK (preset::isExcludedFromProject ("tape.tx.rec"));
    CHECK_FALSE (preset::isExcludedFromProject ("tape.play"));
}

TEST_CASE ("tape transport: MIDI-mapped REC and STOP")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    for (auto [id, cc] : { std::pair<const char*, int> { Core::Ids::tapeTxRec, 40 }, { Core::Ids::tapeTxStop, 41 } })
    {
        MidiMapping m;
        m.paramId = id;
        m.number = cc;
        m.channel = 1;
        p.core().midiMap().add (m);
    }
    p.core().midiMapChanged();
    const int before = p.transportMessageCount();
    juce::MidiBuffer rec;
    rec.addEvent (juce::MidiMessage::controllerEvent (1, 40, 127), 0);
    rec.addEvent (juce::MidiMessage::controllerEvent (1, 40, 0), 10);
    process (p, 2, rec);
    p.pollRemoteControls();
    CHECK (p.tapeRecording());
    CHECK (p.transportMessageCount() == before + 1);
    CHECK (p.lastTransportMessage().isNotEmpty());
    juce::MidiBuffer stop;
    stop.addEvent (juce::MidiMessage::controllerEvent (1, 41, 127), 0);
    process (p, 2, stop);
    p.pollRemoteControls();
    CHECK_FALSE (p.tapeRolling());
    CHECK_FALSE (p.tapeRecordReady());
}

TEST_CASE ("editor: tape page transport panel, arm dots, keys")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectMode (ui::Mode::Tape);
    auto& d = e.displayComponent();
    CHECK (e.encoder (0).boundId() == "tape.tr1.arm"); // the selected track first

    // the hint tells what to do on an empty tape
    i18n::setCurrent (i18n::Language::German);
    CHECK (d.tapeHint() == juce::String::fromUTF8 ("Spur w\xc3\xa4hlen, REC dr\xc3\xbc" "cken"));
    i18n::setCurrent (i18n::Language::English);
    CHECK (d.tapeHint() == "Pick a track, press REC");

    // five transport buttons inside the transport panel, side by side
    for (int b = 0; b < ui::Display::numTransportButtons; ++b)
    {
        const auto r = d.transportButton (b);
        CHECK_FALSE (r.isEmpty());
        CHECK (d.characterArea().contains (r));
        if (b > 0) CHECK (r.getX() > d.transportButton (b - 1).getRight());
    }

    // clicking REC on the display records on the selected track; the REC key lights up
    e.selectTapeTrack (2);
    const auto recBtn = d.transportButton (ui::Display::TxRecord).getCentre();
    d.mouseDown (mouseEvent (d, recBtn));
    CHECK (p.armedTrackMask() == 4);
    CHECK (p.tapeRecording());
    e.advanceAnimation (0.1f); // blink phase "on"
    CHECK (e.recordKey().isActive());
    CHECK (e.lastFlash().contains ("3"));
    process (p, 8);
    e.advanceAnimation (0.2f);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    // other pages show the tape state in the display header
    e.selectMode (ui::Mode::Synth);
    e.advanceAnimation (0.1f);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.selectMode (ui::Mode::Tape);

    // the STOP button, then the arm dot of track 5
    d.mouseDown (mouseEvent (d, d.transportButton (ui::Display::TxStop).getCentre()));
    CHECK_FALSE (p.tapeRolling());
    d.mouseDown (mouseEvent (d, d.trackArmArea (4).getCentre()));
    CHECK ((p.armedTrackMask() & 16) != 0);
    CHECK ((int) std::lround (p.core().params().get (Core::Ids::tapeEditTrack)) == 5);
    CHECK (d.getTooltip().isNotEmpty());

    // the PLAY key on the body runs sequencer and tape together (linked)
    e.playTransportKey().onClick();
    CHECK (p.tapeRolling());
    e.transport (T::Stop);
    CHECK_FALSE (p.tapeRolling());

    // plug-in hosts own Space and R: the shortcuts are for the standalone app only
    CHECK_FALSE (e.handleShortcut (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK_FALSE (e.handleShortcut (juce::KeyPress ('r')));
}

TEST_CASE ("editor: library and pro view hide every plate control and legend")
{
    OhpeProcessor p;
    TempFolder tmp;
    p.presets().setUserFolder (tmp.dir);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    // the legend glyphs above the lid (p14: designArea, 88..100; the lid starts at 104)
    const auto engineLegend = OhpeEditor::designArea ("engineLegend").withTrimmedRight (100.0f);
    const auto modeLegend = OhpeEditor::designArea ("modeLegend").withTrimmedRight (120.0f);
    const float engineClosed = darkest (e, engineLegend), modeClosed = darkest (e, modeLegend);

    e.showLibrary (true);
    REQUIRE (e.libraryVisible());
    CHECK (e.overlayOpen());
    checkNothingUnder (e, e.libraryComponent());
    // the "ENGINE / P1" and "MODE" legends above the lid's edge are gone (only the plate is left there)
    CHECK (darkest (e, engineLegend) > engineClosed + 0.1f);
    CHECK (darkest (e, modeLegend) > modeClosed + 0.1f);
    e.showLibrary (false);
    CHECK (e.displayComponent().isVisible());
    CHECK (e.encoder (0).isVisible());

    // pro view in every mode (Effects = the FX view)
    for (int m = 0; m < ui::numModes; ++m)
    {
        CAPTURE (m);
        e.selectMode ((ui::Mode) m);
        e.showProView (true);
        checkNothingUnder (e, e.proViewComponent());
        CHECK (darkest (e, engineLegend) > engineClosed + 0.1f);
        e.showProView (false);
    }
    // switching from the pro view straight to the library keeps the plate hidden
    e.showProView (true);
    e.showLibrary (true);
    CHECK_FALSE (e.proViewVisible());
    checkNothingUnder (e, e.libraryComponent());
    e.showLibrary (false);
    CHECK_FALSE (e.overlayOpen());
    for (auto* c : { (juce::Component*) &e.displayComponent(), (juce::Component*) &e.encoder (3), (juce::Component*) &e.recordKey() })
        CHECK (c->isVisible());
    CHECK (darkest (e, engineLegend) == doctest::Approx (engineClosed));
}
