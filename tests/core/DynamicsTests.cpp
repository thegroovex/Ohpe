// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p88: dynamics - the compressors catch the first hit after silence (onset guard, no make-up overshoot), the sustained
// level follows the static curve as before; the Multiband Comp rack type (three bands, upward + downward).

#include <doctest/doctest.h>

#include "OhpeCore.h"
#include "fx/Compressor.h"
#include "fx/FxRack.h"
#include "fx/InsertSlot.h"
#include "fx/MultibandComp.h"
#include "fx/RackEffects.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

using namespace ohpe;

namespace
{
constexpr float sr = 48000.0f;

struct Buf
{
    std::vector<float> l, r;
    explicit Buf (size_t n) : l (n, 0.0f), r (n, 0.0f) {}
    size_t size() const { return l.size(); }
};

/** silence for `lead` seconds, then a sine burst (instant onset at a zero crossing) for `len` seconds. */
Buf burst (float lead, float len, float amp, float hz, float floorAmp = 0.0f, float fadeMs = 0.0f)
{
    Buf b ((size_t) ((lead + len) * sr));
    uint32_t seed = 7;
    for (size_t i = 0; i < b.size(); ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        const float noise = floorAmp * ((float) (seed >> 8) / 8388608.0f - 1.0f);
        const float t = (float) i / sr - lead;
        const float fade = fadeMs > 0.0f ? std::min (1.0f, t * 1000.0f / fadeMs) : 1.0f;
        const float s = t >= 0.0f ? fade * amp * std::sin (2.0f * 3.14159265f * hz * t) : 0.0f;
        b.l[i] = s + noise;
        b.r[i] = s + noise;
    }
    return b;
}

template <typename Fx>
void run (Fx& fx, Buf& b, int block = 256)
{
    for (size_t pos = 0; pos < b.size(); pos += (size_t) block)
        fx.process (b.l.data() + pos, b.r.data() + pos, (int) std::min<size_t> ((size_t) block, b.size() - pos));
}

float peak (const Buf& b, float from, float to)
{
    float p = 0.0f;
    for (size_t i = (size_t) (from * sr); i < std::min (b.size(), (size_t) (to * sr)); ++i) p = std::max ({ p, std::abs (b.l[i]), std::abs (b.r[i]) });
    return p;
}

float rms (const Buf& b, float from, float to)
{
    double s = 0.0;
    size_t n = 0;
    for (size_t i = (size_t) (from * sr); i < std::min (b.size(), (size_t) (to * sr)); ++i, ++n) s += (double) b.l[i] * b.l[i];
    return n > 0 ? (float) std::sqrt (s / (double) n) : 0.0f;
}

float db (float x) { return 20.0f * std::log10 (std::max (1.0e-9f, x)); }

bool finite (const Buf& b)
{
    for (size_t i = 0; i < b.size(); ++i)
        if (! std::isfinite (b.l[i]) || ! std::isfinite (b.r[i])) return false;
    return true;
}

void setDefaults (IEffect& e)
{
    const auto d = e.getParamDescriptors();
    for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
}
} // namespace

TEST_CASE ("p88 compressor: the first hit after silence is caught - no make-up overshoot, the sustained level as the curve says")
{
    // the p87 case: threshold -24 dB, ratio 3, attack 4 ms, release 160 ms, knee 8 dB, make-up +24 dB, after 1 s of silence
    for (float attack : { 0.1f, 4.0f, 30.0f })
    {
        CAPTURE (attack);
        CompressorEffect c;
        c.prepare (sr, 512);
        setDefaults (c);
        c.setParam (CompressorEffect::Threshold, -24.0f);
        c.setParam (CompressorEffect::Ratio, 3.0f);
        c.setParam (CompressorEffect::Attack, attack);
        c.setParam (CompressorEffect::Release, 160.0f);
        c.setParam (CompressorEffect::Knee, 8.0f);
        c.setParam (CompressorEffect::Makeup, 24.0f);
        c.reset();
        auto b = burst (1.0f, 1.0f, 0.5f, 220.0f);
        run (c, b, 512);
        REQUIRE (finite (b));
        const float onset = peak (b, 1.0f, 1.05f), steady = peak (b, 1.5f, 2.0f);
        // the static curve at -6 dBFS: 18 dB over -> -12 dB, + 24 make-up -> +6 dBFS
        const float expected = -6.0f + CompressorEffect::staticGainDb (-6.02f, -24.0f, 3.0f, 8.0f) + 24.0f;
        CHECK (std::abs (db (steady) - expected) < 0.5f);
        // the transient allowance: 0.3 dB per ms of attack, at most 3 dB (+ a little margin)
        CHECK (db (onset) <= db (steady) + std::min (3.0f, 0.3f * attack) + 0.6f);
        std::printf ("OHPE-STATS p88 comp attack %.1f ms: onset %+.2f dB, steady %+.2f dB (curve %+.2f)\n", attack, db (onset), db (steady), expected);
    }
}

TEST_CASE ("p88 compressor: the mixer's compressor (channel lite, master) has the same onset guard")
{
    Compressor c;
    c.prepare (sr);
    c.set (-24.0f, 4.0f, 10.0f, 150.0f, 18.0f);
    auto b = burst (1.0f, 1.0f, 0.5f, 110.0f);
    for (size_t pos = 0; pos < b.size(); pos += 512) c.process (b.l.data() + pos, b.r.data() + pos, (int) std::min<size_t> (512, b.size() - pos));
    REQUIRE (finite (b));
    CHECK (db (peak (b, 1.0f, 1.05f)) <= db (peak (b, 1.5f, 2.0f)) + 3.6f);
    CHECK (c.gainReductionDb() > 5.0f);
}

TEST_CASE ("p88 compressor: no gain change below the threshold, a quiet signal passes with the make-up only")
{
    CompressorEffect c;
    c.prepare (sr, 256);
    setDefaults (c);
    c.setParam (CompressorEffect::Threshold, -12.0f);
    c.setParam (CompressorEffect::Knee, 0.0f);
    c.setParam (CompressorEffect::Makeup, 6.0f);
    c.reset();
    auto b = burst (0.2f, 0.5f, 0.05f, 440.0f); // -26 dBFS
    run (c, b);
    CHECK (std::abs (db (peak (b, 0.4f, 0.7f)) - (db (0.05f) + 6.0f)) < 0.05f);
}

TEST_CASE ("p88 multiband comp: registered for every slot of every rack at the end, names EN / DE, styles in range")
{
    Core core;
    const auto& reg = core.params();
    CHECK (InsertSlot::MultibandComp == InsertSlot::Resonator + 1);
    CHECK (InsertSlot::numTypes == InsertSlot::firstMbcType + 1);
    CHECK (InsertSlot::typeNamesEn()[(size_t) InsertSlot::MultibandComp] == "Multiband Comp");
    CHECK (InsertSlot::typeNamesDe()[(size_t) InsertSlot::MultibandComp] == "Multiband-Kompressor");
    CHECK (std::string (InsertSlot::typeInfo (InsertSlot::MultibandComp).id) == "mbcomp");
    MultibandCompEffect e;
    const auto d = e.getParamDescriptors();
    REQUIRE ((int) d.size() == MultibandCompEffect::numParams);
    for (const auto& pre : FxRack::allSlotPrefixes (Core::numParts))
        for (const auto& x : d) CHECK (reg.indexOf (pre + "mbcomp." + x.id) >= 0);
    // the block ends the layout (after the p77 3D parameters)
    CHECK (reg.indexOf ("p1.fx.mbcomp.depth") == reg.indexOf ("p4.space.amount") + 1);
    CHECK (reg.indexOf ("mfx4.mbcomp.highsolo") == reg.size() - 1);
    std::set<std::string> names;
    const auto& st = MultibandCompEffect::styles();
    CHECK (st.size() >= 6);
    for (const auto& s : st)
    {
        CAPTURE (s.nameEn);
        CHECK (names.insert (s.nameEn).second);
        for (size_t i = 0; i < d.size(); ++i) CHECK ((s.values[i] >= d[i].minValue && s.values[i] <= d[i].maxValue));
    }
    CHECK (names.count ("Squash") == 1);
    CHECK (names.count ("Glue") == 1);
    CHECK (names.count ("Air & Weight") == 1);
    // the default depth is about 40 %
    CHECK (d[MultibandCompEffect::Depth].defaultValue == doctest::Approx (0.4f));
}

TEST_CASE ("p88 multiband comp: depth 0 is an all-pass - flat level, phase-coherent crossovers")
{
    for (float hz : { 40.0f, 120.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f, 15000.0f })
    {
        CAPTURE (hz);
        MultibandCompEffect m;
        m.prepare (sr, 256);
        setDefaults (m);
        m.setParam (MultibandCompEffect::Depth, 0.0f);
        m.reset();
        auto b = burst (0.0f, 1.0f, 0.25f, hz);
        const float in = rms (b, 0.5f, 1.0f);
        run (m, b);
        CHECK (std::abs (db (rms (b, 0.5f, 1.0f)) - db (in)) < 0.05f);
    }
}

TEST_CASE ("p88 multiband comp: punch at a controlled level - quiet detail lifted, loud material held, the onset caught")
{
    MultibandCompEffect m;
    m.prepare (sr, 256);
    setDefaults (m);
    m.setParam (MultibandCompEffect::Depth, 1.0f);
    m.reset();
    // a quiet noise floor (-50 dBFS) lifted, then a loud burst (-6 dBFS) pushed down; its first hit must not carry the lift
    auto b = burst (1.0f, 1.0f, 0.5f, 300.0f, 0.003f, 2.0f); // a 2 ms fade-in: the onset of a note, not a click
    const float quietIn = rms (b, 0.5f, 1.0f);
    run (m, b);
    REQUIRE (finite (b));
    CHECK (db (rms (b, 0.5f, 1.0f)) > db (quietIn) + 4.0f);           // upward: the quiet part comes forward
    const float onset = peak (b, 1.0f, 1.03f), steady = peak (b, 1.5f, 2.0f);
    CHECK (db (steady) < -6.0f - 6.0f);                                // downward: the loud part is held
    CHECK (onset <= dsp::dbToGain (MultibandCompEffect::safetyCeilingDb) + 1.0e-4f);
    // the mid band alone (solo - the onset's own spread into the low band is not compression): its first hit is caught
    MultibandCompEffect mid;
    mid.prepare (sr, 256);
    setDefaults (mid);
    mid.setParam (MultibandCompEffect::Depth, 1.0f);
    mid.setParam (MultibandCompEffect::bandParam (1, MultibandCompEffect::Solo), 1.0f);
    mid.reset();
    auto c = burst (1.0f, 1.0f, 0.5f, 600.0f, 0.003f, 2.0f);
    run (mid, c);
    CHECK (db (peak (c, 1.0f, 1.03f)) <= db (peak (c, 1.5f, 2.0f)) + 1.0f);
    std::printf ("OHPE-STATS p88 mbcomp: quiet %+.1f -> %+.1f dB rms, burst onset %+.2f dB, steady %+.2f dB\n", db (quietIn),
                 db (rms (b, 0.5f, 1.0f)), db (onset), db (steady));
}

TEST_CASE ("p88 multiband comp: the output never passes the safety ceiling; extreme settings stay finite; solo")
{
    MultibandCompEffect m;
    m.prepare (sr, 256);
    setDefaults (m);
    m.setParam (MultibandCompEffect::Input, 24.0f);
    m.setParam (MultibandCompEffect::Output, 24.0f);
    m.setParam (MultibandCompEffect::Time, 0.0f);
    for (int b = 0; b < 3; ++b)
    {
        m.setParam (MultibandCompEffect::bandParam (b, MultibandCompEffect::DownRatio), 100.0f);
        m.setParam (MultibandCompEffect::bandParam (b, MultibandCompEffect::UpRatio), 100.0f);
        m.setParam (MultibandCompEffect::bandParam (b, MultibandCompEffect::Gain), 24.0f);
    }
    m.reset();
    auto b = burst (0.3f, 1.0f, 0.9f, 80.0f, 0.05f);
    run (m, b, 31);
    REQUIRE (finite (b));
    CHECK (peak (b, 0.0f, 2.0f) <= dsp::dbToGain (MultibandCompEffect::safetyCeilingDb) + 1.0e-4f);
    CHECK (m.safetyDb() > 0.0f);
    // solo the high band: a 60 Hz tone all but disappears
    MultibandCompEffect s;
    s.prepare (sr, 256);
    setDefaults (s);
    s.setParam (MultibandCompEffect::bandParam (2, MultibandCompEffect::Solo), 1.0f);
    s.reset();
    auto t = burst (0.0f, 1.0f, 0.3f, 60.0f);
    run (s, t);
    CHECK (db (rms (t, 0.5f, 1.0f)) < db (0.3f / std::sqrt (2.0f)) - 40.0f);
    // silence in -> silence out (no lift of nothing, no denormals)
    MultibandCompEffect z;
    z.prepare (sr, 256);
    setDefaults (z);
    Buf q (48000);
    run (z, q);
    CHECK (peak (q, 0.0f, 1.0f) == 0.0f);
}

TEST_CASE ("p88 multiband comp: CPU (informational)")
{
    MultibandCompEffect m;
    m.prepare (sr, 512);
    setDefaults (m);
    auto b = burst (0.0f, 10.0f, 0.3f, 220.0f, 0.05f);
    const auto t0 = std::chrono::steady_clock::now();
    run (m, b, 512);
    const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    CompressorEffect c;
    c.prepare (sr, 512);
    setDefaults (c);
    auto b2 = burst (0.0f, 10.0f, 0.3f, 220.0f, 0.05f);
    const auto t1 = std::chrono::steady_clock::now();
    run (c, b2, 512);
    const double s2 = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count();
    std::printf ("OHPE-STATS p88 cpu: Multiband Comp %.3f %% of one core, Compressor %.3f %% (48 kHz stereo)\n", s / 10.0 * 100.0, s2 / 10.0 * 100.0);
    CHECK (finite (b));
}
