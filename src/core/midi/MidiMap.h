// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"
#include "../model/ParamRegistry.h"
#include "../model/SpscQueue.h"

#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace ohpe
{
enum class MidiSourceType { CC = 0, Note };
enum class MidiMapMode { Absolute = 0, Toggle, Relative };

/** One mapping: a MIDI control drives one parameter (by stable string ID). */
struct MidiMapping
{
    std::string paramId;
    MidiSourceType type = MidiSourceType::CC;
    int channel = 0;   ///< 0 = any channel, 1..16
    int number = 0;    ///< CC number or note number
    float minNorm = 0.0f, maxNorm = 1.0f; ///< normalised range the control sweeps
    MidiMapMode mode = MidiMapMode::Absolute;

    bool sameSource (const MidiMapping& o) const noexcept { return type == o.type && channel == o.channel && number == o.number; }
};

/**
    The editable MIDI map (message thread). Any number of controls may drive one parameter and
    one control may drive several parameters. Saved inside the state and as map files:
    {"format":"ohpe-midimap","version":1,"mappings":[{"param":..,"type":"cc"|"note","channel":..,"number":..,"min":..,"max":..,"mode":"absolute"|"toggle"|"relative"}]}
*/
class MidiMap
{
public:
    const std::vector<MidiMapping>& mappings() const noexcept { return list; }

    /** Adds a mapping; an existing mapping of the same source to the same parameter is replaced. */
    void add (const MidiMapping& m);
    /** Removes every mapping of this parameter. Returns the number removed. */
    int clearParam (const std::string& paramId);
    void clear() { list.clear(); }
    std::vector<MidiMapping> forParam (const std::string& paramId) const;
    bool isMapped (const std::string& paramId) const;

    std::string toJson() const;
    bool fromJson (const std::string& json);   ///< replaces the map; false on malformed input (map unchanged)

private:
    std::vector<MidiMapping> list;
};

/** A parameter value change caused by MIDI (for host notification / UI). */
struct ParamChange
{
    int paramIndex = -1;
    float normValue = 0.0f;
};

/** Result of MIDI learn, produced on the audio thread, applied on the message thread. */
struct LearnResult
{
    int paramIndex = -1;
    MidiSourceType type = MidiSourceType::CC;
    int channel = 1;
    int number = 0;
};

/**
    Real-time side of the MIDI map. publish() compiles the map into a fixed table the audio
    thread reads lock-free; handleMidi() applies mapped controls to the registry; learn mode
    captures the next control for an armed parameter.

    Threads: publish(), armLearn(), cancelLearn(), applyPendingLearns(), popChange() = message
    thread. handleMidi() = audio thread.
*/
class MidiMapper
{
public:
    static constexpr int maxEntries = 1024;

    explicit MidiMapper (ParamRegistry& registry) : reg (registry) {}

    void publish (const MidiMap& map);

    void armLearn (int paramIndex) noexcept { learnTarget.store (paramIndex); }
    void cancelLearn() noexcept { learnTarget.store (-1); }
    int learningParam() const noexcept { return learnTarget.load(); }

    /**
        Audio thread. Returns true when the event was consumed by the map or by learn (the
        caller should then not play it as a note / controller).
    */
    bool handleMidi (const MidiEvent& e) noexcept;

    /** Message thread: moves learned controls into the map (and republishes). Returns true if the map changed. */
    bool applyPendingLearns (MidiMap& map);

    /** Message thread: next parameter change caused by MIDI (to notify the host / UI). */
    bool popChange (ParamChange& out) noexcept { return changes.pop (out); }

private:
    struct Entry
    {
        int paramIndex;
        MidiSourceType type;
        int channel, number;
        float minNorm, maxNorm;
        MidiMapMode mode;
    };
    struct Table
    {
        std::array<Entry, maxEntries> entries;
        int count = 0;
    };

    ParamRegistry& reg;
    std::array<Table, 2> tables {};
    std::atomic<int> active { 0 }, reading { -1 };
    std::atomic<int> learnTarget { -1 };
    SpscQueue<LearnResult, 64> learned;
    SpscQueue<ParamChange, 1024> changes;
};
} // namespace ohpe
