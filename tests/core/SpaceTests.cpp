// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// 1.0: the 3D spatialiser (SpaceEffect): per-part stage, rack type, motion paths, binaural cues, mono safety.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/InsertSlot.h"
#include "fx/Space3D.h"
#include "fx/SpaceMode.h"
#include "model/StateSerializer.h"
#include "preset/Preset.h"
#include "mod/ModMatrix.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <cmath>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr float twoPi = 6.28318530718f;

struct Fx
{
    SpaceEffect e;
    explicit Fx (int monitor = SpaceEffect::Headphones)
    {
        e.prepare (sampleRate, 512);
        const auto d = e.getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
        e.setMonitorMode (monitor);
        e.setTempo (120.0);
    }
    Fx& set (SpaceEffect::Param p, float v)
    {
        e.setParam (p, v);
        return *this;
    }
    void run (Stereo& s, int block = 256)
    {
        for (size_t pos = 0; pos < s.size(); pos += (size_t) block)
            e.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> ((size_t) block, s.size() - pos));
    }
};

Stereo sine (float hz, size_t n, float amp = 0.3f)
{
    Stereo s (n);
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = amp * std::sin (twoPi * hz * (float) i / (float) sampleRate);
    return s;
}

Stereo noise (size_t n, uint32_t seed, bool stereo = false)
{
    Stereo s (n);
    dsp::Random a (seed), b (seed * 7 + 3);
    for (size_t i = 0; i < n; ++i)
    {
        s.l[i] = 0.3f * a.nextBipolar();
        s.r[i] = stereo ? 0.3f * b.nextBipolar() : s.l[i];
    }
    return s;
}

/** Largest second difference relative to what a clean sine of the output's peak would have. */
double clickRatio (const std::vector<float>& v, float hz, size_t from)
{
    double d2 = 0.0;
    for (size_t i = from + 1; i + 1 < v.size(); ++i) d2 = std::max (d2, (double) std::abs (v[i + 1] - 2.0f * v[i] + v[i - 1]));
    const double w = twoPi * hz / sampleRate;
    return d2 / ((double) peak (v, from) * w * w);
}

int peakIndex (const std::vector<float>& v)
{
    int k = 0;
    for (int i = 0; i < (int) v.size(); ++i)
        if (std::abs (v[(size_t) i]) > std::abs (v[(size_t) k])) k = i;
    return k;
}

double energy (const std::vector<float>& v, size_t from = 0)
{
    double s = 0.0;
    for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return s;
}

/** Energy of the first difference (a crude high-frequency weighting). */
double hfEnergy (const std::vector<float>& v, size_t from = 1)
{
    double s = 0.0;
    for (size_t i = std::max<size_t> (1, from); i < v.size(); ++i) s += (double) (v[i] - v[i - 1]) * (v[i] - v[i - 1]);
    return s;
}

/** Mean power of `bins` Goertzel bins evenly spaced from lo to hi (Hz): a band level that does not depend on how one
    pure tone happens to fall on a comb or notch. */
double bandEnergy (const std::vector<float>& v, size_t from, double lo, double hi, int bins = 25)
{
    double acc = 0.0;
    for (int k = 0; k < bins; ++k)
    {
        const double f = lo + (hi - lo) * k / (bins - 1), c = 2.0 * std::cos (2.0 * 3.14159265358979 * f / sampleRate);
        double s1 = 0.0, s2 = 0.0;
        for (size_t i = from; i < v.size(); ++i)
        {
            const double s0 = v[i] + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        acc += s1 * s1 + s2 * s2 - c * s1 * s2;
    }
    return acc / bins;
}

double db (double ratio) { return 10.0 * std::log10 (std::max (1.0e-30, ratio)); }

struct CoreRig
{
    Core core;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    Stereo rec;
    CoreRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        core.params().set ("mix.master.limit", 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }
    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            Transport t;
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, t);
            rec.l.insert (rec.l.end(), l[0].begin(), l[0].end());
            rec.r.insert (rec.r.end(), r[0].begin(), r[0].end());
        }
    }
};
} // namespace

TEST_CASE ("1.0 space: parameters - per-part stage, monitoring, rack type in every slot, reserved matrix targets")
{
    Core core;
    const auto& reg = core.params();
    for (int p = 1; p <= Core::numParts; ++p)
    {
        const auto n = "p" + std::to_string (p) + ".";
        for (const char* id : { "space.on", "space.azimuth", "space.elevation", "space.distance", "space.pathrate", "space.pathphase",
                                "space.path", "space.pathlen", "space.pathsize", "space.spread", "space.room", "space.roomsize",
                                "space.doppler", "space.air", "space.rolloff", "space.lowkeep", "space.glide" })
            CHECK (reg.indexOf (n + id) >= 0);
        CHECK (reg.indexOf (n + "fx.space3d.azimuth") >= 0);
        CHECK (reg.indexOf (n + "fx6.space3d.glide") >= 0);
        CHECK (reg.get (n + "space.on") == 0.0f); // off by default: every older preset sounds as before
    }
    CHECK (reg.indexOf ("mfx4.space3d.path") >= 0);
    REQUIRE (reg.indexOf ("space.out") >= 0);
    CHECK (reg.descriptor (reg.indexOf ("space.out")).choices.size() == 2);
    CHECK (reg.indexOf ("space.monocheck") >= 0);
    CHECK (InsertSlot::firstResoType == InsertSlot::firstSpaceType + 1); // p19: the resonator follows
    CHECK (InsertSlot::typeNamesEn()[(size_t) InsertSlot::Space3D] == "Space 3D");
    CHECK (InsertSlot::typeNamesDe()[(size_t) InsertSlot::Space3D] == "3D-Raum");
    CHECK (std::string (InsertSlot::typeInfo (InsertSlot::Space3D).id) == "space3d");
    // the per-part block and the rack block share local IDs; the first five match the 0.7 reserved targets
    const auto d = SpaceEffect().getParamDescriptors();
    REQUIRE (d.size() == (size_t) SpaceEffect::numParams);
    CHECK (d[0].id == "azimuth");
    CHECK (d[1].id == "elevation");
    CHECK (d[2].id == "distance");
    CHECK (d[3].id == "pathrate");
    CHECK (d[4].id == "pathphase");
    // the whole 1.0 block comes after every older parameter
    CHECK (reg.indexOf ("p1.space.on") > reg.indexOf ("p4.monobass"));
    CHECK (reg.indexOf ("p1.space.on") > reg.indexOf ("tape.tx.link")); // appended after the tape transport IDs (p11)
    // the 1.0 block ends with the master racks' Space 3D blocks; only the punch pass IDs (ducking, chorus ensemble) follow
    CHECK (reg.indexOf ("mfx4.space3d.glide") + 1 == reg.indexOf ("mix.duck.src"));
    // p14: the performance wheels (perf.bend, perf.mod) come right after the punch pass IDs (later blocks follow them)
    CHECK (reg.indexOf ("perf.bend") == reg.indexOf ("mfx4.chorus.lowcut") + 1);
    CHECK (reg.indexOf ("perf.mod") == reg.indexOf ("perf.bend") + 1);
    // p16: the morph space block follows the later blocks
    CHECK (reg.indexOf ("perf.mod") < reg.indexOf ("p1.morph.on"));
}

TEST_CASE ("1.0 space: motion paths and orientation (pure functions)")
{
    using namespace space;
    auto near = [] (Vec3 a, Vec3 b) { return std::abs (a.x - b.x) < 1e-4f && std::abs (a.y - b.y) < 1e-4f && std::abs (a.z - b.z) < 1e-4f; };
    const Vec3 right = orient ({ 0.0f, 1.0f, 0.0f }, 90.0f, 0.0f);
    CHECK (near (right, { 1.0f, 0.0f, 0.0f }));
    CHECK (near (orient ({ 0.0f, 1.0f, 0.0f }, 0.0f, 90.0f), { 0.0f, 0.0f, 1.0f }));
    CHECK (azimuthOf ({ -1.0f, 0.0f, 0.0f }) == doctest::Approx (-90.0f));
    CHECK (elevationOf ({ 0.0f, 1.0f, 1.0f }) == doctest::Approx (45.0f));
    for (int p = 0; p < numPaths; ++p)
        for (float size : { 0.0f, 0.5f, 1.0f })
        {
            CAPTURE (p);
            CAPTURE (size);
            if (pathIsPeriodic (p)) CHECK (near (pathPoint (p, 0.3, size, 5), pathPoint (p, 1.3, size, 5))); // periodic
            for (double ph = 0.0; ph < 2.0; ph += 0.01) // finite, never inside the head
            {
                const auto v = pathPoint (p, ph, size, 5);
                CHECK (std::isfinite (v.x));
                CHECK (lengthOf (v) > 0.25f);
            }
        }
    // circle: a full turn, level at size 0
    CHECK (near (pathPoint (PathCircle, 0.25, 0.0f, 1), { 1.0f, 0.0f, 0.0f }));
    CHECK (near (pathPoint (PathCircle, 0.5, 0.0f, 1), { 0.0f, -1.0f, 0.0f }));
    // fly-by: closest (straight ahead) halfway, far to the sides at the ends
    CHECK (near (pathPoint (PathFlyBy, 0.25, 0.5f, 1), { 0.0f, 1.0f, 0.0f }));
    CHECK (lengthOf (pathPoint (PathFlyBy, 0.0, 1.0f, 1)) > 10.0f);
    // pendulum: overhead at phase 0, in front / behind at the extremes
    CHECK (near (pathPoint (PathPendulum, 0.0, 1.0f, 1), { 0.0f, 0.0f, 1.0f }));
    CHECK (pathPoint (PathPendulum, 0.25, 1.0f, 1).y == doctest::Approx (1.0f).epsilon (0.001));
    CHECK (pathPoint (PathPendulum, 0.75, 1.0f, 1).y == doctest::Approx (-1.0f).epsilon (0.001));
    // spiral: rises
    CHECK (elevationOf (pathPoint (PathSpiral, 0.9, 0.5f, 1)) > elevationOf (pathPoint (PathSpiral, 0.1, 0.5f, 1)) + 60.0f);
    // random orbit: deterministic per seed, different seeds differ, continuous
    CHECK (near (pathPoint (PathRandom, 3.7, 0.8f, 9), pathPoint (PathRandom, 3.7, 0.8f, 9)));
    CHECK_FALSE (near (pathPoint (PathRandom, 3.7, 0.8f, 9), pathPoint (PathRandom, 3.7, 0.8f, 10)));
    for (double ph = 0.0; ph < 4.0; ph += 0.001)
    {
        const auto a = pathPoint (PathRandom, ph, 1.0f, 3), b = pathPoint (PathRandom, ph + 0.001, 1.0f, 3);
        CHECK (lengthOf ({ a.x - b.x, a.y - b.y, a.z - b.z }) < 0.05f);
    }
}

TEST_CASE ("1.0 space: interaural time difference has the right sign and size; speakers have none")
{
    for (float az : { 90.0f, -90.0f, 0.0f })
    {
        CAPTURE (az);
        Fx fx;
        fx.set (SpaceEffect::Azimuth, az).set (SpaceEffect::Room, 0.0f).set (SpaceEffect::Spread, 0.0f).set (SpaceEffect::Doppler, 0.0f);
        Stereo s (4096);
        s.l[1000] = s.r[1000] = 1.0f;
        fx.run (s);
        const int lag = peakIndex (s.l) - peakIndex (s.r); // > 0: the left ear hears it later
        const int expect = (int) std::lround (space::itdSeconds ({ 1.0f, 0.0f, 0.0f }) * sampleRate); // ~31 samples
        if (az > 0.0f) { CHECK (lag >= expect - 3); CHECK (lag <= expect + 3); CHECK (peak (s.r) > 2.0f * peak (s.l)); }
        else if (az < 0.0f) { CHECK (-lag >= expect - 3); CHECK (-lag <= expect + 3); CHECK (peak (s.l) > 2.0f * peak (s.r)); }
        else { CHECK (lag == 0); CHECK (peak (s.l) == doctest::Approx (peak (s.r))); }
    }
    Fx spk (SpaceEffect::Speakers);
    spk.set (SpaceEffect::Azimuth, 60.0f).set (SpaceEffect::Room, 0.0f).set (SpaceEffect::Spread, 0.0f);
    Stereo s (4096);
    s.l[1000] = s.r[1000] = 1.0f;
    spk.run (s);
    CHECK (peakIndex (s.l) == peakIndex (s.r));
    CHECK (peak (s.r) > peak (s.l));
}

TEST_CASE ("1.0 space: moving sources stay click-free (orbit, jumps, motion, monitoring switch)")
{
    for (int monitor : { SpaceEffect::Headphones, SpaceEffect::Speakers })
        for (int path : { space::PathCircle, space::PathFigure8, space::PathRandom, space::PathPendulum, space::PathSpiral,
                          space::PathPingPong, space::PathSwarm, space::PathOverhead, space::PathWeave,
                          // p73 (the Doppler paths Fly-Over and Loop Orbit are checked at one bar, like the fly-by / walk)
                          space::PathPingPongLR, space::PathCorners, space::PathBreathe, space::PathHelix, space::PathBounce,
                          space::PathWander })
            for (float room : { 0.0f, 0.5f })
            {
                CAPTURE (monitor);
                CAPTURE (path);
                CAPTURE (room);
                Fx fx (monitor);
                fx.set (SpaceEffect::Path, (float) path).set (SpaceEffect::PathLength, 2.0f).set (SpaceEffect::PathSize, 1.0f)
                    .set (SpaceEffect::Doppler, 1.0f).set (SpaceEffect::Room, room).set (SpaceEffect::Glide, 0.005f);
                auto s = sine (300.0f, 96000);
                fx.run (s, 173);
                CHECK (allFinite (s.l));
                if (room > 0.0f)
                {
                    CHECK (clickRatio (s.l, 300.0f, 4800) < 1.6);
                    CHECK (clickRatio (s.r, 300.0f, 4800) < 1.6);
                }
                // The short-term level of the direct sound never jumps: 10 ms windows, neighbours within 2 dB - per ear on
                // headphones, on the summed power of both speakers in speaker mode. Checked without the room: a pure tone
                // beats against its moving reflections (comb notches sweeping through it - real room acoustics, not a
                // discontinuity; the click check above covers the room). One speaker alone is not checked because
                // constant-power panning to the far side silences it by design (cos -> 0 is a dB cliff, not a jump).
                if (room > 0.0f) continue;
                const bool sum = monitor == SpaceEffect::Speakers;
                for (int ch = 0; ch < (sum ? 1 : 2); ++ch)
                {
                    auto power = [&s, sum, ch] (size_t from) {
                        const auto& a = ch == 0 ? s.l : s.r;
                        return energy ({ a.begin() + (long) from, a.begin() + (long) from + 480 })
                               + (sum ? energy ({ s.r.begin() + (long) from, s.r.begin() + (long) from + 480 }) : 0.0);
                    };
                    for (size_t w = 4800; w + 960 < s.size(); w += 480)
                        CHECK (std::abs (db (power (w) + 1e-12) - db (power (w + 480) + 1e-12)) < 2.0);
                }
            }
    // parameter jumps (automation without smoothing in the host) and the monitoring switch
    Fx fx;
    auto s = sine (300.0f, 96000);
    for (size_t pos = 0; pos < s.size(); pos += 256)
    {
        const int k = (int) (pos / 4096);
        fx.set (SpaceEffect::Azimuth, k % 2 ? 180.0f : (k % 3 ? -90.0f : 90.0f));
        fx.set (SpaceEffect::Elevation, k % 4 == 1 ? 80.0f : -30.0f);
        fx.set (SpaceEffect::Distance, k % 5 == 2 ? 20.0f : 0.5f);
        fx.e.setMonitorMode (k % 6 == 3 ? SpaceEffect::Speakers : SpaceEffect::Headphones);
        fx.e.process (s.l.data() + pos, s.r.data() + pos, 256);
    }
    // The distance jumps (0.5 <-> 20 m) make the source fly at the Doppler limit (0.12 c), so the tone legitimately rises
    // up to 1 / 0.88 of its pitch while it approaches: the curvature is measured against that highest legal frequency.
    CHECK (clickRatio (s.l, 300.0f / 0.88f, 4800) < 1.6);
    CHECK (clickRatio (s.r, 300.0f / 0.88f, 4800) < 1.6);
}

TEST_CASE ("1.0 space: Doppler shifts the pitch of a fly-by, within bounds")
{
    Fx fx;
    fx.set (SpaceEffect::Path, (float) space::PathFlyBy).set (SpaceEffect::PathLength, 3.0f).set (SpaceEffect::PathSize, 1.0f)
        .set (SpaceEffect::Distance, 2.0f).set (SpaceEffect::Doppler, 1.0f).set (SpaceEffect::Room, 0.0f).set (SpaceEffect::Spread, 0.0f);
    auto s = sine (1000.0f, 48000 * 3);
    fx.run (s);
    double lo = 1.0e9, hi = 0.0;
    for (size_t w = 9600; w + 960 < s.size(); w += 960)
    {
        Stereo m (960);
        for (size_t i = 0; i < 960; ++i) m.l[i] = s.l[w + i] + s.r[w + i];
        if (rms (m.l) < 1.0e-4f) continue;
        const double f = measureFrequency (m.l, sampleRate, 0, 960);
        lo = std::min (lo, f);
        hi = std::max (hi, f);
    }
    CHECK (hi > 1030.0); // audible: approaching sounds higher ...
    CHECK (lo < 970.0);  // ... leaving lower
    CHECK (hi < 1000.0 / 0.87);
    CHECK (lo > 1000.0 / 1.13);
    // Doppler 0: no pitch change
    Fx still;
    still.set (SpaceEffect::Path, (float) space::PathFlyBy).set (SpaceEffect::PathLength, 3.0f).set (SpaceEffect::Doppler, 0.0f)
        .set (SpaceEffect::Room, 0.0f).set (SpaceEffect::Spread, 0.0f);
    auto t = sine (1000.0f, 48000 * 2);
    still.run (t);
    CHECK (measureFrequency (t.r, sampleRate, 48000, 96000) == doctest::Approx (1000.0).epsilon (0.002));
}

TEST_CASE ("1.0 space: mono compatibility - low end mono, speaker mode folds without loss, front is mono-identical")
{
    // below "Mono below" the output is mono even for a source at the side
    Fx side;
    side.set (SpaceEffect::Azimuth, 90.0f).set (SpaceEffect::Room, 0.0f);
    auto lowSine = sine (50.0f, 48000);
    side.run (lowSine);
    double corr = 0.0, el = 0.0, er = 0.0;
    for (size_t i = 24000; i < lowSine.size(); ++i) { corr += lowSine.l[i] * lowSine.r[i]; el += lowSine.l[i] * lowSine.l[i]; er += lowSine.r[i] * lowSine.r[i]; }
    CHECK (corr / std::sqrt (el * er) > 0.99);

    // speakers: the mono fold loses at most 3 dB anywhere on the circle (amplitude panning, no time differences)
    for (float az : { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, -120.0f })
    {
        CAPTURE (az);
        Fx fx (SpaceEffect::Speakers);
        fx.set (SpaceEffect::Azimuth, az).set (SpaceEffect::Room, 0.3f);
        auto s = noise (48000, 3);
        fx.run (s);
        Stereo m (s.size());
        for (size_t i = 0; i < s.size(); ++i) m.l[i] = 0.5f * (s.l[i] + s.r[i]);
        CHECK (db (2.0 * energy (m.l, 4800) / (0.5 * (energy (s.l, 4800) + energy (s.r, 4800)) * 2.0)) > -3.2);
    }
    // headphones, straight ahead: both ears identical -> mono fold changes nothing
    Fx front;
    front.set (SpaceEffect::Room, 0.0f);
    auto s = noise (48000, 4);
    front.run (s);
    for (size_t i = 0; i < s.size(); ++i) REQUIRE (s.l[i] == doctest::Approx (s.r[i]).epsilon (1e-4));
}

TEST_CASE ("1.0 space: binaural cues - front / back, elevation notch, distance level and air")
{
    auto render = [] (float az, float el, float dist, float hz = 0.0f) {
        Fx fx;
        fx.set (SpaceEffect::Azimuth, az).set (SpaceEffect::Elevation, el).set (SpaceEffect::Distance, dist).set (SpaceEffect::Room, 0.0f);
        auto s = hz > 0.0f ? sine (hz, 48000) : noise (48000, 21);
        fx.run (s);
        return s;
    };
    // behind is darker than in front, the low / mid level about the same
    const auto f = render (0.0f, 0.0f, 1.5f), b = render (180.0f, 0.0f, 1.5f);
    CHECK (db (hfEnergy (b.l, 4800) / hfEnergy (f.l, 4800)) < -3.0);
    const auto f300 = render (0.0f, 0.0f, 1.5f, 300.0f), b300 = render (180.0f, 0.0f, 1.5f, 300.0f);
    CHECK (std::abs (db (energy (b300.l, 4800) / energy (f300.l, 4800))) < 3.0);
    // the pinna notch moves up with elevation: 7.5 kHz is cut at ear height, not at 45 degrees up
    const auto n0 = render (0.0f, 0.0f, 1.5f, 7560.0f), n45 = render (0.0f, 45.0f, 1.5f, 7560.0f);
    CHECK (db (energy (n0.l, 4800) / energy (n45.l, 4800)) < -4.0);
    // above: the 8.5 kHz directional band is lifted (noise, 7-10 kHz band level). Measured on the band, not on one tone at
    // 12 kHz: up there the pinna notch rising with elevation and the shoulder reflection make the level of a single tone
    // swing either way, as in a real ear - the "above" cue is the band around 8 kHz.
    const auto a0 = render (0.0f, -20.0f, 1.5f), a70 = render (0.0f, 70.0f, 1.5f);
    CHECK (db (bandEnergy (a70.l, 4800, 7000.0, 10000.0) / bandEnergy (a0.l, 4800, 7000.0, 10000.0)) > 2.0);
    // distance: quieter and darker further away; the default position plays at the input level (+-1.5 dB, 300 Hz)
    const auto near = render (0.0f, 0.0f, 1.5f, 300.0f), far = render (0.0f, 0.0f, 12.0f, 300.0f);
    CHECK (db (energy (far.l, 9600) / energy (near.l, 9600)) < -6.0);
    const auto in = sine (300.0f, 48000);
    CHECK (std::abs (db (energy (near.l, 4800) / energy (in.l, 4800))) < 1.5);
    // air: 30 m away the 8-14 kHz band falls at least 3 dB more than 1-3 kHz (ISO 9613-1, 20 C / 50 %: about 3-6 dB over
    // 28.5 m there). Measured in bands: a first-difference weighting would be ruled by 15-24 kHz, where the fractional
    // delay's interpolation (its loss depends on the delay's fraction, i.e. on the distance) outweighs the air.
    const auto farN = render (0.0f, 0.0f, 30.0f), nearN = render (0.0f, 0.0f, 1.5f);
    const double tilt = db (bandEnergy (farN.l, 9600, 8000.0, 14000.0) / bandEnergy (nearN.l, 9600, 8000.0, 14000.0))
                        - db (bandEnergy (farN.l, 9600, 1000.0, 3000.0) / bandEnergy (nearN.l, 9600, 1000.0, 3000.0));
    CHECK (tilt < -3.0);
}

TEST_CASE ("1.0 space: tempo-synced motion is locked to the song position")
{
    auto positionAt = [] (double ppq) {
        Fx fx;
        fx.set (SpaceEffect::Path, (float) space::PathCircle).set (SpaceEffect::PathLength, 4.0f).set (SpaceEffect::PathSize, 0.0f);
        fx.e.setTransportPosition (ppq, true);
        Stereo s (16);
        fx.run (s, 16);
        return fx.e.currentPosition();
    };
    const auto a = positionAt (0.0), b = positionAt (4.0), c = positionAt (2.0), d = positionAt (1.0);
    CHECK (a.x == doctest::Approx (b.x).epsilon (0.01));
    CHECK (a.y == doctest::Approx (b.y).epsilon (0.01));
    CHECK (c.y == doctest::Approx (-a.y).epsilon (0.02)); // half a bar: behind
    CHECK (d.x > 1.0f);                                  // a quarter: on the right
}

TEST_CASE ("1.0 space: no NaN, no denormals, tails decay - random extreme settings")
{
    dsp::Random rng (77);
    for (int monitor : { SpaceEffect::Headphones, SpaceEffect::Speakers })
    {
        Fx fx (monitor);
        const auto d = fx.e.getParamDescriptors();
        Stereo s = noise (48000 * 2, 5, true);
        for (size_t i = 0; i < s.size(); ++i) { s.l[i] *= 3.0f; s.r[i] *= 3.0f; }
        Stereo silence (48000 * 4);
        for (size_t pos = 0; pos < s.size(); pos += 128)
        {
            for (size_t k = 0; k < d.size(); ++k)
            {
                const float x = 0.5f + 0.5f * rng.nextBipolar();
                fx.e.setParam ((int) k, rng.nextBipolar() > 0.6f ? (x > 0.5f ? d[k].maxValue : d[k].minValue) : d[k].minValue + x * (d[k].maxValue - d[k].minValue));
            }
            fx.e.setTransportPosition ((double) pos / 24000.0, true);
            fx.e.process (s.l.data() + pos, s.r.data() + pos, 128);
        }
        CHECK (allFinite (s.l));
        CHECK (allFinite (s.r));
        fx.set (SpaceEffect::Room, 1.0f).set (SpaceEffect::RoomSize, 1.0f);
        fx.run (silence);
        CHECK (allFinite (silence.l));
        CHECK (noDenormals (silence.l));
        CHECK (noDenormals (silence.r));
        CHECK (peak (silence.l, 48000 * 3) < 1.0e-4f);
        CHECK (peak (silence.r, 48000 * 3) < 1.0e-4f);
    }
}

TEST_CASE ("1.0 space: the part stage - off is an exact bypass, on moves the part, monitoring and mono check")
{
    auto render = [] (bool on, bool touchParams, int monitor = 0, bool monoCheck = false) {
        CoreRig rig;
        rig.set ("p1.engine", (float) EngineType::Analog);
        if (touchParams) { rig.set ("p1.space.azimuth", 90.0f); rig.set ("p1.space.path", (float) space::PathCircle); }
        rig.set ("p1.space.on", on ? 1.0f : 0.0f);
        rig.set ("space.out", (float) monitor);
        rig.set ("space.monocheck", monoCheck ? 1.0f : 0.0f);
        rig.run (120, { MidiEvent::noteOn (1, 52, 110) });
        return rig.rec;
    };
    const auto plain = render (false, false), offTouched = render (false, true);
    for (size_t i = 0; i < plain.size(); ++i) REQUIRE (plain.l[i] == offTouched.l[i]);
    const auto on = render (true, true);
    CHECK (allFinite (on.l));
    CHECK (rms (on.l) > 0.0f);
    double diff = 0.0;
    for (size_t i = 0; i < on.size(); ++i) diff += std::abs (on.l[i] - on.r[i]);
    CHECK (diff > 1.0); // moved off the centre
    const auto mono = render (true, true, 0, true);
    for (size_t i = 0; i < mono.size(); ++i) REQUIRE (mono.l[i] == mono.r[i]);

    // switching on and off while a note holds does not click
    CoreRig rig;
    rig.set ("p1.engine", (float) EngineType::Analog);
    rig.set ("p1.analog.cutoff", 400.0f);
    rig.set ("p1.space.azimuth", -120.0f);
    rig.run (20, { MidiEvent::noteOn (1, 40, 100) });
    for (int k = 0; k < 6; ++k)
    {
        rig.set ("p1.space.on", k % 2 ? 0.0f : 1.0f);
        rig.run (20);
    }
    double jump = 0.0, typical = 0.0;
    for (size_t i = 2400; i + 1 < rig.rec.size(); ++i)
    {
        jump = std::max (jump, (double) std::abs (rig.rec.l[i + 1] - 2.0f * rig.rec.l[i] + rig.rec.l[i - 1]));
        typical += std::abs (rig.rec.l[i + 1] - 2.0f * rig.rec.l[i] + rig.rec.l[i - 1]);
    }
    typical /= (double) rig.rec.size();
    CHECK (jump < 40.0 * typical + 1.0e-3);
}

TEST_CASE ("1.0 space: the matrix moves the stage (macro -> azimuth); the stage reports its position for the display")
{
    CoreRig rig;
    rig.set ("p1.engine", (float) EngineType::Analog);
    rig.set ("p1.space.on", 1.0f);
    rig.set ("p1.space.glide", 0.005f);
    rig.set ("p1.mod.s1.src", (float) mod::SrcMacro1);
    rig.set ("p1.mod.s1.dst", (float) mod::DstAzimuth);
    rig.set ("p1.mod.s1.amt", 0.25f); // a quarter of the range = 90 degrees
    rig.set ("p1.macro1", 1.0f);
    rig.run (40, { MidiEvent::noteOn (1, 60, 100) });
    const auto p = rig.core.part (0).spaceStage().uiPosition();
    CHECK (space::azimuthOf (p) == doctest::Approx (90.0f).epsilon (0.05));
    CHECK (rig.core.part (0).spaceRunning());
}

TEST_CASE ("1.0 space: CPU - four part stages and eight rack instances")
{
    std::vector<std::unique_ptr<SpaceEffect>> fx;
    for (int i = 0; i < 12; ++i)
    {
        auto e = std::make_unique<SpaceEffect>();
        e->prepare (48000.0, 256);
        const auto d = e->getParamDescriptors();
        for (size_t k = 0; k < d.size(); ++k) e->setParam ((int) k, d[k].defaultValue);
        e->setParam (SpaceEffect::Path, (float) (1 + i % (space::numPaths - 1))); // every path incl. the p30 ones
        e->setParam (SpaceEffect::Room, 0.5f);
        fx.push_back (std::move (e));
    }
    Stereo s = noise (256, 9, true);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < 48000 / 256 * 2; ++b)
        for (auto& e : fx)
        {
            Stereo x = s;
            e->process (x.l.data(), x.r.data(), 256);
        }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    MESSAGE ("12 Space 3D instances, 2 s of audio at 48 kHz: " << secs << " s (" << secs * 50.0 << " % of one core)");
#ifdef NDEBUG
    CHECK (secs < 1.0); // generous: shared CI machines; typical well below 0.4 s
#endif
}

// ------------------------------------------------------------------------------------------------ p30: more motion paths
namespace
{
/** A sine with its phase in double precision: the float phase of sine() rounds by up to 3e-4 rad after a few seconds,
    a jitter whose second difference the near ear's head-shadow treble lift (whisper: 0.4 m) turns into false clicks. */
Stereo precSine (double hz, size_t n, float amp = 0.3f)
{
    Stereo s (n);
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = amp * (float) std::sin (2.0 * 3.14159265358979323846 * hz * (double) i / sampleRate);
    return s;
}
} // namespace

TEST_CASE ("p30 space: the motion path choice is append-only - old indices keep their meaning")
{
    Fx fx;
    const auto d = fx.e.getParamDescriptors();
    const auto& path = d[(size_t) SpaceEffect::Path];
    REQUIRE (path.choices.size() == (size_t) space::numPaths);
    REQUIRE (path.choicesDe.size() == (size_t) space::numPaths);
    const char* before[] = { "Off", "Circle", "Figure-8", "Fly-By", "Random Orbit", "Pendulum", "Spiral Up" };
    for (int i = 0; i < 7; ++i) CHECK (path.choices[(size_t) i] == before[i]);
    CHECK (path.choices[(size_t) space::PathPingPong] == "Ping-Pong F/B");
    CHECK (path.choices[(size_t) space::PathWeave] == "Weave");
    CHECK (space::PathSpiral == 6);
    CHECK (space::PathPingPong == 7);
    CHECK (path.defaultValue == 0.0f);
}

TEST_CASE ("p30 space: shapes of the new paths (pure functions)")
{
    using namespace space;
    auto near = [] (Vec3 a, Vec3 b) { return std::abs (a.x - b.x) < 1e-4f && std::abs (a.y - b.y) < 1e-4f && std::abs (a.z - b.z) < 1e-4f; };
    // ping-pong: front at the cycle start, behind after half a cycle; snappy (size 1) arrives early and holds
    CHECK (pathPoint (PathPingPong, 0.0, 1.0f, 1).y == doctest::Approx (1.0f).epsilon (0.001));
    CHECK (pathPoint (PathPingPong, 0.5, 1.0f, 1).y == doctest::Approx (-1.0f).epsilon (0.001));
    CHECK (pathPoint (PathPingPong, 0.3, 1.0f, 1).y < -0.99f);  // already behind, holding
    CHECK (pathPoint (PathPingPong, 0.3, 0.0f, 1).y > -0.99f);  // smooth: still on its way
    CHECK (pathPoint (PathPingPong, 0.08, 0.5f, 1).x > 0.0f);   // front -> back passes on the right ...
    CHECK (pathPoint (PathPingPong, 0.58, 0.5f, 1).x < 0.0f);   // ... back -> front on the left
    CHECK (pathPoint (PathPingPong, 0.08, 0.5f, 1).z > 0.0f);   // and above ear height
    // swarm: stays around the front, deterministic per seed, continuous, not periodic
    for (double ph = 0.0; ph < 8.0; ph += 0.0005)
    {
        const auto a = pathPoint (PathSwarm, ph, 1.0f, 3), b = pathPoint (PathSwarm, ph + 0.0005, 1.0f, 3);
        CHECK (lengthOf ({ a.x - b.x, a.y - b.y, a.z - b.z }) < 0.03f);
        CHECK (std::abs (azimuthOf (a)) < 145.0f);
    }
    CHECK_FALSE (near (pathPoint (PathSwarm, 2.3, 0.8f, 9), pathPoint (PathSwarm, 2.3, 0.8f, 10)));
    CHECK_FALSE (near (pathPoint (PathSwarm, 0.3, 0.8f, 9), pathPoint (PathSwarm, 1.3, 0.8f, 9)));
    // whisper: always close behind / beside the head, nearest at the ears
    for (double ph = 0.0; ph < 1.0; ph += 0.01)
    {
        const auto v = pathPoint (PathWhisper, ph, 0.5f, 1);
        CHECK (lengthOf (v) <= 0.401f);
        CHECK (v.y < 0.5f);
    }
    CHECK (pathPoint (PathWhisper, 0.25, 1.0f, 1).x > 0.25f); // the right ear
    CHECK (pathPoint (PathWhisper, 0.75, 1.0f, 1).x < -0.25f); // the left ear
    // distance walk: far at the start, near halfway; size sets how far
    CHECK (lengthOf (pathPoint (PathWalk, 0.0, 1.0f, 1)) == doctest::Approx (16.0f).epsilon (0.001));
    CHECK (lengthOf (pathPoint (PathWalk, 0.0, 0.0f, 1)) == doctest::Approx (2.0f).epsilon (0.001));
    CHECK (lengthOf (pathPoint (PathWalk, 0.5, 1.0f, 1)) == doctest::Approx (0.35f).epsilon (0.001));
    // over the head: top, right ear, underneath, left ear
    CHECK (near (pathPoint (PathOverhead, 0.0, 0.0f, 1), { 0.0f, 0.0f, 1.0f }));
    CHECK (near (pathPoint (PathOverhead, 0.25, 0.0f, 1), { 1.0f, 0.0f, 0.0f }));
    CHECK (near (pathPoint (PathOverhead, 0.5, 0.0f, 1), { 0.0f, 0.0f, -1.0f }));
    CHECK (pathPoint (PathOverhead, 0.0, 1.0f, 1).y > 0.6f); // tilted: the top leans forward
    // weave: a full turn with three highs and three lows
    int crossings = 0; // zero crossings of the elevation inside one turn (at 1/6 ... 5/6; 0 and 1 are on the zero line)
    float prev = elevationOf (pathPoint (PathWeave, 0.01, 1.0f, 1));
    for (double ph = 0.012; ph < 0.99; ph += 0.002)
    {
        const float e = elevationOf (pathPoint (PathWeave, ph, 1.0f, 1));
        if ((prev < 0.0f) != (e < 0.0f)) ++crossings;
        prev = e;
    }
    CHECK (crossings == 5);
    CHECK (azimuthOf (pathPoint (PathWeave, 0.25, 0.0f, 1)) == doctest::Approx (90.0f).epsilon (0.001));
    CHECK (elevationOf (pathPoint (PathWeave, 0.25, 0.0f, 1)) == doctest::Approx (-10.0f).epsilon (0.01)); // a low point
}

TEST_CASE ("p30 space: new paths - no NaN, bounded output, click-free distance walk, near-field whisper level")
{
    for (int path = space::PathPingPong; path < space::numPaths; ++path)
        for (int monitor : { SpaceEffect::Headphones, SpaceEffect::Speakers })
        {
            CAPTURE (path);
            CAPTURE (monitor);
            Fx fx (monitor);
            fx.set (SpaceEffect::Path, (float) path).set (SpaceEffect::PathLength, 1.0f).set (SpaceEffect::PathSize, 1.0f)
                .set (SpaceEffect::Distance, 0.2f).set (SpaceEffect::Doppler, 1.0f).set (SpaceEffect::Room, 1.0f)
                .set (SpaceEffect::Glide, 0.005f).set (SpaceEffect::PathRate, 4.0f);
            Stereo s = noise (48000 * 2, 11, true);
            fx.run (s, 61);
            CHECK (allFinite (s.l));
            CHECK (allFinite (s.r));
            CHECK (peak (s.l) < 8.0f); // input peak 0.3 at 0.2 m with full room: near-field gain (x2 distance, x1.75 near ear) + room stay bounded
            CHECK (peak (s.r) < 8.0f);
            Stereo silence (48000 * 4);
            fx.run (silence);
            CHECK (noDenormals (silence.l));
            CHECK (noDenormals (silence.r));
            CHECK (peak (silence.l, 48000 * 3) < 1.0e-4f);
        }
    // the distance walk (a Doppler path like the fly-by) and the near-field whisper (the source passes right by the ear):
    // click-free at musical lengths (1 bar and longer; the generic test above runs every path once per beat)
    for (int path : { space::PathWalk, space::PathWhisper })
    for (int monitor : { SpaceEffect::Headphones, SpaceEffect::Speakers })
    {
        CAPTURE (path);
        CAPTURE (monitor);
        Fx fx (monitor);
        fx.set (SpaceEffect::Path, (float) path).set (SpaceEffect::PathLength, 4.0f).set (SpaceEffect::PathSize, 1.0f)
            .set (SpaceEffect::Doppler, 1.0f).set (SpaceEffect::Room, 0.5f).set (SpaceEffect::Glide, 0.005f);
        auto s = precSine (300.0, 96000 * 2);
        fx.run (s, 173);
        CHECK (allFinite (s.l));
        CHECK (clickRatio (s.l, 300.0f / 0.88f, 4800) < 1.6);
        CHECK (clickRatio (s.r, 300.0f / 0.88f, 4800) < 1.6);
    }
    // whisper at the right ear (headphones): much louder on the right, and louder overall than at the default distance
    {
        Fx fx;
        fx.set (SpaceEffect::Path, (float) space::PathWhisper).set (SpaceEffect::PathRate, 0.0f).set (SpaceEffect::PathPhase, 90.0f)
            .set (SpaceEffect::PathSize, 1.0f).set (SpaceEffect::Room, 0.0f).set (SpaceEffect::Spread, 0.0f).set (SpaceEffect::Doppler, 0.0f);
        Stereo s = noise (48000, 21);
        fx.run (s);
        CHECK (db (energy (s.r, 9600) / energy (s.l, 9600)) > 6.0);
        Fx far;
        far.set (SpaceEffect::Azimuth, 70.0f).set (SpaceEffect::Room, 0.0f).set (SpaceEffect::Spread, 0.0f).set (SpaceEffect::Doppler, 0.0f);
        Stereo t = noise (48000, 21);
        far.run (t);
        CHECK (energy (s.r, 9600) > 2.0 * energy (t.r, 9600));
    }
    // the stage reports the motion phase for the display's path preview
    {
        Fx fx;
        fx.set (SpaceEffect::Path, (float) space::PathWeave).set (SpaceEffect::PathLength, 2.0f);
        Stereo s (48000);
        fx.run (s);
        CHECK (fx.e.uiPhase() == doctest::Approx (2.0).epsilon (0.02)); // 1 s at 120 BPM, a cycle of one beat
    }
}

// ------------------------------------------------------------------------------------------------- p41: the 3D switch
TEST_CASE ("p41 space: the 3D switch IDs - one per part, appended after the metronome, Off / Headphones / Speakers")
{
    Core core;
    const auto& reg = core.params();
    for (int p = 0; p < Core::numParts; ++p)
    {
        const int idx = reg.indexOf (spacemode::modeId (p));
        REQUIRE (idx >= 0);
        const auto& d = reg.descriptor (idx);
        CHECK (d.kind == ParamKind::Choice);
        REQUIRE (d.choices.size() == 3);
        CHECK (d.choices[0] == "Off");
        CHECK (d.choices[1] == "Headphones");
        CHECK (d.choices[2] == "Speakers");
        CHECK (d.choicesDe[1] == "Kopfhörer");
        CHECK (d.choicesDe[2] == "Lautsprecher");
        CHECK (d.defaultValue == 0.0f); // off: as space.on
        CHECK (idx > reg.indexOf ("tape.loopsnap")); // p41 block follows p42
        if (p > 0) CHECK (idx == reg.indexOf (spacemode::modeId (p - 1)) + 1);
    }
    CHECK (spacemode::derive (false, 0) == spacemode::Off);
    CHECK (spacemode::derive (false, 1) == spacemode::Off);
    CHECK (spacemode::derive (true, 0) == spacemode::Headphones);
    CHECK (spacemode::derive (true, 1) == spacemode::Speakers);
}

TEST_CASE ("p41 space: the link - a moved switch sets space.on / space.out, changed values move the switches")
{
    Core core;
    auto& reg = core.params();
    spacemode::Link<Core::numParts> link;
    int writes = 0;
    auto get = [&] (const std::string& id) { return reg.get (id); };
    auto set = [&] (const std::string& id, float v) { ++writes; reg.set (id, v); };
    link.update (Core::numParts, get, set);
    CHECK (writes == 0); // a fresh instrument is consistent (all off)

    // encoder 1 / host / MIDI: part 1 -> Headphones
    reg.set ("p1.space.mode", 1.0f);
    link.update (Core::numParts, get, set);
    CHECK (reg.get ("p1.space.on") == 1.0f);
    CHECK (reg.get ("space.out") == 0.0f);
    // part 2 -> Speakers: the monitoring is global, part 1's switch follows
    reg.set ("p2.space.mode", 2.0f);
    link.update (Core::numParts, get, set);
    CHECK (reg.get ("p2.space.on") == 1.0f);
    CHECK (reg.get ("space.out") == 1.0f);
    CHECK (reg.get ("p1.space.mode") == 2.0f);
    CHECK (reg.get ("p3.space.mode") == 0.0f); // off stays off
    // part 1 -> Off: only part 1's stage switches off
    reg.set ("p1.space.mode", 0.0f);
    link.update (Core::numParts, get, set);
    CHECK (reg.get ("p1.space.on") == 0.0f);
    CHECK (reg.get ("p2.space.on") == 1.0f);
    CHECK (reg.get ("space.out") == 1.0f);
    // the monitoring page / automation of the older IDs: the switches follow
    reg.set ("space.out", 0.0f);
    reg.set ("p3.space.on", 1.0f);
    link.update (Core::numParts, get, set);
    CHECK (reg.get ("p2.space.mode") == 1.0f);
    CHECK (reg.get ("p3.space.mode") == 1.0f);
    CHECK (reg.get ("p1.space.mode") == 0.0f);
    // the stored values win when both move in one tick (a preset): the switch never undoes a load
    reg.set ("p3.space.on", 0.0f);
    reg.set ("p3.space.mode", 2.0f);
    link.update (Core::numParts, get, set);
    CHECK (reg.get ("p3.space.on") == 0.0f);
    CHECK (reg.get ("p3.space.mode") == 0.0f);
    CHECK (reg.get ("space.out") == 0.0f);
    // nothing moved: nothing is written
    writes = 0;
    link.update (Core::numParts, get, set);
    link.update (Core::numParts, get, set);
    CHECK (writes == 0);
}

TEST_CASE ("p41 space: presets and sessions set the switches from space.on / space.out (also when older ones lack them)")
{
    auto core = std::make_unique<Core>();
    auto& reg = core->params();
    // an older part preset: space on, no "space.mode" in it
    preset::Preset old;
    old.meta.kind = preset::Kind::Part;
    old.params["engine"] = (float) EngineType::Analog;
    old.params["space.on"] = 1.0f;
    reg.set ("space.out", 1.0f);
    reg.set ("p2.space.on", 1.0f);
    reg.set ("p2.space.mode", 2.0f);
    REQUIRE (preset::applyPart (old, reg, 1).ok);
    CHECK (reg.get ("p2.space.on") == 1.0f);
    CHECK (reg.get ("p2.space.mode") == 2.0f); // speakers monitoring stays (it belongs to the listener)
    reg.set ("space.out", 0.0f);
    REQUIRE (preset::applyPart (old, reg, 2).ok);
    CHECK (reg.get ("p3.space.mode") == 1.0f);

    // a session older than the switch: space.on of part 4 on, monitoring speakers, no mode values
    // (the cores live on the heap: Windows test threads have small stacks)
    auto b = std::make_unique<Core>();
    b->params().set ("p4.space.on", 1.0f);
    b->params().set ("space.out", 1.0f);
    auto j = nlohmann::json::parse (b->saveState());
    b.reset();
    for (int p = 0; p < Core::numParts; ++p) j["params"].erase (spacemode::modeId (p));
    const auto json = j.dump();
    auto c = std::make_unique<Core>();
    REQUIRE (c->loadState (json));
    CHECK (c->params().get ("p4.space.on") == 1.0f);
    CHECK (c->params().get ("p4.space.mode") == 2.0f);
    CHECK (c->params().get ("p1.space.mode") == 0.0f);
    // round trip with the switch: unchanged
    const auto saved = c->saveState();
    auto d = std::make_unique<Core>();
    REQUIRE (d->loadState (saved));
    for (int p = 0; p < Core::numParts; ++p) CHECK (d->params().get (spacemode::modeId (p)) == c->params().get (spacemode::modeId (p)));
}

TEST_CASE ("p41 space: Speakers renders the speaker path (no interaural delay), Headphones the binaural one, Off is a bypass")
{
    auto render = [] (int mode) {
        CoreRig rig;
        rig.set ("p1.engine", (float) EngineType::Analog);
        rig.set ("p1.space.azimuth", 90.0f);
        rig.set ("p1.space.room", 0.0f);
        rig.set ("p1.space.mode", (float) mode);
        rig.set ("p1.space.on", mode > 0 ? 1.0f : 0.0f); // what the plug-in's link writes for the switch
        rig.set ("space.out", mode == 2 ? 1.0f : 0.0f);
        rig.run (80, { MidiEvent::noteOn (1, 52, 110) });
        return rig.rec;
    };
    const auto off = render (0), phones = render (1), speakers = render (2);
    for (const auto* r : { &off, &phones, &speakers })
    {
        CHECK (allFinite (r->l));
        CHECK (allFinite (r->r));
        CHECK (noDenormals (r->l));
        CHECK (peak (r->l) < 4.0f);
    }
    // a source hard right: speakers pan it (left almost silent), headphones keep the far ear's shadowed, delayed sound
    const double lp = rms (phones.l), ls = rms (speakers.l), rs = rms (speakers.r);
    CHECK (rs > 0.0);
    CHECK (ls < 0.5 * rs);
    CHECK (lp > ls);
    double diff = 0.0;
    for (size_t i = 0; i < phones.size(); ++i) diff += std::abs (phones.l[i] - speakers.l[i]);
    CHECK (diff > 1.0);
}

// ------------------------------------------------------------------------------------------------- p73: more motion paths
TEST_CASE ("p73 space: eight more motion paths, appended - old indices and names keep their meaning")
{
    Fx fx;
    const auto d = fx.e.getParamDescriptors();
    const auto& path = d[(size_t) SpaceEffect::Path];
    REQUIRE (space::numPaths == 21);
    REQUIRE (path.choices.size() == (size_t) space::numPaths);
    REQUIRE (path.choicesDe.size() == (size_t) space::numPaths);
    const char* old[] = { "Off", "Circle", "Figure-8", "Fly-By", "Random Orbit", "Pendulum", "Spiral Up", "Ping-Pong F/B",
                          "Swarm", "Whisper", "Distance Walk", "Over the Head", "Weave" };
    for (int i = 0; i < 13; ++i) CHECK (path.choices[(size_t) i] == old[i]);
    const char* added[] = { "Ping-Pong L/R", "Four Corners", "Breathe", "Helix", "Fly-Over", "Bounce", "Wander", "Loop Orbit" };
    for (int i = 0; i < 8; ++i) CHECK (path.choices[(size_t) (13 + i)] == added[i]);
    CHECK (space::PathWeave == 12);
    CHECK (space::PathPingPongLR == 13);
    CHECK (space::PathLoopOrbit == 20);
    CHECK (path.defaultValue == 0.0f);
    CHECK (space::pathIsPeriodic (space::PathWander)); // a random shape, but the same loop every cycle
}

TEST_CASE ("p73 space: the new paths are continuous (also across the loop point), deterministic and shaped as named")
{
    using namespace space;
    auto dist = [] (Vec3 a, Vec3 b) { return lengthOf ({ a.x - b.x, a.y - b.y, a.z - b.z }); };
    for (int p = PathPingPongLR; p < numPaths; ++p)
        for (float size : { 0.0f, 0.37f, 1.0f })
        {
            CAPTURE (p);
            CAPTURE (size);
            // the loop point: the end of one cycle meets the start of the next
            CHECK (dist (pathPoint (p, 0.999999, size, 7), pathPoint (p, 1.0, size, 7)) < 1.0e-3f);
            CHECK (dist (pathPoint (p, 0.0, size, 7), pathPoint (p, 3.0, size, 7)) < 1.0e-4f);
            for (double ph = -1.0; ph < 2.0; ph += 0.0005) // no jumps anywhere (relative to the distance)
            {
                const auto a = pathPoint (p, ph, size, 7), b = pathPoint (p, ph + 0.0005, size, 7);
                CHECK (dist (a, b) / std::max (0.3f, lengthOf (a)) < 0.05f);
                CHECK (lengthOf (a) > 0.3f);
                CHECK (lengthOf (a) < 13.0f);
            }
            CHECK (dist (pathPoint (p, 0.43, size, 9), pathPoint (p, 0.43, size, 9)) == 0.0f); // deterministic
        }
    // ping-pong L/R: left ear at the start, right ear halfway, in front on the way right, behind on the way back
    CHECK (pathPoint (PathPingPongLR, 0.0, 0.5f, 1).x == doctest::Approx (-1.0f).epsilon (0.001));
    CHECK (pathPoint (PathPingPongLR, 0.5, 0.5f, 1).x == doctest::Approx (1.0f).epsilon (0.001));
    CHECK (pathPoint (PathPingPongLR, 0.4, 1.0f, 1).x > 0.99f); // snappy: arrived and holding
    CHECK (pathPoint (PathPingPongLR, 0.1, 0.5f, 1).y > 0.1f);
    CHECK (pathPoint (PathPingPongLR, 0.6, 0.5f, 1).y < -0.1f);
    // four corners: front left, front right, back right, back left - one per quarter, holding at the end of each
    CHECK (azimuthOf (pathPoint (PathCorners, 0.0, 0.5f, 1)) == doctest::Approx (-45.0f).epsilon (0.01));
    CHECK (azimuthOf (pathPoint (PathCorners, 0.249, 1.0f, 1)) == doctest::Approx (45.0f).epsilon (0.01));
    CHECK (azimuthOf (pathPoint (PathCorners, 0.499, 1.0f, 1)) == doctest::Approx (135.0f).epsilon (0.01));
    CHECK (azimuthOf (pathPoint (PathCorners, 0.749, 1.0f, 1)) == doctest::Approx (-135.0f).epsilon (0.01));
    // breathe: farthest at the start (size sets how far), nearest halfway
    CHECK (lengthOf (pathPoint (PathBreathe, 0.0, 1.0f, 1)) == doctest::Approx (3.0f).epsilon (0.001));
    CHECK (lengthOf (pathPoint (PathBreathe, 0.5, 0.3f, 1)) == doctest::Approx (0.45f).epsilon (0.001));
    // helix: low at the start, high halfway, low again; 2 .. 4 turns
    CHECK (elevationOf (pathPoint (PathHelix, 0.0, 0.5f, 1)) == doctest::Approx (-25.0f).epsilon (0.01));
    CHECK (elevationOf (pathPoint (PathHelix, 0.5, 0.5f, 1)) == doctest::Approx (75.0f).epsilon (0.01));
    CHECK (azimuthOf (pathPoint (PathHelix, 0.125, 0.0f, 1)) == doctest::Approx (90.0f).epsilon (0.01)); // 2 turns
    // fly-over: far in front at the start, right overhead after a quarter, far behind halfway
    CHECK (pathPoint (PathFlyOver, 0.0, 1.0f, 1).y == doctest::Approx (12.0f).epsilon (0.001));
    CHECK (dist (pathPoint (PathFlyOver, 0.25, 0.5f, 1), { 0.0f, 0.0f, 1.0f }) < 1.0e-4f);
    CHECK (pathPoint (PathFlyOver, 0.5, 0.0f, 1).y == doctest::Approx (-2.0f).epsilon (0.001));
    // bounce: on the floor at the start, six hops per cycle, from the left to the right
    CHECK (elevationOf (pathPoint (PathBounce, 0.0, 1.0f, 1)) == doctest::Approx (-15.0f).epsilon (0.01));
    CHECK (elevationOf (pathPoint (PathBounce, 1.0 / 12.0, 1.0f, 1)) == doctest::Approx (45.0f).epsilon (0.01));
    CHECK (azimuthOf (pathPoint (PathBounce, 0.0, 1.0f, 1)) == doctest::Approx (-70.0f).epsilon (0.01));
    CHECK (azimuthOf (pathPoint (PathBounce, 0.5, 1.0f, 1)) == doctest::Approx (70.0f).epsilon (0.01));
    // wander: every seed its own loop
    CHECK (dist (pathPoint (PathWander, 0.3, 0.8f, 9), pathPoint (PathWander, 0.3, 0.8f, 10)) > 1.0e-3f);
    CHECK (dist (pathPoint (PathWander, 0.3, 0.8f, 9), pathPoint (PathWander, 4.3, 0.8f, 9)) < 1.0e-4f);
    // loop orbit: nearest (1 - loop size) in the middle of each loop, a quarter turn apart
    CHECK (lengthOf (pathPoint (PathLoopOrbit, 0.125, 1.0f, 1)) == doctest::Approx (0.3317f).epsilon (0.01));
    CHECK (lengthOf (pathPoint (PathLoopOrbit, 0.0, 1.0f, 1)) > 1.6f);
}

TEST_CASE ("p73 space: new paths render clean - every sample rate, 1-sample buffers, deterministic, click-free")
{
    for (int path = space::PathPingPongLR; path < space::numPaths; ++path)
        for (double rate : { 44100.0, (double) sampleRate, 96000.0 })
        {
            CAPTURE (path);
            CAPTURE (rate);
            auto render = [path, rate] (int block) {
                SpaceEffect e;
                e.prepare (rate, 512);
                const auto d = e.getParamDescriptors();
                for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
                e.setTempo (120.0);
                e.setParam (SpaceEffect::Path, (float) path);
                e.setParam (SpaceEffect::PathLength, 4.0f);
                e.setParam (SpaceEffect::PathSize, 1.0f);
                e.setParam (SpaceEffect::Doppler, 1.0f);
                e.setParam (SpaceEffect::Room, 0.0f);
                Stereo s ((size_t) (rate * 2.2));
                for (size_t i = 0; i < s.size(); ++i)
                    s.l[i] = s.r[i] = 0.3f * (float) std::sin (2.0 * 3.14159265358979323846 * 300.0 * (double) i / rate);
                for (size_t pos = 0; pos < s.size(); pos += (size_t) block)
                    e.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> ((size_t) block, s.size() - pos));
                return s;
            };
            const auto one = render (1);
            CHECK (allFinite (one.l));
            CHECK (allFinite (one.r));
            CHECK (peak (one.l) < 4.0f);
            CHECK (peak (one.r) < 4.0f);
            const auto big = render (512); // the same output on every render; any buffer size gives the same level
            const auto again = render (512);
            double maxRun = 0.0;
            for (size_t i = 0; i < big.size(); ++i) maxRun = std::max (maxRun, (double) std::abs (big.l[i] - again.l[i]));
            CHECK (maxRun == 0.0);
            CHECK (std::abs (db (energy (big.l) + 1e-12) - db (energy (one.l) + 1e-12)) < 0.5);
            CHECK (std::abs (db (energy (big.r) + 1e-12) - db (energy (one.r) + 1e-12)) < 0.5);
            if (rate != (double) sampleRate) continue;
            // a 300 Hz tone stays smooth over a whole cycle of one bar (curvature measured against the highest pitch the
            // Doppler limit allows), the loop point included
            CHECK (clickRatio (one.l, 300.0f / 0.88f, 4800) < 1.6);
            CHECK (clickRatio (one.r, 300.0f / 0.88f, 4800) < 1.6);
        }
}
