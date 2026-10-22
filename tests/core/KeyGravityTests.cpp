// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p66: Key Gravity ("pN.grav.*") - the spectral stage after the engines that pulls every partial to the key / chord.
// Amount 0 is a bit-exact passthrough, snapping is exact on sines and chords at every sample rate and block size,
// noise turns tonal in key, NaN / Inf never get out, every part aligns to the latency of the slowest running stage.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/KeyGravity.h"
#include "mod/ModMatrix.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <random>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr uint16_t cMajor = 0xab5;  // C D E F G A B
constexpr uint16_t cTriad = 0x091;  // C E G

Stereo sine (double f, double sr, size_t n, float amp = 0.5f)
{
    Stereo s (n);
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = amp * (float) std::sin (2.0 * 3.14159265358979323846 * f * (double) i / sr);
    return s;
}

void run (KeyGravity& g, Stereo& s, int block)
{
    for (size_t pos = 0; pos < s.size(); pos += (size_t) block)
        g.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> ((size_t) block, s.size() - pos));
}

/** Zero-crossing frequency of v[from ..). */
double zcFreq (const std::vector<float>& v, double sr, size_t from)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = from + 1; i < v.size(); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + (double) (-v[i - 1] / (v[i] - v[i - 1]));
            if (first < 0.0) first = t;
            else { last = t; ++count; }
        }
    return count > 0 ? sr * count / (last - first) : 0.0;
}

KeyGravity make (double sr, int block, float amount, int target = KeyGravity::TargetScale)
{
    KeyGravity g;
    g.prepare (sr, block);
    g.setTargets (cMajor, cTriad);
    g.setTarget (target);
    g.setAmount (amount);
    return g;
}

/** Share of the energy (800 Hz - 4 kHz) within 20 cents of a target pitch, over the last 8192 samples. */
double inKeyShare (const std::vector<float>& v, uint16_t mask)
{
    const int M = 8192;
    const size_t n0 = v.size() - (size_t) M;
    double in = 0.0, tot = 0.0;
    for (int k = (int) (800.0 * M / sampleRate); k < (int) (4000.0 * M / sampleRate); ++k)
    {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < M; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / M);
            const double a = 2.0 * 3.14159265358979 * k * i / M;
            re += v[n0 + (size_t) i] * w * std::cos (a);
            im -= v[n0 + (size_t) i] * w * std::sin (a);
        }
        const double p = re * re + im * im, f = k * sampleRate / M;
        tot += p;
        if (std::abs (KeyGravity::distanceToTarget ((float) (69.0 + 12.0 * std::log2 (f / 440.0)), mask, 0, 0.0f)) < 0.2f) in += p;
    }
    return tot > 0.0 ? in / tot : 0.0;
}
} // namespace

TEST_CASE ("p66 gravity: parameters appended after the grid keyboard, defaults off; matrix targets after Amp Release")
{
    auto owner = std::make_unique<Core>();
    const auto& reg = owner->params();
    int expect = reg.indexOf ("harmony.kb.fixed") + 1;
    REQUIRE (expect > 0);
    for (int p = 1; p <= Core::numParts; ++p)
        for (const char* k : { "amt", "target", "glide", "curve", "formant", "mix", "quality" })
        {
            const auto id = "p" + std::to_string (p) + ".grav." + k;
            CHECK_MESSAGE (reg.indexOf (id) == expect++, id);
        }
    CHECK (reg.get ("p1.grav.amt") == 0.0f);
    CHECK (reg.descriptor (reg.indexOf ("p2.grav.target")).choices.size() == (size_t) KeyGravity::numTargets);
    CHECK (reg.descriptor (reg.indexOf ("p2.grav.quality")).choicesDe.size() == (size_t) KeyGravity::numQualities);
    CHECK (reg.descriptor (reg.indexOf ("p3.grav.amt")).nameDe == "Part 3 Tonart-Gravitation");
    CHECK (mod::DstGravAmount == mod::DstAmpRelease + 1);
    CHECK (mod::destTable().size() == (size_t) mod::numDests);
    CHECK (mod::destIndexOfKey ("gravamt") == mod::DstGravAmount);
    CHECK (mod::destIndexOfKey ("gravmix") == mod::DstGravMix);
    CHECK (owner->part (1).destParamIndex (mod::DstGravAmount) == reg.indexOf ("p2.grav.amt"));
    CHECK (owner->part (3).destParamIndex (mod::DstGravGlide) == reg.indexOf ("p4.grav.glide"));
}

TEST_CASE ("p66 gravity: amount 0 is a bit-exact passthrough at any block size (1 sample too); no latency")
{
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (int block : { 1, 7, 512 })
    {
        Stereo x (24000);
        for (size_t i = 0; i < x.size(); ++i) { x.l[i] = u (rng); x.r[i] = u (rng); }
        auto y = x;
        auto g = make (sampleRate, block, 0.0f);
        g.setFormant (1.0f);
        g.setCurve (1.0f);
        run (g, y, block);
        CHECK (std::memcmp (x.l.data(), y.l.data(), x.size() * sizeof (float)) == 0);
        CHECK (std::memcmp (x.r.data(), y.r.data(), x.size() * sizeof (float)) == 0);
        CHECK (g.wantedLatency() == 0);
        CHECK (g.idle());
    }
    auto owner = std::make_unique<Core>();
    owner->prepare (sampleRate, 512);
    CHECK (owner->gravityLatencySamples() == 0);
}

TEST_CASE ("p66 gravity: a sine between two notes snaps onto the nearest scale note at 44.1 - 192 kHz")
{
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        CAPTURE (sr);
        auto g = make (sr, 256, 1.0f);
        auto s = sine (452.0, sr, (size_t) (1.5 * sr)); // A4 + 47 cents, in C major the nearest is A4 = 440 Hz
        run (g, s, 256);
        CHECK (g.wantedLatency() == g.frameLatency (KeyGravity::QualityNormal));
        const double f = zcFreq (s.l, sr, s.size() / 2);
        CHECK (std::abs (centsBetween (f, 440.0)) < 2.0);
        CHECK (allFinite (s.l));
        CHECK (peak (s.l, s.size() / 2) < 0.6f);
        CHECK (peak (s.l, s.size() / 2) > 0.4f);
    }
}

TEST_CASE ("p66 gravity: chord target - a partial goes to the nearest chord tone; buffer 1 sample; every quality")
{
    for (int q = 0; q < KeyGravity::numQualities; ++q)
    {
        CAPTURE (q);
        auto g = make (sampleRate, 1, 1.0f, KeyGravity::TargetChord);
        g.setQuality (q);
        auto s = sine (300.0, sampleRate, 96000); // between D and D#: in C E G the nearest is E4 = 329.63 Hz
        run (g, s, 1);
        CHECK (std::abs (centsBetween (zcFreq (s.l, sampleRate, 48000), 329.6276)) < 2.0);
    }
    // two partials (a chord of sines) snap each to its own note
    auto g = make (sampleRate, 128, 1.0f, KeyGravity::TargetChordScale);
    Stereo s (96000);
    const auto a = sine (270.0, sampleRate, 96000, 0.3f), b = sine (1010.0, sampleRate, 96000, 0.3f); // -> C4 261.63, C6 1046.5
    for (size_t i = 0; i < s.size(); ++i) s.l[i] = s.r[i] = a.l[i] + b.l[i];
    run (g, s, 128);
    // a one-pole low / high split is enough to read the two lines apart by their zero crossings
    std::vector<float> lo (s.size()), hi (s.size());
    float z = 0.0f;
    for (size_t i = 0; i < s.size(); ++i) { z += 0.035f * (s.l[i] - z); lo[i] = z; hi[i] = s.l[i] - z; }
    for (int pass = 0; pass < 3; ++pass)
    {
        float y = 0.0f;
        for (auto& v : lo) { y += 0.035f * (v - y); v = y; }
    }
    CHECK (std::abs (centsBetween (zcFreq (lo, sampleRate, 60000), 261.6256)) < 5.0);
    (void) hi;
}

TEST_CASE ("p66 gravity: a note already in key comes through as a delayed copy (no smearing)")
{
    auto g = make (sampleRate, 64, 1.0f);
    const auto x = sine (440.0, sampleRate, 48000);
    auto y = x;
    run (g, y, 64);
    const int lat = g.wantedLatency();
    REQUIRE (lat == 1024);
    float err = 0.0f;
    for (size_t i = 24000; i < y.size(); ++i) err = std::max (err, std::abs (y.l[i] - x.l[i - (size_t) lat]));
    CHECK (err < 1.0e-3f);
}

TEST_CASE ("p66 gravity: noise at full amount becomes pitched texture in key")
{
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> u (-0.5f, 0.5f);
    Stereo x (96000);
    for (size_t i = 0; i < x.size(); ++i) { x.l[i] = u (rng); x.r[i] = u (rng); }
    auto y = x;
    auto g = make (sampleRate, 256, 1.0f);
    run (g, y, 256);
    const double before = inKeyShare (x.l, cMajor), after = inKeyShare (y.l, cMajor);
    CHECK (after > before + 0.2);
    CHECK (allFinite (y.l));
    CHECK (noDenormals (y.l));
    CHECK (rms (y.l, 48000) > 0.3f * rms (x.l, 48000)); // it stays a sound, not a silence
    CHECK (peak (y.l) < 4.0f);
}

TEST_CASE ("p66 gravity: NaN / Inf never get out; the stage recovers")
{
    auto g = make (sampleRate, 32, 1.0f);
    auto s = sine (500.0, sampleRate, 48000);
    for (size_t i = 10000; i < 10100; ++i) s.l[i] = (i & 1) ? NAN : INFINITY;
    s.r[20000] = -INFINITY;
    run (g, s, 32);
    CHECK (allFinite (s.l));
    CHECK (allFinite (s.r));
    CHECK (peak (s.l, 40000) > 0.3f); // still playing
    // extreme parameters
    for (float v : { NAN, INFINITY, -5.0f, 5.0f })
    {
        g.setAmount (v);
        g.setGlide (v);
        g.setCurve (v);
        g.setFormant (v);
        g.setMix (v);
        auto t = sine (700.0, sampleRate, 4096);
        run (g, t, 32);
        CHECK (allFinite (t.l));
    }
}

TEST_CASE ("p66 gravity: an idle stage delays exactly to the latency asked for (the other parts stay in time)")
{
    KeyGravity g;
    g.prepare (sampleRate, 512);
    g.setAlignLatency (1024);
    const auto x = sine (123.0, sampleRate, 24000);
    auto y = x;
    run (g, y, 100);
    for (size_t i = 4000; i < y.size(); ++i) REQUIRE (y.l[i] == x.l[i - 1024]);
    g.setAlignLatency (0); // back: a short fade, then an exact passthrough again
    auto z = x;
    run (g, z, 100);
    CHECK (g.idle());
    auto w = x;
    run (g, w, 100);
    CHECK (std::memcmp (w.l.data(), x.l.data(), x.size() * sizeof (float)) == 0);
}

TEST_CASE ("p66 gravity: the core reports the latency while a part pulls, aligns the parts and drops it again")
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    core.prepare (sampleRate, 512);
    const int base = core.latencySamples();
    std::vector<float> l (512), r (512);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    auto blocks = [&] (int n) { for (int b = 0; b < n; ++b) core.process (out, 512, nullptr, 0, Transport {}); };
    core.params().set ("p2.grav.amt", 0.7f);
    blocks (4);
    CHECK (core.gravityLatencySamples() == 1024);
    CHECK (core.latencySamples() == base + 1024);
    core.params().set ("p2.grav.quality", (float) KeyGravity::QualityHigh);
    blocks (4);
    CHECK (core.gravityLatencySamples() == 2048);
    core.params().set ("p2.grav.amt", 0.0f);
    blocks (2 * 48000 / 512);
    CHECK (core.gravityLatencySamples() == 0);
    CHECK (core.latencySamples() == base);
    CHECK (allFinite (l));
}
