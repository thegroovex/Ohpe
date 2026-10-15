// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Phase-2 work package "tape": the eight-track tape machine (long length, varispeed, reverse,
// loop points, lift / drop / cut / splice, unlimited undo, saturation and wow / flutter).
// This header fixes the interface the core and the mixer will use; the implementation lives
// in src/core/tape/. Tape tracks feed mixer sources source::tape1 .. tape1 + 7.

#include "../model/ParamDescriptor.h"

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

/**
    The tape machine interface (to be implemented in phase 2).
    Real-time rules as everywhere: process() never allocates; editing operations (lift, drop,
    cut, splice, undo) run on the message thread and hand the audio thread new block lists
    lock-free.
*/
class ITapeMachine
{
public:
    static constexpr int numTracks = 8;
    virtual ~ITapeMachine() = default;

    virtual std::vector<ParamDescriptor> getParamDescriptors() const = 0;
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void setParam (int localIndex, float plainValue) noexcept = 0;

    /** inputs: what is recorded (per armed track); outputs: per-track stereo playback (track-major). */
    virtual void process (const float* const* inputs, float* const* trackOutputs, int numSamples) noexcept = 0;

    virtual TapeTransportState transport() const noexcept = 0;

    // ---- editing (message thread) ----
    virtual bool lift (int track, double start, double end) = 0;
    virtual bool drop (int track, double at) = 0;
    virtual bool cut (int track, double start, double end) = 0;
    virtual bool splice (int track, double at) = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;
};
} // namespace ohpe
