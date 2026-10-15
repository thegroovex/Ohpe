// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TestHelpers.h"
#include "seq/StepSequencer.h"

#include <doctest/doctest.h>

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
};

/** Runs the sequencer for `samples` at 120 BPM and collects note events with absolute times. */
std::vector<Hit> runSeq (StepSequencer& seq, int samples, int block = 512, std::vector<ParamLockEvent>* locksOut = nullptr)
{
    std::vector<Hit> hits;
    std::vector<ParamLockEvent> lockStore;
    lockStore.reserve (256);
    SequencerOutput out;
    out.locks = &lockStore;
    out.maxLocks = 256;
    Transport tr;
    tr.bpm = 120.0;
    tr.playing = true;
    long long pos = 0;
    while (pos < samples)
    {
        tr.ppqAtBlockStart = (double) pos / sampleRate * 2.0; // 120 BPM = 2 quarters / s
        out.clear();
        seq.process (tr, block, out);
        for (const auto& e : out.events)
            if (pos + e.sampleOffset < samples) hits.push_back ({ pos + e.sampleOffset, e.data1, e.isNoteOn() });
        if (locksOut != nullptr)
            for (const auto& l : lockStore) locksOut->push_back ({ (int) (pos + l.sampleOffset), l.paramIndex, l.value, l.clearAll });
        pos += block;
    }
    return hits;
}
} // namespace

TEST_CASE ("step: 16ths at 120 BPM land on exact sample positions")
{
    StepSequencer seq;
    seq.prepare (sampleRate, 512);
    for (int i = 0; i < 16; ++i)
    {
        StepSequencer::Step s;
        s.on = true;
        s.note = 60 + i;
        seq.setStep (i, s);
    }
    const auto hits = runSeq (seq, 48000); // one second = 8 sixteenths
    std::vector<Hit> ons;
    for (const auto& h : hits) if (h.on) ons.push_back (h);
    REQUIRE (ons.size() == 8);
    for (int i = 0; i < 8; ++i)
    {
        CHECK (ons[(size_t) i].time == i * 6000);
        CHECK (ons[(size_t) i].note == 60 + i);
    }
    // every note-on is followed by its note-off after half a step (gate 0.5)
    int offs = 0;
    for (const auto& h : hits) if (! h.on) ++offs;
    CHECK (offs >= 7);
}

TEST_CASE ("step: length wraps, empty steps are silent")
{
    StepSequencer seq;
    seq.prepare (sampleRate, 512);
    seq.setParam (StepSequencer::Length, 4.0f);
    StepSequencer::Step s;
    s.on = true;
    seq.setStep (0, s);
    const auto hits = runSeq (seq, 48000);
    int ons = 0;
    for (const auto& h : hits) if (h.on) { ++ons; CHECK (h.time % 24000 == 0); }
    CHECK (ons == 2); // step 0 at 0 and at 4 steps later (0.5 s)
}

TEST_CASE ("step: probability 0 never plays, 50 % plays some")
{
    StepSequencer seq;
    seq.prepare (sampleRate, 512);
    seq.setParam (StepSequencer::Length, 1.0f);
    StepSequencer::Step s;
    s.on = true;
    s.probability = 0;
    seq.setStep (0, s);
    CHECK (runSeq (seq, 48000).empty());

    seq.reset();
    s.probability = 50;
    seq.setStep (0, s);
    int ons = 0;
    for (const auto& h : runSeq (seq, 48000 * 4)) ons += h.on ? 1 : 0;
    CHECK (ons > 8);
    CHECK (ons < 56);
}

TEST_CASE ("step: ratchets subdivide the step")
{
    StepSequencer seq;
    seq.prepare (sampleRate, 512);
    seq.setParam (StepSequencer::Length, 16.0f);
    StepSequencer::Step s;
    s.on = true;
    s.ratchet = 4;
    seq.setStep (0, s);
    std::vector<long long> ons;
    for (const auto& h : runSeq (seq, 6000)) if (h.on) ons.push_back (h.time);
    REQUIRE (ons.size() == 4);
    for (int i = 0; i < 4; ++i) CHECK (ons[(size_t) i] == i * 1500);
}

TEST_CASE ("step: parameter locks are emitted with the step and released by the next")
{
    ParamRegistry reg;
    reg.add (pd::continuous ("p", "P", "P", 0.0f, 10.0f, 0.0f));
    reg.freeze();
    StepSequencer seq (&reg);
    seq.prepare (sampleRate, 512);
    StepSequencer::Step s;
    s.on = true;
    s.locks[0] = { 0, 0.5f };
    seq.setStep (0, s);
    std::vector<ParamLockEvent> locks;
    runSeq (seq, 13000, 512, &locks);
    bool sawLock = false, sawClearAfter = false;
    for (const auto& l : locks)
    {
        if (! l.clearAll && l.paramIndex == 0) { sawLock = true; CHECK (l.value == doctest::Approx (5.0f).epsilon (0.001)); }
        if (l.clearAll && l.sampleOffset >= 6000) sawClearAfter = true;
    }
    CHECK (sawLock);
    CHECK (sawClearAfter);
}

TEST_CASE ("step: stopping ends every sounding note")
{
    StepSequencer seq;
    seq.prepare (sampleRate, 512);
    StepSequencer::Step s;
    s.on = true;
    s.gate = 1.0f;
    seq.setStep (0, s);
    SequencerOutput out;
    Transport tr;
    tr.bpm = 120.0;
    tr.playing = true;
    seq.process (tr, 512, out);
    REQUIRE (out.events.size() == 1);
    out.clear();
    tr.playing = false;
    seq.process (tr, 512, out);
    REQUIRE (out.events.size() == 1);
    CHECK (out.events[0].isNoteOff());
}

TEST_CASE ("step: pattern JSON round-trip including locks")
{
    ParamRegistry reg;
    reg.add (pd::continuous ("a.b", "P", "P", 0.0f, 1.0f, 0.0f));
    reg.freeze();
    StepSequencer seq (&reg);
    StepSequencer::Step s;
    s.on = true; s.note = 72; s.velocity = 90; s.gate = 0.25f; s.probability = 75; s.ratchet = 3;
    s.locks[1] = { 0, 0.125f };
    seq.setStep (5, s);
    seq.setStep (63, s);
    const auto json = seq.toJson();

    StepSequencer other (&reg);
    REQUIRE (other.fromJson (json));
    for (int i : { 5, 63 })
    {
        const auto t = other.getStep (i);
        CHECK (t.on);
        CHECK (t.note == 72);
        CHECK (t.velocity == 90);
        CHECK (t.gate == doctest::Approx (0.25f));
        CHECK (t.probability == 75);
        CHECK (t.ratchet == 3);
        CHECK (t.locks[0].paramIndex == 0);
        CHECK (t.locks[0].normValue == doctest::Approx (0.125f).epsilon (0.001));
    }
    CHECK_FALSE (other.getStep (6).on);
    CHECK_FALSE (other.fromJson ("{broken"));
}

TEST_CASE ("step: saved pattern is stable when loaded and saved again")
{
    ParamRegistry reg;
    reg.add (pd::continuous ("a.b", "P", "P", 0.0f, 1.0f, 0.0f));
    reg.freeze();
    StepSequencer empty (&reg);
    const auto first = empty.toJson();
    StepSequencer loaded (&reg);
    REQUIRE (loaded.fromJson (first));
    CHECK (loaded.toJson() == first); // loading resets steps to the default: still nothing to write

    StepSequencer::Step s;
    s.on = true; s.note = 50;
    empty.setStep (3, s);
    empty.toggleStep (3); // switched off again: keeps its note, so it is written
    const auto second = empty.toJson();
    REQUIRE (loaded.fromJson (second));
    CHECK (loaded.toJson() == second);
    CHECK (loaded.getStep (3).note == 50);
}
