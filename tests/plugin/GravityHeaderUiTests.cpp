// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p70: Key Gravity on the display's header - the ring next to the key exists, reflects pN.grav.amt of the edit part,
// opens the Key Gravity encoder page, wheel / drag set the amount, and it never shifts TAP / tempo or covers the key.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/ModFxViews.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
juce::MouseEvent gravMouse (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos, juce::ModifierKeys mods = {})
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPos, now, 1, pos != downPos);
}
} // namespace

TEST_CASE ("p70 header gravity: exists next to the key, reflects the amount, opens the Key Gravity page, wheel / drag")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<OhpeEditor*> (ed.get());
    REQUIRE (e != nullptr);
    auto& d = e->displayComponent();
    auto& g = d.headerGravity();
    auto& reg = p.core().params();

    e->selectPart (0);
    e->selectMode (ui::Mode::Synth);
    e->advanceAnimation (0.05f);
    REQUIRE (g.isVisible());
    CHECK (g.paramId() == "p1.grav.amt");
    CHECK (g.getWidth() > 8);
    CHECK (g.getRight() <= d.headerKey().getX());           // right next to the key, left of it
    CHECK (g.getRight() >= d.headerKey().getX() - 2);
    CHECK_FALSE (g.getBounds().intersects (d.headerTempo().getBounds()));
    CHECK_FALSE (g.getBounds().intersects (d.headerTap().getBounds()));
    CHECK (g.shownAmount() == doctest::Approx (0.0f)); // empty at 0 %
    CHECK (g.getTooltip().contains ("Key Gravity"));

    // reflects the amount of the edit part
    reg.set ("p1.grav.amt", 0.6f);
    e->advanceAnimation (0.05f);
    CHECK (g.shownAmount() == doctest::Approx (0.6f));
    e->selectPart (1);
    e->advanceAnimation (0.05f);
    CHECK (g.paramId() == "p2.grav.amt");
    CHECK (g.shownAmount() == doctest::Approx (0.0f));
    e->selectPart (0);
    e->advanceAnimation (0.05f);

    // wheel: +- 5 % steps
    const juce::Point<float> c (g.getWidth() * 0.5f, g.getHeight() * 0.5f);
    g.mouseWheelMove (gravMouse (g, c, c), juce::MouseWheelDetails { 0.0f, 1.0f, false, false, false });
    CHECK (reg.get ("p1.grav.amt") == doctest::Approx (0.65f).epsilon (0.001));
    g.mouseWheelMove (gravMouse (g, c, c), juce::MouseWheelDetails { 0.0f, -1.0f, false, false, false });
    g.mouseWheelMove (gravMouse (g, c, c), juce::MouseWheelDetails { 0.0f, -1.0f, false, false, false });
    CHECK (reg.get ("p1.grav.amt") == doctest::Approx (0.55f).epsilon (0.001));
    // drag down lowers it, and a drag does not open the page
    const auto modeBefore = e->currentMode();
    g.mouseDown (gravMouse (g, c, c));
    g.mouseDrag (gravMouse (g, c.translated (0.0f, 30.0f), c));
    g.mouseUp (gravMouse (g, c.translated (0.0f, 30.0f), c));
    CHECK (reg.get ("p1.grav.amt") < 0.55f);
    CHECK (e->currentMode() == modeBefore);
    // clamped to 0 .. 1
    for (int i = 0; i < 40; ++i) g.mouseWheelMove (gravMouse (g, c, c), juce::MouseWheelDetails { 0.0f, 1.0f, false, false, false });
    CHECK (reg.get ("p1.grav.amt") == doctest::Approx (1.0f));

    // a click opens the Key Gravity page (Effects mode, Sends view) with Gravity on encoder 1
    g.mouseDown (gravMouse (g, c, c));
    g.mouseUp (gravMouse (g, c, c));
    CHECK (e->currentMode() == ui::Mode::Effects);
    CHECK (e->pageContext().fxView == ui::modfx::FxViewSends);
    const auto ids = ui::modeParams (ui::Mode::Effects, p.core(), e->pageContext());
    const int page = e->shownPage();
    REQUIRE ((size_t) page * 4 < ids.size());
    CHECK (ids[(size_t) page * 4] == "p1.grav.amt");
    const auto titles = ui::pageTitles (ui::Mode::Effects, p.core(), e->pageContext());
    CHECK (titles[(size_t) page] == "Key Gravity");

    // from another part and another FX view it opens that part's page
    e->selectPart (2);
    e->selectView (ui::modfx::FxViewRack);
    e->advanceAnimation (0.05f);
    e->openKeyGravity();
    CHECK (e->pageContext().fxView == ui::modfx::FxViewSends);
    const auto ids3 = ui::modeParams (ui::Mode::Effects, p.core(), e->pageContext());
    CHECK (ids3[(size_t) e->shownPage() * 4] == "p3.grav.amt");

    // German tooltip
    e->setLanguage (i18n::Language::German);
    CHECK (g.getTooltip().contains ("Tonart-Gravitation"));
    e->setLanguage (i18n::Language::English);
}

TEST_CASE ("p70 header gravity: fits every mode and size without overlapping the key, TAP, tempo or the 3D badge (EN / DE)")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<OhpeEditor*> (ed.get());
    REQUIRE (e != nullptr);
    for (int w : { OhpeEditor::designWidth * 2 / 3, OhpeEditor::designWidth, OhpeEditor::designWidth * 2 })
    {
        e->setSize (w, w * OhpeEditor::designHeight / OhpeEditor::designWidth);
        for (auto lang : { i18n::Language::English, i18n::Language::German })
            for (auto mode : { ui::Mode::Synth, ui::Mode::Envelope, ui::Mode::Effects, ui::Mode::Mixer, ui::Mode::Tape })
            {
                e->setLanguage (lang);
                e->selectPart (0);
                e->selectMode (mode);
                if (mode == ui::Mode::Synth) e->selectEngine (EngineType::Vocal);
                p.setKeyFromUi (6, (int) scales::HarmonicMinor);
                auto& d = e->displayComponent();
                e->advanceAnimation (0.05f);
                (void) d.createComponentSnapshot (d.getLocalBounds());
                const auto& g = d.headerGravity();
                REQUIRE (g.isVisible());
                CHECK (g.getWidth() > 6);
                CHECK (g.getRight() <= d.headerKey().getX());
                CHECK_FALSE (g.getBounds().toFloat().intersects (d.spaceBadgeArea()));
                CHECK_FALSE (g.getBounds().intersects (d.headerTap().getBounds()));
                CHECK_FALSE (g.getBounds().intersects (d.headerTempo().getBounds()));
            }
    }
    e->setSize (OhpeEditor::designWidth, OhpeEditor::designHeight);
    e->setLanguage (i18n::Language::English);
}
