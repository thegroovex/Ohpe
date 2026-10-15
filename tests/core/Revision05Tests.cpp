// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.5 (sound revision 1): swing and humanise for the Orbit and Euclid sequencers, poly glide
// (chords glide note by note from the chord before), the synthesised Boom drum kit.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "engines/AnalogEngine.h"
#include "engines/VoiceAllocator.h"
#include "sample/FactoryBank.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct Hit
{
    long long time;
    int note, velocity;
};

std::vector<Hit> runOns (ISequencer& seq, long long samples, int block = 480, double bpm = 120.0)
{
    std::vector<Hit> hits;
    SequencerOutput out;
    Transport tr;
    tr.bpm = bpm;
    tr.playing = true;
    tr.hostDriven = true;
    for (long long pos = 0; pos < samples; pos += block)
    {
        tr.ppqAtBlockStart = (double) pos / sampleRate * bpm / 60.0;
        out.clear();
        seq.process (tr, block, out);
        for (const auto& e : out.events)
            if (e.isNoteOn() && pos + e.sampleOffset < samples) hits.push_back ({ pos + e.sampleOffset, e.data1, e.data2 });
    }
    return hits;
}

void orbitSixteenths (OrbitSequencer& o)
{
    using P = OrbitSequencer;
    o.prepare (sampleRate, 480);
    o.setParam (P::On, 1.0f);
    o.setParam (P::firstLaneParam + P::Length, 16.0f);
    o.setParam (P::firstLaneParam + P::Rate, (float) seq::R1_16);
    for (int l = 1; l < P::numLanes; ++l) o.setParam (P::firstLaneParam + l * P::numLaneParams + P::Length, 1.0f);
    for (int i = 0; i < 16; ++i) o.setStep (0, i, { true, 60, 100 });
}
} // namespace

TEST_CASE ("0.5: Orbit and Euclid register swing and humanise at the end of the layout")
{
    Core core;
    for (const char* id : { "seq.orbit.swing", "seq.orbit.human", "seq.euclid.swing", "seq.euclid.human", "p1.polyglide", "p4.polyglide" })
        CHECK (core.params().indexOf (id) >= 0);
    CHECK (core.params().indexOf ("seq.orbit.swing") > core.params().indexOf ("p4.gain"));
    CHECK (OrbitSequencer().getLateParamDescriptors().size() == 6);
    CHECK (EuclidSequencer().getLateParamDescriptors().size() == 6);
}

TEST_CASE ("0.5: Orbit swing delays every second sixteenth by swing / 2 of a step")
{
    OrbitSequencer o;
    orbitSixteenths (o);
    o.setParam (OrbitSequencer::Swing, 0.32f); // ~58 % swing
    const auto hits = runOns (o, 48000);       // 8 sixteenths at 120 BPM (6000 samples each)
    REQUIRE (hits.size() == 8);
    for (int i = 0; i < 8; ++i)
    {
        const long long expected = i * 6000 + ((i & 1) ? (long long) std::llround (0.32 * 0.5 * 6000.0) : 0);
        CHECK (hits[(size_t) i].time == expected);
        CHECK (hits[(size_t) i].velocity == 100); // no humanise: velocities untouched
    }
}

TEST_CASE ("0.5: humanise moves notes late by at most 10 ms and velocities by at most 16, repeatably")
{
    OrbitSequencer a, b;
    orbitSixteenths (a);
    orbitSixteenths (b);
    a.setParam (OrbitSequencer::Human, 1.0f);
    b.setParam (OrbitSequencer::Human, 1.0f);
    const auto ha = runOns (a, 96000), hb = runOns (b, 96000);
    REQUIRE (ha.size() == 16);
    REQUIRE (hb.size() == ha.size());
    int moved = 0, velChanged = 0;
    for (size_t i = 0; i < ha.size(); ++i)
    {
        const long long grid = (long long) i * 6000;
        CHECK (ha[i].time >= grid);
        CHECK (ha[i].time <= grid + 480); // 10 ms at 48 kHz
        CHECK (std::abs (ha[i].velocity - 100) <= 16);
        moved += ha[i].time != grid ? 1 : 0;
        velChanged += ha[i].velocity != 100 ? 1 : 0;
        CHECK (ha[i].time == hb[i].time); // deterministic
        CHECK (ha[i].velocity == hb[i].velocity);
    }
    CHECK (moved >= 10);
    CHECK (velChanged >= 10);
}

TEST_CASE ("0.5: Euclid swing and humanise")
{
    using E = EuclidSequencer;
    E e;
    e.prepare (sampleRate, 480);
    e.setParam (E::On, 1.0f);
    e.setParam (E::Rate, (float) seq::R1_16);
    e.setParam (E::firstRingParam + E::Steps, 16.0f);
    e.setParam (E::firstRingParam + E::Pulses, 16.0f);
    e.setParam (E::firstRingParam + E::Note, 36.0f);
    e.setParam (E::Swing, 0.5f);
    auto hits = runOns (e, 24000);
    REQUIRE (hits.size() == 4);
    CHECK (hits[0].time == 0);
    CHECK (hits[1].time == 6000 + 1500);
    CHECK (hits[2].time == 12000);
    e.reset();
    e.setParam (E::Swing, 0.0f);
    e.setParam (E::Human, 0.5f);
    hits = runOns (e, 48000);
    REQUIRE (hits.size() == 8);
    for (size_t i = 0; i < hits.size(); ++i)
    {
        CHECK (hits[i].time >= (long long) i * 6000);
        CHECK (hits[i].time <= (long long) i * 6000 + 240);
        CHECK (std::abs (hits[i].velocity - 100) <= 8);
    }
}

TEST_CASE ("0.5: allocateNearest hands a new note the released slot nearest in pitch")
{
    VoiceAllocator<8> a;
    a.setPolyphony (4);
    bool stolen = false;
    const float pitch[4] = { 60.0f, 64.0f, 67.0f, 0.0f };
    for (int n : { 60, 64, 67 }) a.allocate (n, stolen);
    a.release (60, [] (int) {});
    a.release (64, [] (int) {});
    a.release (67, [] (int) {});
    auto pitchOf = [&] (int i) { return pitch[i]; };
    CHECK (a.allocateNearest (65, stolen, pitchOf) == 1); // 64 is nearest
    CHECK (stolen);
    CHECK (a.allocateNearest (69, stolen, pitchOf) == 2); // 67 (64 is held again)
    CHECK (a.allocateNearest (59, stolen, pitchOf) == 0);
    CHECK (a.allocateNearest (72, stolen, pitchOf) == 3); // nothing released left: an idle slot
    CHECK_FALSE (stolen);
}

TEST_CASE ("0.5: poly glide - each voice of the next chord glides from the nearest note of the last one")
{
    auto play = [] (bool polyGlide, AnalogEngine& e) {
        e.prepare (sampleRate, 512);
        auto c = plainCommon();
        c.glideSeconds = 0.25f;
        c.polyGlide = polyGlide;
        c.amp = { 0.01f, 0.5f, 0.8f, 0.8f };
        e.setCommon (c);
        e.setPolyphony (6);
        for (int n : { 60, 64, 67 }) e.noteOn (n, 0.8f);
        renderEngine (e, 9600);
        for (int n : { 60, 64, 67 }) e.noteOff (n);
        renderEngine (e, 480);
        for (int n : { 62, 65, 69 }) e.noteOn (n, 0.8f);
        renderEngine (e, 960); // 20 ms into a 250 ms glide
    };
    AnalogEngine glide, plain;
    play (true, glide);
    play (false, plain);
    std::map<int, float> g, p;
    for (int i = 0; i < 6; ++i)
    {
        if (glide.voiceNote (i) >= 62) g[glide.voiceNote (i)] = glide.voiceGlidePitch (i);
        if (plain.voiceNote (i) >= 62 && plain.voiceNote (i) != 64 && plain.voiceNote (i) != 67) p[plain.voiceNote (i)] = plain.voiceGlidePitch (i);
    }
    REQUIRE (g.size() == 3);
    CHECK (g[62] > 60.0f);
    CHECK (g[62] < 61.9f); // still on its way up from 60
    CHECK (g[65] > 64.0f);
    CHECK (g[65] < 64.95f);
    CHECK (g[69] > 67.0f);
    CHECK (g[69] < 68.9f);
    REQUIRE (p.size() == 3);
    for (const auto& [note, pitch] : p) CHECK (pitch == doctest::Approx ((float) note)); // without poly glide: no slide
    CHECK (glide.activeVoiceCount() == 3); // the gliding voices took over the released ones
}

TEST_CASE ("0.5: the Boom kit is factory kit 5")
{
    CHECK (factory::numKits == 6);
    CHECK (std::string (factory::kitId (5)) == "boom");
    CHECK (factory::kitNamesEn().back() == "Boom");
    Core core;
    const int idx = core.params().indexOf ("drum.kit");
    REQUIRE (idx >= 0);
    CHECK (core.params().descriptor (idx).choices.size() == 6);
}

TEST_CASE ("0.5: swing moves only off-beat sixteenths - lanes at 1/8 are never swung")
{
    OrbitSequencer o;
    orbitSixteenths (o);
    using P = OrbitSequencer;
    o.setParam (P::firstLaneParam + P::Rate, (float) seq::R1_8);
    o.setParam (P::Swing, 0.5f);
    const auto hits = runOns (o, 48000);
    REQUIRE (hits.size() == 4);
    for (int i = 0; i < 4; ++i) CHECK (hits[(size_t) i].time == i * 12000);
}

TEST_CASE ("0.5: Euclid ring velocities (0 follows the Euclid velocity)")
{
    using E = EuclidSequencer;
    E e;
    e.prepare (sampleRate, 480);
    e.setParam (E::On, 1.0f);
    e.setParam (E::Velocity, 90.0f);
    for (int r = 0; r < 2; ++r)
    {
        e.setParam (E::firstRingParam + r * E::numRingParams + E::Steps, 4.0f);
        e.setParam (E::firstRingParam + r * E::numRingParams + E::Pulses, 1.0f);
        e.setParam (E::firstRingParam + r * E::numRingParams + E::Note, 36.0f + r);
    }
    e.setParam (E::RingVelocity1, 120.0f);
    const auto hits = runOns (e, 12000);
    std::map<int, int> vel;
    for (const auto& h : hits) vel[h.note] = h.velocity;
    CHECK (vel[36] == 120);
    CHECK (vel[37] == 90);
}

TEST_CASE ("0.5: Orbit lanes with their own target part; gate 100 % ties equal notes (not on the drum kit)")
{
    Core core;
    core.prepare (sampleRate, 480);
    auto& p = core.params();
    p.set ("tempo", 120.0f);
    p.set ("seq.orbit.on", 1.0f);
    p.set ("seq.orbit.part", 1.0f);
    p.set ("seq.orbit.drums", 1.0f);
    p.set ("seq.orbit.gate", 1.0f);
    p.set ("seq.orbit.l2.part", 3.0f);
    for (int l = 0; l < 4; ++l) p.set ("seq.orbit.l" + std::to_string (l + 1) + ".len", 4.0f);
    auto& o = core.orbitSequencer();
    for (int i = 0; i < 4; ++i)
    {
        o.setStep (0, i, { true, 42, 100 }); // drum lane: closed hat four times (one-shots, never tied)
        o.setStep (1, i, { i < 3, 60, 100 }); // part 3: one note tied over three steps
    }
    p.set ("sync.notesout", 1.0f);
    p.set ("seq.run", 1.0f);
    int partNotes = 0, drumHits = 0;
    long long firstOff = -1;
    std::vector<float> l (480), r (480);
    for (long long pos = 0; pos < 24000; pos += 480)
    {
        OutputBuffers out;
        out.left[0] = l.data();
        out.right[0] = r.data();
        core.process (out, 480, nullptr, 0, Transport {});
        for (const auto& e : core.midiOutput())
        {
            if (e.channel() == 3 && e.isNoteOn()) ++partNotes;
            if (e.channel() == 3 && e.isNoteOff() && firstOff < 0) firstOff = pos + e.sampleOffset;
            if (e.channel() == Core::drumChannel && e.isNoteOn()) ++drumHits;
        }
    }
    CHECK (partNotes == 1);
    CHECK (firstOff == 18001); // three sixteenths at 120 BPM (+ the one-sample legato overlap of gate 100 %, 0.6)
    CHECK (drumHits == 4);
}

TEST_CASE ("0.5: step gate 100 % ties equal notes; a different note or a shorter gate retriggers")
{
    StepSequencer seq;
    seq.prepare (sampleRate, 480);
    auto put = [&] (int i, int note, float gate) {
        StepSequencer::Step s;
        s.on = true;
        s.note = note;
        s.gate = gate;
        seq.setStep (i, s);
    };
    put (0, 60, 1.0f);
    put (1, 60, 1.0f);
    put (2, 60, 1.0f);
    put (3, 62, 1.0f);
    put (4, 62, 0.5f);
    put (5, 62, 0.5f);
    seq.setParam (StepSequencer::Length, 8.0f);
    SequencerOutput out;
    Transport tr;
    tr.bpm = 120.0;
    tr.playing = true;
    tr.hostDriven = true;
    std::vector<std::pair<long long, MidiEvent>> ev;
    for (long long pos = 0; pos < 48000; pos += 480)
    {
        tr.ppqAtBlockStart = (double) pos / sampleRate * 2.0;
        out.clear();
        seq.process (tr, 480, out);
        for (const auto& e : out.events) ev.push_back ({ pos + e.sampleOffset, e });
    }
    std::vector<long long> on60, off60, on62;
    for (const auto& [t, e] : ev)
    {
        if (e.data1 == 60 && e.isNoteOn()) on60.push_back (t);
        if (e.data1 == 60 && e.isNoteOff()) off60.push_back (t);
        if (e.data1 == 62 && e.isNoteOn()) on62.push_back (t);
    }
    CHECK (on60 == std::vector<long long> { 0 });
    CHECK (off60 == std::vector<long long> { 18001 }); // 0.6: gate 100 % overlaps the next note by one sample (legato)
    CHECK (on62 == std::vector<long long> { 18000, 30000 }); // step 3 (100 %) ties into step 4 (50 %), step 5 is a new note
    seq.setDrumsMode (true); // on the drum kit every step plays
    seq.reset();
    int ons = 0;
    for (long long pos = 0; pos < 24000; pos += 480)
    {
        tr.ppqAtBlockStart = (double) pos / sampleRate * 2.0;
        out.clear();
        seq.process (tr, 480, out);
        for (const auto& e : out.events) ons += e.isNoteOn() ? 1 : 0;
    }
    CHECK (ons == 4);
}

TEST_CASE ("0.5: the send delay follows the tempo when synced")
{
    CHECK (Mixer::delaySyncBeats (0) == 0.0);
    CHECK (Mixer::delaySyncBeats (5) == doctest::Approx (0.75)); // 1/8.
    CHECK (Mixer::delaySyncBeats (6) == doctest::Approx (1.0));
    Core core;
    const int idx = core.params().indexOf ("fxa.delay.sync");
    REQUIRE (idx >= 0);
    CHECK (core.params().descriptor (idx).choices.size() == 9);
}
