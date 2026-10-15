// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "EngineFactory.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace ohpe
{
/**
    One instrument part: a choice of engine plus the settings every engine shares (volume,
    voices, tuning, envelopes, LFO, glide). Parameter IDs are "p<N>." + name, engine
    parameters "p<N>.<engine id>.<local id>".
*/
class Part
{
public:
    explicit Part (int partNumber); ///< 1-based

    const std::string& prefix() const noexcept { return idPrefix; }

    /** Registers the common part parameters (append-only list). */
    void registerCommonParams (ParamRegistry& reg) const;
    /** Registers the parameters of one engine type (no-op for unimplemented types). */
    void registerEngineParams (ParamRegistry& reg, EngineType type) const;
    /** 0.4: registers the engine's late parameters (ISynthEngine::firstLateParam() onwards). */
    void registerLateEngineParams (ParamRegistry& reg, EngineType type) const;
    /** Resolves parameter indices; call after the registry is frozen. */
    void bind (const ParamRegistry& reg);

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** Audio thread: reads the part's parameters from the block snapshot. */
    void pullParams (const ParamSnapshot& snap) noexcept;

    void handleMidi (const MidiEvent& e) noexcept;
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff (bool hard) noexcept;
    /** Per-note expression (MPE) for the voices playing `note` (-1 = all) in every engine. */
    void noteExpression (int note, const NoteExpression& x) noexcept;

    /** Renders into l/r (overwrites). numSamples <= maxBlockSize. */
    void render (float* l, float* r, int numSamples) noexcept;

    EngineType currentEngineType() const noexcept { return current; }
    ISynthEngine* engine (EngineType t) noexcept { return engines[(size_t) t].get(); }
    int activeVoiceCount() const noexcept;

    /** Local parameter ID helper: "p1." + name */
    std::string id (const std::string& name) const { return idPrefix + name; }

private:
    struct EngineSlot
    {
        std::vector<int> paramIndex;
        std::vector<float> lastValue;
    };

    int number;
    std::string idPrefix;
    std::array<std::unique_ptr<ISynthEngine>, (size_t) numEngineTypes> engines;
    std::array<EngineSlot, (size_t) numEngineTypes> slots;
    EngineType current = EngineType::Analog;

    enum Common
    {
        cEngine = 0, cVolume, cVoices, cTune, cFine, cGlide, cBendRange, cVelSens,
        cAmpA, cAmpD, cAmpS, cAmpR, cModA, cModD, cModS, cModR, cModAmount,
        cLfoRate, cLfoDepth, cLfoShape, cLfoDest,
        numCommon
    };
    std::array<int, numCommon> commonIndex {};
    VoiceCommon common;
    float volume = 0.8f, bendRange = 2.0f, tune = 0.0f, bend = 0.0f;
    bool sustain = false;
    std::array<bool, 128> sustained {};
    int polyphony = 8;
};
} // namespace ohpe
