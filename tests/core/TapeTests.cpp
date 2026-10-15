// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "tape/TapeMachine.h"

#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
/** A tape machine with its own parameter registry, driven like the core drives it. */
struct TapeRig
{
    ParamRegistry reg;
    ParamSnapshot snap;
    TapeMachine tape;
    static constexpr int block = 256;
    std::array<std::vector<float>, 2 * TapeMachine::numTracks> out;

    explicit TapeRig (double lengthSeconds = 30.0)
    {
        TapeMachine::registerParams (reg);
        reg.freeze();
        tape.bind (reg);
        snap.prepare (reg);
        tape.setLengthSeconds (lengthSeconds);
        tape.prepare (sampleRate, block);
        for (auto& o : out) o.assign (block, 0.0f);
    }

    void set (const std::string& id, float v) { REQUIRE (reg.set ("tape." + id, v)); }

    /** Runs numSamples; `in` (may be null) feeds the given source; returns track `track` (L, R). */
    Stereo run (size_t numSamples, const Stereo* in = nullptr, int track = 0, int source = tapesource::input)
    {
        Stereo result (numSamples);
        for (size_t pos = 0; pos < numSamples; pos += block)
        {
            const int n = (int) std::min ((size_t) block, numSamples - pos);
            snap.pull (reg);
            tape.pullParams (snap);
            TapeInputs ti;
            if (in != nullptr)
            {
                ti.left[(size_t) source] = in->l.data() + pos;
                ti.right[(size_t) source] = in->r.data() + pos;
            }
            std::array<float*, 2 * TapeMachine::numTracks> o {};
            for (size_t c = 0; c < o.size(); ++c) o[c] = out[c].data();
            tape.process (ti, o.data(), n);
            std::copy (out[(size_t) (2 * track)].begin(), out[(size_t) (2 * track)].begin() + n, result.l.begin() + (long) pos);
            std::copy (out[(size_t) (2 * track + 1)].begin(), out[(size_t) (2 * track + 1)].begin() + n, result.r.begin() + (long) pos);
            tape.housekeeping(); // the plug-in does this at 20 Hz on the message thread
        }
        return result;
    }

    /** Records `in` on `track` from `at` (speed 1), then stops recording and commits. */
    void record (const Stereo& in, int track, int64_t at, bool overdub = false)
    {
        const auto tr = "tr" + std::to_string (track + 1);
        set ("overdub", overdub ? 1.0f : 0.0f);
        set (tr + ".arm", 1.0f);
        set (tr + ".src", (float) tapesource::input);
        tape.locateFrames (at);
        set ("play", 1.0f);
        set ("rec", 1.0f);
        run (in.size(), &in, track);
        set ("rec", 0.0f);
        set ("play", 0.0f);
        set (tr + ".arm", 0.0f);
        run (block); // the recording flag drops, housekeeping commits
        tape.housekeeping();
    }

    Stereo playFrom (int64_t at, size_t n, int track = 0)
    {
        tape.locateFrames (at);
        set ("play", 1.0f);
        auto r = run (n, nullptr, track);
        set ("play", 0.0f);
        run (block);
        return r;
    }

    std::vector<float> committed (int track, int64_t start, int n, bool left = true)
    {
        std::vector<float> l ((size_t) n), r ((size_t) n);
        tape.readCommitted (track, start, n, l.data(), r.data());
        return left ? l : r;
    }
};

Stereo noise (size_t n, uint32_t seed)
{
    dsp::Random rng (seed);
    Stereo s (n);
    for (size_t i = 0; i < n; ++i) { s.l[i] = rng.nextBipolar() * 0.5f; s.r[i] = rng.nextBipolar() * 0.5f; }
    return s;
}

Stereo sine (size_t n, double hz, float amp = 0.5f)
{
    Stereo s (n);
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = amp * (float) std::sin (2.0 * 3.14159265358979323846 * hz * (double) i / sampleRate);
    return s;
}
} // namespace

TEST_CASE ("tape: parameters are registered with the tape. prefix")
{
    TapeRig rig;
    CHECK (rig.reg.indexOf ("tape.play") >= 0);
    CHECK (rig.reg.indexOf ("tape.tr8.out") >= 0);
    CHECK ((int) TapeMachine::describeParams().size() == TapeMachine::numParams);
}

TEST_CASE ("tape: record and play back sample-exact, across block boundaries")
{
    TapeRig rig;
    const auto in = noise (10000, 7);
    rig.record (in, 2, 4000); // crosses the block boundaries at 4096 and 8192
    CHECK (rig.tape.undoSteps() == 1);
    CHECK (rig.tape.dropouts() == 0);
    const auto out = rig.playFrom (4000, 10000, 2);
    for (size_t i = 0; i < in.size(); ++i)
    {
        REQUIRE (out.l[i] == in.l[i]);
        REQUIRE (out.r[i] == in.r[i]);
    }
    // the other tracks stay silent; the committed content agrees
    CHECK (peak (rig.playFrom (4000, 2000, 0).l) == 0.0f);
    CHECK (rig.committed (2, 4000, 10)[3] == in.l[3]);
    CHECK (rig.tape.contentEndFrames (2) == 14000);
}

TEST_CASE ("tape: recording from a part source, input monitoring only for the input source")
{
    TapeRig rig;
    const auto in = noise (3000, 9);
    rig.set ("tr1.arm", 1.0f);
    rig.set ("tr1.src", (float) tapesource::part1);
    // armed but not recording: a part is not monitored through the tape (it is heard on its own channel)
    CHECK (peak (rig.run (512, &in, 0, tapesource::part1).l) == 0.0f);
    rig.set ("tr1.src", (float) tapesource::input);
    CHECK (peak (rig.run (512, &in, 0, tapesource::input).l) > 0.1f);
    rig.set ("tr1.src", (float) tapesource::part1);
    rig.tape.locateFrames (0);
    rig.set ("play", 1.0f);
    rig.set ("rec", 1.0f);
    rig.run (in.size(), &in, 0, tapesource::part1);
    rig.set ("rec", 0.0f);
    rig.set ("play", 0.0f);
    rig.set ("tr1.arm", 0.0f);
    rig.run (256);
    const auto out = rig.playFrom (0, 3000);
    CHECK (out.l[1234] == in.l[1234]);
}

TEST_CASE ("tape: overdub adds, replace overwrites, undo / redo per recording pass")
{
    TapeRig rig;
    const auto a = noise (5000, 1);
    const auto b = noise (5000, 2);
    rig.record (a, 0, 0);
    rig.record (b, 0, 0, true); // overdub
    auto out = rig.playFrom (0, 5000);
    CHECK (out.l[100] == doctest::Approx (a.l[100] + b.l[100]));
    CHECK (rig.tape.undoSteps() == 2);
    REQUIRE (rig.tape.undo());
    out = rig.playFrom (0, 5000);
    CHECK (out.l[100] == a.l[100]);
    REQUIRE (rig.tape.redo());
    out = rig.playFrom (0, 5000);
    CHECK (out.l[100] == doctest::Approx (a.l[100] + b.l[100]));
    rig.record (b, 0, 1000, false); // replace from 1000
    out = rig.playFrom (0, 7000);
    CHECK (out.l[500] == doctest::Approx (a.l[500] + b.l[500]));
    CHECK (out.l[1000] == b.l[0]);
    CHECK (out.l[3999] == b.l[2999]);
    // undo twice: back to "a" only; a new edit drops the redo branch
    REQUIRE (rig.tape.undo());
    REQUIRE (rig.tape.undo());
    CHECK (rig.playFrom (0, 200).l[100] == a.l[100]);
    REQUIRE (rig.tape.clearTrack (0));
    CHECK_FALSE (rig.tape.redo());
    CHECK (peak (rig.playFrom (0, 5000).l) == 0.0f);
    REQUIRE (rig.tape.undo());
    CHECK (rig.playFrom (0, 200).l[100] == a.l[100]);
}

TEST_CASE ("tape: varispeed changes pitch by the speed ratio (interpolated)")
{
    TapeRig rig;
    rig.record (sine (48000, 1000.0), 0, 0);
    for (float speed : { 1.5f, 0.75f, 2.0f, 0.5f })
    {
        INFO ("speed " << speed);
        rig.set ("speed", speed);
        const auto out = rig.playFrom (0, 12000);
        const double f = measureFrequency (out.l, sampleRate, 6000, 12000); // after the speed smoothing
        CHECK (std::abs (centsBetween (f, 1000.0 * speed)) < 2.0);
        CHECK (allFinite (out.l));
    }
    rig.set ("speed", 1.0f);
    const double f1 = measureFrequency (rig.playFrom (0, 12000).l, sampleRate, 2000, 12000);
    CHECK (std::abs (centsBetween (f1, 1000.0)) < 0.5);
}

TEST_CASE ("tape: reverse plays the recording backwards, exactly at speed 1")
{
    TapeRig rig;
    Stereo ramp (3000);
    for (size_t i = 0; i < ramp.size(); ++i) { ramp.l[i] = (float) i * 1.0e-4f; ramp.r[i] = -(float) i * 1.0e-4f; }
    rig.record (ramp, 0, 0);
    rig.set ("reverse", 1.0f);
    const auto out = rig.playFrom (2999, 3200);
    for (size_t k = 0; k < 3000; ++k)
    {
        REQUIRE (out.l[k] == ramp.l[2999 - k]);
        REQUIRE (out.r[k] == ramp.r[2999 - k]);
    }
    CHECK (peak (out.l, 3001) == 0.0f); // parked at the start of the tape: silence
    CHECK (rig.tape.positionFrames() == 0);
    CHECK (rig.tape.transport().speed == 0.0f);
}

TEST_CASE ("tape: loop in / out wraps the playhead")
{
    TapeRig rig;
    rig.record (noise (4800, 3), 0, 0);
    rig.set ("loopin", 0.0f);
    rig.set ("loopout", 0.01f); // 480 frames
    rig.set ("loop", 1.0f);
    const auto out = rig.playFrom (0, 2000);
    for (size_t i = 0; i < 480; ++i)
    {
        REQUIRE (out.l[i + 480] == out.l[i]);
        REQUIRE (out.l[i + 960] == out.l[i]);
    }
    auto st = rig.tape.transport();
    CHECK (st.loopOn);
    CHECK (st.loopEnd == doctest::Approx (0.01));
}

TEST_CASE ("tape: lift, cut, drop, splice against a reference, each undoable")
{
    TapeRig rig;
    const int64_t n = 20000;
    const auto in = noise ((size_t) n, 11);
    rig.record (in, 1, 0);
    std::vector<float> ref (in.l.begin(), in.l.end());
    auto check = [&] (const std::vector<float>& expect, const char* what) {
        INFO (what);
        const auto got = rig.committed (1, 0, (int) expect.size() + 5000);
        for (size_t i = 0; i < expect.size(); ++i)
            if (got[i] != expect[i]) { FAIL_CHECK ("mismatch at " << i); return; }
        for (size_t i = expect.size(); i < got.size(); ++i)
            if (got[i] != 0.0f) { FAIL_CHECK ("not silent at " << i); return; }
    };

    // cut 3000..7000: later audio moves earlier, the clipboard holds the cut
    REQUIRE (rig.tape.cutFrames (1, 3000, 7000));
    CHECK (rig.tape.clipboardFrames() == 4000);
    std::vector<float> cutRef (ref.begin(), ref.begin() + 3000);
    cutRef.insert (cutRef.end(), ref.begin() + 7000, ref.end());
    check (cutRef, "cut");

    // splice the clipboard back at 3000: the original returns
    REQUIRE (rig.tape.spliceFrames (1, 3000));
    check (ref, "splice");

    // lift 100..5100 leaves silence, drop puts it at 12000 (overwrites)
    REQUIRE (rig.tape.liftFrames (1, 100, 5100));
    auto liftRef = ref;
    std::fill (liftRef.begin() + 100, liftRef.begin() + 5100, 0.0f);
    check (liftRef, "lift");
    REQUIRE (rig.tape.dropFrames (1, 12000));
    auto dropRef = liftRef;
    for (int i = 0; i < 5000; ++i) dropRef[(size_t) (12000 + i)] = ref[(size_t) (100 + i)];
    check (dropRef, "drop");

    // the playback sees the edit too
    CHECK (rig.playFrom (12000, 10, 1).l[3] == ref[103]);

    // undo every step
    REQUIRE (rig.tape.undo()); check (liftRef, "undo drop");
    REQUIRE (rig.tape.undo()); check (ref, "undo lift");
    REQUIRE (rig.tape.undo()); check (cutRef, "undo splice");
    REQUIRE (rig.tape.undo()); check (ref, "undo cut");
    REQUIRE (rig.tape.undo()); check ({}, "undo recording");
    CHECK_FALSE (rig.tape.undo());
    for (int i = 0; i < 5; ++i) REQUIRE (rig.tape.redo());
    check (dropRef, "redo all");
    CHECK_FALSE (rig.tape.redo());

    // seconds-based interface
    REQUIRE (rig.tape.lift (1, 0.0, 0.001));
    CHECK (rig.committed (1, 0, 48)[47] == 0.0f);
}

TEST_CASE ("tape: edits are refused while recording")
{
    TapeRig rig;
    const auto in = noise (2048, 5);
    rig.set ("tr1.arm", 1.0f);
    rig.set ("tr1.src", (float) tapesource::input);
    rig.set ("play", 1.0f);
    rig.set ("rec", 1.0f);
    rig.run (1024, &in);
    CHECK (rig.tape.transport().recording);
    CHECK_FALSE (rig.tape.cutFrames (0, 0, 100));
    CHECK_FALSE (rig.tape.undo());
    rig.set ("rec", 0.0f);
    rig.run (256);
    CHECK (rig.tape.cutFrames (0, 0, 100));
}

TEST_CASE ("tape: character - saturation, hiss only when asked, wow / flutter move the pitch")
{
    TapeRig rig;
    rig.record (sine (24000, 440.0, 0.9f), 0, 0);
    const auto clean = rig.playFrom (0, 8000);
    rig.set ("sat", 1.0f);
    const auto sat = rig.playFrom (0, 8000);
    CHECK (allFinite (sat.l));
    CHECK (peak (sat.l) < peak (clean.l));
    rig.set ("sat", 0.0f);
    CHECK (rig.playFrom (0, 8000).l == clean.l); // sat 0 is an exact bypass

    rig.set ("hiss", 1.0f);
    const auto hissy = rig.playFrom (25000, 8000); // past the recording: only hiss
    CHECK (peak (hissy.l) > 1.0e-4f);
    CHECK (peak (hissy.l) < 0.05f);
    rig.set ("hiss", 0.0f);
    CHECK (peak (rig.playFrom (25000, 4000).l) == 0.0f);

    rig.set ("wow", 1.0f);
    rig.set ("flutter", 1.0f);
    const auto wobbly = rig.playFrom (0, 8000);
    CHECK (allFinite (wobbly.l));
    float diff = 0.0f;
    for (size_t i = 0; i < 8000; ++i) diff = std::max (diff, std::abs (wobbly.l[i] - clean.l[i]));
    CHECK (diff > 1.0e-3f);
}

TEST_CASE ("tape: audio persistence round-trip, junk rejected")
{
    TapeRig a;
    const auto in = noise (9000, 21);
    a.record (in, 5, 100);
    std::vector<uint8_t> bin;
    a.tape.saveAudio (bin);
    CHECK (bin.size() < 3 * 2 * 4096 * 4 + 200); // only blocks holding audio

    TapeRig b;
    REQUIRE (b.tape.loadAudio (bin.data(), bin.size()));
    CHECK (b.tape.undoSteps() == 0);
    const auto out = b.playFrom (100, 9000, 5);
    for (size_t i = 0; i < in.size(); ++i) REQUIRE (out.r[i] == in.r[i]);

    CHECK_FALSE (b.tape.loadAudio ("junk", 4));
    bin.resize (bin.size() - 7);
    CHECK_FALSE (b.tape.loadAudio (bin.data(), bin.size()));
    CHECK (b.playFrom (100, 10, 5).l[1] == in.l[1]); // a failed load leaves the tape alone

    TapeRig c;
    std::vector<uint8_t> empty;
    c.tape.saveAudio (empty);
    REQUIRE (b.tape.loadAudio (empty.data(), empty.size()));
    CHECK (b.tape.contentEndFrames() == 0);
}

TEST_CASE ("tape: shared content plays in a second machine, settings JSON")
{
    TapeRig a;
    const auto in = noise (3000, 4);
    a.record (in, 0, 0);
    TapeRig b;
    b.tape.shareContentFrom (a.tape);
    CHECK (b.playFrom (0, 3000).l[2000] == in.l[2000]);
    // recording in b does not touch a (shared blocks are frozen)
    b.record (noise (3000, 99), 0, 0);
    CHECK (a.playFrom (0, 3000).l[2000] == in.l[2000]);

    a.tape.setLengthSeconds (125.0);
    TapeRig c;
    c.tape.loadSettingsJson (a.tape.settingsJson());
    CHECK (c.tape.lengthSeconds() == doctest::Approx (125.0));
    CHECK (c.tape.lengthFrames() == 125 * 48000);
    c.tape.loadSettingsJson ("{broken");
    CHECK (c.tape.lengthSeconds() == doctest::Approx (125.0));
}

TEST_CASE ("tape: the end of the tape stops the playhead, the length is configurable")
{
    TapeRig rig (10.0);
    CHECK (rig.tape.lengthFrames() == 480000);
    rig.tape.locateFrames (480000 - 100);
    rig.set ("play", 1.0f);
    rig.run (1000);
    CHECK (rig.tape.positionFrames() == 480000);
    rig.set ("play", 0.0f);
    rig.tape.setLengthSeconds (1.0); // clamped to the minimum
    CHECK (rig.tape.lengthSeconds() == doctest::Approx (TapeMachine::minLengthSeconds));
}

// ---------------------------------------------------------------- through the core
namespace
{
struct CoreRig
{
    Core core;
    static constexpr int n = 512;
    std::array<std::vector<float>, numOutputPairs> l, r;
    CoreRig()
    {
        core.prepare (sampleRate, n);
        for (auto& v : l) v.assign (n, 0.0f);
        for (auto& v : r) v.assign (n, 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }
    /** Renders `blocks` blocks; returns the peak of output pair `pair`. */
    float run (int blocks, int pair, std::array<bool, numOutputPairs> enabled, const float* inL = nullptr, const float* inR = nullptr)
    {
        float p = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            OutputBuffers out;
            for (int o = 0; o < numOutputPairs; ++o)
                if (enabled[(size_t) o]) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
            core.setAudioInput (inL, inR);
            core.process (out, n, nullptr, 0, Transport {});
            core.tape().housekeeping();
            if (enabled[(size_t) pair]) p = std::max (p, std::max (peak (l[(size_t) pair]), peak (r[(size_t) pair])));
        }
        return p;
    }
};
constexpr std::array<bool, numOutputPairs> allOn { true, true, true, true, true, true, true, true, true };
constexpr std::array<bool, numOutputPairs> mainOnly { true, false, false, false, false, false, false, false, false };
} // namespace

TEST_CASE ("core: input is recorded to tape, tape direct out reaches its aux output or falls back to main")
{
    CoreRig rig;
    // channels 6-8 play tape tracks 1-3 by default: take channel 6 off tape 1 so only the direct out carries it
    rig.set ("mix.ch6.src", 0.0f);
    const auto in = sine (CoreRig::n, 375.0); // whole periods per block
    rig.set ("tape.tr1.arm", 1.0f);
    rig.set ("tape.tr1.src", (float) tapesource::input);
    rig.set ("tape.play", 1.0f);
    rig.set ("tape.rec", 1.0f);
    rig.run (20, 0, allOn, in.l.data(), in.r.data());
    rig.set ("tape.rec", 0.0f);
    rig.set ("tape.play", 0.0f);
    rig.set ("tape.tr1.arm", 0.0f);
    rig.run (1, 0, allOn);
    CHECK_FALSE (rig.core.tape().housekeeping()); // already committed inside run()
    CHECK (rig.core.tape().undoSteps() == 1);
    CHECK (rig.core.tape().contentEndFrames (0) >= 19 * CoreRig::n);

    rig.set ("tape.tr1.out", 3.0f);
    rig.core.tape().locate (0.0);
    rig.set ("tape.play", 1.0f);
    CHECK (rig.run (10, 3, allOn) > 0.4f);
    rig.core.tape().locate (0.0);
    CHECK (rig.run (10, 0, allOn) == 0.0f); // not on main while Out 3 exists
    rig.core.tape().locate (0.0);
    CHECK (rig.run (10, 0, mainOnly) > 0.4f); // Out 3 disabled -> main

    // a mixer channel can play the track too (channel 7 -> tape 1 -> Out 5)
    rig.set ("tape.tr1.out", 0.0f);
    rig.set ("mix.ch7.src", (float) source::tape1);
    rig.set ("mix.ch7.out", 5.0f);
    rig.core.tape().locate (0.0);
    CHECK (rig.run (10, 5, allOn) > 0.4f);
}

TEST_CASE ("core: state keeps the tape settings section")
{
    Core a;
    a.tape().setLengthSeconds (300.0);
    a.params().set ("tape.speed", 1.25f);
    const auto json = a.saveState();
    CHECK (nlohmann::json::parse (json)["sections"].contains ("tape"));
    Core b;
    REQUIRE (b.loadState (json));
    CHECK (b.tape().lengthSeconds() == doctest::Approx (300.0));
    CHECK (b.params().get ("tape.speed") == doctest::Approx (1.25f));
}
