// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p22: the motion recorder on screen (LFO mode, view Motion) - the encoder page, recording a knob movement through the
// host parameters while the internal transport runs, painting the lanes in both languages.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/ModFxViews.h"
#include "ui/MotionView.h"
#include "ui/PluginEditor.h"

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

void setHost (OhpeProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}
} // namespace

TEST_CASE ("editor p22: the Motion view - page, recording a knob through the host, lanes on screen")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (1);
    e.selectMode (ui::Mode::Lfo);
    e.selectView (ui::modfx::LfoViewMotion);
    CHECK (e.encoder (0).boundId() == "p2.motion.on");
    CHECK (e.encoder (1).boundId() == "p2.motion.rec");
    CHECK (e.encoder (2).boundId() == "p2.motion.len");
    CHECK (e.encoder (3).boundId() == "p2.motion.amt");

    auto& core = p.core();
    setHost (p, "p2.motion.rec", 1.0f);
    setHost (p, "seq.run", 1.0f);
    play (p, 4);
    for (int k = 0; k < 30; ++k)
    {
        setHost (p, "p2.macro1", 0.02f + 0.03f * (float) k);
        play (p, 2);
    }
    CHECK (core.motion().laneTarget (1, 0) == core.params().indexOf ("p2.macro1"));
    CHECK (core.motion().laneTarget (1, 1) == -1);
    CHECK (ui::motionview::laneTitle (core, 1, 0).isNotEmpty());

    const auto before = i18n::current();
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        i18n::setCurrent (lang);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    i18n::setCurrent (i18n::Language::English);
    CHECK (ui::motionview::laneTitle (core, 1, 3) == "Empty");
    i18n::setCurrent (before);

    setHost (p, "p2.motion.clear", 1.0f);
    play (p, 2);
    CHECK_FALSE (core.motion().anyLane (1));
    setHost (p, "p2.motion.clear", 0.0f);
    setHost (p, "seq.run", 0.0f);
    play (p, 2);
}
