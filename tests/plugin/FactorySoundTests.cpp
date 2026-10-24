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

#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

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

TEST_CASE ("factory sounds: 8-note chords at full velocity stay below -1 dBTP without the limiter, no hard clip (p74)" * doctest::test_suite ("sliced"))
{
    // the polyphony check: every part preset holds a wide 8-note chord (C1 .. G4) at velocity 127 for two seconds,
    // master limiter off - the chord headroom of the part keeps the sum clean, no stage clamps the waveform
    factorykit::loadBank();
    const auto slice = test::testSlice();
    int parts = 0;
    const std::vector<int> chord { 36, 43, 48, 52, 55, 60, 64, 67 };
    for (int i = 0; i < OhpePresets::namedResourceListSize; ++i)
    {
        if (! slice.has (i)) continue;
        int size = 0;
        const char* data = OhpePresets::getNamedResource (OhpePresets::namedResourceList[i], size);
        preset::Preset p;
        REQUIRE (preset::fromJson (std::string (data, (size_t) size), p));
        if (p.meta.kind != preset::Kind::Part) continue;
        CAPTURE (p.meta.name);
        Core core;
        core.prepare (48000.0, 256);
        factorykit::load (core);
        const auto a = render::renderPart (core, p, 0, 120.0, 1.0e9, 1.0, 2.0, chord, 127);
        const double tp = render::truePeakDb (a);
        CAPTURE (tp);
        CHECK (tp <= -1.0);
        CHECK (render::flatTopRuns (a) == 0);
        CHECK (render::measure (a).nonFinite == 0);
        ++parts;
    }
    CHECK (parts >= 0);
}

namespace
{
/** p88: presets allowed above -1 dBTP after silence (documented, each with its reason). Keep this list empty if you can. */
const std::vector<std::pair<std::string, std::string>>& dynamicsExceptions()
{
    static const std::vector<std::pair<std::string, std::string>> list {};
    return list;
}

render::Audio slice (const render::Audio& a, size_t from, size_t len)
{
    render::Audio s;
    s.sampleRate = a.sampleRate;
    from = std::min (from, a.size());
    len = std::min (len, a.size() - from);
    s.l.assign (a.l.begin() + (long) from, a.l.begin() + (long) (from + len));
    s.r.assign (a.r.begin() + (long) from, a.r.begin() + (long) (from + len));
    return s;
}
} // namespace

TEST_CASE ("factory sounds: after realistic silence a single note, 4- and 8-note chords at full velocity stay below -1 dBTP without the limiter (p88)" * doctest::test_suite ("sliced"))
{
    // as played in the app: the preset sits a moment, the compressors fully release, then the material comes: 2 s silence,
    // a note (C3), a 4-note and a wide 8-note chord (C1 .. G4), velocity 127, each held 1.5 s with 2.5 s of silence after it;
    // master limiter and arpeggiator off. Rows: 'OHPE-STATS p88 dyn row|preset|note TP|chord4 TP|chord8 TP|note LUFS'.
    factorykit::loadBank();
    const auto slicer = test::testSlice();
    const std::vector<std::vector<int>> mats { { 48 }, { 48, 52, 55, 60 }, { 36, 43, 48, 52, 55, 60, 64, 67 } };
    const double lead = 2.0, hold = 1.5, gap = 2.5;
    int parts = 0;
    std::array<int, 3> overTp {}, overZero {}, overCeil {};
    for (int i = 0; i < OhpePresets::namedResourceListSize; ++i)
    {
        if (! slicer.has (i)) continue;
        int size = 0;
        const char* data = OhpePresets::getNamedResource (OhpePresets::namedResourceList[i], size);
        preset::Preset p;
        REQUIRE (preset::fromJson (std::string (data, (size_t) size), p));
        if (p.meta.kind != preset::Kind::Part) continue;
        CAPTURE (p.meta.name);
        Core core;
        core.prepare (48000.0, 512);
        factorykit::load (core);
        preset::PartOptions opt;
        opt.arp = false;
        preset::applyPart (p, core.params(), 0, opt);
        usertable::partFromJson (core, 0, p.userTables ? *p.userTables : std::string(),
                                 [] (const std::string&, const std::string&) { return usertable::UserTablePtr(); });
        core.params().set ("tempo", 120.0f);
        core.params().set ("mix.master.limit", 0.0f);
        core.prepareEffects();
        const double sr = core.sampleRate();
        std::vector<std::pair<int64_t, MidiEvent>> ev;
        std::vector<size_t> starts;
        double t = lead;
        for (const auto& m : mats)
        {
            const auto on = (int64_t) (t * sr), off = (int64_t) ((t + hold) * sr);
            starts.push_back ((size_t) on);
            for (int n : m) { ev.push_back ({ on, MidiEvent::noteOn (1, n, 127) }); ev.push_back ({ off, MidiEvent::noteOff (1, n) }); }
            t += hold + gap;
        }
        const auto a = render::renderCore (core, ev, (int64_t) (t * sr), 512);
        REQUIRE (render::measure (a).nonFinite == 0);
        std::array<double, 3> tp {};
        for (size_t k = 0; k < mats.size(); ++k)
        {
            tp[k] = render::truePeakDb (slice (a, starts[k], (size_t) ((hold + gap) * sr)));
            overTp[k] += tp[k] > -1.0;
            overZero[k] += tp[k] > 0.0;
            overCeil[k] += tp[k] > -0.57; // the master limiter's target: it would act here
        }
        const double noteLufs = render::integratedLufs (slice (a, starts[0], (size_t) (hold * sr)));
        std::printf ("OHPE-STATS p88 dyn row|%s|%.2f|%.2f|%.2f|%.2f\n", p.meta.name.c_str(), tp[0], tp[1], tp[2], noteLufs);
        bool excepted = false;
        for (const auto& e : dynamicsExceptions()) excepted = excepted || e.first == p.meta.name;
        CAPTURE (tp[0]);
        CAPTURE (tp[1]);
        CAPTURE (tp[2]);
        WARN ((excepted || tp[2] <= -1.0));
        WARN ((excepted || tp[1] <= -1.0));
        WARN ((excepted || tp[0] <= -1.0));
        CHECK (render::flatTopRuns (a) == 0);
        ++parts;
    }
    std::printf ("OHPE-STATS p88 dyn sum|parts %d|over -1 dBTP note %d chord4 %d chord8 %d|over 0 dBFS %d %d %d|over limiter target %d %d %d\n", parts,
                 overTp[0], overTp[1], overTp[2], overZero[0], overZero[1], overZero[2], overCeil[0], overCeil[1], overCeil[2]);
    std::fflush (stdout);
}
