// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"
#include "model/StateSerializer.h"

#include <doctest/doctest.h>

using namespace ohpe;

TEST_CASE ("state: full round-trip of parameters, MIDI map, pattern and UI section")
{
    Core a;
    auto& reg = a.params();
    // set every parameter to a non-default value inside its range
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        reg.setNormalised (i, d.toNormalised (d.defaultValue) > 0.5f ? 0.2f : 0.8f);
    }
    MidiMapping m;
    m.paramId = "p1.fm.index";
    m.number = 71;
    a.midiMap().add (m);
    a.midiMapChanged();
    StepSequencer::Step s;
    s.on = true;
    s.note = 50;
    a.stepSequencer().setStep (3, s);

    const auto json = a.saveState ("{\"lang\":\"de\"}");

    Core b;
    std::string ui;
    REQUIRE (b.loadState (json, &ui));
    for (int i = 0; i < reg.size(); ++i)
    {
        CAPTURE (reg.descriptor (i).id);
        const auto& id = reg.descriptor (i).id;
        if (id == "pads.stutter" || id == "pads.stop" || id == "pads.sweep" || id == "pads.freeze")
            CHECK (b.params().get (i) == 0.0f); // p28: held / latched pads are not restored
        else
            CHECK (b.params().get (i) == reg.get (i));
    }
    REQUIRE (b.midiMap().mappings().size() == 1);
    CHECK (b.midiMap().mappings()[0].paramId == "p1.fm.index");
    CHECK (b.stepSequencer().getStep (3).on);
    CHECK (b.stepSequencer().getStep (3).note == 50);
    CHECK (ui.find ("\"de\"") != std::string::npos);
}

TEST_CASE ("state: unknown IDs ignored, missing IDs return to defaults, garbage rejected")
{
    Core c;
    auto& reg = c.params();
    reg.set ("p1.vol", 0.1f);
    const auto r = state::loadState (R"({"format":"ohpe-state","version":7,"params":{"tempo":99,"zz.future":3}})", reg);
    CHECK (r.ok);
    CHECK (r.applied == 1);
    CHECK (r.unknown == 1);
    CHECK (reg.get ("tempo") == 99.0f);
    CHECK (reg.get ("p1.vol") == doctest::Approx (0.8f)); // default again
    CHECK_FALSE (c.loadState ("garbage"));
    CHECK_FALSE (c.loadState (R"({"format":"something-else"})"));
}

TEST_CASE ("preset: save part 1 and load with prefix mapping")
{
    Core c;
    auto& reg = c.params();
    reg.set ("p1.analog.cutoff", 1234.0f);
    reg.set ("p1.engine", 0.0f);
    reg.set ("tempo", 150.0f);
    const auto preset = state::savePreset (reg, "p1.", "Bass");
    CHECK (preset.find ("tempo") == std::string::npos);

    reg.resetToDefaults();
    const auto r = state::loadPreset (preset, reg, "p1.");
    CHECK (r.ok);
    CHECK (reg.get ("p1.analog.cutoff") == doctest::Approx (1234.0f));
    CHECK (reg.get ("p1.engine") == 0.0f);
    CHECK (reg.get ("tempo") == 120.0f);
}
