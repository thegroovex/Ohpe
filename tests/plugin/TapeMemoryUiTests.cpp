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
    CHECK (p.tapeMemory().heldSeconds() > 0.5); // 1.07 s pushed, the 0.5 s guard is not capturable
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
    play (p, 300, 60);
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
    CHECK (d.softKeyList()[13].label == juce::String::fromUTF8 ("Memory"));
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.setLanguage (i18n::Language::English);
    CHECK (i18n::allKeys().contains ("Captured into Part"));
    ed.reset();
}

namespace
{
/** p72: a factory part preset: a Sampler one that picks a factory instrument, or one of another engine. */
int factoryPartPreset (OhpeProcessor& p, bool sampler, preset::Preset* out = nullptr)
{
    const auto& list = p.presets().entries();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& e = list[(size_t) i];
        if (! e.factory || e.meta.kind != preset::Kind::Part || e.meta.engine.empty() || (e.meta.engine == "sampler") != sampler) continue;
        preset::Preset pr;
        if (! preset::fromJson (p.presets().read (e), pr)) continue;
        const auto inst = [&pr] (const char* id) { auto it = pr.params.find (id); return it != pr.params.end() ? it->second : 0.0f; };
        if (sampler && inst ("sampler.inst") <= 0.0f && inst ("sampler.inst2") <= 0.0f) continue;
        if (out != nullptr) *out = pr;
        return i;
    }
    return -1;
}

void captureInto (OhpeProcessor& p, int part)
{
    REQUIRE (p.tapeMemory().capture (part, tapemem::ModeChromatic));
    p.tapeMemory().finishNow();
    REQUIRE ((p.tapeMemory().captureOf (part) != nullptr));
}
} // namespace

TEST_CASE ("plug-in p72: a preset loaded after CAPTURE plays completely; the capture is set aside, not lost")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    play (p, 200, 57);
    captureInto (p, 0);
    captureInto (p, 1);
    const auto* cap0 = p.tapeMemory().captureOf (0);
    const auto sample0 = cap0->sample;

    preset::Preset sp;
    const int samplerPreset = factoryPartPreset (p, true, &sp);
    const int otherPreset = factoryPartPreset (p, false);
    REQUIRE (samplerPreset >= 0);
    REQUIRE (otherPreset >= 0);

    // library path: a Sampler preset with a factory instrument - its instrument plays, not the capture
    REQUIRE (p.presets().load (samplerPreset, 0));
    CHECK (p.core().params().get ("p1.engine") == (float) EngineType::Sampler);
    CHECK (samplerOf (p, 0)->zones().empty()); // no zone overrides the factory instrument
    for (const char* id : { "sampler.inst", "sampler.inst2" })
        if (auto it = sp.params.find (id); it != sp.params.end())
            CHECK (p.core().params().get (std::string ("p1.") + id) == doctest::Approx (it->second));
    CHECK ((p.tapeMemory().captureOf (0) == nullptr));
    REQUIRE ((p.tapeMemory().storedCapture (0) != nullptr)); // set aside, not lost
    CHECK ((p.tapeMemory().storedCapture (0)->sample == sample0));
    CHECK (p.tapeMemory().lastMessage() == "Capture set aside - Memory menu restores it");
    // no leak: part 2 keeps its own capture
    REQUIRE ((p.tapeMemory().captureOf (1) != nullptr));
    CHECK (samplerOf (p, 1)->zones().size() == 1);

    // the Memory menu brings it back
    REQUIRE (p.tapeMemory().restore (0));
    CHECK (p.core().params().get ("p1.engine") == (float) EngineType::Sampler);
    REQUIRE ((p.tapeMemory().captureOf (0) != nullptr));
    CHECK ((samplerOf (p, 0)->zones()[0].sample == sample0));

    // preset strip arrows
    REQUIRE (p.presets().step (1, 0));
    CHECK ((p.tapeMemory().captureOf (0) == nullptr));
    CHECK (samplerOf (p, 0)->zones().empty());

    // another engine: it plays; a later Sampler preset does not bring the capture back either
    REQUIRE (p.tapeMemory().restore (0));
    REQUIRE (p.presets().load (otherPreset, 0));
    CHECK (p.core().params().get ("p1.engine") != (float) EngineType::Sampler);
    CHECK (samplerOf (p, 0)->zones().empty());
    REQUIRE (p.presets().load (samplerPreset, 0));
    CHECK (samplerOf (p, 0)->zones().empty());

    // library preview: browsing sets the capture aside, closing without loading brings it back
    REQUIRE (p.tapeMemory().restore (0));
    const int before = p.tapeMemory().messageCount();
    p.presets().beginPreview (0);
    REQUIRE (p.presets().load (samplerPreset, 0));
    CHECK (samplerOf (p, 0)->zones().empty());
    p.presets().revertPreview();
    REQUIRE ((p.tapeMemory().captureOf (0) != nullptr));
    CHECK (p.core().params().get ("p1.engine") == (float) EngineType::Sampler);
    CHECK (p.tapeMemory().messageCount() == before); // quiet while browsing

    // a breed child keeps the part's sample choice, so the capture stays
    {
        preset::Preset child;
        child.meta.kind = preset::Kind::Part;
        child.params["sampler.tone"] = 0.3f;
        REQUIRE (p.presets().apply (child, 0, true));
        CHECK ((p.tapeMemory().captureOf (0) != nullptr));
    }

    // session: only captures still playing are stored; a set-aside one is not restored into the part
    REQUIRE (p.presets().load (samplerPreset, 0));
    juce::MemoryBlock state;
    p.getStateInformation (state);
    {
        OhpeProcessor q;
        q.prepareToPlay (48000.0, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (samplerOf (q, 0)->zones().empty());
        CHECK ((q.tapeMemory().captureOf (0) == nullptr));
        REQUIRE ((q.tapeMemory().captureOf (1) != nullptr));
    }

    // a project brings its own sound into every part: captures are set aside; a project saved with a capture
    // brings it back
    REQUIRE (p.tapeMemory().restore (0));
    TempFolder tmp;
    p.presets().setUserFolder (tmp.dir);
    preset::Meta meta;
    meta.name = "Capture Project";
    const auto file = p.presets().save (meta, true, 0, true);
    REQUIRE (file.existsAsFile());
    p.presets().rescan();
    const int projIdx = p.presets().indexOfFile (file);
    REQUIRE (projIdx >= 0);
    REQUIRE (p.presets().load (samplerPreset, 0));
    REQUIRE (p.presets().load (otherPreset, 1));
    CHECK ((p.tapeMemory().captureOf (1) == nullptr));
    REQUIRE (p.presets().load (projIdx, 0));
    REQUIRE ((p.tapeMemory().captureOf (0) != nullptr));
    REQUIRE ((p.tapeMemory().captureOf (1) != nullptr));
    CHECK ((p.tapeMemory().captureOf (0)->sample->numFrames() == sample0->numFrames()));
    preset::Preset plain = preset::captureProject (p.core(), {}, {});
    REQUIRE (p.presets().apply (plain, 0));
    CHECK ((p.tapeMemory().captureOf (0) == nullptr));
    CHECK ((p.tapeMemory().captureOf (1) == nullptr));
    CHECK ((p.tapeMemory().storedCapture (1) != nullptr));
}

TEST_CASE ("plug-in p72: RANDOM shapes a captured sound (the sample stays); DE menu text")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    play (p, 200, 57);
    captureInto (p, 0);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (0);
    e.selectMode (ui::Mode::Synth);
    e.selectEngine (EngineType::Sampler);
    e.randomiseSound (42);
    CHECK ((p.tapeMemory().captureOf (0) != nullptr));
    i18n::setCurrent (i18n::Language::German);
    CHECK (i18n::tr ("Tape memory") == "Tape-Memory");
    CHECK (i18n::tr ("Restore last capture") == juce::String::fromUTF8 ("Zuletzt Gemerktes zurückholen"));
    i18n::setCurrent (i18n::Language::English);
    ed.reset();
}
