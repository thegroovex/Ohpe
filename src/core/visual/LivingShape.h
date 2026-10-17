// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>

namespace ohpe::living
{
/**
    p26: what the living display listens to - one sample per UI frame, taken from a part (or the master): the
    part-wide envelopes and LFOs of the modulation matrix, how much the matrix moves right now, and the stereo
    picture of the signal (mid / side balance, correlation, level). Values outside their range (or NaN) are clamped.
*/
struct Inputs
{
    float ampEnv = 0.0f;               ///< amp envelope of the newest note, 0..1
    float modEnv = 0.0f;               ///< mod envelope, 0..1
    std::array<float, 3> lfo {};       ///< LFO 1-3, -1..1
    float modDepth = 0.0f;             ///< how much the matrix moves parameters right now, 0..1
    float width = 0.0f;                ///< side share of the signal, 0 (mono) .. 1 (wide)
    float correlation = 1.0f;          ///< -1..1 (phase: below about 0.2 the object splits into two lobes)
    float level = 0.0f;                ///< signal level, 0 (silent) .. 1 (full scale)
    bool sounding = false;             ///< voices are playing
};

/**
    p26: the sound as a 3D object - a deformable sphere in "object units" (1 = the resting radius) that breathes with
    the amp envelope and the level, bulges with the three LFOs (squash / twist / tilt), ripples with the mod envelope
    and the matrix depth, stretches sideways with the stereo width and pinches into two lobes when the channels drift
    out of phase. It turns slowly while it plays and comes to rest (a still sphere) when the sound has faded, so a
    resting object needs no repaint.

    Plain C++ (no UI): the display samples Inputs ~30 times a second, calls update() and draws the mesh from radius()
    and the transform values. consumeChanged() tells whether the picture moved visibly since the last call.
*/
class Shape
{
public:
    static constexpr int numRings = 9;      ///< latitude rings of the mesh (poles excluded)
    static constexpr int numSegments = 28;  ///< points per ring / meridians
    static constexpr float minRadius = 0.35f, maxRadius = 1.9f;
    static constexpr float maxStretch = 1.8f; ///< sideways stretch at full width

    void reset() noexcept;
    /** One UI frame. `animate` = false: no turning / travelling ripple and no smoothing (static display setting). */
    void update (const Inputs& in, float seconds, bool animate) noexcept;

    /** Radius of the surface in a world direction (polar angle 0..pi from the top, azimuth 0 = right, pi / 2 = away
        from the viewer), object units, always within [minRadius, maxRadius]. 1 everywhere when resting. The surface
        pattern turns with yaw(); the stereo stretch / pinch stay left-right. */
    float radius (float polar, float azimuth) const noexcept;
    /** Sideways (left-right) stretch of the whole object, 1..maxStretch. */
    float stretch() const noexcept { return 1.0f + (maxStretch - 1.0f) * width; }
    /** Turn of the object about its vertical axis (radians). */
    float yaw() const noexcept { return turn; }
    /** 0..1: how lit the object is (envelope + level, gated by activity) - the display's glow. */
    float glow() const noexcept;
    /** 0..1: how far the object has split into two lobes (out-of-phase stereo). */
    float split() const noexcept { return pinch; }
    /** 0..1: how alive it is right now (fades to 0 after the sound stops). */
    float activity() const noexcept { return alive; }

    /** The sound has faded and every value has settled: a still sphere. */
    bool isResting() const noexcept;
    /** True when the picture moved visibly since the last call (the display repaints the object only then). */
    bool consumeChanged() noexcept;

    // smoothed state (tests / the readouts)
    float envelope() const noexcept { return env; }
    float widthValue() const noexcept { return width; }
    float lobe (int i) const noexcept { return lobes[(std::size_t) (i < 0 ? 0 : i > 2 ? 2 : i)]; }
    float rippleValue() const noexcept { return ripple; }

private:
    float env = 0.0f, lvl = 0.0f, ripple = 0.0f, width = 0.0f, pinch = 0.0f, alive = 0.0f, depth = 0.0f;
    std::array<float, 3> lobes {};
    float turn = 0.0f, phase = 0.0f;
    std::array<float, 11> shown {}; ///< signature of the last picture reported by consumeChanged()
    bool shownResting = false, first = true;
};
} // namespace ohpe::living
