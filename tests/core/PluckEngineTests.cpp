// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EngineTestKit.h"
#include "engines/PluckEngine.h"

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr auto type = EngineType::Pluck;

void ringing (ISynthEngine& e, float material)
{
    e.setParam (pluck::Material, material);
    e.setParam (pluck::Damping, 0.1f);
    e.setParam (pluck::Body, 0.0f);
}
} // namespace

TEST_CASE ("pluck: silent when idle") { kit::checkSilentWhenIdle (type); }
TEST_CASE ("pluck: finite, no denormals, bounded at extreme settings") { kit::checkExtremes (type); }
TEST_CASE ("pluck: release ends in true silence and frees the voice") { kit::checkReleaseToSilence (type); }
TEST_CASE ("pluck: 16-voice polyphony") { kit::checkPolyphony (type); }
TEST_CASE ("pluck: parameters survive a state round trip") { kit::checkStateRoundTrip (type); }

TEST_CASE ("pluck: string pitch accuracy within 3 cents across the keyboard")
{
    for (int note : { 28, 40, 52, 64, 76, 88, 96 })
        for (float bright : { 0.2f, 0.9f })
        {
            PluckEngine e;
            kit::prepare (e);
            ringing (e, 0.0f);
            e.setParam (pluck::Brightness, bright);
            e.noteOn (note, 1.0f);
            auto out = renderEngine (e, 36000);
            const double ref = dsp::midiToHz ((float) note);
            const double f = kit::bandFrequency (out.l, sampleRate, 4800, 36000, ref);
            CAPTURE (note);
            CAPTURE (bright);
            CAPTURE (f);
            CHECK (std::abs (centsBetween (f, ref)) < 3.0);
        }
}

TEST_CASE ("pluck: mallet pitch accuracy within 2 cents")
{
    for (int note : { 48, 60, 72, 84 })
    {
        PluckEngine e;
        kit::prepare (e);
        ringing (e, 1.0f);
        e.noteOn (note, 1.0f);
        auto out = renderEngine (e, 24000);
        const double ref = dsp::midiToHz ((float) note);
        const double f = kit::bandFrequency (out.l, sampleRate, 2400, 24000, ref);
        CAPTURE (note);
        CHECK (std::abs (centsBetween (f, ref)) < 2.0);
    }
}

TEST_CASE ("pluck: the string decays by itself, damping shortens it")
{
    auto level = [] (float damping) {
        PluckEngine e;
        kit::prepare (e);
        ringing (e, 0.0f);
        e.setParam (pluck::Damping, damping);
        e.noteOn (55, 1.0f); // key held: only the string's own loss makes it decay
        auto out = renderEngine (e, 48000);
        return std::pair<float, float> { rms (out.l, 0, 4800), rms (out.l, 43200, 48000) };
    };
    const auto light = level (0.2f), heavy = level (0.8f);
    CHECK (light.first > 0.01f);
    CHECK (light.second < light.first);
    CHECK (heavy.second < 0.1f * heavy.first);
    CHECK (heavy.second < light.second);
}

TEST_CASE ("pluck: macro ranges, every macro changes the sound")
{
    kit::checkMacros (type, [] (ISynthEngine& e) { e.setParam (pluck::Material, 0.5f); });
}

TEST_CASE ("pluck: CPU sanity") { kit::checkCpu (type, [] (ISynthEngine& e) { e.setParam (pluck::Material, 0.5f); }); }
