// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EngineTestKit.h"
#include "engines/ClusterEngine.h"

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr auto type = EngineType::Cluster;

void single (ISynthEngine& e)
{
    e.setParam (cluster::StackVoices, 1.0f);
    e.setParam (cluster::Detune, 0.0f);
    e.setParam (cluster::Drift, 0.0f);
    e.setParam (cluster::Tone, 1.0f);
    e.setParam (cluster::Resonance, 0.0f);
    e.setParam (cluster::KeyTrack, 0.0f);
}
} // namespace

TEST_CASE ("cluster: silent when idle") { kit::checkSilentWhenIdle (type); }
TEST_CASE ("cluster: finite, no denormals, bounded at extreme settings") { kit::checkExtremes (type); }
TEST_CASE ("cluster: release ends in true silence and frees the voice") { kit::checkReleaseToSilence (type); }
TEST_CASE ("cluster: 16-voice polyphony") { kit::checkPolyphony (type); }
TEST_CASE ("cluster: parameters survive a state round trip") { kit::checkStateRoundTrip (type); }

TEST_CASE ("cluster: pitch accuracy within 2 cents")
{
    for (int note : { 33, 45, 57, 69, 81, 93 })
    {
        ClusterEngine e;
        kit::prepare (e);
        single (e);
        e.noteOn (note, 1.0f);
        auto out = renderEngine (e, 48000);
        const double ref = dsp::midiToHz ((float) note);
        const double f = kit::bandFrequency (out.l, sampleRate, 4800, 48000, ref);
        CAPTURE (note);
        CAPTURE (f);
        CHECK (std::abs (centsBetween (f, ref)) < 2.0);
    }
}

TEST_CASE ("cluster: chord shapes stack the right intervals")
{
    CHECK (cluster::chordInterval (cluster::Unison, 3) == 0);
    CHECK (cluster::chordInterval (cluster::Major, 1) == 4);
    CHECK (cluster::chordInterval (cluster::Minor, 1) == 3);
    CHECK (cluster::chordInterval (cluster::Fifth, 1) == 7);
    CHECK (cluster::chordInterval (cluster::Minor7, 3) == 10);
    for (int s = 0; s < cluster::numShapes; ++s) CHECK (cluster::chordInterval (s, 0) == 0);

    // a major stack of three sines-ish (filtered saws) has its third and fifth in the spectrum
    ClusterEngine e;
    kit::prepare (e);
    single (e);
    e.setParam (cluster::StackVoices, 3.0f);
    e.setParam (cluster::Chord, (float) cluster::Major);
    e.setParam (cluster::SideLevel, 1.0f);
    e.noteOn (48, 1.0f);
    auto out = renderEngine (e, 8192 + 16384);
    const double f0 = dsp::midiToHz (48.0f);
    const auto mag = kit::spectrum (out.l, 8192, 16384);
    auto at = [&] (double hz) { const size_t b = (size_t) std::round (hz * 16384.0 / sampleRate); return std::max ({ mag[b - 1], mag[b], mag[b + 1] }); };
    const double root = at (f0), third = at (f0 * std::pow (2.0, 4.0 / 12.0)), fifth = at (f0 * std::pow (2.0, 7.0 / 12.0));
    const double off = at (f0 * std::pow (2.0, 5.5 / 12.0));
    CHECK (third > 0.3 * root);
    CHECK (fifth > 0.3 * root);
    CHECK (off < 0.05 * root);
}

TEST_CASE ("cluster: spread widens the stereo image, one voice stays centred")
{
    auto sideRatio = [] (int voices, float spread) {
        ClusterEngine e;
        kit::prepare (e);
        e.setParam (cluster::StackVoices, (float) voices);
        e.setParam (cluster::Spread, spread);
        e.noteOn (52, 1.0f);
        auto out = renderEngine (e, 24000);
        double mid = 0.0, side = 0.0;
        for (size_t i = 4800; i < out.size(); ++i)
        {
            mid += std::pow (out.l[i] + out.r[i], 2.0);
            side += std::pow (out.l[i] - out.r[i], 2.0);
        }
        return side / mid;
    };
    CHECK (sideRatio (1, 1.0f) < 1.0e-9);
    CHECK (sideRatio (7, 0.0f) < 1.0e-9);
    CHECK (sideRatio (7, 1.0f) > 0.05);
}

TEST_CASE ("cluster: low aliasing on a bright high stack")
{
    ClusterEngine e;
    kit::prepare (e);
    single (e);
    e.noteOn (91, 1.0f);
    auto out = renderEngine (e, 8192 + 16384);
    const double db = kit::inharmonicDb (out.l, sampleRate, dsp::midiToHz (91.0f));
    CAPTURE (db);
    CHECK (db < -35.0);
}

TEST_CASE ("cluster: macro ranges, every macro changes the sound")
{
    kit::checkMacros (type, [] (ISynthEngine&) {});
}

TEST_CASE ("cluster: CPU sanity") { kit::checkCpu (type, [] (ISynthEngine&) {}); }
