// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"
#include "../model/ParamDescriptor.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "DrumVoice.h"

#include <array>
#include <string>
#include <vector>

namespace ohpe::drum
{
/**
    The drum kit: 24 pads. Each pad plays a sample - the user's, else the factory kit's recorded
    sample - or, without one (or with source = Synth), its synthetic drum model (kick, snare, hat,
    clap, tom, perc). Per pad: tune / decay / level / pan / tone and a choke group.

    The core has no files: the plug-in hands the factory recordings in with setFactorySample()
    and user samples with setPadSample(). Without any samples (core tests) every pad still sounds
    through its model.

    Parameters: "drum.vol", "drum.base" (MIDI note of pad 1), then per pad
    "drum.padN.{src,type,tune,decay,level,pan,choke,tone}" (N = 1..24). Append-only.

    Notes: pad = note - base. Every pad is monophonic (a new hit chokes its previous one);
    pads with the same choke group (1..8) cut each other. Note-offs are ignored (one-shots).

    Threading as for engines: registerParams / bind / prepare / setPadSample on the message
    thread, everything else on the audio thread (real-time safe).
*/
class DrumKit
{
public:
    static constexpr int numPads = 24;
    static constexpr int numVoices = 16;
    static constexpr int numChokeGroups = 8;
    static constexpr const char* prefix = "drum.";

    enum PadParam { pSource = 0, pType, pTune, pDecay, pLevel, pPan, pChoke, pTone, numPadParams };

    DrumKit();

    /** Local descriptors (IDs without the "drum." prefix), in registration order. */
    static std::vector<ParamDescriptor> paramDescriptors();
    /** Registers the kit parameters with the "drum." prefix (append-only block). */
    static void registerParams (ParamRegistry& reg);
    /** The factory setting of one pad (the default kit). */
    static PadSettings defaultPad (int pad) noexcept;
    /** Short English name of a factory pad ("kick", "open hat" ...). */
    static const char* defaultPadName (int pad) noexcept;

    void bind (const ParamRegistry& reg);
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // ---- audio thread ----
    void pullParams (const ParamSnapshot& snap) noexcept;
    void handleMidi (const MidiEvent& e) noexcept;
    void noteOn (int note, float velocity) noexcept;
    void triggerPad (int pad, float velocity) noexcept;
    void allNotesOff (bool hard) noexcept;
    /** Renders into l/r (overwrites). */
    void render (float* l, float* r, int numSamples) noexcept;
    int activeVoiceCount() const noexcept;
    bool isPadSounding (int pad) const noexcept;

    /** Direct setters (tests, or a host without a registry); index = local descriptor index. */
    void setParam (int localIndex, float plainValue) noexcept;
    const PadSettings& pad (int index) const noexcept { return pads[(size_t) std::clamp (index, 0, numPads - 1)]; }
    int baseNote() const noexcept { return base; }

    // ---- message thread ----
    /** The user's sample for a pad (nullptr = back to the factory sample, if any). */
    void setPadSample (int pad, SamplePtr sample);
    /** The user's sample of a pad (nullptr = none). */
    SamplePtr padSample (int pad) const;
    /** The factory kit's recording for a pad (plug-in, at start-up). */
    void setFactorySample (int pad, SamplePtr sample);
    SamplePtr factorySample (int pad) const;
    /** The sample the pad plays now (user, else factory; nullptr = model). */
    SamplePtr effectiveSample (int pad) const;

private:
    std::array<PadSettings, numPads> pads {};
    void publish (int pad);

    std::array<RtShared<SampleData>, numPads> samples; ///< effective sample per pad (audio side)
    std::array<SamplePtr, numPads> userSamples, factorySamples;
    std::array<DrumVoice, numVoices> voices;
    float volume = 0.8f;
    int base = 36;
    uint64_t counter = 0;

    std::vector<int> paramIndex;
    std::vector<float> lastValue;
};
} // namespace ohpe::drum
