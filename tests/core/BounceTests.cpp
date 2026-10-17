// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"
#include "SampleTestHelpers.h"
#include "bounce/Bounce.h"
#include "engines/GrainEngine.h"
#include "engines/SamplerEngine.h"
#include "preset/Preset.h"
#include "tape/TapeMachine.h"

#include <doctest/doctest.h>

#include <cmath>
#include <map>
#include <memory>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr double twoPi = 6.283185307179586;

bounce::Audio tone (double seconds, double hz, float amp = 0.25f, double rate = sampleRate)
{
    bounce::Audio a;
    a.rate = rate;
    const auto n = (size_t) (seconds * rate);
    a.l.resize (n);
    a.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        a.l[i] = amp * (float) std::sin (twoPi * hz * (double) i / rate);
        a.r[i] = amp * (float) std::sin (twoPi * hz * 1.003 * (double) i / rate + 0.4);
    }
    return a;
}

/** The largest step between neighbouring frames of a channel, optionally across the loop seam. */
float maxStep (const float* x, int64_t from, int64_t to, bool wrap)
{
    float m = 0.0f;
    for (int64_t i = from + 1; i < to; ++i) m = std::max (m, std::abs (x[i] - x[i - 1]));
    if (wrap) m = std::max (m, std::abs (x[from] - x[to - 1]));
    return m;
}

/** A tape machine with its own registry (as in the tape tests); records stereo material on a track. */
struct TapeRig
{
    ParamRegistry reg;
    ParamSnapshot snap;
    TapeMachine tape;
    static constexpr int block = 256;
    std::array<std::vector<float>, 2 * TapeMachine::numTracks> out;

    TapeRig()
    {
        TapeMachine::registerParams (reg);
        reg.freeze();
        tape.bind (reg);
        snap.prepare (reg);
        tape.setLengthSeconds (30.0);
        tape.prepare (sampleRate, block);
        for (auto& o : out) o.assign (block, 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE (reg.set ("tape." + id, v)); }
    void run (size_t numSamples, const bounce::Audio* in)
    {
        for (size_t pos = 0; pos < numSamples; pos += block)
        {
            const int n = (int) std::min ((size_t) block, numSamples - pos);
            snap.pull (reg);
            tape.pullParams (snap);
            TapeInputs ti;
            if (in != nullptr)
            {
                ti.left[(size_t) tapesource::input] = in->l.data() + pos;
                ti.right[(size_t) tapesource::input] = in->r.data() + pos;
            }
            std::array<float*, 2 * TapeMachine::numTracks> o {};
            for (size_t c = 0; c < o.size(); ++c) o[c] = out[c].data();
            tape.process (ti, o.data(), n);
            tape.housekeeping();
        }
    }
    void record (const bounce::Audio& in, int track, int64_t at)
    {
        const auto tr = "tr" + std::to_string (track + 1);
        set (tr + ".arm", 1.0f);
        set (tr + ".src", (float) tapesource::input);
        tape.locateFrames (at);
        set ("play", 1.0f);
        set ("rec", 1.0f);
        run (in.l.size(), &in);
        set ("rec", 0.0f);
        set ("play", 0.0f);
        set (tr + ".arm", 0.0f);
        run (block, nullptr);
        tape.housekeeping();
    }
};

template <typename Engine>
struct Rig
{
    Engine e;
    Rig()
    {
        e.prepare (sampleRate, 512);
        e.setCommon (plainCommon());
        const auto d = e.getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
    }
    Stereo play (int note, size_t n)
    {
        e.allNotesOff (true);
        e.noteOn (note, 1.0f);
        return renderEngine (e, n);
    }
};
} // namespace

TEST_CASE ("p23 bounce: parameters appended after the breeding triggers, defaults, not part of project presets")
{
    auto core = std::make_unique<Core>();
    const auto& reg = core->params();
    CHECK (reg.indexOf (Core::Ids::bounceSrc) == reg.indexOf (Core::Ids::libBreedNext) + 1);
    CHECK (reg.indexOf (Core::Ids::bounceTo) == reg.indexOf (Core::Ids::bounceSrc) + 1);
    CHECK (reg.indexOf (Core::Ids::bounceLoop) == reg.indexOf (Core::Ids::bounceSrc) + 2);
    CHECK (reg.indexOf (Core::Ids::bounceLen) == reg.indexOf (Core::Ids::bounceSrc) + 3);
    CHECK (reg.indexOf (Core::Ids::bounceGo) == reg.indexOf (Core::Ids::bounceSrc) + 4);
    CHECK (reg.get (Core::Ids::bounceSrc) == (float) bounce::SourceEditTrack);
    CHECK (reg.get (Core::Ids::bounceTo) == (float) bounce::TargetGrain);
    CHECK (reg.get (Core::Ids::bounceLoop) == 1.0f);
    CHECK (reg.get (Core::Ids::bounceLen) == (float) bounce::Len4Bars);
    CHECK (reg.get (Core::Ids::bounceGo) == 0.0f);
    CHECK (reg.descriptor (reg.indexOf (Core::Ids::bounceSrc)).choices.size() == (size_t) bounce::numSources);
    CHECK (reg.descriptor (reg.indexOf (Core::Ids::bounceLen)).choices.size() == (size_t) bounce::numLengths);
    for (const char* id : { Core::Ids::bounceSrc, Core::Ids::bounceTo, Core::Ids::bounceLoop, Core::Ids::bounceLen, Core::Ids::bounceGo })
        CHECK (preset::isExcludedFromProject (id));
    CHECK (bounce::sourceName (bounce::SourceEditTrack, 2) == "Tape 3");
    CHECK (bounce::sourceName (bounce::SourceTrack1 + 7, 0) == "Tape 8");
    CHECK (bounce::sourceName (bounce::SourceMix, 0) == "Tape Mix");
    CHECK (bounce::sourceName (bounce::SourceInput, 0) == "Input");
    CHECK (bounce::lengthSeconds (bounce::Len4Bars, 120.0) == doctest::Approx (8.0));
    CHECK (bounce::lengthSeconds (bounce::Len1Bar, 60.0) == doctest::Approx (4.0));
    CHECK (bounce::lengthSeconds (bounce::Len8Bars, 20.0) == doctest::Approx (bounce::maxSeconds));
}

TEST_CASE ("p23 bounce: endless loop - the seam is as smooth as the material, guard frames repeat the other end")
{
    // 0.73 s of a 331 Hz tone: the period does not divide the length, a plain loop would jump
    auto a = tone (0.73, 331.0);
    const auto n = a.frames();
    const float stepInside = maxStep (a.l.data(), 0, n, false);
    CHECK (maxStep (a.l.data(), 0, n, true) > 4.0f * stepInside); // the raw material clicks at its seam

    auto s = bounce::prepare (a, {});
    REQUIRE ((s != nullptr));
    int64_t ls = 0, le = 0;
    REQUIRE (s->seamlessLoop (ls, le));
    CHECK (s->endless);
    CHECK (ls == 1);
    CHECK (le == s->numFrames() - 3);
    CHECK (sample::hasEndlessLayout (s->fileLoopStart, s->fileLoopEnd, s->numFrames()));
    CHECK (le - ls == n - (int64_t) (bounce::loopFadeSeconds * sampleRate));
    for (int c = 0; c < s->numChannels(); ++c)
    {
        const float* x = s->channel (c);
        CHECK (x[0] == x[le - 1]);
        for (int k = 0; k < 3; ++k) CHECK (x[le + k] == x[ls + k]);
        // after normalising the seam is no rougher than the inside of the loop (allow the gain)
        const float inside = maxStep (x, ls, le, false);
        CHECK (std::abs (x[ls] - x[le - 1]) <= inside * 1.05f);
    }
    CHECK (s->peak() == doctest::Approx (bounce::targetPeak).epsilon (0.01));
    CHECK (s->numChannels() == 2);
}

TEST_CASE ("p23 bounce: one-shot - silence trimmed, faded edges, normalised; silence and junk give nothing")
{
    auto a = tone (0.5, 220.0, 0.1f);
    bounce::Audio padded;
    padded.rate = a.rate;
    padded.l.assign (24000, 0.0f);
    padded.r.assign (24000, 0.0f);
    padded.l.insert (padded.l.end(), a.l.begin(), a.l.end());
    padded.r.insert (padded.r.end(), a.r.begin(), a.r.end());
    padded.l.resize (padded.l.size() + 24000, 0.0f);
    padded.r.resize (padded.r.size() + 24000, 0.0f);
    padded.l[100] = NAN; // junk is cleaned up
    padded.r[200] = INFINITY;
    bounce::Options o;
    o.endless = false;
    auto s = bounce::prepare (padded, o, "", "Tape 1");
    REQUIRE ((s != nullptr));
    CHECK_FALSE (s->endless);
    int64_t ls = 0, le = 0;
    CHECK_FALSE (s->seamlessLoop (ls, le));
    CHECK (s->fileLoopStart == -1);
    CHECK (s->numFrames() < a.frames() + (int64_t) (0.02 * sampleRate));
    CHECK (s->numFrames() >= a.frames());
    CHECK (s->channel (0)[0] == 0.0f);
    CHECK (s->channel (0)[s->numFrames() - 1] == 0.0f);
    CHECK (s->peak() == doctest::Approx (bounce::targetPeak).epsilon (0.01));
    CHECK (s->name() == "Tape 1");
    for (int c = 0; c < 2; ++c)
        for (int64_t i = 0; i < s->numFrames(); ++i) REQUIRE (std::isfinite (s->channel (c)[i]));

    bounce::Audio silent;
    silent.l.assign (48000, 0.0f);
    silent.r.assign (48000, 0.0f);
    CHECK ((bounce::prepare (silent, {}) == nullptr));
    CHECK ((bounce::prepare (silent, o) == nullptr));
    CHECK ((bounce::prepare (bounce::Audio(), {}) == nullptr));
    // quiet material is lifted at most +24 dB; mono material stays one channel
    auto quiet = tone (0.3, 440.0, 0.002f);
    quiet.r = quiet.l;
    auto q = bounce::prepare (quiet, o);
    REQUIRE ((q != nullptr));
    CHECK (q->numChannels() == 1);
    CHECK (q->peak() == doctest::Approx (0.002f * bounce::maxGain).epsilon (0.02));
}

TEST_CASE ("p23 bounce: reading the tape - a track, the mix of all tracks, a range")
{
    TapeRig rig;
    const auto a = tone (0.5, 200.0, 0.2f);
    const auto b = tone (0.25, 300.0, 0.1f);
    rig.record (a, 1, 4800);
    rig.record (b, 4, 0);
    auto t2 = bounce::readTape (rig.tape, 1);
    CHECK (t2.frames() == rig.tape.contentEndFrames (1));
    CHECK (t2.l[4800 + 100] == a.l[100]);
    CHECK (t2.r[4800 + 1000] == a.r[1000]);
    CHECK (t2.l[10] == 0.0f);
    auto mix = bounce::readTape (rig.tape, -1);
    CHECK (mix.frames() == rig.tape.contentEndFrames());
    CHECK (mix.l[5000] == doctest::Approx (a.l[200] + b.l[5000]));
    auto range = bounce::readTape (rig.tape, 1, 4800, 4800 + 1000);
    CHECK (range.frames() == 1000);
    CHECK (range.l[7] == a.l[7]);
    CHECK (bounce::readTape (rig.tape, 6).frames() == 0); // empty track
    CHECK (bounce::readTape (rig.tape, 9).frames() == 0);
}

TEST_CASE ("p23 bounce: apply - switches the part to Grain / Sampler and loads the bounce")
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    core.prepare (sampleRate, 512);
    std::map<std::string, float> sets;
    bounce::Setter set = [&] (const std::string& id, float v) { sets[id] = v; core.params().set (id, v); };

    auto loop = bounce::prepare (tone (1.0, 110.0), {});
    REQUIRE (bounce::apply (core, 2, bounce::TargetGrain, loop, set));
    CHECK (core.params().get ("p3.engine") == (float) EngineType::Grain);
    CHECK (core.params().get ("p3.grain.source") == (float) grain::SourceSample);
    auto* g = dynamic_cast<GrainEngine*> (core.part (2).engine (EngineType::Grain));
    REQUIRE ((g != nullptr));
    CHECK ((g->sample() == loop));

    REQUIRE (bounce::apply (core, 1, bounce::TargetSampler, loop, set));
    CHECK (core.params().get ("p2.engine") == (float) EngineType::Sampler);
    CHECK (core.params().get ("p2.sampler.loop") == (float) sampler::LoopForward);
    CHECK (core.params().get ("p2.sampler.start") == 0.0f);
    auto* s = dynamic_cast<SamplerEngine*> (core.part (1).engine (EngineType::Sampler));
    REQUIRE ((s != nullptr));
    REQUIRE (s->zones().size() == 1);
    CHECK ((s->zones()[0].sample == loop));
    CHECK (s->zones()[0].rootKey == 60);

    bounce::Options o;
    o.endless = false;
    auto shot = bounce::prepare (tone (0.5, 110.0), o);
    REQUIRE (bounce::apply (core, 1, bounce::TargetSampler, shot, set));
    CHECK (core.params().get ("p2.sampler.loop") == (float) sampler::LoopOff);
    // the other parts are untouched
    CHECK (sets.count ("p1.engine") == 0);
    CHECK (sets.count ("p4.engine") == 0);
    CHECK_FALSE (bounce::apply (core, 4, bounce::TargetGrain, loop, set));
    CHECK_FALSE ((bounce::apply (core, 0, bounce::TargetGrain, nullptr, set)));
}

TEST_CASE ("p23 bounce: the Sampler holds an endless loop as long as the key, a one-shot ends")
{
    auto loop = bounce::prepare (tone (0.4, 220.0), {});
    REQUIRE ((loop != nullptr));
    Rig<SamplerEngine> rig;
    rig.e.setParam (sampler::LoopMode, (float) sampler::LoopForward);
    rig.e.setSample (loop, 60);
    const auto held = rig.play (60, (size_t) (1.5 * sampleRate)); // almost four passes
    CHECK (allFinite (held.l));
    CHECK (noDenormals (held.l));
    CHECK (peak (held.l) < 1.5f);
    const auto last = rms (held.l, (size_t) (1.2 * sampleRate));
    CHECK (last > 0.3f * rms (held.l, 2000, (size_t) (0.3 * sampleRate)));
    // the seam does not click: no sample-to-sample step above the first pass' own (the seam is at ~0.35 s)
    const float all = maxStep (held.l.data(), 2000, (int64_t) held.l.size(), false);
    const float firstPass = maxStep (held.l.data(), 2000, (int64_t) (0.3 * sampleRate), false);
    CHECK (all < firstPass * 1.3f);

    bounce::Options o;
    o.endless = false;
    auto shot = bounce::prepare (tone (0.4, 220.0), o);
    rig.e.setParam (sampler::LoopMode, (float) sampler::LoopOff);
    rig.e.setSample (shot, 60);
    const auto once = rig.play (60, (size_t) (1.0 * sampleRate));
    CHECK (peak (once.l, (size_t) (0.5 * sampleRate)) == 0.0f);
}

TEST_CASE ("p23 bounce: Grain wraps across an endless loop's seam - bounded, finite, keeps sounding at the end")
{
    auto loop = bounce::prepare (tone (0.5, 180.0), {});
    REQUIRE ((loop != nullptr));
    Rig<GrainEngine> rig;
    rig.e.setSample (loop);
    rig.e.setParam (grain::Position, 0.98f); // grains start at the end and run across the seam
    rig.e.setParam (grain::Size, 300.0f);
    rig.e.setParam (grain::Scan, 1.5f);
    rig.e.setParam (grain::ReverseProb, 0.5f);
    rig.e.setParam (grain::Pitch, 12.0f);
    const auto out = rig.play (60, (size_t) (2.0 * sampleRate));
    CHECK (allFinite (out.l));
    CHECK (allFinite (out.r));
    CHECK (noDenormals (out.l));
    CHECK (peak (out.l) < 4.0f);
    CHECK (rms (out.l, (size_t) (1.5 * sampleRate)) > 0.01f);

    // a sample without the endless layout is not wrapped (the 0.8 behaviour stays)
    auto plain = makeSine (180.0, sampleRate, 0.5);
    int64_t a = 0, b = 0;
    CHECK_FALSE (plain->seamlessLoop (a, b));
    plain->fileLoopStart = 1;
    plain->fileLoopEnd = plain->numFrames() - 3;
    CHECK_FALSE (plain->seamlessLoop (a, b)); // the flag is set by the bounce / the loader only
}

TEST_CASE ("p23 bounce: the stereo input capture records, stops at its length, hands the audio over")
{
    bounce::StereoCapture cap;
    CHECK_FALSE (cap.recording());
    CHECK_FALSE (cap.finished());
    cap.arm (0.01, sampleRate); // 480 frames
    CHECK (cap.recording());
    std::vector<float> l (256, 0.5f), r (256, -0.25f);
    cap.push (l.data(), r.data(), 256);
    CHECK (cap.progress() == doctest::Approx (256.0 / 480.0));
    cap.push (l.data(), nullptr, 256); // mono input: copied to both sides
    CHECK_FALSE (cap.recording());
    CHECK (cap.finished());
    auto a = cap.take();
    REQUIRE (a.frames() == 480);
    CHECK (a.l[10] == 0.5f);
    CHECK (a.r[10] == -0.25f);
    CHECK (a.r[300] == 0.5f);
    CHECK (a.rate == sampleRate);
    CHECK_FALSE (cap.finished());
    cap.push (l.data(), r.data(), 256); // not armed: ignored
    CHECK (cap.take().frames() == 0);
}
