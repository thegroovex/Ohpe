// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p77: 3D motion retrigger (pN.space.retrig: Free / First / Note) and 3D strength (pN.space.amount).
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/Space3D.h"
#include "mod/ModMatrix.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <cmath>
#include <fstream>
#include <string>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr float twoPi = 6.28318530718f;

struct Fx
{
    SpaceEffect e;
    Fx()
    {
        e.prepare (sampleRate, 512);
        const auto d = e.getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
        e.setMonitorMode (SpaceEffect::Headphones);
        e.setTempo (120.0);
    }
    void silence (int n)
    {
        std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
        e.process (l.data(), r.data(), n);
    }
};

Stereo sine (float hz, size_t n, float amp = 0.3f)
{
    Stereo s (n);
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = amp * std::sin (twoPi * hz * (float) i / (float) sampleRate);
    return s;
}

Stereo noise (size_t n, uint32_t seed)
{
    Stereo s (n);
    dsp::Random a (seed), b (seed * 7 + 3);
    for (size_t i = 0; i < n; ++i) { s.l[i] = 0.3f * a.nextBipolar(); s.r[i] = 0.3f * b.nextBipolar(); }
    return s;
}

double clickRatio (const std::vector<float>& v, float hz, size_t from)
{
    double d2 = 0.0;
    for (size_t i = from + 1; i + 1 < v.size(); ++i) d2 = std::max (d2, (double) std::abs (v[i + 1] - 2.0f * v[i] + v[i - 1]));
    const double w = twoPi * hz / sampleRate;
    return d2 / ((double) peak (v, from) * w * w);
}

double rmsDb (const Stereo& s, size_t from)
{
    double acc = 0.0;
    for (size_t i = from; i < s.size(); ++i) acc += (double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i];
    return 10.0 * std::log10 (std::max (1.0e-30, acc / (double) (2 * (s.size() - from))));
}

float angleBetween (space::Vec3 a, space::Vec3 b)
{
    const float la = space::lengthOf (a), lb = space::lengthOf (b);
    const float c = (a.x * b.x + a.y * b.y + a.z * b.z) / std::max (1.0e-6f, la * lb);
    return std::acos (std::clamp (c, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
}
} // namespace

TEST_CASE ("p77 space: retrigger and amount IDs - appended per part after the chord headroom, choices, defaults, matrix target")
{
    Core core;
    const auto& reg = core.params();
    int prev = reg.indexOf ("p4.chordroom");
    REQUIRE (prev >= 0);
    for (int p = 1; p <= Core::numParts; ++p)
    {
        const auto P = "p" + std::to_string (p) + ".space.";
        const int r = reg.indexOf (P + "retrig"), a = reg.indexOf (P + "amount");
        REQUIRE (r >= 0);
        REQUIRE (a >= 0);
        CHECK (r == prev + 1);
        CHECK (a == r + 1);
        prev = a;
        const auto& dr = reg.descriptor (r);
        REQUIRE (dr.choices.size() == 3);
        CHECK (dr.choices[0] == "Free");
        CHECK (dr.choices[1] == "First");
        CHECK (dr.choices[2] == "Note");
        CHECK (dr.defaultValue == 0.0f);
        const auto& da = reg.descriptor (a);
        CHECK (da.defaultValue == 1.0f);
        CHECK (da.nameEn.find ("Amount") != std::string::npos);
        CHECK (da.nameDe.find ("Stärke") != std::string::npos);
    }
    CHECK (mod::destIndexOfKey ("spaceamount") == mod::DstSpaceAmount);
    CHECK (mod::DstSpaceAmount == mod::DstGravMix + 1);
    CHECK (core.part (2).destParamIndex (mod::DstSpaceAmount) == reg.indexOf ("p3.space.amount"));
}

TEST_CASE ("p77 space: Note restarts the motion at its start position on each note, Free keeps running")
{
    for (int mode : { SpaceEffect::RetrigFree, SpaceEffect::RetrigNote })
    {
        CAPTURE (mode);
        Fx fx;
        fx.e.setParam (SpaceEffect::Path, (float) space::PathCircle);
        fx.e.setParam (SpaceEffect::PathLength, 4.0f); // one bar = 2 s at 120 bpm
        fx.e.setParam (SpaceEffect::PathSize, 1.0f);
        fx.e.setRetrigger (mode);
        fx.silence (512);
        const auto start = fx.e.currentPosition();
        fx.silence (24000); // a quarter turn later: 90 degrees away
        CHECK (angleBetween (start, fx.e.currentPosition()) > 45.0f);
        fx.e.retrigger();
        fx.silence (48 * 40); // 40 ms: the quick glide has arrived
        const float off = angleBetween (start, fx.e.currentPosition());
        if (mode == SpaceEffect::RetrigNote) CHECK (off < 12.0f); // 25 ms of motion since the restart + the glide's rest
        else CHECK (off > 45.0f);
    }
}

TEST_CASE ("p77 space: First / Note follow the keys in the part (first note after silence, every note, mono legato)")
{
    Core core;
    core.prepare (sampleRate, 256);
    auto& reg = core.params();
    REQUIRE (reg.set ("p1.space.on", 1.0f));
    REQUIRE (reg.set ("p1.space.path", (float) space::PathCircle));
    REQUIRE (reg.set ("p1.space.pathlen", 4.0f));
    REQUIRE (reg.set ("p1.space.pathsize", 1.0f));
    std::array<std::vector<float>, numOutputPairs> l, r;
    for (auto& v : l) v.assign (256, 0.0f);
    for (auto& v : r) v.assign (256, 0.0f);
    auto run = [&] (int blocks, std::vector<MidiEvent> midi = {}) {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            Transport t;
            core.process (out, 256, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, t);
        }
    };
    auto on = [] (int note) { return MidiEvent::noteOn (1, note, 100); };
    auto off = [] (int note) { return MidiEvent::noteOff (1, note); };
    const auto& st = core.part (0).spaceStage();
    auto restarted = [&] (std::vector<MidiEvent> midi) { // runs a quarter turn, plays the events, true if the motion jumped back
        run (94);
        const double before = st.uiPhase();
        run (1, midi);
        run (4);
        return st.uiPhase() < before - 0.1;
    };
    REQUIRE (reg.set ("p1.space.retrig", (float) SpaceEffect::RetrigFirst));
    run (2);
    CHECK (restarted ({ on (60) }));        // first note after silence
    CHECK_FALSE (restarted ({ on (64) })); // a second key while the first is down
    CHECK (restarted ({ off (60), off (64), on (67) })); // all released first: restarts
    run (1, { off (67) });
    REQUIRE (reg.set ("p1.space.retrig", (float) SpaceEffect::RetrigNote));
    CHECK (restarted ({ on (60) }));
    CHECK (restarted ({ on (64) })); // every note (poly)
    run (1, { off (60), off (64) });
    REQUIRE (reg.set ("p1.voices", 1.0f));
    CHECK (restarted ({ on (60) }));
    CHECK_FALSE (restarted ({ on (62) })); // mono legato: no restart
    run (1, { off (60), off (62) });
    REQUIRE (reg.set ("p1.space.retrig", (float) SpaceEffect::RetrigFree));
    CHECK_FALSE (restarted ({ on (60) }));
    run (1, { off (60) });
}

TEST_CASE ("p77 space: retrigger is click-free (quick glide, no step) on fast paths")
{
    for (int path : { space::PathCircle, space::PathPingPongLR, space::PathCorners, space::PathFigure8, space::PathBounce })
        for (float room : { 0.0f, 0.4f })
        {
            CAPTURE (path);
            CAPTURE (room);
            Fx fx;
            fx.e.setParam (SpaceEffect::Path, (float) path);
            fx.e.setParam (SpaceEffect::PathLength, 3.0f); // 1/2
            fx.e.setParam (SpaceEffect::PathSize, 1.0f);
            fx.e.setParam (SpaceEffect::Room, room);
            fx.e.setParam (SpaceEffect::Glide, 0.005f);
            fx.e.setRetrigger (SpaceEffect::RetrigNote);
            auto s = sine (300.0f, 96000);
            for (size_t pos = 0; pos < s.size(); pos += 128)
            {
                if (pos % 6144 == 0) fx.e.retrigger(); // a note every 128 ms
                fx.e.process (s.l.data() + pos, s.r.data() + pos, 128);
            }
            CHECK (allFinite (s.l));
            CHECK (allFinite (s.r));
            if (room > 0.0f) // the criteria of the 1.0 motion test: curvature with the room ...
            {
                CHECK (clickRatio (s.l, 300.0f / 0.88f, 4800) < 1.6);
                CHECK (clickRatio (s.r, 300.0f / 0.88f, 4800) < 1.6);
                continue;
            }
            // ... and without it the short-term level per ear never jumps (10 ms windows, neighbours within 2 dB)
            for (const auto* a : { &s.l, &s.r })
            {
                auto power = [a] (size_t from) {
                    double e = 0.0;
                    for (size_t i = from; i < from + 480; ++i) e += (double) (*a)[i] * (*a)[i];
                    return 10.0 * std::log10 (e + 1.0e-12);
                };
                for (size_t w = 4800; w + 960 < a->size(); w += 480)
                    CHECK (std::abs (power (w) - power (w + 480)) < 2.0);
            }
        }
}

TEST_CASE ("p77 space: amount - 1 is unchanged, 0 is the plain stereo input, sweeps are click-free and level-steady")
{
    auto render = [] (float amount, bool sweep, Stereo in) {
        Fx fx;
        fx.e.setParam (SpaceEffect::Path, (float) space::PathCircle);
        fx.e.setParam (SpaceEffect::PathLength, 4.0f);
        fx.e.setParam (SpaceEffect::Azimuth, 60.0f);
        fx.e.setParam (SpaceEffect::Distance, 3.0f);
        fx.e.setAmount (amount);
        for (size_t pos = 0; pos < in.size(); pos += 256)
        {
            if (sweep) fx.e.setAmount (0.5f + 0.5f * std::cos (twoPi * (float) pos / 96000.0f));
            fx.e.process (in.l.data() + pos, in.r.data() + pos, (int) std::min<size_t> (256, in.size() - pos));
        }
        return in;
    };
    const auto src = noise (96000, 7);
    // default (amount 1) is bit-identical to a stage that never heard of the amount
    {
        Fx a;
        a.e.setParam (SpaceEffect::Path, (float) space::PathCircle);
        a.e.setParam (SpaceEffect::PathLength, 4.0f);
        a.e.setParam (SpaceEffect::Azimuth, 60.0f);
        a.e.setParam (SpaceEffect::Distance, 3.0f);
        auto ref = src;
        for (size_t pos = 0; pos < ref.size(); pos += 256) a.e.process (ref.l.data() + pos, ref.r.data() + pos, 256);
        const auto full = render (1.0f, false, src);
        CHECK (full.l == ref.l);
        CHECK (full.r == ref.r);
    }
    // amount 0: the input itself (delayed, at the base distance level), the stereo image kept
    {
        const auto zero = render (0.0f, false, src);
        const float g = std::pow (space::refDistance / 3.0f, 0.7f); // default rolloff
        int bestLag = 0;
        double best = -1.0;
        for (int lag = 0; lag < 200; ++lag)
        {
            double c = 0.0;
            for (size_t i = 4800; i < 20000; ++i) c += (double) zero.l[i] * src.l[i - (size_t) lag];
            if (c > best) { best = c; bestLag = lag; }
        }
        double err = 0.0, ref = 0.0;
        for (size_t i = 4800; i < zero.size(); ++i)
        {
            const double el = zero.l[i] - g * src.l[i - (size_t) bestLag], er = zero.r[i] - g * src.r[i - (size_t) bestLag];
            err += el * el + er * er;
            ref += g * g * ((double) src.l[i] * src.l[i] + (double) src.r[i] * src.r[i]);
        }
        CHECK (10.0 * std::log10 (err / ref + 1.0e-30) < -30.0); // within -30 dB of the plain (fractionally delayed) input
    }
    // the level hardly moves with the amount
    const double l1 = rmsDb (render (1.0f, false, src), 4800), l5 = rmsDb (render (0.5f, false, src), 4800),
                 l0 = rmsDb (render (0.0f, false, src), 4800);
    CHECK (std::abs (l1 - l0) < 3.0);
    CHECK (std::abs (l5 - l0) < 3.0);
    CHECK (std::abs (l5 - l1) < 3.0);
    // a sweep (automation steps per block) on a sine: no clicks
    const auto sw = render (1.0f, true, sine (300.0f, 96000));
    CHECK (allFinite (sw.l));
    CHECK (clickRatio (sw.l, 300.0f / 0.88f, 4800) < 1.6);
    CHECK (clickRatio (sw.r, 300.0f / 0.88f, 4800) < 1.6);
}

TEST_CASE ("p77 space: the new values round-trip through a part preset and the session state")
{
    Core core;
    REQUIRE (core.params().set ("p2.space.retrig", 2.0f));
    REQUIRE (core.params().set ("p2.space.amount", 0.35f));
    const auto pr = preset::capturePart (core.params(), 1, preset::Meta {});
    Core other;
    preset::applyPart (pr, other.params(), 1);
    CHECK (other.params().get ("p2.space.retrig") == 2.0f);
    CHECK (other.params().get ("p2.space.amount") == doctest::Approx (0.35f));
}
