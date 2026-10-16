// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleTestHelpers.h"
#include "engines/GrainEngine.h"

#include <doctest/doctest.h>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct GrainRig
{
    GrainEngine e;
    GrainRig()
    {
        e.prepare (sampleRate, 512);
        e.setCommon (plainCommon());
        const auto d = e.getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
        // deterministic grains for measurements
        e.setParam (grain::Spray, 0.0f);
        e.setParam (grain::Spread, 0.0f);
        e.setParam (grain::Size, 100.0f);
        e.setParam (grain::Density, 20.0f);
        e.setParam (grain::Position, 0.4f);
        e.setParam (grain::Scan, 0.0f);
    }
};

/** A tape-like region for the interface test. */
struct FakeTapeRegion final : IAudioRegion
{
    std::vector<float> data;
    explicit FakeTapeRegion (double freq)
    {
        data.resize (96000);
        for (size_t i = 0; i < data.size(); ++i) data[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979323846 * freq * (double) i / 48000.0);
    }
    int numChannels() const noexcept override { return 1; }
    int64_t numFrames() const noexcept override { return (int64_t) data.size(); }
    double sampleRate() const noexcept override { return 48000.0; }
    const float* channel (int) const noexcept override { return data.data(); }
};
} // namespace

TEST_CASE ("grain: silence when idle")
{
    GrainRig rig;
    auto out = renderEngine (rig.e, 4800);
    CHECK (peak (out.l) == 0.0f);
    CHECK (peak (out.r) == 0.0f);
}

TEST_CASE ("grain: pitch accuracy on a loaded sample (note and grain pitch)")
{
    GrainRig rig;
    rig.e.setSample (makeSine (440.0, 44100.0, 2.0)); // different source rate on purpose
    struct Case { int note; float grainPitch; double expected; };
    for (const auto c : { Case { 60, 0.0f, 440.0 }, Case { 72, 0.0f, 880.0 }, Case { 60, -12.0f, 220.0 }, Case { 67, 0.0f, 440.0 * std::pow (2.0, 7.0 / 12.0) } })
    {
        CAPTURE (c.note);
        CAPTURE (c.grainPitch);
        rig.e.allNotesOff (true);
        rig.e.setParam (grain::Pitch, c.grainPitch);
        rig.e.noteOn (c.note, 1.0f);
        auto out = renderEngine (rig.e, 48000);
        const double f = measureFrequency (out.l, sampleRate, 9600, 48000);
        CHECK (std::abs (centsBetween (f, c.expected)) < 10.0);
    }
}

TEST_CASE ("grain: reverse grains keep the pitch, output stays finite")
{
    GrainRig rig;
    rig.e.setSample (makeSine (440.0, 48000.0, 2.0));
    rig.e.setParam (grain::ReverseProb, 1.0f);
    rig.e.noteOn (60, 1.0f);
    auto out = renderEngine (rig.e, 48000);
    CHECK (allFinite (out.l));
    CHECK (rms (out.l, 9600) > 0.01f);
    CHECK (std::abs (centsBetween (measureFrequency (out.l, sampleRate, 9600, 48000), 440.0)) < 10.0);
}

TEST_CASE ("grain: density sets how often grains start")
{
    GrainRig rig;
    rig.e.setSample (makeSine (440.0, 48000.0, 2.0));
    auto silentFraction = [&] (float density) {
        rig.e.allNotesOff (true);
        rig.e.setParam (grain::Density, density);
        rig.e.setParam (grain::Size, 20.0f);
        rig.e.noteOn (60, 1.0f);
        auto out = renderEngine (rig.e, 48000);
        size_t quiet = 0;
        for (size_t i = 4800; i < out.size(); ++i) quiet += std::abs (out.l[i]) < 1.0e-4f ? 1 : 0;
        return (double) quiet / (double) (out.size() - 4800);
    };
    const double sparse = silentFraction (2.0f);
    const double dense = silentFraction (100.0f);
    CHECK (sparse > 0.8);  // 2 grains of 20 ms per second: mostly gaps
    CHECK (dense < 0.05);  // 100 grains per second overlap
}

TEST_CASE ("grain: granulates a tape region through the interface, falls back to the sample")
{
    GrainRig rig;
    FakeTapeRegion tape (320.0); // 16 periods per grain interval: overlapping grains stay in phase
    rig.e.setParam (grain::Source, (float) grain::SourceTape);
    rig.e.setTapeRegion (&tape);
    rig.e.noteOn (60, 1.0f);
    auto out = renderEngine (rig.e, 48000);
    CHECK (std::abs (centsBetween (measureFrequency (out.l, sampleRate, 9600, 48000), 320.0)) < 10.0);

    rig.e.setTapeRegion (nullptr); // no region: the sample (built-in tone at C4) plays
    auto after = renderEngine (rig.e, 48000);
    CHECK (rms (after.l, 9600) > 0.005f);
    CHECK (allFinite (after.l));
}

TEST_CASE ("grain: finite, no denormals at extreme settings, release to silence")
{
    GrainRig rig;
    const float settings[][6] = { // size, density, spray, pitch, scan, tone
                                  { 5.0f, 100.0f, 1.0f, 24.0f, 2.0f, 0.0f },
                                  { 500.0f, 100.0f, 1.0f, -24.0f, -2.0f, 1.0f },
                                  { 5.0f, 1.0f, 0.0f, 24.0f, 0.0f, 0.5f } };
    for (const auto& s : settings)
    {
        rig.e.setParam (grain::Size, s[0]);
        rig.e.setParam (grain::Density, s[1]);
        rig.e.setParam (grain::Spray, s[2]);
        rig.e.setParam (grain::Pitch, s[3]);
        rig.e.setParam (grain::Scan, s[4]);
        rig.e.setParam (grain::Tone, s[5]);
        rig.e.setParam (grain::ReverseProb, 0.5f);
        rig.e.setParam (grain::Position, 1.0f);
        for (int n : { 0, 36, 60, 96, 127 }) rig.e.noteOn (n, 1.0f);
        auto out = renderEngine (rig.e, 24000, 61);
        CHECK (allFinite (out.l));
        CHECK (allFinite (out.r));
        CHECK (noDenormals (out.l));
        CHECK (peak (out.l) < 4.0f);
        rig.e.allNotesOff (false);
        renderEngine (rig.e, 48000);
        CHECK (rig.e.activeVoiceCount() == 0);
        auto tail = renderEngine (rig.e, 1024);
        CHECK (peak (tail.l) == 0.0f);
    }
}

TEST_CASE ("grain: replacing the sample while playing is safe")
{
    GrainRig rig;
    rig.e.noteOn (60, 1.0f);
    renderEngine (rig.e, 4800);
    rig.e.setSample (makeSine (220.0, 48000.0, 1.0));
    auto out = renderEngine (rig.e, 24000);
    CHECK (allFinite (out.l));
    CHECK (std::abs (centsBetween (measureFrequency (out.l, sampleRate, 9600, 24000), 220.0)) < 10.0);
    rig.e.setSample (nullptr);
    CHECK ((rig.e.sample() == nullptr));
}

TEST_CASE ("grain: a held note scanning through a one-shot never clicks at the sample end (0.8 punch pass)")
{
    // A struck one-shot: loud attack, ring decaying to a quiet tail. The scan (and the spray around it) carries grains
    // to the end of the sample; before 0.8 a grain read across the end into the attack - a click every scan cycle.
    std::vector<float> x (48000 * 3);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.8f * std::exp (-2.0f * (float) i / 48000.0f) * (float) std::sin (2.0 * 3.14159265358979323846 * 220.0 * (double) i / 48000.0);
    for (float rev : { 0.0f, 0.5f })
    {
        CAPTURE (rev);
        GrainRig rig;
        rig.e.setSample (SampleData::fromMono (std::move (std::vector<float> (x)), 48000.0, {}));
        rig.e.setParam (grain::Position, 0.1f);
        rig.e.setParam (grain::Size, 90.0f);
        rig.e.setParam (grain::Density, 40.0f);
        rig.e.setParam (grain::Spray, 0.35f);
        rig.e.setParam (grain::Scan, 1.2f);
        rig.e.setParam (grain::ReverseProb, rev);
        rig.e.noteOn (grain::rootNote, 1.0f); // root pitch: the sample's own 220 Hz
        auto out = renderEngine (rig.e, 48000 * 8);
        float peak = 0.0f, d2 = 0.0f;
        for (size_t i = 2400; i < out.l.size(); ++i)
        {
            peak = std::max (peak, std::abs (out.l[i]));
            d2 = std::max (d2, std::abs (out.l[i] - 2.0f * out.l[i - 1] + out.l[i - 2]));
        }
        REQUIRE (peak > 0.01f);
        // smooth grains of a 220 Hz tone: the second difference stays a small fraction of the peak; a jump inside a
        // grain window (tail -> attack) is of the order of the attack itself
        CHECK (d2 < 0.02f * peak);
    }
}
