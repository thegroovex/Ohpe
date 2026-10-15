// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/Envelope.h"
#include "dsp/Lfo.h"

#include <doctest/doctest.h>

using namespace ohpe::dsp;

TEST_CASE ("ADSR attack reaches the peak after the attack time")
{
    Adsr env;
    env.prepare (48000.0f);
    env.setParams ({ 0.1f, 0.2f, 0.5f, 0.3f });
    env.noteOn();
    int peakAt = -1;
    for (int i = 0; i < 48000; ++i)
        if (env.next() >= 1.0f) { peakAt = i + 1; break; }
    CHECK (peakAt == doctest::Approx (4800).epsilon (0.005));
}

TEST_CASE ("ADSR decays to sustain and releases to silence in time")
{
    Adsr env;
    env.prepare (48000.0f);
    env.setParams ({ 0.001f, 0.1f, 0.5f, 0.2f });
    env.noteOn();
    for (int i = 0; i < 48000; ++i) env.next();
    CHECK (env.getLevel() == doctest::Approx (0.5f));
    CHECK (env.getStage() == Adsr::Stage::Sustain);
    env.noteOff();
    int idleAt = -1;
    for (int i = 0; i < 48000 * 2; ++i)
    {
        env.next();
        if (! env.isActive()) { idleAt = i; break; }
    }
    // -60 dB of the distance in 0.2 s; idle threshold 1e-4 relative to 0.5 -> a bit longer than 0.2 s
    CHECK (idleAt > 0.2 * 48000 * 0.9);
    CHECK (idleAt < 0.2 * 48000 * 1.6);
}

TEST_CASE ("LFO stays bipolar for every shape")
{
    Lfo lfo;
    lfo.prepare (48000.0f);
    lfo.setRate (5.0f);
    for (int shape = 0; shape < Lfo::numShapes; ++shape)
    {
        lfo.setShape (shape);
        float mn = 2.0f, mx = -2.0f;
        for (int i = 0; i < 2000; ++i)
        {
            const float v = lfo.advance (32);
            mn = std::min (mn, v);
            mx = std::max (mx, v);
        }
        CHECK (mn >= -1.0001f);
        CHECK (mx <= 1.0001f);
        CHECK (mx - mn > 0.5f);
    }
}
