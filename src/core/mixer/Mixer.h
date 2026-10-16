// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Biquad.h"
#include "../dsp/Filters.h"
#include "../fx/Compressor.h"
#include "../fx/Delay.h"
#include "../fx/Limiter.h"
#include "../fx/Reverb.h"
#include "../fx/TapeSaturator.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"

#include <array>
#include <atomic>
#include <vector>

namespace ohpe
{
class FxRack;
/**
    Mixer sources: what feeds a channel ("mix.chN.src"). The numbers are permanent
    (append-only, 0..31 reserved). Unavailable sources are silent.
*/
namespace source
{
constexpr int off = 0;
constexpr int part1 = 1;   ///< parts 1..4 -> 1..4
constexpr int drum = 5;
constexpr int tape1 = 6;   ///< tape tracks 1..8 -> 6..13
constexpr int maxSources = 32;
} // namespace source

/** Output targets of a channel ("mix.chN.out"): 0 = main (through master), 1..8 = aux stereo outputs. */
constexpr int numAuxOutputs = 8;
constexpr int numOutputPairs = 1 + numAuxOutputs;

/** Stereo buffers the core hands to the mixer, indexed by source number (nullptr = silent). */
struct SourceBuffers
{
    std::array<const float*, source::maxSources> left {};
    std::array<const float*, source::maxSources> right {};
};

/** Stereo output buffers; pair 0 = main, 1..8 = aux. A null pair is a disabled bus. */
struct OutputBuffers
{
    std::array<float*, numOutputPairs> left {};
    std::array<float*, numOutputPairs> right {};
};

/**
    8 channels + master. Per channel: source, 3-band EQ, volume, pan, mute, solo, two
    post-fader sends, output target (main or aux 1..8). Send A = delay, send B = reverb,
    master = volume + limiter. Routing to a disabled aux output falls back to the main mix so
    no sound is ever lost.
*/
class Mixer
{
public:
    static constexpr int numChannels = 8;

    void registerParams (ParamRegistry& reg) const;
    /** 0.2.0 parameters (appended at the bottom of the layout): per-channel compressor-lite,
        master compressor, master tape saturation, limiter ceiling. */
    void registerDynamicsParams (ParamRegistry& reg) const;
    /** 0.5: "fxa.delay.sync" (Off or a note value: the delay time follows the tempo), appended at the end. */
    void registerLate05Params (ParamRegistry& reg) const;
    /** 0.6: "mix.master.lowpass" (master low-pass filter, 20 kHz = off). */
    void registerLate06Params (ParamRegistry& reg) const;
    /** 0.7: the master FX rack (owner: the core), between the send returns and the master dynamics. */
    void setMasterRack (FxRack* rack) noexcept { masterRack = rack; }
    /** 0.5: note lengths of the "fxa.delay.sync" choices in quarter notes (index 0 = Off -> 0). */
    static double delaySyncBeats (int choice) noexcept;
    /** 0.5: tempo for the synced delay (audio thread, before pullParams). */
    void setTempo (double bpm) noexcept { tempo = bpm > 0.0 ? bpm : tempo; }
    void bind (const ParamRegistry& reg);
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void pullParams (const ParamSnapshot& snap) noexcept;

    /** Mixes sources into the outputs (which are overwritten). numSamples <= maxBlockSize. */
    void process (const SourceBuffers& in, const OutputBuffers& out, int numSamples) noexcept;

    /**
        Audio thread, before process(): adds a stereo signal straight to an output for the next
        process() call only (tape direct outs). target 1..8 = aux output; a disabled aux output
        (or target 0) falls back to the main mix (before master dynamics). Up to 16 per block.
    */
    void addDirect (const float* left, const float* right, int target) noexcept;

    /** Gain reduction in dB (>= 0): channel 0..7, master compressor = 8. Any thread. */
    float gainReduction (int index) const noexcept { return reductions[(size_t) index].load (std::memory_order_relaxed); }

    /** Peak meters (0..1+), channel 0..7 and master = 8. Readable from any thread. */
    float meter (int index) const noexcept { return meters[(size_t) index].load (std::memory_order_relaxed); }

    static int defaultSourceForChannel (int ch) noexcept; ///< 0-based channel -> default source

private:
    enum ChannelParam { Src = 0, Vol, Pan, Mute, Solo, EqLow, EqMid, EqHigh, SendA, SendB, Out, numChannelParams };
    enum MasterParam { MasterVol = 0, MasterLimit, ReturnA, ReturnB, numMasterParams };

    struct Channel
    {
        std::array<int, numChannelParams> idx {};
        int src = 0, out = 0;
        float gain = 1.0f, pan = 0.0f, sendA = 0.0f, sendB = 0.0f;
        bool mute = false, solo = false;
        float eqLow = 0.0f, eqMid = 0.0f, eqHigh = 0.0f;
        std::array<dsp::Biquad, 2> low, mid, high;
        dsp::Smoother gainL, gainR;
        int compIdx = -1;
        float compAmount = 0.0f;
        Compressor comp;
    };
    enum DynParam { MComp = 0, MThresh, MRatio, MAttack, MRelease, MMakeup, MTapeSat, MCeiling, numDynParams };
    struct Direct { const float* l; const float* r; int target; };

    std::array<Channel, numChannels> channels;
    std::array<int, numMasterParams> masterIdx {};
    std::vector<int> delayIdx, reverbIdx;
    std::vector<float> delayLast, reverbLast;
    int delaySyncIdx = -1;
    double tempo = 120.0;
    float masterGain = 1.0f, returnA = 0.7f, returnB = 0.7f;
    bool limiterOn = true;
    float sr = 48000.0f;

    DelayEffect delay;
    ReverbEffect reverb;
    Limiter limiter;
    std::array<int, numDynParams> dynIdx {};
    bool masterCompOn = false;
    Compressor masterComp;
    TapeSaturator masterSat;
    int lowpassIdx = -1;               ///< 0.6
    float lowpassHz = 20000.0f;
    std::array<dsp::Svf, 2> masterLp;  ///< 0.6: master low-pass (12 dB/oct.), bypassed at 20 kHz
    std::array<Direct, 16> directs {};
    int numDirects = 0;
    std::array<std::atomic<float>, numChannels + 1> reductions {};
    std::vector<float> chL, chR, sendAL, sendAR, sendBL, sendBR, masterL, masterR;
    std::array<std::atomic<float>, numChannels + 1> meters {};
    FxRack* masterRack = nullptr; ///< 0.7
};
} // namespace ohpe
