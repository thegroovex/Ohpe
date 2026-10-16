// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Envelope.h"
#include "../dsp/Lfo.h"
#include "../mod/ModVoice.h"
#include "ISynthEngine.h"
#include "VoiceAllocator.h"

#include <algorithm>
#include <array>

namespace ohpe
{
/**
    0.7: the amplitude envelope of a voice with the modulation matrix's level on top: next() returns the
    envelope times a gain that ramps sample by sample to its target (no zipper noise at control rate).
    With the gain at 1 the result is exactly the envelope.
*/
class ModulatedAdsr : public dsp::Adsr
{
public:
    float next() noexcept
    {
        const float e = dsp::Adsr::next();
        if (rampLeft > 0)
        {
            gain += gainStep;
            if (--rampLeft == 0) gain = gainTarget;
        }
        return e * gain;
    }
    /** Ramps the gain to `target` over n samples. */
    void rampGain (float target, int n) noexcept
    {
        if (target == gainTarget && rampLeft == 0) return;
        gainTarget = target;
        rampLeft = std::max (1, n);
        gainStep = (target - gain) / (float) rampLeft;
    }
    void resetGain (float g = 1.0f) noexcept { gain = gainTarget = g; rampLeft = 0; gainStep = 0.0f; }

private:
    float gain = 1.0f, gainTarget = 1.0f, gainStep = 0.0f;
    int rampLeft = 0;
};

/**
    Everything a voice of any engine shares: note / glide, velocity, amp + mod envelopes, LFO.
    Engine voices derive from this and implement renderVoice().
*/
class VoiceBase
{
public:
    static constexpr int controlRate = 32; ///< samples between modulation updates

    void prepareBase (float sampleRate) noexcept
    {
        sr = sampleRate;
        ampEnv.prepare (sr);
        modEnv.prepare (sr);
        lfo.prepare (sr);
    }

    /**
        0.6: how the next start() begins. glideFrom: legacyGlide = glide from this voice's own pitch when the
        voice is taken over (as before 0.6), noGlide = jump, >= 0 = glide from this pitch (semitones).
        retrigger = false keeps the envelopes running (mono legato).
    */
    static constexpr float legacyGlide = -2.0f, noGlide = -1.0f;
    void setStartOptions (float glideFromPitch, bool retrigger) noexcept
    {
        glideFrom = glideFromPitch;
        retriggerNext = retrigger;
    }

    void startBase (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
    {
        note = midiNote;
        bool glide = false;
        float from = currentNote;
        if (glideFrom <= legacyGlide) glide = legato && currentNote >= 0.0f;
        else if (glideFrom >= 0.0f) { glide = true; from = glideFrom; }
        glide = glide && c.glideSeconds > 0.0f;
        currentNote = glide ? from : (float) midiNote;
        // constant rate: the glide time is per octave of distance (at least 1/24 of it)
        glideTime = c.glideConstantRate ? c.glideSeconds * std::clamp (std::abs ((float) midiNote - from) / 12.0f, 1.0f / 24.0f, 8.0f)
                                        : c.glideSeconds;
        const bool retrigger = retriggerNext || ! ampEnv.isActive() || ampEnv.isReleasing();
        glideFrom = legacyGlide;
        retriggerNext = true;
        if (! retrigger) return; // mono legato: same envelopes, LFO, expression and pitch envelope
        velocity = vel;
        ampEnv.setParams (c.amp);
        modEnv.setParams (c.mod);
        ampEnv.noteOn();
        modEnv.noteOn();
        lfo.reset();
        expr = exprIn = exprGlide = NoteExpression {};
        exprFresh = true;
        exprVibPhase = 0.0f;
        pitchEnvPhase = 0.0f;
        ampEnv.resetGain();
        if (c.modProgram != nullptr) vmod.start (midiNote, vel, *c.modProgram); // 0.7: modulation matrix (LFO 2 / 3, env 3, random)
    }

    void releaseBase() noexcept { ampEnv.noteOff(); modEnv.noteOff(); vmod.release(); }
    void killBase() noexcept
    {
        ampEnv.reset();
        modEnv.reset();
        vmod.kill();
        currentNote = -1.0f;
        pitchEnvPhase = 1.0f;
    }
    bool isActive() const noexcept { return ampEnv.isActive(); }
    int getNote() const noexcept { return note; }
    /** The pitch the voice is gliding at now (semitones, -1 when it has never sounded). */
    float glidePitch() const noexcept { return currentNote; }
    /** Per-note expression; a note's first value applies at once, later ones glide (0.8: exprSmooth / exprSlide). */
    void setExpression (const NoteExpression& x) noexcept
    {
        exprIn = x;
        if (exprFresh) exprGlide = x;
        applyVoiceMod();
    }

protected:
    /** Per control block: updates glide, LFO and envelope settings; returns the pitch in semitones. */
    float controlTick (const VoiceCommon& c, int n) noexcept
    {
        ampEnv.setParams (c.amp);
        modEnv.setParams (c.mod);
        const float glideT = c.glideConstantRate ? glideTime : c.glideSeconds;
        if (c.glideSeconds > 0.0f && glideT > 0.0f)
        {
            const float coeff = std::exp (-(float) n / (glideT * sr));
            currentNote = (float) note + (currentNote - (float) note) * coeff;
        }
        else
            currentNote = (float) note;
        if (! exprFresh) glideExpression (c, n); // 0.8: smooth per-note bend / pressure / timbre
        exprFresh = false;
        lfo.setRate (c.lfoRate);
        lfo.setShape (c.lfoShape);
        lfoDepth = std::clamp (c.lfoDepth + expr.lfo, 0.0f, 1.0f);
        const float lfoRaw = lfo.advance (n);
        lfoValue = lfoRaw * lfoDepth;
        float pitch = currentNote + c.pitchOffset + expr.pitch;
        if (c.modProgram != nullptr && (c.modProgram->numRoutes > 0 || modActive)) // 0.7: per-voice routings of the modulation matrix
        {
            const auto& o = vmod.tick (*c.modProgram, n, sr, ampEnv.getLevel(), modEnv.getLevel(), lfoRaw, exprIn.pressure, exprIn.slide,
                                       exprIn.noteBend);
            modActive = c.modProgram->numRoutes > 0;
            ampEnv.rampGain (modActive ? o.level : 1.0f, n);
            pitch += modActive ? o.pitch : 0.0f;
            applyVoiceMod();
        }
        if (pitchEnvPhase < 1.0f) // 0.6 pitch envelope: starts `pitchEnvAmount` away, arrives after `pitchEnvTime`
        {
            if (c.pitchEnvAmount != 0.0f)
                pitch += c.pitchEnvAmount * std::pow (1.0f - pitchEnvPhase, 1.0f + 3.0f * std::clamp (c.pitchEnvCurve, 0.0f, 1.0f));
            pitchEnvPhase = std::min (1.0f, pitchEnvPhase + (float) n / (std::max (0.001f, c.pitchEnvTime) * sr));
        }
        if (c.lfoDest == LfoDest::Pitch) pitch += lfoValue * 2.0f; // +-2 semitones at full depth
        if (expr.vibrato > 0.0f) // 0.8: per-note vibrato (MPE / aftertouch target), 5.5 Hz
        {
            exprVibPhase += dsp::twoPi * 5.5f * (float) n / sr;
            if (exprVibPhase > dsp::twoPi) exprVibPhase -= dsp::twoPi;
            pitch += std::min (1.0f, expr.vibrato) * 0.5f * std::sin (exprVibPhase);
        }
        return pitch;
    }

    /** expr = the note's expression + the matrix's per-voice pan / timbre. */
    void applyVoiceMod() noexcept
    {
        expr = exprGlide;
        if (! modActive) return;
        const auto& o = vmod.output();
        expr.timbre += o.timbre;
        if (o.pan != 0.0f) expr.pan = std::clamp (expr.pan + o.pan, -1.0f, 1.0f);
    }

    float velocityGain (const VoiceCommon& c) const noexcept { return 1.0f - c.velocitySens + c.velocitySens * velocity; }

    /** Tremolo factor 1-depth .. 1 when the LFO targets the amplitude, else 1. */
    float lfoAmp (const VoiceCommon& c) const noexcept
    {
        return (c.lfoDest == LfoDest::Amp ? 1.0f - 0.5f * (lfoValue + lfoDepth) : 1.0f) * expr.amp;
    }

    /** Pan gains (equal power, centre when the LFO does not target the pan). */
    void lfoPan (const VoiceCommon& c, float& l, float& r) const noexcept
    {
        const float pan = (c.lfoDest == LfoDest::Pan ? lfoValue : 0.0f) + expr.pan;
        if (c.lfoDest == LfoDest::Pan || expr.pan != 0.0f) dsp::panGains (std::clamp (pan, -1.0f, 1.0f), l, r);
        else l = r = 0.70710678f;
    }

    float sr = 48000.0f;
    int note = -1;
    float velocity = 1.0f;
    float currentNote = -1.0f;
    float glideTime = 0.0f;           ///< 0.6: glide time of the current note (constant rate)
    float glideFrom = legacyGlide;    ///< 0.6: see setStartOptions()
    bool retriggerNext = true;
    float pitchEnvPhase = 1.0f;       ///< 0.6: 0 at note start .. 1 = arrived
    float lfoValue = 0.0f;
    float lfoDepth = 0.0f;
    NoteExpression expr;      ///< per-note expression (MPE) plus the matrix's per-voice pan / timbre (what engines read)
    NoteExpression exprIn;    ///< 0.7: the note's own expression (MPE input; 0.8: the value the glide moves to)
    NoteExpression exprGlide; ///< 0.8: the note's expression as it glides (exprSmooth / exprSlide)
    bool exprFresh = true;    ///< 0.8: the voice just started: the first expression applies at once
    float exprVibPhase = 0.0f;
    mod::VoiceMod vmod;       ///< 0.7: per-voice modulation matrix state
    bool modActive = false;
    ModulatedAdsr ampEnv;
    dsp::Adsr modEnv;

    /** 0.8: one control block of expression glide: pitch over exprSlide (at least 2 ms), the rest over exprSmooth. */
    void glideExpression (const VoiceCommon& c, int n) noexcept
    {
        auto& e = exprGlide;
        const auto& tg = exprIn;
        const float aPitch = 1.0f - std::exp (-(float) n / (std::max (0.002f, c.exprSlide) * sr));
        e.pitch += (tg.pitch - e.pitch) * aPitch;
        if (std::abs (tg.pitch - e.pitch) < 1.0e-5f) e.pitch = tg.pitch;
        const float pitchNow = e.pitch;
        if (c.exprSmooth <= 0.0f) { e = tg; e.pitch = pitchNow; applyVoiceMod(); return; }
        const float a = 1.0f - std::exp (-(float) n / (c.exprSmooth * sr));
        auto g = [a] (float& v, float t) noexcept { v += (t - v) * a; if (std::abs (t - v) < 1.0e-6f) v = t; };
        g (e.timbre, tg.timbre); g (e.amp, tg.amp); g (e.lfo, tg.lfo); g (e.pan, tg.pan);
        g (e.cutoff, tg.cutoff); g (e.position, tg.position); g (e.index, tg.index);
        g (e.density, tg.density); g (e.drive, tg.drive); g (e.vibrato, tg.vibrato);
        g (e.formant, tg.formant); g (e.blend, tg.blend);
        e.pressure = tg.pressure; e.slide = tg.slide; e.noteBend = tg.noteBend; // raw matrix sources: never glided
        applyVoiceMod();
    }
    dsp::Lfo lfo;
};

/**
    Polyphonic engine base: voice allocation, note handling and block splitting. An engine
    derives from PolyEngine<MyVoice> and implements info(), getParamDescriptors() and
    setParam(); MyVoice derives from VoiceBase and provides:
        void prepare (float sr);
        void start (int note, float vel, bool legato, const VoiceCommon&);   // calls startBase
        void release();                                                       // calls releaseBase
        void kill();
        void render (float* l, float* r, int n, const VoiceCommon&);          // adds
*/
template <typename Voice>
class PolyEngine : public ISynthEngine
{
public:
    void prepare (double sampleRate, int) override
    {
        for (auto& v : voices) v.prepare ((float) sampleRate);
        reset();
    }

    void reset() noexcept override
    {
        for (auto& v : voices) v.kill();
        alloc.resetAll();
        heldCount = 0;
        lastPitch = -1.0f;
    }

    void setCommon (const VoiceCommon& c) noexcept override { common = c; }
    void setPolyphony (int n) noexcept override
    {
        const int old = alloc.getPolyphony();
        alloc.setPolyphony (n);
        for (int i = alloc.getPolyphony(); i < old; ++i)
        {
            voices[(size_t) i].kill();
            alloc.voiceFinished (i);
        }
    }

    void noteOn (int note, float velocity) noexcept override
    {
        const bool keyHeld = heldCount > 0 && ! (heldCount == 1 && held[0].note == note);
        pushHeld (note, velocity);
        bool stolen = false;
        const bool polyGlide = common.polyGlide && common.glideSeconds > 0.0f && alloc.getPolyphony() > 1;
        const int slot = polyGlide ? alloc.allocateNearest (note, stolen, [this] (int i) { return voices[(size_t) i].glidePitch(); })
                                   : alloc.allocate (note, stolen);
        auto& v = voices[(size_t) slot];
        float from = VoiceBase::legacyGlide;
        bool retrigger = true;
        switch (common.glideMode)
        {
            case GlideLegato:
                if (! keyHeld) from = VoiceBase::noGlide;
                retrigger = ! (keyHeld && alloc.getPolyphony() == 1);
                break;
            case GlideAlways:
                if (! (stolen && v.glidePitch() >= 0.0f)) from = lastPitch >= 0.0f ? lastPitch : VoiceBase::noGlide;
                break;
            default: break;
        }
        v.setStartOptions (from, retrigger);
        v.start (note, velocity, stolen, common);
        lastPitch = (float) note;
    }

    void noteOff (int note) noexcept override
    {
        const bool wasTop = heldCount > 0 && held[(size_t) heldCount - 1].note == note;
        removeHeld (note);
        // 0.6 mono: releasing the newest key returns to the previous key still held (legato, gliding back)
        if (alloc.getPolyphony() == 1 && wasTop && heldCount > 0 && voices[0].isActive() && voices[0].getNote() == note)
        {
            const auto prev = held[(size_t) heldCount - 1];
            bool stolen = false;
            const int slot = alloc.allocate (prev.note, stolen);
            voices[(size_t) slot].setStartOptions (VoiceBase::legacyGlide, false);
            voices[(size_t) slot].start (prev.note, prev.velocity, true, common);
            lastPitch = (float) prev.note;
            return;
        }
        alloc.release (note, [this] (int slot) { voices[(size_t) slot].release(); });
    }

    void allNotesOff (bool hard) noexcept override
    {
        for (int i = 0; i < ISynthEngine::maxVoices; ++i)
        {
            if (hard) { voices[(size_t) i].kill(); alloc.voiceFinished (i); }
            else if (alloc.slot (i).held) voices[(size_t) i].release();
        }
        if (! hard)
            for (int n = 0; n < 128; ++n) alloc.release (n, [] (int) {});
        heldCount = 0;
    }

    void render (float* l, float* r, int numSamples) noexcept override
    {
        for (int i = 0; i < ISynthEngine::maxVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.isActive()) continue;
            v.render (l, r, numSamples, common);
            if (! v.isActive()) alloc.voiceFinished (i);
        }
    }

    /** Glide pitch of voice slot `i` (semitones; tests and displays), -1 when it has not sounded. */
    float voiceGlidePitch (int i) const noexcept { return voices[(size_t) std::clamp (i, 0, ISynthEngine::maxVoices - 1)].glidePitch(); }
    /** Note of voice slot `i` when it is active, else -1. */
    int voiceNote (int i) const noexcept
    {
        const auto& v = voices[(size_t) std::clamp (i, 0, ISynthEngine::maxVoices - 1)];
        return v.isActive() ? v.getNote() : -1;
    }

    int activeVoiceCount() const noexcept override
    {
        int n = 0;
        for (const auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

    void noteExpression (int note, const NoteExpression& x) noexcept override
    {
        for (auto& v : voices)
            if (v.isActive() && (note < 0 || v.getNote() == note)) v.setExpression (x);
    }

protected:
    /** 0.6: keys held down, oldest first (mono note priority: the newest). */
    struct HeldKey { int note = -1; float velocity = 1.0f; };
    void pushHeld (int note, float velocity) noexcept
    {
        removeHeld (note);
        if (heldCount == (int) held.size())
        {
            for (int i = 1; i < heldCount; ++i) held[(size_t) i - 1] = held[(size_t) i];
            --heldCount;
        }
        held[(size_t) heldCount++] = { note, velocity };
    }
    void removeHeld (int note) noexcept
    {
        int w = 0;
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i].note != note) held[(size_t) w++] = held[(size_t) i];
        heldCount = w;
    }

    std::array<HeldKey, 16> held {};
    int heldCount = 0;
    float lastPitch = -1.0f; ///< 0.6: the last note started (glide mode Always)
    std::array<Voice, (size_t) ISynthEngine::maxVoices> voices;
    VoiceAllocator<ISynthEngine::maxVoices> alloc;
    VoiceCommon common;
};
} // namespace ohpe
