// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p60: the sustain pedal (CC 64) and sostenuto (CC 66) hold the arpeggiator's keys while it plays a part: released keys
// stay in the pattern until pedal up, keys pressed meanwhile are added, sostenuto keeps only the keys down when pressed,
// pedal up with no key held ends the arpeggio without hanging notes, switching the arp's part releases everything.

#include "OhpeCore.h"
#include "TestHelpers.h"

#include <doctest/doctest.h>

#include <array>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr int block = 256;

MidiEvent cc (int ch, int number, int value, int offset = 0) { return MidiEvent::controller (ch, number, value, offset); }

// ---- the arpeggiator on its own (free-running, transport stopped)
struct ArpRig
{
    Arpeggiator arp;
    std::set<int> sounding; ///< arp notes on without their note-off yet
    std::vector<int> ons;   ///< every note-on, in order
    ArpRig (bool latch = false)
    {
        arp.prepare (sampleRate, block);
        arp.setParam (Arpeggiator::On, 1.0f);
        arp.setParam (Arpeggiator::Part, 1.0f);
        arp.setParam (Arpeggiator::Mode, (float) Arpeggiator::Up);
        arp.setParam (Arpeggiator::Rate, (float) seq::R1_16);
        arp.setParam (Arpeggiator::Gate, 0.5f);
        arp.setParam (Arpeggiator::Latch, latch ? 1.0f : 0.0f);
    }
    void run (int blocks, std::vector<MidiEvent> in = {})
    {
        for (const auto& e : in) arp.noteInput (e);
        for (int b = 0; b < blocks; ++b)
        {
            SequencerOutput out;
            Transport t; // stopped: free-running
            t.bpm = 120.0;
            arp.process (t, block, out);
            for (const auto& e : out.events)
            {
                if (e.isNoteOn()) { sounding.insert (e.data1); ons.push_back (e.data1); }
                else if (e.isNoteOff()) sounding.erase (e.data1);
            }
        }
    }
    std::set<int> playedSince (size_t from) const { return { ons.begin() + (std::ptrdiff_t) from, ons.end() }; }
};

// ---- the whole core: part 1 (channel 1) played through the arpeggiator
struct CoreRig
{
    std::unique_ptr<Core> owner = std::make_unique<Core>(); // on the heap (Windows stack)
    Core& core = *owner;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> all;
    CoreRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        set ("p1.engine", (float) EngineType::Wavetable);
        set ("p1.wavetable.unison", 0.0f);
        set ("p1.wavetable.univoices", 1.0f);
        set ("p1.amp.a", 0.001f);
        set ("p1.amp.s", 1.0f);
        set ("p1.amp.r", 0.01f);
        set ("seq.arp.part", 1.0f);
        set ("seq.arp.on", 1.0f);
        run (2);
    }
    void set (const std::string& id, float v) { REQUIRE_MESSAGE (core.params().set (id, v), id); }
    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            Transport t;
            core.process (out, block, b == 0 && ! midi.empty() ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, t);
            all.insert (all.end(), l[0].begin(), l[0].end());
        }
    }
    Arpeggiator& arp() { return core.arpeggiator(); }
    int voices() { return core.part (0).activeVoiceCount(); }
    void checkClean()
    {
        CHECK (allFinite (all));
        CHECK (noDenormals (all));
        CHECK (peak (all) < 4.0f);
    }
};
} // namespace

TEST_CASE ("p60 arp sustain: released keys stay in the pattern, keys pressed meanwhile are added, pedal up ends it")
{
    ArpRig rig;
    rig.run (1, { MidiEvent::noteOn (1, 60, 100), MidiEvent::noteOn (1, 64, 100) });
    rig.run (1, { cc (1, 64, 127) });
    CHECK (rig.arp.sustainDown());
    rig.run (1, { MidiEvent::noteOff (1, 60), MidiEvent::noteOff (1, 64) });
    CHECK (rig.arp.heldCount() == 2);
    rig.run (1, { MidiEvent::noteOn (1, 67, 100), MidiEvent::noteOff (1, 67, 10) }); // a short key under the pedal
    CHECK (rig.arp.heldCount() == 3);
    const size_t from = rig.ons.size();
    rig.run (200); // ~1 s, many steps
    CHECK (rig.playedSince (from) == std::set<int> { 60, 64, 67 });
    rig.run (1, { cc (1, 64, 0) });
    CHECK_FALSE (rig.arp.sustainDown());
    CHECK (rig.arp.heldCount() == 0);
    const size_t after = rig.ons.size();
    rig.run (100);
    CHECK (rig.ons.size() == after); // stopped
    CHECK (rig.sounding.empty());   // no hanging note
    CHECK (rig.arp.playPosition() == -1);
}

TEST_CASE ("p60 arp sustain: pedal up keeps the keys still held, drops the rest")
{
    ArpRig rig;
    rig.run (1, { MidiEvent::noteOn (1, 60, 100), cc (1, 64, 127), MidiEvent::noteOn (1, 64, 100) });
    rig.run (1, { MidiEvent::noteOff (1, 64) });
    CHECK (rig.arp.heldCount() == 2);
    rig.run (1, { cc (1, 64, 0) });
    CHECK (rig.arp.heldCount() == 1);
    const size_t from = rig.ons.size();
    rig.run (100);
    CHECK (rig.playedSince (from) == std::set<int> { 60 });
    rig.run (1, { MidiEvent::noteOff (1, 60) });
    rig.run (50);
    CHECK (rig.sounding.empty());
}

TEST_CASE ("p60 arp sostenuto: holds only the keys down when it went down")
{
    ArpRig rig;
    rig.run (1, { MidiEvent::noteOn (1, 60, 100) });
    rig.run (1, { cc (1, 66, 127) });
    CHECK (rig.arp.sostenutoDown());
    rig.run (1, { MidiEvent::noteOn (1, 67, 100) });
    rig.run (1, { MidiEvent::noteOff (1, 60), MidiEvent::noteOff (1, 67) });
    CHECK (rig.arp.heldCount() == 1); // 60 held by the pedal, 67 (pressed later) is gone
    const size_t from = rig.ons.size();
    rig.run (100);
    CHECK (rig.playedSince (from) == std::set<int> { 60 });
    rig.run (1, { cc (1, 66, 0) });
    CHECK (rig.arp.heldCount() == 0);
    rig.run (50);
    CHECK (rig.sounding.empty());
}

TEST_CASE ("p60 arp sustain + latch: the set stays after pedal up, keys under the pedal add to the latched set")
{
    ArpRig rig (true);
    rig.run (1, { MidiEvent::noteOn (1, 60, 100), MidiEvent::noteOff (1, 60, 50) });
    CHECK (rig.arp.heldCount() == 1);
    rig.run (1, { cc (1, 64, 127), MidiEvent::noteOn (1, 64, 100, 10), MidiEvent::noteOff (1, 64, 60) });
    CHECK (rig.arp.heldCount() == 2); // added, not a new set
    rig.run (1, { cc (1, 64, 0) });
    CHECK (rig.arp.heldCount() == 2); // latch keeps it
    rig.run (1, { MidiEvent::noteOn (1, 72, 100) });
    CHECK (rig.arp.heldCount() == 1); // without the pedal: a new set
    // unlatch while the pedal holds: the pedal-held key stays until pedal up
    rig.run (1, { cc (1, 64, 127), MidiEvent::noteOff (1, 72, 10) });
    rig.arp.setParam (Arpeggiator::Latch, 0.0f);
    CHECK (rig.arp.heldCount() == 1);
    rig.run (1, { cc (1, 64, 0) });
    CHECK (rig.arp.heldCount() == 0);
    rig.run (50);
    CHECK (rig.sounding.empty());
}

TEST_CASE ("p60 arp sustain in the core: CC 64 from MIDI and from the controller queue holds the arp, not the voices")
{
    for (const bool controller : { false, true })
    {
        CAPTURE (controller);
        CoreRig rig;
        auto send = [&] (const MidiEvent& e) {
            if (controller) rig.core.queueControllerEvent (0, e);
            else rig.run (1, { e });
        };
        send (MidiEvent::noteOn (1, 60, 100));
        send (MidiEvent::noteOn (1, 64, 100));
        send (cc (1, 64, 127));
        send (MidiEvent::noteOff (1, 60));
        send (MidiEvent::noteOff (1, 64));
        rig.run (4);
        CHECK (rig.arp().sustainDown());
        CHECK (rig.arp().heldCount() == 2);
        CHECK_FALSE (rig.core.part (0).sustainPedal()); // the voices are not held (the arp's notes stay crisp)
        rig.run (100);
        CHECK (rig.arp().heldCount() == 2);
        send (cc (1, 64, 0));
        rig.run (1);
        CHECK (rig.arp().heldCount() == 0);
        rig.run (60);
        CHECK (rig.voices() == 0); // no hanging notes
        rig.checkClean();
    }
}

TEST_CASE ("p60 arp sostenuto in the core: only the keys down at press time")
{
    CoreRig rig;
    rig.run (1, { MidiEvent::noteOn (1, 60, 100) });
    rig.run (1, { cc (1, 66, 127), MidiEvent::noteOn (1, 67, 100, 20) });
    rig.run (1, { MidiEvent::noteOff (1, 60), MidiEvent::noteOff (1, 67) });
    CHECK (rig.arp().heldCount() == 1);
    CHECK_FALSE (rig.core.part (0).sostenutoPedal());
    rig.run (1, { cc (1, 66, 0) });
    CHECK (rig.arp().heldCount() == 0);
    rig.run (60);
    CHECK (rig.voices() == 0);
    rig.checkClean();
}

TEST_CASE ("p60 arp sustain: switching the arp to another part or off releases the pedal-held set")
{
    for (const bool off : { false, true })
    {
        CAPTURE (off);
        CoreRig rig;
        rig.run (1, { MidiEvent::noteOn (1, 60, 100), MidiEvent::noteOn (1, 64, 100), cc (1, 64, 127) });
        rig.run (1, { MidiEvent::noteOff (1, 60), MidiEvent::noteOff (1, 64) });
        rig.run (20);
        REQUIRE (rig.arp().heldCount() == 2);
        if (off) rig.set ("seq.arp.on", 0.0f);
        else rig.set ("seq.arp.part", 2.0f);
        rig.run (1);
        CHECK (rig.arp().heldCount() == 0);
        CHECK_FALSE (rig.arp().sustainDown());
        rig.run (60);
        CHECK (rig.voices() == 0); // part 1: nothing hangs
        rig.run (1, { cc (1, 64, 0) }); // the pedal's late release changes nothing
        rig.run (20);
        CHECK (rig.voices() == 0);
        rig.checkClean();
    }
}

TEST_CASE ("p60 arp sustain: a pedal down on the part before the arp took it over comes up (no stuck voices)")
{
    CoreRig rig;
    rig.set ("seq.arp.on", 0.0f);
    rig.run (1);
    rig.run (1, { MidiEvent::noteOn (1, 60, 100), cc (1, 64, 127) });
    rig.run (1, { MidiEvent::noteOff (1, 60) });
    rig.run (10);
    REQUIRE (rig.core.part (0).sustainPedal());
    REQUIRE (rig.voices() >= 1);
    rig.set ("seq.arp.on", 1.0f); // the pedal goes to the arp from now on: its release would never reach the part
    rig.run (1);
    CHECK_FALSE (rig.core.part (0).sustainPedal());
    rig.run (40);
    CHECK (rig.voices() == 0);
}

TEST_CASE ("p60 arp sustain: one-finger chords and scale quantise under the pedal leave nothing hanging")
{
    CoreRig rig;
    rig.set ("p1.chord", (float) scales::Triad);
    rig.set ("key.lock", 1.0f);
    rig.run (1);
    rig.run (1, { MidiEvent::noteOn (1, 60, 100), cc (1, 64, 127) });
    rig.run (1, { MidiEvent::noteOff (1, 60), MidiEvent::noteOn (1, 61, 100, 30) });
    rig.run (1, { MidiEvent::noteOff (1, 61) });
    CHECK (rig.arp().heldCount() >= 3); // the chord's notes, held by the pedal
    rig.run (50);
    rig.run (1, { cc (1, 64, 0) });
    CHECK (rig.arp().heldCount() == 0);
    rig.run (60);
    CHECK (rig.voices() == 0);
    rig.checkClean();
}
