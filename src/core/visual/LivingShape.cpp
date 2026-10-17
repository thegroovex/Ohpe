// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LivingShape.h"

#include <algorithm>
#include <cmath>

namespace ohpe::living
{
namespace
{
constexpr float twoPi = 6.28318530718f;

float clean (float v, float lo, float hi, float fallback) noexcept
{
    if (! std::isfinite (v)) return fallback;
    return std::clamp (v, lo, hi);
}

/** One-pole step towards `target` with time constant `tau` (seconds); values that end up tiny snap to the target. */
float glide (float value, float target, float seconds, float tau, bool smooth) noexcept
{
    if (! smooth) return target;
    const float k = 1.0f - std::exp (-seconds / std::max (tau, 1.0e-3f));
    value += (target - value) * k;
    if (std::abs (value - target) < 1.0e-4f) value = target;
    return value;
}

float wrap (float a) noexcept
{
    a = std::fmod (a, twoPi);
    return a < 0.0f ? a + twoPi : a;
}

float angleDiff (float a, float b) noexcept
{
    const float d = std::abs (wrap (a) - wrap (b));
    return std::min (d, twoPi - d);
}
} // namespace

void Shape::reset() noexcept
{
    env = lvl = ripple = width = pinch = alive = depth = 0.0f;
    lobes = {};
    turn = phase = 0.0f;
    shown = {};
    shownResting = false;
    first = true;
}

void Shape::update (const Inputs& in, float seconds, bool animate) noexcept
{
    seconds = clean (seconds, 0.0f, 0.5f, 0.0f);
    const float level = clean (in.level, 0.0f, 1.0f, 0.0f);
    const bool on = in.sounding || level > 0.003f;

    // activity: quick to wake, slow to fall asleep (the object settles after the release)
    alive = glide (alive, on ? 1.0f : 0.0f, seconds, on ? 0.05f : 0.6f, animate);
    const float gate = alive;

    const float e = clean (in.ampEnv, 0.0f, 1.0f, 0.0f) * gate;
    env = glide (env, e, seconds, e > env ? 0.02f : 0.15f, animate);
    lvl = glide (lvl, level * gate, seconds, 0.08f, animate);
    depth = glide (depth, clean (in.modDepth, 0.0f, 1.0f, 0.0f) * gate, seconds, 0.12f, animate);
    for (std::size_t i = 0; i < lobes.size(); ++i)
        lobes[i] = glide (lobes[i], clean (in.lfo[i], -1.0f, 1.0f, 0.0f) * gate, seconds, 0.06f, animate);
    const float r = (0.6f * clean (in.modEnv, 0.0f, 1.0f, 0.0f) + 0.4f * depth) * gate;
    ripple = glide (ripple, r, seconds, 0.08f, animate);
    width = glide (width, clean (in.width, 0.0f, 1.0f, 0.0f) * gate, seconds, 0.25f, animate);
    const float corr = clean (in.correlation, -1.0f, 1.0f, 1.0f);
    pinch = glide (pinch, std::clamp ((0.2f - corr) / 0.8f, 0.0f, 1.0f) * gate, seconds, 0.3f, animate);

    if (animate) // turning and the travelling ripple stop with the sound
    {
        turn = wrap (turn + seconds * (0.35f + 0.8f * depth) * gate);
        phase = wrap (phase + seconds * (1.2f + 3.0f * depth) * gate);
    }
}

float Shape::radius (float polar, float azimuth) const noexcept
{
    if (! std::isfinite (polar) || ! std::isfinite (azimuth)) return 1.0f;
    const float st = std::sin (polar), z = std::cos (polar);
    const float local = azimuth - turn;                // the surface turns with the object ...
    const float x = st * std::cos (azimuth);           // ... the stereo pinch stays left / right
    float r = 1.0f + 0.32f * env + 0.12f * lvl;
    r += 0.26f * lobes[0] * (1.5f * z * z - 0.5f);                              // LFO 1: squash / stretch upright
    r += 0.24f * lobes[1] * st * st * std::cos (2.0f * local + phase);           // LFO 2: a twisting waist
    r += 0.20f * lobes[2] * 2.0f * z * st * std::cos (local);                    // LFO 3: tilt
    r += 0.14f * ripple * std::sin (5.0f * local + 3.0f * phase) * std::sin (4.0f * polar); // mod env + matrix depth
    r *= 1.0f - 0.55f * pinch * (1.0f - x * x);                                  // out of phase: two lobes left / right
    return std::clamp (r, minRadius, maxRadius);
}

float Shape::glow() const noexcept { return std::clamp (0.25f * alive + 0.5f * env + 0.25f * lvl, 0.0f, 1.0f); }

bool Shape::isResting() const noexcept
{
    if (alive > 1.0e-3f) return false;
    const float sum = env + lvl + ripple + width + pinch + depth + std::abs (lobes[0]) + std::abs (lobes[1]) + std::abs (lobes[2]);
    return sum < 1.0e-3f;
}

bool Shape::consumeChanged() noexcept
{
    const std::array<float, 11> now { env, lvl, ripple, width, pinch, lobes[0], lobes[1], lobes[2], alive, turn, phase };
    const bool rest = isResting();
    bool moved = first || rest != shownResting;
    for (std::size_t i = 0; i < now.size() && ! moved; ++i)
    {
        const float d = i >= 9 ? angleDiff (now[i], shown[i]) : std::abs (now[i] - shown[i]);
        if (d > 2.0e-3f) moved = true;
    }
    if (moved)
    {
        shown = now;
        shownResting = rest;
        first = false;
    }
    return moved;
}
} // namespace ohpe::living
