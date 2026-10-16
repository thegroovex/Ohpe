// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/Compressor.h"
#include "fx/InsertEffects.h"
#include "fx/InsertSlot.h"
#include "fx/TapeSaturator.h"

#include <doctest/doctest.h>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
Stereo testSignal (size_t n)
{
    Stereo s (n);
    dsp::Random rng (77);
    for (size_t i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sampleRate;
        s.l[i] = 0.6f * std::sin (dsp::twoPi * 220.0f * t) + 0.1f * rng.nextBipolar();
        s.r[i] = 0.6f * std::sin (dsp::twoPi * 330.0f * t) + 0.1f * rng.nextBipolar();
    }
    return s;
}

/** Runs an effect over a signal followed by silence; returns the output. */
Stereo runEffect (IEffect& e, const Stereo& in, size_t silence)
{
    Stereo s (in.size() + silence);
    std::copy (in.l.begin(), in.l.end(), s.l.begin());
    std::copy (in.r.begin(), in.r.end(), s.r.begin());
    for (size_t pos = 0; pos < s.size(); pos += 256)
    {
        const int n = (int) std::min<size_t> (256, s.size() - pos);
        e.process (s.l.data() + pos, s.r.data() + pos, n);
    }
    return s;
}

struct SlotRig
{
    ParamRegistry reg;
    ParamSnapshot snap;
    InsertSlot slot;
    SlotRig()
    {
        InsertSlot::registerParams (reg, "p1.fx.", "part1/fx");
        InsertSlot::registerBlocks (reg, "p1.fx.", "part1/fx", InsertSlot::numLegacyTypes, InsertSlot::numTypes); // later types
        reg.freeze();
        slot.bind (reg, "p1.fx.");
        snap.prepare (reg);
        slot.prepare (sampleRate, 256);
    }
    void set (const std::string& id, float v) { REQUIRE (reg.set ("p1.fx." + id, v)); }
    Stereo run (const Stereo& in)
    {
        snap.pull (reg);
        slot.pullParams (snap);
        Stereo s = in;
        for (size_t pos = 0; pos < s.size(); pos += 256)
        {
            const int n = (int) std::min<size_t> (256, s.size() - pos);
            slot.process (s.l.data() + pos, s.r.data() + pos, n);
        }
        return s;
    }
};
} // namespace

TEST_CASE ("fx: new insert effects stay finite and denormal-free at extreme settings, tails decay")
{
    const auto in = testSignal (24000);
    std::vector<std::unique_ptr<IEffect>> effects;
    effects.push_back (std::make_unique<ChorusEffect>());
    effects.push_back (std::make_unique<PhaserEffect>());
    effects.push_back (std::make_unique<BitcrushEffect>());
    effects.push_back (std::make_unique<FilterDriveEffect>());
    for (auto& e : effects)
    {
        INFO (std::string (e->info().id));
        e->prepare (sampleRate, 256);
        const auto descs = e->getParamDescriptors();
        for (int corner = 0; corner < 3; ++corner)
        {
            for (size_t i = 0; i < descs.size(); ++i)
            {
                const auto& d = descs[i];
                const float v = corner == 0 ? d.minValue : corner == 1 ? d.maxValue : (((int) i + corner) % 2 ? d.maxValue : d.minValue);
                e->setParam ((int) i, v);
            }
            e->reset();
            const auto out = runEffect (*e, in, 48000);
            CHECK (allFinite (out.l));
            CHECK (allFinite (out.r));
            CHECK (noDenormals (out.l));
            CHECK (peak (out.l) < 16.0f);
            CHECK (peak (out.l, out.size() - 4800) < 1.0e-3f); // silence in -> (near) silence out
        }
    }
}

TEST_CASE ("fx: chorus, phaser, bitcrush and filter drive change the sound; mix 0 is transparent")
{
    const auto in = testSignal (12000);
    auto differs = [&] (IEffect& e) {
        const auto out = runEffect (e, in, 0);
        float d = 0.0f;
        for (size_t i = 0; i < in.size(); ++i) d = std::max (d, std::abs (out.l[i] - in.l[i]));
        return d;
    };
    ChorusEffect ch;
    PhaserEffect ph;
    BitcrushEffect bc;
    FilterDriveEffect fd;
    IEffect* all[] = { &ch, &ph, &bc, &fd };
    for (auto* e : all)
    {
        INFO (std::string (e->info().id));
        e->prepare (sampleRate, 256);
        CHECK (differs (*e) > 0.003f); // bitcrush at its default 8 bits: up to 1/256
        // every new insert has "mix" as its last parameter
        e->setParam ((int) e->getParamDescriptors().size() - 1, 0.0f);
        e->reset();
        CHECK (differs (*e) < 1.0e-6f);
    }
}

TEST_CASE ("fx slot: Off is an exact bypass, every type renders finite, parameters reach the effect")
{
    SlotRig rig;
    const auto in = testSignal (6000);
    CHECK (rig.run (in).l == in.l);
    // the 0.2 types; the rack types (registered per rack by FxRack) are covered in RackFxTests.cpp
    for (int t = InsertSlot::Chorus; t < InsertSlot::firstRackType; ++t)
    {
        INFO ("type " << t);
        rig.set ("type", (float) t);
        const auto out = rig.run (in);
        CHECK (rig.slot.currentType() == t);
        CHECK (allFinite (out.l));
        CHECK (out.l != in.l);
    }
    rig.set ("type", (float) InsertSlot::Delay);
    rig.set ("delay.mix", 0.0f);
    rig.slot.reset();
    const auto dry = rig.run (in);
    for (size_t i = 0; i < in.size(); ++i) REQUIRE (dry.l[i] == doctest::Approx (in.l[i]).epsilon (1e-6));
    rig.set ("type", (float) InsertSlot::Bitcrush);
    rig.set ("bitcrush.bits", 1.0f);
    const auto crushed = rig.run (in);
    for (size_t i = 0; i < 100; ++i) CHECK ((crushed.l[i] == 0.0f || std::abs (crushed.l[i]) == 1.0f));
    rig.set ("type", 0.0f);
    CHECK (rig.run (in).l == in.l);
}

TEST_CASE ("compressor: gain reduction follows threshold and ratio, transparent below threshold")
{
    Compressor c;
    c.prepare ((float) sampleRate);
    c.set (-20.0f, 4.0f, 1.0f, 500.0f, 0.0f, 0.0f);
    CHECK (c.staticGainDb (0.0f) == doctest::Approx (-15.0f));
    CHECK (c.staticGainDb (-30.0f) == 0.0f);
    Stereo s (48000);
    for (size_t i = 0; i < s.size(); ++i) s.l[i] = s.r[i] = std::sin (dsp::twoPi * 1000.0f * (float) i / (float) sampleRate);
    c.process (s.l.data(), s.r.data(), (int) s.size());
    CHECK (c.gainReductionDb() == doctest::Approx (15.0f).epsilon (0.07));
    CHECK (peak (s.l, 24000) == doctest::Approx (dsp::dbToGain (-15.0f)).epsilon (0.12));

    // below the threshold nothing changes (bit-exact)
    Compressor q;
    q.prepare ((float) sampleRate);
    q.set (-6.0f, 8.0f, 5.0f, 100.0f, 0.0f, 0.0f);
    Stereo quiet (4800);
    for (size_t i = 0; i < quiet.size(); ++i) quiet.l[i] = quiet.r[i] = 0.1f * std::sin ((float) i * 0.05f);
    auto copy = quiet;
    q.process (quiet.l.data(), quiet.r.data(), (int) quiet.size());
    CHECK (quiet.l == copy.l);
    CHECK (q.gainReductionDb() == 0.0f);

    // makeup gain
    Compressor m;
    m.prepare ((float) sampleRate);
    m.set (0.0f, 1.0f, 5.0f, 100.0f, 6.0f, 0.0f);
    float x = 0.25f, y = 0.25f;
    m.process (&x, &y, 1);
    CHECK (x == doctest::Approx (0.25f * dsp::dbToGain (6.0f)));
}

TEST_CASE ("tape saturator: amount 0 is exact, more amount = softer peaks, finite")
{
    TapeSaturator s;
    s.prepare ((float) sampleRate);
    const auto in = testSignal (4800);
    auto a = in;
    s.process (a.l.data(), a.r.data(), (int) a.size());
    CHECK (a.l == in.l);
    s.setAmount (1.0f);
    auto big = in;
    for (auto& v : big.l) v *= 4.0f;
    for (auto& v : big.r) v *= 4.0f;
    s.process (big.l.data(), big.r.data(), (int) big.size());
    CHECK (allFinite (big.l));
    CHECK (peak (big.l) < 1.0f);
}

// ---------------------------------------------------------------- mixer dynamics
namespace
{
struct DynRig
{
    Core core;
    static constexpr int n = 512;
    std::array<std::vector<float>, numOutputPairs> l, r;
    DynRig()
    {
        core.prepare (sampleRate, n);
        for (auto& v : l) v.assign (n, 0.0f);
        for (auto& v : r) v.assign (n, 0.0f);
        // a steady loud tone on tape track 1 (= mixer channel 6) as the test source
        Stereo s (TapeBlock::frames * 12);
        for (size_t i = 0; i < s.size(); ++i) s.l[i] = s.r[i] = 0.9f * std::sin (dsp::twoPi * 200.0f * (float) i / (float) sampleRate);
        TapeMachine src;
        src.prepare (sampleRate, n);
        ParamRegistry reg;
        TapeMachine::registerParams (reg);
        reg.freeze();
        ParamSnapshot snap;
        snap.prepare (reg);
        src.bind (reg);
        reg.set ("tape.tr1.arm", 1.0f);
        reg.set ("tape.tr1.src", 0.0f);
        reg.set ("tape.play", 1.0f);
        reg.set ("tape.rec", 1.0f);
        snap.pull (reg);
        src.pullParams (snap);
        std::array<std::vector<float>, 16> o;
        for (auto& v : o) v.assign (n, 0.0f);
        std::array<float*, 16> op {};
        for (size_t c = 0; c < 16; ++c) op[c] = o[c].data();
        for (size_t pos = 0; pos < s.size(); pos += n)
        {
            TapeInputs ti;
            ti.left[0] = s.l.data() + pos;
            ti.right[0] = s.r.data() + pos;
            src.process (ti, op.data(), n);
            src.housekeeping();
        }
        reg.set ("tape.rec", 0.0f);
        snap.pull (reg);
        src.pullParams (snap);
        src.process ({}, op.data(), n);
        src.housekeeping();
        core.tape().shareContentFrom (src);
        set ("tape.play", 1.0f);
    }
    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }
    float run (int blocks, int pair = 0)
    {
        float p = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            OutputBuffers out;
            for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
            core.process (out, n, nullptr, 0, Transport {});
            if (b >= blocks / 2) p = std::max (p, peak (l[(size_t) pair]));
        }
        return p;
    }
};
} // namespace

TEST_CASE ("mixer: compressor-lite, master compressor, tape stage and limiter ceiling")
{
    DynRig rig;
    rig.set ("mix.master.limit", 0.0f);
    const float plain = rig.run (40);
    CHECK (plain == doctest::Approx (0.9f).epsilon (0.02));
    CHECK (rig.core.mixer().gainReduction (5) == 0.0f);

    rig.set ("mix.ch6.comp", 1.0f);
    rig.core.tape().locate (0.0);
    const float squashed = rig.run (40);
    CHECK (rig.core.mixer().gainReduction (5) > 6.0f);
    CHECK (squashed < plain);
    rig.set ("mix.ch6.comp", 0.0f);

    rig.set ("mix.master.comp", 1.0f);
    rig.set ("mix.master.comp.thresh", -20.0f);
    rig.set ("mix.master.comp.ratio", 10.0f);
    rig.core.tape().locate (0.0);
    const float masterComp = rig.run (40);
    CHECK (rig.core.mixer().gainReduction (8) > 10.0f);
    CHECK (masterComp < 0.3f);
    rig.set ("mix.master.comp", 0.0f);

    rig.set ("mix.master.tapesat", 1.0f);
    rig.core.tape().locate (0.0);
    CHECK (rig.run (40) < plain);
    rig.set ("mix.master.tapesat", 0.0f);

    rig.set ("mix.master.vol", 6.0f);
    rig.set ("mix.master.limit", 1.0f);
    rig.set ("mix.master.ceiling", -6.0f);
    rig.core.tape().locate (0.0);
    CHECK (rig.run (40) <= dsp::dbToGain (-6.0f) + 1.0e-4f);

    // everything back to defaults: bit-identical to the plain render of a fresh core
    for (auto id : { "mix.master.vol", "mix.master.ceiling" })
        rig.set (id, rig.core.params().descriptor (rig.core.params().indexOf (id)).defaultValue);
    rig.set ("mix.master.limit", 0.0f);
    rig.core.tape().locate (0.0);
    rig.core.mixer().reset();
    rig.run (1);
    DynRig fresh;
    fresh.set ("mix.master.limit", 0.0f);
    fresh.run (1);
    CHECK (rig.l[0] == fresh.l[0]);
}

TEST_CASE ("core: part FX slot is applied to part 1 before the mixer")
{
    Core core;
    core.prepare (sampleRate, 512);
    core.params().set ("p1.fx.type", (float) InsertSlot::Bitcrush);
    core.params().set ("p1.fx.bitcrush.bits", 8.0f);
    std::vector<float> l (512), r (512);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    core.params().set ("mix.master.limit", 0.0f);
    core.queueNote (60, 1.0f, true);
    for (int b = 0; b < 20; ++b) core.process (out, 512, nullptr, 0, Transport {});
    CHECK (core.insertFx (0).currentType() == InsertSlot::Bitcrush);
    // with 8 bits the part only produces multiples of 1/128 (the channel gain has settled at 1)
    for (float v : l) REQUIRE (std::abs (v * 128.0f - std::round (v * 128.0f)) < 1.0e-3f);
    CHECK (peak (l) > 0.01f);
}
