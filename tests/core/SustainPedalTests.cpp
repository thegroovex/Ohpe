// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p58: the sustain pedal (CC 64) and sostenuto (CC 66) hold a part's notes after their note-off and let them go when
// the pedal comes up - from the controller queue (the Grid-Controller's pedal jack), from host / device MIDI, in a
// normal and in an MPE / Fingerboard part (the pedal on the master channel holds the member channels' notes).

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "midi/MpeInput.h"

#include <doctest/doctest.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr int block = 256;

struct PedalRig
{
    std::unique_ptr<Core> owner = std::make_unique<Core>(); // on the heap (Windows stack)
    Core& core = *owner;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> all;
    explicit PedalRig (bool mpe = false, bool fingerboard = false)
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
        if (mpe) set ("p1.mpe", 1.0f);
        if (fingerboard)
        {
            set ("p1.mpe.bend", 96.0f);
            set ("p1.mpe.onset", (float) MpeInput::OnsetPressure);
            set ("p1.mpe.hd", (float) MpeInput::HdAuto);
        }
        run (2);
    }
    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }
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
    int voices() { return core.part (0).activeVoiceCount(); }
};

MidiEvent cc (int ch, int number, int value) { return MidiEvent::controller (ch, number, value, 0); }
} // namespace

TEST_CASE ("p58 sustain: CC 64 from the controller queue holds the note after note-off, pedal up releases it")
{
    PedalRig rig;
    rig.core.queueControllerEvent (0, MidiEvent::noteOn (1, 60, 100));
    rig.run (2);
    REQUIRE (rig.voices() >= 1);
    rig.core.queueControllerEvent (0, cc (1, 64, 127));
    rig.run (1);
    CHECK (rig.core.part (0).sustainPedal());
    rig.core.queueControllerEvent (0, MidiEvent::noteOff (1, 60));
    rig.run (40); // ~0.2 s, many times the release
    CHECK (rig.voices() >= 1);
    rig.core.queueControllerEvent (0, cc (1, 64, 0));
    rig.run (40);
    CHECK_FALSE (rig.core.part (0).sustainPedal());
    CHECK (rig.voices() == 0);
    CHECK (allFinite (rig.all));
    CHECK (noDenormals (rig.all));
    CHECK (peak (rig.all) < 4.0f);
}

TEST_CASE ("p58 sustain: the same from host / device MIDI (any channel of an omni part)")
{
    PedalRig rig;
    rig.run (2, { MidiEvent::noteOn (3, 62, 100), cc (3, 64, 127) });
    rig.run (1, { MidiEvent::noteOff (3, 62) });
    rig.run (40);
    CHECK (rig.voices() >= 1);
    rig.run (40, { cc (3, 64, 0) });
    CHECK (rig.voices() == 0);
}

TEST_CASE ("p58 sustain: an MPE / Fingerboard part - the pedal on the master channel holds every member channel")
{
    for (const bool fingerboard : { false, true })
    {
        CAPTURE (fingerboard);
        PedalRig rig (true, fingerboard);
        // like the controller's pads: notes on member channels 2 and 3, the pedal on channel 1
        rig.core.queueControllerEvent (0, MidiEvent::noteOn (2, 60, 100));
        rig.core.queueControllerEvent (0, MidiEvent::noteOn (3, 64, 100));
        if (fingerboard)
        {
            MidiEvent pr;
            pr.status = 0xd1;
            pr.data1 = 100;
            rig.core.queueControllerEvent (0, pr);
            pr.status = 0xd2;
            rig.core.queueControllerEvent (0, pr);
        }
        rig.core.queueControllerEvent (0, cc (1, 64, 127));
        rig.run (2);
        const int held = rig.voices();
        REQUIRE (held >= 2);
        rig.core.queueControllerEvent (0, MidiEvent::noteOff (2, 60));
        rig.core.queueControllerEvent (0, MidiEvent::noteOff (3, 64));
        rig.run (40);
        CHECK (rig.voices() >= 2);
        rig.core.queueControllerEvent (0, cc (1, 64, 0));
        rig.run (40);
        CHECK (rig.voices() == 0);
        CHECK (allFinite (rig.all));
    }
}

TEST_CASE ("p58 sostenuto: CC 66 from the controller queue holds the keys down when pressed, not later ones")
{
    PedalRig rig;
    rig.core.queueControllerEvent (0, MidiEvent::noteOn (1, 60, 100));
    rig.run (1);
    const int one = rig.voices(); // the voices of one note
    REQUIRE (one >= 1);
    rig.core.queueControllerEvent (0, cc (1, 66, 127));
    rig.run (1);
    CHECK (rig.core.part (0).sostenutoPedal());
    rig.core.queueControllerEvent (0, MidiEvent::noteOn (1, 67, 100)); // after the pedal: not held
    rig.run (1);
    rig.core.queueControllerEvent (0, MidiEvent::noteOff (1, 60));
    rig.core.queueControllerEvent (0, MidiEvent::noteOff (1, 67));
    rig.run (40);
    CHECK (rig.voices() == one);
    rig.core.queueControllerEvent (0, cc (1, 66, 0));
    rig.run (40);
    CHECK (rig.voices() == 0);
}
