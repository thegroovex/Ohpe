// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "../sample/SampleData.h"
#include "../sample/Slicer.h"
#include "PolyEngine.h"

#include <atomic>
#include <string>
#include <vector>

namespace ohpe
{
/**
    Sampler engine: multisample zones (key + velocity range, root key, loop), or auto-slicing of
    one sample by its transients with one slice per key ("slice to key").

    Samples arrive from the message thread (setSample / addZone / setZones); the engine keeps an
    immutable SampleMap published through RtShared, so the audio thread never allocates. Without
    any loaded sample the engine plays a built-in harmonic tone (root C4), so it always sounds -
    or, since 0.4, the factory instrument chosen with "inst" (see sample/FactoryBank.h).
*/
namespace sampler
{
/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Start = 0, Tone, LoopStart, LoopEnd, // macros 1-4
    Root, LoopMode, Reverse, Mode, SliceSensitivity, SliceBaseKey,
    Instrument, // 0.4: factory instrument (late parameter, registered at the end of the layout)
    Instrument2, // 0.8: keyboard instruments ("inst2", 0 = off; registered at the end of the layout)
    numParams
};

enum LoopModes { LoopOff = 0, LoopForward, LoopPingPong };
enum Modes { ModeZones = 0, ModeSlices };

/** One multisample zone as the message thread describes it. */
struct ZoneSpec
{
    SamplePtr sample;
    int keyLo = 0, keyHi = 127;
    int velLo = 1, velHi = 127;
    int rootKey = -1;            ///< -1 = use the "root" parameter
    int64_t loopStart = -1, loopEnd = -1; ///< frames; -1 = use the loop parameters / file loop
};

/** Immutable playback map (built on the message thread). */
struct SampleMap
{
    struct Zone
    {
        const SampleData* sample = nullptr;
        int keyLo = 0, keyHi = 127, velLo = 1, velHi = 127, rootKey = -1;
        int64_t loopStart = -1, loopEnd = -1;
    };
    std::vector<ZoneSpec> specs;          ///< owners + descriptions (for saving)
    std::vector<Zone> zones;              ///< same order as specs
    SamplePtr sliceSource;                ///< sample that is sliced (first zone's sample)
    std::vector<int64_t> slices;          ///< slice start frames (first = 0)
    float sliceSensitivity = 0.5f;        ///< sensitivity the slices were built with
    bool builtIn = false;                 ///< true = only the built-in tone
};

struct Shared
{
    float start = 0.0f, tone = 1.0f, loopStart = 0.0f, loopEnd = 1.0f;
    int root = 60, loopMode = LoopOff, mode = ModeZones, sliceBase = 36;
    bool reverse = false;
    RtShared<SampleMap>* map = nullptr;
    int instrument = 0;                    ///< 0.4: factory instrument index (0 = built-in tone)
    int instSel = 0, inst2Sel = 0;         ///< 0.8: the "inst" / "inst2" values (instrument = the effective one)
    const SampleMap* factory = nullptr;    ///< 0.4: its map (process lifetime), nullptr = not loaded
};

class Voice : public VoiceBase
{
public:
    const Shared* shared = nullptr;

    void prepare (float sampleRate) noexcept;
    void start (int note, float vel, bool legato, const VoiceCommon& c) noexcept;
    void release() noexcept { releaseBase(); }
    void kill() noexcept;
    void render (float* l, float* r, int n, const VoiceCommon& c) noexcept;

private:
    bool pickRegion (const SampleMap& m) noexcept;

    const SampleMap* playingMap = nullptr;  ///< map the voice reads (user map or factory map)
    const SampleMap* ownerMap = nullptr;    ///< the engine's published map when the voice started
    const SampleData* sample = nullptr;
    int rootKey = 60;
    bool slice = false;
    double pos = 0.0;
    int direction = 1;
    int64_t regionStart = 0, regionEnd = 0, loopStart = 0, loopEnd = 0;
    int loopMode = LoopOff;
    float declick = 1.0f;
    dsp::Svf filter[2];
    float filterMix = -1.0f; ///< 0.8: tone-filter crossfade (0 = bypassed, 1 = filtering, < 0 = set at the next block)
};
} // namespace sampler

class SamplerEngine final : public PolyEngine<sampler::Voice>
{
public:
    SamplerEngine();
    EngineInfo info() const noexcept override { return { "sampler", "Sampler", "Sampler", "sampler" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    int firstLateParam() const noexcept override { return sampler::Instrument; }
    int firstLate08Param() const noexcept override { return sampler::Instrument2; }
    void setParam (int localIndex, float value) noexcept override;

    // ---- message thread: samples and zones ----
    /** One sample over the whole keyboard (replaces all zones). nullptr = back to the built-in tone. */
    void setSample (SamplePtr sample, int rootKey = -1);
    /** Adds a multisample zone (the built-in tone is dropped with the first real zone). */
    void addZone (const sampler::ZoneSpec& zone);
    /** Replaces all zones at once. Empty = built-in tone. */
    void setZones (std::vector<sampler::ZoneSpec> zones);
    void clearZones() { setZones ({}); }
    /** The current zones (the built-in tone is not listed). */
    std::vector<sampler::ZoneSpec> zones() const;
    /** Slice start frames of the sliced sample. */
    std::vector<int64_t> slicePoints() const;
    /** Re-slices when the "slicesens" parameter moved since the last build; true if it re-sliced. */
    bool updateSlicing();
    /** Re-slices now with the given sensitivity. */
    void rebuildSlices (float sensitivity);

    const sampler::Shared& getShared() const noexcept { return shared; }

private:
    void publish (std::vector<sampler::ZoneSpec> specs, float sensitivity);

    sampler::Shared shared;
    RtShared<sampler::SampleMap> map;
    SamplePtr builtInTone;
    std::atomic<float> requestedSensitivity { 0.5f };
};
} // namespace ohpe
