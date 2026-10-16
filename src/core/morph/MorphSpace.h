// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ohpe
{
class Part;

/**
    p16: the morph space ("Morph-Raum") - an XY pad per part that morphs between four complete sounds.

    * Corners: A top left, B top right, C bottom left, D bottom right (x = 0 .. 1 left to right, y = 0 .. 1 bottom
      to top). A corner holds a snapshot of every sound parameter of the part ("pN.*" without the morph block and the
      part's MIDI routing: channel, scale / chord, MPE switches). An empty corner stands for the part's own sound (its
      parameters as edited), so a part with one assigned corner morphs between its own sound and that corner.
    * Weights: bilinear, wA = (1 - x) y, wB = x y, wC = (1 - x)(1 - y), wD = x (1 - y); they always add up to 1.
    * Lanes: corners whose discrete parameters agree (engine, layer engine, FX slot types, every choice / switch /
      whole-number parameter that matters for that engine and those effects) form one lane. Inside a lane the
      continuous parameters are interpolated (in normalised units, so skewed ranges morph evenly); a lane is played by
      one Part. Corners that differ in a discrete parameter (another engine, another effect, another filter type ...)
      play in parallel lanes - extra Part instances created on the message thread - mixed with equal-power gains
      sqrt (lane weight), so the loudness stays even across the pad and nothing switches abruptly. A lane whose weight
      has been 0 for 100 ms stops rendering (CPU); when it gets weight again it restarts with the keys being held.
    * Exactness: on a corner (weight 1) every parameter of that corner's lane is the corner's value, bit for bit.
    * Position: "pN.morph.x" / ".y" (host automation, MIDI learn) + the modulation matrix (targets Morph X / Y) + the
      pressure / slide / mod wheel control ("pN.morph.xsrc" / ".ysrc"), glided with "pN.morph.glide". The glided
      position is the matrix source Morph X / Y.

    Threads: corners, plans and lanes are built on the message thread and published to the audio thread through
    atomics (retired plans are freed by a later update() once the audio thread has moved on).
*/
class MorphSpace
{
public:
    static constexpr int numParts = 4;
    static constexpr int numCorners = 4;
    static constexpr int maxLanes = 4;
    enum Corner { A = 0, B, C, D };
    /** What moves X / Y in addition to the pad ("pN.morph.xsrc" / ".ysrc"; append-only). */
    enum Control { CtrlOff = 0, CtrlPressure, CtrlSlide, CtrlModWheel, numControls };

    MorphSpace();
    ~MorphSpace();
    MorphSpace (const MorphSpace&) = delete;
    MorphSpace& operator= (const MorphSpace&) = delete;

    /** Registers "pN.morph.on / x / y / glide / xsrc / ysrc" for every part (append-only block). */
    static void registerParams (ParamRegistry& reg, int numParts);
    /** Bilinear corner weights for a position (A, B, C, D). */
    static std::array<float, numCorners> weights (float x, float y) noexcept;
    /** Local IDs ("vol", "analog.cutoff" ...) a corner does not keep (morph block, MIDI routing of the part). */
    static bool isExcluded (const std::string& localId) noexcept;
    /** Corner letter "A" .. "D". */
    static const char* cornerName (int corner) noexcept;

    /** Message thread, once: resolves the parameters of every part. */
    void bind (const ParamRegistry& reg, ParamSnapshot& mainSnapshot, const std::array<Part*, numParts>& parts);
    /** Message thread (audio stopped). */
    void prepare (double sampleRate, int maxBlockSize);
    /** Audio thread: silences the lanes (they restart when needed). */
    void reset() noexcept;

    // ---- corners (message thread) -----------------------------------------------------------------------------------
    /** Corner from the part's current sound (its registry values). */
    void captureCorner (int part, int corner, const std::string& name);
    /** Corner from sparse local parameters (a part preset's "params"): parameters it does not list take their
        defaults, like loading the preset into the part. */
    void setCorner (int part, int corner, const std::string& name, const std::map<std::string, float>& localParams);
    void clearCorner (int part, int corner);
    void clearPart (int part);
    void clearAll();
    bool cornerSet (int part, int corner) const;
    bool anyCorner (int part) const;
    std::string cornerLabel (int part, int corner) const; ///< the name given when it was assigned
    /** The corner's parameters (local IDs, every sound parameter; empty when not set). */
    std::map<std::string, float> cornerParams (int part, int corner) const;

    /** JSON of one part's corners ({"corners": [null | {"name", "params"}, x4]}; params sparse: non-default values). */
    std::string partToJson (int part) const;
    bool partFromJson (int part, const std::string& json);
    /** All parts ({"version": 1, "parts": [...]}), state section "morph". */
    std::string toJson() const;
    bool fromJson (const std::string& json);

    /** Message thread (timer): rebuilds the plan of a part whose own sound changed a discrete parameter while it
        stands in for an empty corner, frees retired plans. True when it rebuilt anything. */
    bool update();

    /** Message thread: lanes of the current plan (1 = one sound, interpolated) and the lane of a corner. */
    int numLanes (int part) const;
    int laneOfCorner (int part, int corner) const;

    // ---- UI (any thread) --------------------------------------------------------------------------------------------
    float uiX (int part) const noexcept { return audio[(size_t) part].uiX.load (std::memory_order_relaxed); }
    float uiY (int part) const noexcept { return audio[(size_t) part].uiY.load (std::memory_order_relaxed); }
    bool uiActive (int part) const noexcept { return audio[(size_t) part].uiOn.load (std::memory_order_relaxed); }
    /** Lanes rendering right now (1 .. 4). */
    int uiLanesAwake (int part) const noexcept { return audio[(size_t) part].uiAwake.load (std::memory_order_relaxed); }

    // ---- audio thread -----------------------------------------------------------------------------------------------
    /** After every snapshot pull, before the parts read it: lays the morph values over the snapshot. */
    void afterPull() noexcept;
    void setMonitorMode (int mode) noexcept { monitorMode = mode; }
    /** Renders part `part` (its Part + its lanes) into l / r (overwritten). The core has set the part's transport. */
    void render (int part, float* l, float* r, int n, double bpm, double ppq, bool playing) noexcept;

    /** Tests: value of a registry parameter in the snapshot of the lane that plays `corner` (after afterPull / render). */
    float laneValue (int part, int corner, int registryIndex) const noexcept;

private:
    struct Plan;
    struct Lane;
    struct PartInfo;
    struct Audio
    {
        std::atomic<Plan*> plan { nullptr };
        std::atomic<Plan*> inUse { nullptr };
        Plan* current = nullptr; ///< audio thread: plan of the last block
        std::array<std::atomic<Lane*>, maxLanes - 1> lanes {};
        float x = 0.5f, y = 0.5f, appliedX = -1.0f, appliedY = -1.0f;
        bool init = false, wasOn = false, lanesNeedPull = true;
        std::array<float, maxLanes> gain {};
        std::array<bool, maxLanes> awake {};
        std::array<int, maxLanes> silent {};
        std::array<std::array<float, numCorners>, maxLanes> nw {}; ///< last valid in-lane weights
        std::atomic<float> uiX { 0.5f }, uiY { 0.5f };
        std::atomic<bool> uiOn { false };
        std::atomic<int> uiAwake { 1 };
    };
    struct CornerData
    {
        bool set = false;
        std::string name;
        std::vector<float> plain; ///< per sound parameter
    };

    Plan* acquire (Audio& a) noexcept;
    bool isOn (int part, const Plan* p) const noexcept;
    void applyValues (int part, const Plan& plan, const std::array<float, numCorners>& w) noexcept;
    void wakeLane (int part, int lane) noexcept;
    void sleepLane (int part, int lane) noexcept;
    float controlValue (int part, int control) const noexcept;
    void rebuild (int part);
    uint64_t homeHash (int part) const;
    std::vector<float> homeValues (int part) const;

    const ParamRegistry* reg = nullptr;
    ParamSnapshot* snap = nullptr;
    std::array<Part*, numParts> mains {};
    std::vector<std::unique_ptr<PartInfo>> info;
    std::array<std::array<CornerData, numCorners>, numParts> corners;
    std::array<Audio, numParts> audio;
    std::array<std::vector<std::unique_ptr<Lane>>, numParts> laneStore; ///< owner (message thread)
    std::array<std::unique_ptr<Plan>, numParts> published; ///< owner of the published plans (message thread)
    std::vector<std::unique_ptr<Plan>> retired;
    std::array<uint64_t, numParts> builtHash {};
    double sr = 48000.0;
    int maxBlock = 256;
    int monitorMode = 0;
    std::vector<float> tmpL, tmpR;
};
} // namespace ohpe
