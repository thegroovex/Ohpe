// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Package D (0.2): Euclid, Arp, Orbit, scale / chord mode, step patterns and song mode,
// host-ppq timing, MIDI clock in / out, MPE, multiple parts, state.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "../TestSlice.h"

#include <doctest/doctest.h>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct Hit
{
    long long time;
    int note;
    bool on;
    int channel;
};

using Feed = std::function<void (ISequencer&, long long blockStart, int block)>;

/** Runs a sequencer with a transport at `bpm` starting at `ppq0` and collects note events (absolute sample times). */
std::vector<Hit> runSeq (ISequencer& seq, long long samples, int block = 512, double bpm = 120.0, double ppq0 = 0.0,
                         bool playing = true, Feed feed = {})
{
    std::vector<Hit> hits;
    SequencerOutput out;
    Transport tr;
    tr.bpm = bpm;
    tr.playing = playing;
    tr.hostDriven = true;
    for (long long pos = 0; pos < samples; pos += block)
    {
        tr.ppqAtBlockStart = ppq0 + (double) pos / sampleRate * bpm / 60.0;
        if (feed) feed (seq, pos, block);
        out.clear();
        seq.process (tr, block, out);
        for (const auto& e : out.events)
            if (pos + e.sampleOffset < samples) hits.push_back ({ pos + e.sampleOffset, e.data1, e.isNoteOn(), e.channel() });
    }
    return hits;
}

std::vector<Hit> onsOnly (const std::vector<Hit>& h)
{
    std::vector<Hit> r;
    for (const auto& x : h)
        if (x.on) r.push_back (x);
    return r;
}

std::string bits (uint32_t mask, int steps)
{
    std::string s;
    for (int i = 0; i < steps; ++i) s += ((mask >> i) & 1u) ? 'x' : '.';
    return s;
}

void setParams (ISequencer& s, const std::vector<std::pair<int, float>>& values)
{
    for (const auto& [i, v] : values) s.setParam (i, v);
}

/** Core with all outputs recorded, MIDI output collected with absolute times. */
struct Rig
{
    Core core;
    int block;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::array<std::vector<float>, numOutputPairs> recL;
    std::vector<Hit> midiOut;
    std::vector<std::pair<long long, MidiEvent>> rawOut;
    long long time = 0;

    explicit Rig (int blockSize = 480) : block (blockSize)
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign ((size_t) block, 0.0f);
        for (auto& v : r) v.assign ((size_t) block, 0.0f);
    }

    void run (int blocks, std::vector<MidiEvent> firstBlockMidi = {}, std::function<Transport (long long)> transport = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            const Transport tr = transport ? transport (time) : Transport {};
            core.process (out, block, b == 0 ? firstBlockMidi.data() : nullptr, b == 0 ? (int) firstBlockMidi.size() : 0, tr);
            for (int o = 0; o < numOutputPairs; ++o) recL[(size_t) o].insert (recL[(size_t) o].end(), l[(size_t) o].begin(), l[(size_t) o].end());
            for (const auto& e : core.midiOutput())
            {
                rawOut.push_back ({ time + e.sampleOffset, e });
                if (e.isNoteOn() || e.isNoteOff()) midiOut.push_back ({ time + e.sampleOffset, e.data1, e.isNoteOn(), e.channel() });
            }
            time += block;
        }
    }

    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }
};

Transport hostAt (double bpm, double ppq0, long long t)
{
    Transport tr;
    tr.bpm = bpm;
    tr.playing = true;
    tr.hostDriven = true;
    tr.ppqAtBlockStart = ppq0 + (double) t / sampleRate * bpm / 60.0;
    return tr;
}
} // namespace

// ================================================================ Euclid
TEST_CASE ("euclid: Bjorklund patterns match the published references")
{
    // Toussaint, "The Euclidean Algorithm Generates Traditional Musical Rhythms" (2005)
    const std::vector<std::tuple<int, int, const char*>> refs {
        { 2, 5, "x.x.." }, { 3, 4, "x.xx" }, { 3, 5, "x.x.x" }, { 3, 7, "x.x.x.." }, { 3, 8, "x..x..x." },
        { 4, 7, "x.x.x.x" }, { 4, 9, "x.x.x.x.." }, { 4, 11, "x..x..x..x." }, { 5, 6, "x.xxxx" }, { 5, 7, "x.xx.xx" },
        { 5, 8, "x.xx.xx." }, { 5, 9, "x.x.x.x.x" }, { 5, 11, "x.x.x.x.x.." }, { 5, 12, "x..x.x..x.x." },
        { 5, 16, "x..x..x..x..x..." }, { 7, 8, "x.xxxxxx" }, { 7, 12, "x.xx.x.xx.x." }, { 7, 16, "x..x.x.x..x.x.x." },
        { 9, 16, "x.xx.x.x.xx.x.x." }, { 11, 24, "x..x.x.x.x.x..x.x.x.x.x." }, { 13, 24, "x.xx.x.x.x.x.xx.x.x.x.x." },
    };
    for (const auto& [k, n, expected] : refs)
    {
        CAPTURE (k);
        CAPTURE (n);
        CHECK (bits (EuclidSequencer::pattern (n, k), n) == expected);
    }
    CHECK (EuclidSequencer::pattern (16, 0) == 0u);
    CHECK (bits (EuclidSequencer::pattern (4, 4), 4) == "xxxx");
    CHECK (EuclidSequencer::pattern (32, 32) == 0xffffffffu);
    for (int n = 1; n <= 32; ++n)
        for (int k = 0; k <= n; ++k)
        {
            const auto p = EuclidSequencer::pattern (n, k);
            int count = 0;
            for (int i = 0; i < n; ++i) count += (int) ((p >> i) & 1u);
            CHECK (count == k);
            if (n < 32) CHECK ((p >> n) == 0u);
        }
    // rotation moves the pattern to the right
    CHECK (bits (EuclidSequencer::rotated (EuclidSequencer::pattern (8, 3), 8, 1), 8) == ".x..x..x");
    CHECK (bits (EuclidSequencer::rotated (EuclidSequencer::pattern (8, 3), 8, 9), 8) == ".x..x..x");
}

TEST_CASE ("euclid: rings play their notes on the grid; rings of different length form a polymeter")
{
    EuclidSequencer e;
    e.prepare (sampleRate, 512);
    using P = EuclidSequencer;
    auto ring = [] (int r, int p) { return P::firstRingParam + r * P::numRingParams + p; };
    setParams (e, { { P::On, 1 }, { P::Part, 2 }, { P::Rate, seq::R1_16 }, { P::Velocity, 90 },
                    { ring (0, P::Steps), 8 }, { ring (0, P::Pulses), 3 }, { ring (0, P::Note), 36 },
                    { ring (1, P::Steps), 3 }, { ring (1, P::Pulses), 1 }, { ring (1, P::Note), 40 },
                    { ring (2, P::Steps), 4 }, { ring (2, P::Pulses), 1 }, { ring (2, P::Note), 41 } });
    const int stepLen = 6000; // 1/16 at 120 BPM, 48 kHz
    const auto hits = onsOnly (runSeq (e, stepLen * 24, 333));
    std::map<int, std::vector<long long>> byNote;
    for (const auto& h : hits)
    {
        CHECK (h.channel == 2);
        byNote[h.note].push_back (h.time / stepLen);
        CHECK (h.time % stepLen == 0);
    }
    CHECK (byNote[36] == std::vector<long long> { 0, 3, 6, 8, 11, 14, 16, 19, 22 });
    CHECK (byNote[40] == std::vector<long long> { 0, 3, 6, 9, 12, 15, 18, 21 });
    CHECK (byNote[41] == std::vector<long long> { 0, 4, 8, 12, 16, 20 });
    // rings 2 and 3 (lengths 3 and 4) meet every 12 steps
    std::vector<long long> both;
    for (auto t : byNote[40])
        if (std::find (byNote[41].begin(), byNote[41].end(), t) != byNote[41].end()) both.push_back (t);
    CHECK (both == std::vector<long long> { 0, 12 });

    // rotation 1 on ring 1
    e.reset();
    e.setParam (ring (0, P::Rotation), 1);
    const auto rot = onsOnly (runSeq (e, stepLen * 8, 512));
    std::vector<long long> r36;
    for (const auto& h : rot)
        if (h.note == 36) r36.push_back (h.time / stepLen);
    CHECK (r36 == std::vector<long long> { 1, 4, 7 });
}

TEST_CASE ("euclid: stop releases every note; disabled sequencer is silent")
{
    EuclidSequencer e;
    e.prepare (sampleRate, 512);
    using P = EuclidSequencer;
    setParams (e, { { P::On, 1 }, { P::Gate, 1.0f }, { P::firstRingParam + P::Pulses, 4 } });
    const auto hits = runSeq (e, 512 * 40, 512); // whole blocks: every event the sequencer produced is counted
    int ons = 0, offs = 0;
    for (const auto& h : hits) (h.on ? ons : offs) += 1;
    CHECK (ons > 0);
    SequencerOutput out;
    Transport stopped;
    e.process (stopped, 512, out);
    for (const auto& ev : out.events) offs += ev.isNoteOff() ? 1 : 0;
    CHECK (offs == ons);
    e.setParam (P::On, 0);
    CHECK (runSeq (e, 48000, 512).empty());
}

// ================================================================ Arp
namespace
{
std::vector<int> arpNotes (int mode, int octaves, std::vector<int> keys, int steps, float latch = 0.0f, bool releaseKeys = false)
{
    Arpeggiator a;
    a.prepare (sampleRate, 512);
    setParams (a, { { Arpeggiator::On, 1 }, { Arpeggiator::Part, 3 }, { Arpeggiator::Mode, (float) mode },
                    { Arpeggiator::Octaves, (float) octaves }, { Arpeggiator::Rate, seq::R1_16 }, { Arpeggiator::Latch, latch } });
    const auto hits = runSeq (a, 6000LL * steps, 500, 120.0, 0.0, true, [&] (ISequencer& s, long long pos, int) {
        auto& arp = static_cast<Arpeggiator&> (s);
        if (pos == 0)
            for (int k : keys) arp.noteInput (MidiEvent::noteOn (1, k, 100, 0));
        if (releaseKeys && pos == 3000)
            for (int k : keys) arp.noteInput (MidiEvent::noteOff (1, k, 0));
    });
    std::vector<int> notes;
    for (const auto& h : onsOnly (hits)) notes.push_back (h.note);
    return notes;
}
} // namespace

TEST_CASE ("arp: orders up, down, up/down, as played, chord, octaves")
{
    CHECK (arpNotes (Arpeggiator::Up, 1, { 64, 60, 67 }, 7) == std::vector<int> { 60, 64, 67, 60, 64, 67, 60 });
    CHECK (arpNotes (Arpeggiator::Down, 1, { 64, 60, 67 }, 6) == std::vector<int> { 67, 64, 60, 67, 64, 60 });
    CHECK (arpNotes (Arpeggiator::UpDown, 1, { 64, 60, 67 }, 8) == std::vector<int> { 60, 64, 67, 64, 60, 64, 67, 64 });
    CHECK (arpNotes (Arpeggiator::Order, 1, { 64, 60, 67 }, 6) == std::vector<int> { 64, 60, 67, 64, 60, 67 });
    CHECK (arpNotes (Arpeggiator::Up, 2, { 60, 64 }, 5) == std::vector<int> { 60, 64, 72, 76, 60 });
    CHECK (arpNotes (Arpeggiator::Down, 2, { 60, 64 }, 4) == std::vector<int> { 76, 72, 64, 60 });
    CHECK (arpNotes (Arpeggiator::UpDown, 1, { 60 }, 3) == std::vector<int> { 60, 60, 60 });
    const auto chord = arpNotes (Arpeggiator::Chord, 2, { 60, 64, 67 }, 2);
    CHECK (chord == std::vector<int> { 60, 64, 67, 72, 76, 79 });
    // random: only held notes (over the octaves), deterministic, not a fixed order
    const auto rnd = arpNotes (Arpeggiator::Random, 2, { 60, 64, 67 }, 64);
    REQUIRE (rnd.size() == 64);
    std::set<int> distinct (rnd.begin(), rnd.end());
    CHECK (distinct == std::set<int> { 60, 64, 67, 72, 76, 79 });
    CHECK (rnd == arpNotes (Arpeggiator::Random, 2, { 60, 64, 67 }, 64));
}

TEST_CASE ("arp: latch keeps the notes, without latch the arpeggio stops with the keys")
{
    CHECK (arpNotes (Arpeggiator::Up, 1, { 60, 62 }, 6, 0.0f, true) == std::vector<int> { 60 });
    CHECK (arpNotes (Arpeggiator::Up, 1, { 60, 62 }, 6, 1.0f, true) == std::vector<int> { 60, 62, 60, 62, 60, 62 });
}

TEST_CASE ("arp: rate incl. triplets, gate length, free running when stopped starts on the key")
{
    for (const auto& [rate, spacing] : std::vector<std::pair<int, long long>> { { seq::R1_8, 12000 }, { seq::R1_16T, 4000 }, { seq::R1_8T, 8000 }, { seq::R1_32T, 2000 }, { seq::R1_8D, 18000 } })
    {
        CAPTURE (rate);
        Arpeggiator a;
        a.prepare (sampleRate, 512);
        setParams (a, { { Arpeggiator::On, 1 }, { Arpeggiator::Rate, (float) rate }, { Arpeggiator::Gate, 0.25f } });
        const auto hits = runSeq (a, spacing * 6 + 10, 256, 120.0, 0.0, true, [] (ISequencer& s, long long pos, int) {
            if (pos == 0) static_cast<Arpeggiator&> (s).noteInput (MidiEvent::noteOn (1, 60, 100, 0));
        });
        std::vector<long long> ons, offs;
        for (const auto& h : hits) (h.on ? ons : offs).push_back (h.time);
        REQUIRE (ons.size() >= 6);
        for (size_t i = 0; i < 6; ++i) CHECK (ons[i] == (long long) i * spacing);
        REQUIRE (! offs.empty());
        CHECK (offs[0] == spacing / 4 - 1);
    }
    // stopped transport: free-running from the key (offset 100), at the transport tempo
    Arpeggiator a;
    a.prepare (sampleRate, 512);
    setParams (a, { { Arpeggiator::On, 1 }, { Arpeggiator::Rate, seq::R1_16 } });
    const auto hits = runSeq (a, 30000, 512, 120.0, 0.0, false, [] (ISequencer& s, long long pos, int) {
        if (pos == 1024) static_cast<Arpeggiator&> (s).noteInput (MidiEvent::noteOn (1, 60, 100, 100));
    });
    const auto ons = onsOnly (hits);
    REQUIRE (ons.size() >= 4);
    CHECK (ons[0].time == 1124);
    CHECK (ons[1].time == 1124 + 6000);
    CHECK (ons[3].time == 1124 + 18000);
}

// ================================================================ Orbit
TEST_CASE ("orbit: lanes with independent lengths and rates stay aligned to the transport")
{
    OrbitSequencer o;
    o.prepare (sampleRate, 512);
    using P = OrbitSequencer;
    auto lane = [] (int l, int p) { return P::firstLaneParam + l * P::numLaneParams + p; };
    setParams (o, { { P::On, 1 }, { P::Part, 4 }, { lane (0, P::Length), 3 }, { lane (0, P::Rate), seq::R1_16 },
                    { lane (1, P::Length), 4 }, { lane (1, P::Rate), seq::R1_16 }, { lane (2, P::Length), 2 },
                    { lane (2, P::Rate), seq::R1_8T }, { lane (2, P::Transpose), 12 }, { lane (3, P::Length), 16 } });
    o.setStep (0, 0, { true, 48, 100 });
    o.setStep (1, 0, { true, 50, 100 });
    o.setStep (2, 1, { true, 40, 100 }); // second step of a 2-step 1/8T lane -> +12 = 52
    const auto hits = onsOnly (runSeq (o, 6000 * 25, 441));
    std::map<int, std::vector<long long>> t;
    for (const auto& h : hits) { t[h.note].push_back (h.time); CHECK (h.channel == 4); }
    CHECK (t[48] == std::vector<long long> { 0, 18000, 36000, 54000, 72000, 90000, 108000, 126000, 144000 });
    CHECK (t[50] == std::vector<long long> { 0, 24000, 48000, 72000, 96000, 120000, 144000 });
    std::vector<long long> together;
    for (auto x : t[48])
        if (std::find (t[50].begin(), t[50].end(), x) != t[50].end()) together.push_back (x);
    CHECK (together == std::vector<long long> { 0, 72000, 144000 }); // lcm (3, 4) = 12 steps
    REQUIRE (t[52].size() >= 3);
    CHECK (t[52][0] == 8000); // 1/8T = 8000 samples, step 2 of 2
    CHECK (t[52][1] == 24000);

    // a loop back in the host lands every lane on the position the transport says
    o.reset();
    const auto looped = onsOnly (runSeq (o, 6000, 500, 120.0, 3.0)); // ppq 3.0 = 16th step 12
    std::set<int> notes;
    for (const auto& h : looped)
        if (h.time == 0) notes.insert (h.note);
    CHECK (notes.count (48) == 1); // 12 % 3 == 0
    CHECK (notes.count (50) == 1); // 12 % 4 == 0

    const auto json = o.toJson();
    OrbitSequencer other;
    REQUIRE (other.fromJson (json));
    CHECK (other.getStep (2, 1).on);
    CHECK (other.getStep (2, 1).note == 40);
    CHECK_FALSE (other.getStep (2, 0).on);
    CHECK (other.toJson() == json);
    CHECK_FALSE (other.fromJson ("[1,2]"));
}

// ================================================================ scale / chord
TEST_CASE ("scale: quantise to the nearest scale note; diatonic one-finger chords")
{
    using namespace scales;
    // C major
    CHECK (quantise (61, 0, Major) == 60); // C# -> C (tie resolves down)
    CHECK (quantise (63, 0, Major) == 62);
    CHECK (quantise (66, 0, Major) == 65);
    CHECK (quantise (64, 0, Major) == 64);
    // A minor pentatonic (A C D E G)
    CHECK (quantise (70, 9, MinorPentatonic) == 69);
    CHECK (quantise (71, 9, MinorPentatonic) == 72);
    CHECK (quantise (73, 2, Chromatic) == 73);
    std::array<int, 5> c {};
    REQUIRE (build (62, Triad, 0, Major, c) == 3);
    CHECK (std::vector<int> (c.begin(), c.begin() + 3) == std::vector<int> { 62, 65, 69 }); // D minor in C major
    REQUIRE (build (67, Seventh, 0, Major, c) == 4);
    CHECK (std::vector<int> (c.begin(), c.begin() + 4) == std::vector<int> { 67, 71, 74, 77 }); // G7
    REQUIRE (build (60, Triad, 0, Chromatic, c) == 3);
    CHECK (std::vector<int> (c.begin(), c.begin() + 3) == std::vector<int> { 60, 64, 67 });
    REQUIRE (build (60, Sus4, 0, Major, c) == 3);
    CHECK (std::vector<int> (c.begin(), c.begin() + 3) == std::vector<int> { 60, 65, 67 });
    REQUIRE (build (57, Power, 0, Minor, c) == 3);
    CHECK (std::vector<int> (c.begin(), c.begin() + 3) == std::vector<int> { 57, 64, 69 });
    // every scale contains its root and fits in 12 bits
    for (int s = 0; s < numScales; ++s) { CHECK ((mask (s) & 1u) == 1u); CHECK (mask (s) <= 0xfffu); }

    // NoteShaper: the key's note-off releases exactly what it played, even after a change
    NoteShaper shaper;
    shaper.set (true, Triad, 0, Major);
    std::vector<MidiEvent> out;
    auto emit = [&] (const MidiEvent& e) { out.push_back (e); };
    shaper.process (MidiEvent::noteOn (1, 61, 100, 5), emit);
    REQUIRE (out.size() == 3);
    CHECK (out[0].data1 == 60);
    CHECK (out[1].data1 == 64);
    CHECK (out[2].data1 == 67);
    CHECK (out[2].sampleOffset == 5);
    shaper.set (false, Off, 0, Major);
    out.clear();
    shaper.process (MidiEvent::noteOff (1, 61, 9), emit);
    REQUIRE (out.size() == 3);
    for (const auto& e : out) CHECK (e.isNoteOff());
    out.clear();
    shaper.process (MidiEvent::controller (1, 1, 5), emit);
    CHECK (out.size() == 1);
}

TEST_CASE ("scale: one-finger chord through the core plays three voices; quantise affects the sequencer")
{
    Rig rig;
    rig.set ("p1.chord", (float) scales::Triad);
    rig.run (4, { MidiEvent::noteOn (1, 60, 100, 0) });
    CHECK (rig.core.activeVoices() == 3);

    Rig seqRig;
    seqRig.set ("p1.quant", 1.0f);
    seqRig.set ("key.scale", (float) scales::Major);
    seqRig.set (Core::Ids::notesOut, 1.0f);
    StepSequencer::Step s;
    s.on = true;
    s.note = 61;
    seqRig.core.stepSequencer().setStep (0, s);
    seqRig.set (Core::Ids::seqRun, 1.0f);
    seqRig.run (10);
    REQUIRE (! seqRig.midiOut.empty());
    CHECK (seqRig.midiOut[0].note == 60);
}

// ================================================================ step patterns, song
TEST_CASE ("step: song mode plays the chain, a pattern change waits for the end of the pattern")
{
    StepSequencer seq;
    seq.prepare (sampleRate, 512);
    seq.setParam (StepSequencer::Length, 4);
    for (int p = 0; p < 3; ++p)
    {
        StepSequencer::Step s;
        s.on = true;
        s.note = 60 + p;
        seq.setStep (p, 0, s); // first step of pattern p plays note 60 + p
    }
    seq.setPatternLength (2, 2);
    seq.setChain ({ { 0, 2 }, { 1, 1 }, { 2, 3 } }); // 4+4 + 4 + 2+2+2 = 18 steps
    seq.setParam (StepSequencer::Song, 1.0f);
    const int stepLen = 6000;
    std::vector<std::pair<long long, int>> got;
    for (const auto& h : onsOnly (runSeq (seq, stepLen * 36, 512)))
        got.push_back ({ h.time / stepLen, h.note });
    const std::vector<std::pair<long long, int>> expected { { 0, 60 }, { 4, 60 }, { 8, 61 }, { 12, 62 }, { 14, 62 }, { 16, 62 },
                                                            { 18, 60 }, { 22, 60 }, { 26, 61 }, { 30, 62 }, { 32, 62 }, { 34, 62 } };
    CHECK (got == expected);

    // song off: pattern 2 selected while pattern 1 plays starts at the next pattern start
    seq.reset();
    seq.setParam (StepSequencer::Song, 0.0f);
    seq.setParam (StepSequencer::Pattern, 1.0f);
    Transport tr;
    tr.bpm = 120.0;
    tr.playing = true;
    std::vector<std::pair<long long, int>> sw;
    SequencerOutput out;
    for (long long pos = 0; pos < stepLen * 12; pos += 500)
    {
        if (pos == 9000) seq.setParam (StepSequencer::Pattern, 2.0f); // during step 1
        tr.ppqAtBlockStart = (double) pos / sampleRate * 2.0;
        out.clear();
        seq.process (tr, 500, out);
        for (const auto& e : out.events)
            if (e.isNoteOn()) sw.push_back ({ (pos + e.sampleOffset) / stepLen, e.data1 });
    }
    CHECK (sw == std::vector<std::pair<long long, int>> { { 0, 60 }, { 4, 61 }, { 8, 61 } });
    CHECK (seq.playingPatternIndex() == 1);
}

TEST_CASE ("step: patterns, lengths and chain survive JSON; 0.1 patterns load into pattern 1")
{
    ParamRegistry reg;
    reg.freeze();
    StepSequencer a (&reg);
    StepSequencer::Step s;
    s.on = true;
    s.note = 70;
    a.setStep (0, 3, s);
    s.note = 71;
    a.setStep (15, 63, s);
    a.setPatternLength (15, 12);
    a.setChain ({ { 0, 4 }, { 15, 2 } });
    const auto json = a.toJson();
    StepSequencer b (&reg);
    REQUIRE (b.fromJson (json));
    CHECK (b.getStep (0, 3).note == 70);
    CHECK (b.getStep (15, 63).note == 71);
    CHECK (b.getPatternLength (15) == 12);
    const auto chain = b.getChain();
    REQUIRE (chain.size() == 2);
    CHECK (chain[1].pattern == 15);
    CHECK (chain[1].repeats == 2);
    CHECK (b.toJson() == json);

    const std::string old = R"({"format":"ohpe-step-pattern","version":1,"steps":[{"i":2,"on":true,"note":55,"vel":100,"gate":0.5,"prob":100,"ratchet":1}]})";
    REQUIRE (b.fromJson (old));
    CHECK (b.getStep (0, 2).note == 55);
    CHECK (b.getStep (2).on); // edit pattern = pattern 1
    CHECK (b.getChain().empty());
    CHECK_FALSE (b.getStep (15, 63).on);
}

// ================================================================ host-ppq timing
TEST_CASE ("timing: sequencer notes are sample-accurate against the host ppq at various tempos and buffer sizes" * doctest::test_suite ("sliced"))
{
    const auto slice = test::testSlice(); // CI runs the combinations in several processes
    long long item = 0;
    for (double bpm : { 60.0, 97.3, 120.0, 174.0, 233.0 })
        for (int block : { 17, 64, 441, 512, 1024 })
            for (double ppq0 : { 0.0, 3.37 })
            {
                if (! slice.has (item++)) continue;
                CAPTURE (bpm);
                CAPTURE (block);
                CAPTURE (ppq0);
                Rig rig (block);
                rig.set (Core::Ids::notesOut, 1.0f);
                rig.set (Core::Ids::seqRun, 1.0f);
                rig.set ("seq.euclid.on", 1.0f);
                rig.set ("seq.euclid.r1.pulses", 16.0f); // every step on part 2
                rig.set ("seq.euclid.rate", (float) seq::R1_16T);
                for (int i = 0; i < 16; ++i)
                {
                    StepSequencer::Step s;
                    s.on = true;
                    s.note = 60;
                    rig.core.stepSequencer().setStep (i, s); // every 16th on part 1
                }
                const long long total = (long long) (sampleRate * 4.0);
                rig.run ((int) (total / block), {}, [&] (long long t) { return hostAt (bpm, ppq0, t); });
                const double samplesPerQuarter = sampleRate * 60.0 / bpm;
                int checked = 0;
                std::map<int, long long> lastK;
                for (const auto& h : rig.midiOut)
                {
                    if (! h.on) continue;
                    const double spq = h.channel == 1 ? 4.0 : 6.0; // part 1 = 16ths, part 2 = 16th triplets
                    const long long k = std::llround ((ppq0 + (double) h.time / samplesPerQuarter) * spq);
                    const double expected = ((double) k / spq - ppq0) * samplesPerQuarter;
                    CHECK (std::abs ((double) h.time - expected) <= 1.0);
                    if (lastK.count (h.channel)) CHECK (k == lastK[h.channel] + 1); // no step lost or doubled
                    lastK[h.channel] = k;
                    ++checked;
                }
                CHECK (checked > 8);
            }
}

TEST_CASE ("timing: a host loop restarts every sequencer on the looped position")
{
    Rig rig (256);
    rig.set (Core::Ids::notesOut, 1.0f);
    rig.set (Core::Ids::seqRun, 1.0f);
    for (int i = 0; i < 16; ++i)
    {
        StepSequencer::Step s;
        s.on = true;
        s.note = 40 + i;
        rig.core.stepSequencer().setStep (i, s);
    }
    // loop of one bar (4 quarters) at 120 BPM: 96000 samples
    rig.run (1200, {}, [] (long long t) {
        Transport tr = hostAt (120.0, 0.0, 0);
        tr.ppqAtBlockStart = std::fmod ((double) t / sampleRate * 2.0, 4.0);
        return tr;
    });
    std::vector<int> notes;
    for (const auto& h : rig.midiOut)
        if (h.on && h.channel == 1) notes.push_back (h.note);
    REQUIRE (notes.size() >= 48);
    for (size_t i = 0; i < 48; ++i) CHECK (notes[i] == 40 + (int) (i % 16));
}

// ================================================================ MIDI clock
TEST_CASE ("clock in: tempo and position follow a jittery MIDI clock smoothly")
{
    for (double bpm : { 90.0, 128.0, 171.0 })
    {
        CAPTURE (bpm);
        MidiClockFollower f;
        f.prepare (sampleRate);
        f.setSmoothing (0.5f);
        dsp::Random rng (77);
        const double spt = sampleRate * 60.0 / bpm / 24.0;
        f.handle ({ 0, midiclock::start, 0, 0 }, 0);
        std::vector<double> bpms, phaseErrors;
        for (int i = 0; i < 24 * 16; ++i)
        {
            const double jitter = rng.nextBipolar() * 0.001 * sampleRate; // +-1 ms
            f.handle ({ 0, midiclock::tick, 0, 0 }, (int64_t) std::llround (100 + i * spt + jitter));
            if (i > 24 * 2)
            {
                bpms.push_back (f.bpm());
                // half a tick later the position is between tick i and i + 1, close to the ideal clock
                const auto tr = f.transportAt ((int64_t) std::llround (100 + (i + 0.5) * spt), 120.0);
                CHECK (tr.playing);
                phaseErrors.push_back (std::abs (tr.ppqAtBlockStart - (i + 0.5) / 24.0) * 60.0 / bpm); // seconds
            }
        }
        for (double b : bpms) CHECK (std::abs (b - bpm) < bpm * 0.01); // within 1 % after two quarter notes
        for (double e : phaseErrors) CHECK (e < 0.0012);                // within 1.2 ms of the ideal clock
        CHECK (f.isRunning());
        // stop / song position (16th 32 = quarter 8) / continue
        f.handle ({ 0, midiclock::stop, 0, 0 }, 0);
        CHECK_FALSE (f.transportAt (0, 120.0).playing);
        f.handle ({ 0, midiclock::songPosition, 32, 0 }, 0);
        f.handle ({ 0, midiclock::cont, 0, 0 }, 0);
        const auto t0 = (int64_t) std::llround (100 + 400 * spt);
        f.handle ({ 0, midiclock::tick, 0, 0 }, t0);
        CHECK (f.transportAt (t0, 120.0).ppqAtBlockStart == doctest::Approx (8.0));
        // a stalled clock never runs more than 1.5 ticks ahead
        CHECK (f.transportAt (t0 + (int64_t) (spt * 100), 120.0).ppqAtBlockStart <= 8.0 + 1.5 / 24.0 + 1.0e-9);
    }
}

TEST_CASE ("clock in: the core follows MIDI clock; sequencer steps stay evenly spaced")
{
    Rig rig (256);
    rig.set (Core::Ids::syncSource, (float) Core::SyncMidiClock);
    rig.set (Core::Ids::notesOut, 1.0f);
    rig.set (Core::Ids::seqRun, 1.0f);
    for (int i = 0; i < 16; ++i)
    {
        StepSequencer::Step s;
        s.on = true;
        rig.core.stepSequencer().setStep (i, s);
    }
    const double bpm = 125.0, spt = sampleRate * 60.0 / bpm / 24.0;
    dsp::Random rng (3);
    long long nextTick = 1000;
    bool started = false;
    OutputBuffers out;
    for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = rig.l[(size_t) o].data(); out.right[(size_t) o] = rig.r[(size_t) o].data(); }
    std::vector<long long> ons;
    int tickIndex = 0;
    for (long long t = 0; t < (long long) sampleRate * 8; t += 256)
    {
        std::vector<MidiEvent> midi;
        if (! started) { midi.push_back ({ 10, midiclock::start, 0, 0 }); started = true; }
        while (nextTick < t + 256)
        {
            midi.push_back ({ (int) (nextTick - t), midiclock::tick, 0, 0 });
            ++tickIndex;
            nextTick = 1000 + (long long) std::llround (tickIndex * spt + rng.nextBipolar() * 0.0007 * sampleRate);
        }
        rig.core.process (out, 256, midi.data(), (int) midi.size(), Transport {});
        for (const auto& e : rig.core.midiOutput())
            if (e.isNoteOn()) ons.push_back (t + e.sampleOffset);
    }
    REQUIRE (ons.size() > 40);
    CHECK (ons[0] >= 1000);
    CHECK (ons[0] < 1000 + 256);
    const double stepLen = spt * 6.0;
    for (size_t i = 8; i < ons.size(); ++i)
        CHECK (std::abs ((double) (ons[i] - ons[i - 1]) - stepLen) < 0.0025 * sampleRate); // < 2.5 ms deviation
    CHECK (rig.core.transportBpm() == doctest::Approx (bpm).epsilon (0.02));
}

TEST_CASE ("clock out: start, 24 ticks per quarter on exact samples, stop; song position after a jump")
{
    Rig rig (300);
    rig.set (Core::Ids::clockOut, 1.0f);
    rig.set (Core::Ids::tempo, 120.0f);
    rig.run (2); // stopped: no clock
    CHECK (rig.rawOut.empty());
    rig.set (Core::Ids::seqRun, 1.0f);
    rig.run (160); // 48000 samples = 2 quarters
    REQUIRE (! rig.rawOut.empty());
    CHECK (rig.rawOut[0].second.status == midiclock::start);
    std::vector<long long> ticks;
    for (const auto& [t, e] : rig.rawOut)
        if (e.status == midiclock::tick) ticks.push_back (t);
    REQUIRE (ticks.size() == 48);
    for (size_t i = 0; i < ticks.size(); ++i) CHECK (ticks[i] == 600 + (long long) i * 1000); // 1000 samples per tick
    rig.set (Core::Ids::seqRun, 0.0f);
    rig.run (1);
    CHECK (rig.rawOut.back().second.status == midiclock::stop);

    // host transport starting mid-song: song position (in 16ths) + continue
    Rig host (512);
    host.set (Core::Ids::clockOut, 1.0f);
    host.set (Core::Ids::seqRun, 1.0f);
    host.run (4, {}, [] (long long t) { return hostAt (120.0, 8.0, t); });
    REQUIRE (host.rawOut.size() >= 3);
    CHECK (host.rawOut[0].second.status == midiclock::songPosition);
    CHECK (host.rawOut[0].second.data1 == 32); // quarter 8 = 16th 32
    CHECK (host.rawOut[1].second.status == midiclock::cont);
    CHECK (host.rawOut[2].second.status == midiclock::tick);
    CHECK (host.rawOut[2].first == 0);
}

// ================================================================ MPE, channels, parts
namespace
{
void plainAnalog (Rig& rig, int part)
{
    const auto p = "p" + std::to_string (part) + ".";
    rig.set (p + "engine", (float) EngineType::Analog);
    rig.set (p + "analog.shape", 0.5f);
    rig.set (p + "analog.detune", 0.0f);
    rig.set (p + "analog.oscmix", 0.0f);
    rig.set (p + "analog.cutoff", 18000.0f);
    rig.set (p + "analog.reso", 0.0f);
    rig.set (p + "mod.amt", 0.0f);
}
} // namespace

TEST_CASE ("mpe: per-note pitch bend on member channels, pressure to level, master channel untouched")
{
    Rig rig (480);
    plainAnalog (rig, 1);
    rig.set ("p1.mpe", 1.0f);
    rig.set ("p1.mpe.bend", 48.0f);
    // note on member channel 2, bent +12 semitones (8192 + 8192 / 4)
    const int bend = 8192 + 2048;
    rig.run (1, { { 0, 0xe1, (uint8_t) (bend & 0x7f), (uint8_t) (bend >> 7) }, MidiEvent::noteOn (2, 57, 127, 0) });
    rig.run (99);
    const auto& rec = rig.recL[0];
    CHECK (std::abs (centsBetween (measureFrequency (rec, sampleRate, 4800, rec.size()), 440.0)) < 5.0);

    // a bend on another member channel does not touch this note
    rig.recL[0].clear();
    rig.run (1, { { 0, 0xe2, 0, 0 } });
    rig.run (60);
    CHECK (std::abs (centsBetween (measureFrequency (rig.recL[0], sampleRate, 4800, rig.recL[0].size()), 440.0)) < 5.0);
    const float before = rms (rig.recL[0], 4800);

    // pressure on its channel raises the level (target: level)
    rig.recL[0].clear();
    rig.run (1, { { 0, 0xd1, 127, 0 } });
    rig.run (60);
    CHECK (rms (rig.recL[0], 4800) > before * 1.6f);

    // per-note release
    rig.run (1, { MidiEvent::noteOff (2, 57, 0) });
    rig.run (100);
    CHECK (rig.core.activeVoices() == 0);
}

TEST_CASE ("mpe: timbre (CC 74) to the analog filter; without MPE channel pressure acts on the whole part")
{
    Rig rig (480);
    plainAnalog (rig, 1);
    rig.set ("p1.analog.cutoff", 400.0f);
    rig.set ("p1.mpe", 1.0f);
    rig.run (1, { MidiEvent::noteOn (3, 48, 120, 0) });
    rig.run (50);
    const float dark = rms (rig.recL[0], 9600);
    rig.recL[0].clear();
    rig.run (1, { MidiEvent::controller (3, 74, 127) });
    rig.run (50);
    CHECK (rms (rig.recL[0], 4800) > dark * 1.3f);

    Rig plain (480);
    plainAnalog (plain, 1);
    plain.run (1, { MidiEvent::noteOn (1, 60, 120, 0) });
    plain.run (50);
    const float quiet = rms (plain.recL[0], 9600);
    plain.recL[0].clear();
    plain.run (1, { { 0, 0xd0, 127, 0 } }); // channel pressure, channel 1
    plain.run (50);
    CHECK (rms (plain.recL[0], 4800) > quiet * 1.6f);
}

TEST_CASE ("parts: MIDI channel per part (default N), omni, routing to the part's mixer channel")
{
    Rig rig;
    for (int p = 1; p <= 4; ++p) rig.set ("mix.ch" + std::to_string (p) + ".out", (float) p); // part N -> aux N
    rig.run (20, { MidiEvent::noteOn (3, 60, 100, 0) });
    CHECK (peak (rig.recL[3]) > 0.005f);
    CHECK (peak (rig.recL[1]) == 0.0f);
    CHECK (peak (rig.recL[2]) == 0.0f);
    CHECK (peak (rig.recL[4]) == 0.0f);
    CHECK (rig.core.lastNote() == 60);

    Rig omni;
    for (int p = 1; p <= 4; ++p) omni.set ("mix.ch" + std::to_string (p) + ".out", (float) p);
    omni.set ("p2.midich", 0.0f);
    omni.run (20, { MidiEvent::noteOn (1, 60, 100, 0) });
    CHECK (peak (omni.recL[1]) > 0.005f);
    CHECK (peak (omni.recL[2]) > 0.005f);
    CHECK (peak (omni.recL[3]) == 0.0f);
}

TEST_CASE ("parts: four parts play simultaneously on different engines driven by four sequencers")
{
    Rig rig (512);
    for (int p = 1; p <= 4; ++p) rig.set ("mix.ch" + std::to_string (p) + ".out", (float) p);
    rig.set ("p1.engine", (float) EngineType::FM);
    rig.set ("p2.engine", (float) EngineType::Analog);
    rig.set ("p3.engine", (float) EngineType::FM);
    rig.set ("p4.engine", (float) EngineType::Analog);
    rig.set (Core::Ids::seqRun, 1.0f);
    // step -> part 1
    StepSequencer::Step s;
    s.on = true;
    s.note = 60;
    rig.core.stepSequencer().setStep (0, s);
    rig.core.stepSequencer().setStep (8, s);
    // euclid -> part 2 (default target), arp -> part 3 (default), orbit -> part 4 (default)
    rig.set ("seq.euclid.on", 1.0f);
    rig.set ("seq.arp.on", 1.0f);
    rig.set ("seq.arp.latch", 1.0f);
    rig.set ("seq.orbit.on", 1.0f);
    rig.core.orbitSequencer().setStep (0, 0, { true, 50, 110 });
    rig.core.orbitSequencer().setStep (1, 3, { true, 62, 110 });
    rig.run (1, { MidiEvent::noteOn (3, 64, 100, 0), MidiEvent::noteOn (3, 67, 100, 10) }); // into the arp (part 3 channel)
    rig.run (1, { MidiEvent::noteOff (3, 64, 0), MidiEvent::noteOff (3, 67, 0) });          // latched
    rig.run (200);
    for (int a = 1; a <= 4; ++a)
    {
        CAPTURE (a);
        CHECK (rms (rig.recL[(size_t) a]) > 0.002f);
        CHECK (allFinite (rig.recL[(size_t) a]));
    }
    CHECK (peak (rig.recL[0]) == 0.0f); // everything went to its own output
    CHECK (rig.core.activeVoices() > 0);
    CHECK (rig.core.arpeggiator().heldCount() == 2);

    // arp off: the part plays the keys directly again
    rig.set ("seq.arp.on", 0.0f);
    rig.run (1);
    CHECK (rig.core.arpeggiator().heldCount() == 0);
}

TEST_CASE ("state: sequencer settings, patterns, chain, orbit steps and routing round-trip")
{
    Core a;
    auto& p = a.params();
    p.set ("seq.euclid.r2.pulses", 5.0f);
    p.set ("seq.arp.mode", (float) Arpeggiator::UpDown);
    p.set ("seq.orbit.l3.len", 5.0f);
    p.set ("p3.midich", 9.0f);
    p.set ("p2.mpe", 1.0f);
    p.set ("key.scale", (float) scales::Dorian);
    p.set ("sync.src", (float) Core::SyncMidiClock);
    p.set ("p4.analog.cutoff", 1234.0f);
    p.set ("seq.step.song", 1.0f);
    StepSequencer::Step s;
    s.on = true;
    s.note = 77;
    a.stepSequencer().setStep (5, 9, s);
    a.stepSequencer().setChain ({ { 5, 3 } });
    a.orbitSequencer().setStep (3, 15, { true, 99, 33 });
    const auto json = a.saveState();

    Core b;
    REQUIRE (b.loadState (json));
    for (const char* id : { "seq.euclid.r2.pulses", "seq.arp.mode", "seq.orbit.l3.len", "p3.midich", "p2.mpe", "key.scale",
                            "sync.src", "p4.analog.cutoff", "seq.step.song" })
    {
        CAPTURE (id);
        CHECK (b.params().get (id) == doctest::Approx (p.get (id)));
    }
    CHECK (b.stepSequencer().getStep (5, 9).note == 77);
    CHECK (b.stepSequencer().getChain().size() == 1);
    CHECK (b.orbitSequencer().getStep (3, 15).note == 99);
    CHECK (b.orbitSequencer().getStep (3, 15).velocity == 33);
    CHECK (b.saveState() == json);

    // a 0.1 state (no orbit section, no new parameters) loads with defaults
    REQUIRE (b.loadState (R"({"format":"ohpe-state","version":1,"params":{"tempo":99}})"));
    CHECK (b.params().get ("tempo") == doctest::Approx (99.0f));
    CHECK (b.params().get ("p3.midich") == doctest::Approx (3.0f));
    CHECK_FALSE (b.orbitSequencer().getStep (3, 15).on);
}
