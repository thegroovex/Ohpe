// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.7: Mid/Side stereo effect (FX slot type of every part, master stereo stage), the scope FIFO
// from the audio thread to the display, stereo / spectrum analysis, parameter registration.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "dsp/StereoAnalysis.h"
#include "fx/MidSide.h"
#include "model/StereoFifo.h"

#include <doctest/doctest.h>

#include <cmath>
#include <fstream>
#include <thread>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr float twoPiF = 6.28318530718f;

/** Wide test signal: different content left and right (mid + side at several frequencies). */
Stereo wideSignal (size_t n)
{
    Stereo s;
    s.l.resize (n);
    s.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sampleRate;
        const float m = 0.3f * std::sin (twoPiF * 220.0f * t) + 0.1f * std::sin (twoPiF * 3100.0f * t);
        const float sd = 0.2f * std::sin (twoPiF * 50.0f * t) + 0.15f * std::sin (twoPiF * 2000.0f * t);
        s.l[i] = m + sd;
        s.r[i] = m - sd;
    }
    return s;
}

struct MsRig
{
    MidSideEffect fx;
    MsRig() { fx.prepare (sampleRate, 256); }
    void set (MidSideEffect::Param p, float v) { fx.setParam ((int) p, v); }
    Stereo run (Stereo s)
    {
        for (size_t pos = 0; pos < s.size(); pos += 256)
            fx.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> (256, s.size() - pos));
        return s;
    }
};

/** RMS of the side signal at one frequency (single-bin DFT over the second half). */
float sideAt (const Stereo& s, float hz)
{
    double re = 0.0, im = 0.0;
    const size_t from = s.size() / 2;
    for (size_t i = from; i < s.size(); ++i)
    {
        const double sd = 0.5 * ((double) s.l[i] - (double) s.r[i]);
        const double a = twoPiF * hz * (double) i / sampleRate;
        re += sd * std::cos (a);
        im += sd * std::sin (a);
    }
    return (float) (2.0 * std::sqrt (re * re + im * im) / (double) (s.size() - from));
}

bool finite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}
} // namespace

TEST_CASE ("mid/side: neutral settings (width 100 %) are a bit-exact bypass, also after a change")
{
    MsRig rig;
    const auto in = wideSignal (24000);
    CHECK (rig.fx.isNeutral());
    auto out = rig.run (in);
    CHECK (out.l == in.l);
    CHECK (out.r == in.r);

    // wide, then back to 100 %: once the gains have settled the output is the input again
    rig.set (MidSideEffect::Width, 1.7f);
    rig.set (MidSideEffect::MidTilt, 3.0f);
    CHECK (rig.run (in).l != in.l);
    rig.set (MidSideEffect::Width, 1.0f);
    rig.set (MidSideEffect::MidTilt, 0.0f);
    rig.run (in); // glide back (20 ms smoothing)
    out = rig.run (in);
    CHECK (out.l == in.l);
    CHECK (out.r == in.r);
}

TEST_CASE ("mid/side: width 0 is mono, 200 % doubles the side, levels act on M and S")
{
    MsRig rig;
    const auto in = wideSignal (24000);
    rig.set (MidSideEffect::Width, 0.0f);
    auto out = rig.run (in);
    for (size_t i = 19200; i < out.size(); ++i) REQUIRE (out.l[i] == out.r[i]); // after the 20 ms glide has landed
    // the mono signal is the mid of the input
    for (size_t i = 19200; i < out.size(); i += 97) CHECK (out.l[i] == doctest::Approx (0.5f * (in.l[i] + in.r[i])).epsilon (1e-4));

    rig.set (MidSideEffect::Width, 2.0f);
    out = rig.run (in);
    CHECK (sideAt (out, 2000.0f) == doctest::Approx (2.0f * sideAt (in, 2000.0f)).epsilon (0.02));

    rig.set (MidSideEffect::Width, 1.0f);
    rig.set (MidSideEffect::Side, -6.0f);
    rig.set (MidSideEffect::Mid, 6.0f);
    out = rig.run (in);
    CHECK (sideAt (out, 2000.0f) == doctest::Approx (0.5012f * sideAt (in, 2000.0f)).epsilon (0.02));
    double midIn = 0.0, midOut = 0.0;
    for (size_t i = 12000; i < in.size(); ++i)
    {
        midIn += std::pow (0.5 * (in.l[i] + in.r[i]), 2.0);
        midOut += std::pow (0.5 * (out.l[i] + out.r[i]), 2.0);
    }
    CHECK (std::sqrt (midOut / midIn) == doctest::Approx (1.9953).epsilon (0.02));
}

TEST_CASE ("mid/side: bass mono removes the side below the frequency and keeps the highs wide")
{
    MsRig rig;
    const auto in = wideSignal (48000);
    rig.set (MidSideEffect::BassMono, 1.0f);
    rig.set (MidSideEffect::MonoFreq, 150.0f);
    const auto out = rig.run (in);
    const float lowIn = sideAt (in, 50.0f), lowOut = sideAt (out, 50.0f);
    const float highIn = sideAt (in, 2000.0f), highOut = sideAt (out, 2000.0f);
    INFO ("50 Hz side " << lowIn << " -> " << lowOut << ", 2 kHz side " << highIn << " -> " << highOut);
    CHECK (lowOut < lowIn * 0.05f);                                   // > 26 dB down
    CHECK (highOut == doctest::Approx (highIn).epsilon (0.05));        // within 0.5 dB
    // the mid passes untouched (mono sum unchanged)
    for (size_t i = 24000; i < out.size(); i += 101) CHECK (out.l[i] + out.r[i] == doctest::Approx (in.l[i] + in.r[i]).epsilon (1e-4));
}

TEST_CASE ("mid/side: micro-shift widens but cancels in mono, balance, tilt; no NaN at extremes")
{
    MsRig rig;
    const auto in = wideSignal (24000);
    rig.set (MidSideEffect::Haas, 1.0f);
    rig.set (MidSideEffect::HaasTime, 0.015f);
    auto out = rig.run (in);
    CHECK (sideAt (out, 220.0f) > 0.05f); // mid content appears in the side
    for (size_t i = 12000; i < out.size(); i += 89) CHECK (out.l[i] + out.r[i] == doctest::Approx (in.l[i] + in.r[i]).epsilon (1e-4));

    MsRig bal;
    bal.set (MidSideEffect::Balance, -1.0f);
    out = bal.run (in);
    for (size_t i = 12000; i < out.size(); i += 53) CHECK (std::abs (out.r[i]) < 1.0e-4f);

    MsRig extreme;
    for (auto [p, v] : std::initializer_list<std::pair<MidSideEffect::Param, float>> {
             { MidSideEffect::Width, 2.0f }, { MidSideEffect::Mid, 12.0f }, { MidSideEffect::Side, 12.0f },
             { MidSideEffect::BassMono, 1.0f }, { MidSideEffect::MonoFreq, 500.0f }, { MidSideEffect::Haas, 1.0f },
             { MidSideEffect::MidTilt, 6.0f }, { MidSideEffect::SideTilt, -6.0f }, { MidSideEffect::HaasTime, 0.001f },
             { MidSideEffect::Balance, 0.4f } })
        extreme.set (p, v);
    Stereo loud = wideSignal (24000);
    for (size_t i = 0; i < loud.size(); ++i) { loud.l[i] *= 3.0f; loud.r[i] *= -2.5f; }
    loud.l[100] = 1.0e-30f; // denormal-range input
    out = extreme.run (loud);
    CHECK (finite (out));
    CHECK (peak (out.l) < 200.0f);
    // silence in -> silence out (tails decay, no denormals)
    Stereo quiet;
    quiet.l.assign (48000, 0.0f);
    quiet.r.assign (48000, 0.0f);
    out = extreme.run (quiet);
    CHECK (finite (out));
    CHECK (std::abs (out.l.back()) < 1.0e-6f);
}

TEST_CASE ("stereo fifo: order, wrap-around, overflow drops, clear")
{
    StereoFifo<8> f;
    float l[16], r[16], ol[16], orr[16];
    for (int i = 0; i < 16; ++i) { l[i] = (float) i; r[i] = (float) -i; }
    CHECK (f.push (l, r, 5) == 5);
    CHECK (f.available() == 5);
    CHECK (f.pop (ol, orr, 3) == 3);
    CHECK (ol[0] == 0.0f);
    CHECK (ol[2] == 2.0f);
    CHECK (orr[2] == -2.0f);
    CHECK (f.push (l + 5, r + 5, 10) == 6); // 2 waiting + 6 = full, 4 dropped
    CHECK (f.droppedFrames() == 4);
    CHECK (f.pop (ol, orr, 16) == 8);
    for (int i = 0; i < 8; ++i) CHECK (ol[i] == (float) (i + 3)); // wrapped around in order
    CHECK (f.pop (ol, orr, 16) == 0);
    f.push (l, r, 4);
    f.clear();
    CHECK (f.available() == 0);
}

TEST_CASE ("stereo fifo: one producer and one consumer thread see every frame in order")
{
    StereoFifo<256> f;
    constexpr int total = 200000;
    std::thread producer ([&] {
        float l[32], r[32];
        int next = 0;
        while (next < total)
        {
            const int n = std::min (32, total - next);
            for (int i = 0; i < n; ++i) { l[i] = (float) (next + i); r[i] = -(float) (next + i); }
            next += (int) f.push (l, r, (size_t) n); // retry what did not fit
        }
    });
    int expected = 0;
    bool ordered = true;
    float l[64], r[64];
    while (expected < total)
    {
        const size_t n = f.pop (l, r, 64);
        for (size_t i = 0; i < n; ++i)
        {
            ordered = ordered && l[i] == (float) expected && r[i] == -(float) expected;
            ++expected;
        }
    }
    producer.join();
    CHECK (ordered);
    CHECK (expected == total);
}

TEST_CASE ("stereo analysis: correlation, M/S levels, spectrum bands")
{
    std::vector<float> a (4096), b (4096), c (4096);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = 0.5f * std::sin (twoPiF * 1000.0f * (float) i / 48000.0f);
        b[i] = -a[i];
        c[i] = 0.5f * std::sin (twoPiF * 1370.0f * (float) i / 48000.0f + 0.3f);
    }
    auto mono = dsp::analyseStereo (a.data(), a.data(), a.size());
    CHECK (mono.correlation == doctest::Approx (1.0f));
    CHECK (mono.sideRms == 0.0f);
    CHECK (mono.midRms == doctest::Approx (0.3536f).epsilon (0.01));
    auto anti = dsp::analyseStereo (a.data(), b.data(), a.size());
    CHECK (anti.correlation == doctest::Approx (-1.0f));
    CHECK (anti.midRms == 0.0f);
    auto unrelated = dsp::analyseStereo (a.data(), c.data(), a.size());
    CHECK (std::abs (unrelated.correlation) < 0.1f);
    std::vector<float> silent (512, 0.0f);
    CHECK (dsp::analyseStereo (silent.data(), silent.data(), silent.size()).correlation == 0.0f);

    dsp::SpectrumBands bands (10, 48);
    const auto& v = bands.analyse (a.data(), a.size(), 48000.0);
    REQUIRE (v.size() == 48);
    size_t best = 0;
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] > v[best]) best = i;
    // band of 1 kHz on the 30 Hz .. 16 kHz log scale
    const size_t expectBand = (size_t) (48.0 * std::log (1000.0 / 30.0) / std::log (16000.0 / 30.0));
    CHECK (std::abs ((int) best - (int) expectBand) <= 1);
    CHECK (v[best] > 0.85f); // -6 dBFS sine near the top of the -72 .. 0 dB range
    CHECK (v[2] < 0.3f);
    for (float x : bands.analyse (silent.data(), silent.size(), 48000.0)) CHECK (x == 0.0f);
}

TEST_CASE ("parameters 0.7: the Mid/Side type in every slot of every rack (parts and master), appended")
{
    Core core;
    const auto& reg = core.params();
    const MidSideEffect ms;
    const int lastBefore = reg.indexOf ("mfx1.type"); // the 0.7 rack parameters come first
    REQUIRE (lastBefore >= 0);
    const auto prefixes = FxRack::allSlotPrefixes (Core::numParts);
    CHECK (prefixes.size() == (size_t) (Core::numParts * FxRack::numPartSlots + FxRack::numMasterSlots));
    int count = 0;
    for (const auto& pre : prefixes)
    {
        INFO (pre);
        const int type = reg.indexOf (pre + "type");
        REQUIRE (type >= 0);
        const auto& d = reg.descriptor (type);
        REQUIRE ((int) d.choices.size() == InsertSlot::numTypes);
        CHECK (d.choices[(size_t) InsertSlot::MidSide] == "Mid/Side");
        CHECK (d.choicesDe[(size_t) InsertSlot::MidSide] == "Mitte/Seite");
        for (const auto& pd : ms.getParamDescriptors())
        {
            const int idx = reg.indexOf (pre + "midside." + pd.id);
            CHECK (idx > lastBefore);
            count += idx >= 0 ? 1 : 0;
        }
    }
    CHECK (count == (int) prefixes.size() * MidSideEffect::numParams);

    // MIDI-learnable like every parameter
    Core c2;
    c2.prepare (sampleRate, 256);
    const int width = c2.params().indexOf ("p3.fx.midside.width");
    c2.midiMapper().armLearn (width);
    std::vector<float> l (256), r (256);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    auto cc = MidiEvent::controller (1, 21, 0);
    c2.process (out, 256, &cc, 1, Transport {});
    CHECK (c2.midiMapper().applyPendingLearns (c2.midiMap()));
    cc = MidiEvent::controller (1, 21, 127);
    c2.process (out, 256, &cc, 1, Transport {});
    CHECK (c2.params().get (width) == doctest::Approx (2.0f));
}

TEST_CASE ("core 0.7: Mid/Side slot on part 2 makes it mono, the scope tap delivers the chosen source")
{
    Core core;
    core.prepare (sampleRate, 256);
    auto& p = core.params();
    p.set ("mix.ch2.pan", 0.0f);
    p.set ("p2.fx.type", (float) InsertSlot::Chorus); // a wide part ...
    p.set ("p2.fx.chorus.depth", 1.0f);
    p.set ("p2.fx.chorus.mix", 0.5f);
    std::vector<float> l (256), r (256);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    auto render = [&] (int blocks, std::vector<MidiEvent> midi = {}) {
        Stereo s;
        for (int b = 0; b < blocks; ++b)
        {
            core.process (out, 256, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, Transport {});
            s.l.insert (s.l.end(), l.begin(), l.end());
            s.r.insert (s.r.end(), r.begin(), r.end());
        }
        return s;
    };
    core.scope().setSource (1); // part 2
    const auto wide = render (40, { MidiEvent::noteOn (2, 60, 100, 0) }); // part 2 listens on channel 2
    CHECK (rms (wide.l) > 0.001f);
    double side = 0.0;
    for (size_t i = 0; i < wide.size(); ++i) side += std::abs (wide.l[i] - wide.r[i]);
    CHECK (side > 0.1);
    // the tap delivered part 2 (after its FX slot)
    std::vector<float> sl (8192), sr (8192);
    const size_t got = core.scope().read (sl.data(), sr.data(), sl.size());
    CHECK (got >= 8000);
    CHECK (rms (sl) > 0.001f);

    // ... made mono by the Mid/Side slot (chorus replaced by Mid/Side, width 0)
    p.set ("p2.fx.type", (float) InsertSlot::MidSide);
    p.set ("p2.fx.midside.width", 0.0f);
    render (60);
    CHECK (core.insertFx (1).currentType() == InsertSlot::MidSide);
    const auto mono = render (20);
    for (size_t i = 0; i < mono.size(); ++i) REQUIRE (mono.l[i] == doctest::Approx (mono.r[i]).epsilon (1e-5));

    // master tap, then off: nothing more arrives
    core.scope().setSource (ScopeTap::Master);
    core.scope().read (sl.data(), sr.data(), sl.size());
    render (4);
    CHECK (core.scope().read (sl.data(), sr.data(), sl.size()) == 4 * 256);
    core.scope().setSource (ScopeTap::None);
    core.scope().read (sl.data(), sr.data(), sl.size());
    render (4);
    CHECK (core.scope().read (sl.data(), sr.data(), sl.size()) == 0);
}

TEST_CASE ("core 0.7: Mid/Side in the master rack narrows the whole mix")
{
    Core core;
    core.prepare (sampleRate, 256);
    auto& p = core.params();
    p.set ("p1.fx.type", (float) InsertSlot::Chorus);
    p.set ("p1.fx.chorus.depth", 1.0f);
    std::vector<float> l (256), r (256);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    auto sideSum = [&] (int blocks, bool note) {
        double s = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            auto on = MidiEvent::noteOn (1, 57, 110, 0);
            core.process (out, 256, note && b == 0 ? &on : nullptr, note && b == 0 ? 1 : 0, Transport {});
            for (int i = 0; i < 256; ++i) s += std::abs (l[(size_t) i] - r[(size_t) i]);
        }
        return s;
    };
    CHECK (sideSum (30, true) > 0.1);
    p.set ("mfx2.type", (float) InsertSlot::MidSide);
    p.set ("mfx2.midside.width", 0.0f);
    core.prepareEffects(); // message thread: the master slots prepare their types lazily
    sideSum (60, false);   // the width glides to 0
    CHECK (sideSum (20, false) < 1.0e-3);
    core.scope().setSource (ScopeTap::Master);
    std::vector<float> sl (4096), sr (4096);
    core.scope().read (sl.data(), sr.data(), sl.size());
    sideSum (4, false);
    REQUIRE (core.scope().read (sl.data(), sr.data(), sl.size()) == 4 * 256);
    const auto st = dsp::analyseStereo (sl.data(), sr.data(), 4 * 256);
    CHECK (st.sideRms < 1.0e-6f);
    CHECK (st.correlation > 0.999f);
}
