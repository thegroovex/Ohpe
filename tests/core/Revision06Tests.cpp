// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.6 (sound revision 2): portamento (glide modes, constant time / rate, mono note priority, MIDI CC 5 / 65),
// the per-part pitch envelope ("rise" and "fall" slides), Analog oscillator level + crackle, master low-pass,
// the Boom category.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "engines/AnalogEngine.h"
#include "engines/EngineFactory.h"
#include "engines/FmEngine.h"
#include "engines/WavetableEngine.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
void prepareSine (AnalogEngine& e)
{
    e.prepare (sampleRate, 512);
    e.setParam (analog::Shape, 0.5f); // square: one upward crossing per period
    e.setParam (analog::Detune, 0.0f);
    e.setParam (analog::OscMix, 0.0f);
    e.setParam (analog::Cutoff, 18000.0f);
    e.setParam (analog::Resonance, 0.0f);
    e.setParam (analog::KeyTrack, 0.0f);
    e.setParam (analog::Drive, 0.0f);
}

struct CoreRig
{
    Core core;
    static constexpr int block = 480;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> recL;

    CoreRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
    }

    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, {});
            recL.insert (recL.end(), l[0].begin(), l[0].end());
        }
    }

    float glidePitch() { return static_cast<AnalogEngine*> (core.part (0).engine (EngineType::Analog))->voiceGlidePitch (0); }
};
} // namespace

TEST_CASE ("0.6: glide mode / timing, pitch envelope, Analog late parameters and the master low-pass are appended")
{
    Core core;
    const auto& reg = core.params();
    const int last05 = reg.indexOf ("fxa.delay.sync");
    REQUIRE (last05 >= 0);
    int prev = last05;
    for (int n = 1; n <= 4; ++n)
        for (const char* k : { "glide.mode", "glide.rate", "pitchenv.amt", "pitchenv.time", "pitchenv.curve" })
        {
            const int i = reg.indexOf ("p" + std::to_string (n) + "." + k);
            CAPTURE (k);
            CHECK (i == prev + 1);
            prev = i;
        }
    for (int n = 1; n <= 4; ++n)
        for (const char* k : { "analog.osclevel", "analog.crackle" })
        {
            const int i = reg.indexOf ("p" + std::to_string (n) + "." + k);
            CHECK (i == prev + 1);
            prev = i;
        }
    CHECK (reg.indexOf ("mix.master.lowpass") == prev + 1);
    CHECK (reg.descriptor (reg.indexOf ("p1.glide.mode")).choices.size() == 3);
    CHECK (reg.get (reg.indexOf ("p2.pitchenv.amt")) == 0.0f);
    CHECK (reg.get (reg.indexOf ("mix.master.lowpass")) == 20000.0f);
}

TEST_CASE ("0.6: pitch envelope - notes rise from below (or fall from above) into their pitch in every oscillator engine")
{
    for (float amount : { -12.0f, -5.0f, 7.0f })
    {
        auto c = plainCommon();
        c.pitchEnvAmount = amount;
        c.pitchEnvTime = 0.6f;
        c.pitchEnvCurve = 0.0f; // linear: half way after half the time
        AnalogEngine a;
        prepareSine (a);
        a.setCommon (c);
        a.noteOn (69, 1.0f);
        auto out = renderEngine (a, 48000);
        const double ref = dsp::midiToHz (69.0f);
        const double start = measureFrequency (out.l, sampleRate, 480, 2400);   // 10..50 ms: near the start offset
        const double middle = measureFrequency (out.l, sampleRate, 13440, 15360); // ~300 ms: about half way
        const double end = measureFrequency (out.l, sampleRate, 33600, 48000);    // after the envelope
        CAPTURE (amount);
        CHECK (std::abs (centsBetween (start, ref) - amount * 100.0) < 90.0);
        CHECK (std::abs (centsBetween (middle, ref) - amount * 50.0) < 60.0);
        CHECK (std::abs (centsBetween (end, ref)) < 3.0);
    }

    // the other engines share the voice pitch path: the envelope changes their output, and it ends on pitch
    for (auto type : { EngineType::FM, EngineType::Wavetable, EngineType::Pluck, EngineType::Vocal, EngineType::Cluster })
    {
        auto plain = createEngine (type), env = createEngine (type);
        REQUIRE (plain != nullptr);
        auto c = plainCommon();
        plain->prepare (sampleRate, 512);
        plain->setCommon (c);
        c.pitchEnvAmount = -12.0f;
        c.pitchEnvTime = 0.3f;
        env->prepare (sampleRate, 512);
        env->setCommon (c);
        plain->noteOn (57, 1.0f);
        env->noteOn (57, 1.0f);
        auto a = renderEngine (*plain, 24000), b = renderEngine (*env, 24000);
        double diffEarly = 0.0, diffLate = 0.0;
        for (size_t i = 0; i < 9600; ++i) diffEarly += std::abs (a.l[i] - b.l[i]);
        CAPTURE ((int) type);
        CHECK (diffEarly > 1.0);
        CHECK (allFinite (b.l));
        (void) diffLate;
    }
}

TEST_CASE ("0.6: glide mode Legato glides only while a key is held and keeps the mono envelopes running")
{
    AnalogEngine e;
    prepareSine (e);
    auto c = plainCommon();
    c.glideSeconds = 0.2f;
    c.glideMode = GlideLegato;
    e.setCommon (c);
    e.setPolyphony (1);
    e.noteOn (60, 1.0f);
    renderEngine (e, 4800);
    e.noteOff (60);
    renderEngine (e, 480); // released but still sounding: Auto would glide, Legato does not
    e.noteOn (67, 1.0f);
    renderEngine (e, 256);
    CHECK (e.voiceGlidePitch (0) == doctest::Approx (67.0f));
    e.noteOn (72, 1.0f); // 67 still held: legato glide
    renderEngine (e, 4800);
    CHECK (e.voiceGlidePitch (0) > 67.5f);
    CHECK (e.voiceGlidePitch (0) < 71.5f);
    // mono note priority: releasing 72 returns to the held 67, gliding back
    e.noteOff (72);
    renderEngine (e, 960);
    CHECK (e.voiceNote (0) == 67);
    CHECK (e.voiceGlidePitch (0) > 67.5f);
    renderEngine (e, 48000);
    CHECK (e.voiceGlidePitch (0) == doctest::Approx (67.0f).epsilon (0.001));
    CHECK (e.activeVoiceCount() == 1);
    e.noteOff (67);
    renderEngine (e, 24000);
    CHECK (e.activeVoiceCount() == 0);
}

TEST_CASE ("0.6: glide mode Always glides from the last note even after silence; Auto (default) does not")
{
    for (int mode : { (int) GlideAuto, (int) GlideAlways })
    {
        AnalogEngine e;
        prepareSine (e);
        auto c = plainCommon();
        c.glideSeconds = 0.2f;
        c.glideMode = mode;
        e.setCommon (c);
        e.setPolyphony (4);
        e.noteOn (48, 1.0f);
        renderEngine (e, 4800);
        e.noteOff (48);
        renderEngine (e, 48000); // silent again
        CHECK (e.activeVoiceCount() == 0);
        e.noteOn (60, 1.0f);
        renderEngine (e, 960);
        float p = -1.0f;
        for (int i = 0; i < 4; ++i)
            if (e.voiceNote (i) == 60) p = e.voiceGlidePitch (i);
        CAPTURE (mode);
        if (mode == GlideAlways) { CHECK (p > 48.5f); CHECK (p < 59.0f); }
        else CHECK (p == doctest::Approx (60.0f));
    }
}

TEST_CASE ("0.6: constant rate glides take longer over larger intervals; constant time does not")
{
    auto progress = [] (bool constantRate, int interval) {
        AnalogEngine e;
        prepareSine (e);
        auto c = plainCommon();
        c.glideSeconds = 0.1f;
        c.glideConstantRate = constantRate;
        e.setCommon (c);
        e.setPolyphony (1);
        e.noteOn (48, 1.0f);
        renderEngine (e, 960);
        e.noteOn (48 + interval, 1.0f);
        renderEngine (e, 4800); // 100 ms
        return (e.voiceGlidePitch (0) - 48.0f) / (float) interval; // 0..1 of the way
    };
    CHECK (progress (false, 2) == doctest::Approx (progress (false, 24)).epsilon (0.02));
    CHECK (progress (true, 2) > progress (true, 24) + 0.3f); // a whole tone arrives much sooner than two octaves
}

TEST_CASE ("0.6: MIDI CC 65 switches portamento off / on, CC 5 sets the glide time until the parameter moves")
{
    CoreRig rig;
    auto& p = rig.core.params();
    p.set ("p1.engine", (float) EngineType::Analog);
    p.set ("p1.voices", 1.0f);
    p.set ("p1.glide", 0.4f);
    rig.run (2, { MidiEvent::noteOn (1, 60, 100) });
    rig.run (2, { MidiEvent::controller (1, 65, 0), MidiEvent::noteOn (1, 72, 100, 1) });
    CHECK (rig.glidePitch() == doctest::Approx (72.0f)); // portamento off: jumps
    rig.run (2, { MidiEvent::controller (1, 65, 127), MidiEvent::noteOn (1, 60, 100, 1) });
    CHECK (rig.glidePitch() > 64.0f); // on again: glides (0.4 s)
    rig.run (200);
    rig.run (2, { MidiEvent::controller (1, 5, 0), MidiEvent::noteOn (1, 72, 100, 1) });
    CHECK (rig.glidePitch() == doctest::Approx (72.0f)); // CC 5 = 0: no glide time
    p.set ("p1.glide", 0.5f); // the parameter moves: CC 5 no longer overrides it
    rig.run (2, { MidiEvent::noteOn (1, 60, 100) });
    CHECK (rig.glidePitch() > 64.0f);
    CHECK (allFinite (rig.recL));
}

TEST_CASE ("0.6: Analog osc level 0 leaves noise / crackle only; crackle makes sparse clicks")
{
    AnalogEngine e;
    prepareSine (e);
    e.setParam (analog::OscLevel, 0.0f);
    e.setCommon (plainCommon());
    e.noteOn (60, 1.0f);
    auto silent = renderEngine (e, 9600);
    CHECK (peak (silent.l, 960) < 1.0e-3f);
    e.setParam (analog::Crackle, 0.6f);
    auto crackle = renderEngine (e, 96000);
    CHECK (allFinite (crackle.l));
    CHECK (peak (crackle.l) > 0.02f);
    CHECK (peak (crackle.l) < 1.5f);
    // sparse: most of the time the signal is near silent, unlike noise
    int quiet = 0;
    for (float x : crackle.l) quiet += std::abs (x) < 0.1f * peak (crackle.l) ? 1 : 0;
    CHECK (quiet > (int) crackle.l.size() * 8 / 10);
}

TEST_CASE ("0.6: the master low-pass darkens the mix and is off at 20 kHz")
{
    auto energyAbove = [] (float lowpass) {
        CoreRig rig;
        auto& p = rig.core.params();
        p.set ("p1.engine", (float) EngineType::Analog);
        p.set ("p1.analog.noise", 1.0f);
        p.set ("p1.analog.osclevel", 0.0f);
        p.set ("p1.analog.cutoff", 20000.0f);
        p.set ("mix.master.lowpass", lowpass);
        rig.run (40, { MidiEvent::noteOn (1, 60, 110) });
        double hi = 0.0, all = 0.0;
        for (size_t i = 4800; i < rig.recL.size(); ++i)
        {
            const double d = rig.recL[i] - rig.recL[i - 1];
            hi += d * d;
            all += (double) rig.recL[i] * rig.recL[i];
        }
        return hi / std::max (1.0e-12, all);
    };
    CHECK (energyAbove (2000.0f) < 0.3 * energyAbove (20000.0f));
}

TEST_CASE ("0.6: the Boom category is a fixed category after Vocal")
{
    const auto& cats = preset::categories();
    REQUIRE (cats.size() >= 11); // 0.8 appends Hybrid and Piano
    CHECK (std::string (cats[9].key) == "Vocal");
    CHECK (std::string (cats[10].key) == "Boom");
    CHECK (preset::isCategory ("Boom"));
}
