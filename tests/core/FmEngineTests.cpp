// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TestHelpers.h"
#include "dsp/DspUtil.h"
#include "engines/FmEngine.h"

#include <doctest/doctest.h>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
void prepareFm (FmEngine& e)
{
    e.prepare (sampleRate, 512);
    e.setCommon (plainCommon());
    e.setPolyphony (8);
}

float brightness (const std::vector<float>& v, size_t from)
{
    double s = 0.0;
    for (size_t i = from + 1; i < v.size(); ++i) s += std::pow (v[i] - v[i - 1], 2.0);
    return (float) std::sqrt (s / (double) (v.size() - from));
}
} // namespace

TEST_CASE ("fm: silent when idle")
{
    FmEngine e;
    prepareFm (e);
    auto out = renderEngine (e, 4800);
    CHECK (peak (out.l) == 0.0f);
}

TEST_CASE ("fm: pure carrier pitch accuracy within 1 cent")
{
    for (int note : { 36, 60, 69, 96 })
    {
        FmEngine e;
        prepareFm (e);
        e.setParam (fm::Algo, 0.0f);
        e.setParam (fm::Index, 0.0f);
        e.setParam (fm::Feedback, 0.0f);
        e.noteOn (note, 1.0f);
        auto out = renderEngine (e, 48000);
        const double f = measureFrequency (out.l, sampleRate, 2400, 48000);
        CAPTURE (note);
        CHECK (std::abs (centsBetween (f, dsp::midiToHz ((float) note))) < 1.0);
    }
}

TEST_CASE ("fm: algorithm tables are well formed and morph linearly")
{
    const auto& algos = fm::algorithms();
    for (const auto& a : algos)
    {
        float outSum = 0.0f;
        for (int d = 0; d < fm::numOps; ++d)
        {
            outSum += a.out[d];
            for (int s = 0; s <= d; ++s) CHECK (a.mod[d][s] == 0.0f); // modulators have higher index
        }
        CHECK (outSum == doctest::Approx (1.0f));
    }
    const auto m = fm::morphAlgorithm (0.25f);
    CHECK (m.mod[0][1] == doctest::Approx (0.75f * algos[0].mod[0][1] + 0.25f * algos[1].mod[0][1]));
    CHECK (m.out[3] == doctest::Approx (0.25f * algos[1].out[3]));
}

TEST_CASE ("fm: stable at maximum index and feedback in every algorithm")
{
    for (float algo = 0.0f; algo <= 7.0f; algo += 0.5f)
    {
        FmEngine e;
        prepareFm (e);
        e.setParam (fm::Algo, algo);
        e.setParam (fm::Index, 1.0f);
        e.setParam (fm::Feedback, 1.0f);
        e.setParam (fm::ModRatio, 8.0f);
        for (int n = 24; n <= 108; n += 12) e.noteOn (n, 1.0f);
        auto out = renderEngine (e, 12000);
        CAPTURE (algo);
        CHECK (allFinite (out.l));
        CHECK (noDenormals (out.l));
        CHECK (peak (out.l) < 4.0f);
        CHECK (peak (out.l) > 0.01f);
    }
}

TEST_CASE ("fm: index raises brightness")
{
    auto render = [] (float index) {
        FmEngine e;
        prepareFm (e);
        e.setParam (fm::Index, index);
        e.noteOn (57, 1.0f);
        return brightness (renderEngine (e, 24000).l, 2400);
    };
    CHECK (render (0.6f) > 2.0f * render (0.0f));
}
