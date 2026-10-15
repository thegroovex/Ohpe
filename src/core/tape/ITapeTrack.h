// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// The eight-track tape machine interface (long length, varispeed, reverse, loop points,
// lift / drop / cut / splice, unlimited undo, saturation and wow / flutter). The
// implementation is TapeMachine (src/core/tape/TapeMachine.h). Tape tracks feed mixer
// sources source::tape1 .. tape1 + 7.

#include "../model/ParamDescriptor.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ohpe
{
/** Tape transport shared by all eight tracks. */
struct TapeTransportState
{
    double positionSeconds = 0.0;
    float speed = 1.0f;      ///< varispeed, negative = reverse
    bool playing = false;
    bool recording = false;
    double loopStart = 0.0, loopEnd = 0.0;
    bool loopOn = false;
};

/** Record sources of a track ("tape.trN.src", append-only list). */
namespace tapesource
{
constexpr int input = 0;   ///< the audio input (plug-in input bus)
constexpr int part1 = 1;   ///< parts 1..4 -> 1..4
constexpr int drum = 5;
constexpr int numSources = 6;
} // namespace tapesource

/** What the tape can record from in one chunk (null = silent). Indexed by tapesource. */
struct TapeInputs
{
    std::array<const float*, tapesource::numSources> left {};
    std::array<const float*, tapesource::numSources> right {};
};

/**
    The tape machine interface.
    Real-time rules as everywhere: process() never allocates; editing operations (lift, drop,
    cut, splice, undo) run on the message thread and hand the audio thread new block lists
    without blocking it (see TapeMachine for the protocol).
*/
class ITapeMachine
{
public:
    static constexpr int numTracks = 8;
    virtual ~ITapeMachine() = default;

    virtual std::vector<ParamDescriptor> getParamDescriptors() const = 0;
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void setParam (int localIndex, float plainValue) noexcept = 0;

    /** inputs: what can be recorded; trackOutputs: 2 * numTracks pointers (L1, R1, L2, R2 ...). */
    virtual void process (const TapeInputs& inputs, float* const* trackOutputs, int numSamples) noexcept = 0;

    virtual TapeTransportState transport() const noexcept = 0;

    // ---- editing (message thread); times in seconds ----
    virtual bool lift (int track, double start, double end) = 0;
    virtual bool drop (int track, double at) = 0;
    virtual bool cut (int track, double start, double end) = 0;
    virtual bool splice (int track, double at) = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;
};
} // namespace ohpe
