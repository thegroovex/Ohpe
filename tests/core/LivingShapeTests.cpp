// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p26: the living display's object - the sound as a deformable 3D shape (envelopes, LFOs, matrix depth, stereo width).
#include "visual/LivingShape.h"

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <utility>

using ohpe::living::Inputs;
using ohpe::living::Shape;

namespace
{
constexpr float pi = 3.14159265358979f;
constexpr float frame = 1.0f / 30.0f;

/** Smallest / largest radius over a dense grid of directions. */
std::pair<float, float> extent (const Shape& s)
{
    float lo = 1.0e9f, hi = -1.0e9f;
    for (int i = 0; i <= 24; ++i)
        for (int j = 0; j < 48; ++j)
        {
            const float r = s.radius (pi * (float) i / 24.0f, 2.0f * pi * (float) j / 48.0f);
            REQUIRE (std::isfinite (r));
            lo = std::min (lo, r);
            hi = std::max (hi, r);
        }
    return { lo, hi };
}

Inputs playing()
{
    Inputs in;
    in.ampEnv = 0.8f;
    in.modEnv = 0.6f;
    in.lfo = { 0.7f, -0.5f, 0.9f };
    in.modDepth = 0.5f;
    in.width = 0.4f;
    in.correlation = 0.8f;
    in.level = 0.7f;
    in.sounding = true;
    return in;
}

void run (Shape& s, const Inputs& in, int frames, bool animate = true)
{
    for (int f = 0; f < frames; ++f) s.update (in, frame, animate);
}
} // namespace

TEST_CASE ("living shape: at rest it is a still unit sphere (feature off = neutral)")
{
    Shape s;
    const auto [lo, hi] = extent (s);
    CHECK (lo == doctest::Approx (1.0f));
    CHECK (hi == doctest::Approx (1.0f));
    CHECK (s.isResting());
    CHECK (s.stretch() == doctest::Approx (1.0f));
    CHECK (s.consumeChanged());       // the first picture
    CHECK_FALSE (s.consumeChanged()); // nothing moved since
    run (s, Inputs {}, 60);           // silence: still nothing to repaint
    CHECK_FALSE (s.consumeChanged());
    CHECK (s.isResting());
}

TEST_CASE ("living shape: it breathes with the envelope, deforms with LFOs and stretches with the width")
{
    Shape s;
    run (s, playing(), 20);
    CHECK_FALSE (s.isResting());
    CHECK (s.envelope() > 0.6f);
    CHECK (s.stretch() > 1.2f);
    CHECK (s.lobe (0) > 0.4f);
    CHECK (s.lobe (1) < -0.3f);
    const auto [lo, hi] = extent (s);
    CHECK (hi > 1.25f);
    CHECK (hi - lo > 0.2f); // not a sphere any more
    // it moves frame by frame while it plays (turning, travelling ripple)
    s.consumeChanged();
    int moved = 0;
    for (int f = 0; f < 10; ++f)
    {
        s.update (playing(), frame, true);
        moved += s.consumeChanged() ? 1 : 0;
    }
    CHECK (moved == 10);

    // wider stereo -> wider object; out of phase -> pinched into two lobes
    Shape mono, wide, phase;
    auto in = playing();
    in.width = 0.0f;
    run (mono, in, 60);
    in.width = 1.0f;
    run (wide, in, 60);
    in.correlation = -0.8f;
    run (phase, in, 60);
    CHECK (wide.stretch() > mono.stretch() + 0.5f);
    CHECK (mono.split() == doctest::Approx (0.0f));
    CHECK (phase.split() > 0.7f);
    // the pinch thins the middle (front) but keeps the left / right ends
    CHECK (phase.radius (pi * 0.5f, pi * 0.5f) < wide.radius (pi * 0.5f, pi * 0.5f) * 0.7f);
}

TEST_CASE ("living shape: bounded, no NaN, for wild and broken inputs")
{
    Shape s;
    Inputs in;
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    const float wild[] = { nan, inf, -inf, 1.0e9f, -1.0e9f, 5.0f, -5.0f, 0.0f, 1.0f };
    int n = 0;
    for (float a : wild)
        for (float b : wild)
        {
            in.ampEnv = a; in.modEnv = b; in.lfo = { b, a, -a }; in.modDepth = a;
            in.width = b; in.correlation = a; in.level = b; in.sounding = (n++ % 2) == 0;
            s.update (in, n % 3 == 0 ? nan : frame, true);
            const auto [lo, hi] = extent (s);
            CHECK (lo >= Shape::minRadius - 1.0e-6f);
            CHECK (hi <= Shape::maxRadius + 1.0e-6f);
            CHECK (std::isfinite (s.yaw()));
            CHECK (s.stretch() >= 1.0f);
            CHECK (s.stretch() <= Shape::maxStretch + 1.0e-6f);
            CHECK (s.glow() >= 0.0f);
            CHECK (s.glow() <= 1.0f);
        }
    CHECK (std::isfinite (s.radius (nan, 0.0f)));
    CHECK (std::isfinite (s.radius (0.3f, inf)));
}

TEST_CASE ("living shape: after the sound it settles and stops asking for repaints (CPU light)")
{
    Shape s;
    run (s, playing(), 30);
    CHECK_FALSE (s.isResting());
    // free-running LFOs keep moving while silent - the object still comes to rest
    Inputs silent;
    silent.lfo = { 1.0f, -1.0f, 0.5f };
    silent.correlation = 0.0f;
    run (s, silent, 30 * 8);
    CHECK (s.isResting());
    s.consumeChanged();
    int moved = 0;
    for (int f = 0; f < 60; ++f)
    {
        silent.lfo[0] = std::sin ((float) f);
        s.update (silent, frame, true);
        moved += s.consumeChanged() ? 1 : 0;
    }
    CHECK (moved == 0);
    const auto [lo, hi] = extent (s);
    CHECK (lo == doctest::Approx (1.0f));
    CHECK (hi == doctest::Approx (1.0f));
    // it wakes up again with the next note
    s.update (playing(), frame, true);
    CHECK (s.consumeChanged());
}

TEST_CASE ("living shape: static display setting - no turning, follows the inputs at once; reset; determinism")
{
    Shape a, b;
    run (a, playing(), 1, false);
    CHECK (a.yaw() == 0.0f);
    CHECK (a.envelope() == doctest::Approx (0.8f));
    CHECK (a.widthValue() == doctest::Approx (0.4f));
    run (a, playing(), 10, false);
    a.consumeChanged();
    a.update (playing(), frame, false);
    CHECK_FALSE (a.consumeChanged()); // same inputs, nothing turns: no repaint

    run (b, playing(), 11, false);
    CHECK (a.radius (1.0f, 2.0f) == b.radius (1.0f, 2.0f));
    a.reset();
    CHECK (a.isResting());
    CHECK (a.radius (1.0f, 2.0f) == doctest::Approx (1.0f));
}
