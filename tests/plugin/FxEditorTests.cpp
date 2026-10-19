// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p55: the graphical FX slot editor (Effects mode, Rack / Master): the points of every effect type map to real
// parameters and stay in range, dragging moves the right values as host gestures, a selected point becomes the encoders'
// focus page, the encoders stay live below the editor, nothing changes by opening it.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/FxEditor.h"
#include "ui/ModFxViews.h"
#include "ui/PluginEditor.h"

#include <cmath>
#include <algorithm>
#include <map>

using namespace ohpe;

namespace
{
void setPlain (OhpeProcessor& p, const std::string& id, float plain)
{
    auto* prm = p.parameter (juce::String (id));
    REQUIRE (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

/** One audio block: the registry takes over the host values (as in the running app). */
void runBlock (OhpeProcessor& p)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    juce::MidiBuffer midi;
    buffer.clear();
    p.processBlock (buffer, midi);
}

float plainOf (OhpeProcessor& p, const std::string& id)
{
    auto* prm = p.parameter (juce::String (id));
    REQUIRE (prm != nullptr);
    return prm->convertFrom0to1 (prm->getValue());
}

std::map<std::string, float> snapshot (OhpeProcessor& p)
{
    std::map<std::string, float> m;
    for (const auto& d : p.core().params().all())
        if (auto* prm = p.parameter (juce::String (d.id))) m[d.id] = prm->getValue();
    return m;
}

struct GestureCounter final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0;
    void parameterValueChanged (int, float) override { ++changes; }
    void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
};

OhpeEditor& openRack (OhpeProcessor& p, std::unique_ptr<juce::AudioProcessorEditor>& ed, int type)
{
    setPlain (p, "p1.fx.type", (float) type);
    p.core().prepareEffects();
    ed.reset (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.setSize (OhpeEditor::designWidth, OhpeEditor::designHeight);
    e.selectPart (0);
    e.selectMode (ui::Mode::Effects);
    e.selectView (ui::modfx::FxViewRack);
    e.selectFxSlot (0);
    e.showFxEditor (true);
    return e;
}
} // namespace

TEST_CASE ("FX editor p55: every type's points map to existing parameters and stay in range when dragged anywhere")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    auto& e = openRack (p, ed, InsertSlot::Eq);
    REQUIRE (e.fxEditorVisible());
    auto& fx = e.fxEditorComponent();
    for (int t = InsertSlot::Off; t < InsertSlot::numTypes; ++t)
    {
        CAPTURE (t);
        setPlain (p, "p1.fx.type", (float) t);
        p.core().prepareEffects();
        fx.refresh();
        auto* g = fx.graph();
        REQUIRE (g != nullptr);
        CHECK (g->getWidth() > 100);
        CHECK (fx.numBars() >= 3); // type, mix, bypass + the effect's parameters
        if (t != InsertSlot::Off && t != InsertSlot::Space3D && t != InsertSlot::Resonator) CHECK (g->numHandles() >= 1);
        const auto& lay = g->layout();
        for (int h = 0; h < g->numHandles(); ++h)
        {
            CAPTURE (h);
            const auto& hd = lay.handles[(size_t) h];
            CHECK (hd.region >= 0);
            CHECK (hd.region < (int) lay.regions.size());
            for (const auto* id : { &hd.x, &hd.y, &hd.alt, &hd.aux })
                if (! id->empty()) CHECK (p.parameter (juce::String (*id)) != nullptr);
            CHECK ((! hd.x.empty() || ! hd.y.empty()));
            // the point lies in its region, wherever it is dragged
            const auto r = g->regionArea (hd.region);
            for (auto at : { r.getTopLeft(), r.getBottomRight(), r.getCentre(), r.getTopLeft() - juce::Point<float> (50.0f, 50.0f),
                             r.getBottomRight() + juce::Point<float> (50.0f, 50.0f) })
            {
                g->dragHandleTo (h, at);
                const auto pos = g->handlePosition (h);
                CHECK (std::isfinite (pos.x));
                CHECK (std::isfinite (pos.y));
                CHECK (r.expanded (1.0f).contains (pos));
                for (const auto& id : g->handleParams (h))
                {
                    const float v = plainOf (p, id);
                    const auto& d = p.core().params().descriptor (p.core().params().indexOf (id));
                    CHECK (std::isfinite (v));
                    CHECK (v >= d.minValue - 1.0e-3f);
                    CHECK (v <= d.maxValue + 1.0e-3f);
                }
            }
        }
        // the picture paints for every type (both skins share the code; one render is enough)
        CHECK (g->createComponentSnapshot (g->getLocalBounds()).isValid());
    }
}

TEST_CASE ("FX editor p55: EQ band point - drag moves frequency and gain as host gestures; axis round trip")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    auto& e = openRack (p, ed, InsertSlot::Eq);
    auto* g = e.fxEditorComponent().graph();
    REQUIRE (g != nullptr);
    int band2 = -1;
    for (int h = 0; h < g->numHandles(); ++h)
        if (g->layout().handles[(size_t) h].x == "p1.fx.eq.p2freq") band2 = h;
    REQUIRE (band2 >= 0);
    CHECK (g->layout().handles[(size_t) band2].y == "p1.fx.eq.p2gain");
    CHECK (g->layout().handles[(size_t) band2].alt == "p1.fx.eq.p2q");

    GestureCounter count;
    auto* freq = p.parameter ("p1.fx.eq.p2freq");
    freq->addListener (&count);
    const float f0 = plainOf (p, "p1.fx.eq.p2freq"), g0 = plainOf (p, "p1.fx.eq.p2gain");
    g->dragHandleTo (band2, g->handlePosition (band2) + juce::Point<float> (60.0f, -40.0f));
    CHECK (plainOf (p, "p1.fx.eq.p2freq") > f0 * 1.2f);
    CHECK (plainOf (p, "p1.fx.eq.p2gain") > g0 + 1.0f);
    CHECK (count.begins == 1);
    CHECK (count.ends == 1);
    freq->removeListener (&count);

    // fraction <-> value on a log axis (20 Hz .. 20 kHz) is consistent
    const auto& hd = g->layout().handles[(size_t) band2];
    for (float f : { 0.2f, 0.5f, 0.8f })
    {
        auto* q = p.parameter ("p1.fx.eq.p2freq");
        q->setValueNotifyingHost (ui::fxgraph::normalisedFor (p, hd.ax, hd.x, f));
        CHECK (ui::fxgraph::fractionOf (p, hd.ax, hd.x) == doctest::Approx (f).epsilon (0.01));
    }
}

TEST_CASE ("FX editor p55: compressor ratio point, multiband band follows its splits")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    auto& e = openRack (p, ed, InsertSlot::Compressor);
    auto* g = e.fxEditorComponent().graph();
    REQUIRE (g != nullptr);
    REQUIRE (g->numHandles() >= 2);
    CHECK (g->layout().handles[1].special == ui::fxgraph::Special::CompRatio);
    const float r0 = plainOf (p, "p1.fx.compressor.ratio");
    g->dragHandleTo (1, g->handlePosition (1) + juce::Point<float> (0.0f, 30.0f)); // lower end = stronger ratio
    CHECK (plainOf (p, "p1.fx.compressor.ratio") > r0);
    // the threshold point sits on the 1:1 line
    const auto th = g->handlePosition (0);
    const auto r = g->regionArea (0);
    CHECK ((th.x - r.getX()) / r.getWidth() == doctest::Approx ((r.getBottom() - th.y) / r.getHeight()).epsilon (0.02));

    setPlain (p, "p1.fx.type", (float) InsertSlot::Multiband);
    p.core().prepareEffects();
    e.fxEditorComponent().refresh();
    g = e.fxEditorComponent().graph();
    REQUIRE (g != nullptr);
    const float x0 = g->handlePosition (0).x;
    setPlain (p, "p1.fx.multiband.lowsplit", 400.0f);
    CHECK (g->handlePosition (0).x > x0); // the low band's point moves with its upper edge
}

TEST_CASE ("FX editor p55: a selected point is the encoders' focus page; closing restores the pages; layout and no side effects")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    setPlain (p, "p1.fx.type", (float) InsertSlot::Eq);
    p.core().prepareEffects();
    runBlock (p);
    ed.reset (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.setSize (OhpeEditor::designWidth, OhpeEditor::designHeight);
    e.selectMode (ui::Mode::Effects);
    e.selectView (ui::modfx::FxViewRack);
    e.selectFxSlot (0);
    const auto before = snapshot (p);
    e.showFxEditor (true);
    REQUIRE (e.fxEditorVisible());
    CHECK (snapshot (p) == before); // opening changes nothing
    // the editor lies over the display and the columns; the encoders stay visible and below it
    CHECK_FALSE (e.displayComponent().isVisible());
    for (int i = 0; i < 4; ++i)
    {
        CHECK (e.encoder (i).isVisible());
        CHECK_FALSE (e.fxEditorComponent().getBounds().intersects (e.encoder (i).getBounds()));
    }
    CHECK (e.fxEditorComponent().createComponentSnapshot (e.fxEditorComponent().getLocalBounds()).isValid());

    auto* g = e.fxEditorComponent().graph();
    REQUIRE (g != nullptr);
    int band3 = -1;
    for (int h = 0; h < g->numHandles(); ++h)
        if (g->layout().handles[(size_t) h].x == "p1.fx.eq.p3freq") band3 = h;
    REQUIRE (band3 >= 0);
    g->selectHandle (band3);
    CHECK (e.encoder (0).boundId() == "p1.fx.eq.p3freq");
    CHECK (e.encoder (1).boundId() == "p1.fx.eq.p3gain");
    CHECK (e.encoder (2).boundId() == "p1.fx.eq.p3q");
    // a slot of another effect ignores the focus
    ui::PageContext c;
    c.fxView = ui::modfx::FxViewRack;
    c.fxSlot = 1;
    c.fxFocus = { "p1.fx.eq.p3freq" };
    const auto other = ui::modeParams (ui::Mode::Effects, p.core(), c);
    CHECK (std::find (other.begin(), other.end(), "p1.fx.eq.p3freq") == other.end());

    e.showFxEditor (false);
    CHECK_FALSE (e.fxEditorVisible());
    CHECK (e.displayComponent().isVisible());
    CHECK (e.encoder (0).boundId() == "p1.fx.type");
    // the editor closes when the view changes
    e.showFxEditor (true);
    e.selectView (ui::modfx::FxViewSends);
    CHECK_FALSE (e.fxEditorVisible());
    // only Rack / Master open it
    e.showFxEditor (true);
    CHECK_FALSE (e.fxEditorVisible());
    e.selectView (ui::modfx::FxViewMaster);
    e.showFxEditor (true);
    CHECK (e.fxEditorVisible());
    CHECK (e.fxEditorComponent().numSlots() == FxRack::numMasterSlots);
    CHECK (e.fxEditorComponent().shownView() == ui::modfx::FxViewMaster);
}

TEST_CASE ("FX editor p57: the picture follows an outside parameter change at the editor's 30 Hz; curves are cached")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    auto& e = openRack (p, ed, InsertSlot::Eq);
    auto& fx = e.fxEditorComponent();
    auto* g = fx.graph();
    REQUIRE (g != nullptr);
    REQUIRE (g->getWidth() > 0);
    int band2 = -1;
    for (int h = 0; h < g->numHandles(); ++h)
        if (g->layout().handles[(size_t) h].y == "p1.fx.eq.p2gain") band2 = h;
    REQUIRE (band2 >= 0);

    (void) g->createComponentSnapshot (g->getLocalBounds());
    const int built = g->curveBuilds();
    CHECK (built >= 1);
    (void) g->createComponentSnapshot (g->getLocalBounds()); // same values: no rebuild
    CHECK (g->curveBuilds() == built);

    // an encoder / MIDI / host change (no mouse): one editor refresh lets the picture see it
    const float y0 = g->handlePosition (band2).y;
    const int ticks = fx.graphTicks();
    setPlain (p, "p1.fx.eq.p2gain", 12.0f);
    fx.refresh();
    CHECK (fx.graphTicks() == ticks + 1);
    CHECK (g->handlePosition (band2).y < y0 - 5.0f); // the point moved up
    (void) g->createComponentSnapshot (g->getLocalBounds());
    CHECK (g->curveBuilds() == built + 1); // the curve was rebuilt once for the new value
}

TEST_CASE ("FX editor p57: German EQ names use the studio terms (Low / High Shelf, Q, Low / High Cut)")
{
    OhpeProcessor p;
    std::map<std::string, std::string> de;
    for (const auto& d : p.core().params().all()) de[d.id] = d.nameDe;
    auto endsWith = [] (const std::string& s, const std::string& t) { return s.size() >= t.size() && s.compare (s.size() - t.size(), t.size(), t) == 0; };
    CHECK (endsWith (de["p1.fx.eq.lowfreq"], "EQ Low Shelf Frequenz"));
    CHECK (endsWith (de["p1.fx.eq.highgain"], "EQ High Shelf Gain"));
    CHECK (endsWith (de["p1.fx.eq.p2q"], "EQ Band 2 Q"));
    CHECK (ui::modfx::compactName ("p1.fx.eq.highfreq", "EQ High Shelf Freq", de["p1.fx.eq.highfreq"]).length() <= 19);
    for (const auto& [id, name] : de)
    {
        CHECK_MESSAGE (name.find ("Kuhschwanz") == std::string::npos, id);
        CHECK_MESSAGE (name.find ("G\xc3\xbcte") == std::string::npos, id);
    }
}
