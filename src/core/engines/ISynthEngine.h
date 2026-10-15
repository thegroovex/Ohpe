// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Envelope.h"
#include "../model/ParamDescriptor.h"

#include <vector>

namespace ohpe
{
/** Static facts about an engine type (for the UI and the parameter IDs). */
struct EngineInfo
{
    const char* id;        ///< stable lowercase ID used in parameter IDs, e.g. "analog" -> "p1.analog.cutoff"
    const char* nameEn;    ///< display name (English)
    const char* nameDe;    ///< display name (German)
    const char* pictogram; ///< pictogram character key for the display, see docs/DESIGN.md
};

/** LFO destinations shared by every engine. "Timbre" is the engine's own brightness control. */
enum class LfoDest { Pitch = 0, Timbre, Amp, Pan, numDests };

/**
    Settings shared by all engines of a part: envelopes, LFO, glide, bend. The part fills this
    from its common parameters each block and hands it to the active engine.
*/
struct VoiceCommon
{
    dsp::AdsrParams amp;        ///< amplitude envelope
    dsp::AdsrParams mod;        ///< filter / modulation envelope
    float modEnvAmount = 0.5f;  ///< -1..+1, how far the mod envelope pushes the engine's timbre
    float lfoRate = 2.0f;       ///< Hz
    float lfoDepth = 0.0f;      ///< 0..1
    int lfoShape = 0;           ///< dsp::Lfo::Shape
    LfoDest lfoDest = LfoDest::Pitch;
    float glideSeconds = 0.0f;
    float pitchOffset = 0.0f;   ///< semitones: tune + fine + pitch bend (already summed)
    float velocitySens = 0.7f;  ///< 0 = no velocity, 1 = full velocity to amp
};

/**
    The interface every synth engine implements (FM, Wavetable, Analog, Pluck, Grain, Vocal,
    Cluster, Sampler ...). One engine instance plays all voices of one part.

    Threading: getParamDescriptors() / prepare() run on the message thread and may allocate.
    Everything else runs on the audio thread and must be real-time safe (no allocation, no
    locks, no I/O). render() ADDS into the buffers.

    Parameters: getParamDescriptors() returns the engine's local descriptors with local IDs
    ("cutoff"); the part prefixes them ("p1.analog.cutoff") and registers them. The first
    four descriptors are the engine's macros (shown on the four encoders). The list is
    append-only like every parameter list. setParam() receives the local index.
*/
class ISynthEngine
{
public:
    static constexpr int numMacros = 4;
    static constexpr int maxVoices = 16;

    virtual ~ISynthEngine() = default;

    virtual EngineInfo info() const noexcept = 0;
    virtual std::vector<ParamDescriptor> getParamDescriptors() const = 0;

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() noexcept = 0;

    virtual void setParam (int localIndex, float plainValue) noexcept = 0;
    virtual void setCommon (const VoiceCommon& common) noexcept = 0;
    virtual void setPolyphony (int voices) noexcept = 0;

    virtual void noteOn (int note, float velocity) noexcept = 0; ///< velocity 0..1
    virtual void noteOff (int note) noexcept = 0;
    virtual void allNotesOff (bool hardStop) noexcept = 0;

    virtual void render (float* left, float* right, int numSamples) noexcept = 0;
    virtual int activeVoiceCount() const noexcept = 0;
};
} // namespace ohpe
