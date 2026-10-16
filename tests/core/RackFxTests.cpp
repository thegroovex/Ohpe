// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.7: the rack effect types (hyper, distortion, flanger, compressor, multiband, EQ, filter, echo, hall).

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/FxRack.h"
#include "fx/RackEffects.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
Stereo noiseAndTones (size_t n, float level = 0.5f)
{
    Stereo s (n);
    dsp::Random rng (11);
    for (size_t i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sampleRate;
        s.l[i] = level * (0.6f * std::sin (dsp::twoPi * 110.0f * t) + 0.3f * std::sin (dsp::twoPi * 1750.0f * t) + 0.1f * rng.nextBipolar());
        s.r[i] = level * (0.6f * std::sin (dsp::twoPi * 165.0f * t) + 0.3f * std::sin (dsp::twoPi * 2600.0f * t) + 0.1f * rng.nextBipolar());
    }
    return s;
}

Stereo run (IEffect& e, Stereo s, int block = 256)
{
    for (size_t pos = 0; pos < s.size(); pos += (size_t) block)
        e.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> ((size_t) block, s.size() - pos));
    return s;
}

std::unique_ptr<IEffect> make (int type)
{
    switch (type)
    {
        case InsertSlot::Hyper: return std::make_unique<HyperEffect>();
        case InsertSlot::Distortion: return std::make_unique<DistortionEffect>();
        case InsertSlot::Flanger: return std::make_unique<FlangerRackEffect>();
        case InsertSlot::Compressor: return std::make_unique<CompressorEffect>();
        case InsertSlot::Multiband: return std::make_unique<MultibandEffect>();
        case InsertSlot::Eq: return std::make_unique<EqEffect>();
        case InsertSlot::Filter: return std::make_unique<FilterRackEffect>();
        case InsertSlot::Echo: return std::make_unique<EchoEffect>();
        default: return std::make_unique<HallEffect>();
    }
}

void setDefaults (IEffect& e)
{
    const auto d = e.getParamDescriptors();
    for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
}

float rmsOf (const Stereo& s, size_t from) { return 0.5f * (rms (s.l, from) + rms (s.r, from)); }
} // namespace

TEST_CASE ("0.7 rack types: registered for every slot of every rack, names in English and German, blocks after the matrix")
{
    Core core;
    const auto& reg = core.params();
    for (int t = InsertSlot::firstRackType; t < InsertSlot::endRackTypes; ++t)
    {
        const auto info = InsertSlot::typeInfo (t);
        CAPTURE (info.id);
        for (const auto& pre : FxRack::allSlotPrefixes (Core::numParts))
        {
            auto e = make (t);
            for (const auto& d : e->getParamDescriptors()) REQUIRE (reg.indexOf (pre + info.id + "." + d.id) >= 0);
        }
    }
    CHECK (reg.indexOf ("p1.fx.hyper.rate") > reg.indexOf ("p4.seqlane"));
    const auto& type = reg.descriptor (reg.indexOf ("p1.fx.type"));
    CHECK (type.choices.size() == (size_t) InsertSlot::numTypes);
    CHECK (type.choicesDe.size() == (size_t) InsertSlot::numTypes);
    CHECK (type.choices[InsertSlot::Eq] == "EQ");
}

TEST_CASE ("0.7 rack types: finite and denormal-free at extreme settings; tails die out")
{
    const auto in = noiseAndTones (24000, 1.5f);
    for (int t = InsertSlot::firstRackType; t < InsertSlot::endRackTypes; ++t)
    {
        auto e = make (t);
        CAPTURE (e->info().id);
        e->prepare (sampleRate, 256);
        e->setTempo (174.0);
        const auto d = e->getParamDescriptors();
        for (int corner = 0; corner < 3; ++corner)
        {
            for (size_t i = 0; i < d.size(); ++i)
                e->setParam ((int) i, corner == 0 ? d[i].minValue : corner == 1 ? d[i].maxValue : d[i].defaultValue);
            e->noteOn();
            Stereo s = in;
            s.l.resize (in.size() + 96000, 0.0f);
            s.r.resize (in.size() + 96000, 0.0f);
            const auto out = run (*e, s);
            REQUIRE (allFinite (out.l));
            REQUIRE (allFinite (out.r));
            CHECK (noDenormals (out.l));
            CHECK (noDenormals (out.r));
            if (corner == 2) CHECK (peak (out.l, out.size() - 4800) < 0.05f); // default settings: the tail has died down
        }
    }
}

TEST_CASE ("0.7 EQ: flat at 0 dB; a band boosts its frequency; the response function matches the sound")
{
    EqEffect eq;
    eq.prepare (sampleRate, 256);
    setDefaults (eq);
    const auto in = noiseAndTones (12000);
    const auto flat = run (eq, in);
    float worst = 0.0f;
    for (size_t i = 0; i < in.size(); ++i) worst = std::max (worst, std::abs (flat.l[i] - in.l[i]));
    CHECK (worst < 1.0e-4f);

    std::array<float, EqEffect::numParams> v {};
    const auto d = eq.getParamDescriptors();
    for (size_t i = 0; i < d.size(); ++i) v[i] = d[i].defaultValue;
    v[EqEffect::P2Freq] = 1750.0f;
    v[EqEffect::P2Gain] = 12.0f;
    v[EqEffect::P2Q] = 2.0f;
    CHECK (EqEffect::responseDb (v.data(), 1750.0f, (float) sampleRate) == doctest::Approx (12.0f).epsilon (0.02));
    CHECK (std::abs (EqEffect::responseDb (v.data(), 110.0f, (float) sampleRate)) < 0.5f);
}

TEST_CASE ("0.7 distortion: drive adds harmonics, the transfer curves are bounded, downsample steps")
{
    for (int m = 0; m < DistortionEffect::numCurves; ++m)
        for (float x = -1.0f; x <= 1.0f; x += 0.05f)
        {
            const float y = DistortionEffect::transfer (m, 1.0f, 0.3f, x);
            CHECK (std::isfinite (y));
            CHECK (std::abs (y) <= 1.6f);
        }
    DistortionEffect dist;
    dist.prepare (sampleRate, 256);
    setDefaults (dist);
    Stereo sine (24000);
    for (size_t i = 0; i < sine.size(); ++i) sine.l[i] = sine.r[i] = 0.5f * std::sin (dsp::twoPi * 220.0f * (float) i / (float) sampleRate);
    auto hf = [] (const Stereo& s) {
        float e = 0.0f;
        for (size_t i = 12000; i < s.size(); ++i) e += std::abs (s.l[i] - 2.0f * s.l[i - 1] + s.l[i - 2]);
        return e;
    };
    dist.setParam (DistortionEffect::Drive, 0.05f);
    dist.reset();
    const float clean = hf (run (dist, sine));
    dist.setParam (DistortionEffect::Drive, 0.9f);
    dist.reset();
    const float driven = hf (run (dist, sine));
    CHECK (driven > 4.0f * clean);
}

TEST_CASE ("0.7 compressor and multiband: gain reduction on loud input, flat multiband at depth 0")
{
    CompressorEffect comp;
    comp.prepare (sampleRate, 256);
    setDefaults (comp);
    comp.setParam (CompressorEffect::Threshold, -30.0f);
    comp.setParam (CompressorEffect::Ratio, 8.0f);
    const auto loud = noiseAndTones (24000, 1.0f);
    const auto out = run (comp, loud);
    CHECK (comp.gainReductionDb() > 6.0f);
    CHECK (rmsOf (out, 12000) < 0.6f * rmsOf (loud, 12000));
    CHECK (CompressorEffect::staticGainDb (-40.0f, -30.0f, 8.0f, 0.0f) == 0.0f);
    CHECK (CompressorEffect::staticGainDb (-10.0f, -30.0f, 4.0f, 0.0f) == doctest::Approx (-15.0f));

    MultibandEffect mb;
    mb.prepare (sampleRate, 256);
    setDefaults (mb);
    mb.setParam (MultibandEffect::Depth, 0.0f);
    const auto in = noiseAndTones (24000);
    const auto flat = run (mb, in);
    const float ratioDb = 20.0f * std::log10 (rmsOf (flat, 6000) / rmsOf (in, 6000));
    CHECK (std::abs (ratioDb) < 0.5f);
    // depth: quiet material comes up, loud material goes down
    CHECK (MultibandEffect::staticGainDb (-60.0f, 1.0f, 1.0f, 1.0f) > 10.0f);
    CHECK (MultibandEffect::staticGainDb (-6.0f, 1.0f, 1.0f, 1.0f) < -10.0f);
    CHECK (MultibandEffect::staticGainDb (-90.0f, 1.0f, 1.0f, 1.0f) == 0.0f); // noise floor stays down
}

TEST_CASE ("0.7 filter, hyper, echo, hall: the sound they promise")
{
    const auto in = noiseAndTones (48000);
    {
        FilterRackEffect f; // 24 dB low-pass at 300 Hz removes the 1.75 / 2.6 kHz tones
        f.prepare (sampleRate, 256);
        setDefaults (f);
        f.setParam (FilterRackEffect::Cutoff, 300.0f);
        f.reset();
        const auto out = run (f, in);
        CHECK (rmsOf (out, 24000) < 0.75f * rmsOf (in, 24000));
    }
    {
        HyperEffect h; // a mono input comes out wide
        h.prepare (sampleRate, 256);
        setDefaults (h);
        h.setParam (HyperEffect::Mix, 1.0f);
        Stereo mono = in;
        mono.r = mono.l;
        const auto out = run (h, mono);
        float side = 0.0f, mid = 0.0f;
        for (size_t i = 24000; i < out.size(); ++i)
        {
            side += std::abs (out.l[i] - out.r[i]);
            mid += std::abs (out.l[i] + out.r[i]);
        }
        CHECK (side > 0.2f * mid);
    }
    {
        EchoEffect e; // synced 1/4 at 120 BPM: the first repeat of an impulse after 0.5 s
        e.prepare (sampleRate, 256);
        setDefaults (e);
        e.setTempo (120.0);
        e.setParam (EchoEffect::Sync, 3.0f);
        e.setParam (EchoEffect::Mode, EchoEffect::Stereo);
        e.setParam (EchoEffect::Mix, 0.5f);
        e.setParam (EchoEffect::LowCut, 20.0f);
        e.reset();
        Stereo imp (48000);
        imp.l[100] = imp.r[100] = 1.0f;
        const auto out = run (e, imp);
        size_t at = 0;
        for (size_t i = 1000; i < out.size(); ++i)
            if (std::abs (out.l[i]) > std::abs (out.l[at])) at = i;
        CHECK ((double) (at - 100) == doctest::Approx (24000.0).epsilon (0.01));
    }
    {
        HallEffect h; // the tail is longer with a longer decay
        h.prepare (sampleRate, 256);
        setDefaults (h);
        h.setParam (HallEffect::Mix, 1.0f);
        auto tailEnergy = [&] (float decay) {
            h.setParam (HallEffect::Decay, decay);
            h.reset();
            Stereo imp (96000);
            imp.l[0] = imp.r[0] = 1.0f;
            const auto out = run (h, imp);
            return rmsOf (out, 48000);
        };
        CHECK (tailEnergy (6.0f) > 3.0f * tailEnergy (1.0f));
    }
}

TEST_CASE ("0.7 rack types: parameter sweeps are smooth (no zipper on cutoff / EQ / drive automation)")
{
    // a slow sine through a filter whose cutoff jumps every 64 samples: the output stays click-free
    FilterRackEffect f;
    f.prepare (sampleRate, 64);
    setDefaults (f);
    f.setParam (FilterRackEffect::Type, FilterRackEffect::LP12);
    f.reset();
    Stereo s (48000);
    for (size_t i = 0; i < s.size(); ++i) s.l[i] = s.r[i] = 0.5f * std::sin (dsp::twoPi * 100.0f * (float) i / (float) sampleRate);
    float worst = 0.0f, prev = 0.0f, prev2 = 0.0f;
    for (size_t pos = 0; pos < s.size(); pos += 64)
    {
        f.setParam (FilterRackEffect::Cutoff, (pos / 64) % 2 ? 200.0f : 5000.0f);
        f.process (s.l.data() + pos, s.r.data() + pos, 64);
        for (size_t i = pos; i < pos + 64; ++i)
        {
            if (i > 2000) worst = std::max (worst, std::abs (s.l[i] - 2.0f * prev + prev2));
            prev2 = prev;
            prev = s.l[i];
        }
    }
    CHECK (worst < 0.01f);
}

TEST_CASE ("0.7 rack types: CPU - six rack effects per part on four parts stay well inside real time")
{
    Core core;
    core.prepare (sampleRate, 512);
    const int types[6] = { InsertSlot::Distortion, InsertSlot::Filter, InsertSlot::Hyper, InsertSlot::Multiband, InsertSlot::Echo, InsertSlot::Hall };
    for (int p = 1; p <= 4; ++p)
    {
        const auto pp = "p" + std::to_string (p) + ".";
        core.params().set (pp + "midich", 1.0f);
        for (int s = 0; s < 6; ++s) core.params().set (FxRack::partSlotPrefix (pp, s) + "type", (float) types[s]);
    }
    for (int s = 0; s < 4; ++s) core.params().set (FxRack::masterSlotPrefix (s) + "type", (float) (InsertSlot::Compressor + s));
    core.prepareEffects();
    for (int n : { 48, 55, 60, 64 }) core.queueNote (n, 0.9f, true);
    std::vector<float> l (512), r (512);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < 94; ++b) core.process (out, 512, nullptr, 0, Transport {});
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    MESSAGE ("24 rack effects + 4 master effects: " << secs << " s for 1 s of audio");
    CHECK (secs < 1.0);
    CHECK (std::isfinite (l[100]));
}
