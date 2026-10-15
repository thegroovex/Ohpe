// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Plug-in layer tests: buses, parameters bound to the registry, state, MIDI learn bridge,
// rendering through the processor, editor construction and localisation.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"
#include "plugin/PluginProcessor.h"
#include "standalone/AudioSettingsPage.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
float renderNote (OhpeProcessor& p, int blocks, int busForPeak, bool sendNote = true)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (b == 0 && sendNote) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
        auto bus = p.getBusBuffer (buffer, false, busForPeak);
        for (int ch = 0; ch < bus.getNumChannels(); ++ch) peak = juce::jmax (peak, bus.getMagnitude (ch, 0, 512));
    }
    return peak;
}

void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil (120); }
} // namespace

TEST_CASE ("processor: buses are main + 8 aux stereo outputs, MIDI in, one audio input (off by default)")
{
    OhpeProcessor p;
    CHECK (p.getBusCount (false) == 1 + numAuxOutputs);
    CHECK (p.getBusCount (true) == 1); // 0.2: tape recording input
    CHECK_FALSE (p.getBus (true, 0)->isEnabled());
    CHECK (p.acceptsMidi());
    CHECK (p.getBus (false, 0)->isEnabled());
    CHECK_FALSE (p.getBus (false, 1)->isEnabled()); // aux off by default in hosts
    CHECK (p.getBus (false, 0)->getName() == "Main");
    CHECK (p.getBus (false, 8)->getName() == "Out 8");
    CHECK (p.enableAllBuses());
    CHECK (p.getTotalNumOutputChannels() == 2 * (1 + numAuxOutputs));

    auto layout = p.getBusesLayout();
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK_FALSE (p.checkBusesLayoutSupported (layout));
}

TEST_CASE ("processor: one host parameter per registry entry, same order and IDs")
{
    OhpeProcessor p;
    const auto& reg = p.core().params();
    auto& params = p.getParameters();
    REQUIRE (params.size() == reg.size());
    for (int i = 0; i < reg.size(); ++i)
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params[i]);
        REQUIRE (rp != nullptr);
        CHECK (rp->getParameterID() == juce::String (reg.descriptor (i).id));
        // the same normalisation on both sides
        const auto& d = reg.descriptor (i);
        CHECK (rp->convertTo0to1 (d.defaultValue) == doctest::Approx (d.toNormalised (d.defaultValue)).epsilon (1e-4));
        CHECK (rp->getDefaultValue() == doctest::Approx (d.toNormalised (d.defaultValue)).epsilon (1e-4));
    }
}

TEST_CASE ("processor: host parameter changes reach the core, note renders on main")
{
    OhpeProcessor p;
    p.enableAllBuses();
    p.prepareToPlay (48000.0, 512);
    CHECK (renderNote (p, 4, 0, false) == 0.0f);
    auto* engine = p.parameter ("p1.engine");
    engine->setValueNotifyingHost (engine->convertTo0to1 ((float) EngineType::FM));
    CHECK (renderNote (p, 20, 0) > 0.01f);
    CHECK (p.core().part (0).currentEngineType() == EngineType::FM);
}

TEST_CASE ("processor: mixer routing to an enabled aux bus")
{
    OhpeProcessor p;
    p.enableAllBuses();
    p.prepareToPlay (48000.0, 512);
    auto* out = p.parameter ("mix.ch1.out");
    out->setValueNotifyingHost (out->convertTo0to1 (4.0f));
    CHECK (renderNote (p, 20, 4) > 0.01f);
    OhpeProcessor q;
    q.enableAllBuses();
    q.prepareToPlay (48000.0, 512);
    q.parameter ("mix.ch1.out")->setValueNotifyingHost (q.parameter ("mix.ch1.out")->convertTo0to1 (4.0f));
    CHECK (renderNote (q, 20, 0) == 0.0f);
}

TEST_CASE ("processor: routing to a disabled aux bus falls back to main")
{
    OhpeProcessor p; // aux buses disabled
    p.prepareToPlay (48000.0, 512);
    auto* out = p.parameter ("mix.ch1.out");
    out->setValueNotifyingHost (out->convertTo0to1 (6.0f));
    CHECK (renderNote (p, 20, 0) > 0.01f);
}

TEST_CASE ("processor: state round-trip through get/setStateInformation")
{
    OhpeProcessor a;
    a.parameter ("p1.analog.cutoff")->setValueNotifyingHost (0.123f);
    a.parameter ("mix.ch3.pan")->setValueNotifyingHost (0.9f);
    MidiMapping m;
    m.paramId = "tempo";
    m.number = 20;
    a.core().midiMap().add (m);
    a.core().midiMapChanged();
    a.uiState.getDynamicObject()->setProperty ("lang", "de");
    juce::MemoryBlock block;
    a.getStateInformation (block);

    OhpeProcessor b;
    b.setStateInformation (block.getData(), (int) block.getSize());
    CHECK (b.parameter ("p1.analog.cutoff")->getValue() == doctest::Approx (0.123f).epsilon (1e-4));
    CHECK (b.parameter ("mix.ch3.pan")->getValue() == doctest::Approx (0.9f).epsilon (1e-4));
    CHECK (b.core().midiMap().isMapped ("tempo"));
    CHECK (b.uiState["lang"].toString() == "de");
    b.setStateInformation ("junk", 4); // ignored, no crash
    CHECK (b.core().midiMap().isMapped ("tempo"));
}

TEST_CASE ("processor: MIDI learn bridge and host notification")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 512);
    p.armMidiLearn ("p1.analog.reso");
    CHECK (p.learningParamId() == "p1.analog.reso");
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 21, 10), 0);
    p.processBlock (buffer, midi);
    pump(); // timer stores the learned control
    CHECK (p.core().midiMap().isMapped ("p1.analog.reso"));
    CHECK (p.describeMidiMapping ("p1.analog.reso") == "CC 21 / Ch 2");
    CHECK (p.learningParamId().isEmpty());

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 21, 127), 0);
    p.processBlock (buffer, midi);
    CHECK (p.parameter ("p1.analog.reso")->getValue() == doctest::Approx (1.0f));
    // the host value now wins again only when the host changes it
    p.processBlock (buffer, midi = {});
    CHECK (p.core().params().get ("p1.analog.reso") == doctest::Approx (1.0f));

    const auto file = juce::File::createTempFile ("ohpemidi");
    CHECK (p.exportMidiMap (file));
    p.clearAllMidiMappings();
    CHECK_FALSE (p.core().midiMap().isMapped ("p1.analog.reso"));
    CHECK (p.importMidiMap (file));
    CHECK (p.core().midiMap().isMapped ("p1.analog.reso"));
    file.deleteFile();
}

TEST_CASE ("editor: builds, resizes with fixed aspect, every mode binds existing parameters")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<OhpeEditor*> (ed.get());
    REQUIRE (e != nullptr);
    CHECK (e->getWidth() == OhpeEditor::designWidth);
    e->setSize (1800, 1200);
    for (int m = 0; m < ui::numModes; ++m)
    {
        e->selectMode ((ui::Mode) m);
        for (int i = 0; i < 4; ++i)
        {
            const auto id = e->encoder (i).boundId();
            if (id.isNotEmpty()) CHECK (p.parameter (id) != nullptr);
        }
    }
    e->selectEngine (EngineType::FM);
    CHECK (e->currentMode() == ui::Mode::Synth);
    CHECK (e->encoder (0).boundId() == "p1.fm.algo");
    e->selectEngine (EngineType::Analog);
    CHECK (e->encoder (2).boundId() == "p1.analog.cutoff");
    e->advanceAnimation (0.5f);
    auto img = e->createComponentSnapshot (e->getLocalBounds());
    CHECK (img.getWidth() == 1800);
}

TEST_CASE ("localisation: every key has a German text, language switch")
{
    i18n::setCurrent (i18n::Language::German);
    for (const auto& k : i18n::allKeys())
        CHECK (i18n::tr (k).isNotEmpty());
    CHECK (i18n::tr ("Audio Settings") == "Audio-Einstellungen");
    // umlauts survive (UTF-8 literals), also in upper case as drawn on the keys
    CHECK (i18n::tr ("Envelope") == juce::String::fromUTF8 ("H\xc3\xbcllkurve"));
    CHECK (i18n::upper (i18n::tr ("Envelope")) == juce::String::fromUTF8 ("H\xc3\x9cLLKURVE"));
    CHECK (i18n::tr ("Buffer size") == juce::String::fromUTF8 ("Puffergr\xc3\xb6\xc3\x9f" "e"));
    for (const auto& k : i18n::allKeys())
        CHECK_FALSE (i18n::tr (k).containsChar ((juce::juce_wchar) 0xc3)); // no UTF-8 read as Latin-1
    i18n::setCurrent (i18n::Language::English);
    CHECK (i18n::tr ("Audio Settings") == "Audio Settings");
}

TEST_CASE ("standalone audio settings: latency text")
{
    CHECK (AudioSettingsPage::latencyText (256, 0, 48000.0).startsWith ("5.3 ms"));
    CHECK (AudioSettingsPage::latencyText (64, 32, 96000.0).startsWith ("1.0 ms"));
}

TEST_CASE ("parameter value texts carry units")
{
    OhpeProcessor p;
    CHECK (p.parameter ("p1.analog.cutoff")->getText (p.parameter ("p1.analog.cutoff")->convertTo0to1 (2000.0f), 32) == "2.00 kHz");
    CHECK (p.parameter ("mix.ch1.vol")->getText (p.parameter ("mix.ch1.vol")->convertTo0to1 (-6.0f), 32) == "-6.0 dB");
    CHECK (p.parameter ("p1.amp.a")->getText (p.parameter ("p1.amp.a")->convertTo0to1 (0.25f), 32) == "250 ms");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    doctest::Context ctx (argc, argv);
    return ctx.run();
}
