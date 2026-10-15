// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "../sample/SampleData.h"
#include "PolyEngine.h"

#include <array>
#include <atomic>

namespace ohpe
{
/**
    Granular engine. Every voice sprays short windowed grains read from a source: the loaded
    sample, or - through the IAudioRegion interface - a region of a tape track. Controls:
    position, grain size, density, spray (random position), pitch, reverse probability, scan,
    stereo spread and tone. The note transposes the grains relative to C4 (MIDI 60).

    Without a loaded sample the engine uses a built-in harmonic tone, so it always sounds.
*/
namespace grain
{
/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Position = 0, Size, Density, Spray, // macros 1-4
    Pitch, ReverseProb, Spread, Scan, Source, Tone,
    Instrument, // 0.4: factory instrument as source (late parameter)
    numParams
};

enum Sources { SourceSample = 0, SourceTape };

constexpr int maxGrains = 24; ///< per voice
constexpr int rootNote = 60;  ///< the note that plays the source at its original pitch

struct Shared
{
    float position = 0.3f, sizeMs = 120.0f, density = 24.0f, spray = 0.15f;
    float pitch = 0.0f, reverseProb = 0.0f, spread = 0.6f, scan = 0.1f, tone = 1.0f;
    int source = SourceSample;
    const RtShared<SampleData>* sample = nullptr;
    const std::atomic<const IAudioRegion*>* tapeRegion = nullptr;
    int instrument = 0;                        ///< 0.4: factory instrument (0 = built-in texture)
    const std::atomic<bool>* userLoaded = nullptr; ///< 0.4: a user sample is loaded (it wins)

    /**
        The region the voices read this block: the tape region if selected and set, else the user
        sample, else the factory instrument (0.4) if chosen and loaded, else the built-in texture.
        `root` receives the key that plays the region at its original pitch.
    */
    const IAudioRegion* activeSource (int& root) const noexcept
    {
        root = rootNote;
        if (source == SourceTape && tapeRegion != nullptr)
            if (const auto* t = tapeRegion->load (std::memory_order_acquire)) return t;
        if (instrument > 0 && (userLoaded == nullptr || ! userLoaded->load (std::memory_order_acquire)))
        {
            int r = rootNote;
            if (const auto* f = factorySource (instrument, r)) { root = r; return f; }
        }
        return sample != nullptr ? sample->get() : nullptr;
    }
    const IAudioRegion* activeSource() const noexcept { int r; return activeSource (r); }
    static const IAudioRegion* factorySource (int instrument, int& root) noexcept;
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
    int activeGrains() const noexcept;

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0, inc = 0.0;
        int age = 0, length = 0;
        float gainL = 0.0f, gainR = 0.0f;
    };
    void spawn (const IAudioRegion& src, float pitch) noexcept;

    std::array<Grain, (size_t) maxGrains> grains {};
    const IAudioRegion* source = nullptr;
    int sourceRoot = rootNote;  ///< key that plays the source at its original pitch
    double scanOffset = 0.0;  ///< seconds the scan has moved the position
    double untilNext = 0.0;   ///< samples until the next grain
    dsp::Random rng;
    dsp::Svf filter[2];
};
} // namespace grain

class GrainEngine final : public PolyEngine<grain::Voice>
{
public:
    GrainEngine();
    EngineInfo info() const noexcept override { return { "grain", "Grain", "Grain", "grain" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    int firstLateParam() const noexcept override { return grain::Instrument; }
    void setParam (int localIndex, float value) noexcept override;

    /** Message thread: the sample to granulate (nullptr = built-in tone). */
    void setSample (SamplePtr sample);
    /** Message thread: the loaded sample (nullptr = built-in tone). */
    SamplePtr sample() const { return userSample; }

    /**
        Any thread: a tape region to granulate when "source" is Tape (nullptr = none). The owner
        (tape machine) keeps the region alive and stable until it sets another one or nullptr
        and one audio block has passed.
    */
    void setTapeRegion (const IAudioRegion* region) noexcept { tapeRegion.store (region, std::memory_order_release); }

    const grain::Shared& getShared() const noexcept { return shared; }

private:
    grain::Shared shared;
    RtShared<SampleData> source;
    SamplePtr builtInTone, userSample;
    std::atomic<const IAudioRegion*> tapeRegion { nullptr };
    std::atomic<bool> userLoaded { false };
};
} // namespace ohpe
