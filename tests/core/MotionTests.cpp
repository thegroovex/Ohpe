// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p22: the motion recorder - knob movements per part recorded as looping lanes, polymetric to the sequencer.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "mod/ModMatrix.h"
#include "motion/MotionRecorder.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <memory>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct Rig
{
    std::unique_ptr<Core> owner = std::make_unique<Core>(); // on the heap (Windows stack)
    Core& core = *owner;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    Stereo rec;
    double ppq = 0.0;
    double bpm = 120.0;
    bool playing = true;
    Rig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        set ("mix.master.limit", 0.0f);
        for (int p = 2; p <= 4; ++p) set ("p" + std::to_string (p) + ".vol", 0.0f);
        set ("seq.run", 1.0f); // the instrument's PLAY: the host transport runs the sequencers (and the lanes) only with it
    }
    void set (const std::string& id, float v) { REQUIRE_MESSAGE (core.params().set (id, v), id); }
    float get (const std::string& id) const { return core.params().get (id); }
    int index (const std::string& id) const { return core.params().indexOf (id); }
    float played (const std::string& id) const { return core.blockValue (index (id)); }
    /** Host transport at `ppq`, one block; perBlock runs before the block with the block's ppq. */
    void run (int blocks, const std::function<void (double)>& perBlock = {}, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            if (perBlock) perBlock (ppq);
            Transport t;
            t.hostDriven = true;
            t.playing = playing;
            t.bpm = bpm;
            t.ppqAtBlockStart = ppq;
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, t);
            rec.l.insert (rec.l.end(), l[0].begin(), l[0].end());
            rec.r.insert (rec.r.end(), r[0].begin(), r[0].end());
            if (playing) ppq += block * bpm / 60.0 / sampleRate;
        }
    }
    /** Blocks for `quarters` quarter notes. */
    int blocksFor (double quarters) const { return (int) std::ceil (quarters * 60.0 / bpm * sampleRate / block); }
};

/** A smooth gesture over one quarter (normalised 0.1 .. 0.9). */
float gesture (double ppq) { return 0.5f + 0.4f * (float) std::sin (2.0 * 3.14159265358979 * std::fmod (ppq, 1.0)); }
} // namespace

TEST_CASE ("p22 motion: parameters appended after the bounce block; matrix sources Motion 1 .. 4")
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    const auto& reg = core.params();
    CHECK (reg.indexOf ("p1.motion.on") == reg.indexOf ("bounce.go") + 1); // after the character macros and the bounce block
    const char* keys[] = { "on", "rec", "len", "amt", "clear" };
    int expect = reg.indexOf ("p1.motion.on");
    for (int p = 1; p <= 4; ++p)
        for (const char* k : keys)
            CHECK (reg.indexOf ("p" + std::to_string (p) + ".motion." + k) == expect++);
    CHECK (reg.indexOf ("p4.motion.clear") == reg.size() - 1);
    CHECK (reg.get ("p1.motion.on") == 0.0f);
    CHECK (reg.get ("p1.motion.len") == 16.0f);
    CHECK (reg.get ("p1.motion.amt") == 1.0f);
    CHECK (mod::SrcMotion1 == mod::SrcMorphY + 1);
    CHECK (mod::sourceIndexOfKey ("motion1") == mod::SrcMotion1);
    CHECK (mod::sourceIndexOfKey ("motion4") == mod::SrcMotion4);
    CHECK (mod::sourceTable().size() == (size_t) mod::numSources);
    CHECK (MotionRecorder::isExcluded ("motion.len"));
    CHECK (MotionRecorder::isExcluded ("midich"));
    CHECK_FALSE (MotionRecorder::isExcluded ("analog.cutoff"));
}

TEST_CASE ("p22 motion: nothing recorded = neutral (bit-identical output, no overlay)")
{
    Rig a, b;
    b.set ("p1.motion.on", 1.0f);
    b.set ("p1.motion.rec", 1.0f);
    const auto note = std::vector<MidiEvent> { MidiEvent::noteOn (1, 57, 100, 0) };
    a.run (40, {}, note);
    b.run (40, {}, note);
    REQUIRE (a.rec.l.size() == b.rec.l.size());
    bool same = true;
    for (size_t i = 0; i < a.rec.l.size(); ++i) same = same && a.rec.l[i] == b.rec.l[i] && a.rec.r[i] == b.rec.r[i];
    CHECK (same);
    CHECK_FALSE (b.core.motion().anyLane());
    CHECK (b.played ("p1.analog.cutoff") == b.get ("p1.analog.cutoff"));
}

TEST_CASE ("p22 motion: a touched knob takes a lane and plays its movement back in a loop")
{
    Rig rig;
    rig.set ("p1.motion.len", 4.0f); // one quarter
    rig.set ("p1.motion.rec", 1.0f);
    rig.run (2); // armed
    const int cutoff = rig.index ("p1.analog.cutoff");
    const auto& d = rig.core.params().descriptor (cutoff);
    rig.run (rig.blocksFor (1.05), [&] (double ppq) { rig.core.params().setNormalised (cutoff, gesture (ppq)); });
    CHECK (rig.core.motion().laneTarget (0, 0) == cutoff);
    CHECK (rig.core.motion().laneTarget (0, 1) == -1);
    int written = 0;
    for (int i = 0; i < 4 * MotionRecorder::pointsPerStep; ++i) written += rig.core.motion().point (0, 0, i) >= 0.0f ? 1 : 0;
    CHECK (written == 4 * MotionRecorder::pointsPerStep); // gaps between blocks are bridged

    rig.set ("p1.motion.rec", 0.0f);
    rig.set ("p1.motion.on", 1.0f);
    rig.core.params().setNormalised (cutoff, 0.0f); // the knob somewhere else: the lane plays
    rig.run (rig.blocksFor (0.4)); // past the touch hold
    float maxErr = 0.0f, sumErr = 0.0f;
    const int blocks = rig.blocksFor (2.0);
    for (int b = 0; b < blocks; ++b)
    {
        const double at = rig.ppq;
        rig.run (1);
        const float err = std::abs (d.toNormalised (rig.played ("p1.analog.cutoff")) - gesture (at));
        maxErr = std::max (maxErr, err);
        sumErr += err;
    }
    // a point holds the knob of the first block inside it (up to one block late) - the gesture (range 0.8) within
    // a slope x one point
    CHECK (maxErr < 0.1f);
    CHECK (sumErr / (float) blocks < 0.04f);
    CHECK (rig.get ("p1.analog.cutoff") == d.fromNormalised (0.0f)); // host / UI value untouched

    // the lane is a matrix source
    const float src = rig.core.part (0).modMatrix().sourceValue (mod::SrcMotion1);
    CHECK (src > 0.05f);
    CHECK (src < 0.95f);

    // depth 0: the knob's own value
    rig.set ("p1.motion.amt", 0.0f);
    rig.run (1);
    CHECK (rig.played ("p1.analog.cutoff") == doctest::Approx (rig.get ("p1.analog.cutoff")));
    rig.set ("p1.motion.amt", 1.0f);

    // transport stopped: neutral
    rig.playing = false;
    rig.run (1);
    CHECK (rig.played ("p1.analog.cutoff") == rig.get ("p1.analog.cutoff"));
    CHECK (rig.core.part (0).modMatrix().sourceValue (mod::SrcMotion1) == 0.0f);
    rig.playing = true;

    // off: neutral
    rig.set ("p1.motion.on", 0.0f);
    rig.run (1);
    CHECK (rig.played ("p1.analog.cutoff") == rig.get ("p1.analog.cutoff"));

    // clear trigger
    rig.set ("p1.motion.clear", 1.0f);
    rig.run (1);
    CHECK_FALSE (rig.core.motion().anyLane (0));
}

TEST_CASE ("p22 motion: lanes loop over their own length (polymetric to the bar)")
{
    Rig rig;
    rig.set ("p2.motion.len", 3.0f); // three sixteenths
    rig.set ("p2.motion.on", 1.0f);
    const int macro = rig.index ("p2.macro1");
    std::vector<float> pts;
    for (int i = 0; i < 3 * MotionRecorder::pointsPerStep; ++i) pts.push_back ((float) i / (3.0f * MotionRecorder::pointsPerStep));
    rig.core.motion().setLane (1, 2, macro, pts);
    rig.run (1);
    std::vector<std::pair<double, float>> seen;
    for (int b = 0; b < rig.blocksFor (3.0); ++b)
    {
        seen.push_back ({ rig.ppq, rig.played ("p2.macro1") });
        rig.run (1);
        seen.back().second = rig.played ("p2.macro1");
    }
    // the value depends on the position modulo 0.75 quarters, not on the bar
    int checked = 0;
    for (const auto& [at, v] : seen)
    {
        const double inLoop = std::fmod (at, 0.75) / 0.75;
        if (inLoop > 0.05 && inLoop < 0.9) { CHECK (v == doctest::Approx (inLoop).epsilon (0.06)); ++checked; }
    }
    CHECK (checked > 20);
    // a longer length plays the recording first, then the knob where nothing was recorded
    rig.set ("p2.motion.len", 8.0f);
    rig.set ("p2.macro1", 0.9f);
    rig.ppq = 1.6; // point 51 of 64: nothing recorded there
    rig.run (1);
    CHECK (rig.played ("p2.macro1") == doctest::Approx (0.9f));
    CHECK (rig.core.motion().uiPosition (1) == doctest::Approx (1.6 * 32.0));
}

TEST_CASE ("p22 motion: a bulk change (preset load) is not recorded; four lanes at most")
{
    Rig rig;
    rig.set ("p1.motion.rec", 1.0f);
    rig.run (2);
    const char* ids[] = { "p1.analog.cutoff", "p1.analog.reso", "p1.analog.detune", "p1.analog.drive", "p1.macro1", "p1.macro2", "p1.macro3", "p1.macro4" };
    rig.run (1, [&] (double) { for (const char* id : ids) rig.core.params().setNormalised (rig.index (id), 0.3f); });
    CHECK_FALSE (rig.core.motion().anyLane (0));
    // one after the other: four lanes, the fifth parameter finds none
    for (int k = 0; k < 5; ++k) rig.run (1, [&] (double) { rig.core.params().setNormalised (rig.index (ids[k]), 0.7f); });
    for (int l = 0; l < 4; ++l) CHECK (rig.core.motion().laneTarget (0, l) == rig.index (ids[l]));
    // discrete parameters are no lanes
    rig.core.motion().clearPart (0);
    rig.run (1, [&] (double) { rig.set ("p1.analog.ftype", 2.0f); rig.set ("p1.motion.len", 7.0f); });
    CHECK_FALSE (rig.core.motion().anyLane (0));
}

TEST_CASE ("p22 motion: state round trip; a part preset keeps the lanes and the motion settings")
{
    Rig rig;
    const int cutoff = rig.index ("p3.analog.cutoff");
    std::vector<float> pts (40, -1.0f);
    for (int i = 0; i < 20; ++i) pts[(size_t) i] = 0.05f * (float) i;
    rig.core.motion().setLane (2, 1, cutoff, pts);
    rig.set ("p3.motion.on", 1.0f);
    rig.set ("p3.motion.len", 5.0f);
    const auto state = rig.core.saveState();

    Rig other;
    REQUIRE (other.core.loadState (state));
    CHECK (other.core.motion().laneTarget (2, 1) == cutoff);
    CHECK (other.core.motion().laneTarget (2, 0) == -1);
    for (int i = 0; i < 40; ++i) CHECK (other.core.motion().point (2, 1, i) == doctest::Approx (pts[(size_t) i]));
    CHECK (other.get ("p3.motion.len") == 5.0f);

    // a part preset neither carries nor resets the motion block
    auto preset = preset::capturePart (rig.core.params(), 2, {});
    CHECK (preset.params.count ("motion.on") == 0);
    CHECK (preset.params.count ("analog.cutoff") == 1);
    preset::applyPart (preset, other.core.params(), 2);
    CHECK (other.get ("p3.motion.on") == 1.0f);
    CHECK (other.core.motion().laneTarget (2, 1) == cutoff);

    // projects carry the lanes; a state without them clears them
    auto project = preset::captureProject (rig.core, {}, {});
    CHECK (project.sections.count ("motion") == 1);
    Rig third;
    REQUIRE (third.core.loadState (third.core.saveState()));
    third.core.motion().setLane (0, 0, third.index ("p1.macro1"), { 0.5f });
    REQUIRE (third.core.loadState (Rig().core.saveState()));
    CHECK_FALSE (third.core.motion().anyLane());
    CHECK (third.core.motion().fromJson ("{\"version\":1,\"parts\":[{\"lanes\":[{\"target\":\"engine\",\"points\":[0.5]}]}]}"));
    CHECK_FALSE (third.core.motion().anyLane()); // a discrete parameter is never a lane
    CHECK_FALSE (third.core.motion().fromJson ("not json"));
}

TEST_CASE ("p22 motion: wild lanes on every part - finite, bounded, no denormals")
{
    Rig rig;
    uint32_t seed = 12345u;
    auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };
    const char* targets[] = { "analog.cutoff", "analog.reso", "analog.drive", "vol" };
    for (int p = 0; p < 4; ++p)
    {
        const auto pp = "p" + std::to_string (p + 1) + ".";
        rig.set (pp + "motion.on", 1.0f);
        rig.set (pp + "motion.len", (float) (3 + 4 * p));
        rig.set (pp + "vol", 0.6f);
        for (int l = 0; l < 4; ++l)
        {
            std::vector<float> pts (MotionRecorder::maxPoints);
            for (auto& v : pts) v = rnd() < 0.1f ? -1.0f : rnd();
            rig.core.motion().setLane (p, l, rig.index (pp + targets[l]), pts);
        }
    }
    std::vector<MidiEvent> notes;
    for (int ch = 1; ch <= 4; ++ch) notes.push_back (MidiEvent::noteOn (ch, 48 + 5 * ch, 110, 0));
    rig.run (rig.blocksFor (6.0), {}, notes);
    CHECK (allFinite (rig.rec.l));
    CHECK (allFinite (rig.rec.r));
    CHECK (noDenormals (rig.rec.l));
    CHECK (peak (rig.rec.l) < 4.0f);
    CHECK (peak (rig.rec.l) > 0.001f);
}
