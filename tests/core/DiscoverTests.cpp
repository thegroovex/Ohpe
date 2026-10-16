// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p21: pro view 2.0 core - the character macros (BRIGHT / HARD / ALIVE / WIDE per part: 0 is neutral, they move real
// parameters, the output stays finite and bounded, matrix targets, MIDI-mappable) and the Discover genome (vary,
// cross, locks, strength, the 20-state timeline).

#include "TestHelpers.h"
#include "OhpeCore.h"
#include "discover/Discover.h"
#include "midi/MidiMap.h"
#include "mod/Character.h"
#include "mod/ModMatrix.h"

#include <doctest/doctest.h>

#include <cmath>
#include <fstream>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr double rate = 48000.0;
constexpr EngineType allEngines[] = { EngineType::FM,      EngineType::Analog, EngineType::Wavetable, EngineType::Pluck,
                                      EngineType::Cluster, EngineType::Grain,  EngineType::Vocal,     EngineType::Sampler };

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

/** A chord on part 1 with engine `t` and the four macros set to `m`. */
Stereo playWith (EngineType t, std::array<float, 4> m, int blocks = 40)
{
    Core core;
    core.prepare (rate, 512);
    core.params().set ("mix.master.limit", 0.0f);
    core.params().set ("p1.engine", (float) t);
    for (int k = 0; k < character::numMacros; ++k) core.params().set (std::string ("p1.char.") + character::key (k), m[(size_t) k]);
    for (int n : { 48, 55, 64 }) core.queueNote (n, 0.8f, true);
    return renderCore (core, blocks);
}

bool identical (const Stereo& a, const Stereo& b) { return a.l == b.l && a.r == b.r; }

/** Energy of the first difference relative to the signal (rises with brightness). */
float brightness (const std::vector<float>& v)
{
    double d = 0.0, e = 0.0;
    for (size_t i = 1; i < v.size(); ++i) { d += (double) (v[i] - v[i - 1]) * (v[i] - v[i - 1]); e += (double) v[i] * v[i]; }
    return e > 0.0 ? (float) (d / e) : 0.0f;
}

float sideRatio (const Stereo& s)
{
    double side = 0.0, mid = 0.0;
    for (size_t i = 0; i < s.l.size(); ++i)
    {
        const double m = 0.5 * (s.l[i] + s.r[i]), d = 0.5 * (s.l[i] - s.r[i]);
        mid += m * m;
        side += d * d;
    }
    return mid > 0.0 ? (float) (side / mid) : 0.0f;
}
} // namespace

TEST_CASE ("character macros: four per part, appended after the morph block, -1 .. 1, default 0, EN / DE names")
{
    Core core;
    const int first = core.params().indexOf ("p1.char.bright");
    REQUIRE (first >= 0);
    CHECK (first > core.params().indexOf ("p4.morph.ysrc"));
    for (int p = 0; p < Core::numParts; ++p)
        for (int m = 0; m < character::numMacros; ++m)
        {
            const auto id = "p" + std::to_string (p + 1) + ".char." + character::key (m);
            CAPTURE (id);
            const int idx = core.params().indexOf (id);
            REQUIRE (idx == first + p * character::numMacros + m);
            const auto& d = core.params().descriptor (idx);
            CHECK (d.minValue == -1.0f);
            CHECK (d.maxValue == 1.0f);
            CHECK (d.defaultValue == 0.0f);
            CHECK (d.nameEn == "Part " + std::to_string (p + 1) + " " + character::nameEn (m));
            CHECK (d.nameDe == "Part " + std::to_string (p + 1) + " " + character::nameDe (m));
        }
    CHECK (std::string (character::nameDe (character::Alive)) == "Lebendig");
}

TEST_CASE ("character macros: 0 is neutral (bit-identical) on every engine; turned, the output stays finite and bounded")
{
    for (auto t : allEngines)
    {
        CAPTURE (engineTypeInfo (t).id);
        const auto ref = playWith (t, { 0.0f, 0.0f, 0.0f, 0.0f }, 20);
        Core plain;
        plain.prepare (rate, 512);
        plain.params().set ("mix.master.limit", 0.0f);
        plain.params().set ("p1.engine", (float) t);
        for (int n : { 48, 55, 64 }) plain.queueNote (n, 0.8f, true);
        CHECK (identical (renderCore (plain, 20), ref));
        for (float v : { -1.0f, 1.0f })
        {
            const auto s = playWith (t, { v, v, v, v }, 20);
            CHECK (allFinite (s.l));
            CHECK (allFinite (s.r));
            CHECK (noDenormals (s.l));
            CHECK (peak (s.l) < 4.0f);
            CHECK (peak (s.r) < 4.0f);
        }
    }
}

TEST_CASE ("character macros move real parameters: BRIGHT brightens, WIDE widens, HARD shortens the attack, ALIVE drives life")
{
    // BRIGHT on the analog engine opens the filter
    const auto dark = playWith (EngineType::Analog, { -1.0f, 0.0f, 0.0f, 0.0f });
    const auto bright = playWith (EngineType::Analog, { 1.0f, 0.0f, 0.0f, 0.0f });
    CHECK (brightness (bright.l) > 1.3f * brightness (dark.l));
    // WIDE on the cluster engine spreads the voices
    const auto narrow = playWith (EngineType::Cluster, { 0.0f, 0.0f, 0.0f, -1.0f });
    const auto wide = playWith (EngineType::Cluster, { 0.0f, 0.0f, 0.0f, 1.0f });
    CHECK (sideRatio (wide) > sideRatio (narrow));
    // HARD: a slow attack becomes faster (more level in the first 20 ms)
    auto onset = [] (float hard) {
        Core core;
        core.prepare (rate, 512);
        core.params().set ("mix.master.limit", 0.0f);
        core.params().set ("p1.amp.a", 0.4f);
        core.params().set ("p1.char.hard", hard);
        core.queueNote (60, 0.8f, true);
        const auto s = renderCore (core, 4);
        return rms (s.l, 0, 1024);
    };
    CHECK (onset (1.0f) > 1.5f * onset (0.0f));
    // the macro values are not written into the parameters they move
    Core core;
    core.params().set ("p1.char.bright", 1.0f);
    CHECK (core.params().get ("p1.analog.cutoff") == core.params().descriptor (core.params().indexOf ("p1.analog.cutoff")).defaultValue);
    // weights: every macro reaches something on every engine family
    CHECK (character::engineWeights ("cutoff").w[character::Bright] > 0.0f);
    CHECK (character::engineWeights ("drive").w[character::Hard] > 0.0f);
    CHECK (character::engineWeights ("drift").w[character::Alive] > 0.0f);
    CHECK (character::engineWeights ("spread").w[character::Wide] > 0.0f);
    CHECK_FALSE (character::engineWeights ("algo").any());
    CHECK (character::lifeAlive > 0.0f);
}

TEST_CASE ("character macros: modulation matrix targets, MIDI-mappable")
{
    Core core;
    core.prepare (rate, 512);
    const char* keys[] = { "charbright", "charhard", "charalive", "charwide" };
    for (int m = 0; m < character::numMacros; ++m)
    {
        const int dst = mod::destIndexOfKey (keys[m]);
        CAPTURE (keys[m]);
        CHECK (dst == mod::DstCharBright + m);
        CHECK (core.part (2).destAvailable (dst));
        CHECK (core.part (2).destParamIndex (dst) == core.params().indexOf (std::string ("p3.char.") + character::key (m)));
    }
    CHECK (mod::destTable()[(size_t) mod::DstCharWide].nameDe == "Weit");
    // the mod wheel opens BRIGHT through the matrix; the parameter itself stays at 0
    const int idx = core.params().indexOf ("p1.char.bright");
    REQUIRE (core.params().set ("p1.macro1", 1.0f));
    REQUIRE (core.params().set ("p1.mod.s1.src", (float) mod::SrcMacro1));
    REQUIRE (core.params().set ("p1.mod.s1.dst", (float) mod::DstCharBright));
    REQUIRE (core.params().set ("p1.mod.s1.amt", 0.4f));
    core.queueNote (60, 0.9f, true);
    renderCore (core, 30);
    CHECK (core.part (0).modulationOf (idx) == doctest::Approx (0.4f).epsilon (0.02));
    CHECK (core.params().get (idx) == 0.0f);

    ParamRegistry& reg = core.params();
    MidiMap map;
    MidiMapper mapper (reg);
    mapper.publish (map);
    mapper.armLearn (idx);
    CHECK (mapper.handleMidi (MidiEvent::controller (1, 22, 0)));
    CHECK (mapper.applyPendingLearns (map));
    CHECK (mapper.handleMidi (MidiEvent::controller (1, 22, 127)));
    CHECK (reg.get (idx) == doctest::Approx (1.0f));
}

TEST_CASE ("discover: the genome holds the sound, not the routing; capture / apply round trip")
{
    CHECK (discover::isBreedable ("fm.index"));
    CHECK (discover::isBreedable ("amp.a"));
    CHECK (discover::isBreedable ("mod.s")); // mod envelope sustain
    CHECK_FALSE (discover::isBreedable ("mod.s3.dst"));
    CHECK_FALSE (discover::isBreedable ("engine"));
    CHECK_FALSE (discover::isBreedable ("midich"));
    CHECK_FALSE (discover::isBreedable ("char.bright"));
    CHECK_FALSE (discover::isBreedable ("sampler.inst"));
    CHECK_FALSE (discover::isBreedable ("fx2.delay.mix"));

    Core core;
    auto& reg = core.params();
    reg.set ("p2.fm.index", 0.62f);
    const auto g = discover::capture (reg, { "p2.fm.index", "p2.fm.algo", "p2.engine", "p2.amp.r", "nope" });
    REQUIRE (g.size() == 3);
    CHECK (g.genes[0].steps == 0);
    CHECK (g.genes[1].steps == 8); // eight algorithms
    Core other;
    discover::apply (other.params(), g);
    CHECK (other.params().get ("p2.fm.index") == doctest::Approx (0.62f).epsilon (0.001));
    CHECK (discover::capture (other.params(), { "p2.fm.index", "p2.fm.algo", "p2.amp.r" }).distance (g) < 1.0e-4f);
}

TEST_CASE ("discover: siblings are deterministic, differ, respect locks; strength scales the distance; cross blends")
{
    Core core;
    std::vector<std::string> ids;
    for (const char* k : { "pos", "warp", "tone", "unison", "table", "reso", "sub", "keytrack" }) ids.push_back (std::string ("p1.wavetable.") + k);
    for (const char* k : { "amp.a", "amp.d", "amp.s", "amp.r", "lfo.rate", "lfo.depth" }) ids.push_back (std::string ("p1.") + k);
    const auto parent = discover::capture (core.params(), ids);
    REQUIRE (parent.size() == ids.size());

    const auto a = discover::siblings (parent, 6, 0.5f, 42u);
    const auto b = discover::siblings (parent, 6, 0.5f, 42u);
    REQUIRE (a.size() == 6);
    for (size_t i = 0; i < a.size(); ++i)
    {
        CHECK (a[i].distance (b[i]) == 0.0f);
        CHECK (a[i].distance (parent) > 0.0f);
        for (const auto& gene : a[i].genes)
        {
            CHECK (gene.value >= 0.0f);
            CHECK (gene.value <= 1.0f);
            CHECK (std::isfinite (gene.value));
        }
    }
    CHECK (a[0].distance (a[1]) > 0.0f);

    // locks: the first half stays
    std::vector<bool> mask (parent.size(), true);
    for (size_t i = 0; i < 7; ++i) mask[i] = false;
    for (const auto& s : discover::siblings (parent, 6, 1.0f, 7u, mask))
        for (size_t i = 0; i < 7; ++i) CHECK (s.genes[i].value == parent.genes[i].value);

    // gentle stays closer than wild (averaged over many seeds)
    float gentle = 0.0f, wild = 0.0f;
    for (uint32_t seed = 1; seed <= 40; ++seed)
    {
        gentle += discover::breeder().vary (parent, 0.0f, seed, {}).distance (parent);
        wild += discover::breeder().vary (parent, 1.0f, seed, {}).distance (parent);
    }
    CHECK (wild > 2.0f * gentle);

    // cross: every continuous gene lies between the parents
    const auto child = discover::breeder().cross (a[0], a[1], 9u);
    for (size_t i = 0; i < child.size(); ++i)
    {
        const float lo = std::min (a[0].genes[i].value, a[1].genes[i].value), hi = std::max (a[0].genes[i].value, a[1].genes[i].value);
        CHECK (child.genes[i].value >= lo - 1.0e-6f);
        CHECK (child.genes[i].value <= hi + 1.0e-6f);
    }
}

TEST_CASE ("discover: the timeline keeps the last 20 states, steps back, drops the future on a new state")
{
    discover::Timeline t;
    CHECK (t.select (3) == nullptr);
    discover::Genome g;
    g.genes.push_back ({ "p1.amp.a", 0.0f, 0 });
    for (int i = 0; i < 25; ++i)
    {
        g.genes[0].value = (float) i / 30.0f;
        t.push (g);
    }
    CHECK (t.size() == 20);
    CHECK (t.current() == 19);
    CHECK (t.at (0)->genes[0].value == doctest::Approx (5.0f / 30.0f));
    t.push (g); // the same state again: no new dot
    CHECK (t.size() == 20);
    const auto* back = t.select (10);
    REQUIRE (back != nullptr);
    CHECK (t.current() == 10);
    g.genes[0].value = 0.99f;
    t.push (g);
    CHECK (t.size() == 12);
    CHECK (t.current() == 11);
}
