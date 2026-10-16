// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p14: the redesigned plate - no visible controls overlap in any view (both languages, 1x and 1.5x),
// everything sits inside the housing, the performance wheels send pitch bend / mod wheel to the
// parts (MIDI-learnable, the pitch wheel springs back), the glide controls follow the edit part.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/Palette.h"
#include "ui/PluginEditor.h"
#include "mod/ModVoice.h"
#include "fx/FxRack.h"

using namespace ohpe;

namespace
{
template <typename Done>
void pump (Done done)
{
    for (int i = 0; i < 100 && ! done(); ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
}

void process (OhpeProcessor& p, int blocks = 1, const juce::MidiBuffer& first = {})
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi = first;
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos, juce::ModifierKeys mods = {})
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, pos, now, 1, false);
}

/** No two visible children of the editor overlap; all of them lie inside the housing. */
void checkLayout (OhpeEditor& e)
{
    const float s = (float) e.getWidth() / (float) OhpeEditor::designWidth;
    const auto h = OhpeEditor::designArea ("housing");
    const auto housing = juce::Rectangle<float> (h.getX() * s, h.getY() * s, h.getWidth() * s, h.getHeight() * s).toNearestInt();
    std::vector<juce::Component*> shown;
    for (auto* c : e.getChildren())
        if (c->isVisible() && dynamic_cast<juce::TooltipWindow*> (c) == nullptr && dynamic_cast<juce::ResizableCornerComponent*> (c) == nullptr)
            shown.push_back (c); // the host window's resize corner sits on the window edge by design
    for (size_t i = 0; i < shown.size(); ++i)
    {
        CAPTURE (shown[i]->getName());
        CAPTURE (shown[i]->getBounds().toString());
        CHECK (housing.contains (shown[i]->getBounds()));
        for (size_t j = i + 1; j < shown.size(); ++j)
        {
            CAPTURE (shown[j]->getName());
            CAPTURE (shown[j]->getBounds().toString());
            CHECK_FALSE (shown[i]->getBounds().intersects (shown[j]->getBounds()));
        }
    }
}
} // namespace

TEST_CASE ("editor layout: no overlapping visible components in any view, language or size")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    for (auto lang : { i18n::Language::English, i18n::Language::German })
        for (int width : { 1200, 1800, 800 })
        {
            CAPTURE (width);
            e.setLanguage (lang);
            e.setSize (width, width * 2 / 3);
            for (auto layout : { OhpeEditor::TransportLayout::Primary, OhpeEditor::TransportLayout::TapeDeck })
            {
                e.setTransportLayout (layout); // rev 3: both variants of the transport block
                for (int m = 0; m < ui::numModes; ++m)
                {
                    CAPTURE (m);
                    e.selectMode ((ui::Mode) m);
                    checkLayout (e);
                }
            }
            e.showLibrary (true);
            checkLayout (e);
            e.showLibrary (false);
            e.showProView (true);
            checkLayout (e);
            e.showProView (false);
            CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        }
    i18n::setCurrent (i18n::Language::English);
}

TEST_CASE ("editor layout: wheels left, glide right of the keyboard; transport block left, octave + MIDI block right, mirrored")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    const auto kb = e.keyboardComponent().getBounds();
    CHECK (e.pitchWheelComponent().getRight() < e.modWheelComponent().getX());
    CHECK (e.modWheelComponent().getRight() < kb.getX());
    CHECK (e.glideModeSwitch().getX() > kb.getRight());
    CHECK (e.glideKnobComponent().getX() > kb.getRight());
    CHECK (e.glideModeSwitch().getBottom() <= e.glideKnobComponent().getY());
    // p14 rev 3: the transport block (bottom left) holds the five transport keys and the tape readout; the octave
    // block (bottom right) mirrors it: the octave keys, MIDI and the keyboard readout - in both layouts
    const auto txPanel = OhpeEditor::designArea ("transportPanel"), octPanel = OhpeEditor::designArea ("octavePanel");
    CHECK (txPanel.getWidth() == octPanel.getWidth());
    CHECK (txPanel.getHeight() == octPanel.getHeight());
    CHECK (txPanel.getY() == octPanel.getY());
    CHECK (txPanel.getX() - OhpeEditor::designArea ("housing").getX() == doctest::Approx (OhpeEditor::designArea ("housing").getRight() - octPanel.getRight()));
    for (auto layout : { OhpeEditor::TransportLayout::TapeDeck, OhpeEditor::TransportLayout::Primary })
    {
        CAPTURE ((int) layout);
        e.setTransportLayout (layout);
        const float s = (float) e.getWidth() / (float) OhpeEditor::designWidth;
        auto design = [s] (juce::Component& c) { return c.getBounds().toFloat() / s; };
        for (auto* c : std::initializer_list<juce::Component*> { &e.recordKey(), &e.playTransportKey(), &e.stopTransportKey(), &e.toStartTransportKey(),
                                                                &e.loopTransportKey(), &e.tapeReadoutComponent() })
        {
            CHECK (txPanel.reduced (11.0f).contains (design (*c)));
            CHECK (c->isVisible());
        }
        for (auto* c : std::initializer_list<juce::Component*> { &e.octaveDownKey(), &e.octaveUpKey(), &e.midiKeyComponent(), &e.keysReadoutComponent() })
        {
            CHECK (octPanel.reduced (11.0f).contains (design (*c)));
            CHECK (c->isVisible());
        }
        // the readouts sit at the same place in their panels, with the same size
        const auto tr = design (e.tapeReadoutComponent()), kr = design (e.keysReadoutComponent());
        CHECK (tr.getX() - txPanel.getX() == doctest::Approx (kr.getX() - octPanel.getX()).epsilon (0.02));
        CHECK (tr.getY() == doctest::Approx (kr.getY()).epsilon (0.02));
        CHECK (tr.getWidth() == doctest::Approx (kr.getWidth()).epsilon (0.02));
        CHECK (tr.getHeight() == doctest::Approx (kr.getHeight()).epsilon (0.02));
        // consistent key heights: a key of the transport block and the octave block on the same row share it
        CHECK (e.recordKey().getHeight() == e.octaveDownKey().getHeight());
        CHECK (e.recordKey().getY() == e.octaveDownKey().getY());
        CHECK (e.stopTransportKey().getHeight() == e.midiKeyComponent().getHeight());
        CHECK (e.stopTransportKey().getY() == e.midiKeyComponent().getY());
        // every key of the two blocks carries a light bar inside its cap, under the pictogram
        for (auto* k : { &e.recordKey(), &e.playTransportKey(), &e.stopTransportKey(), &e.toStartTransportKey(), &e.loopTransportKey(),
                         &e.octaveDownKey(), &e.octaveUpKey(), &e.midiKeyComponent() })
        {
            const auto bar = k->barArea(), cap = k->capArea();
            CHECK_FALSE (bar.isEmpty());
            CHECK (cap.contains (bar));
            CHECK (bar.getCentreY() > cap.getCentreY());
        }
    }
    // the tape-deck row: TO START STOP PLAY REC LOOP joined left to right, equal widths, one row
    e.setTransportLayout (OhpeEditor::TransportLayout::TapeDeck);
    {
        ui::PictoKey* row[] = { &e.toStartTransportKey(), &e.stopTransportKey(), &e.playTransportKey(), &e.recordKey(), &e.loopTransportKey() };
        for (int i = 1; i < 5; ++i)
        {
            CHECK (row[i]->getY() == row[0]->getY());
            CHECK (std::abs (row[i]->getX() - row[i - 1]->getRight()) <= 1);
            CHECK (std::abs (row[i]->getWidth() - row[0]->getWidth()) <= 1);
        }
    }
    // the large keys of the Primary layout are as large as the column keys
    e.setTransportLayout (OhpeEditor::TransportLayout::Primary);
    CHECK (e.recordKey().getWidth() == e.engineKey (EngineType::Analog).getWidth());
    CHECK (e.recordKey().getHeight() == e.engineKey (EngineType::Analog).getHeight());
    CHECK (e.playTransportKey().getWidth() == e.recordKey().getWidth());
    // PORTAMENTO over GLIDE: one text column - the captions start at the same x, in the same font size
    {
        const auto sw = e.glideModeSwitch().captionArea() + e.glideModeSwitch().getPosition().toFloat();
        const auto kn = e.glideKnobComponent().captionArea() + e.glideKnobComponent().getPosition().toFloat();
        CHECK (sw.getX() == doctest::Approx (kn.getX()));
        CHECK (sw.getHeight() == doctest::Approx (kn.getHeight()));
        CHECK (sw.getBottom() <= kn.getY());
        CHECK (e.glideModeSwitch().getCaption() == "Portamento");
        CHECK (e.glideModeSwitch().labelArea (0).getX() == doctest::Approx (e.glideModeSwitch().captionArea().getX()));
    }
    // three octaves, C3..C6 by default
    CHECK (e.keyboardComponent().getRangeEnd() - e.keyboardComponent().getRangeStart() == 36);
    // the overlays cover the plate, not the header or the keyboard row
    const auto lid = OhpeEditor::designArea ("overlay");
    CHECK (lid.getBottom() <= OhpeEditor::designArea ("keyboardBed").getY());
    CHECK (lid.getY() >= OhpeEditor::designArea ("engineLegend").getBottom());
}

TEST_CASE ("wheels: the pitch wheel bends the edit part, the mod wheel feeds the matrix, both MIDI-learnable")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 512);
    process (p, 2);
    auto& m1 = p.core().part (0).modMatrix();
    CHECK (m1.sourceValue (mod::SrcPitchBend) == doctest::Approx (0.0f));

    p.parameter (Core::Ids::perfBend)->setValueNotifyingHost (1.0f); // full up
    p.parameter (Core::Ids::perfMod)->setValueNotifyingHost (0.5f);
    process (p);
    CHECK (m1.sourceValue (mod::SrcPitchBend) == doctest::Approx (1.0f).epsilon (0.001));
    CHECK (m1.sourceValue (mod::SrcModWheel) == doctest::Approx (64.0f / 127.0f).epsilon (0.01));
    p.parameter (Core::Ids::perfBend)->setValueNotifyingHost (0.5f); // back to the centre
    process (p);
    CHECK (m1.sourceValue (mod::SrcPitchBend) == doctest::Approx (0.0f).epsilon (0.001));

    // the wheels play the edit part's channel (part 2 listens on channel 2)
    p.parameter (Core::Ids::editPart)->setValueNotifyingHost (p.parameter (Core::Ids::editPart)->convertTo0to1 (2.0f));
    process (p);
    p.parameter (Core::Ids::perfBend)->setValueNotifyingHost (0.0f); // full down
    process (p);
    CHECK (p.core().part (1).modMatrix().sourceValue (mod::SrcPitchBend) == doctest::Approx (-1.0f).epsilon (0.001));

    // MIDI learn: a CC moves the mod wheel, the mod wheel moves the matrix source
    p.armMidiLearn (Core::Ids::perfMod);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 10), 0);
    process (p, 1, midi);
    pump ([&] { return p.core().midiMap().isMapped (Core::Ids::perfMod); });
    CHECK (p.core().midiMap().isMapped (Core::Ids::perfMod));
    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 127), 0);
    process (p, 1, midi);
    process (p, 1);
    CHECK (p.parameter (Core::Ids::perfMod)->getValue() == doctest::Approx (1.0f));
    CHECK (p.core().part (1).modMatrix().sourceValue (mod::SrcModWheel) == doctest::Approx (1.0f));
}

TEST_CASE ("wheels: dragging the pitch wheel bends, releasing springs back; the mod wheel stays")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& pw = e.pitchWheelComponent();
    const auto c = pw.slotArea().getCentre();
    pw.mouseDown (mouseEvent (pw, c));
    pw.mouseDrag (mouseEvent (pw, c.translated (0, -pw.slotArea().getHeight() * 0.3f)));
    CHECK (p.parameter (Core::Ids::perfBend)->getValue() > 0.7f);
    pw.mouseUp (mouseEvent (pw, c.translated (0, -pw.slotArea().getHeight() * 0.3f)));
    pump ([&] { return p.parameter (Core::Ids::perfBend)->getValue() == 0.5f; });
    CHECK (p.parameter (Core::Ids::perfBend)->getValue() == doctest::Approx (0.5f));

    auto& mw = e.modWheelComponent();
    const auto mc = mw.slotArea().getCentre();
    mw.mouseDown (mouseEvent (mw, mc));
    mw.mouseDrag (mouseEvent (mw, mc.translated (0, -mw.slotArea().getHeight() * 0.4f)));
    mw.mouseUp (mouseEvent (mw, mc.translated (0, -mw.slotArea().getHeight() * 0.4f)));
    pump ([] { return false; }); // give a spring a chance - there is none
    CHECK (p.parameter (Core::Ids::perfMod)->getValue() == doctest::Approx (0.4f).epsilon (0.02));
    CHECK_FALSE (pw.springsBack() == mw.springsBack());
}

TEST_CASE ("glide controls follow the edit part; RANDOM changes the engine's sound only; SAVE opens the save panel")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    CHECK (e.glideModeSwitch().boundId() == "p1.glide.mode");
    CHECK (e.glideKnobComponent().boundId() == "p1.glide");
    e.selectPart (2);
    CHECK (e.glideModeSwitch().boundId() == "p3.glide.mode");
    CHECK (e.glideKnobComponent().boundId() == "p3.glide");
    // a click on "Always" (third position)
    auto& sw = e.glideModeSwitch();
    sw.mouseDown (mouseEvent (sw, sw.labelArea (2).getCentre()));
    auto* gm = p.parameter ("p3.glide.mode");
    CHECK (std::lround (gm->convertFrom0to1 (gm->getValue())) == 2);
    CHECK (sw.position() == 2);

    e.selectPart (0);
    e.selectEngine (EngineType::Analog);
    const float vol = p.core().params().get ("p1.vol"), cutoff = p.parameter ("p1.analog.cutoff")->getValue();
    e.randomiseSound (1234);
    CHECK (p.core().params().get ("p1.vol") == doctest::Approx (vol));
    CHECK (p.parameter ("p1.analog.cutoff")->getValue() != doctest::Approx (cutoff));
    CHECK (e.lastFlash().isNotEmpty());

    e.openSave();
    CHECK (e.libraryVisible());
    e.showLibrary (false);
}

TEST_CASE ("key LEDs: engine keys light yellow for the edit part, violet for other parts, with part dots")
{
    using ui::Led;
    ui::LedInputs in;
    in.partEngine = { (int) EngineType::Analog, (int) EngineType::FM, (int) EngineType::Analog, (int) EngineType::Grain };
    in.editPart = 0;
    auto leds = ui::keyLeds (in);
    CHECK (leds.engine[(size_t) EngineType::Analog] == Led::Focus);
    // rev 3: one bar segment per part - the edit part's segment yellow (Focus), other parts violet (Active)
    {
        const auto& seg = leds.engineSegments[(size_t) EngineType::Analog];
        for (int p = 0; p < 4; ++p)
        {
            CAPTURE (p);
            const bool plays = in.partEngine[(size_t) p] == (int) EngineType::Analog;
            CHECK (seg[(size_t) p] == (! plays ? Led::Off : p == in.editPart ? Led::Focus : Led::Active));
        }
        for (const auto l : leds.engineSegments[(size_t) EngineType::Wavetable]) CHECK (l == Led::Off);
    }
    CHECK (leds.engineParts[(size_t) EngineType::Analog] == 0b0101u);
    CHECK (leds.engine[(size_t) EngineType::FM] == Led::Active);
    CHECK (leds.engineParts[(size_t) EngineType::FM] == 0b0010u);
    CHECK (leds.engine[(size_t) EngineType::Grain] == Led::Active);
    CHECK (leds.engineParts[(size_t) EngineType::Grain] == 0b1000u);
    CHECK (leds.engine[(size_t) EngineType::Wavetable] == Led::Off);
    CHECK (leds.engineParts[(size_t) EngineType::Wavetable] == 0u);
    // another edit part: its engine takes the focus, the edit part's former engine stays lit for parts 1 and 3
    in.editPart = 1;
    leds = ui::keyLeds (in);
    CHECK (leds.engine[(size_t) EngineType::FM] == Led::Focus);
    CHECK (leds.engine[(size_t) EngineType::Analog] == Led::Active);
}

TEST_CASE ("key LEDs: mode keys show the page in yellow and the status in violet / red")
{
    using ui::Led;
    using ui::Mode;
    ui::LedInputs in;
    in.page = Mode::Synth;
    auto leds = ui::keyLeds (in);
    for (int m = 0; m < ui::numModes; ++m)
        CHECK (leds.mode[(size_t) m] == ((Mode) m == Mode::Synth ? Led::Focus : Led::Off));

    in.sequencerRunning = true;
    in.drumSeqRunning = true;
    in.effectsActive = true;
    in.modulationActive = true;
    in.tapePlaying = true;
    leds = ui::keyLeds (in);
    CHECK (leds.mode[(size_t) Mode::Sequencer] == Led::Active);
    CHECK (leds.mode[(size_t) Mode::Drum] == Led::Active);
    CHECK (leds.mode[(size_t) Mode::Effects] == Led::Active);
    CHECK (leds.mode[(size_t) Mode::Lfo] == Led::Active);
    CHECK (leds.mode[(size_t) Mode::Tape] == Led::Active);
    CHECK (leds.mode[(size_t) Mode::Envelope] == Led::Off);
    CHECK (leds.mode[(size_t) Mode::Mixer] == Led::Off);
    // recording wins on the tape key, the status wins over the page colour
    in.tapeRecording = true;
    in.page = Mode::Sequencer;
    leds = ui::keyLeds (in);
    CHECK (leds.mode[(size_t) Mode::Tape] == Led::Record);
    CHECK (leds.mode[(size_t) Mode::Sequencer] == Led::Active);
    CHECK (leds.mode[(size_t) Mode::Synth] == Led::Off);
}

TEST_CASE ("key LEDs: the inputs are read from the core (sequencer, drums, FX rack, modulation, engines)")
{
    OhpeProcessor p;
    auto& core = p.core();
    auto& reg = core.params();
    auto set = [&reg] (const char* id, float v) { REQUIRE (reg.indexOf (id) >= 0); reg.set (id, v); };
    // a clean state: no slot, no routing, nothing running
    for (const auto& pre : FxRack::allSlotPrefixes (Core::numParts)) reg.set (pre + "type", 0.0f);
    for (int part = 1; part <= Core::numParts; ++part)
    {
        const std::string pp = "p" + std::to_string (part) + ".";
        reg.set (pp + "lfo.depth", 0.0f);
        for (int k = 1; k <= mod::numSlots; ++k) reg.set (pp + "mod.s" + std::to_string (k) + ".src", 0.0f);
    }
    set (Core::Ids::seqRun, 0.0f);
    auto in = ui::ledInputs (core, 0, ui::Mode::Synth, false, false);
    CHECK_FALSE (in.sequencerRunning);
    CHECK_FALSE (in.drumSeqRunning);
    CHECK_FALSE (in.effectsActive);
    CHECK_FALSE (in.modulationActive);

    set ("p2.engine", (float) EngineType::FM);
    set (Core::Ids::seqRun, 1.0f);
    set (Core::Ids::stepDrums, 1.0f);
    set ("p3.fx2.type", 1.0f); // Chorus
    set ("p4.lfo.depth", 0.5f);
    in = ui::ledInputs (core, 1, ui::Mode::Synth, true, false);
    CHECK (in.partEngine[1] == (int) EngineType::FM);
    CHECK (in.editPart == 1);
    CHECK (in.sequencerRunning);
    CHECK (in.drumSeqRunning);
    CHECK (in.effectsActive);
    CHECK (in.modulationActive);
    CHECK (in.tapeRecording);
    CHECK (in.tapePlaying); // recording rolls the tape
    // a bypassed slot is off; a matrix routing needs source, target and amount
    set ("p3.fx2.bypass", 1.0f);
    set ("p4.lfo.depth", 0.0f);
    set ("p1.mod.s3.src", (float) mod::SrcLfo2);
    set ("p1.mod.s3.dst", (float) mod::DstPitch);
    set ("p1.mod.s3.amt", 0.0f);
    in = ui::ledInputs (core, 0, ui::Mode::Synth, false, false);
    CHECK_FALSE (in.effectsActive);
    CHECK_FALSE (in.modulationActive);
    set ("p1.mod.s3.amt", 0.4f);
    CHECK (ui::ledInputs (core, 0, ui::Mode::Synth, false, false).modulationActive);
}

TEST_CASE ("key LEDs: the editor lights the edit part's engine and the shown mode")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectMode (ui::Mode::Envelope);
    e.advanceAnimation (1.0f / 30.0f);
    const auto leds = ui::keyLeds (ui::ledInputs (p.core(), 0, ui::Mode::Envelope, false, false));
    CHECK (leds.mode[(size_t) ui::Mode::Envelope] != ui::Led::Off);
    CHECK (leds.engine[(size_t) ui::selectedEngine (p.core(), 0)] == ui::Led::Focus);
    // rev 3: the keys show it as light bars - the edit part's segment of its engine yellow, the mode bar yellow
    auto& ek = e.engineKey (ui::selectedEngine (p.core(), 0));
    CHECK (ek.getBarSegmentCount() == 4);
    CHECK (ek.getSegmentLevel (0) > 0.0f);
    CHECK (ek.getSegmentColour (0) == ui::colour::sunYellow);
    for (int t = 0; t < numEngineTypes; ++t)
        if ((EngineType) t != ui::selectedEngine (p.core(), 0))
            for (int part = 0; part < 4; ++part)
                if ((int) ui::selectedEngine (p.core(), part) != t) CHECK (e.engineKey ((EngineType) t).getSegmentLevel (part) == 0.0f);
    CHECK (e.modeKey (ui::Mode::Envelope).getLedLevel() > 0.0f);
    CHECK (e.modeKey (ui::Mode::Envelope).getLedColour() == ui::colour::sunYellow);
    CHECK (e.modeKey (ui::Mode::Mixer).getLedLevel() == 0.0f);
    // part 2 on another engine: its segment lights violet on that engine's key
    p.core().params().set ("p2.engine", (float) EngineType::FM);
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.engineKey (EngineType::FM).getSegmentLevel (1) > 0.0f);
    CHECK (e.engineKey (EngineType::FM).getSegmentColour (1) == ui::colour::ultraviolet);
}

TEST_CASE ("transport light bars: PLAY violet while running, REC red, LOOP yellow, octave keys yellow when shifted")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.playTransportKey().getLedLevel() == 0.0f);
    CHECK (e.recordKey().getLedLevel() == 0.0f);
    CHECK (e.stopTransportKey().getLedLevel() == 0.0f);
    p.core().params().set (Core::Ids::seqRun, 1.0f);
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.playTransportKey().getLedLevel() > 0.0f);
    CHECK (e.playTransportKey().getLedColour() == ui::colour::ultraviolet);
    p.core().params().set (Core::Ids::seqRun, 0.0f);
    p.core().params().set ("tape.loop", 1.0f);
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.loopTransportKey().getLedLevel() > 0.0f);
    CHECK (e.loopTransportKey().getLedColour() == ui::colour::sunYellow);
    CHECK (e.playTransportKey().getLedLevel() == 0.0f);
    // octave keys
    CHECK (e.octaveUpKey().getLedLevel() == 0.0f);
    e.octaveUpKey().onClick();
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.octaveUpKey().getLedLevel() > 0.0f);
    CHECK (e.octaveDownKey().getLedLevel() == 0.0f);
    CHECK (e.keysReadoutComponent().asideText() == "+1");
    CHECK (e.keysReadoutComponent().mainText().startsWith ("C4"));
    e.octaveDownKey().onClick();
    e.octaveDownKey().onClick();
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.octaveDownKey().getLedLevel() > 0.0f);
    CHECK (e.octaveUpKey().getLedLevel() == 0.0f);
    // the tape readout shows the tape time
    CHECK (e.tapeReadoutComponent().mainText() == "0:00.0");
    CHECK (e.tapeReadoutComponent().asideText() == "1.1");
}
