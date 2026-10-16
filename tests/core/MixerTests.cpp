// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TestHelpers.h"
#include "mixer/Mixer.h"

#include <doctest/doctest.h>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct MixerRig
{
    ParamRegistry reg;
    ParamSnapshot snap;
    Mixer mixer;
    static constexpr int n = 512;
    std::vector<float> srcL = std::vector<float> (n, 0.5f), srcR = std::vector<float> (n, 0.5f);
    std::array<std::vector<float>, numOutputPairs> outL, outR;

    MixerRig()
    {
        mixer.registerParams (reg);
        reg.freeze();
        mixer.bind (reg);
        snap.prepare (reg);
        mixer.prepare (sampleRate, n);
        for (auto& v : outL) v.assign (n, 0.0f);
        for (auto& v : outR) v.assign (n, 0.0f);
        reg.set ("mix.master.limit", 0.0f);
    }

    void set (const std::string& id, float v) { REQUIRE (reg.set (id, v)); }

    /** Runs a few blocks (gain smoothing settles) with source part 1 = constant 0.5. */
    void run (std::array<bool, numOutputPairs> enabled = { true, true, true, true, true, true, true, true, true }, int blocks = 20)
    {
        snap.pull (reg);
        mixer.pullParams (snap);
        SourceBuffers src;
        src.left[(size_t) source::part1] = srcL.data();
        src.right[(size_t) source::part1] = srcR.data();
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o)
        {
            out.left[(size_t) o] = enabled[(size_t) o] ? outL[(size_t) o].data() : nullptr;
            out.right[(size_t) o] = enabled[(size_t) o] ? outR[(size_t) o].data() : nullptr;
        }
        for (int b = 0; b < blocks; ++b) mixer.process (src, out, n);
    }

    float last (int pair, bool left = true) const { return (left ? outL : outR)[(size_t) pair][n - 1]; }
};
} // namespace

TEST_CASE ("mixer: unity channel to main")
{
    MixerRig m;
    m.run();
    CHECK (m.last (0) == doctest::Approx (0.5f).epsilon (0.01));
    CHECK (m.last (0, false) == doctest::Approx (0.5f).epsilon (0.01));
    for (int o = 1; o < numOutputPairs; ++o) CHECK (m.last (o) == 0.0f);
}

TEST_CASE ("mixer: routing to an aux output removes the channel from main")
{
    for (int target = 1; target <= numAuxOutputs; ++target)
    {
        MixerRig m;
        m.set ("mix.ch1.out", (float) target);
        m.run();
        CAPTURE (target);
        CHECK (m.last (0) == 0.0f);
        for (int o = 1; o < numOutputPairs; ++o)
            CHECK (m.last (o) == doctest::Approx (o == target ? 0.5f : 0.0f).epsilon (0.01));
    }
}

TEST_CASE ("mixer: a disabled aux output falls back to main")
{
    MixerRig m;
    m.set ("mix.ch1.out", 3.0f);
    std::array<bool, numOutputPairs> en { true, true, true, false, true, true, true, true, true };
    m.run (en);
    CHECK (m.last (0) == doctest::Approx (0.5f).epsilon (0.01));
}

TEST_CASE ("mixer: volume in dB, pan law, mute and solo")
{
    {
        MixerRig m;
        m.set ("mix.ch1.vol", -6.0f);
        m.run();
        CHECK (m.last (0) == doctest::Approx (0.5f * 0.501187f).epsilon (0.01));
    }
    {
        MixerRig m;
        m.set ("mix.ch1.pan", -1.0f);
        m.run();
        CHECK (m.last (0, true) == doctest::Approx (0.5f * 1.41421356f).epsilon (0.01));
        CHECK (std::abs (m.last (0, false)) < 1.0e-4f);
    }
    {
        MixerRig m;
        m.set ("mix.ch1.mute", 1.0f);
        m.run();
        CHECK (std::abs (m.last (0)) < 1.0e-4f);
    }
    {
        MixerRig m;
        m.set ("mix.ch2.solo", 1.0f); // another channel soloed -> ch1 silent
        m.run();
        CHECK (std::abs (m.last (0)) < 1.0e-4f);
    }
    {
        MixerRig m;
        m.set ("mix.ch1.src", 0.0f); // source off
        m.run();
        CHECK (m.last (0) == 0.0f);
    }
}

TEST_CASE ("mixer: sends reach the effects, limiter holds the ceiling, output stays finite")
{
    MixerRig m;
    m.set ("mix.ch1.senda", 1.0f);
    m.set ("mix.ch1.sendb", 1.0f);
    m.set ("mix.ch1.vol", 6.0f);
    m.set ("mix.master.vol", 6.0f);
    m.set ("mix.master.limit", 1.0f);
    m.run();
    CHECK (allFinite (m.outL[0]));
    CHECK (noDenormals (m.outL[0]));
    CHECK (peak (m.outL[0]) <= 0.97f);
    CHECK (peak (m.outR[0]) > 0.5f);
}

TEST_CASE ("mixer: EQ boosts and cuts")
{
    MixerRig m;
    // DC-like source: low shelf boost raises it
    m.set ("mix.ch1.eqlo", 12.0f);
    m.run();
    CHECK (m.last (0) > 0.5f * 3.0f);
}

TEST_CASE ("mixer: one constant latency (limiter look-ahead), limiter on or off, main and aux outputs alike")
{
    for (int target : { 0, 2 })
        for (float on : { 0.0f, 1.0f })
        {
            CAPTURE (target);
            CAPTURE (on);
            MixerRig m;
            m.set ("mix.master.limit", on);
            m.set ("mix.ch1.out", (float) target);
            m.run(); // gains settle on the constant source
            std::fill (m.srcL.begin(), m.srcL.end(), 0.0f);
            std::fill (m.srcR.begin(), m.srcR.end(), 0.0f);
            m.run ({ true, true, true, true, true, true, true, true, true }, 2); // flush
            for (auto& v : m.outL) std::fill (v.begin(), v.end(), 0.0f);
            m.srcL[0] = m.srcR[0] = 0.25f;
            m.run ({ true, true, true, true, true, true, true, true, true }, 1);
            const int lat = m.mixer.latencySamples();
            CHECK (lat == 83); // 1.5 ms look-ahead (71) + the true-peak interpolator (12)
            const auto& o = m.outL[(size_t) target];
            int at = -1;
            float best = 0.0f;
            for (int i = 0; i < MixerRig::n; ++i)
                if (std::abs (o[(size_t) i]) > best) { best = std::abs (o[(size_t) i]); at = i; }
            CHECK (best > 0.1f);
            CHECK (at == lat);
        }
}
