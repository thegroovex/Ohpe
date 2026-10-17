// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p24: the harmony brain on screen (LFO mode, view Harmony) and the isomorphic grid keyboard - the encoder page, the
// soft keys, playing the grid (MPE member channels, plain parts), the take-key trigger, both languages.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/HarmonyView.h"
#include "ui/ModFxViews.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
void play (OhpeProcessor& p, int blocks, juce::MidiBuffer first = {})
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

void setHost (OhpeProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

juce::MouseEvent mouseAt (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos)
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPos, now, 1, pos != downPos);
}

juce::MidiBuffer chord (std::initializer_list<int> notes, bool on)
{
    juce::MidiBuffer m;
    for (int n : notes) m.addEvent (on ? juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, n), 0);
    return m;
}
} // namespace

TEST_CASE ("editor p24: the Harmony view - page, chord heard from part 1, take the key, both languages")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (1);
    e.selectMode (ui::Mode::Lfo);
    e.selectView (ui::modfx::LfoViewHarmony);
    CHECK (e.encoder (0).boundId() == "harmony.p2.follow");
    CHECK (e.encoder (1).boundId() == "harmony.memory");
    CHECK (e.encoder (2).boundId() == "harmony.hold");
    CHECK (e.encoder (3).boundId() == "harmony.kb.layout");

    auto& core = p.core();
    // E minor: Em, Am, B7, Em
    for (int round = 0; round < 2; ++round)
        for (auto c : { std::initializer_list<int> { 52, 55, 59 }, { 57, 60, 64 }, { 59, 63, 66, 69 }, { 52, 55, 59, 64 } })
        {
            play (p, 30, chord (c, true));
            play (p, 2, chord (c, false));
        }
    CHECK (ui::harmonyview::chordName (core.harmony().chord()) == "Em");
    CHECK (core.harmony().keyRoot() == 4);
    CHECK (core.harmony().keyScale() == scales::Minor);

    // TAKE KEY (remote trigger, message thread): key.root / key.scale follow the brain
    setHost (p, "harmony.take", 1.0f);
    p.pollRemoteControls();
    setHost (p, "harmony.take", 0.0f);
    p.pollRemoteControls();
    CHECK ((int) std::lround (core.params().get ("key.root")) == 4);
    CHECK ((int) std::lround (core.params().get ("key.scale")) == scales::Minor);

    const auto before = i18n::current();
    i18n::setCurrent (i18n::Language::German);
    CHECK (ui::harmonyview::noteName (11) == "H");
    CHECK (ui::harmonyview::chordName ({ 11, harmony::Dom7 }) == "H7");
    CHECK (ui::harmonyview::keyName (4, scales::Minor) == "E Moll");
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        i18n::setCurrent (lang);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    i18n::setCurrent (i18n::Language::English);
    CHECK (ui::harmonyview::chordName ({ 11, harmony::Dom7 }) == "B7");
    CHECK (ui::harmonyview::chordName ({}) != "");
    CHECK (ui::harmonyview::statusLine (core, 1).isNotEmpty());
    i18n::setCurrent (before);

    // clear (trigger)
    setHost (p, "harmony.clear", 1.0f);
    play (p, 2);
    setHost (p, "harmony.clear", 0.0f);
    play (p, 2);
    CHECK_FALSE (core.harmony().heard());
}

TEST_CASE ("editor p24: the grid keyboard - GRID key swaps it in, cells play the edit part (MPE member channels)")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (0);
    CHECK_FALSE (e.gridKeyboardShown());
    REQUIRE (e.gridKeyComponent().onClick != nullptr);
    e.gridKeyComponent().onClick(); // GRID
    REQUIRE (e.gridKeyboardShown());
    CHECK_FALSE (e.keyboardComponent().isVisible());
    auto& g = e.gridKeyboard();
    CHECK (g.getWidth() > 0);
    CHECK (g.lowestNote() == 12 * (e.keyboardComponent().getBaseOctave() + 1));
    CHECK (g.noteAt (0, 1) == g.lowestNote() + 5); // Fourths: a row up = a fourth
    CHECK (g.highestNote() > g.lowestNote() + 24);

    // a plain part: the note on its channel
    const auto cell = g.cellArea (2, 0).getCentre();
    g.mouseDown (mouseAt (g, cell, cell));
    play (p, 4);
    CHECK (g.isPressed (g.noteAt (2, 0)));
    CHECK (p.core().lastNote() == g.noteAt (2, 0));
    CHECK (p.core().part (0).activeVoiceCount() >= 1);
    g.mouseUp (mouseAt (g, cell, cell));
    play (p, 4);
    CHECK_FALSE (g.isPressed (g.noteAt (2, 0)));

    // MPE part: a member channel, sliding sideways bends that note
    setHost (p, "p1.mpe", 1.0f);
    play (p, 2);
    const auto c2 = g.cellArea (5, 2).getCentre();
    g.mouseDown (mouseAt (g, c2, c2));
    g.mouseDrag (mouseAt (g, c2.translated (g.cellArea (0, 0).getWidth() * 2.0f, -10.0f), c2));
    play (p, 4);
    CHECK (p.core().lastNote() == g.noteAt (5, 2));
    CHECK (p.core().part (0).activeVoiceCount() >= 1);
    g.mouseUp (mouseAt (g, c2, c2));
    play (p, 4);

    // In Key layout: only key notes
    setHost (p, "harmony.kb.layout", (float) harmony::LayoutInKey);
    play (p, 2);
    const auto& h = p.core().harmony();
    for (int c = 0; c < ui::GridKeyboard::cols; ++c) CHECK (scales::inScale (g.noteAt (c, 0), h.keyRoot(), h.keyScale()));

    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        const auto before = i18n::current();
        i18n::setCurrent (lang);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        i18n::setCurrent (before);
    }
    e.showGridKeyboard (false);
    CHECK (e.keyboardComponent().isVisible());
    CHECK_FALSE (e.gridKeyboardShown());
}
