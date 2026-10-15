// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Envelope.h"
#include "../dsp/Lfo.h"
#include "ISynthEngine.h"
#include "VoiceAllocator.h"

#include <algorithm>
#include <array>

namespace ohpe
{
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

    void startBase (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
    {
        note = midiNote;
        velocity = vel;
        if (! legato || c.glideSeconds <= 0.0f || currentNote < 0.0f)
            currentNote = (float) midiNote;
        ampEnv.setParams (c.amp);
        modEnv.setParams (c.mod);
        ampEnv.noteOn();
        modEnv.noteOn();
        lfo.reset();
    }

    void releaseBase() noexcept { ampEnv.noteOff(); modEnv.noteOff(); }
    void killBase() noexcept { ampEnv.reset(); modEnv.reset(); currentNote = -1.0f; }
    bool isActive() const noexcept { return ampEnv.isActive(); }
    int getNote() const noexcept { return note; }

protected:
    /** Per control block: updates glide, LFO and envelope settings; returns the pitch in semitones. */
    float controlTick (const VoiceCommon& c, int n) noexcept
    {
        ampEnv.setParams (c.amp);
        modEnv.setParams (c.mod);
        if (c.glideSeconds > 0.0f)
        {
            const float coeff = std::exp (-(float) n / (c.glideSeconds * sr));
            currentNote = (float) note + (currentNote - (float) note) * coeff;
        }
        else
            currentNote = (float) note;
        lfo.setRate (c.lfoRate);
        lfo.setShape (c.lfoShape);
        lfoValue = lfo.advance (n) * c.lfoDepth;
        float pitch = currentNote + c.pitchOffset;
        if (c.lfoDest == LfoDest::Pitch) pitch += lfoValue * 2.0f; // +-2 semitones at full depth
        return pitch;
    }

    float velocityGain (const VoiceCommon& c) const noexcept { return 1.0f - c.velocitySens + c.velocitySens * velocity; }

    /** Tremolo factor 1-depth .. 1 when the LFO targets the amplitude, else 1. */
    float lfoAmp (const VoiceCommon& c) const noexcept
    {
        return c.lfoDest == LfoDest::Amp ? 1.0f - 0.5f * (lfoValue + c.lfoDepth) : 1.0f;
    }

    /** Pan gains (equal power, centre when the LFO does not target the pan). */
    void lfoPan (const VoiceCommon& c, float& l, float& r) const noexcept
    {
        if (c.lfoDest == LfoDest::Pan) dsp::panGains (lfoValue, l, r);
        else l = r = 0.70710678f;
    }

    float sr = 48000.0f;
    int note = -1;
    float velocity = 1.0f;
    float currentNote = -1.0f;
    float lfoValue = 0.0f;
    dsp::Adsr ampEnv, modEnv;
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
        bool stolen = false;
        const int slot = alloc.allocate (note, stolen);
        voices[(size_t) slot].start (note, velocity, stolen, common);
    }

    void noteOff (int note) noexcept override
    {
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

    int activeVoiceCount() const noexcept override
    {
        int n = 0;
        for (const auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

protected:
    std::array<Voice, (size_t) ISynthEngine::maxVoices> voices;
    VoiceAllocator<ISynthEngine::maxVoices> alloc;
    VoiceCommon common;
};
} // namespace ohpe
