// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TestHelpers.h"
#include "dsp/DspUtil.h"
#include "engines/AnalogEngine.h"

#include <doctest/doctest.h>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
void prepareAnalog (AnalogEngine& e)
{
    e.prepare (sampleRate, 512);
    e.setCommon (plainCommon());
    e.setPolyphony (8);
}

/** Highpassed (first difference) energy as a brightness measure. */
float brightness (const std::vector<float>& v, size_t from)
{
    double s = 0.0;
    for (size_t i = from + 1; i < v.size(); ++i) s += std::pow (v[i] - v[i - 1], 2.0);
    return (float) std::sqrt (s / (double) (v.size() - from));
}
} // namespace

TEST_CASE ("analog: silent when idle")
{
    AnalogEngine e;
    prepareAnalog (e);
    auto out = renderEngine (e, 4800);
    CHECK (peak (out.l) == 0.0f);
    CHECK (peak (out.r) == 0.0f);
    CHECK (e.activeVoiceCount() == 0);
}

TEST_CASE ("analog: pitch accuracy within 3 cents")
{
    for (int note : { 45, 57, 69, 81 })
    {
        AnalogEngine e;
        prepareAnalog (e);
        e.setParam (analog::Shape, 0.5f); // square: one upward crossing per period
        e.setParam (analog::Detune, 0.0f);
        e.setParam (analog::OscMix, 0.0f);
        e.setParam (analog::Cutoff, 18000.0f);
        e.setParam (analog::Resonance, 0.0f);
        e.setParam (analog::KeyTrack, 0.0f);
        e.setParam (analog::Drive, 0.0f);
        e.noteOn (note, 1.0f);
        auto out = renderEngine (e, 48000);
        const double f = measureFrequency (out.l, sampleRate, 4800, 48000);
        const double ref = dsp::midiToHz ((float) note);
        CAPTURE (note);
        CAPTURE (f);
        CHECK (std::abs (centsBetween (f, ref)) < 3.0);
    }
}

TEST_CASE ("analog: finite, no denormals, bounded at extreme settings; every filter type")
{
    for (int ft = 0; ft < 4; ++ft)
    {
        AnalogEngine e;
        prepareAnalog (e);
        e.setParam (analog::FilterType, (float) ft);
        e.setParam (analog::Resonance, 1.0f);
        e.setParam (analog::Drive, 1.0f);
        e.setParam (analog::Noise, 1.0f);
        e.setParam (analog::Sub, 1.0f);
        e.setParam (analog::Cutoff, 20.0f);
        for (int n = 24; n < 120; n += 12) e.noteOn (n, 1.0f);
        auto a = renderEngine (e, 24000);
        e.setParam (analog::Cutoff, 20000.0f);
        auto b = renderEngine (e, 24000);
        CAPTURE (ft);
        CHECK (allFinite (a.l));
        CHECK (allFinite (b.l));
        CHECK (noDenormals (a.l));
        CHECK (noDenormals (b.r));
        CHECK (peak (b.l) < 8.0f);
    }
}

TEST_CASE ("analog: release ends in true silence and frees the voice")
{
    AnalogEngine e;
    prepareAnalog (e);
    e.noteOn (60, 0.8f);
    renderEngine (e, 4800);
    CHECK (e.activeVoiceCount() == 1);
    e.noteOff (60);
    auto tail = renderEngine (e, 48000);
    CHECK (e.activeVoiceCount() == 0);
    CHECK (peak (tail.l, 24000) == 0.0f);
    CHECK (noDenormals (tail.l));
}

TEST_CASE ("analog: cutoff controls brightness")
{
    auto render = [] (float cutoff) {
        AnalogEngine e;
        prepareAnalog (e);
        e.setParam (analog::Cutoff, cutoff);
        e.setParam (analog::KeyTrack, 0.0f);
        e.noteOn (48, 1.0f);
        auto out = renderEngine (e, 24000);
        return brightness (out.l, 4800);
    };
    CHECK (render (8000.0f) > 2.0f * render (300.0f));
}
