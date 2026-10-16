// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// 0.4 (package F): factory sample bank (instruments, drum kits), sequencers playing the drum kit,
// part gain, and every factory preset rendering cleanly (core only: built-in fallbacks for the
// sample bank; the plug-in tests render them again with the embedded recordings).
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "../TestSlice.h"
#include "preset/Preset.h"
#include "render/PresetRender.h"
#include "sample/FactoryBank.h"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ohpe;
namespace fs = std::filesystem;

namespace
{
SamplePtr sine (double freq, double seconds, double rate = 48000.0)
{
    std::vector<float> v ((size_t) (seconds * rate));
    for (size_t i = 0; i < v.size(); ++i) v[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * freq * (double) i / rate);
    return SampleData::fromMono (std::move (v), rate);
}

double frequencyOf (const render::Audio& a, double from, double to)
{
    return test::measureFrequency (a.l, a.sampleRate, (size_t) (from * a.sampleRate), (size_t) (to * a.sampleRate));
}

std::vector<fs::path> factoryFiles()
{
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator (fs::path (OHPE_SOURCE_DIR) / "resources" / "presets" / "factory"))
        if (e.path().extension() == ".ohpepreset") files.push_back (e.path());
    std::sort (files.begin(), files.end());
    return files;
}
} // namespace

TEST_CASE ("factory bank: the Sampler and Grain engines play the chosen factory instrument")
{
    // instrument 10 gets a 1 kHz sine with root C4 (the bank is process-wide; first call wins)
    sampler::ZoneSpec z;
    z.sample = sine (1000.0, 1.0);
    z.rootKey = 60;
    factory::setInstrument (10, { z });
    REQUIRE (factory::instrumentMap (10) != nullptr);
    CHECK (factory::instrumentMap (3) == nullptr); // nothing else installed in the core tests

    auto play = [] (const char* engineParam, int engine, int inst) {
        Core core;
        core.prepare (48000.0, 256);
        core.params().set ("p1.engine", (float) engine);
        core.params().set (engineParam, (float) inst);
        core.params().set ("p1.amp.a", 0.001f);
        core.params().set ("p1.amp.s", 1.0f);
        core.params().set ("p1.mod.amt", 0.0f);
        return render::renderCore (core, { { 0, MidiEvent::noteOn (1, 60, 100) } }, 24000);
    };
    // Sampler: built-in tone (C4 = 261.6 Hz) versus the factory instrument (1 kHz)
    CHECK (frequencyOf (play ("p1.sampler.inst", (int) EngineType::Sampler, 0), 0.1, 0.45) == doctest::Approx (261.6).epsilon (0.03));
    CHECK (frequencyOf (play ("p1.sampler.inst", (int) EngineType::Sampler, 10), 0.1, 0.45) == doctest::Approx (1000.0).epsilon (0.02));
    // Grain: the grains read the factory instrument at its original pitch for its root key
    const auto g = play ("p1.grain.inst", (int) EngineType::Grain, 10);
    CHECK (test::allFinite (g.l));
    CHECK (test::rms (g.l, 4800, 24000) > 0.001f);

    // a user sample wins over the factory instrument
    Core core;
    core.prepare (48000.0, 256);
    core.params().set ("p1.engine", (float) EngineType::Sampler);
    core.params().set ("p1.sampler.inst", 10.0f);
    core.params().set ("p1.mod.amt", 0.0f);
    dynamic_cast<SamplerEngine*> (core.part (0).engine (EngineType::Sampler))->setSample (sine (500.0, 1.0), 60);
    const auto u = render::renderCore (core, { { 0, MidiEvent::noteOn (1, 60, 100) } }, 24000);
    CHECK (frequencyOf (u, 0.1, 0.45) == doctest::Approx (500.0).epsilon (0.02));
}

TEST_CASE ("factory bank: drum kits replace the pad recordings, user samples still win")
{
    factory::setKitSample (4, 0, sine (1000.0, 0.4));
    REQUIRE (factory::kitSample (4, 0) != nullptr);
    CHECK (factory::kitSample (0, 0) == nullptr); // kit 0 lives in each DrumKit (setFactorySample)

    Core core;
    core.prepare (48000.0, 256);
    core.params().set ("drum.kit", 4.0f);
    auto hit = [&core] { return render::renderCore (core, { { 0, MidiEvent::noteOn (10, 36, 120) } }, 9600); };
    CHECK (frequencyOf (hit(), 0.02, 0.15) == doctest::Approx (1000.0).epsilon (0.02));
    CHECK (core.drumKit().kitIndex() == 4);
    CHECK (core.drumKit().effectiveSample (0) != nullptr);

    core.params().set ("drum.kit", 0.0f); // the synth kick model (no recordings in the core tests)
    CHECK (frequencyOf (hit(), 0.02, 0.15) < 300.0);

    core.params().set ("drum.kit", 4.0f);
    core.drumKit().setPadSample (0, sine (400.0, 0.4));
    CHECK (frequencyOf (hit(), 0.02, 0.15) == doctest::Approx (400.0).epsilon (0.02));
}

TEST_CASE ("sequencers: Step, Euclid and Orbit can play the drum kit instead of a part")
{
    for (const char* which : { "euclid", "orbit", "step" })
    {
        CAPTURE (which);
        Core core;
        core.prepare (48000.0, 256);
        auto& reg = core.params();
        reg.set ("tempo", 120.0f);
        if (std::string (which) == "euclid")
        {
            reg.set ("seq.euclid.on", 1.0f);
            reg.set ("seq.euclid.part", 2.0f);
            reg.set ("seq.euclid.r1.steps", 4.0f);
            reg.set ("seq.euclid.r1.pulses", 4.0f);
            reg.set ("seq.euclid.r1.note", 36.0f);
        }
        else if (std::string (which) == "orbit")
        {
            reg.set ("seq.orbit.on", 1.0f);
            reg.set ("seq.orbit.part", 2.0f);
            for (int i = 0; i < 16; i += 4) core.orbitSequencer().setStep (0, i, { true, 36, 110 });
        }
        else
        {
            reg.set ("seq.step.channel", 2.0f);
            for (int i = 0; i < 16; i += 4)
            {
                StepSequencer::Step s;
                s.on = true;
                s.note = 36;
                core.stepSequencer().setStep (0, i, s);
            }
        }
        reg.set ("seq.run", 1.0f);
        const std::string drumsId = std::string ("seq.") + which + ".drums";
        reg.set (drumsId, 1.0f);
        render::renderCore (core, {}, 12000);
        CHECK (core.drumKit().activeVoiceCount() > 0);
        CHECK (core.part (1).activeVoiceCount() == 0);

        reg.set (drumsId, 0.0f);
        core.drumKit().allNotesOff (true);
        render::renderCore (core, {}, 24000);
        CHECK (core.part (1).activeVoiceCount() > 0);
    }
}

TEST_CASE ("part gain: dB on top of the part volume")
{
    auto level = [] (float gainDb) {
        Core core;
        core.prepare (48000.0, 256);
        core.params().set ("p1.gain", gainDb);
        core.params().set ("mix.master.limit", 0.0f);
        core.params().set ("p1.vol", 0.2f);
        const auto a = render::renderCore (core, { { 0, MidiEvent::noteOn (1, 60, 100) } }, 19200);
        return test::rms (a.l, 4800, 19200);
    };
    const float ref = level (0.0f);
    REQUIRE (ref > 0.0f);
    CHECK (level (6.0f) / ref == doctest::Approx (1.995).epsilon (0.02));
    CHECK (level (-12.0f) / ref == doctest::Approx (0.251).epsilon (0.02));
}

TEST_CASE ("factory presets: every preset loads and renders finite, unclipped audio at a sane level" * doctest::test_suite ("sliced"))
{
    const auto files = factoryFiles();
    REQUIRE (files.size() >= 100);
    const auto slice = test::testSlice(); // CI renders the presets in several processes
    int parts = 0, projects = 0;
    long long item = 0;
    for (const auto& file : files)
    {
        CAPTURE (file.filename().string());
        std::ifstream f (file);
        std::stringstream ss;
        ss << f.rdbuf();
        preset::Preset p;
        REQUIRE (preset::fromJson (ss.str(), p));
        CHECK (p.meta.author == "groovex");
        CHECK (! p.meta.comment.empty());
        REQUIRE (! p.meta.tags.empty());
        for (const auto& t : p.meta.tags) CHECK ((preset::isCategory (t) || t == "MPE" || t == "Morph")); // 0.8: the free tag MPE marks MPE-ready presets (p16: Morph = morph presets)

        if (! slice.has (item++))
        {
            ++(p.meta.kind == preset::Kind::Part ? parts : projects); // counted, rendered by another slice
            continue;
        }
        Core core;
        core.prepare (48000.0, 256);
        const int before = core.params().size();
        render::Audio a;
        if (p.meta.kind == preset::Kind::Part)
        {
            ++parts;
            const auto it = p.params.find ("engine");
            REQUIRE (it != p.params.end());
            CHECK (engineTypeInfo ((EngineType) (int) it->second).id == p.meta.engine);
            a = render::renderPart (core, p, 0, 100.0, 2.0, 0.4); // two beats and a short tail
        }
        else
        {
            ++projects;
            a = render::renderProject (core, p, 2.5);
        }
        CHECK (core.params().size() == before);
        const auto m = render::measure (a);
        CHECK (m.nonFinite == 0);
        CHECK (m.denormals == 0);
        CHECK (std::abs (m.dc) < 0.01);
        // without the sample bank (core tests) presets that play a factory recording fall back to the
        // built-in sound, which is far louder than the recording their gain is calibrated for: only
        // the numbers are checked for them here; the plug-in tests check level and peak with the bank
        const auto inst = [&] (const char* id) { const auto i = p.params.find (id); return i != p.params.end() && i->second > 0.5f; };
        const bool recording = inst ("sampler.inst") || inst ("grain.inst") || inst ("sampler.inst2") || inst ("grain.inst2"); // 0.8 keyboards
        CAPTURE (recording);
        if (! recording)
        {
            CHECK (m.peakDb < 0.0);
            // the level window here is wide; the plug-in tests check the real window
            CHECK (m.activeRmsDb > -34.0);
            CHECK (m.activeRmsDb < -8.0);
        }
    }
    CHECK (parts >= 100);
    CHECK (projects >= 6);
}
