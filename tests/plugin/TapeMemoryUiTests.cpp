// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p67: tape memory in the plug-in - the output fills the memory, CAPTURE (soft key, tapemem.go) builds a Sampler
// instrument off the message thread, the session keeps it, WAV export, the soft keys in both languages.

#include <doctest/doctest.h>

#include "engines/SamplerEngine.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
struct TempFolder
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("ohpe-test-tapemem-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    TempFolder() { dir.createDirectory(); }
    ~TempFolder() { dir.deleteRecursively(); }
};

void play (OhpeProcessor& p, int blocks, int note)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0 && note >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
        if (b == blocks / 2 && note >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

SamplerEngine* samplerOf (OhpeProcessor& p, int part)
{
    return dynamic_cast<SamplerEngine*> (p.core().part (part).engine (EngineType::Sampler));
}
} // namespace

TEST_CASE ("plug-in p67: the output fills the tape memory; CAPTURE makes a Sampler instrument the session keeps")
{
    TempFolder tmp;
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.tapeMemory().setLibraryFolder (tmp.dir.getChildFile ("Captures"));
    CHECK (p.tapeMemory().heldSeconds() == 0.0);
    CHECK_FALSE (p.tapeMemory().capture (1)); // empty
    CHECK (p.tapeMemory().lastMessage() == "Tape memory is empty - play something first");

    play (p, 200, 57); // about a second of part 1
    CHECK (p.tapeMemory().heldSeconds() > 0.9);
    REQUIRE (p.tapeMemory().capture (1, tapemem::ModeChromatic));
    CHECK (p.tapeMemory().busy());
    CHECK_FALSE (p.tapeMemory().capture (1)); // one at a time
    p.tapeMemory().finishNow();
    CHECK_FALSE (p.tapeMemory().busy());
    CHECK (p.core().params().get ("p2.engine") == (float) EngineType::Sampler);
    auto* s = samplerOf (p, 1);
    REQUIRE ((s != nullptr));
    REQUIRE (s->zones().size() == 1);
    const auto* c = p.tapeMemory().captureOf (1);
    REQUIRE ((c != nullptr));
    CHECK ((s->zones()[0].sample == c->sample));
    CHECK (s->zones()[0].rootKey == c->root);
    CHECK (p.tapeMemory().lastMessage().startsWith ("Captured into Part 2"));

    // WAV export into the capture library
    const auto file = p.tapeMemory().exportWav();
    CHECK (file.existsAsFile());
    CHECK (file.getParentDirectory() == tmp.dir.getChildFile ("Captures"));

    // the session keeps the capture (audio inside the state, no file needed)
    file.deleteFile();
    juce::MemoryBlock state;
    p.getStateInformation (state);
    {
        OhpeProcessor q;
        q.prepareToPlay (48000.0, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        auto* back = samplerOf (q, 1);
        REQUIRE ((back != nullptr));
        REQUIRE (back->zones().size() == 1);
        CHECK (back->zones()[0].rootKey == c->root);
        CHECK (back->zones()[0].sample->numFrames() == c->sample->numFrames());
        CHECK (q.core().params().get ("p2.engine") == (float) EngineType::Sampler);
        REQUIRE ((q.tapeMemory().captureOf (1) != nullptr));
    }

    // Slices through the trigger parameter (the MIDI-mappable CAPTURE) into the target part
    p.core().params().set (Core::Ids::tapememMode, (float) tapemem::ModeSlices);
    p.core().params().set (Core::Ids::tapememTo, (float) (tapemem::TargetPart1 + 3));
    REQUIRE (p.tapeMemory().capture());
    p.tapeMemory().finishNow();
    CHECK (p.core().params().get ("p4.sampler.mode") == (float) sampler::ModeSlices);
    REQUIRE ((p.tapeMemory().captureOf (3) != nullptr));
    CHECK (p.tapeMemory().captureOf (3)->slices >= 1);

    // a different sample in the part: the capture is no longer stored with the session
    samplerOf (p, 3)->setSample (nullptr);
    CHECK ((p.tapeMemory().captureOf (3) == nullptr));
    p.core().tapeMemory().clear();
    CHECK (p.tapeMemory().heldSeconds() == 0.0);
}

TEST_CASE ("plug-in p67: CAPTURE and MEMORY soft keys on the tape page, CAPTURE on the Sampler page, EN / DE")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();
    e.setLanguage (i18n::Language::English);
    e.selectMode (ui::Mode::Tape);
    const auto& keys = d.softKeyList();
    REQUIRE (keys.size() == 14);
    CHECK (keys[6].label == "Capture");
    CHECK (keys[13].label == "Memory");
    CHECK_FALSE (keys[6].enabled); // nothing held yet
    play (p, 100, 60);
    e.selectMode (ui::Mode::Mixer);
    e.selectMode (ui::Mode::Tape); // the keys are rebuilt (the editor's timer does it while open)
    CHECK (d.softKeyList()[6].enabled);
    e.selectPart (2);
    p.core().params().set (Core::Ids::tapememTo, (float) tapemem::TargetEditPart);
    e.softKey (170); // CAPTURE: into the edit part
    p.tapeMemory().finishNow();
    CHECK (p.core().params().get ("p3.engine") == (float) EngineType::Sampler);
    e.selectMode (ui::Mode::Synth);
    e.selectEngine (EngineType::Sampler);
    CHECK (d.softKeyList().size() == 4);
    CHECK (d.softKeyList()[3].label == "Capture");
    e.setLanguage (i18n::Language::German);
    CHECK (d.softKeyList()[3].label == juce::String::fromUTF8 ("Merken"));
    e.selectMode (ui::Mode::Tape);
    CHECK (d.softKeyList()[13].label == juce::String::fromUTF8 ("Gedächtnis"));
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.setLanguage (i18n::Language::English);
    CHECK (i18n::allKeys().contains ("Captured into Part"));
    ed.reset();
}
