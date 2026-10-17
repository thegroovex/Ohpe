// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p25: performance pads - quantised stutter, tape stop, filter sweep and reverb freeze on the main mix.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "mod/ModMatrix.h"
#include "perform/PerformancePads.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <random>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
/** The pads alone: own registry and snapshot, a transport that runs. */
struct PadRig
{
    ParamRegistry reg;
    ParamSnapshot snap;
    PerformancePads pads;
    double ppq = 0.0, bpm = 120.0;
    bool playing = true;
    static constexpr int block = 256;
    std::mt19937 rng { 7 };
    Stereo out;

    PadRig()
    {
        PerformancePads::registerParams (reg);
        reg.freeze();
        snap.prepare (reg);
        pads.bind (reg);
        pads.prepare (sampleRate);
    }
    void set (const std::string& id, float v) { REQUIRE_MESSAGE (reg.set (id, v), id); }
    /** `blocks` blocks of the signal `gen(sampleIndex)` (noise by default). */
    void run (int blocks, float level = 0.3f, int kind = 0)
    {
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        std::vector<float> l (block), r (block);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const auto n = (double) out.size() + i;
                const float x = kind == 0 ? d (rng) : kind == 1 ? (float) std::sin (2.0 * 3.14159265358979 * 220.0 * n / sampleRate) : 0.0f;
                l[(size_t) i] = level * x;
                r[(size_t) i] = level * (kind == 0 ? d (rng) : x);
            }
            snap.pull (reg);
            pads.pullParams (snap);
            pads.setTransport (bpm, ppq, playing);
            if (pads.isActive()) pads.process (l.data(), r.data(), block);
            out.l.insert (out.l.end(), l.begin(), l.end());
            out.r.insert (out.r.end(), r.begin(), r.end());
            if (playing) ppq += block * bpm / 60.0 / sampleRate;
        }
    }
    int blocksFor (double seconds) const { return (int) std::ceil (seconds * sampleRate / block); }
    size_t now() const { return out.size(); }
};

/** Energy above / below ~ 2 kHz (first difference vs. moving sum) of a mono stretch. */
float highShare (const std::vector<float>& v, size_t from, size_t to)
{
    double hi = 0.0, all = 0.0;
    for (size_t i = from + 1; i < to; ++i)
    {
        const double d = v[i] - v[i - 1];
        hi += d * d;
        all += (double) v[i] * v[i];
    }
    return all > 0.0 ? (float) (hi / all) : 0.0f;
}
/** Share of the energy that survives a 32-sample moving average (the lows, below ~ 1.5 kHz). */
float lowShare (const std::vector<float>& v, size_t from, size_t to)
{
    double lo = 0.0, all = 0.0, acc = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        acc += v[i];
        if (i >= from + 32) acc -= v[i - 32];
        if (i >= from + 32) { lo += (acc / 32.0) * (acc / 32.0); all += (double) v[i] * v[i]; }
    }
    return all > 0.0 ? (float) (lo / all) : 0.0f;
}
} // namespace

TEST_CASE ("p25 pads: parameters appended after the motion block; matrix sources Pad Sweep, Pads Held")
{
    auto owner = std::make_unique<Core>();
    const auto& reg = owner->params();
    const char* ids[] = { "pads.stutter", "pads.stop", "pads.sweep", "pads.freeze", "pads.quant", "pads.stutter.div",
                          "pads.stop.len", "pads.sweep.mode", "pads.sweep.len", "pads.sweep.res", "pads.freeze.mix" };
    int expect = reg.indexOf ("p4.motion.clear") + 1;
    for (const char* id : ids)
    {
        CAPTURE (id);
        CHECK (reg.indexOf (id) == expect++);
    }
    CHECK (mod::sourceIndexOfKey ("motion4") == 26);
    CHECK (mod::sourceIndexOfKey ("padsweep") == 27);
    CHECK (mod::sourceIndexOfKey ("padheld") == 28);
    CHECK (mod::sourceTable().size() == (size_t) mod::numSources);
    for (int p = 0; p < PerformancePads::numPads; ++p) CHECK (preset::isExcludedFromProject (PerformancePads::padId (p)));
    CHECK_FALSE (preset::isExcludedFromProject ("pads.quant"));
    CHECK_FALSE (owner->pads().isActive());
}

TEST_CASE ("p25 pads: nothing held = the signal passes untouched")
{
    PadRig a;
    a.set ("pads.sweep.res", 0.9f);
    a.set ("pads.freeze.mix", 1.0f);
    a.run (8, 0.3f, 1);
    for (size_t i = 0; i < a.out.size(); ++i)
    {
        const auto n = (double) i;
        REQUIRE (a.out.l[i] == 0.3f * (float) std::sin (2.0 * 3.14159265358979 * 220.0 * n / sampleRate));
    }
    CHECK_FALSE (a.pads.isActive());
}

TEST_CASE ("p25 pads: quantised engage - a pad waits for the grid line while the transport runs, not while it stops")
{
    PadRig a;
    a.set ("pads.quant", 3.0f); // 1/4
    a.ppq = 0.1;
    a.run (1);
    a.set ("pads.stutter", 1.0f);
    a.run (1);
    CHECK (a.pads.uiState (PerformancePads::Stutter) == PerformancePads::Pending);
    while (a.ppq < 0.98) a.run (1);
    CHECK (a.pads.uiState (PerformancePads::Stutter) == PerformancePads::Pending);
    a.run (2); // crosses beat 2
    CHECK (a.pads.uiState (PerformancePads::Stutter) == PerformancePads::Engaged);
    CHECK (a.pads.heldSource() == 1.0f);
    a.set ("pads.stutter", 0.0f);
    a.run (2);
    CHECK (a.pads.uiState (PerformancePads::Stutter) == PerformancePads::Idle);
    CHECK_FALSE (a.pads.isActive());

    PadRig s;
    s.playing = false;
    s.set ("pads.quant", 5.0f); // 1 bar, but the transport stops: at once
    s.ppq = 0.3;
    s.set ("pads.stutter", 1.0f);
    s.run (1);
    CHECK (s.pads.uiState (PerformancePads::Stutter) == PerformancePads::Engaged);
}

TEST_CASE ("p25 pads: stutter repeats one slice of the division")
{
    PadRig a;
    a.set ("pads.quant", 0.0f);
    a.set ("pads.stutter.div", 2.0f); // 1/16 at 120 bpm = 6000 samples
    a.run (4);
    a.set ("pads.stutter", 1.0f);
    const size_t start = a.now();
    a.run (a.blocksFor (1.0));
    const size_t len = 6000;
    // after capture + one repeat: the output is periodic with the slice length (away from the edge fades)
    double diff = 0.0, energy = 0.0;
    for (size_t i = start + 2 * len + 200; i < start + 3 * len - 200; ++i)
    {
        diff += std::abs (a.out.l[i] - a.out.l[i + len]);
        energy += std::abs (a.out.l[i]);
    }
    CHECK (energy > 100.0);
    CHECK (diff < 1.0e-3 * energy);
    CHECK (allFinite (a.out.l));
    CHECK (peak (a.out.l) <= 0.31f);
}

TEST_CASE ("p25 pads: tape stop slows to silence; release returns to the live mix")
{
    PadRig a;
    a.set ("pads.quant", 0.0f);
    a.set ("pads.stop.len", 1.0f); // 1/4 at 120 bpm = 0.5 s
    a.run (4, 0.3f, 1);
    a.set ("pads.stop", 1.0f);
    const size_t start = a.now();
    a.run (a.blocksFor (1.0), 0.3f, 1);
    // the 220 Hz tone falls: fewer zero crossings in the second 0.2 s than in the first
    auto crossings = [&] (size_t from, size_t to) {
        int c = 0;
        for (size_t i = from + 1; i < to; ++i) c += (a.out.l[i - 1] < 0.0f) != (a.out.l[i] < 0.0f) ? 1 : 0;
        return c;
    };
    const size_t s02 = (size_t) (0.2 * sampleRate);
    CHECK (crossings (start + s02, start + 2 * s02) < crossings (start, start + s02));
    CHECK (rms (a.out.l, start + (size_t) (0.6 * sampleRate), start + (size_t) (1.0 * sampleRate)) < 1.0e-4f);
    a.set ("pads.stop", 0.0f);
    const size_t back = a.now();
    a.run (a.blocksFor (0.2), 0.3f, 1);
    CHECK (rms (a.out.l, back + 1000, a.now()) > 0.15f);
    CHECK_FALSE (a.pads.isActive());
    CHECK (allFinite (a.out.l));
    CHECK (noDenormals (a.out.l));
}

TEST_CASE ("p25 pads: filter sweep - low-pass darkens, high-pass thins; sweeps back after release")
{
    for (int mode = 0; mode < 2; ++mode)
    {
        CAPTURE (mode);
        PadRig a;
        a.set ("pads.quant", 0.0f);
        a.set ("pads.sweep.mode", (float) mode);
        a.set ("pads.sweep.len", 1.0f); // 1/4 = 0.5 s
        a.run (8);
        const float before = mode == 0 ? highShare (a.out.l, 0, a.now()) : lowShare (a.out.l, 0, a.now());
        a.set ("pads.sweep", 1.0f);
        a.run (a.blocksFor (0.7));
        const size_t s = a.now();
        a.run (a.blocksFor (0.3));
        const float during = mode == 0 ? highShare (a.out.l, s, a.now()) : lowShare (a.out.l, s, a.now());
        CHECK (a.pads.sweepSource() == doctest::Approx (1.0f));
        if (mode == 0) CHECK (during < 0.05f * before);
        else CHECK (during < 0.3f * before);
        CHECK (peak (a.out.l, s) < 4.0f);
        a.set ("pads.sweep", 0.0f);
        a.run (a.blocksFor (0.3));
        CHECK (a.pads.sweepSource() == 0.0f);
        CHECK_FALSE (a.pads.isActive());
        CHECK (allFinite (a.out.l));
        CHECK (noDenormals (a.out.l));
    }
}

TEST_CASE ("p25 pads: reverb freeze holds the throw while held, decays after release")
{
    PadRig a;
    a.set ("pads.quant", 0.0f);
    a.run (4);
    a.set ("pads.freeze", 1.0f);
    a.run (a.blocksFor (0.3)); // the throw
    const size_t silentFrom = a.now();
    a.run (a.blocksFor (3.0), 0.0f); // the input stops: the frozen wash stays
    const float held = rms (a.out.l, silentFrom + (size_t) (2.0 * sampleRate), a.now());
    CHECK (held > 0.01f);
    CHECK (held > 0.5f * rms (a.out.l, silentFrom + (size_t) (0.2 * sampleRate), silentFrom + (size_t) (1.0 * sampleRate)));
    CHECK (peak (a.out.l, silentFrom) < 4.0f);
    a.set ("pads.freeze", 0.0f);
    a.run (a.blocksFor (12.0), 0.0f);
    CHECK (rms (a.out.l, a.now() - 4800, a.now()) < 1.0e-5f);
    CHECK_FALSE (a.pads.isActive());
    CHECK (allFinite (a.out.l));
    CHECK (allFinite (a.out.r));
    CHECK (noDenormals (a.out.l));
}

TEST_CASE ("p25 pads: all four at once, hammered, in the core - finite and bounded; settings survive the state")
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    constexpr int block = 256;
    core.prepare (sampleRate, block);
    std::array<std::vector<float>, numOutputPairs> l, r;
    for (auto& v : l) v.assign (block, 0.0f);
    for (auto& v : r) v.assign (block, 0.0f);
    OutputBuffers out;
    for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
    Stereo rec;
    double ppq = 0.0;
    std::vector<MidiEvent> on { MidiEvent::noteOn (1, 48, 110, 0), MidiEvent::noteOn (1, 55, 110, 0) };
    core.params().set ("pads.sweep.res", 1.0f);
    core.params().set ("pads.freeze.mix", 1.0f);
    for (int b = 0; b < 600; ++b)
    {
        for (int p = 0; p < PerformancePads::numPads; ++p)
            core.params().set (PerformancePads::padId (p), ((b / (7 + 5 * p)) % 2) == 0 ? 1.0f : 0.0f);
        Transport t;
        t.hostDriven = true;
        t.playing = true;
        t.bpm = 128.0;
        t.ppqAtBlockStart = ppq;
        core.process (out, block, b == 0 ? on.data() : nullptr, b == 0 ? 2 : 0, t);
        rec.l.insert (rec.l.end(), l[0].begin(), l[0].end());
        rec.r.insert (rec.r.end(), r[0].begin(), r[0].end());
        ppq += block * 128.0 / 60.0 / sampleRate;
    }
    CHECK (allFinite (rec.l));
    CHECK (allFinite (rec.r));
    CHECK (noDenormals (rec.l));
    CHECK (peak (rec.l) < 1.5f);
    CHECK (rms (rec.l) > 1.0e-3f);

    core.params().set ("pads.quant", 4.0f);
    core.params().set ("pads.stutter.div", 5.0f);
    core.params().set ("pads.sweep.mode", 1.0f);
    const auto state = core.saveState();
    auto other = std::make_unique<Core>();
    REQUIRE (other->loadState (state));
    CHECK (other->params().get ("pads.quant") == 4.0f);
    CHECK (other->params().get ("pads.stutter.div") == 5.0f);
    CHECK (other->params().get ("pads.sweep.mode") == 1.0f);
    CHECK (other->params().get ("pads.freeze.mix") == doctest::Approx (1.0f));
}

TEST_CASE ("p28 pads: a pad held or latched when the host saves does not come back on with the state")
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    core.prepare (sampleRate, 256);
    for (int p = 0; p < PerformancePads::numPads; ++p) REQUIRE (core.params().set (PerformancePads::padId (p), 1.0f));
    core.params().set ("pads.quant", 3.0f);
    const auto state = core.saveState();

    auto other = std::make_unique<Core>();
    other->prepare (sampleRate, 256);
    for (int p = 0; p < PerformancePads::numPads; ++p) other->params().set (PerformancePads::padId (p), 1.0f); // even if on before
    REQUIRE (other->loadState (state));
    for (int p = 0; p < PerformancePads::numPads; ++p)
        CHECK (other->params().get (PerformancePads::padId (p)) == 0.0f);
    CHECK (other->params().get ("pads.quant") == 3.0f); // the pad settings still come back
}
