// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.4 (package F): factory sounds with the embedded recordings - the sample bank decodes, every
// drum kit plays every pad, and every factory preset renders finite, unclipped audio inside its
// loudness window (the quality loop of tools/presets/analyse.py, as a test).

#include <doctest/doctest.h>

#include "OhpePresets.h"
#include "plugin/FactoryKit.h"
#include "render/PresetRender.h"
#include "sample/FactoryBank.h"
#include "../TestSlice.h"

using namespace ohpe;

TEST_CASE ("factory sounds: the sample bank is embedded and decodes (instruments, drum kits)")
{
    factorykit::loadBank();
    CHECK (factorykit::numBankFiles() > 100);
    CHECK (factory::numLoadedInstruments() == factory::numInstruments - 1);
    CHECK (factory::numLoadedKitSamples() == (factory::numKits - 1) * drum::DrumKit::numPads);
    for (int i = 1; i < factory::numInstruments; ++i)
    {
        CAPTURE (factory::instrumentId (i));
        const auto* m = factory::instrumentMap (i);
        REQUIRE (m != nullptr);
        CHECK (m->zones.size() >= 4);
        int root = 0;
        CHECK (factory::grainSource (i, root) != nullptr);
        CHECK (root > 30);
    }
    CHECK (factorykit::loadBank() == 0); // once per process
}

TEST_CASE ("factory sounds: every drum kit plays every pad")
{
    factorykit::loadBank();
    for (int kit = 0; kit < factory::numKits; ++kit)
    {
        Core core;
        core.prepare (48000.0, 256);
        factorykit::load (core);
        core.params().set ("drum.kit", (float) kit);
        for (int pad = 0; pad < drum::DrumKit::numPads; ++pad)
        {
            CAPTURE (kit);
            CAPTURE (pad);
            if (kit > 0) CHECK ((core.drumKit().effectiveSample (pad) != nullptr));
            core.drumKit().allNotesOff (true);
            const auto a = render::renderCore (core, { { 0, MidiEvent::noteOn (10, 36 + pad, 110) } }, 7200);
            const auto m = render::measure (a);
            CHECK (m.nonFinite == 0);
            CHECK (m.peakDb < 0.0);
            CHECK (m.peakDb > -40.0);
        }
    }
}

TEST_CASE ("factory sounds: every factory preset renders cleanly with the recordings, inside its level window" * doctest::test_suite ("sliced"))
{
    factorykit::loadBank();
    const auto slice = test::testSlice(); // CI renders the presets in several processes
    int parts = 0, projects = 0;
    for (int i = 0; i < OhpePresets::namedResourceListSize; ++i)
    {
        int size = 0;
        const char* data = OhpePresets::getNamedResource (OhpePresets::namedResourceList[i], size);
        preset::Preset p;
        REQUIRE (preset::fromJson (std::string (data, (size_t) size), p));
        CAPTURE (p.meta.name);
        if (! slice.has (i))
        {
            ++(p.meta.kind == preset::Kind::Part ? parts : projects); // counted, rendered by another slice
            continue;
        }
        Core core;
        core.prepare (48000.0, 256);
        factorykit::load (core);
        render::Audio a;
        const bool part = p.meta.kind == preset::Kind::Part;
        if (part)
        {
            ++parts;
            a = render::renderPart (core, p, 0, 100.0, 4.0, 1.0); // four beats of its phrase, one second of tail
        }
        else
        {
            ++projects;
            a = render::renderProject (core, p, 4.0);
        }
        const auto m = render::measure (a);
        CAPTURE (m.activeRmsDb);
        CAPTURE (m.peakDb);
        CHECK (m.nonFinite == 0);
        CHECK (m.denormals == 0);
        CHECK (std::abs (m.dc) < 0.005);
        if (part)
        {
            CHECK (m.peakDb < -0.5);          // no clipping, the limiter stays out of it
            CHECK (m.activeRmsDb > -21.0);    // level window -18 ... -12 dBFS (quality loop), with
            CHECK (m.activeRmsDb < -10.0);    // margin for the shorter phrase
            CHECK (m.hfDb < -3.0);            // no wide-band fizz above 16 kHz
        }
        else
        {
            CHECK (m.peakDb < 0.0);
            CHECK (m.activeRmsDb > -24.0);
            CHECK (m.activeRmsDb < -8.0);
        }
    }
    CHECK (parts >= 100);
    CHECK (projects >= 6);
}
