// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Envelope.h"
#include "../model/ParamDescriptor.h"

#include <cstdint>

#include <vector>

namespace ohpe
{
namespace mod { struct VoiceProgram; }

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
enum GlideMode { GlideAuto = 0, GlideLegato, GlideAlways };

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
    bool polyGlide = false;     ///< 0.5: with several voices, new notes glide from the nearest note just released
    /** 0.6 glide mode: Auto = glide while the previous note still sounds (as before 0.6); Legato = glide only
        while another key is held (mono: the envelopes are not retriggered); Always = glide from the previous
        note even after silence. */
    int glideMode = 0;
    bool glideConstantRate = false; ///< 0.6: false = the glide takes `glideSeconds`, true = `glideSeconds` per octave
    float pitchEnvAmount = 0.0f;    ///< 0.6: semitones the pitch starts away from the note (-24..+24, 0 = off)
    float pitchEnvTime = 0.25f;     ///< 0.6: seconds until the pitch has arrived at the note
    float pitchEnvCurve = 0.5f;     ///< 0.6: 0 = linear, 1 = fast start, slow arrival
    float pitchOffset = 0.0f;   ///< semitones: tune + fine + pitch bend (already summed)
    float velocitySens = 0.7f;  ///< 0 = no velocity, 1 = full velocity to amp
    const mod::VoiceProgram* modProgram = nullptr; ///< 0.7: per-voice routings of the modulation matrix (owned by the part)
    float exprSmooth = 0.0f;    ///< 0.8: seconds the per-note expression (pressure, timbre) glides to new values (0 = at once)
    float exprSlide = 0.0f;     ///< 0.8: seconds per-note pitch bends glide (at least 2 ms: no zipper steps)
    /** p50 MPE HD: low-pass cutoffs (Hz) of the per-note X (pitch, note bend), Y (slide and what the timbre target
        moves) and Z (pressure and what the pressure target moves; the level with the pressure onset). All 0 = the
        classic exprSmooth / exprSlide above. Targets: MpeInput::Target of "pN.mpe.press" / "pN.mpe.timbre". */
    float exprHzX = 0.0f, exprHzY = 0.0f, exprHzZ = 0.0f;
    int exprZTarget = -1, exprYTarget = -1;
    bool exprAmpZ = false;
    /** p50 pitch rounding of per-note bends: 0 = off, 1 = the note starts on the nearest semitone (the slide after
        it is free), 2 = also pulled to the nearest semitone continuously, over pitchRoundTime seconds. */
    int pitchRound = 0;
    float pitchRoundTime = 0.15f;
    /** p15 "Life": 0 = static (the voices sound exactly as without it) .. 1 = most alive. Every voice drifts on its
        own: pitch, filter / brightness breathing, engine colour (wavetable position, FM index, formant), grain
        scatter, a little pan and level. Followed by every voice over ~60 ms, so turning it never steps. */
    float life = 0.0f;
    uint32_t lifeSeed = 0;      ///< p15: per part (and layer) seed of the voices' life motion (deterministic)
    /** p31 SUB layer (PartSub): level 0 = off (the voices render exactly as without it) .. 1; octave 0 = -1 oct,
        1 = -2 oct; shape PartSub::Shape; direct = after the engine's filter (engines that filter in the voice: Analog,
        Wavetable; the others always add the sub after their own tone shaping). */
    float subLevel = 0.0f;
    int subOct = 0;
    int subShape = 0;
    bool subDirect = false;
};

/**
    Per-note expression (MPE): set on the voices playing one note, reset to neutral when a voice
    starts. Values are already mapped to their targets by the part's expression routing.
*/
struct NoteExpression
{
    float pitch = 0.0f;  ///< semitones added to the note (per-note pitch bend)
    float timbre = 0.0f; ///< -1..+1 added to the engine's own brightness control
    float amp = 1.0f;    ///< gain factor 0..2
    float lfo = 0.0f;    ///< added to the LFO depth (0..1 in total)
    float pan = 0.0f;    ///< -1..+1 added to the pan position
    // 0.7: the raw expression of the note, as modulation matrix sources (independent of the targets above)
    float pressure = 0.0f; ///< 0..1 channel / poly pressure or MPE pressure
    float slide = 0.0f;    ///< -1..+1 timbre (CC 74, MPE "slide"), 0 = centre
    float noteBend = 0.0f; ///< -1..+1 per-note pitch bend of an MPE member channel
    // 0.8: more targets (-1..+1 unless noted; engines without the matching control ignore them)
    float cutoff = 0.0f;   ///< filter / tone, +-3 octaves
    float position = 0.0f; ///< Wavetable position, Grain position (+-0.5)
    float index = 0.0f;    ///< FM index (FM engine, Wavetable FM)
    float density = 0.0f;  ///< Grain density, +-2 octaves
    float drive = 0.0f;    ///< saturation (Analog drive, Wavetable pre-filter drive), 0..1
    float vibrato = 0.0f;  ///< per-note vibrato depth (0..1 = up to half a semitone)
    float formant = 0.0f;  ///< formant shift (Vocal +-12 semitones, Wavetable warp)
    float blend = 0.0f;    ///< layer blend of the part (latest value, part-wide: -1 = main engine only .. +1 = more layer)
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

    /**
        0.4: index of the first descriptor that was added after the engine's parameters were
        first registered. Those "late" parameters are registered in a later append-only block
        (Part::registerLateEngineParams) so the global parameter order never changes. Default:
        none.
    */
    virtual int firstLateParam() const noexcept { return 1 << 20; }
    /** 0.8: index of the first descriptor added in 0.8 (registered by Part::registerLate08EngineParams). */
    virtual int firstLate08Param() const noexcept { return 1 << 20; }

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

    /** p20: play the user material (user wavetable / user formants) instead of the built-in one. Optional. */
    virtual void setUserSource (bool on) noexcept { (void) on; }

    /** Per-note expression for every voice playing `note` (-1 = all voices). Optional. */
    virtual void noteExpression (int note, const NoteExpression& expression) noexcept { (void) note; (void) expression; }
};
} // namespace ohpe
