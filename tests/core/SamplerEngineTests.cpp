// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleTestHelpers.h"
#include "engines/SamplerEngine.h"
#include "sample/Slicer.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct SamplerRig
{
    SamplerEngine e;
    SamplerRig()
    {
        e.prepare (sampleRate, 512);
        e.setCommon (plainCommon());
        const auto d = e.getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
    }
    Stereo play (int note, size_t n, float vel = 1.0f)
    {
        e.allNotesOff (true);
        e.noteOn (note, vel);
        return renderEngine (e, n);
    }
};

const std::vector<int64_t> hitFrames { 12000, 28800, 48000, 69600 }; // 0.25 s, 0.6 s, 1.0 s, 1.45 s at 48 kHz
} // namespace

TEST_CASE ("sampler: silence when idle, built-in tone without samples")
{
    SamplerRig rig;
    CHECK (peak (renderEngine (rig.e, 4800).l) == 0.0f);
    auto out = rig.play (60, 9600);
    CHECK (rms (out.l) > 0.05f);
    CHECK (rig.e.zones().empty()); // the built-in tone is not a user zone
}

TEST_CASE ("sampler: pitch accuracy from the root key (zone root and root parameter)")
{
    SamplerRig rig;
    rig.e.setSample (makeSine (440.0, 44100.0, 2.0), 69);
    for (int note : { 69, 81, 64, 45 })
    {
        CAPTURE (note);
        auto out = rig.play (note, 24000);
        CHECK (std::abs (centsBetween (measureFrequency (out.l, sampleRate, 2400, 24000), dsp::midiToHz ((float) note))) < 2.0);
    }
    rig.e.setSample (makeSine (440.0, 48000.0, 2.0)); // zone without root: the "root" parameter applies
    rig.e.setParam (sampler::Root, 57.0f);             // 440 Hz sample "is" A3 -> note 69 sounds an octave up
    auto out = rig.play (69, 24000);
    CHECK (std::abs (centsBetween (measureFrequency (out.l, sampleRate, 2400, 24000), 880.0)) < 2.0);
}

TEST_CASE ("sampler: multisample zones by key and velocity")
{
    SamplerRig rig;
    sampler::ZoneSpec low, high, soft, loud;
    low.sample = makeSine (220.0, 48000.0, 1.0);
    low.keyLo = 0; low.keyHi = 59; low.rootKey = 57;
    high.sample = makeSine (880.0, 48000.0, 1.0);
    high.keyLo = 60; high.keyHi = 71; high.rootKey = 81;
    soft.sample = makeSine (440.0, 48000.0, 1.0, 0.1f);
    soft.keyLo = 72; soft.keyHi = 127; soft.velLo = 1; soft.velHi = 64; soft.rootKey = 69;
    loud.sample = makeSine (440.0, 48000.0, 1.0, 0.8f);
    loud.keyLo = 72; loud.keyHi = 127; loud.velLo = 65; loud.velHi = 127; loud.rootKey = 69;
    rig.e.setZones ({ low, high, soft, loud });
    CHECK (rig.e.zones().size() == 4);

    auto a = rig.play (57, 24000);
    CHECK (std::abs (centsBetween (measureFrequency (a.l, sampleRate, 2400, 24000), 220.0)) < 2.0);
    auto b = rig.play (69, 24000); // high zone, root 81 -> an octave down from 880
    CHECK (std::abs (centsBetween (measureFrequency (b.l, sampleRate, 2400, 24000), 440.0)) < 2.0);
    const float softPeak = peak (rig.play (81, 9600, 0.3f).l, 2400);
    const float loudPeak = peak (rig.play (81, 9600, 1.0f).l, 2400);
    CHECK (softPeak == doctest::Approx (0.1f * 0.7071f).epsilon (0.05));
    CHECK (loudPeak == doctest::Approx (0.8f * 0.7071f).epsilon (0.05));
}

TEST_CASE ("sampler: loop modes sustain, one-shot ends and frees the voice")
{
    SamplerRig rig;
    rig.e.setSample (makeSine (480.0, 48000.0, 0.1), 69); // 100 ms, whole periods
    rig.e.setParam (sampler::LoopMode, (float) sampler::LoopOff);
    auto oneShot = rig.play (69, 24000);
    CHECK (rms (oneShot.l, 0, 4000) > 0.1f);
    CHECK (peak (oneShot.l, 6000) == 0.0f);
    CHECK (rig.e.activeVoiceCount() == 0);

    for (int mode : { (int) sampler::LoopForward, (int) sampler::LoopPingPong })
    {
        CAPTURE (mode);
        rig.e.setParam (sampler::LoopMode, (float) mode);
        rig.e.setParam (sampler::LoopStart, 0.25f);
        rig.e.setParam (sampler::LoopEnd, 0.75f);
        auto held = rig.play (69, 48000);
        CHECK (rms (held.l, 38400, 48000) > 0.1f);
        CHECK (allFinite (held.l));
        CHECK (rig.e.activeVoiceCount() == 1);
        rig.e.noteOff (69);
        renderEngine (rig.e, 48000);
        CHECK (rig.e.activeVoiceCount() == 0);
    }
}

TEST_CASE ("sampler: reverse plays the sample backwards, start offset")
{
    SamplerRig rig;
    std::vector<float> ramp (4800);
    for (size_t i = 0; i < ramp.size(); ++i) ramp[i] = (float) i / 4800.0f; // rises 0 -> 1
    rig.e.setSample (SampleData::fromMono (ramp, 48000.0), 60);
    rig.e.setParam (sampler::Reverse, 1.0f);
    auto rev = rig.play (60, 4800);
    CHECK (rev.l[200] > rev.l[3000]);
    CHECK (rev.l[200] > 0.6f);

    rig.e.setParam (sampler::Reverse, 0.0f);
    rig.e.setParam (sampler::Start, 0.5f);
    auto fwd = rig.play (60, 4800);
    CHECK (fwd.l[100] > 0.3f); // starts in the middle of the ramp
}

TEST_CASE ("slicer: transients of a synthetic signal are found within 2 ms")
{
    auto s = makeHits (48000.0, 2.0, hitFrames, { 0.8f, 0.5f, 0.9f, 0.3f });
    sample::SliceSettings settings;
    settings.sensitivity = 0.5f;
    const auto slices = sample::detectTransients (*s, settings);
    REQUIRE (slices.size() == hitFrames.size() + 1);
    CHECK (slices[0] == 0);
    for (size_t i = 0; i < hitFrames.size(); ++i)
    {
        CAPTURE (i);
        CHECK (slices[i + 1] <= hitFrames[i] + 2);
        CHECK (slices[i + 1] >= hitFrames[i] - 96); // at most 2 ms early
    }
}

TEST_CASE ("slicer: sensitivity decides about quiet hits; silence gives one slice")
{
    auto s = makeHits (48000.0, 2.0, { 12000, 48000 }, { 0.9f, 0.02f }); // second hit -33 dB
    sample::SliceSettings low, high;
    low.sensitivity = 0.0f;
    high.sensitivity = 1.0f;
    CHECK (sample::detectTransients (*s, low).size() == 2);
    CHECK (sample::detectTransients (*s, high).size() == 3);

    auto silent = SampleData::fromMono (std::vector<float> (48000, 0.0f), 48000.0);
    CHECK (sample::detectTransients (*silent, high) == std::vector<int64_t> { 0 });
}

TEST_CASE ("sampler: slice to key plays each slice on its own key")
{
    SamplerRig rig;
    rig.e.setSample (makeHits (48000.0, 2.0, hitFrames, { 0.8f, 0.5f, 0.9f, 0.05f })); // last hit quiet
    rig.e.setParam (sampler::Mode, (float) sampler::ModeSlices);
    rig.e.setParam (sampler::SliceBaseKey, 36.0f);
    const auto slices = rig.e.slicePoints();
    REQUIRE (slices.size() == 5);

    // key 38 = slice 2: starts with the hit at 0.6 s, ends where slice 3 begins
    auto out = rig.play (38, 48000);
    const auto len = (size_t) (slices[3] - slices[2]);
    CHECK (peak (out.l, 0, 480) > 0.2f);     // the attack is right at the start
    CHECK (peak (out.l, len + 64) == 0.0f); // nothing after the slice end
    CHECK (rig.e.activeVoiceCount() == 0);

    CHECK (peak (rig.play (35, 9600).l) == 0.0f); // below the first slice key
    CHECK (peak (rig.play (41, 9600).l) == 0.0f); // beyond the last slice

    // the sensitivity parameter re-slices on the message thread
    rig.e.setParam (sampler::SliceSensitivity, 0.0f);
    CHECK (rig.e.updateSlicing());
    CHECK_FALSE (rig.e.updateSlicing());
    CHECK (rig.e.slicePoints().size() < slices.size());
}

TEST_CASE ("sampler: finite, no denormals at extremes; replacing samples while playing is safe")
{
    SamplerRig rig;
    rig.e.setSample (makeSine (440.0, 22050.0, 0.5), 69);
    rig.e.setParam (sampler::LoopMode, (float) sampler::LoopPingPong);
    rig.e.setParam (sampler::LoopStart, 0.999f);
    rig.e.setParam (sampler::LoopEnd, 1.0f);
    rig.e.setParam (sampler::Tone, 0.0f);
    rig.e.setParam (sampler::Start, 1.0f);
    for (int n : { 0, 30, 69, 110, 127 }) rig.e.noteOn (n, 1.0f);
    auto out = renderEngine (rig.e, 24000, 33);
    CHECK (allFinite (out.l));
    CHECK (noDenormals (out.l));
    CHECK (peak (out.l) < 4.0f);

    rig.e.setParam (sampler::Start, 0.0f);
    rig.e.noteOn (69, 1.0f);
    renderEngine (rig.e, 512);
    rig.e.setSample (makeSine (330.0, 48000.0, 1.0)); // voices of the old map stop, no stale reads
    auto after = renderEngine (rig.e, 4800);
    CHECK (allFinite (after.l));
    CHECK (rig.e.activeVoiceCount() == 0);
    rig.e.setSample (nullptr);
    CHECK (rig.e.zones().empty());
}

TEST_CASE ("sampler: a held forward loop whose points do not meet never clicks (0.8 punch pass)")
{
    // 437 Hz over a loop of 0.25..0.75 s: the loop holds a non-whole number of periods, so the end and the start differ in
    // phase (and the decay makes them differ in level). Before 0.8 every pass jumped - a click per loop of a held note.
    std::vector<float> x (48000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.6f * std::exp (-0.8f * (float) i / 48000.0f) * (float) std::sin (2.0 * 3.14159265358979323846 * 437.0 * (double) i / 48000.0);
    for (float rev : { 0.0f, 1.0f })
    {
        CAPTURE (rev);
        SamplerRig rig;
        rig.e.setSample (SampleData::fromMono (std::move (std::vector<float> (x)), 48000.0), 69);
        rig.e.setParam (sampler::LoopMode, (float) sampler::LoopForward);
        rig.e.setParam (sampler::LoopStart, 0.25f);
        rig.e.setParam (sampler::LoopEnd, 0.75f);
        rig.e.setParam (sampler::Reverse, rev);
        auto held = rig.play (69, 48000 * 4); // eight loop passes
        float pk = 0.0f, d2 = 0.0f;
        for (size_t i = 4800; i < held.l.size(); ++i)
        {
            pk = std::max (pk, std::abs (held.l[i]));
            d2 = std::max (d2, std::abs (held.l[i] - 2.0f * held.l[i - 1] + held.l[i - 2]));
        }
        REQUIRE (pk > 0.05f);
        CHECK (rig.e.activeVoiceCount() == 1);
        // a 437 Hz sine: second difference ~0.75 % of the peak; a loop jump is of the order of the peak itself
        CHECK (d2 < 0.03f * pk);
    }
}
