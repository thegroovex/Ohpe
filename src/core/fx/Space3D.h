// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "IEffect.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ohpe
{
namespace space
{
/** Head-centred coordinates in metres: x = right, y = front, z = up. */
struct Vec3
{
    float x = 0.0f, y = 1.0f, z = 0.0f;
};

/** Motion paths (append-only, index = the "path" choice). */
enum PathType
{
    PathOff = 0, PathCircle, PathFigure8, PathFlyBy, PathRandom, PathPendulum, PathSpiral,
    // p30 (appended - the indices above keep their meaning):
    PathPingPong, ///< tempo-synced front <-> back jumps (each half cycle starts with a quick move, then holds)
    PathSwarm,    ///< random flutter: fast small jitter on top of a slow random drift (not periodic)
    PathWhisper,  ///< near-field: very close behind the head, lingering at one ear, then the other
    PathWalk,     ///< walks away into the distance and comes back (Doppler, air, level)
    PathOverhead, ///< a vertical circle through both ears: over the head and underneath
    PathWeave,    ///< an orbit that weaves up and down three times per turn and breathes in distance
    // p73 (appended - the indices above keep their meaning):
    PathPingPongLR, ///< tempo-synced left <-> right ear jumps (in front on the way right, behind on the way back)
    PathCorners,    ///< steps through four corners (front left, front right, back right, back left), each on the beat
    PathBreathe,    ///< near <-> far in front of you, slowly, rising a little as it comes close
    PathHelix,      ///< a helix: orbits while rising from below ear height to above the head and back down, evenly
    PathFlyOver,    ///< a straight line over the head: far in front -> overhead -> far behind and back
    PathBounce,     ///< hops like a ball, left -> right -> left, six hops per cycle
    PathWander,     ///< a smooth random loop that repeats every cycle (seeded: every part its own shape)
    PathLoopOrbit,  ///< an orbit with four small loops per turn (an epicycle) - the source curls back and forth
    numPaths
};

/** True for paths that repeat exactly every cycle (all but the random ones). */
constexpr bool pathIsPeriodic (int path) noexcept { return path != PathRandom && path != PathSwarm; }

/** Path lengths of one cycle in beats ("pathlen" choice, append-only). */
double pathLengthBeats (int choice) noexcept;
constexpr int numPathLengths = 11;

/**
    Where a path puts the source at phase `phase` (cycles, unwrapped; the random orbit uses the integer part too).
    Returns a position before the base rotation: direction in the path's own frame (front = +y) scaled by a distance
    factor (1 = the base distance). Pure function - shared by the DSP, the display and the tests.
*/
Vec3 pathPoint (int path, double phase, float size, uint32_t seed) noexcept;

/** Rotates a path-frame position by the base elevation (pitch, degrees) and azimuth (yaw, degrees, + = right). */
Vec3 orient (Vec3 v, float azimuthDeg, float elevationDeg) noexcept;

/** Azimuth (degrees, -180..180, + = right) and elevation (degrees) of a position. */
float azimuthOf (const Vec3& v) noexcept;
float elevationOf (const Vec3& v) noexcept;
float lengthOf (const Vec3& v) noexcept;

/** Interaural time difference of a direction in seconds (spherical head, + = the right ear leads). */
float itdSeconds (const Vec3& unitDirection) noexcept;

constexpr float headRadius = 0.0875f;  ///< metres
constexpr float speedOfSound = 343.0f; ///< metres / second
constexpr float minDistance = 0.2f, maxDistance = 40.0f, refDistance = 1.5f;
} // namespace space

/**
    1.0: "Space 3D" - the 3D spatialiser. Used twice with the same parameter block:
      * the per-part stage "pN.space.*" (Part, after the FX rack and bass mono, before the mixer; switched by "pN.space.on"),
      * FX rack type "Space 3D" (InsertSlot::Space3D, "pN.fxK.space3d.*", "mfxK.space3d.*").

    Model (own, parametric - no measured data set):
      * low band below "lowkeep" (Linkwitz-Riley 24 dB/oct.) stays mono and unspatialised (only distance level / delay);
      * the stereo input becomes two virtual sources (left / right channel) "spread" degrees apart around the position;
      * per source: air absorption (one-pole, distance), distance level (rolloff), directional bands (front presence,
        back band + pinna shadow, above band), two pinna notches that rise with elevation;
      * per ear: fractional delay line (cubic) = distance delay (Doppler, slew-limited) + interaural time difference
        (spherical head), spherical-head shadow filter (first order, coefficients interpolated per sample), shoulder
        reflection tap, near-field level difference;
      * room: six image-source early reflections of a shoebox room that follow the source + a small diffuse tail (FDN);
      * speaker mode ("monitor" = speakers): no interaural delay, no head shadow / pinna colouration - constant-power
        panning, depth by level, air, back darkening and the room; mono-compatible. Switching glides (30 ms).
    Everything moves sample-smoothly: the position glides ("glide"), delays ramp per sample, filters update every
    16 samples from the smoothed position. No allocation after prepare().
*/
class SpaceEffect final : public IEffect
{
public:
    /// Descriptor order. The first five local IDs are the modulation-matrix targets of the per-part stage
    /// ("space.azimuth" ...); the first four are "FX k Param 1-4" of the rack type.
    enum Param
    {
        Azimuth = 0, Elevation, Distance, PathRate, PathPhase, Path, PathLength, PathSize,
        Spread, Room, RoomSize, Doppler, Air, Rolloff, LowKeep, Glide, numParams
    };
    enum Monitor { Headphones = 0, Speakers };

    SpaceEffect();

    EffectInfo info() const noexcept override { return { "space3d", "Space 3D", "3D-Raum", "space" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float plainValue) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 1.5; }
    void setTempo (double bpm) noexcept override { tempo = bpm > 1.0 ? bpm : 120.0; }
    void setTransportPosition (double ppq, bool playing) noexcept override;
    void setMonitorMode (int mode) noexcept override { monitor = mode == Speakers ? Speakers : Headphones; }
    int modulationParam (int j) const noexcept override { return j; }

    /** p77: motion retrigger (per-part stage only, own parameter "pN.space.retrig"): Free = runs on (and locks to the song
        position at rate x1 while the transport plays), First = restarts on a note after silence, Note = on every note. */
    enum Retrig { RetrigFree = 0, RetrigFirst, RetrigNote, numRetrigModes };
    void setRetrigger (int mode) noexcept { retrigMode = mode < 0 || mode >= numRetrigModes ? RetrigFree : mode; }
    int retriggerMode() const noexcept { return retrigMode; }
    /** p77: restarts the motion from its start position (phase 0 + phase offset); the position glides there in a few ms
        (whatever the Motion Glide), the interaural delay at the Doppler speed limit. */
    void retrigger() noexcept { retrigPending = true; }
    /** p77: strength of the 3D effect 0..1 ("pN.space.amount"): 1 = full (unchanged), 0 = the plain stereo input at the base
        distance level. Scales motion size, base direction, distance, HRTF colouring, Doppler, air and room; smoothed. */
    void setAmount (float a) noexcept { amount = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
    float currentAmount() const noexcept { return amountSm; }
    static constexpr float retrigGlideSeconds = 0.008f; ///< p77: the glide to the start position (two stages, ~25 ms to arrive)

    /** Seed of the random orbit (the part number), so parts do not move alike. */
    void setSeed (uint32_t s) noexcept { seed = s; }

    // ---- UI (any thread) ----
    space::Vec3 uiPosition() const noexcept
    {
        return { uiPos[0].load (std::memory_order_relaxed), uiPos[1].load (std::memory_order_relaxed), uiPos[2].load (std::memory_order_relaxed) };
    }
    float uiLevel() const noexcept { return uiLevelValue.load (std::memory_order_relaxed); }
    float uiSpread() const noexcept { return uiSpreadValue.load (std::memory_order_relaxed); }
    /** Current motion phase in cycles incl. the phase offset (unwrapped) - the display draws the path ahead from it. */
    double uiPhase() const noexcept { return uiPhaseValue.load (std::memory_order_relaxed); }
    uint32_t getSeed() const noexcept { return seed; }

    // ---- tests ----
    space::Vec3 currentPosition() const noexcept { return pos; }
    float currentDirectDelaySamples() const noexcept { return directDelay; }
    static constexpr int controlRate = 16;

private:
    struct Svf // TPT state-variable filter (bell / high shelf / low / high pass)
    {
        float ic1 = 0.0f, ic2 = 0.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;
        void bell (float g, float q, float db) noexcept;
        void highShelf (float g, float q, float db) noexcept;
        void lowPass (float g, float q) noexcept;
        void highPass (float g, float q) noexcept;
        float process (float x) noexcept;
        void reset() noexcept { ic1 = ic2 = 0.0f; }
    };
    struct Line // power-of-two delay line, fractional reads
    {
        std::vector<float> buf;
        int mask = 0, pos = 0;
        void prepare (int minLength);
        void reset() noexcept;
        void push (float x) noexcept { pos = (pos + 1) & mask; buf[(size_t) pos] = x; }
        float cubic (float delay) const noexcept;  ///< delay >= 1 (samples)
        float linear (float delay) const noexcept; ///< delay >= 0
    };
    struct Ear // per-ear state of one virtual source (ramped per sample)
    {
        float delay = 0.0f, dDelay = 0.0f, b0 = 1.0f, dB0 = 0.0f, b1 = 0.0f, dB1 = 0.0f, gain = 1.0f, dGain = 0.0f;
        float sh = 0.0f, dSh = 0.0f, shDelay = 1.0f;
        float x1 = 0.0f, y1 = 0.0f; // head-shadow filter state
    };
    struct Source // src[0] = mid (one point source), src[1] = side (a pair +-spread/2 apart, opposite polarity)
    {
        Svf bands[6];
        float air = 0.0f, airCoeff = 1.0f, gain = 1.0f, dGain = 0.0f;
        Line line;
        Ear ear[4]; // mid: left, right; side: A left, A right, B left, B right
        int numEars = 2;
        float lp = 0.0f, direct = 0.0f, dDirect = 0.0f; // side only: its low part straight to the ears (binaural)
    };
    struct Reflection
    {
        float delay[2] {}, dDelay[2] {}, gain[2] {}, dGain[2] {};
    };

    void control (int n) noexcept;  ///< position, delays and filter targets for the next n samples
    void advancePhase (int n) noexcept;
    void updateSources (const space::Vec3& p, float binaural, int n) noexcept;
    void setEar (Ear& e, const space::Vec3& unitDir, float d, float binaural, float sign, int ear, float invN) noexcept;
    void updateRoom (const space::Vec3& p, float binaural, int n) noexcept;
    void setRoomShape() noexcept;
    float distanceGain (float d) const noexcept;

    // parameters
    float azimuth = 0.0f, elevation = 0.0f, distance = 1.5f, pathRate = 1.0f, pathPhase = 0.0f, pathSize = 0.5f, spread = 40.0f,
          room = 0.25f, roomSize = 0.4f, doppler = 0.5f, air = 0.6f, rolloff = 0.7f, lowKeep = 120.0f, glide = 0.05f;
    int path = space::PathOff, pathLen = 4;
    int retrigMode = RetrigFree, retrigHold = 0; // p77
    bool retrigPending = false;
    float amount = 1.0f, amountSm = 1.0f, roomEff = 0.25f, airEff = 0.6f, dryGain = 0.0f, dDryGain = 0.0f, wetGain = 1.0f, dWetGain = 0.0f, dryOffset = 0.0f, earUp = 1.0f, earDown = 1.0f; // p77
    Line dryLine[2]; // p77: the input, delayed like the wet signal's centre (amount < 1)
    int monitor = Headphones;
    uint32_t seed = 1;

    // state
    float sr = 48000.0f;
    double tempo = 120.0, phase = 0.0, ppq = 0.0;
    bool playing = false, wasPlaying = false, hasPpq = false, fresh = true;
    int64_t ppqSamples = 0;
    space::Vec3 dir { 0.0f, 1.0f, 0.0f }, dirMid { 0.0f, 1.0f, 0.0f }, pos { 0.0f, 1.5f, 0.0f };
    float logDist = 0.0f, logDistMid = 0.0f, binauralAmt = 1.0f, directDelay = 0.0f, lowDelay = 0.0f, dLowDelay = 0.0f, lowGain = 1.0f, dLowGain = 0.0f;
    float lowKeepSet = -1.0f, roomSizeSet = -1.0f;
    int countdown = 0;
    Svf xoLp[2], xoHp[2][2]; // crossover: low pass of the mid (2 stages), high pass per channel (2 stages)
    Line lowLine, roomLine;
    Source src[2];
    Reflection refl[6];
    float roomLp = 0.0f, roomLpCoeff = 0.5f, roomAmt = 0.0f, dRoomAmt = 0.0f;
    // diffuse tail: 4-line FDN
    std::array<std::vector<float>, 4> fdn;
    std::array<int, 4> fdnLen {}, fdnPos {};
    std::array<float, 4> fdnLp {};
    std::array<float, 4> fdnGainLine {};
    float levelAcc = 0.0f;
    int levelCount = 0;
    std::array<float, 6> bandFreqG {};
    float sideLpCoeff = 0.1f;

    std::array<std::atomic<float>, 3> uiPos {};
    std::atomic<float> uiLevelValue { 0.0f }, uiSpreadValue { 40.0f };
    std::atomic<double> uiPhaseValue { 0.0 };
};
} // namespace ohpe
