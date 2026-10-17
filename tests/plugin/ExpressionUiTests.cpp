// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p33: the Expression pages on screen (Envelope mode: Velocity, Aftertouch, Expression Curves), the pro view group
// Expression, both languages.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

#include <algorithm>

using namespace ohpe;

TEST_CASE ("editor p33: the Expression pages - Velocity, Aftertouch, Curves; the pro view group; painted EN / DE")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (1);
    e.selectMode (ui::Mode::Envelope);

    ui::PageContext ctx;
    ctx.part = 1;
    const auto ids = ui::modeParams (ui::Mode::Envelope, p.core(), ctx);
    const auto titles = ui::pageTitles (ui::Mode::Envelope, p.core(), ctx);
    REQUIRE (titles.size() == 7);
    CHECK (titles[4] == "Velocity");
    CHECK (titles[5] == "Aftertouch");
    CHECK (titles[6] == "Expression Curves");
    CHECK (ids[16] == "p2.expr.vel.dst1");
    CHECK (ids[17] == "p2.expr.vel.amt1");
    CHECK (ids[20] == "p2.expr.at.dst1");
    CHECK (ids[27] == "p2.expr.at.curve2");

    // the pro view (Synth): one Expression group with all twelve, no stray page groups
    const auto groups = ui::proGroups (ui::Mode::Synth, p.core(), ctx);
    const auto it = std::find_if (groups.begin(), groups.end(), [] (const ui::ParamGroup& g) { return g.title == "Expression"; });
    REQUIRE (it != groups.end());
    CHECK (it->ids.size() == 12);
    for (const char* t : { "Velocity", "Aftertouch", "Expression Curves" })
        CHECK (std::none_of (groups.begin(), groups.end(), [t] (const ui::ParamGroup& g) { return g.title == t; }));

    // the Aftertouch page painted in both languages, with an active slot and pressure
    e.displayComponent().onPageChange (5); // a page delta: Amp Envelope -> Aftertouch
    CHECK (e.encoder (0).boundId() == "p2.expr.at.dst1");
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        if (auto* q = p.parameter ("p2.expr.at.amt1")) q->setValueNotifyingHost (q->convertTo0to1 (0.5f));
        juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::channelPressureChange (2, 90), 0);
        p.processBlock (buffer, midi);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    CHECK (i18n::tr ("Expression Curves").isNotEmpty());
    e.setLanguage (i18n::Language::German);
    CHECK (i18n::tr ("Expression") == "Ausdruck");
    e.setLanguage (i18n::Language::English);
}
