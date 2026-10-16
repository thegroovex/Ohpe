// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Space3D.h"

#include "../dsp/DspUtil.h"

#include <algorithm>
#include <cmath>

namespace ohpe
{
namespace space
{
namespace
{
constexpr float pi = 3.14159265358979f;
constexpr float degToRad = pi / 180.0f;

/** Deterministic hash of (seed, knot, salt) -> -1 .. +1. */
float hashUnit (uint32_t seed, int64_t knot, uint32_t salt) noexcept
{
    uint32_t h = seed * 0x9E3779B1u ^ ((uint32_t) (knot & 0xffffffff) * 0x85EBCA77u) ^ (salt * 0xC2B2AE3Du);
    h ^= h >> 15; h *= 0x2C1B3C6Du;
    h ^= h >> 12; h *= 0x297A2D39u;
    h ^= h >> 15;
    return (float) ((double) h / 4294967295.0) * 2.0f - 1.0f;
}

float catmullRom (float y0, float y1, float y2, float y3, float t) noexcept
{
    const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c = -0.5f * y0 + 0.5f * y2;
    return ((a * t + b) * t + c) * t + y1;
}

Vec3 direction (float azDeg, float elDeg) noexcept
{
    const float a = azDeg * degToRad, e = elDeg * degToRad;
    return { std::sin (a) * std::cos (e), std::cos (a) * std::cos (e), std::sin (e) };
}

Vec3 scaled (Vec3 v, float k) noexcept { return { v.x * k, v.y * k, v.z * k }; }
} // namespace

double pathLengthBeats (int choice) noexcept
{
    static constexpr double beats[numPathLengths] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 1.5 };
    return beats[std::clamp (choice, 0, numPathLengths - 1)];
}

Vec3 pathPoint (int path, double phase, float size, uint32_t seed) noexcept
{
    size = std::clamp (size, 0.0f, 1.0f);
    const double fr = phase - std::floor (phase);
    const float th = (float) (fr * 2.0 * 3.14159265358979323846);
    switch (path)
    {
        case PathCircle: // a full orbit; size tilts the orbit's plane (0 = level, 1 = 60 degrees)
        {
            const float t = size * 60.0f * degToRad;
            return { std::sin (th) * std::cos (t), std::cos (th), std::sin (th) * std::sin (t) };
        }
        case PathFigure8: // a lying eight on the sphere: wide left / right sweep, the crossing in front
        {
            const float a = 30.0f + 150.0f * size;
            const float e = 25.0f * (0.4f + 0.6f * size);
            return direction (a * std::sin (th), e * std::sin (2.0f * th));
        }
        case PathFlyBy: // passes in front from left to right and back (a straight line, slowest at the far ends)
        {
            const float l = 2.0f + 10.0f * size;
            return { -std::cos (th) * l, 1.0f, 0.0f };
        }
        case PathRandom: // a smooth random orbit (four knots per cycle, Catmull-Rom)
        {
            const double t = phase * 4.0;
            const auto k = (int64_t) std::floor (t);
            const float f = (float) (t - (double) k);
            float az[4], el[4], d[4];
            for (int i = 0; i < 4; ++i)
            {
                az[i] = 180.0f * (0.35f + 0.65f * size) * hashUnit (seed, k + i - 1, 1);
                el[i] = 40.0f * size * hashUnit (seed, k + i - 1, 2);
                d[i] = 1.0f + 0.4f * size * hashUnit (seed, k + i - 1, 3);
            }
            const auto v = direction (catmullRom (az[0], az[1], az[2], az[3], f), catmullRom (el[0], el[1], el[2], el[3], f));
            return scaled (v, std::max (0.3f, catmullRom (d[0], d[1], d[2], d[3], f)));
        }
        case PathPendulum: // swings front <-> back over the head (size = how far it swings)
        {
            const float psi = (90.0f - 90.0f * size * std::sin (th)) * degToRad;
            return { 0.0f, std::cos (psi), std::sin (psi) };
        }
        case PathSpiral: // climbs from below ear height to above the head in 1-4 turns, a bit closer at the top,
        {                // then sinks back in the last tenth of the cycle (continuous: no jump at the cycle start)
            const float u = (float) fr;
            const float turns = std::round (1.0f + 3.0f * size);
            const float t = u / 0.9f; // eased at both ends, so the speed never jumps either (no Doppler step)
            const float h = u < 0.9f ? t * t * (3.0f - 2.0f * t) : 0.5f + 0.5f * std::cos ((u - 0.9f) / 0.1f * pi);
            return scaled (direction (360.0f * turns * u, -20.0f + 105.0f * h), 1.0f - 0.3f * h);
        }
        default: return { 0.0f, 1.0f, 0.0f };
    }
}

Vec3 orient (Vec3 v, float azimuthDeg, float elevationDeg) noexcept
{
    const float e = elevationDeg * degToRad, a = azimuthDeg * degToRad;
    const float ce = std::cos (e), se = std::sin (e), ca = std::cos (a), sa = std::sin (a);
    const float y1 = v.y * ce - v.z * se, z1 = v.y * se + v.z * ce; // pitch (front tilts up)
    return { v.x * ca + y1 * sa, -v.x * sa + y1 * ca, z1 };          // yaw (+ = to the right)
}

float lengthOf (const Vec3& v) noexcept { return std::sqrt (v.x * v.x + v.y * v.y + v.z * v.z); }
float azimuthOf (const Vec3& v) noexcept { return std::atan2 (v.x, v.y) / degToRad; }
float elevationOf (const Vec3& v) noexcept
{
    const float l = lengthOf (v);
    return l > 1.0e-9f ? std::asin (std::clamp (v.z / l, -1.0f, 1.0f)) / degToRad : 0.0f;
}

float itdSeconds (const Vec3& u) noexcept
{
    const float s = std::clamp (u.x, -1.0f, 1.0f);
    const float b = std::asin (s);
    return headRadius / speedOfSound * (b + s); // Woodworth: (a / c) (beta + sin beta)
}
} // namespace space

using space::Vec3; using space::itdSeconds; using space::lengthOf; using space::pathPoint; using space::orient;
using space::minDistance; using space::maxDistance; using space::refDistance; using space::speedOfSound; using space::headRadius;
using space::PathOff; using space::numPaths; using space::numPathLengths; using space::pathLengthBeats;

namespace
{
constexpr float pi = 3.14159265358979f;
constexpr float sqrt2 = 1.41421356f;
constexpr float itdMax = headRadius / speedOfSound * (pi * 0.5f + 1.0f); // ~0.655 ms
constexpr float shadowBeta = 2.0f * speedOfSound / headRadius;          // 2 w0 of the spherical head
constexpr float alphaMin = 0.1f, thetaMin = 150.0f / 180.0f * pi;

/** Head-shadow zero factor for the angle between source and ear axis (spherical head, first order). */
float shadowAlpha (float theta) noexcept
{
    return (1.0f + alphaMin * 0.5f) + (1.0f - alphaMin * 0.5f) * std::cos (theta / thetaMin * pi);
}
const float alphaFront = shadowAlpha (0.5f * pi); // both ears at 90 degrees: the free-field (front) reference

float fastTan (float x) noexcept { return std::tan (x); }

enum Band { BPresence = 0, BBack, BShadow, BAbove, BNotch1, BNotch2 };
constexpr float bandHz[4] = { 4200.0f, 1100.0f, 5200.0f, 8500.0f };
} // namespace

// ---------------------------------------------------------------------------------------------- building blocks
void SpaceEffect::Svf::bell (float g, float q, float db) noexcept
{
    const float A = std::pow (10.0f, db / 40.0f);
    const float k = 1.0f / (q * A);
    a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    m0 = 1.0f; m1 = k * (A * A - 1.0f); m2 = 0.0f;
}
void SpaceEffect::Svf::highShelf (float g0, float q, float db) noexcept
{
    const float A = std::pow (10.0f, db / 40.0f);
    const float g = g0 * std::sqrt (A), k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A;
}
void SpaceEffect::Svf::lowPass (float g, float q) noexcept
{
    const float k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    m0 = 0.0f; m1 = 0.0f; m2 = 1.0f;
}
void SpaceEffect::Svf::highPass (float g, float q) noexcept
{
    const float k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    m0 = 1.0f; m1 = -k; m2 = -1.0f;
}
inline float SpaceEffect::Svf::process (float x) noexcept
{
    const float v3 = x - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = dsp::flush (2.0f * v1 - ic1);
    ic2 = dsp::flush (2.0f * v2 - ic2);
    return m0 * x + m1 * v1 + m2 * v2;
}

void SpaceEffect::Line::prepare (int minLength)
{
    int n = 1;
    while (n < minLength + 8) n <<= 1;
    buf.assign ((size_t) n, 0.0f);
    mask = n - 1;
    pos = 0;
}
void SpaceEffect::Line::reset() noexcept
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    pos = 0;
}
inline float SpaceEffect::Line::cubic (float delay) const noexcept
{
    const float d = std::clamp (delay, 1.0f, (float) (mask - 4));
    const int i = (int) d;
    const float f = d - (float) i;
    const float ym1 = buf[(size_t) ((pos - i + 1) & mask)], y0 = buf[(size_t) ((pos - i) & mask)];
    const float y1 = buf[(size_t) ((pos - i - 1) & mask)], y2 = buf[(size_t) ((pos - i - 2) & mask)];
    // 4-point Hermite between y0 (delay i) and y1 (delay i + 1)
    const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}
inline float SpaceEffect::Line::linear (float delay) const noexcept
{
    const float d = std::clamp (delay, 0.0f, (float) (mask - 2));
    const int i = (int) d;
    const float f = d - (float) i;
    const float y0 = buf[(size_t) ((pos - i) & mask)], y1 = buf[(size_t) ((pos - i - 1) & mask)];
    return y0 + (y1 - y0) * f;
}

// ---------------------------------------------------------------------------------------------- the effect
SpaceEffect::SpaceEffect()
{
    for (auto& p : uiPos) p.store (0.0f);
    uiPos[1].store (refDistance);
}

std::vector<ParamDescriptor> SpaceEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("azimuth", "Azimuth", "Azimut", -180.0f, 180.0f, 0.0f, ParamUnit::Degrees),
        continuous ("elevation", "Elevation", "Elevation", -60.0f, 90.0f, 0.0f, ParamUnit::Degrees),
        continuous ("distance", "Distance", "Entfernung", minDistance, maxDistance, refDistance, ParamUnit::Metres, 0.3f),
        continuous ("pathrate", "Motion Rate", "Bewegungstempo", -4.0f, 4.0f, 1.0f),
        continuous ("pathphase", "Motion Phase", "Bewegungsphase", 0.0f, 360.0f, 0.0f, ParamUnit::Degrees),
        choice ("path", "Motion Path", "Bewegungspfad",
                { "Off", "Circle", "Figure-8", "Fly-By", "Random Orbit", "Pendulum", "Spiral Up" },
                { "Aus", "Kreis", "Acht", "Vorbeiflug", "Zufallsorbit", "Pendel", "Spirale aufwärts" }, PathOff),
        choice ("pathlen", "Motion Length", "Bewegungslänge",
                { "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars", "32 Bars", "1/4." },
                { "1/16", "1/8", "1/4", "1/2", "1 Takt", "2 Takte", "4 Takte", "8 Takte", "16 Takte", "32 Takte", "1/4." }, 5),
        continuous ("pathsize", "Motion Size", "Bewegungsgröße", 0.0f, 1.0f, 0.5f, ParamUnit::Percent),
        continuous ("spread", "Source Spread", "Quellenbreite", 0.0f, 180.0f, 40.0f, ParamUnit::Degrees),
        continuous ("room", "Room", "Raumanteil", 0.0f, 1.0f, 0.25f, ParamUnit::Percent),
        continuous ("roomsize", "Room Size", "Raumgröße", 0.0f, 1.0f, 0.4f, ParamUnit::Percent),
        continuous ("doppler", "Doppler", "Doppler", 0.0f, 1.0f, 0.5f, ParamUnit::Percent),
        continuous ("air", "Air Absorption", "Luftdämpfung", 0.0f, 1.0f, 0.6f, ParamUnit::Percent),
        continuous ("rolloff", "Distance Rolloff", "Entfernungsabfall", 0.0f, 1.0f, 0.7f, ParamUnit::Percent),
        continuous ("lowkeep", "Mono Below", "Mono unter", 40.0f, 300.0f, 120.0f, ParamUnit::Hertz, 0.5f),
        continuous ("glide", "Motion Glide", "Bewegungsglättung", 0.005f, 2.0f, 0.06f, ParamUnit::Seconds, 0.3f),
    };
}

void SpaceEffect::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? (float) sampleRate : 48000.0f;
    const int directMax = (int) std::ceil ((maxDistance / speedOfSound + 2.0f * itdMax + 0.002f) * sr) + 8;
    for (auto& s : src) s.line.prepare (directMax);
    lowLine.prepare (directMax);
    roomLine.prepare ((int) std::ceil (0.42f * sr) + directMax);
    for (int i = 0; i < 4; ++i) fdn[(size_t) i].assign ((size_t) (0.05f * sr) + 8, 0.0f);
    for (int b = 0; b < 4; ++b) bandFreqG[(size_t) b] = fastTan (pi * std::min (bandHz[b], 0.45f * sr) / sr);
    sideLpCoeff = 1.0f - std::exp (-2.0f * pi * 700.0f / sr);
    lowKeepSet = roomSizeSet = -1.0f;
    reset();
}

void SpaceEffect::reset() noexcept
{
    for (auto& s : src)
    {
        s.line.reset();
        for (auto& b : s.bands) b.reset();
        s.air = 0.0f;
        s.lp = 0.0f;
        for (auto& e : s.ear) e.x1 = e.y1 = 0.0f;
    }
    src[1].numEars = 4;
    lowLine.reset();
    roomLine.reset();
    for (auto& f : xoLp) f.reset();
    for (auto& c : xoHp)
        for (auto& f : c) f.reset();
    for (auto& f : fdn) std::fill (f.begin(), f.end(), 0.0f);
    fdnPos.fill (0);
    fdnLp.fill (0.0f);
    roomLp = 0.0f;
    levelAcc = 0.0f;
    levelCount = 0;
    countdown = 0;
    fresh = true;
    phase = 0.0;
    wasPlaying = false;
}

void SpaceEffect::setTransportPosition (double p, bool isPlaying) noexcept
{
    ppq = p;
    playing = isPlaying;
    hasPpq = true;
    ppqSamples = 0;
}

void SpaceEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Azimuth: azimuth = std::clamp (v, -180.0f, 180.0f); break;
        case Elevation: elevation = std::clamp (v, -60.0f, 90.0f); break;
        case Distance: distance = std::clamp (v, minDistance, maxDistance); break;
        case PathRate: pathRate = std::clamp (v, -4.0f, 4.0f); break;
        case PathPhase: pathPhase = std::clamp (v, 0.0f, 360.0f) / 360.0f; break;
        case Path: path = std::clamp ((int) std::lround (v), 0, numPaths - 1); break;
        case PathLength: pathLen = std::clamp ((int) std::lround (v), 0, numPathLengths - 1); break;
        case PathSize: pathSize = dsp::clamp01 (v); break;
        case Spread: spread = std::clamp (v, 0.0f, 180.0f); break;
        case Room: room = dsp::clamp01 (v); break;
        case RoomSize: roomSize = dsp::clamp01 (v); break;
        case Doppler: doppler = dsp::clamp01 (v); break;
        case Air: air = dsp::clamp01 (v); break;
        case Rolloff: rolloff = dsp::clamp01 (v); break;
        case LowKeep: lowKeep = std::clamp (v, 20.0f, 400.0f); break;
        case Glide: glide = std::clamp (v, 0.001f, 4.0f); break;
        default: break;
    }
}

float SpaceEffect::distanceGain (float d) const noexcept
{
    return std::min (2.0f, std::pow (refDistance / std::max (d, minDistance), rolloff));
}

void SpaceEffect::advancePhase (int n) noexcept
{
    const double len = pathLengthBeats (pathLen);
    phase += tempo / 60.0 / len * (double) pathRate * (double) n / (double) sr;
    if (playing && hasPpq && path != PathOff && std::abs (pathRate - 1.0f) < 1.0e-4f)
    {
        // phase-locked to the song position while the transport plays at the nominal rate: repeatable motion
        const double now = ppq + (double) ppqSamples / (double) sr * tempo / 60.0;
        const double locked = now / len;
        const double err = locked - phase;
        if (! wasPlaying || std::abs (err) > 0.2) phase = locked; // jump: the glide turns it into a short move
        else phase += 0.1 * err;
    }
    wasPlaying = playing;
    ppqSamples += n;
    if (phase > 1.0e6 || phase < -1.0e6) phase -= std::floor (phase);
}

void SpaceEffect::setRoomShape() noexcept
{
    if (roomSize == roomSizeSet) return;
    roomSizeSet = roomSize;
    static constexpr float base[4] = { 0.0113f, 0.0137f, 0.0171f, 0.0197f };
    const float rt60 = 0.25f + 1.1f * roomSize;
    for (int i = 0; i < 4; ++i)
    {
        const int len = std::clamp ((int) (base[i] * (0.5f + 1.5f * roomSize) * sr), 8, (int) fdn[(size_t) i].size() - 2);
        fdnLen[(size_t) i] = len;
        fdnGainLine[(size_t) i] = std::pow (10.0f, -3.0f * (float) len / (sr * rt60));
    }
}

void SpaceEffect::control (int n) noexcept
{
    if (lowKeep != lowKeepSet)
    {
        lowKeepSet = lowKeep;
        const float g = fastTan (pi * std::min (lowKeep, 0.45f * sr) / sr);
        for (auto& f : xoLp) f.lowPass (g, 0.70710678f);
        for (auto& c : xoHp)
            for (auto& f : c) f.highPass (g, 0.70710678f);
    }
    setRoomShape();
    advancePhase (n);

    // target position
    const Vec3 local = pathPoint (path, phase + (double) pathPhase, pathSize, seed);
    const float k = std::max (0.05f, lengthOf (local));
    const Vec3 o = orient (local, azimuth, elevation);
    const Vec3 tDir { o.x / k, o.y / k, o.z / k };
    const float tDist = std::clamp (distance * k, minDistance, maxDistance);

    // glide: direction on the sphere (normalised lerp), distance in the log domain. Two cascaded stages (critically
    // damped, each half the glide time): the source starts and stops moving smoothly instead of jumping to full speed,
    // so a jump of the position (automation, a fresh target) never steps the Doppler pitch.
    const float c = fresh ? 1.0f : 1.0f - std::exp (-(float) n / (0.5f * glide * sr));
    auto slerp = [c] (const Vec3& from, const Vec3& to) {
        Vec3 nd { from.x + (to.x - from.x) * c, from.y + (to.y - from.y) * c, from.z + (to.z - from.z) * c };
        float l = lengthOf (nd);
        if (l < 0.05f) // exactly opposite: go round the side instead of through the head
        {
            nd = { from.x + 0.2f * from.y, from.y - 0.2f * from.x, from.z };
            l = lengthOf (nd);
        }
        return Vec3 { nd.x / l, nd.y / l, nd.z / l };
    };
    dirMid = slerp (dirMid, tDir);
    dir = slerp (dir, dirMid);
    const float tLog = std::log (tDist);
    logDistMid = fresh ? tLog : logDistMid + (tLog - logDistMid) * c;
    logDist = fresh ? tLog : logDist + (logDistMid - logDist) * c;
    const float d = std::exp (logDist);
    pos = { dir.x * d, dir.y * d, dir.z * d };

    const float bTarget = monitor == Headphones ? 1.0f : 0.0f;
    binauralAmt = fresh ? bTarget : binauralAmt + (bTarget - binauralAmt) * (1.0f - std::exp (-(float) n / (0.03f * sr)));
    if (std::abs (binauralAmt - bTarget) < 1.0e-4f) binauralAmt = bTarget;
    const float b = binauralAmt;

    // distance delay (Doppler): slew-limited, so the pitch shift stays within about +-12 %
    // The limit is a radial speed of 0.12 c: a delay line read with slope s shifts the pitch by (1 - s), and a source at
    // speed v gives s = v / (c + v) receding (pitch / 1.12) and s = -v / (c - v) approaching (pitch / 0.88).
    const float tDelay = doppler * std::max (0.0f, d - minDistance) / speedOfSound * sr;
    constexpr float vMax = 0.12f;
    const float upStep = vMax / (1.0f + vMax) * (float) n, downStep = vMax / (1.0f - vMax) * (float) n;
    const float newDelay = fresh ? tDelay : directDelay + std::clamp (tDelay - directDelay, -downStep, upStep);
    directDelay = newDelay;

    // low band: mono, the mean ear delay and the distance level
    const float lowTarget = 1.0f + itdMax * 0.5f * sr + newDelay;
    const float invN = 1.0f / (float) n;
    if (fresh) { lowDelay = lowTarget; lowGain = distanceGain (d); }
    dLowDelay = (lowTarget - lowDelay) * invN;
    dLowGain = (distanceGain (d) - lowGain) * invN;

    updateSources (pos, b, n);
    updateRoom (pos, b, n);

    uiPos[0].store (pos.x, std::memory_order_relaxed);
    uiPos[1].store (pos.y, std::memory_order_relaxed);
    uiPos[2].store (pos.z, std::memory_order_relaxed);
    uiSpreadValue.store (spread, std::memory_order_relaxed);
    fresh = false;
}

void SpaceEffect::setEar (Ear& E, const Vec3& u, float d, float b, float sign, int e, float invN) noexcept
{
    // interaural delay, spherical-head shadow, near-field level, shoulder reflection; speakers: constant-power pan
    const float K = 2.0f * sr;
    const float axis = e == 0 ? -1.0f : 1.0f;
    const float cosT = std::clamp (u.x * axis, -1.0f, 1.0f);
    const float alpha = std::min (2.8f, shadowAlpha (std::acos (cosT)) / alphaFront);
    const float aEff = 1.0f + b * (alpha - 1.0f);
    const float b0 = (shadowBeta + aEff * K) / (shadowBeta + K), b1 = (shadowBeta - aEff * K) / (shadowBeta + K);
    const float itd = itdSeconds (u) * sr * 0.5f * b;
    const float dTarget = 1.0f + itdMax * 0.5f * sr + directDelay + (e == 0 ? itd : -itd); // source right: left ear later
    const float nf = d < 1.0f ? std::clamp ((1.0f / d - 1.0f) * 0.5f, 0.0f, 1.5f) : 0.0f;
    const float hp = std::max (0.1f, 1.0f + nf * (0.5f * std::max (0.0f, cosT) - 0.4f * std::max (0.0f, -cosT)));
    const float panAngle = (std::clamp (u.x, -1.0f, 1.0f) + 1.0f) * pi * 0.25f;
    const float pan = sqrt2 * (e == 0 ? std::cos (panAngle) : std::sin (panAngle));
    const float gT = sign * (b * hp + (1.0f - b) * pan);
    const float shT = b * 0.22f * (0.5f + 0.5f * u.z);
    E.shDelay = (0.0003f + 0.0006f * (0.5f + 0.5f * u.z)) * sr;
    if (fresh) { E.delay = dTarget; E.b0 = b0; E.b1 = b1; E.gain = gT; E.sh = shT; }
    E.dDelay = (dTarget - E.delay) * invN;
    E.dB0 = (b0 - E.b0) * invN;
    E.dB1 = (b1 - E.b1) * invN;
    E.dGain = (gT - E.gain) * invN;
    E.dSh = (shT - E.sh) * invN;
}

void SpaceEffect::updateSources (const Vec3& p, float b, int n) noexcept
{
    const float d = std::max (minDistance, lengthOf (p));
    const Vec3 u { p.x / d, p.y / d, p.z / d };
    const float invN = 1.0f / (float) n;
    const float back = std::max (0.0f, -u.y), front = std::max (0.0f, u.y), above = std::max (0.0f, u.z), below = std::max (0.0f, -u.z);

    // level: distance rolloff (speakers: a little quieter behind); the side pair keeps the input's width
    const float g = distanceGain (d) * (1.0f - (1.0f - b) * 0.16f * back);
    // (side pair: its two sources add up incoherently and sit off-centre, where the near ear hears them louder)
    const float gTarget[2] = { g, g * 0.6f * (1.0f - 0.35f * spread / 180.0f) };

    // air absorption
    const float fc = 24000.0f / (1.0f + air * 0.15f * std::max (0.0f, d - refDistance));
    const float airCoeff = fc >= 0.45f * sr ? 1.0f : 1.0f - std::exp (-2.0f * pi * fc / sr);

    // directional bands (computed for the mid, shared by the side): binaural set (b = 1) blended with the speaker set
    auto& M = src[0];
    const float el = std::asin (std::clamp (u.z, -1.0f, 1.0f)) * 180.0f / pi;
    M.bands[BPresence].bell (bandFreqG[0], 1.0f, b * (2.0f * front - 1.5f * below - 1.5f * back));
    M.bands[BBack].bell (bandFreqG[1], 1.4f, b * 2.5f * back);
    M.bands[BShadow].highShelf (bandFreqG[2], 0.7071f, b * (-7.0f * back * (1.0f - 0.5f * above)) + (1.0f - b) * (-4.0f * back));
    M.bands[BAbove].bell (bandFreqG[3], 2.0f, b * 5.0f * above + (1.0f - b) * 2.5f * above);
    const float notch = std::clamp (6000.0f * std::pow (2.0f, (el + 45.0f) / 135.0f), 3000.0f, 0.42f * sr);
    const float notchDepth = b * (1.0f - 0.7f * above);
    M.bands[BNotch1].bell (fastTan (pi * notch / sr), 4.0f, -12.0f * notchDepth);
    M.bands[BNotch2].bell (fastTan (pi * std::min (1.55f * notch, 0.44f * sr) / sr), 4.0f, -6.0f * notchDepth);
    for (int s = 0; s < 2; ++s)
    {
        auto& S = src[(size_t) s];
        if (s == 1)
            for (int k = 0; k < 6; ++k) // same coefficients, own state
            {
                auto& t = S.bands[k];
                const auto& f = M.bands[k];
                t.a1 = f.a1; t.a2 = f.a2; t.a3 = f.a3; t.m0 = f.m0; t.m1 = f.m1; t.m2 = f.m2;
            }
        S.airCoeff = airCoeff;
        if (fresh) S.gain = gTarget[s];
        S.dGain = (gTarget[s] - S.gain) * invN;
    }

    // the side pair cancels at low frequencies (the ears hear both sources almost alike): its low part goes to the
    // ears directly, so wide material keeps its width and level (binaural only - panned speakers do not cancel)
    {
        auto& S = src[1];
        const float target = b * 0.9f * std::min (1.0f, spread / 40.0f);
        if (fresh) S.direct = target;
        S.dDirect = (target - S.direct) * invN;
    }
    // mid: one point source at the position
    src[0].numEars = 2;
    for (int e = 0; e < 2; ++e) setEar (src[0].ear[e], u, d, b, 1.0f, e, invN);
    // side: a pair rotated by -+spread/2 around the vertical axis, B with opposite polarity (L = M + S, R = M - S)
    src[1].numEars = 4;
    const float half = 0.5f * spread * pi / 180.0f;
    for (int k = 0; k < 2; ++k)
    {
        const float a = k == 0 ? -half : half, ca = std::cos (a), sa = std::sin (a);
        const Vec3 q { u.x * ca + u.y * sa, -u.x * sa + u.y * ca, u.z };
        for (int e = 0; e < 2; ++e) setEar (src[1].ear[k * 2 + e], q, d, b, k == 0 ? 1.0f : -1.0f, e, invN);
    }
}

void SpaceEffect::updateRoom (const Vec3& p, float b, int n) noexcept
{
    const float invN = 1.0f / (float) n;
    const float rTarget = room;
    if (fresh) roomAmt = rTarget;
    dRoomAmt = (rTarget - roomAmt) * invN;
    const float W = 4.0f + 26.0f * roomSize, H = 2.6f + 6.0f * roomSize, h = std::min (1.6f, 0.5f * H);
    const Vec3 img[6] = { { W - p.x, p.y, p.z }, { -W - p.x, p.y, p.z }, { p.x, W - p.y, p.z },
                          { p.x, -W - p.y, p.z }, { p.x, p.y, -2.0f * h - p.z }, { p.x, p.y, 2.0f * (H - h) - p.z } };
    static constexpr float absorb[6] = { 0.7f, 0.7f, 0.7f, 0.7f, 0.45f, 0.6f };
    const float d = lengthOf (p);
    const float base = 1.0f + itdMax * 0.5f * sr + directDelay;
    const float maxDelay = (float) roomLine.mask - 4.0f;
    const float reflUp = 0.12f / 1.12f * (float) n, reflDown = 0.12f / 0.88f * (float) n;
    for (int i = 0; i < 6; ++i)
    {
        auto& R = refl[(size_t) i];
        const float r = std::max (d, lengthOf (img[i]));
        const Vec3 u { img[i].x / r, img[i].y / r, img[i].z / r };
        const float extra = (r - d) / speedOfSound * sr;
        const float g = 3.0f * absorb[i] * std::pow (refDistance / r, std::max (0.5f, rolloff));
        const float itd = itdSeconds (u) * sr * 0.5f * b;
        const float s = std::clamp (u.x, -1.0f, 1.0f);
        const float panAngle = (s + 1.0f) * pi * 0.25f;
        for (int e = 0; e < 2; ++e)
        {
            const float hp = std::sqrt (0.5f * (1.0f + (e == 0 ? -0.6f : 0.6f) * s)) * sqrt2;
            const float pan = sqrt2 * (e == 0 ? std::cos (panAngle) : std::sin (panAngle));
            const float gT = g * (b * hp + (1.0f - b) * pan);
            const float dT = std::min (maxDelay, base + extra + (e == 0 ? itd : -itd));
            if (fresh) { R.delay[e] = dT; R.gain[e] = gT; }
            // reflections follow the source with the same bounded speed as the direct sound (0.12 c, see control()):
            // no reflection is Doppler-shifted further than the source itself, and no pitch jumps when the room size changes
            const float step = std::clamp (dT - R.delay[e], -reflDown, reflUp);
            R.dDelay[e] = step * invN;
            R.dGain[e] = (gT - R.gain[e]) * invN;
        }
    }
    roomLpCoeff = 1.0f - std::exp (-2.0f * pi * std::min (7000.0f, 0.4f * sr) / sr);
}

void SpaceEffect::process (float* left, float* right, int numSamples) noexcept
{
    float lvl = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        if (countdown <= 0)
        {
            countdown = std::min (controlRate, numSamples - i);
            control (countdown);
        }
        --countdown;

        const float inL = left[i], inR = right[i];
        // crossover: the low band of the mid stays mono and unspatialised
        const float low = xoLp[1].process (xoLp[0].process (0.5f * (inL + inR)));
        const float hl = xoHp[0][1].process (xoHp[0][0].process (inL)), hr = xoHp[1][1].process (xoHp[1][0].process (inR));
        const float hi[2] = { 0.5f * (hl + hr), 0.5f * (hl - hr) }; // mid, side
        lowLine.push (low);
        const float lowOut = lowLine.linear (lowDelay) * lowGain;
        lowDelay += dLowDelay;
        lowGain += dLowGain;
        float outL = lowOut, outR = lowOut, raw = 0.0f;

        for (int s = 0; s < 2; ++s)
        {
            auto& S = src[(size_t) s];
            float x = hi[s] * S.gain;
            S.gain += S.dGain;
            S.air = dsp::flush (S.air + S.airCoeff * (x - S.air));
            x = S.air;
            for (auto& band : S.bands) x = band.process (x);
            S.line.push (x);
            if (s == 0) raw = x;
            for (int k = 0; k < S.numEars; ++k)
            {
                const int e = k & 1;
                auto& E = S.ear[(size_t) k];
                const float y = S.line.cubic (E.delay) + E.sh * S.line.linear (E.delay + E.shDelay);
                const float a = (shadowBeta - 2.0f * sr) / (shadowBeta + 2.0f * sr);
                const float out = E.b0 * y + E.b1 * E.x1 - a * E.y1;
                E.x1 = y;
                E.y1 = dsp::flush (out);
                (e == 0 ? outL : outR) += out * E.gain;
                E.delay += E.dDelay; E.b0 += E.dB0; E.b1 += E.dB1; E.gain += E.dGain; E.sh += E.dSh;
            }
        }

        {
            auto& S = src[1]; // low part of the side, straight to the ears (delayed like the rest)
            S.lp = dsp::flush (S.lp + sideLpCoeff * (S.line.linear (lowDelay) - S.lp));
            const float sd = S.lp * S.direct;
            S.direct += S.dDirect;
            outL += sd;
            outR -= sd;
        }

        // room: image-source reflections of the (darkened) source + a small diffuse tail
        roomLp = dsp::flush (roomLp + roomLpCoeff * (raw - roomLp));
        roomLine.push (roomLp);
        if (roomAmt > 0.0f || dRoomAmt != 0.0f)
        {
            float rl = 0.0f, rr = 0.0f;
            for (auto& R : refl)
            {
                rl += roomLine.linear (R.delay[0]) * R.gain[0];
                rr += roomLine.linear (R.delay[1]) * R.gain[1];
                R.delay[0] += R.dDelay[0]; R.delay[1] += R.dDelay[1];
                R.gain[0] += R.dGain[0]; R.gain[1] += R.dGain[1];
            }
            // 4-line feedback delay network (Hadamard), fed with the reflections
            float o[4];
            for (int k = 0; k < 4; ++k)
            {
                const auto& f = fdn[(size_t) k];
                const int sz = (int) f.size();
                int rp = fdnPos[(size_t) k] - fdnLen[(size_t) k];
                if (rp < 0) rp += sz;
                fdnLp[(size_t) k] = dsp::flush (fdnLp[(size_t) k] + 0.55f * (f[(size_t) rp] - fdnLp[(size_t) k]));
                o[k] = fdnLp[(size_t) k] * fdnGainLine[(size_t) k];
            }
            const float in = 0.35f * (rl + rr);
            const float h0 = 0.5f * (o[0] + o[1] + o[2] + o[3]), h1 = 0.5f * (o[0] - o[1] + o[2] - o[3]);
            const float h2 = 0.5f * (o[0] + o[1] - o[2] - o[3]), h3 = 0.5f * (o[0] - o[1] - o[2] + o[3]);
            const float hv[4] = { h0 + in, h1 + in, h2 - in, h3 - in };
            for (int k = 0; k < 4; ++k)
            {
                auto& f = fdn[(size_t) k];
                f[(size_t) fdnPos[(size_t) k]] = dsp::flush (hv[k]);
                if (++fdnPos[(size_t) k] >= (int) f.size()) fdnPos[(size_t) k] = 0;
            }
            const float tl = 0.6f * (o[0] - o[2]), tr = 0.6f * (o[1] - o[3]);
            outL += roomAmt * (rl + tl);
            outR += roomAmt * (rr + tr);
        }
        roomAmt += dRoomAmt;
        if (countdown == 0) // exact targets at the end of every control block (no drift)
        {
            dRoomAmt = 0.0f;
            roomAmt = room;
        }

        left[i] = outL;
        right[i] = outR;
        lvl = std::max (lvl, std::abs (outL) + std::abs (outR));
    }
    levelAcc = std::max (levelAcc * 0.9f, 0.5f * lvl);
    uiLevelValue.store (levelAcc, std::memory_order_relaxed);
}
} // namespace ohpe
