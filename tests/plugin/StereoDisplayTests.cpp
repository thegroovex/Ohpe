// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.7: Mid/Side FX slot on every part, master stereo pages, the display visuals (stereo scope,
// spectrum terrain) fed from the scope tap, headless rendering of the deeper display.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

#include <algorithm>

using namespace ohpe;

namespace
{
void play (OhpeProcessor& p, int blocks, int channel = 1, int note = -1)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0 && note >= 0) midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

void setPlain (OhpeProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

/** Mean brightness of an image region (0..255). */
float brightness (const juce::Image& img, juce::Rectangle<int> r)
{
    double sum = 0.0;
    int n = 0;
    r = r.getIntersection (img.getBounds());
    for (int y = r.getY(); y < r.getBottom(); y += 2)
        for (int x = r.getX(); x < r.getRight(); x += 2, ++n)
        {
            const auto c = img.getPixelAt (x, y);
            sum += (c.getRed() + c.getGreen() + c.getBlue()) / 3.0;
        }
    return n > 0 ? (float) (sum / n) : 0.0f;
}
} // namespace

TEST_CASE ("editor 0.7: the Mid/Side slot page of part 2 shows the stereo scope of part 2, live")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();

    REQUIRE (p.parameter ("p2.fx.type") != nullptr);
    setPlain (p, "p2.fx.type", (float) InsertSlot::MidSide);
    setPlain (p, "p2.fx.midside.width", 1.6f);
    p.core().params().set ("p2.fx.type", (float) InsertSlot::MidSide);
    e.selectPart (1);
    e.selectMode (ui::Mode::Effects);
    d.onPageChange (3); // first FX slot page: type, width, mid, side
    CHECK (e.encoder (0).boundId() == "p2.fx.type");
    CHECK (e.encoder (1).boundId() == "p2.fx.midside.width");
    CHECK (d.shownVisual() == ui::Display::Visual::Scope);
    CHECK (d.scopeSource() == 1);

    e.advanceAnimation (1.0f / 30.0f); // selects the tap source
    CHECK (p.core().scope().getSource() == 1);
    play (p, 6, 2, 57);
    for (int i = 0; i < 4; ++i)
    {
        play (p, 2);
        e.advanceAnimation (1.0f / 30.0f);
    }
    CHECK (d.visualsState().framesSeen() > 1000);
    CHECK (d.visualsState().midLevelDb() > -60.0f);
    auto img = e.createComponentSnapshot (e.getLocalBounds());
    REQUIRE (img.isValid());
    // the scope area of the display is drawn (not empty glass)
    const auto area = d.characterArea().toNearestInt() + d.getPosition();
    CHECK (brightness (img, area) > 6.0f);

    // the second slot page (bass mono ...) still shows the scope; the delay pages do not
    d.onPageChange (1);
    CHECK (e.encoder (0).boundId() == "p2.fx.midside.bassmono");
    CHECK (d.shownVisual() == ui::Display::Visual::Scope);
    d.onPageChange (-4);
    CHECK (d.scopeSource() != 1);
}

TEST_CASE ("editor 0.7: master scope view, terrain, animation setting, tap released with the editor")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
        auto& d = e.displayComponent();
        d.setAnimatedVisuals (true);
        // the master stereo scope as the display view (here with Mid/Side in the master rack)
        setPlain (p, "mfx1.type", (float) InsertSlot::MidSide);
        setPlain (p, "mfx1.midside.width", 0.5f);
        p.core().params().set ("mfx1.type", (float) InsertSlot::MidSide);
        p.core().prepareEffects();
        e.selectMode (ui::Mode::Mixer);
        d.setVisual (ui::Display::Visual::Scope);
        CHECK (d.shownVisual() == ui::Display::Visual::Scope);
        CHECK (d.scopeSource() == ScopeTap::Master);
        e.advanceAnimation (1.0f / 30.0f);
        play (p, 2, 1, 64);
        for (int i = 0; i < 6; ++i)
        {
            play (p, 2);
            e.advanceAnimation (1.0f / 30.0f);
        }
        CHECK (d.visualsState().framesSeen() > 1000);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        d.setVisual (ui::Display::Visual::Character);

        // Synth mode: character with the terrain of the edit part behind it; views cycle
        e.selectMode (ui::Mode::Synth);
        CHECK (d.shownVisual() == ui::Display::Visual::Character);
        CHECK (d.scopeSource() == 0);
        play (p, 2, 1, 60);
        for (int i = 0; i < 12; ++i)
        {
            play (p, 2);
            e.advanceAnimation (1.0f / 30.0f);
        }
        CHECK_FALSE (d.visualsState().isResting());
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        d.setVisual (ui::Display::Visual::Terrain);
        CHECK (d.shownVisual() == ui::Display::Visual::Terrain);
        CHECK (d.scopeSource() == ScopeTap::Master);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        d.setVisual (ui::Display::Visual::Scope);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());

        // animation off: no terrain, no backdrop tap; the scope (a meter) stays available
        d.setAnimatedVisuals (false);
        d.setVisual (ui::Display::Visual::Terrain);
        CHECK (d.shownVisual() == ui::Display::Visual::Character);
        CHECK (d.scopeSource() == ScopeTap::None);
        d.setVisual (ui::Display::Visual::Character);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (p.core().scope().getSource() == ScopeTap::None);
        d.setAnimatedVisuals (true);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (p.core().scope().getSource() == 0);

        // every mode renders headless at two sizes, in both languages
        for (auto lang : { i18n::Language::German, i18n::Language::English })
        {
            e.setLanguage (lang);
            for (int m = 0; m < ui::numModes; ++m)
            {
                e.selectMode ((ui::Mode) m);
                e.advanceAnimation (1.0f / 30.0f);
                CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
            }
        }
        e.setSize (1800, 1200);
        e.selectMode (ui::Mode::Synth);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).getWidth() == 1800);
    }
    CHECK (p.core().scope().getSource() == ScopeTap::None);
    CHECK (i18n::allKeys().contains ("Correlation"));
}
