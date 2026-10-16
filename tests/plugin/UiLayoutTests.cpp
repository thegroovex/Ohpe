// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p14: the redesigned plate - no visible controls overlap in any view (both languages, 1x and 1.5x),
// everything sits inside the housing, the performance wheels send pitch bend / mod wheel to the
// parts (MIDI-learnable, the pitch wheel springs back), the glide controls follow the edit part.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "mod/ModVoice.h"

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
            for (int m = 0; m < ui::numModes; ++m)
            {
                CAPTURE (m);
                e.selectMode ((ui::Mode) m);
                checkLayout (e);
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

TEST_CASE ("editor layout: wheels left of the keyboard, glide right of it, transport + MIDI bottom left")
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
    // the transport cluster and the MIDI key share the bottom-left panel, under the engine column
    const auto panel = OhpeEditor::designArea ("transportPanel");
    for (auto* c : { (juce::Component*) &e.recordKey(), (juce::Component*) &e.playTransportKey(), (juce::Component*) &e.midiKeyComponent() })
    {
        CHECK (panel.contains (c->getBounds().toFloat()));
        CHECK (c->isVisible());
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
