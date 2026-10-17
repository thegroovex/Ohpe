// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p26: the living display - the sound as a 3D object in the display's visual area: chosen like the other views,
// fed by the edit part (envelopes, LFOs, matrix) and its stereo picture, repainting only its own region while it
// moves and nothing once it has come to rest.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/LivingView.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
/** One display frame (1/30 s) of audio, then the animation tick. */
void frame (OhpeProcessor& p, OhpeEditor& e, juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 400);
    for (int b = 0; b < 4; ++b)
    {
        buffer.clear();
        p.processBlock (buffer, midi);
        midi.clear();
    }
    e.advanceAnimation (1.0f / 30.0f);
}
} // namespace

TEST_CASE ("editor p26: the living object - chosen, alive while playing, repaints only its region, rests after")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 400);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();
    e.selectPart (0);
    e.selectMode (ui::Mode::Synth);
    d.setVisual (ui::Display::Visual::Living);
    CHECK (d.shownVisual() == ui::Display::Visual::Living);
    CHECK (d.scopeSource() == 0); // the edit part's own signal
    frame (p, e);
    CHECK (d.livingOnScreen());
    CHECK (d.livingShape().isResting());

    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
    on.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 110), 0);
    frame (p, e, on);
    for (int f = 0; f < 20; ++f) frame (p, e);
    CHECK_FALSE (d.livingShape().isResting());
    CHECK (d.livingShape().envelope() > 0.05f);
    const auto dirty = d.livingRepaintArea();
    CHECK_FALSE (dirty.isEmpty());
    CHECK (d.characterArea().expanded (1.0f).contains (dirty)); // never more than the visual area
    CHECK (dirty.getWidth() * dirty.getHeight() < d.getWidth() * d.getHeight() * 0.42f);

    // the gathered inputs are sane
    const auto in = ui::living::gather (p.core(), 0, d.visualsState());
    CHECK (in.sounding);
    CHECK (in.level > 0.0f);
    CHECK (in.width >= 0.0f);
    CHECK (in.width <= 1.0f);

    // both languages paint
    const auto before = i18n::current();
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        i18n::setCurrent (lang);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    i18n::setCurrent (before);

    // release: the object fades and comes to rest - then it asks for no repaint at all
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    off.addEvent (juce::MidiMessage::allSoundOff (1), 0);
    frame (p, e, off);
    int f = 0;
    while (! d.livingShape().isResting() && f++ < 900) frame (p, e);
    CHECK (d.livingShape().isResting());
    frame (p, e);
    int repaints = 0;
    for (int k = 0; k < 30; ++k)
    {
        frame (p, e);
        repaints += d.livingRepaintArea().isEmpty() ? 0 : 1;
    }
    CHECK (repaints == 0);

    // the mixer shows the master as a living object; other views leave it alone
    e.selectMode (ui::Mode::Mixer);
    frame (p, e);
    CHECK (d.scopeSource() == (int) ScopeTap::Master);
    CHECK (d.livingOnScreen());
    e.selectMode (ui::Mode::Tape);
    frame (p, e);
    CHECK_FALSE (d.livingOnScreen());
    d.setVisual (ui::Display::Visual::Character);
    e.selectMode (ui::Mode::Synth);
    frame (p, e);
    CHECK_FALSE (d.livingOnScreen());
}
