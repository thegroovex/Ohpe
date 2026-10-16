// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamDescriptor.h"

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
};
} // namespace ohpe
