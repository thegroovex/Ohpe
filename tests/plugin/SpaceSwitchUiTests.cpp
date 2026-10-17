// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p41: the 3D switch on screen (Effects mode, view 3D Space) - encoder 1 is a segmented switch Off / Headphones /
// Speakers that drives pN.space.on + space.out, the header badge follows with its own pictograms, and the strip
// below the rows never draws two texts over each other (EN + DE, both skins).

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/Palette.h"
#include "ui/PluginEditor.h"
#include "ui/SpaceView.h"

using namespace ohpe;

namespace
{
void play (OhpeProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos)
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, pos, now, 1, false);
}

float hostPlain (OhpeProcessor& p, const juce::String& id)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    return prm->convertFrom0to1 (prm->getValue());
}

void setPlain (OhpeProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

/** Clicks segment `k` of the switch on display row 0 and lets the processor / an audio block catch up. */
void clickSegment (OhpeProcessor& p, ui::Display& d, int k)
{
    const auto r = d.segmentArea (0, k, 3);
    d.mouseDown (mouseEvent (d, r.getCentre()));
    d.mouseUp (mouseEvent (d, r.getCentre()));
    play (p, 2);
}
} // namespace

TEST_CASE ("editor p41: the 3D switch - encoder 1 / a click selects Off, Headphones, Speakers; stage and monitoring follow")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();
    e.selectPart (0);
    e.selectMode (ui::Mode::Effects);
    e.selectView (3); // Sends | Rack | Master | 3D Space
    CHECK (e.encoder (0).boundId() == "p1.space.mode");
    CHECK (e.encoder (1).boundId() == "p1.space.path");

    // the three segments sit inside row 1, side by side, left to right
    for (int k = 0; k < 3; ++k)
    {
        const auto r = d.segmentArea (0, k, 3);
        CHECK (d.rowArea (0).contains (r));
        if (k > 0) CHECK (! r.intersects (d.segmentArea (0, k - 1, 3)));
        if (k > 0) CHECK (r.getX() > d.segmentArea (0, k - 1, 3).getX());
    }

    clickSegment (p, d, 1); // Headphones
    CHECK (hostPlain (p, "p1.space.mode") == 1.0f);
    CHECK (hostPlain (p, "p1.space.on") == 1.0f);
    CHECK (hostPlain (p, "space.out") == 0.0f);
    CHECK (ui::space3d::partState (p.core(), 0) == ui::space3d::State::Binaural);
    CHECK (ui::space3d::partMode (p.core(), 0) == 1);

    clickSegment (p, d, 2); // Speakers
    CHECK (hostPlain (p, "p1.space.on") == 1.0f);
    CHECK (hostPlain (p, "space.out") == 1.0f);
    CHECK (ui::space3d::partState (p.core(), 0) == ui::space3d::State::Speakers);
    CHECK (ui::space3d::partMode (p.core(), 0) == 2);

    clickSegment (p, d, 0); // Off
    CHECK (hostPlain (p, "p1.space.on") == 0.0f);
    CHECK (hostPlain (p, "space.out") == 1.0f); // the monitoring stays as chosen
    CHECK (ui::space3d::partState (p.core(), 0) == ui::space3d::State::Off);

    // host automation / MIDI of the switch itself (the timer keeps the three in step)
    setPlain (p, "p1.space.mode", 1.0f);
    p.syncSpaceModes();
    play (p, 2);
    CHECK (hostPlain (p, "p1.space.on") == 1.0f);
    CHECK (hostPlain (p, "space.out") == 0.0f);
    // the older IDs still work and move the switch
    setPlain (p, "space.out", 1.0f);
    p.syncSpaceModes();
    CHECK (hostPlain (p, "p1.space.mode") == 2.0f);
    setPlain (p, "p1.space.on", 0.0f);
    p.syncSpaceModes();
    CHECK (hostPlain (p, "p1.space.mode") == 0.0f);

    // a session round trip keeps all three
    setPlain (p, "p2.space.mode", 1.0f);
    p.syncSpaceModes();
    play (p, 2);
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    OhpeProcessor q;
    q.prepareToPlay (48000.0, 256);
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    q.syncSpaceModes();
    CHECK (hostPlain (q, "p2.space.mode") == 1.0f);
    CHECK (hostPlain (q, "p2.space.on") == 1.0f);
    CHECK (hostPlain (q, "space.out") == 0.0f);
    CHECK (hostPlain (q, "p1.space.mode") == 0.0f);
}

TEST_CASE ("editor p41: the 3D view's strip - the switch-on hint and the motion never share a line, texts fit (EN, DE, both skins)")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();
    e.selectPart (0);
    e.selectMode (ui::Mode::Effects);
    e.selectView (3);
    for (auto skin : { ui::Skin::Light, ui::Skin::Dark })
    {
        ui::applySkin (skin);
        for (auto lang : { i18n::Language::English, i18n::Language::German })
        {
            e.setLanguage (lang);
            for (int mode = 0; mode < 3; ++mode)
                for (int mono = 0; mono < 2; ++mono)
                {
                    setPlain (p, "p1.space.mode", (float) mode);
                    setPlain (p, "space.monocheck", (float) mono);
                    p.syncSpaceModes();
                    play (p, 2);
                    const auto lines = d.spaceStripLines();
                    REQUIRE (lines.size() == 2);
                    const bool hint = mode == 0;
                    CHECK ((lines[0].colour == ui::colour::sunYellow) == hint); // the hint takes line 1 alone
                    for (size_t a = 0; a < lines.size(); ++a)
                    {
                        const auto& t = lines[a];
                        INFO ("lang " << (int) lang << " mode " << mode << " line " << a << ": " << t.text.toStdString());
                        CHECK (t.text.isNotEmpty());
                        CHECK (d.getLocalBounds().toFloat().contains (t.area));
                        // fits: drawn on one line, narrowed at most to minScale
                        const float w = juce::GlyphArrangement::getStringWidth (t.font, t.text);
                        CHECK (w * t.minScale <= t.area.getWidth() + 1.0f);
                        for (size_t b = a + 1; b < lines.size(); ++b) CHECK (! t.area.intersects (lines[b].area));
                        for (int r = 0; r < 4; ++r) CHECK (! t.area.intersects (d.rowArea (r)));
                    }
                }
        }
    }
    ui::applySkin (ui::Skin::Light);
    e.setLanguage (i18n::Language::English);
}
