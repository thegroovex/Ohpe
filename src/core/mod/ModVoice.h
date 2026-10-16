// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// 0.7: modulation matrix - sources, curves, the extra LFOs / envelope and the per-voice evaluation.
// Header-only: voices evaluate their routings at control rate (VoiceBase::controlRate samples).

#include "../dsp/DspUtil.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ohpe::mod
{
/** Modulation sources ("pN.mod.sK.src" / ".via"; append-only). Bipolar ones run -1..+1, the rest 0..1. */
enum Source
{
    SrcOff = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcAmpEnv, SrcModEnv, SrcEnv3, SrcVelocity, SrcKeyTrack, SrcModWheel, SrcPitchBend,
    SrcChannelPressure, SrcNotePressure, SrcSlide, SrcNoteBend, SrcRandom, SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcSeqLane,
    SrcMorphX, SrcMorphY, // p16: the morph pad's position (Part scope, fed by the morph space)
    numSources
};
/** Sources of the 0.7 list; sources appended later (from here on) are Part-scope values fed by other modules. */
static constexpr int numBuiltinSources = SrcSeqLane + 1;

/** Modulation destinations ("pN.mod.sK.dst"; append-only; what each one moves: destTable() in ModMatrix.h).
    Pitch .. Timbre act on every voice on its own (per voice), the others on a parameter of the part (one value
    for the part, from part-wide sources and the newest note). */
enum Dest
{
    DstOff = 0, DstPitch, DstLevel, DstPan, DstTimbre,
    DstEngine1, // ... DstEngine1 + numEngineDests - 1: parameter 1..24 of the active engine (its own order)
    DstVolume = DstEngine1 + 24,
    DstLfo1Rate, DstLfo1Depth, DstLfo2Rate, DstLfo2Depth, DstLfo3Rate, DstLfo3Depth, DstModEnvAmount,
    DstFx1Mix, // ... FX 1-6 mix
    DstFx1Param1 = DstFx1Mix + 6, // ... FX slot k parameter j: DstFx1Param1 + 4 * (k - 1) + (j - 1)
    DstAzimuth = DstFx1Param1 + 24, DstElevation, DstDistance, DstPathRate, DstPathPhase, // reserved: 3D spatialiser (1.0)
    DstMorphX, DstMorphY, // p16: the morph pad's position ("pN.morph.x" / "pN.morph.y")
    numDests
};
static constexpr int numEngineDests = 24;
static constexpr int numFxSlots = 6;
static constexpr int numFxDestParams = 4;
static constexpr int numSlots = 16; ///< routings per part

enum Curve { CurveLinear = 0, CurveExp, CurveLog, CurveS, CurveSteps, numCurves };

inline bool isBipolar (int src) noexcept
{
    return src == SrcLfo1 || src == SrcLfo2 || src == SrcLfo3 || src == SrcKeyTrack || src == SrcPitchBend || src == SrcSlide
        || src == SrcNoteBend || src == SrcRandom;
}
inline bool isVoiceDest (int dst) noexcept { return dst >= DstPitch && dst <= DstTimbre; }

/** Shapes the source value (mirrored for negative values, so bipolar sources stay symmetric). */
inline float applyCurve (int curve, float x) noexcept
{
    if (curve == CurveLinear) return x;
    const float a = std::min (1.0f, std::abs (x));
    float y = a;
    switch (curve)
    {
        case CurveExp: y = a * a * a; break;
        case CurveLog: y = 1.0f - (1.0f - a) * (1.0f - a) * (1.0f - a); break;
        case CurveS: y = a * a * (3.0f - 2.0f * a); break;
        case CurveSteps: y = std::floor (a * 8.0f + 1.0e-4f) / 8.0f; break;
        default: break;
    }
    return x < 0.0f ? -y : y;
}

// ------------------------------------------------------------------------------------------------ LFO 2 / 3
/** Tempo divisions of LFO 2 / 3 ("pN.lfo2.sync"; append-only; 0 = free running in Hz). */
inline double syncBeats (int division) noexcept
{
    static constexpr double beats[] = { 0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 3.0, 1.5, 0.75, 0.375,
                                        8.0 / 3.0, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0 };
    return division > 0 && division < (int) (sizeof (beats) / sizeof (beats[0])) ? beats[division] : 0.0;
}
static constexpr int numSyncDivisions = 17;

enum LfoShape { ShapeSine = 0, ShapeTriangle, ShapeSawUp, ShapeSawDown, ShapeSquare, ShapeSampleHold, ShapeSmoothRandom, numLfoShapes };
enum LfoMode { ModeFree = 0, ModeRetrigger, ModeOneShot, numLfoModes };

struct LfoSettings
{
    float rate = 1.0f;  ///< Hz when sync = 0
    int sync = 0;
    int shape = ShapeSine;
    float phase = 0.0f; ///< start phase 0..1
    float fade = 0.0f;  ///< seconds the depth fades in after a note starts
    int mode = ModeRetrigger;
    float depth = 1.0f;

    float hz (double bpm) const noexcept
    {
        const double b = syncBeats (sync);
        return b > 0.0 ? (float) (std::max (1.0, bpm) / 60.0 / b) : rate;
    }
};

/** LFO with seven shapes, start phase, one-shot (envelope) mode. Advanced at control rate; value -1..+1. */
class ModLfo
{
public:
    void trigger (const LfoSettings& s, uint32_t seed) noexcept
    {
        rng.seed (seed ^ 0x2545f491u);
        phase = std::clamp (s.phase, 0.0f, 1.0f);
        if (phase >= 1.0f) phase = 0.0f;
        prev = rng.nextBipolar();
        held = rng.nextBipolar();
        finished = false;
        out = shape (s.shape);
    }

    /** Sets the phase directly (free-running LFOs locked to the transport). */
    void setPhase (float p, int shapeIndex) noexcept
    {
        if (p < phase - 0.5f) wrap(); // passed a cycle boundary
        phase = p;
        out = shape (shapeIndex);
    }

    float advance (const LfoSettings& s, int n, float sampleRate, double bpm) noexcept
    {
        if (finished) return out;
        phase += s.hz (bpm) * (float) n / sampleRate;
        if (phase >= 1.0f)
        {
            if (s.mode == ModeOneShot)
            {
                phase = 1.0f;
                finished = true;
                out = s.shape == ShapeSampleHold || s.shape == ShapeSmoothRandom ? held : shapeEnd (s.shape);
                return out;
            }
            phase -= std::floor (phase);
            wrap();
        }
        out = shape (s.shape);
        return out;
    }

    float value() const noexcept { return out; }
    float currentPhase() const noexcept { return phase; }

private:
    void wrap() noexcept
    {
        prev = held;
        held = rng.nextBipolar();
    }
    float shape (int s) const noexcept
    {
        switch (s)
        {
            case ShapeTriangle: return 1.0f - 4.0f * std::abs (phase - 0.5f);
            case ShapeSawUp: return 2.0f * phase - 1.0f;
            case ShapeSawDown: return 1.0f - 2.0f * phase;
            case ShapeSquare: return phase < 0.5f ? 1.0f : -1.0f;
            case ShapeSampleHold: return held;
            case ShapeSmoothRandom: { const float t = 0.5f - 0.5f * std::cos (dsp::pi * phase); return prev + (held - prev) * t; }
            default: return std::sin (dsp::twoPi * phase);
        }
    }
    static float shapeEnd (int s) noexcept
    {
        switch (s)
        {
            case ShapeTriangle: return -1.0f;
            case ShapeSawUp: return 1.0f;
            case ShapeSawDown: return -1.0f;
            case ShapeSquare: return -1.0f;
            default: return 0.0f;
        }
    }

    dsp::Random rng { 0x1u };
    float phase = 0.0f, prev = 0.0f, held = 0.0f, out = 0.0f;
    bool finished = false;
};

/** Fade-in gain t seconds after the note started. */
inline float fadeGain (float fadeSeconds, float t) noexcept { return fadeSeconds > 0.0f ? std::min (1.0f, t / fadeSeconds) : 1.0f; }

// ------------------------------------------------------------------------------------------------ envelope 3
struct Env3Settings
{
    float delay = 0.0f, attack = 0.01f, hold = 0.0f, decay = 0.5f, sustain = 0.5f, release = 0.5f;
};

/** Delay - attack (linear) - hold - decay - sustain - release (exponential), 0..1, at control rate. */
class ModEnv3
{
public:
    enum Stage { Idle = 0, Delay, Attack, Hold, Decay, Sustain, Release };

    void noteOn() noexcept { stage = Delay; time = 0.0f; }
    void noteOff() noexcept
    {
        if (stage != Idle) stage = Release;
    }
    void reset() noexcept { stage = Idle; level = 0.0f; time = 0.0f; }

    float advance (const Env3Settings& s, int n, float sr) noexcept
    {
        const float dt = (float) n / sr;
        switch (stage)
        {
            case Idle: break;
            case Delay:
                time += dt;
                if (time >= s.delay) { stage = Attack; time = 0.0f; }
                break;
            case Attack:
                level += dt / std::max (0.0005f, s.attack);
                if (level >= 1.0f) { level = 1.0f; stage = Hold; time = 0.0f; }
                break;
            case Hold:
                time += dt;
                if (time >= s.hold) stage = Decay;
                break;
            case Decay:
                level = s.sustain + (level - s.sustain) * std::exp (-4.6f * dt / std::max (0.001f, s.decay));
                if (std::abs (level - s.sustain) < 1.0e-4f) { level = s.sustain; stage = Sustain; }
                break;
            case Sustain: level = s.sustain; break;
            case Release:
                level *= std::exp (-4.6f * dt / std::max (0.001f, s.release));
                if (level < 1.0e-4f) { level = 0.0f; stage = Idle; }
                break;
        }
        return level;
    }

    float value() const noexcept { return level; }
    bool isActive() const noexcept { return stage != Idle; }

private:
    Stage stage = Idle;
    float level = 0.0f, time = 0.0f;
};

// ------------------------------------------------------------------------------------------------ routings
struct Route
{
    int src = SrcOff, via = SrcOff, dst = DstOff, curve = CurveLinear;
    float amount = 0.0f;
};

/** Value of one routing for a set of source values (source value through the curve, scaled by via and amount). */
inline float evaluate (const Route& r, const float* sources) noexcept
{
    float x = applyCurve (r.curve, sources[r.src]);
    if (r.via != SrcOff) x *= sources[r.via];
    return x * r.amount;
}

/**
    What every voice of a part needs to evaluate the per-voice routings (destination Pitch, Level, Pan,
    Timbre). Written by the part before rendering, read by its voices (same thread).
*/
struct VoiceProgram
{
    std::array<Route, numSlots> routes {};
    int numRoutes = 0;
    std::array<LfoSettings, 2> lfo {}; ///< LFO 2, LFO 3
    Env3Settings env3;
    std::array<float, numSources> mono {}; ///< part-wide source values (wheel, bend, macros, free-running LFO 2 / 3 ...)
    double bpm = 120.0;
    uint32_t seed = 1; ///< changes with every note (per-note random, LFO S&H)
};

/** Per-voice result: semitones, gain factor, pan offset, timbre offset. */
struct VoiceOut
{
    float pitch = 0.0f, level = 1.0f, pan = 0.0f, timbre = 0.0f;
};

/** The per-voice part of the matrix: LFO 2 / 3, envelope 3, per-note random, evaluated per control block. */
class VoiceMod
{
public:
    static constexpr float pitchRange = 12.0f; ///< semitones at amount 100 %

    void start (int note, float velocity, const VoiceProgram& p) noexcept
    {
        key = std::clamp (((float) note - 60.0f) / 60.0f, -1.0f, 1.0f);
        vel = velocity;
        rng.seed (p.seed * 0x9e3779b1u + (uint32_t) note * 0x85ebca6bu);
        random = rng.nextBipolar();
        for (size_t i = 0; i < 2; ++i) lfo[i].trigger (p.lfo[i], p.seed + (uint32_t) i * 7919u);
        env3.noteOn();
        age = 0.0f;
        first = true;
    }
    void release() noexcept { env3.noteOff(); }
    void kill() noexcept { env3.reset(); }

    /**
        One control block of n samples: advances LFO 2 / 3 and envelope 3, evaluates the per-voice routings.
        amp / mod: the voice's envelope levels; lfo1: the voice's LFO 1 (-1..+1, before its depth);
        pressure / slide / bend: the note's expression (0..1, -1..+1, -1..+1).
    */
    const VoiceOut& tick (const VoiceProgram& p, int n, float sr, float amp, float modEnv, float lfo1, float pressure, float slide,
                          float noteBend) noexcept
    {
        age += (float) n / sr;
        std::array<float, numSources> s = p.mono;
        for (size_t i = 0; i < 2; ++i)
        {
            const auto& ls = p.lfo[i];
            const float v = ls.mode == ModeFree ? p.mono[(size_t) SrcLfo2 + i] : lfo[i].advance (ls, n, sr, p.bpm) * ls.depth;
            s[(size_t) SrcLfo2 + i] = v * fadeGain (ls.fade, age);
        }
        s[SrcOff] = 0.0f;
        s[SrcLfo1] = lfo1;
        s[SrcAmpEnv] = amp;
        s[SrcModEnv] = modEnv;
        s[SrcEnv3] = env3.advance (p.env3, n, sr);
        s[SrcVelocity] = vel;
        s[SrcKeyTrack] = key;
        s[SrcNotePressure] = pressure;
        s[SrcSlide] = slide;
        s[SrcNoteBend] = noteBend;
        s[SrcRandom] = random;

        float pitch = 0.0f, level = 0.0f, pan = 0.0f, timbre = 0.0f;
        for (int i = 0; i < p.numRoutes; ++i)
        {
            const auto& r = p.routes[(size_t) i];
            const float v = evaluate (r, s.data());
            switch (r.dst)
            {
                case DstPitch: pitch += v; break;
                case DstLevel: level += v; break;
                case DstPan: pan += v; break;
                case DstTimbre: timbre += v; break;
                default: break;
            }
        }
        // a short glide (about 1 ms) takes the edges off steps (S&H, Steps curve) without blurring them
        const float a = first ? 1.0f : 1.0f - std::exp (-(float) n / (0.001f * sr));
        first = false;
        result.pitch += (pitch * pitchRange - result.pitch) * a;
        result.pan += (pan - result.pan) * a;
        result.timbre += (timbre - result.timbre) * a;
        result.level = std::max (0.0f, 1.0f + level); // ramped per sample by the voice
        return result;
    }

    const VoiceOut& output() const noexcept { return result; }

private:
    std::array<ModLfo, 2> lfo;
    ModEnv3 env3;
    dsp::Random rng { 7u };
    float key = 0.0f, vel = 1.0f, random = 0.0f, age = 0.0f;
    bool first = true;
    VoiceOut result;
};
} // namespace ohpe::mod
