// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p15: life per part ("pN.life") - held notes keep evolving on every engine. 0 is neutral (bit-identical), the output
// stays finite, bounded and level-matched, turning it never clicks, the motion is deterministic, and it is a
// parameter like any other: a modulation matrix target, MIDI-mappable, part of presets.

#include "EngineTestKit.h"
#include "SampleTestHelpers.h"
#include "engines/GrainEngine.h"
#include "midi/MidiMap.h"
#include "mod/ModMatrix.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <fstream>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr EngineType allEngines[] = { EngineType::FM,      EngineType::Analog, EngineType::Wavetable, EngineType::Pluck,
                                      EngineType::Cluster, EngineType::Grain,  EngineType::Vocal,     EngineType::Sampler };

VoiceCommon lifeCommon (float life, uint32_t seed)
{
    auto c = plainCommon();
    c.life = life;
    c.lifeSeed = seed;
    return c;
}

std::unique_ptr<ISynthEngine> makeEngine (EngineType t, const VoiceCommon& c)
{
    auto e = createEngine (t);
    REQUIRE (e != nullptr);
    kit::prepare (*e, c);
    kit::setDefaults (*e);
    if (t == EngineType::Grain) static_cast<GrainEngine&> (*e).setSample (makeSine (220.0, 48000.0, 2.0));
    return e;
}

/** A held chord (C3 G3 E4); `perBlock` may change the common settings before each 256-sample block. */
Stereo playChord (EngineType t, const VoiceCommon& c, size_t n, const std::function<void (ISynthEngine&, size_t)>& perBlock = {})
{
    auto e = makeEngine (t, c);
    for (int note : { 48, 55, 64 }) e->noteOn (note, 0.8f);
    Stereo s (n);
    for (size_t pos = 0; pos < n; pos += 256)
    {
        if (perBlock) perBlock (*e, pos);
        const int len = (int) std::min ((size_t) 256, n - pos);
        e->render (s.l.data() + pos, s.r.data() + pos, len);
    }
    return s;
}

bool identical (const Stereo& a, const Stereo& b)
{
    return a.size() == b.size() && std::memcmp (a.l.data(), b.l.data(), a.size() * sizeof (float)) == 0
        && std::memcmp (a.r.data(), b.r.data(), a.size() * sizeof (float)) == 0;
}

double energy (const Stereo& s, size_t from)
{
    double e = 1.0e-20;
    for (size_t i = from; i < s.size(); ++i) e += (double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i];
    return e / (double) (s.size() - from);
}

/** Largest second difference of either channel: steps and corners show up here (a click metric). */
float maxCorner (const Stereo& s, size_t from)
{
    float m = 0.0f;
    for (size_t i = std::max<size_t> (from, 2); i < s.size(); ++i)
    {
        m = std::max (m, std::abs (s.l[i] - 2.0f * s.l[i - 1] + s.l[i - 2]));
        m = std::max (m, std::abs (s.r[i] - 2.0f * s.r[i - 1] + s.r[i - 2]));
    }
    return m;
}

Stereo renderCore (Core& core, int blocks, int block = 512)
{
    Stereo s ((size_t) (blocks * block));
    std::vector<float> l ((size_t) block), r ((size_t) block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    for (int b = 0; b < blocks; ++b)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        core.process (out, block, nullptr, 0, Transport {});
        std::copy (l.begin(), l.end(), s.l.begin() + (size_t) b * (size_t) block);
        std::copy (r.begin(), r.end(), s.r.begin() + (size_t) b * (size_t) block);
    }
    return s;
}
} // namespace

TEST_CASE ("life: registered for every part after the performance wheels, 0 .. 1, default 0")
{
    Core core;
    const int first = core.params().indexOf ("p1.life");
    REQUIRE (first >= 0);
    CHECK (first > core.params().indexOf (Core::Ids::perfMod)); // appended after the performance wheels
    for (int p = 0; p < Core::numParts; ++p)
    {
        const auto id = "p" + std::to_string (p + 1) + ".life";
        const int idx = core.params().indexOf (id);
        CAPTURE (id);
        REQUIRE (idx == first + p);
        const auto& d = core.params().descriptor (idx);
        CHECK (d.minValue == 0.0f);
        CHECK (d.maxValue == 1.0f);
        CHECK (d.defaultValue == 0.0f);
        CHECK (d.nameEn == "Part " + std::to_string (p + 1) + " Life");
        CHECK (d.nameDe == "Part " + std::to_string (p + 1) + " Leben");
    }
}

TEST_CASE ("life: 0 is neutral - bit-identical whatever the seed, on every engine; the core path too")
{
    for (auto t : allEngines)
    {
        CAPTURE (engineTypeInfo (t).id);
        const auto ref = playChord (t, plainCommon(), 24000);
        CHECK (identical (playChord (t, lifeCommon (0.0f, 12345u), 24000), ref));
        CHECK (identical (playChord (t, lifeCommon (0.0f, 0xdeadbeefu), 24000), ref));
    }
    // the core: life turned up and back to 0 while nothing sounds leaves no trace
    auto render = [] (bool touch) {
        Core core;
        core.prepare (sampleRate, 512);
        core.params().set ("mix.master.limit", 0.0f);
        core.params().set ("p1.engine", (float) EngineType::Wavetable);
        if (touch)
        {
            core.params().set ("p1.life", 0.8f);
            renderCore (core, 10);
            core.params().set ("p1.life", 0.0f);
            renderCore (core, 10);
        }
        else
            renderCore (core, 20);
        core.queueNote (52, 0.9f, true);
        return renderCore (core, 60);
    };
    CHECK (identical (render (true), render (false)));
}

TEST_CASE ("life: alive on every engine; finite, no denormals, bounded, no level jump")
{
    for (auto t : allEngines)
    {
        CAPTURE (engineTypeInfo (t).id);
        const size_t n = 48000 * 3;
        const auto still = playChord (t, lifeCommon (0.0f, 7u), n);
        const auto alive = playChord (t, lifeCommon (1.0f, 7u), n);
        CHECK (allFinite (alive.l));
        CHECK (allFinite (alive.r));
        CHECK (noDenormals (alive.l));
        CHECK (noDenormals (alive.r));
        CHECK (peak (alive.l) < 4.0f);
        CHECK (peak (alive.r) < 4.0f);
        CHECK (peak (alive.l) <= 1.5f * peak (still.l) + 0.05f);
        const double d = kit::difference (still, alive);
        CAPTURE (d);
        CHECK (d > 1.0e-4); // it does something
        const double db = 10.0 * std::log10 (energy (alive, 12000) / energy (still, 12000));
        CAPTURE (db);
        CHECK (std::abs (db) < 2.0); // breathing, not a level change
    }
}

TEST_CASE ("life: the pitch drifts only a few cents")
{
    auto render = [] (float life, uint32_t seed) {
        auto e = makeEngine (EngineType::Analog, lifeCommon (life, seed));
        e->setParam (1, 0.0f); // no oscillator detune: a clean pitch to measure
        e->noteOn (57, 0.8f);  // A3, 220 Hz
        return renderEngine (*e, 48000 * 4);
    };
    const auto still = render (0.0f, 1u);
    for (uint32_t seed : { 3u, 11u, 99u })
    {
        const auto alive = render (1.0f, seed);
        for (size_t from = 0; from + 24000 <= alive.size(); from += 24000)
        {
            const double f0 = kit::bandFrequency (still.l, sampleRate, from + 2400, from + 24000, 220.0);
            const double f = kit::bandFrequency (alive.l, sampleRate, from + 2400, from + 24000, 220.0);
            CAPTURE (seed);
            CAPTURE (from);
            CAPTURE (f0);
            CAPTURE (f);
            CHECK (std::abs (centsBetween (f, f0)) < 16.0);
        }
    }
}

TEST_CASE ("life: deterministic - the same seed renders the same, another part / layer seed differs")
{
    for (auto t : { EngineType::Analog, EngineType::Grain, EngineType::Wavetable })
    {
        CAPTURE (engineTypeInfo (t).id);
        const auto a = playChord (t, lifeCommon (0.7f, 42u), 48000);
        const auto b = playChord (t, lifeCommon (0.7f, 42u), 48000);
        const auto c = playChord (t, lifeCommon (0.7f, 43u), 48000);
        CHECK (identical (a, b));
        CHECK_FALSE (identical (a, c));
    }
    // two cores, same notes: the same output
    auto render = [] {
        Core core;
        core.prepare (sampleRate, 512);
        core.params().set ("p1.engine", (float) EngineType::FM);
        core.params().set ("p1.life", 0.6f);
        core.queueNote (60, 0.9f, true);
        core.queueNote (67, 0.9f, true);
        return renderCore (core, 80);
    };
    CHECK (identical (render(), render()));
}

TEST_CASE ("life: turning it (even in jumps) never clicks; the amount glides")
{
    for (auto t : allEngines)
    {
        CAPTURE (engineTypeInfo (t).id);
        const size_t n = 48000 * 2;
        const auto still = playChord (t, lifeCommon (0.0f, 5u), n);
        // jumps 0 -> 1 -> 0 -> 1 every 0.25 s
        const auto jumpy = playChord (t, lifeCommon (0.0f, 5u), n, [] (ISynthEngine& e, size_t pos) {
            e.setCommon (lifeCommon ((pos / 12000) % 2 == 1 ? 1.0f : 0.0f, 5u));
        });
        CHECK (allFinite (jumpy.l));
        CHECK (noDenormals (jumpy.r));
        const float cs = maxCorner (still, 4800), cj = maxCorner (jumpy, 4800);
        CAPTURE (cs);
        CAPTURE (cj);
        CHECK (cj <= 2.0f * cs + 0.01f);
    }
}

TEST_CASE ("life: a modulation matrix target, MIDI-mappable, part of part presets")
{
    Core core;
    core.prepare (sampleRate, 512);
    const int idx = core.params().indexOf ("p2.life");
    REQUIRE (idx >= 0);
    CHECK (mod::destIndexOfKey ("life") == mod::DstLife);
    CHECK (mod::destTable()[(size_t) mod::DstLife].nameEn == "Life");
    CHECK (mod::destTable()[(size_t) mod::DstLife].nameDe == "Leben");
    CHECK (core.part (1).destAvailable (mod::DstLife));
    CHECK (core.part (1).destParamIndex (mod::DstLife) == idx);
    // macro 1 -> life on part 1: the matrix moves it, the parameter itself stays
    const int idx1 = core.params().indexOf ("p1.life");
    REQUIRE (core.params().set ("p1.macro1", 1.0f));
    REQUIRE (core.params().set ("p1.mod.s1.src", (float) mod::SrcMacro1));
    REQUIRE (core.params().set ("p1.mod.s1.dst", (float) mod::DstLife));
    REQUIRE (core.params().set ("p1.mod.s1.amt", 0.5f));
    core.queueNote (60, 0.9f, true);
    renderCore (core, 30);
    CHECK (core.part (0).modulationOf (idx1) == doctest::Approx (0.5f).epsilon (0.02));
    CHECK (core.params().get (idx1) == 0.0f);

    // MIDI learn: a CC drives it
    ParamRegistry& reg = core.params();
    MidiMap map;
    MidiMapper mapper (reg);
    mapper.publish (map);
    mapper.armLearn (idx);
    CHECK (mapper.handleMidi (MidiEvent::controller (1, 21, 0)));
    CHECK (mapper.applyPendingLearns (map));
    REQUIRE (map.mappings().size() == 1);
    CHECK (map.mappings()[0].paramId == "p2.life");
    CHECK (mapper.handleMidi (MidiEvent::controller (1, 21, 127)));
    CHECK (reg.get (idx) == doctest::Approx (1.0f));
}
