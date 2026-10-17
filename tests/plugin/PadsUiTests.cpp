// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p25: the performance pads on screen (Effects mode, view Pads) - the encoder pages, a pad thrown through its host
// parameter while the internal transport runs, painting the pads in both languages.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/ModFxViews.h"
#include "ui/PadsView.h"
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

TEST_CASE ("editor p25: the Pads view - pages, a pad thrown through the host, pads on screen")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectMode (ui::Mode::Effects);
    e.selectView (ui::modfx::FxViewPads);
    CHECK (e.encoder (0).boundId() == "pads.stutter");
    CHECK (e.encoder (1).boundId() == "pads.stop");
    CHECK (e.encoder (2).boundId() == "pads.sweep");
    CHECK (e.encoder (3).boundId() == "pads.freeze");
    CHECK (ui::padsview::padAt ({ 0.0f, 0.0f, 100.0f, 100.0f }, { 75.0f, 75.0f }) == 3);

    auto& core = p.core();
    setHost (p, "pads.quant", 0.0f);
    setHost (p, "pads.sweep", 1.0f);
    setHost (p, "pads.freeze", 1.0f);
    play (p, 4);
    CHECK (core.pads().uiState (PerformancePads::Sweep) == PerformancePads::Engaged);
    CHECK (core.pads().uiState (PerformancePads::Freeze) == PerformancePads::Engaged);

    const auto before = i18n::current();
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        i18n::setCurrent (lang);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        CHECK (ui::padsview::statusLine (core).isNotEmpty());
    }
    i18n::setCurrent (before);

    setHost (p, "pads.sweep", 0.0f);
    setHost (p, "pads.freeze", 0.0f);
    play (p, 4);
    CHECK (core.pads().uiState (PerformancePads::Sweep) != PerformancePads::Engaged);
    CHECK (core.pads().uiState (PerformancePads::Freeze) != PerformancePads::Engaged);
}
