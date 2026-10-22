// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p23: bounce to engine in the plug-in - a recorded tape track into Grain (endless loop, the bounce library's WAV
// with its loop chunk, restored with the session) and the Sampler (one-shot), the bounce.go trigger, the input
// recording, empty sources, the BOUNCE soft keys in both languages.

#include <doctest/doctest.h>

#include "engines/GrainEngine.h"
#include "engines/SamplerEngine.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
using T = OhpeProcessor::TransportKey;

struct TempFolder
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("ohpe-test-bounce-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    TempFolder() { dir.createDirectory(); }
    ~TempFolder() { dir.deleteRecursively(); }
};

void process (OhpeProcessor& p, int blocks, const juce::MidiBuffer& first = {})
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi = first;
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

/** Records about a second of part 1 (a held note) on tape track 1 with the one-touch record. */
void recordTrack1 (OhpeProcessor& p)
{
    p.runTransport (T::Record);
    REQUIRE (p.tapeRecording());
    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 110), 0);
    p.core().tape().housekeeping();
    for (int k = 0; k < 20; ++k)
    {
        process (p, 10, k == 0 ? note : juce::MidiBuffer());
        p.core().tape().housekeeping();
    }
    p.runTransport (T::Stop);
    process (p, 4);
    p.core().tape().housekeeping();
    REQUIRE (p.core().tape().contentEndFrames (0) > 24000);
}

GrainEngine* grainOf (OhpeProcessor& p, int part) { return dynamic_cast<GrainEngine*> (p.core().part (part).engine (EngineType::Grain)); }
SamplerEngine* samplerOf (OhpeProcessor& p, int part)
{
    return dynamic_cast<SamplerEngine*> (p.core().part (part).engine (EngineType::Sampler));
}
} // namespace

TEST_CASE ("plug-in p23: bounce a tape track into Grain (endless loop), the library file, the session restores it")
{
    TempFolder tmp;
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.bounces().setLibraryFolder (tmp.dir.getChildFile ("Bounces"));
    recordTrack1 (p);

    // the trigger (host / MIDI): edit track 1 -> Grain of the edit part, endless loop (the defaults)
    p.core().params().set (Core::Ids::editPart, 2.0f);
    p.parameter (Core::Ids::editPart)->setValueNotifyingHost (p.parameter (Core::Ids::editPart)->convertTo0to1 (2.0f));
    const int before = p.bounces().messageCount();
    p.parameter (Core::Ids::bounceGo)->setValueNotifyingHost (1.0f);
    p.pollRemoteControls();
    CHECK (p.bounces().messageCount() == before + 1);
    CHECK (p.bounces().lastSucceeded());
    CHECK (p.bounces().lastPart() == 1);
    CHECK (p.core().params().get ("p2.engine") == (float) EngineType::Grain);
    CHECK (p.parameter ("p2.engine")->convertFrom0to1 (p.parameter ("p2.engine")->getValue()) == (float) EngineType::Grain);
    auto* g = grainOf (p, 1);
    REQUIRE ((g != nullptr));
    REQUIRE ((g->sample() != nullptr));
    CHECK (g->sample()->endless);
    int64_t ls = 0, le = 0;
    CHECK (g->sample()->seamlessLoop (ls, le));
    const auto file = p.bounces().lastFile();
    REQUIRE (file.existsAsFile());
    CHECK (file.getParentDirectory() == tmp.dir.getChildFile ("Bounces"));
    CHECK (file.getFileName().startsWith ("Tape 1"));
    CHECK (juce::String (g->sample()->path()) == file.getFullPathName());
    CHECK (p.bounces().lastMessage().contains ("Grain"));
    p.parameter (Core::Ids::bounceGo)->setValueNotifyingHost (0.0f);
    p.pollRemoteControls();

    // it plays: part 2 on Grain, a held note sounds and stays finite
    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 100), 0);
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    float peak = 0.0f;
    bool finite = true;
    for (int b = 0; b < 120; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi = note;
        buffer.clear();
        p.processBlock (buffer, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
            {
                finite &= std::isfinite (buffer.getSample (c, i));
                peak = juce::jmax (peak, std::abs (buffer.getSample (c, i)));
            }
    }
    CHECK (finite);
    CHECK (peak < 4.0f);

    // the session keeps the path; the WAV's loop chunk brings the endless loop back
    juce::MemoryBlock state;
    p.getStateInformation (state);
    {
        OhpeProcessor q;
        q.prepareToPlay (48000.0, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        auto* back = grainOf (q, 1);
        REQUIRE ((back != nullptr));
        REQUIRE ((back->sample() != nullptr));
        CHECK (juce::String (back->sample()->path()) == file.getFullPathName());
        CHECK (back->sample()->endless);
        CHECK (back->sample()->numFrames() == g->sample()->numFrames());
        CHECK (back->sample()->fileLoopStart == g->sample()->fileLoopStart);
        CHECK (back->sample()->fileLoopEnd == g->sample()->fileLoopEnd);
    }
}

TEST_CASE ("plug-in p23: Sampler one-shot, empty tracks, the input recording, the soft keys")
{
    TempFolder tmp;
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.bounces().setLibraryFolder (tmp.dir.getChildFile ("Bounces"));
    recordTrack1 (p);

    // one-shot into the Sampler of part 3
    p.core().params().set (Core::Ids::bounceLoop, 0.0f);
    REQUIRE (p.bounces().bounce (2, bounce::TargetSampler, bounce::SourceTrack1));
    CHECK (p.core().params().get ("p3.engine") == (float) EngineType::Sampler);
    CHECK (p.core().params().get ("p3.sampler.loop") == (float) sampler::LoopOff);
    auto* s = samplerOf (p, 2);
    REQUIRE ((s != nullptr));
    REQUIRE (s->zones().size() == 1);
    CHECK_FALSE (s->zones()[0].sample->endless);
    CHECK (s->zones()[0].rootKey == 60);
    CHECK (p.bounces().lastMessage().contains ("Sampler"));

    // the tape mix bounces too; an empty track does not
    p.core().params().set (Core::Ids::bounceLoop, 1.0f);
    CHECK (p.bounces().bounce (3, bounce::TargetSampler, bounce::SourceMix));
    CHECK (p.core().params().get ("p4.sampler.loop") == (float) sampler::LoopForward);
    CHECK_FALSE (p.bounces().bounce (0, bounce::TargetGrain, bounce::SourceTrack1 + 5));
    CHECK_FALSE (p.bounces().lastSucceeded());
    CHECK (p.bounces().lastMessage() == "Nothing to bounce - the track is empty");
    CHECK (p.core().params().get ("p1.engine") != (float) EngineType::Grain);

    // the live input: records (the input bus is off here = silence), nothing is bounced from silence
    REQUIRE (p.bounces().bounce (0, bounce::TargetGrain, bounce::SourceInput));
    CHECK (p.bounces().recording());
    CHECK_FALSE (p.bounces().bounce (0, bounce::TargetGrain, bounce::SourceTrack1)); // one at a time
    process (p, 20);
    CHECK (p.bounces().progress() > 0.0f);
    CHECK (p.bounces().finishRecording());
    CHECK_FALSE (p.bounces().recording());
    CHECK (p.bounces().lastMessage() == "Nothing to bounce - the material is silent");

    // the soft keys: BOUNCE on the tape page (p38; p67: fourteen keys) and on the Grain / Sampler page (third key)
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& d = e.displayComponent();
    e.selectMode (ui::Mode::Tape);
    CHECK_FALSE (d.softKeyArea (13).isEmpty()); // p67: + CAPTURE, MEMORY
    CHECK (d.softKeyArea (14).isEmpty());
    e.selectPart (0);
    e.softKey (90); // the tape page bounces at once: edit track 1 -> Grain of part 1
    CHECK (p.bounces().lastSucceeded());
    CHECK (p.core().params().get ("p1.engine") == (float) EngineType::Grain);
    e.selectMode (ui::Mode::Synth);
    e.selectEngine (EngineType::Grain);
    for (int k = 0; k < 3; ++k) CHECK_FALSE (d.softKeyArea (k).isEmpty());
    CHECK (d.softKeyArea (3).isEmpty());
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        for (auto m : { ui::Mode::Synth, ui::Mode::Tape })
        {
            e.selectMode (m);
            e.advanceAnimation (1.0f / 30.0f);
            CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        }
    }
    e.setLanguage (i18n::Language::English);
    CHECK (i18n::allKeys().contains ("Bounced to Grain"));
    ed.reset();
}
