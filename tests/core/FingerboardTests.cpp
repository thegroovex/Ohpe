// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p50: Fingerboard mode - continuous-surface MPE controllers with the high-resolution MPE protocol ("MPE HD"):
// CC 87 LSB prefix (14-bit pressure / timbre, 21-bit bend), NRPN 100..102 smoothing cutoffs, RPN 0 bend range,
// RPN 6 zone, note onset from pressure, pitch rounding, the Key + Bend matrix source, sostenuto.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "midi/MpeInput.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
MidiEvent cc (int ch, int n, int v) { return MidiEvent::controller (ch, n, v); }
MidiEvent press (int ch, int v)
{
    MidiEvent e;
    e.status = (uint8_t) (0xd0 | (ch - 1));
    e.data1 = (uint8_t) (v & 0x7f);
    return e;
}
MidiEvent bend (int ch, int v14)
{
    MidiEvent e;
    e.status = (uint8_t) (0xe0 | (ch - 1));
    e.data1 = (uint8_t) (v14 & 0x7f);
    e.data2 = (uint8_t) ((v14 >> 7) & 0x7f);
    return e;
}
/** 14-bit pressure / timbre value v (0 .. 0x3F80) as CC 87 + MSB message. */
void press14 (std::vector<MidiEvent>& ev, int ch, int v)
{
    ev.push_back (cc (ch, 87, v & 0x7f));
    ev.push_back (press (ch, v >> 7));
}
/** 21-bit bend (0 .. 0x1FFF80) as CC 87 + bend message. */
void bend21 (std::vector<MidiEvent>& ev, int ch, int v)
{
    ev.push_back (cc (ch, 87, v & 0x7f));
    ev.push_back (bend (ch, v >> 7));
}
void nrpn (std::vector<MidiEvent>& ev, int ch, int lsb, int value)
{
    ev.push_back (cc (ch, 99, 0));
    ev.push_back (cc (ch, 98, lsb));
    ev.push_back (cc (ch, 6, value));
    ev.push_back (cc (ch, 99, 127)); // null
    ev.push_back (cc (ch, 98, 127));
}
void rpn (std::vector<MidiEvent>& ev, int ch, int lsb, int value)
{
    ev.push_back (cc (ch, 101, 0));
    ev.push_back (cc (ch, 100, lsb));
    ev.push_back (cc (ch, 6, value));
    ev.push_back (cc (ch, 101, 127));
    ev.push_back (cc (ch, 100, 127));
}

struct Rig
{
    Core core;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    Stereo rec;

    explicit Rig (bool fingerboard = true)
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        set ("mix.master.limit", 0.0f);
        set ("p1.engine", (float) EngineType::Wavetable);
        set ("p1.wavetable.pos", 0.0f);
        set ("p1.wavetable.unison", 0.0f);
        set ("p1.wavetable.univoices", 1.0f);
        set ("p1.wavetable.tone", 1.0f);
        set ("p1.amp.a", 0.001f);
        set ("p1.amp.s", 1.0f);
        set ("p1.mpe", 1.0f);
        if (fingerboard) // what the "Fingerboard mode" button sets
        {
            set ("p1.mpe.bend", 96.0f);
            set ("p1.mpe.onset", (float) MpeInput::OnsetPressure);
            set ("p1.mpe.hd", (float) MpeInput::HdAuto);
        }
        for (int p = 2; p <= Core::numParts; ++p) set ("p" + std::to_string (p) + ".vol", 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }
    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, {});
            rec.l.insert (rec.l.end(), l[0].begin(), l[0].end());
            rec.r.insert (rec.r.end(), r[0].begin(), r[0].end());
        }
    }
    float source (int src) { return core.part (0).modMatrix().sourceValue (src); }
    const MpeInput& mpe() const { return core.mpeInput (0); }
};
} // namespace

TEST_CASE ("p50: parameters, defaults and the Key + Bend matrix source")
{
    Core core;
    const auto& reg = core.params();
    for (int p = 1; p <= Core::numParts; ++p)
    {
        const std::string pre = "p" + std::to_string (p) + ".";
        for (const char* k : { "mpe.hd", "mpe.onset", "mpe.round", "mpe.round.time", "mpe.pcurve" }) REQUIRE (reg.indexOf (pre + k) >= 0);
        CHECK (reg.descriptor (reg.indexOf (pre + "mpe.hd")).defaultValue == (float) MpeInput::HdAuto);
        CHECK (reg.descriptor (reg.indexOf (pre + "mpe.onset")).defaultValue == (float) MpeInput::OnsetVelocity);
        CHECK (reg.descriptor (reg.indexOf (pre + "mpe.round")).defaultValue == 0.0f);
        CHECK (reg.descriptor (reg.indexOf (pre + "mpe.pcurve")).defaultValue == 0.0f);
        CHECK (reg.descriptor (reg.indexOf (pre + "mpe.bend")).maxValue == 96.0f);
    }
    CHECK (mod::SrcKeyBend == 33);
    CHECK (mod::SrcKeyBend == mod::SrcVibrato + 1);
    CHECK (mod::sourceTable().size() == (size_t) mod::numSources);
    CHECK (mod::sourceIndexOfKey ("keybend") == mod::SrcKeyBend);
    CHECK (mod::isBipolar (mod::SrcKeyBend));
}

TEST_CASE ("p50: CC 87 gives 14-bit pressure / timbre and 21-bit bend; 7-bit and 14-bit messages mix freely")
{
    Rig rig;
    rig.run (1, { MidiEvent::noteOn (2, 60, 127) });
    // 14-bit pressure: MSB 0x40, LSB 0x40 = 0x2040 / 0x3F80
    std::vector<MidiEvent> ev;
    press14 (ev, 2, 0x2040);
    rig.run (1, ev);
    CHECK (rig.source (mod::SrcNotePressure) == doctest::Approx ((double) 0x2040 / (double) 0x3F80).epsilon (1.0e-6));
    CHECK ((rig.mpe().statusBits() & MpeInput::StatusHdData) != 0);
    // the next plain message is 7-bit again (the prefix was used up)
    rig.run (1, { press (2, 0x40) });
    CHECK (rig.source (mod::SrcNotePressure) == doctest::Approx (64.0 / 127.0).epsilon (1.0e-6));
    // maximum: 0x3F80 = exactly 1.0 (not 0x3FFF); the 7-bit maximum is the same
    ev.clear();
    press14 (ev, 2, 0x3F80);
    rig.run (1, ev);
    CHECK (rig.source (mod::SrcNotePressure) == 1.0f);
    rig.run (1, { press (2, 127) });
    CHECK (rig.source (mod::SrcNotePressure) == 1.0f);
    ev.clear();
    press14 (ev, 2, 0x3FFF); // above the maximum: clamped
    rig.run (1, ev);
    CHECK (rig.source (mod::SrcNotePressure) == 1.0f);
    // fine steps: two 14-bit values one LSB apart differ (7-bit could not tell them apart)
    ev.clear();
    press14 (ev, 2, 0x1000);
    rig.run (1, ev);
    const float a = rig.source (mod::SrcNotePressure);
    ev.clear();
    press14 (ev, 2, 0x1001);
    rig.run (1, ev);
    CHECK (rig.source (mod::SrcNotePressure) - a == doctest::Approx (1.0 / (double) 0x3F80).epsilon (0.01));

    // 14-bit timbre (CC 74): 100 + 32/128
    ev.clear();
    ev.push_back (cc (2, 87, 32));
    ev.push_back (cc (2, 74, 100));
    rig.run (1, ev);
    CHECK (rig.source (mod::SrcSlide) == doctest::Approx ((100.25 - 64.0) / 63.0).epsilon (1.0e-6));
    rig.run (1, { cc (2, 74, 100) }); // 7-bit again
    CHECK (rig.source (mod::SrcSlide) == doctest::Approx ((100.0 - 64.0) / 63.0).epsilon (1.0e-6));

    // 21-bit bend: centre + 0.5 of one 14-bit step
    ev.clear();
    bend21 (ev, 2, (8192 << 7) + 64);
    rig.run (1, ev);
    CHECK (rig.source (mod::SrcNoteBend) == doctest::Approx (0.5 / 8191.0).epsilon (1.0e-3));
    CHECK (rig.source (mod::SrcKeyBend) * 60.0f == doctest::Approx (0.5 / 8192.0 * 96.0).epsilon (2.0e-2));
    rig.run (1, { bend (2, 8192) });
    CHECK (rig.source (mod::SrcNoteBend) == 0.0f);

    // the prefix belongs to its channel: a CC 87 on channel 3 does not change channel 2's next value
    rig.run (1, { cc (3, 87, 127), press (2, 10) });
    CHECK (rig.source (mod::SrcNotePressure) == doctest::Approx (10.0 / 127.0).epsilon (1.0e-6));

    // HD off: CC 87 is ignored (plain 7-bit)
    rig.set ("p1.mpe.hd", (float) MpeInput::HdOff);
    rig.run (1, { cc (2, 87, 64), press (2, 64) });
    CHECK (rig.source (mod::SrcNotePressure) == doctest::Approx (64.0 / 127.0).epsilon (1.0e-6));
}

TEST_CASE ("p50: NRPN 100..102 set the X / Y / Z cutoffs (2 Hz units); null NRPN; the Z cutoff shapes the level response")
{
    {
        Rig rig;
        CHECK (rig.mpe().cutoffHz (0) == 0.0f); // Auto, nothing received: the classic smoothing
        std::vector<MidiEvent> ev;
        nrpn (ev, 1, 100, 60);
        nrpn (ev, 1, 101, 30);
        nrpn (ev, 1, 102, 125);
        ev.push_back (cc (1, 6, 1)); // after the null NRPN: ignored
        rig.run (1, ev);
        CHECK (rig.mpe().cutoffHz (0) == 120.0f);
        CHECK (rig.mpe().cutoffHz (1) == 60.0f);
        CHECK (rig.mpe().cutoffHz (2) == 250.0f);
        CHECK ((rig.mpe().statusBits() & MpeInput::StatusCutoffs) != 0);
        CHECK (rig.mpe().receivedCutoff (2) == 250.0f);
        rig.set ("p1.mpe.hd", (float) MpeInput::HdOff);
        rig.run (1);
        CHECK (rig.mpe().cutoffHz (2) == 0.0f);
    }
    {
        Rig rig;
        rig.set ("p1.mpe.hd", (float) MpeInput::HdOn); // On: default cutoffs before anything arrives
        rig.run (1);
        CHECK (rig.mpe().cutoffHz (0) == MpeInput::defaultCutoffHz[0]);
        CHECK (rig.mpe().cutoffHz (1) == MpeInput::defaultCutoffHz[1]);
        CHECK (rig.mpe().cutoffHz (2) == MpeInput::defaultCutoffHz[2]);
    }
    // level 10 ms after the pressure jumps 0 -> max: fast with Z = 250 Hz, slow with Z = 2 Hz
    auto rise = [] (int zValue) {
        Rig rig;
        std::vector<MidiEvent> ev;
        nrpn (ev, 1, 102, zValue);
        ev.push_back (bend (2, 8192));
        ev.push_back (press (2, 0));
        ev.push_back (MidiEvent::noteOn (2, 57, 127));
        rig.run (20, ev);
        const size_t at = rig.rec.l.size();
        std::vector<MidiEvent> p;
        press14 (p, 2, 0x3F80);
        rig.run (40, p);
        const float early = rms (rig.rec.l, at, at + 480), late = rms (rig.rec.l, rig.rec.l.size() - 4800, rig.rec.l.size());
        CHECK (allFinite (rig.rec.l));
        CHECK (noDenormals (rig.rec.l));
        return std::array<float, 2> { early, late };
    };
    const auto fast = rise (125), slow = rise (1);
    CHECK (fast[1] > 0.01f);
    CHECK (fast[0] > 0.7f * fast[1]);
    CHECK (slow[0] < 0.3f * fast[0]);
}

TEST_CASE ("p50: RPN 0 sets the member bend range (96 semitones); RPN 6 selects the zone")
{
    Rig rig (false);
    rig.set ("p1.mpe.bend", 48.0f);
    std::vector<MidiEvent> ev;
    rpn (ev, 2, 0, 96);
    rig.run (1, ev);
    CHECK (rig.mpe().memberBendRange() == 96.0f);
    CHECK (rig.mpe().receivedBendRange() == 96);
    rig.run (1, { MidiEvent::noteOn (2, 60, 100), bend (2, 8192 + 1024) }); // +1/8 of 96 = +12 semitones
    CHECK (rig.source (mod::SrcKeyBend) * 60.0f == doctest::Approx (12.0).epsilon (1.0e-4));
    // HD off: the parameter applies again
    rig.set ("p1.mpe.hd", (float) MpeInput::HdOff);
    rig.run (1);
    CHECK (rig.mpe().memberBendRange() == 48.0f);
    rig.set ("p1.mpe.hd", (float) MpeInput::HdAuto);
    // upper zone: channel 16 is the master channel, its bend is the part's bend (not a note's)
    ev.clear();
    rpn (ev, 16, 6, 15);
    rig.run (1, ev);
    CHECK ((rig.mpe().statusBits() & MpeInput::StatusUpperZone) != 0);
    CHECK (rig.mpe().memberBendRange() == 48.0f); // a new zone starts at the configured range
    ev.clear();
    rpn (ev, 1, 6, 15); // back to the lower zone
    rig.run (1, ev);
    CHECK ((rig.mpe().statusBits() & MpeInput::StatusUpperZone) == 0);
}

TEST_CASE ("p50: a bend before the note starts the voice at the exact pitch (cents), 21-bit, range 96")
{
    Rig rig;
    std::vector<MidiEvent> ev;
    rpn (ev, 2, 0, 96);
    const int b = ((8192 + 43) << 7) + 64; // +43.5 / 8192 * 96 semitones = +0.5098 st
    bend21 (ev, 2, b);
    press14 (ev, 2, 0x3000);
    ev.push_back (MidiEvent::noteOn (2, 57, 127));
    rig.run (100, ev);
    const double st = 43.5 / 8192.0 * 96.0;
    const double want = 220.0 * std::pow (2.0, st / 12.0);
    CHECK (std::abs (centsBetween (measureFrequency (rig.rec.l, sampleRate, 240, 2400), want)) < 3.0); // from the start
    CHECK (std::abs (centsBetween (measureFrequency (rig.rec.l, sampleRate, 4800, rig.rec.l.size()), want)) < 0.5);

    // pitch rounding "initial": the same gesture starts on the note itself
    Rig round;
    round.set ("p1.mpe.round", 1.0f);
    round.run (100, ev);
    CHECK (std::abs (centsBetween (measureFrequency (round.rec.l, sampleRate, 240, 2400), 220.0 * std::pow (2.0, 1.0 / 12.0))) < 3.0);
    // continuous: also pulled to the nearest semitone (0.51 st rounds up to 58)
    Rig cont;
    cont.set ("p1.mpe.round", 2.0f);
    cont.set ("p1.mpe.round.time", 0.05f);
    cont.run (100, ev);
    CHECK (std::abs (centsBetween (measureFrequency (cont.rec.l, sampleRate, 9600, cont.rec.l.size()), 220.0 * std::pow (2.0, 1.0 / 12.0))) < 1.0);
}

TEST_CASE ("p50: pressure onset - velocity 127 is ignored, Z = 0 is silent, the level rises with the pressure")
{
    Rig rig;
    rig.run (20, { bend (2, 8192), press (2, 0), MidiEvent::noteOn (2, 57, 127) });
    CHECK (peak (rig.rec.l) < 1.0e-4f);
    float last = 0.0f;
    for (int v : { 0x0800, 0x1800, 0x2800, 0x3F80 })
    {
        std::vector<MidiEvent> ev;
        press14 (ev, 2, v);
        const size_t at = rig.rec.l.size();
        rig.run (20, ev);
        const float level = rms (rig.rec.l, at + 2400, rig.rec.l.size());
        CHECK (level > last * 1.2f);
        last = level;
    }
    CHECK (allFinite (rig.rec.l));
    CHECK (noDenormals (rig.rec.l));
    // Velocity mode: the same note sounds at once
    Rig vel (false);
    vel.run (20, { MidiEvent::noteOn (2, 57, 127) });
    CHECK (peak (vel.rec.l) > 0.01f);
}

TEST_CASE ("p50: a realistic stream - 7-bit onset burst, 14-bit sustain, a slide over two octaves - stays clean")
{
    Rig rig;
    std::vector<MidiEvent> ev;
    rpn (ev, 2, 0, 96);
    nrpn (ev, 2, 100, 60);
    nrpn (ev, 2, 101, 30);
    nrpn (ev, 2, 102, 125);
    ev.push_back (bend (2, 8192));
    ev.push_back (cc (2, 74, 64));
    ev.push_back (press (2, 0));
    ev.push_back (MidiEvent::noteOn (2, 48, 127));
    rig.run (1, ev);
    for (int v : { 20, 50, 90, 110 }) rig.run (1, { press (2, v) }); // onset burst (7-bit)
    // 14-bit sustain while sliding up 24 semitones (range 96: +2048 14-bit steps), timbre opening
    const int steps = 200;
    for (int i = 1; i <= steps; ++i)
    {
        std::vector<MidiEvent> s;
        const double t = (double) i / steps;
        bend21 (s, 2, (int) std::lround ((8192.0 + 2048.0 * t) * 128.0));
        s.push_back (cc (2, 87, (i * 37) & 0x7f));
        s.push_back (cc (2, 74, 64 + (int) (40.0 * t)));
        press14 (s, 2, 0x3000 + (int) (0x600 * std::sin (t * 9.0)));
        rig.run (1, s);
    }
    rig.run (60);
    CHECK (allFinite (rig.rec.l));
    CHECK (allFinite (rig.rec.r));
    CHECK (noDenormals (rig.rec.l));
    CHECK (peak (rig.rec.l) < 4.0f);
    const double f = measureFrequency (rig.rec.l, sampleRate, rig.rec.l.size() - 9600, rig.rec.l.size());
    CHECK (std::abs (centsBetween (f, 130.8128 * 4.0)) < 2.0); // C3 + two octaves
    CHECK (rig.source (mod::SrcKeyBend) == doctest::Approx ((48.0 + 24.0 - 60.0) / 60.0).epsilon (1.0e-3));
}

TEST_CASE ("p50: plain MPE is unchanged - Auto with a plain stream renders exactly like Off")
{
    auto render = [] (int hd) {
        Rig rig (false);
        rig.set ("p1.mpe.hd", (float) hd);
        rig.set ("p1.mpe.smooth", 0.02f);
        rig.set ("p1.mpe.slide", 0.01f);
        rig.run (2, { bend (2, 9000), MidiEvent::noteOn (2, 60, 90), press (2, 40), cc (2, 74, 90) });
        for (int i = 0; i < 30; ++i) rig.run (1, { press (2, (i * 5) % 128), bend (2, 8192 + i * 40), cc (2, 74, 64 + i) });
        rig.run (20, { MidiEvent::noteOff (2, 60) });
        return rig.rec.l;
    };
    const auto off = render (MpeInput::HdOff), autoMode = render (MpeInput::HdAuto);
    REQUIRE (off.size() == autoMode.size());
    float diff = 0.0f;
    for (size_t i = 0; i < off.size(); ++i) diff = std::max (diff, std::abs (off[i] - autoMode[i]));
    CHECK (diff == 0.0f);
    CHECK (rms (off) > 0.001f);
}

TEST_CASE ("p50: sostenuto (CC 66) holds the keys that were down; sustain on the master channel holds every note")
{
    Rig rig (false);
    rig.set ("p1.amp.r", 0.01f);
    rig.run (4, { MidiEvent::noteOn (2, 60, 100), cc (1, 66, 127), MidiEvent::noteOn (3, 64, 100) });
    rig.run (4, { MidiEvent::noteOff (2, 60), MidiEvent::noteOff (3, 64) });
    CHECK (rig.core.part (0).sostenutoPedal());
    rig.run (20);
    CHECK (rig.core.part (0).modMatrix().sourceValue (mod::SrcKeyTrack) == doctest::Approx ((64.0 - 60.0) / 60.0));
    CHECK (rms (rig.rec.l, rig.rec.l.size() - 2400, rig.rec.l.size()) > 0.001f); // 60 still sounds
    rig.run (40, { cc (1, 66, 0) });
    CHECK (rms (rig.rec.l, rig.rec.l.size() - 2400, rig.rec.l.size()) < 1.0e-4f);
    // master-channel sustain
    rig.run (4, { cc (1, 64, 127), MidiEvent::noteOn (4, 67, 100) });
    rig.run (40, { MidiEvent::noteOff (4, 67) });
    CHECK (rms (rig.rec.l, rig.rec.l.size() - 2400, rig.rec.l.size()) > 0.001f);
    rig.run (40, { cc (1, 64, 0) });
    CHECK (rms (rig.rec.l, rig.rec.l.size() - 2400, rig.rec.l.size()) < 1.0e-4f);
}

TEST_CASE ("p50: every engine stays finite with HD smoothing, pressure onset, rounding and a pressure curve")
{
    for (int engine = 0; engine < numEngineTypes; ++engine)
    {
        CAPTURE (engine);
        Rig rig;
        rig.set ("p1.engine", (float) engine);
        rig.set ("p1.mpe.hd", (float) MpeInput::HdOn);
        rig.set ("p1.mpe.round", 2.0f);
        rig.set ("p1.mpe.pcurve", 0.7f);
        rig.set ("p1.mpe.press", (float) MpeInput::Cutoff);
        rig.set ("p1.mod.s1.src", (float) mod::SrcKeyBend);
        rig.set ("p1.mod.s1.dst", (float) mod::DstTimbre);
        rig.set ("p1.mod.s1.amt", 1.0f);
        std::vector<MidiEvent> ev;
        bend21 (ev, 2, 0x1FFF80);
        press14 (ev, 2, 0x3F80);
        ev.push_back (cc (2, 87, 127));
        ev.push_back (cc (2, 74, 127));
        ev.push_back (MidiEvent::noteOn (2, 60, 127));
        rig.run (16, ev);
        rig.run (16, { bend (2, 0), press (2, 0), MidiEvent::noteOff (2, 60) });
        CHECK (allFinite (rig.rec.l));
        CHECK (allFinite (rig.rec.r));
        CHECK (noDenormals (rig.rec.l));
        CHECK (peak (rig.rec.l) < 8.0f);
    }
}

TEST_CASE ("p50: state round trip; a part in Fingerboard mode keeps its controller setup when a sound loads")
{
    Core a;
    a.params().set ("p3.mpe", 1.0f);
    a.params().set ("p3.mpe.bend", 96.0f);
    a.params().set ("p3.mpe.hd", (float) MpeInput::HdOn);
    a.params().set ("p3.mpe.onset", (float) MpeInput::OnsetBoth);
    a.params().set ("p3.mpe.round", 2.0f);
    a.params().set ("p3.mpe.round.time", 0.3f);
    a.params().set ("p3.mpe.pcurve", -0.4f);
    const auto json = a.saveState();
    Core b;
    REQUIRE (b.loadState (json));
    auto get = [&b] (const char* id) { return b.params().get (b.params().indexOf (id)); };
    CHECK (get ("p3.mpe.hd") == (float) MpeInput::HdOn);
    CHECK (get ("p3.mpe.onset") == (float) MpeInput::OnsetBoth);
    CHECK (get ("p3.mpe.round") == 2.0f);
    CHECK (get ("p3.mpe.round.time") == doctest::Approx (0.3f));
    CHECK (get ("p3.mpe.pcurve") == doctest::Approx (-0.4f));

    // a plain sound (MPE bend 48 in it) loads into the fingerboard part: MPE, bend 96, HD and onset stay
    preset::Preset plain;
    plain.meta.kind = preset::Kind::Part;
    plain.params["engine"] = (float) EngineType::Analog;
    plain.params["mpe.bend"] = 48.0f;
    REQUIRE (preset::applyPart (plain, b.params(), 2).ok);
    CHECK (get ("p3.mpe") == 1.0f);
    CHECK (get ("p3.mpe.bend") == 96.0f);
    CHECK (get ("p3.mpe.onset") == (float) MpeInput::OnsetBoth);
    CHECK (get ("p3.mpe.round") == 0.0f); // a sound setting: back to its default
    // a part in plain MPE / velocity mode resets as before
    REQUIRE (preset::applyPart (plain, b.params(), 0).ok);
    CHECK (b.params().get (b.params().indexOf ("p1.mpe.bend")) == 48.0f);
    CHECK (b.params().get (b.params().indexOf ("p1.mpe")) == 0.0f);
}
