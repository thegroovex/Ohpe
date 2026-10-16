// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../fx/FxRack.h"
#include "../mod/ModMatrix.h"
#include "../model/Events.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "EngineFactory.h"

#include <array>
#include <atomic>
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
    /** 0.4: common parameters added later ("gain"), registered at the end of the layout. */
    void registerLateCommonParams (ParamRegistry& reg) const;
    /** 0.5: "polyglide" (glide between chords with several voices), registered at the end of the layout. */
    void registerLate05Params (ParamRegistry& reg) const;
    /** 0.6: glide mode / rate and the pitch envelope, registered at the end of the layout. */
    void registerLate06Params (ParamRegistry& reg) const;
    /**
        0.7: the FX rack of the part (slots 2-6, mix / bypass of every slot) and the modulation matrix
        (routings, LFO 2 / 3, envelope 3, macros, sequencer lane), registered at the end of the layout.
        registerLate07Params registers both blocks of this part in that order; the 0.7 layout calls the rack
        part for every part first (registerLate07Rack), then the matrix part (registerLate07Mod).
    */
    void registerLate07Rack (ParamRegistry& reg) const;
    void registerLate07Mod (ParamRegistry& reg) const;
    /** 0.4: registers the engine's late parameters (ISynthEngine::firstLateParam() onwards). */
    void registerLateEngineParams (ParamRegistry& reg, EngineType type) const;
    /** Resolves parameter indices; call after the registry is frozen. */
    void bind (const ParamRegistry& reg);

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** Audio thread: reads the part's parameters from the block snapshot (fresh values of a new block). */
    void pullParams (const ParamSnapshot& snap) noexcept;
    /** 0.7: the block snapshot the modulation matrix writes its modulated parameter values into (owner: the core). */
    void setSnapshot (ParamSnapshot* s) noexcept { liveSnapshot = s; }
    /** 0.7, audio thread: tempo and song position at the start of the next render() (tempo-synced LFOs). */
    void setTransport (double bpm, double ppq, bool playing) noexcept
    {
        matrix.setTransport (bpm, ppq, playing);
        rack.setTempo (bpm > 0.0 ? bpm : 120.0);
    }
    /** 0.7: the FX rack (insert effects after the engine; slot 1 = the 0.2 FX slot) and the modulation matrix. */
    FxRack& fxRack() noexcept { return rack; }
    const FxRack& fxRack() const noexcept { return rack; }
    mod::PartMatrix& modMatrix() noexcept { return matrix; }
    const mod::PartMatrix& modMatrix() const noexcept { return matrix; }
    /** 0.7: registry index of the parameter a matrix destination currently moves (-1: none / per voice). Audio thread
        (it depends on the engine and the FX types in use). */
    int destParamIndex (int dest) const noexcept;
    /** 0.7: the target does something in this build (reserved targets whose parameters do not exist yet: false). */
    bool destAvailable (int dest) const noexcept;
    /** 0.7, any thread: current modulation of a parameter in normalised units (0 when not modulated) - UI rings. */
    float modulationOf (int registryIndex) const noexcept;

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
    void applyParams (const ParamSnapshot& s) noexcept; ///< everything pullParams does except capturing modulation bases
    void captureModBases (const ParamSnapshot& s) noexcept;
    void writeModulation() noexcept;
    void renderEngines (float* l, float* r, int n) noexcept;
    void finishBlock (float* l, float* r, int n) noexcept; ///< DC blocker, volume (ramped), FX rack

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
    int gainIndex = -1;
    int polyGlideIndex = -1; ///< 0.5: "pN.polyglide"  ///< 0.4: "pN.gain" (dB), multiplies the part volume
    enum Late06 { lGlideMode = 0, lGlideRate, lPitchEnvAmt, lPitchEnvTime, lPitchEnvCurve, numLate06 };
    std::array<int, numLate06> late06Index {};
    // 0.6 MIDI portamento: CC 65 switches glide off (< 64) / on, CC 5 overrides the glide time until the
    // "glide" parameter itself changes
    bool portaSwitch = true;
    float ccGlideSeconds = -1.0f, lastGlideParam = -1.0f;
    void updateGlide() noexcept;
    VoiceCommon common;
    float dcX[2] = { 0.0f, 0.0f }, dcY[2] = { 0.0f, 0.0f }, dcCoeff = 0.9993f; ///< 0.4: DC blocker (about 5 Hz)
    float volume = 0.8f, gainFactor = 1.0f, bendRange = 2.0f, tune = 0.0f, bend = 0.0f;
    bool sustain = false;
    std::array<bool, 128> sustained {};
    int polyphony = 8;

    // ---- 0.7: FX rack, modulation matrix
    FxRack rack { FxRack::numPartSlots };
    mod::PartMatrix matrix;
    ParamSnapshot* liveSnapshot = nullptr;
    const ParamRegistry* registry = nullptr;
    struct ModTarget
    {
        int index = -1;
        float baseNorm = 0.0f;
    };
    std::array<ModTarget, mod::numSlots> modTargets {};
    std::array<int, mod::numSlots> slotTarget {}; ///< routing -> modTargets entry (-1 = none)
    int numModTargets = 0;
    std::array<std::atomic<int>, mod::numSlots> uiTargetIndex {};
    std::array<std::atomic<float>, mod::numSlots> uiTargetDelta {};
    std::array<int, mod::numDests> partDestIndex {}; ///< registry index of every PartParam target (-1: not registered)
    float lastGain = -1.0f;
    static constexpr int controlBlock = 32; ///< samples per modulation update (as VoiceBase::controlRate)
};
} // namespace ohpe
