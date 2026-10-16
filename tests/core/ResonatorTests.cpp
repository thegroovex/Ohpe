// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p19: the resonator body (ResonatorEffect): per-part stage, rack type, models, tuning, stability, bypass, state.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/InsertSlot.h"
#include "fx/Resonator.h"
#include "mod/ModMatrix.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <cmath>
#include <complex>
#include <memory>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
using R = ResonatorEffect;

struct Fx
{
    R e;
    KeyState keys;
    explicit Fx (int model = R::ModelString, bool withKeys = true)
    {
        e.prepare (sampleRate, 512);
        const auto d = e.getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
        e.setParam (R::Model, (float) model);
        if (withKeys) e.setKeys (&keys);
        e.reset();
    }
    Fx& set (R::Param p, float v)
    {
        e.setParam (p, v);
        return *this;
    }
    void note (int n)
    {
        keys.lastNote = n;
        keys.held[0] = n;
        keys.numHeld = 1;
        ++keys.noteCounter;
    }
    void run (Stereo& s, int block = 256)
    {
        for (size_t pos = 0; pos < s.size(); pos += (size_t) block)
            e.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> ((size_t) block, s.size() - pos));
    }
};

Stereo impulse (size_t n, float amp = 1.0f)
{
    Stereo s (n);
    s.l[16] = s.r[16] = amp;
    return s;
}

Stereo noise (size_t n, uint32_t seed, float amp = 0.3f)
{
    Stereo s (n);
    dsp::Random a (seed), b (seed * 7 + 3);
    for (size_t i = 0; i < n; ++i) { s.l[i] = amp * a.nextBipolar(); s.r[i] = amp * b.nextBipolar(); }
    return s;
}

/** The strongest frequency within +-range (fraction) of `hz` and its magnitude (fine Goertzel scan). */
std::pair<double, double> strongestNear (const std::vector<float>& v, size_t from, size_t to, double hz, double range = 0.04)
{
    double best = 0.0, bestHz = hz;
    for (double f = hz * (1.0 - range); f <= hz * (1.0 + range); f += hz * 0.0005)
    {
        const double c = 2.0 * std::cos (2.0 * 3.14159265358979 * f / sampleRate);
        double s1 = 0.0, s2 = 0.0;
        for (size_t i = from; i < to; ++i)
        {
            const double s0 = v[i] + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double m = std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (double) (to - from);
        if (m > best) { best = m; bestHz = f; }
    }
    return { bestHz, best };
}

double noteHz (double n) { return 440.0 * std::pow (2.0, (n - 69.0) / 12.0); }

struct CoreRig
{
    std::unique_ptr<Core> owner = std::make_unique<Core>();
    Core& core = *owner;
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

TEST_CASE ("p19 resonator: parameters - per-part stage, rack type in every slot, matrix targets, appended last")
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    const auto& reg = core.params();
    const auto d = R().getParamDescriptors();
    REQUIRE (d.size() == (size_t) R::numParams);
    for (int p = 1; p <= Core::numParts; ++p)
    {
        const auto n = "p" + std::to_string (p) + ".";
        REQUIRE (reg.indexOf (n + "reso.on") >= 0);
        CHECK (reg.get (n + "reso.on") == 0.0f); // off by default: every older preset sounds as before
        for (const auto& x : d)
        {
            CHECK (reg.indexOf (n + "reso." + x.id) >= 0);
            CHECK (reg.indexOf (n + "fx.resonator." + x.id) >= 0);
            CHECK (reg.indexOf (n + "fx6.resonator." + x.id) >= 0);
        }
    }
    CHECK (reg.indexOf ("mfx4.resonator.mix") >= 0);
    CHECK (InsertSlot::firstResoType == InsertSlot::firstSpaceType + 1);
    CHECK (InsertSlot::numTypes == InsertSlot::firstResoType + 1);
    CHECK (InsertSlot::typeNamesEn()[(size_t) InsertSlot::Resonator] == "Resonator");
    CHECK (InsertSlot::typeNamesDe()[(size_t) InsertSlot::Resonator] == "Resonanzkörper");
    CHECK (std::string (InsertSlot::typeInfo (InsertSlot::Resonator).id) == "resonator");
    // the block follows the p20 audio to wavetable block; the master racks' blocks end it (later blocks follow: p21)
    CHECK (reg.indexOf ("p1.reso.on") == reg.indexOf ("p4.usertable.vocal") + 1);
    CHECK (reg.indexOf ("mfx4.resonator.slideamt") < reg.indexOf ("p1.char.bright"));
    // matrix targets, appended after Morph Y
    const char* keys[] = { "resotune", "resosize", "resomaterial", "resodecay", "resoposition", "resomix" };
    const char* suffix[] = { "reso.tune", "reso.size", "reso.material", "reso.decay", "reso.position", "reso.mix" };
    CHECK (mod::DstResoTune == mod::DstMorphY + 1);
    CHECK (mod::numDests > mod::DstResoMix); // p21: the character targets follow
    for (int k = 0; k < 6; ++k)
    {
        CHECK (mod::destIndexOfKey (keys[k]) == mod::DstResoTune + k);
        CHECK (core.part (2).destParamIndex (mod::DstResoTune + k) == reg.indexOf (std::string ("p3.") + suffix[k]));
        CHECK (core.part (2).destAvailable (mod::DstResoTune + k));
    }
}

TEST_CASE ("p19 resonator: mix 0 and off are exact bypasses (effect, part stage)")
{
    for (int model = 0; model < R::numModels; ++model)
    {
        CAPTURE (model);
        Fx fx (model);
        fx.set (R::Mix, 0.0f).set (R::Drive, 1.0f).set (R::Decay, 20.0f);
        fx.e.reset();
        fx.note (60);
        const Stereo in = noise (20000, 3);
        Stereo out = in;
        fx.run (out);
        for (size_t i = 0; i < in.size(); ++i)
        {
            REQUIRE (out.l[i] == in.l[i]);
            REQUIRE (out.r[i] == in.r[i]);
        }
        // turning the mix up and back down to 0 ends in an exact bypass again
        fx.set (R::Mix, 0.7f);
        Stereo mid = in;
        fx.run (mid);
        CHECK (rms (mid.l) > 0.0f);
        fx.set (R::Mix, 0.0f);
        Stereo fade = in;
        fx.run (fade); // fades out over ~20 ms
        Stereo after = in;
        fx.run (after);
        for (size_t i = 0; i < in.size(); ++i) REQUIRE (after.l[i] == in.l[i]);
    }

    // the part stage: off (with body parameters touched), and on at mix 0, render exactly like the plain part
    auto render = [] (int mode) {
        CoreRig rig;
        rig.set ("p1.engine", (float) EngineType::Pluck);
        if (mode > 0)
        {
            rig.set ("p1.reso.model", (float) R::ModelPlate);
            rig.set ("p1.reso.drive", 0.8f);
            rig.set ("p1.reso.decay", 9.0f);
        }
        if (mode == 2) { rig.set ("p1.reso.on", 1.0f); rig.set ("p1.reso.mix", 0.0f); }
        rig.run (80, { MidiEvent::noteOn (1, 57, 110) });
        return rig.rec;
    };
    const auto plain = render (0), offTouched = render (1), onMix0 = render (2);
    REQUIRE (rms (plain.l) > 0.0f);
    for (size_t i = 0; i < plain.size(); ++i)
    {
        REQUIRE (plain.l[i] == offTouched.l[i]);
        REQUIRE (plain.l[i] == onMix0.l[i]);
        REQUIRE (plain.r[i] == onMix0.r[i]);
    }
}

TEST_CASE ("p19 resonator: stability sweep - every model and tuning, extreme and random settings, modulated every block")
{
    const auto d = R().getParamDescriptors();
    dsp::Random rng (1234);
    for (int model = 0; model < R::numModels; ++model)
        for (int follow = 0; follow < R::numFollowModes; ++follow)
        {
            CAPTURE (model);
            CAPTURE (follow);
            Fx fx (model);
            fx.set (R::Follow, (float) follow);
            // loud, nasty input: full-scale noise bursts, a DC step, a clipped square near the body pitch, then silence
            Stereo s (48000 * 2);
            dsp::Random n (7u + (uint32_t) model);
            for (size_t i = 0; i < s.size(); ++i)
            {
                float x = 0.0f;
                if (i < 12000) x = 4.0f * n.nextBipolar();
                else if (i < 20000) x = 1.0f;
                else if (i < 60000) x = std::sin (6.2831853f * 261.6f * (float) i / 48000.0f) > 0.0f ? 2.0f : -2.0f;
                s.l[i] = x;
                s.r[i] = -x;
            }
            for (size_t pos = 0; pos < s.size(); pos += 128)
            {
                for (size_t k = 0; k < d.size(); ++k)
                {
                    if ((int) k == R::Model || (int) k == R::Follow) continue;
                    const float u = 0.5f + 0.5f * rng.nextBipolar();
                    const float v = rng.nextBipolar() > 0.3f ? (u > 0.5f ? d[k].maxValue : d[k].minValue)
                                                             : d[k].minValue + u * (d[k].maxValue - d[k].minValue);
                    fx.e.setParam ((int) k, v);
                }
                if (pos % 1024 == 0) // keys: jumps over the whole range, chords, bends, expression
                {
                    fx.keys.lastNote = (int) (rng.nextU32() % 128u);
                    fx.keys.numHeld = (int) (rng.nextU32() % 9u);
                    for (int h = 0; h < fx.keys.numHeld; ++h) fx.keys.held[(size_t) h] = (int) (rng.nextU32() % 128u);
                    fx.keys.bendSemitones = 48.0f * rng.nextBipolar();
                    fx.keys.pressure = 0.5f + 0.5f * rng.nextBipolar();
                    fx.keys.slide = rng.nextBipolar();
                }
                fx.e.process (s.l.data() + pos, s.r.data() + pos, 128);
            }
            CHECK (allFinite (s.l));
            CHECK (allFinite (s.r));
            CHECK (peak (s.l) <= 4.0f); // between the dry signal (|x| <= 4) and the wet one (|wet| <= 1)
            CHECK (fx.e.guardResets() == 0);
            // then silence with default settings: finite, bounded, no denormals, and it rings out
            for (size_t k = 0; k < d.size(); ++k)
                if ((int) k != R::Model && (int) k != R::Follow) fx.e.setParam ((int) k, d[k].defaultValue);
            fx.set (R::Mix, 1.0f).set (R::Decay, 1.0f);
            Stereo silence (48000 * 6);
            fx.run (silence);
            CHECK (allFinite (silence.l));
            CHECK (noDenormals (silence.l));
            CHECK (noDenormals (silence.r));
            CHECK (peak (silence.l) <= 1.0f);
            CHECK (peak (silence.l, 48000 * 5) < 1.0e-4f);
            CHECK (fx.e.guardResets() == 0);
        }
}

TEST_CASE ("p19 resonator: bounded level - the body at its loudest never exceeds full scale")
{
    for (int model = 0; model < R::numModels; ++model)
    {
        CAPTURE (model);
        Fx fx (model);
        fx.set (R::Mix, 1.0f).set (R::Decay, 20.0f).set (R::Drive, 1.0f).set (R::Material, 1.0f).set (R::Sympathetic, 1.0f).set (R::Inharm, 0.0f);
        fx.note (48);
        Stereo s (48000 * 2);
        for (size_t i = 0; i < s.size(); ++i) s.l[i] = s.r[i] = std::sin (6.2831853f * 130.81f * (float) i / 48000.0f); // on the body pitch
        fx.run (s);
        CHECK (allFinite (s.l));
        CHECK (peak (s.l) <= 1.0f);
        CHECK (peak (s.r) <= 1.0f);
        CHECK (rms (s.l, 48000) > 0.1f); // it really resonates
    }
}

TEST_CASE ("p19 resonator: tuning - follows the note, open tube all harmonics, closed tube odd ones, plate fundamental, bend")
{
    const size_t n = 48000;
    auto ring = [&] (int model, int note, float bend = 0.0f, float tune = 0.0f) {
        Fx fx (model);
        fx.set (R::Mix, 1.0f).set (R::Sympathetic, 0.0f).set (R::Inharm, 0.0f).set (R::Decay, 4.0f).set (R::Tune, tune);
        fx.note (note);
        fx.keys.bendSemitones = bend;
        Stereo s = impulse (n);
        fx.run (s);
        CHECK (fx.e.currentPitchHz() == doctest::Approx (noteHz (note + bend + tune)).epsilon (0.001));
        return s;
    };
    for (int model : { (int) R::ModelString, (int) R::ModelOpenTube, (int) R::ModelClosedTube, (int) R::ModelPlate })
        for (int note : { 45, 57, 69 })
        {
            CAPTURE (model);
            CAPTURE (note);
            const auto s = ring (model, note);
            const double f0 = noteHz (note);
            const auto h1 = strongestNear (s.l, 2400, 2400 + 16384, f0);
            CHECK (std::abs (centsBetween (h1.first, f0)) < 15.0);
            if (model == R::ModelPlate) continue;
            const auto h2 = strongestNear (s.l, 2400, 2400 + 16384, 2.0 * f0, 0.01);
            const auto h3 = strongestNear (s.l, 2400, 2400 + 16384, 3.0 * f0);
            CHECK (std::abs (centsBetween (h3.first, 3.0 * f0)) < 15.0);
            if (model == R::ModelClosedTube) CHECK (h2.second < 0.1 * h3.second); // closed at one end: odd harmonics
            else CHECK (h2.second > 0.2 * h3.second);
        }
    // pitch bend and tune move the body
    const auto bent = ring (R::ModelString, 57, 2.0f);
    CHECK (std::abs (centsBetween (strongestNear (bent.l, 2400, 2400 + 16384, noteHz (59)).first, noteHz (59))) < 15.0);
    ring (R::ModelString, 57, 0.0f, -12.0f);
    // fixed tuning ignores the keys: C3 + tune
    Fx fixed (R::ModelString);
    fixed.set (R::Follow, (float) R::FollowFixed).set (R::Tune, 7.0f);
    fixed.note (80);
    Stereo s = impulse (512);
    fixed.run (s);
    CHECK (fixed.e.currentPitchHz() == doctest::Approx (noteHz (55)).epsilon (0.001));
    // no keyboard (master rack): fixed tuning too
    Fx master (R::ModelString, false);
    Stereo s2 = impulse (512);
    master.run (s2);
    CHECK (master.e.currentPitchHz() == doctest::Approx (noteHz (48)).epsilon (0.001));
}

TEST_CASE ("p19 resonator: sympathetic strings - octaves / fifths of the note, the held chord, kept after release")
{
    Fx fx (R::ModelString);
    fx.set (R::Follow, (float) R::FollowChord).set (R::Sympathetic, 1.0f).set (R::Size, 1.0f).set (R::Mix, 1.0f);
    fx.keys.lastNote = 60;
    fx.keys.numHeld = 3;
    fx.keys.held = { 60, 64, 67, 0, 0, 0, 0, 0 };
    Stereo s = noise (4096, 9);
    fx.run (s);
    REQUIRE (fx.e.activeSympathetic() == R::maxSympathetic);
    CHECK (fx.e.sympatheticHz (0) == doctest::Approx (noteHz (60)).epsilon (0.001));
    CHECK (fx.e.sympatheticHz (1) == doctest::Approx (noteHz (64)).epsilon (0.001));
    CHECK (fx.e.sympatheticHz (2) == doctest::Approx (noteHz (67)).epsilon (0.001));
    CHECK (fx.e.sympatheticHz (3) == doctest::Approx (noteHz (72)).epsilon (0.001));
    fx.keys.numHeld = 0; // released: they keep ringing at the chord
    Stereo s2 = noise (4096, 10);
    fx.run (s2);
    CHECK (fx.e.sympatheticHz (1) == doctest::Approx (noteHz (64)).epsilon (0.001));
    // note mode: octave and fifth above the note
    fx.set (R::Follow, (float) R::FollowNote).set (R::Size, 0.0f);
    Stereo s3 = noise (4096, 11);
    fx.run (s3);
    CHECK (fx.e.activeSympathetic() == 2);
    CHECK (fx.e.sympatheticHz (0) == doctest::Approx (noteHz (72)).epsilon (0.001));
    CHECK (fx.e.sympatheticHz (1) == doctest::Approx (noteHz (67)).epsilon (0.001));
    // they ring on: with sympathetic strings the tail is longer
    auto tail = [] (float symp) {
        Fx f (R::ModelPlate);
        f.set (R::Sympathetic, symp).set (R::Mix, 1.0f).set (R::Decay, 0.4f);
        f.note (57);
        Stereo x = noise (48000 * 2, 4);
        for (size_t i = 4800; i < x.size(); ++i) x.l[i] = x.r[i] = 0.0f;
        f.run (x);
        return rms (x.l, 48000, 96000);
    };
    CHECK (tail (1.0f) > 2.0f * tail (0.0f));
}

TEST_CASE ("p19 resonator: decay sets the tail; MPE pressure rings longer / damps, slide moves the position")
{
    auto tailAfter = [] (float decay, float pressure, float pressAmt) {
        Fx fx (R::ModelString);
        fx.set (R::Mix, 1.0f).set (R::Decay, decay).set (R::Sympathetic, 0.0f).set (R::PressAmt, pressAmt);
        fx.note (57);
        fx.keys.pressure = pressure;
        Stereo s = impulse (48000 * 2);
        fx.run (s);
        return rms (s.l, 24000, 48000) / std::max (1.0e-12f, rms (s.l, 0, 12000));
    };
    CHECK (tailAfter (4.0f, 0.0f, 0.5f) > 4.0f * tailAfter (0.5f, 0.0f, 0.5f));
    CHECK (tailAfter (1.0f, 1.0f, 1.0f) > 2.0f * tailAfter (1.0f, 0.0f, 1.0f));  // pressure: longer
    CHECK (tailAfter (1.0f, 1.0f, -1.0f) < 0.5f * tailAfter (1.0f, 0.0f, -1.0f)); // negative amount: pressure damps

    // slide moves the excitation point (the display shows it)
    Fx fx (R::ModelPlate);
    fx.set (R::Position, 0.3f).set (R::SlideAmt, 1.0f);
    fx.keys.slide = 1.0f;
    Stereo s = noise (48000 / 2, 2);
    fx.run (s);
    CHECK (fx.e.uiExcitePosition() == doctest::Approx (0.8f).epsilon (0.02));
    CHECK (fx.e.uiLevel() > 0.0f);
}

TEST_CASE ("p19 resonator: the part stage follows the keyboard; the rack type too; the matrix moves it")
{
    CoreRig rig;
    rig.set ("p1.engine", (float) EngineType::Pluck);
    rig.set ("p1.reso.on", 1.0f);
    rig.set ("p1.reso.mix", 1.0f);
    rig.set ("p1.fx2.type", (float) InsertSlot::Resonator);
    rig.core.fxRack (0).prepareSelected (rig.core.params());
    rig.run (40, { MidiEvent::noteOn (1, 64, 100) });
    CHECK (allFinite (rig.rec.l));
    CHECK (rms (rig.rec.l) > 0.0f);
    CHECK (rig.core.part (0).resoRunning());
    CHECK (rig.core.part (0).keyState().lastNote == 64);
    CHECK (rig.core.part (0).resoStage().currentPitchHz() == doctest::Approx (noteHz (64)).epsilon (0.002));
    auto* rackReso = dynamic_cast<R*> (rig.core.fxRack (0).slot (1).effect (InsertSlot::Resonator));
    REQUIRE (rackReso != nullptr);
    CHECK (rackReso->currentPitchHz() == doctest::Approx (noteHz (64)).epsilon (0.002));
    // pitch bend (2 semitones range by default) follows
    rig.run (40, { MidiEvent { 0, (uint8_t) 0xe0, 127, 127 } }); // channel 1, full up
    CHECK (rig.core.part (0).resoStage().currentPitchHz() == doctest::Approx (noteHz (66)).epsilon (0.01));
    // chord: the held keys
    rig.run (4, { MidiEvent::noteOn (1, 67, 100), MidiEvent::noteOn (1, 71, 100) });
    CHECK (rig.core.part (0).keyState().numHeld == 3);
    // the matrix: macro 1 -> body tune (a quarter of the 48 semitone range = +12)
    rig.set ("p1.mod.s1.src", (float) mod::SrcMacro1);
    rig.set ("p1.mod.s1.dst", (float) mod::DstResoTune);
    rig.set ("p1.mod.s1.amt", 0.25f);
    rig.set ("p1.macro1", 1.0f);
    rig.run (40);
    CHECK (rig.core.part (0).resoStage().currentPitchHz() == doctest::Approx (noteHz (71 + 2 + 12)).epsilon (0.01));
    CHECK (rig.core.params().get ("p1.reso.tune") == 0.0f);
}

TEST_CASE ("p19 resonator: state and part presets keep the body")
{
    auto pa = std::make_unique<Core>(), pb = std::make_unique<Core>(), pc = std::make_unique<Core>();
    Core &a = *pa, &b = *pb, &c = *pc;
    const std::pair<const char*, float> values[] = { { "p2.reso.on", 1.0f }, { "p2.reso.model", 3.0f }, { "p2.reso.follow", 2.0f },
                                                     { "p2.reso.tune", -5.0f }, { "p2.reso.size", 0.8f }, { "p2.reso.material", 0.2f },
                                                     { "p2.reso.decay", 7.5f }, { "p2.reso.position", 0.66f }, { "p2.reso.inharm", 0.4f },
                                                     { "p2.reso.sympathetic", 0.9f }, { "p2.reso.width", 0.1f }, { "p2.reso.drive", 0.3f },
                                                     { "p2.reso.mix", 0.75f }, { "p2.reso.pressamt", -0.6f }, { "p2.reso.slideamt", 0.2f },
                                                     { "p2.fx3.type", (float) InsertSlot::Resonator }, { "p2.fx3.resonator.model", 1.0f },
                                                     { "mfx2.resonator.decay", 12.0f } };
    for (const auto& [id, v] : values) REQUIRE (a.params().set (id, v));
    REQUIRE (b.loadState (a.saveState()));
    for (const auto& [id, v] : values)
    {
        CAPTURE (id);
        CHECK (b.params().get (id) == doctest::Approx (v));
    }
    // a part preset carries the stage into another part
    auto p = preset::capturePart (a.params(), 1, {});
    preset::Preset back;
    REQUIRE (preset::fromJson (preset::toJson (p), back));
    REQUIRE (preset::applyPart (back, c.params(), 3).ok);
    CHECK (c.params().get ("p4.reso.on") == 1.0f);
    CHECK (c.params().get ("p4.reso.decay") == doctest::Approx (7.5f));
    CHECK (c.params().get ("p4.reso.model") == 3.0f);
}
