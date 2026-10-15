// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "midi/MidiMap.h"

#include <doctest/doctest.h>

using namespace ohpe;

namespace
{
struct Rig
{
    ParamRegistry reg;
    MidiMap map;
    std::unique_ptr<MidiMapper> mapper;
    Rig()
    {
        reg.add (pd::continuous ("cut", "Cutoff", "Cutoff", 0.0f, 100.0f, 0.0f));
        reg.add (pd::toggle ("mute", "Mute", "Stumm", false));
        reg.add (pd::continuous ("vol", "Volume", "Lautstärke", 0.0f, 1.0f, 0.5f));
        reg.freeze();
        mapper = std::make_unique<MidiMapper> (reg);
        mapper->publish (map);
    }
};
} // namespace

TEST_CASE ("midi learn: the next CC is assigned to the armed parameter")
{
    Rig r;
    r.mapper->armLearn (0);
    CHECK (r.mapper->learningParam() == 0);
    CHECK (r.mapper->handleMidi (MidiEvent::controller (3, 74, 10))); // consumed by learn
    CHECK (r.mapper->learningParam() == -1);
    CHECK (r.mapper->applyPendingLearns (r.map));
    REQUIRE (r.map.mappings().size() == 1);
    const auto& m = r.map.mappings()[0];
    CHECK (m.paramId == "cut");
    CHECK (m.type == MidiSourceType::CC);
    CHECK (m.channel == 3);
    CHECK (m.number == 74);

    // now the CC drives the parameter (absolute) and reports the change
    CHECK (r.mapper->handleMidi (MidiEvent::controller (3, 74, 127)));
    CHECK (r.reg.get (0) == doctest::Approx (100.0f));
    ParamChange ch;
    REQUIRE (r.mapper->popChange (ch));
    CHECK (ch.paramIndex == 0);
    CHECK (ch.normValue == doctest::Approx (1.0f));
    // other channel / other CC are not consumed
    CHECK_FALSE (r.mapper->handleMidi (MidiEvent::controller (4, 74, 0)));
    CHECK_FALSE (r.mapper->handleMidi (MidiEvent::controller (3, 75, 0)));
    CHECK (r.reg.get (0) == doctest::Approx (100.0f));
}

TEST_CASE ("midi learn on a note maps a toggle parameter as toggle")
{
    Rig r;
    r.mapper->armLearn (1);
    r.mapper->handleMidi (MidiEvent::noteOn (1, 36, 100));
    r.mapper->applyPendingLearns (r.map);
    REQUIRE (r.map.mappings().size() == 1);
    CHECK (r.map.mappings()[0].mode == MidiMapMode::Toggle);
    r.mapper->handleMidi (MidiEvent::noteOn (1, 36, 100));
    CHECK (r.reg.get (1) == 1.0f);
    r.mapper->handleMidi (MidiEvent::noteOff (1, 36));
    CHECK (r.reg.get (1) == 1.0f);
    r.mapper->handleMidi (MidiEvent::noteOn (1, 36, 100));
    CHECK (r.reg.get (1) == 0.0f);
}

TEST_CASE ("midi map: relative mode, ranges, omni channel, clear")
{
    Rig r;
    MidiMapping m;
    m.paramId = "vol";
    m.channel = 0;
    m.number = 20;
    m.mode = MidiMapMode::Relative;
    r.map.add (m);
    MidiMapping m2;
    m2.paramId = "cut";
    m2.number = 21;
    m2.minNorm = 0.2f;
    m2.maxNorm = 0.4f;
    r.map.add (m2);
    r.mapper->publish (r.map);

    r.mapper->handleMidi (MidiEvent::controller (9, 20, 10));  // +10
    CHECK (r.reg.get (2) == doctest::Approx (0.5f + 10.0f / 127.0f));
    r.mapper->handleMidi (MidiEvent::controller (1, 20, 118)); // -10
    CHECK (r.reg.get (2) == doctest::Approx (0.5f));
    r.mapper->handleMidi (MidiEvent::controller (1, 21, 127));
    CHECK (r.reg.get (0) == doctest::Approx (40.0f));
    r.mapper->handleMidi (MidiEvent::controller (1, 21, 0));
    CHECK (r.reg.get (0) == doctest::Approx (20.0f));

    CHECK (r.map.clearParam ("vol") == 1);
    r.mapper->publish (r.map);
    CHECK_FALSE (r.mapper->handleMidi (MidiEvent::controller (1, 20, 10)));
}

TEST_CASE ("midi map: JSON round-trip and malformed input")
{
    MidiMap a;
    MidiMapping m;
    m.paramId = "p1.analog.cutoff";
    m.type = MidiSourceType::Note;
    m.channel = 10;
    m.number = 42;
    m.minNorm = 0.1f;
    m.maxNorm = 0.9f;
    m.mode = MidiMapMode::Toggle;
    a.add (m);
    m.paramId = "mix.ch1.vol";
    m.type = MidiSourceType::CC;
    m.mode = MidiMapMode::Relative;
    a.add (m);

    MidiMap b;
    REQUIRE (b.fromJson (a.toJson()));
    REQUIRE (b.mappings().size() == 2);
    CHECK (b.mappings()[0].paramId == "p1.analog.cutoff");
    CHECK (b.mappings()[0].type == MidiSourceType::Note);
    CHECK (b.mappings()[0].channel == 10);
    CHECK (b.mappings()[0].number == 42);
    CHECK (b.mappings()[0].minNorm == doctest::Approx (0.1f));
    CHECK (b.mappings()[0].mode == MidiMapMode::Toggle);
    CHECK (b.mappings()[1].mode == MidiMapMode::Relative);

    CHECK_FALSE (b.fromJson ("not json"));
    CHECK_FALSE (b.fromJson ("{\"format\":\"other\"}"));
    CHECK (b.mappings().size() == 2); // unchanged
}

TEST_CASE ("midi map: unknown parameter IDs are kept but inactive")
{
    Rig r;
    MidiMapping m;
    m.paramId = "from.a.newer.version";
    m.number = 1;
    r.map.add (m);
    r.mapper->publish (r.map);
    CHECK_FALSE (r.mapper->handleMidi (MidiEvent::controller (1, 1, 64)));
    CHECK (r.map.mappings().size() == 1);
}
