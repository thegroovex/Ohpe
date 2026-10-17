// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p31: the SUB layer in the editor - the Synth encoder page "Sub", the pro view group with its picture (every synth
// engine), editing in the picture, both languages.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/Pages.h"
#include "ui/PluginEditor.h"
#include "ui/ProTiles.h"
#include "ui/ProView.h"

#include <algorithm>

using namespace ohpe;

namespace
{
float plain (OhpeProcessor& p, const char* id)
{
    auto* q = p.parameter (id);
    return q->convertFrom0to1 (q->getValue());
}
} // namespace

TEST_CASE ("editor p31: the Synth mode ends its engine pages with Glide, Pitch Envelope, Sub (then the layer)")
{
    OhpeProcessor p;
    for (int part = 0; part < Core::numParts; ++part)
    {
        ui::PageContext ctx;
        ctx.part = part;
        const auto ids = ui::modeParams (ui::Mode::Synth, p.core(), ctx);
        const auto titles = ui::pageTitles (ui::Mode::Synth, p.core(), ctx);
        REQUIRE (titles.size() == (size_t) ui::numPages (ids));
        const auto pp = "p" + std::to_string (part + 1) + ".";
        const auto it = std::find (ids.begin(), ids.end(), pp + "sub.level");
        REQUIRE (it != ids.end());
        const auto at = (size_t) std::distance (ids.begin(), it);
        CHECK (at % 4 == 0); // a page of its own
        CHECK (ids[at + 1] == pp + "sub.oct");
        CHECK (ids[at + 2] == pp + "sub.shape");
        CHECK (ids[at + 3] == pp + "sub.direct");
        CHECK (titles[at / 4] == "Sub");
        CHECK (titles[at / 4 - 1] == "Pitch Envelope");
        CHECK (titles[at / 4 - 2] == "Glide");
        if (at / 4 + 1 < titles.size()) CHECK (titles[at / 4 + 1] == "Layer");
    }
}

TEST_CASE ("editor p31: the pro view shows the Sub group with its picture on every synth engine; drag sets the level")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.setSize (1440, 960);
    e.selectPart (1);
    e.selectMode (ui::Mode::Synth);
    e.showProView (true);
    auto& pv = e.proViewComponent();
    for (auto t : { EngineType::FM, EngineType::Analog, EngineType::Wavetable, EngineType::Pluck, EngineType::Cluster, EngineType::Grain,
                    EngineType::Vocal, EngineType::Sampler })
    {
        CAPTURE (engineTypeInfo (t).id);
        e.selectEngine (t);
        const int c = pv.cardIndex ("Sub");
        REQUIRE (c >= 0);
        CHECK (pv.tile (c) != nullptr);
        const auto& ids = pv.cardGroup (c).ids;
        REQUIRE (ids.size() == 4);
        CHECK (ids[0] == "p2.sub.level");
    }
    e.selectEngine (EngineType::Analog);
    const int c = pv.cardIndex ("Sub");
    REQUIRE (c >= 0);
    auto* tile = pv.tile (c);
    REQUIRE (tile != nullptr);
    CHECK (plain (p, "p2.sub.level") == 0.0f);
    // drag up in the middle of the picture: more sub
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    const juce::Point<float> down ((float) tile->getWidth() * 0.4f, (float) tile->getHeight() * 0.6f);
    const auto at = down.translated (0.0f, -(float) tile->getHeight() * 0.5f);
    auto ev = [&] (juce::Point<float> pos) { return juce::MouseEvent (src, pos, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, tile, tile, now, down, now, 1, pos != down); };
    tile->mouseDown (ev (down));
    tile->mouseDrag (ev (at));
    tile->mouseUp (ev (at));
    CHECK (plain (p, "p2.sub.level") > 0.3f);
    // hover text in both languages, a valid picture in both
    const auto before = i18n::current();
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        i18n::setCurrent (lang);
        CHECK (ui::pro::explanation ("p2.sub.level").isNotEmpty());
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    i18n::setCurrent (i18n::Language::German);
    CHECK (i18n::tr ("Into filter") == "Ins Filter");
    CHECK (i18n::tr ("Direct") == "Direkt");
    i18n::setCurrent (before);
}
