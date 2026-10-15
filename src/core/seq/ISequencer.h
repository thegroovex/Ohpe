// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"
#include "../model/ParamDescriptor.h"

#include <string>
#include <vector>

namespace ohpe
{
/** A parameter lock emitted by a sequencer step: holds `paramIndex` at `value` until cleared. */
struct ParamLockEvent
{
    int sampleOffset = 0;
    int paramIndex = -1;   ///< registry index; -1 together with clearAll = release all locks
    float value = 0.0f;    ///< plain value
    bool clearAll = false;
};

/** What a sequencer produces per block: note events (MIDI) and parameter locks. */
struct SequencerOutput
{
    EventList<512> events;
    std::vector<ParamLockEvent>* locks = nullptr; ///< preallocated by the core (reserve), never grown on the audio thread
    int maxLocks = 0;

    void clear() noexcept
    {
        events.clear();
        if (locks != nullptr) locks->clear();
    }
    bool addLock (const ParamLockEvent& l) noexcept
    {
        if (locks == nullptr || (int) locks->size() >= maxLocks) return false;
        locks->push_back (l); // capacity reserved, no allocation
        return true;
    }
};

/**
    Interface of every sequencer (Step, Euclid, Arp, Orbit ...).

    Threading: getParamDescriptors() / prepare() / pattern editing API / toJson / fromJson on
    the message thread; setParam(), process() and reset() on the audio thread (real-time
    safe). Pattern data must be shared lock-free (e.g. atomics per step) because the UI edits
    while the audio thread plays. Pattern data is not host-automatable (it is saved in the
    state); the sequencer's playback settings are parameters (local IDs, prefixed by the owner).
*/
class ISequencer
{
public:
    virtual ~ISequencer() = default;

    virtual const char* id() const noexcept = 0; ///< "step", "euclid", ... (parameter ID segment)
    virtual std::vector<ParamDescriptor> getParamDescriptors() const = 0;
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() noexcept = 0;
    virtual void setParam (int localIndex, float plainValue) noexcept = 0;

    /** Produces the events of the next numSamples samples for the given transport. */
    virtual void process (const Transport& transport, int numSamples, SequencerOutput& out) noexcept = 0;

    /** Index of the step / position playing now (UI), -1 when stopped. */
    virtual int playPosition() const noexcept = 0;

    virtual std::string toJson() const = 0;
    virtual bool fromJson (const std::string& json) = 0;
};
} // namespace ohpe
