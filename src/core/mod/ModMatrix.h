// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Envelope.h"
#include "../model/FormatVersions.h"
#include "../dsp/Lfo.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "Expression.h"
#include "ModVoice.h"

#include <array>
#include <map>
#include <atomic>
#include <string>
#include <vector>

namespace ohpe::mod
{
/**
    Version of the matrix's meaning (source / target lists, scaling). Presets that contain matrix parameters carry it
    ("modSchema"); migrate() is the hook that adapts presets of older schemas. The lists themselves are append-only,
    so the choice values stored in presets and projects keep their meaning; a schema bump is only needed if the
    scaling of an existing source or target ever had to change.
*/
constexpr int schemaVersion = formats::modSchema;

/** Where a source's value comes from. */
enum class SourceScope
{
    Voice, ///< computed in every voice (envelopes, velocity, per-note expression, random, retriggered LFOs)
    Part   ///< one value for the part: computed by the matrix or fed by another module via PartMatrix::setSourceValue
};

struct SourceInfo
{
    const char* key;    ///< stable key (tools, preset generators, migrations): "lfo2", "macro1" ...
    const char* nameEn;
    const char* nameDe;
    bool bipolar;
    SourceScope scope;
};

/** What a target moves. */
enum class DestKind
{
    None,
    Voice,       ///< per voice: arg = DstPitch / DstLevel / DstPan / DstTimbre
    EngineParam, ///< parameter `arg` (0-based, the engine's own order) of the part's active engine
    PartParam,   ///< the part parameter "pN." + paramSuffix (inert while that parameter does not exist)
    FxMix,       ///< mix of FX rack slot `arg`
    FxParam      ///< parameter (arg % 4) of the effect in FX rack slot (arg / 4)
};

struct DestInfo
{
    const char* key;
    std::string nameEn, nameDe;
    DestKind kind;
    int arg;
    const char* paramSuffix; ///< PartParam only
};

/**
    The source and target tables (index = the value of "pN.mod.sK.src" / ".dst"; append-only). To add a source:
    append it to `Source` and to the table; a Part-scope source is fed with PartMatrix::setSourceValue (e.g. a
    morph pad, a motion lane, a performance pad, harmony events), a Voice-scope source also needs VoiceMod. To
    add a target: append it to `Dest` and the table - a PartParam target names the parameter suffix it moves
    (e.g. a morph position or a resonator body parameter) and works as soon as that parameter is registered.
*/
const std::vector<SourceInfo>& sourceTable();
const std::vector<DestInfo>& destTable();
int sourceIndexOfKey (const std::string& key) noexcept;
int destIndexOfKey (const std::string& key) noexcept;

/**
    Migration hook for presets ("params" of a part or project preset, local or full IDs) written with matrix schema
    `fromSchema` (0 = no matrix parameters). Schema 1 is the first one: nothing to do yet.
*/
void migrate (int fromSchema, std::map<std::string, float>& params);

std::vector<std::string> sourceNamesEn();
std::vector<std::string> sourceNamesDe();
std::vector<std::string> destNamesEn();
std::vector<std::string> destNamesDe();
std::vector<std::string> curveNamesEn();
std::vector<std::string> curveNamesDe();
std::vector<std::string> syncNamesEn();
std::vector<std::string> syncNamesDe();

/**
    0.7: the modulation matrix of one part - 16 routings "pN.mod.s1.*" ... "pN.mod.s16.*" (source, via, target,
    amount, curve), LFO 2 / 3 ("pN.lfo2.*", "pN.lfo3.*"), envelope 3 ("pN.env3.*"), macros 1-4 ("pN.macro1" ...) and
    the sequencer lane ("pN.seqlane", meant for step locks).

    Routings to Pitch, Level, Pan and Timbre run in every voice (VoiceProgram / VoiceMod). Routings to
    parameters (engine, volume, LFO, FX) move the parameter's normalised value by amount x source and use
    part-wide ("mono") source values: free-running LFOs, the envelopes / velocity / key / random / expression of
    the newest note. The part writes the modulated values into its block snapshot every control block, so every
    module sees them like a parameter value; host automation and the UI keep the unmodulated value.

    Audio thread: everything except registerParams / bind, real-time safe.
*/
class PartMatrix
{
public:
    static void registerParams (ParamRegistry& reg, const std::string& partPrefix, int partNumber);
    /**
        p33: the Expression page "pN.expr.vel.dst1/amt1/curve1/dst2/amt2/curve2" and "pN.expr.at.*" (aftertouch: channel
        pressure and poly / MPE pressure, i.e. the Note Pressure source). Amount 0 (the default) = no routing.
        Registered after everything else (append-only IDs); bind() picks them up when they exist.
    */
    static void registerExpressionParams (ParamRegistry& reg, const std::string& partPrefix, int partNumber);
    /**
        p33: which matrix target each expression function (expr::Target) means for the part's active engine (-1 = none
        on this engine); set by the part before pull(). Vibrato maps to Pitch (via the vibrato source).
    */
    void setExpressionTargets (const int* map) noexcept { exprMap = map; }
    void bind (const ParamRegistry& reg, const std::string& partPrefix);
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Reads routings and settings (LFO 2 / 3, env 3, macros, lane); the part's amp / mod envelope and LFO 1 settings. */
    void pull (const ParamSnapshot& s, const dsp::AdsrParams& amp, const dsp::AdsrParams& modEnv, float lfo1Rate, int lfo1Shape) noexcept;

    // ---- performance input (audio thread) ----
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;
    void setModWheel (float v) noexcept { mono[SrcModWheel] = v; }
    void setPitchBend (float v) noexcept { mono[SrcPitchBend] = v; }
    void setChannelPressure (float v) noexcept { mono[SrcChannelPressure] = v; }
    void setNoteExpression (int note, float pressure, float slide, float noteBend, float notePitch = 0.0f) noexcept;
    /** Value of a Part-scope source supplied by another module (audio thread, before render). Only sources appended
        after the 0.7 list (from numBuiltinSources on) take values this way. */
    void setSourceValue (int src, float v) noexcept
    {
        if (src >= numBuiltinSources && src < numSources) mono[(size_t) src] = v;
    }
    void setTransport (double bpm, double ppq, bool playing) noexcept { tempo = bpm > 0.0 ? bpm : 120.0; ppqPos = ppq; transportOn = playing; }

    /** Advances the part-wide sources by n samples (call once per rendered block / sub-block). */
    void advance (int n) noexcept;

    const VoiceProgram& voiceProgram() const noexcept { return program; }
    /** Routings 0..15 = "pN.mod.s1..16", 16..19 = the Expression page (p33: velocity 1 / 2, aftertouch 1 / 2). */
    const std::array<Route, numAllSlots>& routes() const noexcept { return slots; }
    /** Routings to parameters (not to Pitch / Level / Pan / Timbre) exist. */
    bool hasParamRoutes() const noexcept { return numParamRoutes > 0; }
    /** Value of routing `slot` (amount x source, mono sources) - for routings to parameters the smoothed value. */
    float routeValue (int slot) const noexcept { return value[(size_t) slot]; }
    /** Part-wide value of a source after advance(). */
    float sourceValue (int src) const noexcept { return src >= 0 && src < numSources ? mono[(size_t) src] : 0.0f; }

    /** Smooths the parameter routings (n samples) and returns their values (index = slot). */
    void tickParamRoutes (int n) noexcept;
    bool isParamRoute (int slot) const noexcept { return isParam[(size_t) slot]; }

    // ---- UI feedback (any thread) ----
    float uiRouteValue (int slot) const noexcept { return uiValue[(size_t) slot].load (std::memory_order_relaxed); }
    float uiSourceValue (int src) const noexcept { return src >= 0 && src < numSources ? uiSource[(size_t) src].load (std::memory_order_relaxed) : 0.0f; }
    void publishUi() noexcept;
    /** p21, any thread: stage of the part-wide amp (which = 0) / mod (1) envelope of the newest note
        (dsp::Adsr::Stage as int: 0 idle, 1 attack, 2 decay, 3 sustain, 4 release) - the pro view's playhead. */
    int uiEnvStage (int which) const noexcept { return uiStage[(size_t) (which & 1)].load (std::memory_order_relaxed); }

private:
    struct SlotIdx { int src = -1, via = -1, dst = -1, amt = -1, curve = -1; };
    std::array<SlotIdx, numAllSlots> idx {}; ///< p33: the expression slots use src = -1 (fixed source), dst = function
    const int* exprMap = nullptr;
    std::array<std::array<int, 7>, 2> lfoIdx {};
    std::array<int, 6> env3Idx {};
    std::array<int, 4> macroIdx {};
    int laneIdx = -1;

    std::array<Route, numAllSlots> slots {};
    std::array<bool, numAllSlots> isParam {};
    std::array<float, numAllSlots> value {}, target {};
    std::array<bool, numAllSlots> fresh {};
    float vibPhase = 0.0f; ///< p33: part-wide SrcVibrato
    int numParamRoutes = 0;
    VoiceProgram program;
    std::array<float, numSources> mono {};

    // part-wide sources
    dsp::Adsr ampEnv, modEnv;
    dsp::AdsrParams ampParams, modParams;
    dsp::Lfo lfo1;
    std::array<ModLfo, 2> lfo;
    ModEnv3 env3;
    dsp::Random rng { 0x6d2b79f5u };
    std::array<bool, 128> held {};
    int heldCount = 0, lastNote = -1;
    float sr = 48000.0f, sinceNote = 0.0f;
    double tempo = 120.0, ppqPos = 0.0;
    bool transportOn = false;
    uint32_t noteCounter = 1;

    std::array<std::atomic<float>, numAllSlots> uiValue {};
    std::array<std::atomic<float>, numSources> uiSource {};
    std::array<std::atomic<int>, 2> uiStage {}; ///< p21
};
} // namespace ohpe::mod
