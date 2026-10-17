// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p39: metronome (own click), count-in for tape recording (sample-accurate), tap tempo.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <fstream>
#include <string>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
/** A core driven in blocks of 512 at 48 kHz (120 BPM: a beat is 24000 samples, a bar 96000 - not a block multiple). */
struct MetroRig
{
    Core core;
    static constexpr int block = 512;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> rec;

    MetroRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        set (Core::Ids::tempo, 120.0f);
    }
    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }

    void run (int blocks)
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            for (auto& v : l) std::fill (v.begin(), v.end(), 0.0f);
            for (auto& v : r) std::fill (v.begin(), v.end(), 0.0f);
            core.process (out, block, nullptr, 0, {});
            rec.insert (rec.end(), l[0].begin(), l[0].end());
            core.tape().housekeeping(); // the plug-in does this on the message thread
        }
    }
    /** Sample indices where a click starts: non-zero after at least 1000 silent samples. */
    std::vector<size_t> onsets() const
    {
        std::vector<size_t> o;
        size_t silent = 1000;
        for (size_t i = 0; i < rec.size(); ++i)
        {
            if (rec[i] != 0.0f && silent >= 1000) o.push_back (i);
            silent = rec[i] == 0.0f ? silent + 1 : 0;
        }
        return o;
    }
    void armAndRecord()
    {
        set ("tape.tr1.arm", 1.0f);
        set ("tape.rec", 1.0f);
        set ("tape.play", 1.0f);
    }
};
} // namespace

TEST_CASE ("metronome: parameters are registered at the end, excluded from presets")
{
    Core core;
    const auto& reg = core.params();
    // appended after the p38 tape triggers (later blocks follow, e.g. p45 Scale Lock)
    CHECK (reg.indexOf ("metro.on") == reg.indexOf ("tape.edit.clearall") + 1);
    CHECK (reg.indexOf ("metro.tap") == reg.indexOf ("metro.on") + 5);
    CHECK (reg.get ("metro.on") == 0.0f);
    CHECK (reg.get ("metro.countin") == 0.0f);
    CHECK (reg.get ("metro.plugin") == 0.0f);
    CHECK (reg.get ("metro.accent") == 1.0f);
    for (const char* id : { "metro.on", "metro.level", "metro.accent", "metro.countin", "metro.plugin", "metro.tap" })
        CHECK (preset::isExcludedFromProject (id));
}

TEST_CASE ("metronome: off by default - silence while the sequencer runs")
{
    MetroRig rig;
    rig.set (Core::Ids::seqRun, 1.0f);
    rig.run (200);
    CHECK (peak (rig.rec) == 0.0f);
    CHECK (rig.core.clickCount() == 0);
}

TEST_CASE ("metronome: clicks on every beat of the running sequencer, accent on the downbeat, bounded")
{
    MetroRig rig;
    rig.set ("metro.on", 1.0f);
    rig.run (40); // stopped: no click
    CHECK (peak (rig.rec) == 0.0f);
    rig.rec.clear();
    rig.set (Core::Ids::seqRun, 1.0f);
    rig.run (500); // 256000 samples: beats at 0, 24000, ... 240000
    const auto o = rig.onsets();
    REQUIRE (o.size() == 11);
    for (size_t i = 0; i < o.size(); ++i) CHECK (o[i] == i * 24000);
    CHECK (allFinite (rig.rec));
    CHECK (noDenormals (rig.rec));
    CHECK (peak (rig.rec) <= 0.6f);
    CHECK (peak (rig.rec) > 0.05f);
    const float down = peak (rig.rec, 0, 2000), up = peak (rig.rec, 24000, 26000);
    CHECK (down > up * 1.2f); // beat 1 accented
    rig.rec.clear();
    rig.set (Core::Ids::seqRun, 0.0f);
    rig.run (200);
    CHECK (peak (rig.rec, 4000) == 0.0f); // stopped: silent again (after the last tail)
}

TEST_CASE ("metronome: in a plug-in host only with 'click in plug-in'")
{
    MetroRig rig;
    rig.core.setStandaloneHost (false);
    rig.set ("metro.on", 1.0f);
    rig.set (Core::Ids::seqRun, 1.0f);
    rig.run (100);
    CHECK (peak (rig.rec) == 0.0f);
    rig.set ("metro.plugin", 1.0f);
    rig.run (100);
    CHECK (peak (rig.rec) > 0.05f);
}

TEST_CASE ("metronome: level 0 is silent, the tape alone also clicks")
{
    MetroRig rig;
    rig.set ("metro.on", 1.0f);
    rig.set ("metro.level", 0.0f);
    rig.set (Core::Ids::seqRun, 1.0f);
    rig.run (100);
    CHECK (peak (rig.rec) == 0.0f);
    rig.set (Core::Ids::seqRun, 0.0f);
    rig.set ("metro.level", 0.6f);
    rig.rec.clear();
    rig.set ("tape.play", 1.0f); // tape rolling, sequencer stopped
    rig.run (100);
    CHECK (rig.onsets().size() >= 2);
}

TEST_CASE ("count-in: one bar, the tape starts recording exactly on the bar (sample-accurate)")
{
    MetroRig rig;
    rig.set ("metro.countin", 1.0f); // 1 bar, metronome off
    rig.armAndRecord();
    rig.run (100);
    CHECK (rig.core.countInActive());
    CHECK (rig.core.countInBeatsLeft() >= 1);
    CHECK (rig.core.tape().positionFrames() == 0); // the tape waits
    rig.run (100); // 200 blocks = 102400 samples > 96000
    CHECK_FALSE (rig.core.countInActive());
    CHECK (rig.core.lastCountInRelease() == 96000);
    CHECK (rig.core.tape().positionFrames() == 200 * 512 - 96000);
    const auto o = rig.onsets();
    REQUIRE (o.size() == 4); // four count-in clicks, none after (the metronome is off)
    for (size_t i = 0; i < 4; ++i) CHECK (o[i] == i * 24000);
    CHECK (rig.core.clickCount() == 4);
    CHECK (peak (rig.rec, 0, 2000) > peak (rig.rec, 24000, 26000) * 1.2f); // beat 1 accented
}

TEST_CASE ("count-in: two bars, countdown, the metronome carries on from the first recorded beat")
{
    MetroRig rig;
    rig.set ("metro.countin", 2.0f);
    rig.set ("metro.on", 1.0f);
    rig.armAndRecord();
    rig.run (1);
    CHECK (rig.core.countInBeatsLeft() == 8);
    rig.run (50); // 26112 samples: beat 2 has begun
    CHECK (rig.core.countInBeatsLeft() == 7);
    rig.run (400); // 451 blocks = 230912 samples
    CHECK (rig.core.lastCountInRelease() == 192000);
    const auto o = rig.onsets();
    REQUIRE (o.size() >= 10);
    for (size_t i = 0; i < 10; ++i) CHECK (o[i] == i * 24000); // 8 count-in clicks, then the tape's beats from 192000
}

TEST_CASE ("count-in: a sequencer started with the tape waits and starts on the release sample")
{
    MetroRig rig;
    rig.set ("metro.countin", 1.0f);
    rig.set (Core::Ids::seqRun, 1.0f); // PLAY starts the tape (linked): both rise together
    rig.armAndRecord();
    rig.run (100);
    CHECK_FALSE (rig.core.transportPlaying());
    rig.run (88); // block 187 holds sample 96000 at offset 256
    CHECK (rig.core.lastCountInRelease() == 96000);
    CHECK (rig.core.transportPlaying());
    CHECK (rig.core.transportPpq() == doctest::Approx (-256.0 / 24000.0)); // quarter note 0 on sample 96000
    rig.run (1);
    CHECK (rig.core.transportPpq() == doctest::Approx (256.0 / 24000.0));
}

TEST_CASE ("count-in: stopping during the count-in records nothing; off = immediate start")
{
    MetroRig rig;
    rig.set ("metro.countin", 1.0f);
    rig.armAndRecord();
    rig.run (50);
    rig.set ("tape.play", 0.0f);
    rig.run (300);
    CHECK_FALSE (rig.core.countInActive());
    CHECK (rig.core.lastCountInRelease() == -1);
    CHECK (rig.core.tape().positionFrames() == 0);

    MetroRig direct; // no count-in: the tape runs at once, no click
    direct.armAndRecord();
    direct.run (10);
    CHECK (direct.core.tape().positionFrames() == 10 * 512);
    CHECK (direct.core.clickCount() == 0);
    CHECK (peak (direct.rec) == 0.0f);
}

TEST_CASE ("count-in: with the sequencer already running, the recording starts on its next bar line")
{
    MetroRig rig;
    rig.set ("metro.countin", 1.0f);
    rig.set (Core::Ids::seqRun, 1.0f);
    rig.run (100); // 51200 samples = 2.133 beats
    rig.armAndRecord();
    rig.run (200);
    // bar line at beat 4 (sample 96000 on the sequencer's grid): between N-1 and N bars after the press
    CHECK (rig.core.lastCountInRelease() == 96000);
}

TEST_CASE ("count-in: without an armed track or with REC off there is no count-in")
{
    MetroRig rig;
    rig.set ("metro.countin", 2.0f);
    rig.set ("tape.play", 1.0f); // plain play
    rig.run (10);
    CHECK_FALSE (rig.core.countInActive());
    CHECK (rig.core.tape().positionFrames() == 10 * 512);
}

TEST_CASE ("tap tempo: average of the last intervals, a long gap starts over, clamped")
{
    TapTempo t;
    CHECK (t.tap (10.0) == 0.0);
    CHECK (t.tap (10.5) == doctest::Approx (120.0));
    CHECK (t.tap (11.0) == doctest::Approx (120.0));
    CHECK (t.tap (11.6) == doctest::Approx (60.0 / ((0.5 + 0.5 + 0.6) / 3.0)));
    CHECK (t.tap (20.0) == 0.0); // gap > 2 s
    CHECK (t.tap (20.1) == doctest::Approx (300.0)); // 600 BPM clamped
    TapTempo slow;
    slow.tap (0.0);
    CHECK (slow.tap (1.9) == doctest::Approx (60.0 / 1.9));
}

TEST_CASE ("metronome: state round trip keeps the settings")
{
    Core a;
    a.params().set ("metro.on", 1.0f);
    a.params().set ("metro.level", 0.25f);
    a.params().set ("metro.countin", 2.0f);
    Core b;
    REQUIRE (b.loadState (a.saveState()));
    CHECK (b.params().get ("metro.on") == 1.0f);
    CHECK (b.params().get ("metro.level") == doctest::Approx (0.25f));
    CHECK (b.params().get ("metro.countin") == 2.0f);
}
