// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p31: the SUB layer per part ("pN.sub.level" / ".oct" / ".shape" / ".direct"). Level 0 is neutral (bit-identical), it
// adds low end one / two octaves down on every synth engine, phase-locked to the voice (follows bend / glide), the
// shapes are band-limited, the output stays finite, bounded and mono, and it is a matrix target and part of the state.

#include "EngineTestKit.h"
#include "SampleTestHelpers.h"
#include "engines/GrainEngine.h"
#include "engines/PartSub.h"
#include "mod/ModMatrix.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstring>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr EngineType allEngines[] = { EngineType::FM,      EngineType::Analog, EngineType::Wavetable, EngineType::Pluck,
                                      EngineType::Cluster, EngineType::Grain,  EngineType::Vocal,     EngineType::Sampler };

VoiceCommon subCommon (float level, int oct = 0, int shape = PartSub::Sine, bool direct = false)
{
    auto c = plainCommon();
    c.subLevel = level;
    c.subOct = oct;
    c.subShape = shape;
    c.subDirect = direct;
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

Stereo play (EngineType t, const VoiceCommon& c, size_t n, std::initializer_list<int> notes = { 48 },
             const std::function<void (ISynthEngine&, size_t)>& perBlock = {})
{
    auto e = makeEngine (t, c);
    for (int note : notes) e->noteOn (note, 0.8f);
    Stereo s (n);
    for (size_t pos = 0; pos < n; pos += 256)
    {
        if (perBlock) perBlock (*e, pos);
        e->render (s.l.data() + pos, s.r.data() + pos, (int) std::min ((size_t) 256, n - pos));
    }
    return s;
}

bool identical (const Stereo& a, const Stereo& b)
{
    return a.size() == b.size() && std::memcmp (a.l.data(), b.l.data(), a.size() * sizeof (float)) == 0
        && std::memcmp (a.r.data(), b.r.data(), a.size() * sizeof (float)) == 0;
}

/** Energy of v[from ..) in a narrow band around f (two passes of a Q 8 band-pass). */
double bandEnergy (const std::vector<float>& v, double f, size_t from)
{
    const double w = 2.0 * 3.14159265358979323846 * f / sampleRate, q = 8.0;
    const double alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
    const double b0 = alpha / a0, b2 = -alpha / a0, a1 = -2.0 * std::cos (w) / a0, a2 = (1.0 - alpha) / a0;
    std::vector<double> x (v.begin(), v.end());
    for (int pass = 0; pass < 2; ++pass)
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (auto& s : x)
        {
            const double y = b0 * s + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = s; y2 = y1; y1 = y;
            s = y;
        }
    }
    double e = 1.0e-20;
    for (size_t i = from; i < x.size(); ++i) e += x[i] * x[i];
    return e / (double) (x.size() - from);
}

/** The sub alone, as the voices render it (no envelope), at a fixed pitch. */
std::vector<float> subAlone (int shape, int oct, float midiPitch, size_t n)
{
    PartSub s;
    s.start (1.0f);
    std::vector<float> v (n);
    for (size_t pos = 0; pos < n; pos += 32)
    {
        s.tick (1.0f, oct, shape, midiPitch, (float) sampleRate, 32);
        for (size_t i = pos; i < std::min (n, pos + 32); ++i) v[i] = s.next();
    }
    return v;
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

TEST_CASE ("p31 sub: parameters appended after the harmony brain, defaults off; matrix target Sub Level after Wide")
{
    auto owner = std::make_unique<Core>();
    const auto& reg = owner->params();
    int expect = reg.indexOf ("harmony.p4.follow") + 1;
    REQUIRE (expect > 0);
    for (int p = 1; p <= Core::numParts; ++p)
        for (const char* k : { "level", "oct", "shape", "direct" })
        {
            const auto id = "p" + std::to_string (p) + ".sub." + k;
            CHECK_MESSAGE (reg.indexOf (id) == expect++, id);
            CHECK (reg.get (id) == 0.0f);
        }
    CHECK (reg.descriptor (reg.indexOf ("p1.sub.oct")).choices.size() == 2);
    CHECK (reg.descriptor (reg.indexOf ("p1.sub.shape")).choices.size() == (size_t) PartSub::numShapes);
    CHECK (reg.descriptor (reg.indexOf ("p2.sub.level")).nameDe == "Part 2 Sub-Pegel");
    CHECK (mod::DstSubLevel == mod::DstCharWide + 1);
    CHECK (mod::DstSubLevel == 85);
    CHECK (mod::destIndexOfKey ("sublevel") == mod::DstSubLevel);
    CHECK (mod::destTable().size() == (size_t) mod::numDests);
    CHECK (mod::destTable()[(size_t) mod::DstSubLevel].nameDe == "Sub-Pegel");
    CHECK (owner->part (2).destAvailable (mod::DstSubLevel));
    CHECK (owner->part (2).destParamIndex (mod::DstSubLevel) == reg.indexOf ("p3.sub.level"));
}

TEST_CASE ("p31 sub: level 0 is neutral - bit-identical whatever the other sub settings, on every engine; the core too")
{
    for (auto t : allEngines)
    {
        CAPTURE (engineTypeInfo (t).id);
        const auto ref = play (t, plainCommon(), 24000, { 48, 55, 64 });
        CHECK (identical (play (t, subCommon (0.0f, 1, PartSub::Square, true), 24000, { 48, 55, 64 }), ref));
        CHECK (identical (play (t, subCommon (0.0f, 0, PartSub::Round, false), 24000, { 48, 55, 64 }), ref));
    }
    // the core: the sub turned up and back to 0 while nothing sounds leaves no trace
    auto render = [] (bool touch) {
        Core core;
        core.prepare (sampleRate, 512);
        core.params().set ("mix.master.limit", 0.0f);
        core.params().set ("p1.engine", (float) EngineType::Analog);
        if (touch)
        {
            core.params().set ("p1.sub.level", 0.8f);
            core.params().set ("p1.sub.shape", 2.0f);
            renderCore (core, 10);
            core.params().set ("p1.sub.level", 0.0f);
            renderCore (core, 10);
        }
        else
            renderCore (core, 20);
        core.queueNote (45, 0.9f, true);
        return renderCore (core, 60);
    };
    CHECK (identical (render (true), render (false)));
}

TEST_CASE ("p31 sub: adds low end one / two octaves down on every engine; finite, no denormals, bounded, mono")
{
    for (auto t : allEngines)
    {
        CAPTURE (engineTypeInfo (t).id);
        const size_t n = 48000;
        const double f0 = dsp::midiToHz (48.0f); // C3
        const auto dry = play (t, plainCommon(), n);
        for (int oct : { 0, 1 })
            for (int shape : { 0, 1, 2 })
                for (bool direct : { false, true })
                {
                    CAPTURE (oct);
                    CAPTURE (shape);
                    CAPTURE (direct);
                    const auto wet = play (t, subCommon (1.0f, oct, shape, direct), n);
                    CHECK (allFinite (wet.l));
                    CHECK (allFinite (wet.r));
                    CHECK (noDenormals (wet.l));
                    CHECK (noDenormals (wet.r));
                    CHECK (peak (wet.l) < 2.0f);
                    CHECK (peak (wet.l) <= peak (dry.l) + 0.6f);
                    const double fs = f0 / (oct == 0 ? 2.0 : 4.0);
                    const double gain = 10.0 * std::log10 (bandEnergy (wet.l, fs, 4800) / bandEnergy (dry.l, fs, 4800));
                    CAPTURE (gain);
                    CHECK (gain > 12.0); // clearly more energy at the sub frequency
                    // mono: the sub is the same on both channels (the difference L - R does not change)
                    double dd = 0.0, de = 1.0e-12;
                    for (size_t i = 0; i < n; ++i)
                    {
                        const double a = (double) wet.l[i] - wet.r[i], b = (double) dry.l[i] - dry.r[i];
                        dd += (a - b) * (a - b);
                        de += (double) wet.l[i] * wet.l[i];
                    }
                    if (direct || (t != EngineType::Analog && t != EngineType::Wavetable)) CHECK (dd / de < 1.0e-8);
                }
    }
}

TEST_CASE ("p31 sub: phase-locked to the voice - follows pitch bend and glide")
{
    // pitch bend (pitchOffset): the sub's frequency is exactly half the bent voice
    auto c = subCommon (1.0f, 0, PartSub::Sine, true);
    c.pitchOffset = 2.0f;
    auto e = makeEngine (EngineType::FM, c);
    e->setParam (2, 0.0f); // FM index 0: a pure sine voice
    e->noteOn (57, 0.8f);  // A3 + 2 = B3
    const auto s = renderEngine (*e, 48000);
    const double fv = dsp::midiToHz (59.0f);
    const double f = kit::bandFrequency (s.l, sampleRate, 4800, 48000, fv / 2.0);
    CAPTURE (f);
    CHECK (std::abs (centsBetween (f, fv / 2.0)) < 3.0);

    // glide: the sub glides with the voice (at the end it sits at the new note's sub)
    auto g = subCommon (1.0f, 1, PartSub::Sine, true);
    g.glideSeconds = 0.05f;
    auto e2 = makeEngine (EngineType::Analog, g);
    e2->setPolyphony (1);
    e2->noteOn (60, 0.8f);
    renderEngine (*e2, 4800);
    e2->noteOn (48, 0.8f); // mono takeover: glides
    const auto s2 = renderEngine (*e2, 48000);
    const double target = dsp::midiToHz (48.0f) / 4.0;
    const double f2 = kit::bandFrequency (s2.l, sampleRate, 24000, 48000, target);
    CAPTURE (f2);
    CHECK (std::abs (centsBetween (f2, target)) < 10.0);

    // the phase follows the voice's own cycles: after k voice cycles the sub is at k / 2 cycles
    PartSub sub;
    sub.start (1.0f);
    sub.tick (1.0f, 0, PartSub::Sine, 69.0f, 48000.0f, 32); // 440 Hz
    for (int i = 0; i < 48000; ++i) sub.next();
    const double voiceCycles = 440.0 * 48000.0 / 48000.0;
    const double subPhase = std::fmod (voiceCycles / 2.0, 1.0);
    const double dph = std::abs (sub.subPhase() - subPhase);
    CHECK (std::min (dph, 1.0 - dph) < 0.02); // on the circle
}

TEST_CASE ("p31 sub: band-limited - no aliasing even high up, every shape")
{
    // inharmonic energy of the sub alone (f0 = the sub frequency); A4 / A6 / C8 voice, one octave down
    for (int shape : { 0, 1, 2 })
        for (float pitch : { 69.0f, 93.0f, 108.0f })
        {
            CAPTURE (shape);
            CAPTURE (pitch);
            const auto v = subAlone (shape, 0, pitch, 8192 + 16384);
            CHECK (allFinite (v));
            const double f0 = dsp::midiToHz (pitch) / 2.0;
            const double db = kit::inharmonicDb (v, sampleRate, f0);
            CAPTURE (db);
            CHECK (db < -60.0);
        }
    // the shapes are about as loud (within 4 dB of the sine)
    const double sine = rms (subAlone (0, 0, 48.0f, 48000));
    for (int shape : { 1, 2 })
    {
        const double db = 20.0 * std::log10 (rms (subAlone (shape, 0, 48.0f, 48000)) / sine);
        CAPTURE (shape);
        CAPTURE (db);
        CHECK (std::abs (db) < 4.0);
    }
}

TEST_CASE ("p31 sub: turning the level (even in jumps) never clicks; it ramps")
{
    for (auto t : { EngineType::Analog, EngineType::FM, EngineType::Wavetable })
    {
        CAPTURE (engineTypeInfo (t).id);
        const auto jumpy = play (t, subCommon (0.0f), 48000, { 48 }, [] (ISynthEngine& e, size_t pos) {
            e.setCommon (subCommon ((pos / 6000) % 2 == 1 ? 1.0f : 0.0f, 0, PartSub::Sine, true));
        });
        CHECK (allFinite (jumpy.l));
        CHECK (noDenormals (jumpy.l));
        float m = 0.0f;
        for (size_t i = 4802; i < jumpy.size(); ++i) m = std::max (m, std::abs (jumpy.l[i] - 2.0f * jumpy.l[i - 1] + jumpy.l[i - 2]));
        const auto steady = play (t, subCommon (1.0f, 0, PartSub::Sine, true), 48000);
        float ms = 0.0f;
        for (size_t i = 4802; i < steady.size(); ++i) ms = std::max (ms, std::abs (steady.l[i] - 2.0f * steady.l[i - 1] + steady.l[i - 2]));
        const auto dry = play (t, plainCommon(), 48000);
        float md = 0.0f;
        for (size_t i = 4802; i < dry.size(); ++i) md = std::max (md, std::abs (dry.l[i] - 2.0f * dry.l[i - 1] + dry.l[i - 2]));
        CAPTURE (m);
        CAPTURE (ms);
        CHECK (m <= 1.5f * std::max (ms, md) + 0.005f);
    }
}

TEST_CASE ("p31 sub: the core plays it from the parameters; a matrix target; state round trip")
{
    auto render = [] (float level) {
        Core core;
        core.prepare (sampleRate, 512);
        core.params().set ("p1.engine", (float) EngineType::Wavetable);
        core.params().set ("p1.sub.level", level);
        core.params().set ("p1.sub.oct", 1.0f);
        core.queueNote (52, 0.9f, true);
        return renderCore (core, 60);
    };
    const auto dry = render (0.0f), wet = render (1.0f);
    CHECK (allFinite (wet.l));
    const double fs = dsp::midiToHz (52.0f) / 4.0;
    CHECK (bandEnergy (wet.l, fs, 4800) > 10.0 * bandEnergy (dry.l, fs, 4800));

    // macro 1 -> sub level: the matrix moves it, the parameter itself stays
    Core core;
    core.prepare (sampleRate, 512);
    const int idx = core.params().indexOf ("p1.sub.level");
    REQUIRE (core.params().set ("p1.macro1", 1.0f));
    REQUIRE (core.params().set ("p1.mod.s1.src", (float) mod::SrcMacro1));
    REQUIRE (core.params().set ("p1.mod.s1.dst", (float) mod::DstSubLevel));
    REQUIRE (core.params().set ("p1.mod.s1.amt", 0.5f));
    core.queueNote (60, 0.9f, true);
    renderCore (core, 30);
    CHECK (core.part (0).modulationOf (idx) == doctest::Approx (0.5f).epsilon (0.02));
    CHECK (core.params().get (idx) == 0.0f);

    // state
    Core a;
    a.params().set ("p3.sub.level", 0.4f);
    a.params().set ("p3.sub.oct", 1.0f);
    a.params().set ("p3.sub.shape", 2.0f);
    a.params().set ("p3.sub.direct", 1.0f);
    const auto json = a.saveState ("{}");
    Core b;
    REQUIRE (b.loadState (json, nullptr));
    for (const char* id : { "p3.sub.level", "p3.sub.oct", "p3.sub.shape", "p3.sub.direct" })
        CHECK_MESSAGE (b.params().get (id) == a.params().get (id), id);
}
