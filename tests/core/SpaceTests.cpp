// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// 1.0: the 3D spatialiser (SpaceEffect): per-part stage, rack type, motion paths, binaural cues, mono safety.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/InsertSlot.h"
#include "fx/Space3D.h"
#include "mod/ModMatrix.h"

#include <doctest/doctest.h>

#include <chrono>
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
    CHECK (InsertSlot::numTypes == InsertSlot::firstSpaceType + 1);
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
    // p14: the performance wheels (perf.bend, perf.mod) come right after the punch pass IDs
    CHECK (reg.indexOf ("mfx4.chorus.lowcut") + 1 == reg.indexOf ("perf.bend"));
    CHECK (reg.indexOf ("perf.bend") + 1 == reg.indexOf ("perf.mod"));
    // p16: the morph space block follows the wheels
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
            if (p != PathRandom) CHECK (near (pathPoint (p, 0.3, size, 5), pathPoint (p, 1.3, size, 5))); // periodic
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
        for (int path : { space::PathCircle, space::PathFigure8, space::PathRandom, space::PathPendulum, space::PathSpiral })
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
        e->setParam (SpaceEffect::Path, (float) (1 + i % 6));
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
