// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamDescriptor.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ohpe
{
struct EffectInfo
{
    const char* id;     ///< stable ID used in parameter IDs, e.g. "delay" -> "fxa.delay.time"
    const char* nameEn;
    const char* nameDe;
    const char* pictogram;
};

/**
    p19: what a part's keyboard is doing, for effects that follow it (the resonator body). Owned and written by the
    part on the audio thread (before it renders); effects read it in process(). Notes are MIDI note numbers.
*/
struct KeyState
{
    static constexpr int maxHeld = 8;
    int lastNote = -1;                   ///< newest note started (-1: none yet)
    int numHeld = 0;                     ///< keys down or sustained (ascending in held[], at most maxHeld)
    std::array<int, maxHeld> held {};
    float bendSemitones = 0.0f;          ///< part tuning + pitch bend x range + the per-note bend of the newest note
    float pressure = 0.0f;               ///< 0..1 pressure of the newest note (MPE / poly / channel pressure)
    float slide = 0.0f;                  ///< -1..+1 slide (CC 74 / MPE timbre) of the newest note
    float velocity = 0.0f;               ///< velocity of the newest note
    uint32_t noteCounter = 0;            ///< counts note starts
};

/**
    Interface of every effect (send effects, master effects, per-engine insert slot).

    Threading like ISynthEngine: getParamDescriptors() and prepare() on the message thread (may
    allocate, e.g. delay lines sized for the sample rate); setParam(), reset() and process()
    on the audio thread, real-time safe. process() works in place on a stereo buffer.
    Parameter descriptors use local IDs; the owner prefixes and registers them (append-only).
*/
class IEffect
{
public:
    virtual ~IEffect() = default;
    virtual EffectInfo info() const noexcept = 0;
    virtual std::vector<ParamDescriptor> getParamDescriptors() const = 0;
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() noexcept = 0;
    virtual void setParam (int localIndex, float plainValue) noexcept = 0;
    virtual void process (float* left, float* right, int numSamples) noexcept = 0;
    /** Tail in seconds after the input goes silent (for hosts / bounce). */
    virtual double tailSeconds() const noexcept { return 0.0; }
    /** 0.8, audio thread: the current tempo (synced effects). Optional. */
    virtual void setTempo (double bpm) noexcept { (void) bpm; }
    /** 0.8, audio thread: a note started on the part this insert belongs to (LFO retrigger). Optional. */
    virtual void noteOn() noexcept {}
    /** 0.7: local index of the parameter the modulation targets "FX K Param 1-4" move (j = 0..3): the effect's most
        expressive continuous parameters. Default: the first four. */
    virtual int modulationParam (int j) const noexcept { return j; }
    /** 1.0, audio thread: song position (quarter notes) at the start of the next block and whether the transport
        plays (tempo-synced motion that repeats on playback). Optional. */
    virtual void setTransportPosition (double ppq, bool playing) noexcept { (void) ppq; (void) playing; }
    /** 1.0, audio thread: master monitoring mode for spatial effects (0 = headphones / binaural, 1 = speakers). Optional. */
    virtual void setMonitorMode (int mode) noexcept { (void) mode; }
    /** p19, audio thread: the keyboard state of the part the effect belongs to (nullptr: none, e.g. the master rack). The
        pointer stays valid while set. Optional. */
    virtual void setKeys (const KeyState* k) noexcept { (void) k; }
};
} // namespace ohpe
