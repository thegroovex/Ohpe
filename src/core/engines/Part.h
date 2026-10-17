// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "../fx/FxRack.h"
#include "../fx/Resonator.h"
#include "../fx/Space3D.h"
#include "../mod/ModMatrix.h"
#include "../mod/Character.h"
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
    /**
        0.8: the layer - a second engine of the same part playing every note with it ("layer.engine",
        Off = none), with its own level, tuning, stereo width, optional own amp envelope and a
        cross-modulation of the main engine by the layer (ring, AM or fold). Registered at the end.
    */
    void registerLate08Params (ParamRegistry& reg) const;
    /** 0.8: registers the engine's 0.8 parameters (ISynthEngine::firstLate08Param() onwards). */
    void registerLate08EngineParams (ParamRegistry& reg, EngineType type) const;
    /** 0.8: MPE / aftertouch target amounts, expression smoothing and per-note slide time (group "partN/midi"). */
    void registerLate08MpeParams (ParamRegistry& reg) const;
    /** 0.8: "pN.monobass" - the side signal below this frequency is removed after the FX slot (20 Hz = off). */
    void registerLate08MonoParams (ParamRegistry& reg) const;
    /** 0.8: makes the low end mono below "pN.monobass" (end of finishBlock, after the FX rack). */
    void applyBassMono (float* l, float* r, int numSamples) noexcept;
    /**
        1.0: the 3D spatialiser stage "pN.space.*" ("on" + the Space 3D block, SpaceEffect), at the very end of the part
        (after the FX rack and bass mono, before the mixer). Off = exact bypass; switching crossfades over 20 ms.
    */
    void registerLate10SpaceParams (ParamRegistry& reg) const;
    /**
        p15: "pN.life" 0..1 (0 = static, the sound exactly as without it): every voice of the part keeps evolving on its
        own - pitch drift, filter / brightness breathing, engine colour, grain scatter (VoiceBase life motion).
    */
    void registerLate15LifeParams (ParamRegistry& reg) const;
    /**
        p20: "pN.usertable.wt" (the Wavetable engine plays the part's user wavetable) and "pN.usertable.vocal" (the
        Vocal engine uses the user formant tracks). Off, or on without a loaded table = the built-in material.
    */
    void registerLate20UserTableParams (ParamRegistry& reg) const;
    /**
        p19: the resonator body stage "pN.reso.*" ("on" + the Resonator block, ResonatorEffect) right after the engines and
        the layer (before the part's volume and FX rack). It follows the part's keyboard and expression (KeyState).
        Off (or mix 0) = exact bypass; switching crossfades over 20 ms.
    */
    void registerLate19ResoParams (ParamRegistry& reg) const;
    /** p19: the resonator stage (UI: body visual, level; tests). */
    const ResonatorEffect& resoStage() const noexcept { return reso; }
    /** p19, any thread: the stage is on (or still fading out). */
    bool resoRunning() const noexcept { return resoRunningFlag.load (std::memory_order_relaxed); }
    /** p19: what the keyboard of this part is doing (audio thread; tests). */
    const KeyState& keyState() const noexcept { return keys; }
    /**
        p21: the character macros "pN.char.bright", "pN.char.hard", "pN.char.alive", "pN.char.wide" (-1..1, 0 = neutral;
        see mod/Character.h): each moves several parameters of the part at once (inside the part, after the matrix).
    */
    void registerLate21CharacterParams (ParamRegistry& reg) const;
    /**
        p32: "pN.glide.press" (-1..+1, 0 = neutral): aftertouch scales the glide time. The pressure is the larger of the
        channel pressure and the newest note's pressure (poly / MPE). Amount > 0: pressing harder makes the glide longer
        (time x (1 + 3 x amount x pressure), up to 4x); amount < 0: faster (time x (1 + amount x pressure), full pressure
        at -1 = instant). Works on the "glide" parameter and the CC 5 portamento time alike.
    */
    void registerLate32GlidePressParams (ParamRegistry& reg) const;
    /** p32 (tests): the glide time the voices use now (seconds, after CC 65 / CC 5 and the pressure scaling). */
    float effectiveGlideSeconds() const noexcept { return common.glideSeconds; }
    /** p33: the Expression page "pN.expr.vel.*" / "pN.expr.at.*" (velocity / aftertouch -> two functions each; matrix routings). */
    void registerLate33ExpressionParams (ParamRegistry& reg) const;
    /** p33: the matrix target an Expression page function (mod::expr::Target) drives on engine `type` (-1 = none there). */
    int expressionTarget (EngineType type, int function) const noexcept
    {
        return function >= 0 && function < mod::expr::numTargets ? exprDest[(size_t) type][(size_t) function] : -1;
    }
    /** 1.0, audio thread: master monitoring mode (SpaceEffect::Headphones / Speakers) for the stage and the rack. */
    void setMonitorMode (int mode) noexcept
    {
        space.setMonitorMode (mode);
        rack.setMonitorMode (mode);
    }
    /** 1.0: the spatialiser stage (UI: position / level; tests). */
    const SpaceEffect& spaceStage() const noexcept { return space; }
    /** 1.0, any thread: the stage is on (or still fading out). */
    bool spaceRunning() const noexcept { return spaceRunningFlag.load (std::memory_order_relaxed); }
    /** 0.8: the layer engine, or nullptr when the layer is off (or set to the main engine). */
    ISynthEngine* layerEngine() noexcept { return layerType >= 0 ? engines[(size_t) layerType].get() : nullptr; }
    enum LayerXMod { XRing = 0, XAm, XFold, numXMods };
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
        rack.setTransportPosition (ppq, playing); // 1.0: tempo-synced motion (Space 3D)
        space.setTempo (bpm > 0.0 ? bpm : 120.0);
        space.setTransportPosition (ppq, playing);
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

    /**
        p16 (morph space), audio thread: parts that play along with this one - the extra sound lanes of the morph pad
        (other engines / effect chains). Every note, expression and controller this part receives is passed on to them.
        Slot k = 0..maxFollowers-1, nullptr = none.
    */
    static constexpr int maxFollowers = 3;
    void setFollower (int k, Part* p) noexcept
    {
        if (k >= 0 && k < maxFollowers) followers[(size_t) k] = p;
    }
    /** p16: velocity of a key that is down (0 = up; sustained keys count as up), the sustain pedal, the pitch bend
        (-1..+1) - to start a woken morph lane with the notes being played. Audio thread. */
    float heldVelocity (int note) const noexcept { return note >= 0 && note < 128 ? held[(size_t) note] : 0.0f; }
    bool sustainPedal() const noexcept { return sustain; }
    float pitchBend() const noexcept { return bend; }

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
        std::vector<character::Weights> charWeights; ///< p21: how the character macros move each parameter
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
    int glidePressIndex = -1;  ///< p32: "pN.glide.press"
    float glidePressAmt = 0.0f; ///< p32: -1..+1 (0 = the glide time ignores the pressure)
    VoiceCommon common;
    // ---- 0.8 layer
    enum Late08 { lyEngine = 0, lyLevel, lyTune, lyFine, lyXMode, lyXMod, lyWidth, lyOwnEnv, lyA, lyD, lyS, lyR, numLate08 };
    int exprSmoothIndex = -1, exprSlideIndex = -1; ///< 0.8: "pN.mpe.smooth", "pN.mpe.slide"
    int monoBassIndex = -1;                        ///< 0.8: "pN.monobass"
    int lifeIndex = -1;                            ///< p15: "pN.life"
    int userWtIndex = -1, userVocalIndex = -1;     ///< p20: "pN.usertable.wt" / "pN.usertable.vocal"
    std::array<int, character::numMacros> charIndex { -1, -1, -1, -1 }; ///< p21: "pN.char.*"
    std::array<float, character::numMacros> charAmount {};             ///< p21: current macro values (-1..1)
    bool charActive = false;                                           ///< p21: any macro != 0
    float monoBassHz = 20.0f, partSr = 48000.0f;
    dsp::Svf monoLp1, monoLp2;                     ///< 4th-order Butterworth high-pass of the side signal
    float exprBlend = 0.0f;                        ///< 0.8: latest per-note "layer blend" expression (part-wide)
    std::array<int, numLate08> late08Index {};
    int layerType = -1; ///< EngineType of the layer, -1 = off
    float layerLevel = 0.7f, layerTune = 0.0f, layerXMod = 0.0f, layerWidth = 0.5f, xEnv = 0.0f, xEnvCoeff = 0.01f;
    int layerXMode = XRing;
    bool layerOwnEnv = false;
    dsp::AdsrParams layerAmp;
    std::vector<float> layerL, layerR;
    void pushCommon() noexcept; ///< hands `common` to every engine (and the tuned copy to the layer)
    float dcX[2] = { 0.0f, 0.0f }, dcY[2] = { 0.0f, 0.0f }, dcCoeff = 0.9993f; ///< 0.4: DC blocker (about 5 Hz)
    float volume = 0.8f, gainFactor = 1.0f, bendRange = 2.0f, tune = 0.0f, bend = 0.0f;
    bool sustain = false;
    std::array<bool, 128> sustained {};
    std::array<float, 128> held {};            ///< p16: velocities of the keys that are down
    std::array<Part*, maxFollowers> followers {}; ///< p16: morph lanes
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
    std::array<ModTarget, mod::numAllSlots> modTargets {};
    std::array<int, mod::numAllSlots> slotTarget {}; ///< routing -> modTargets entry (-1 = none); p33: + the expression slots
    int numModTargets = 0;
    std::array<std::atomic<int>, mod::numAllSlots> uiTargetIndex {};
    std::array<std::atomic<float>, mod::numAllSlots> uiTargetDelta {};
    std::array<std::array<int, mod::expr::numTargets>, numEngineTypes> exprDest {}; ///< p33: function -> matrix target per engine
    float glideMod = 0.0f; ///< p33: the matrix's offset on the glide time (seconds; added to the parameter or CC 5 time)
    std::array<int, mod::numDests> partDestIndex {}; ///< registry index of every PartParam target (-1: not registered)
    float lastGain = -1.0f;
    // ---- 1.0: 3D spatialiser stage
    void applySpace (float* l, float* r, int n) noexcept;
    SpaceEffect space;
    int spaceOnIndex = -1;
    std::array<int, SpaceEffect::numParams> spaceIndex {};
    std::array<float, SpaceEffect::numParams> spaceLast {};
    bool spaceOn = false;
    float spaceFade = 0.0f, spaceFadeStep = 0.001f;
    std::vector<float> spaceDryL, spaceDryR;
    std::atomic<bool> spaceRunningFlag { false };
    // ---- p19: resonator body stage
    void applyReso (float* l, float* r, int n) noexcept;
    void updateHeldKeys() noexcept;
    ResonatorEffect reso;
    KeyState keys;
    int resoOnIndex = -1;
    std::array<int, ResonatorEffect::numParams> resoIndex {};
    std::array<float, ResonatorEffect::numParams> resoLast {};
    bool resoOn = false;
    float resoFade = 0.0f;
    std::vector<float> resoDryL, resoDryR;
    std::atomic<bool> resoRunningFlag { false };
    float keyNoteBend = 0.0f; ///< per-note bend (semitones) of the newest note
    static constexpr int controlBlock = 32; ///< samples per modulation update (as VoiceBase::controlRate)
};
} // namespace ohpe
