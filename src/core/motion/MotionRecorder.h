// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"

#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace ohpe
{
/**
    p22: the motion recorder ("Bewegungs-Recorder") - knob movements of a part recorded as looping lanes.

    * Per part four lanes. A lane belongs to one continuous parameter of the part ("pN.*", not the motion block):
      while "pN.motion.rec" is on and the transport runs, the first touch of a parameter takes the next free lane,
      and every touch writes the knob's value into the lane at the play position (gaps between two blocks are
      filled linearly). Untouched parts of a lane keep what they had (overdub).
    * Lanes loop over "pN.motion.len" sixteenth steps (1 .. 64, eight points per step) following the transport, so
      a lane of 5 steps against a 16-step pattern runs polymetrically. All lanes of a part share the length; a
      shorter length plays the first part of the recording, nothing is lost.
    * Playback ("pN.motion.on", or while recording): the lane's value is laid over the parameter in the block
      snapshot (below sequencer locks, above the registry value; the modulation matrix moves around it). Depth
      "pN.motion.amt" blends from the knob's own value (0) to the recorded movement (1). Points never written play
      the knob's own value. A lane being touched plays the knob (you hear what you do).
    * Matrix: the four lanes are Part sources "Motion 1 .. 4" (the recorded value 0 .. 1 at the play position,
      0 where nothing is recorded or the transport stops), so a recorded gesture can move other targets too.
    * "pN.motion.clear" (trigger) clears the part's lanes. A block in which more than a few parameters change at
      once (a preset load, a breeding step) is not recorded.
    * Transport stopped or feature off: nothing is laid over the snapshot (neutral).

    Threads: lane data are atomics. The audio thread records and plays; the message thread may clear lanes and
    load / save them (state section "motion").
*/
class MotionRecorder
{
public:
    static constexpr int numParts = 4;
    static constexpr int numLanes = 4;
    static constexpr int maxSteps = 64;
    static constexpr int pointsPerStep = 8;
    static constexpr int maxPoints = maxSteps * pointsPerStep;
    static constexpr int bulkChange = 6; ///< more changed parameters in one block = not a knob movement

    MotionRecorder();
    MotionRecorder (const MotionRecorder&) = delete;
    MotionRecorder& operator= (const MotionRecorder&) = delete;

    /** Registers "pN.motion.on / rec / len / amt / clear" for every part (append-only block). */
    static void registerParams (ParamRegistry& reg, int numParts);
    /** Local IDs a lane can not record (the motion block, MIDI routing / setup of the part). */
    static bool isExcluded (const std::string& localId) noexcept;

    /** Message thread, once: resolves the recordable parameters of every part. */
    void bind (const ParamRegistry& reg, ParamSnapshot& snapshot);
    /** Message thread (audio stopped). */
    void prepare (double sampleRate) noexcept { sr = sampleRate > 0.0 ? sampleRate : 48000.0; }
    /** Audio thread: forgets touches, removes the overlay. */
    void reset() noexcept;

    /** Audio thread, before the snapshot is pulled: records touches, lays the lanes over the snapshot. */
    void process (const Transport& tr, int numSamples) noexcept;
    /** Audio thread, after process(): lane value for the matrix source (0 .. 1, 0 = nothing). */
    float sourceValue (int part, int lane) const noexcept { return audio[(size_t) part].source[(size_t) lane]; }

    // ---- lanes (any thread) ---------------------------------------------------------------------------------------
    int laneTarget (int part, int lane) const noexcept { return lanes[(size_t) part][(size_t) lane].target.load (std::memory_order_acquire); }
    /** Normalised point value 0 .. 1, or -1 where nothing is recorded. */
    float point (int part, int lane, int index) const noexcept;
    bool anyLane (int part) const noexcept;
    bool anyLane() const noexcept;
    void clearLane (int part, int lane) noexcept;
    void clearPart (int part) noexcept;
    void clearAll() noexcept;
    /** Tests / tools: sets a lane directly (normalised points, -1 = empty; extra points ignored). */
    void setLane (int part, int lane, int registryIndex, const std::vector<float>& points) noexcept;

    /** JSON of all lanes ({"version": 1, "parts": [{"lanes": [null | {"target": local id, "points": [...]}]}]}). */
    std::string toJson() const;
    bool fromJson (const std::string& json);

    // ---- UI (any thread) ------------------------------------------------------------------------------------------
    /** Play position in points (0 .. len * pointsPerStep), -1 while not playing. */
    float uiPosition (int part) const noexcept { return audio[(size_t) part].uiPos.load (std::memory_order_relaxed); }
    bool uiTouched (int part, int lane) const noexcept { return audio[(size_t) part].uiTouch[(size_t) lane].load (std::memory_order_relaxed); }
    /** Loop length in points ("pN.motion.len" x pointsPerStep). */
    int lengthPoints (int part) const noexcept;

private:
    struct Lane
    {
        std::atomic<int> target { -1 };
        std::array<std::atomic<float>, maxPoints> points;
        Lane();
    };
    struct PartIdx
    {
        std::string prefix;
        int on = -1, rec = -1, len = -1, amt = -1, clear = -1;
        std::vector<int> recordable;
    };
    struct Audio
    {
        std::vector<float> prev;
        bool wasRec = false, wasClear = false;
        std::array<int, numLanes> applied {}, touch {}, lastPoint {};
        std::array<float, numLanes> lastNorm {}, source {};
        std::atomic<float> uiPos { -1.0f };
        std::array<std::atomic<bool>, numLanes> uiTouch {};
    };

    void recordPart (int part, int pointIndex, int lenPoints, int numSamples) noexcept;
    void writePoints (Lane& lane, int from, int to, float fromValue, float toValue, int lenPoints) noexcept;
    void clearOverlay (int part) noexcept;
    float valueAt (const Lane& lane, double pos, int lenPoints) const noexcept;

    const ParamRegistry* reg = nullptr;
    ParamSnapshot* snap = nullptr;
    std::array<PartIdx, numParts> idx;
    std::array<std::array<Lane, numLanes>, numParts> lanes;
    std::array<Audio, numParts> audio;
    double sr = 48000.0;
};
} // namespace ohpe
