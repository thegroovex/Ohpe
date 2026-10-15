// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "engines/Part.h"
#include "midi/MidiMap.h"
#include "mixer/Mixer.h"
#include "model/Events.h"
#include "model/ParamRegistry.h"
#include "model/ParamSnapshot.h"
#include "model/SpscQueue.h"
#include "seq/StepSequencer.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace ohpe
{
/**
    The whole instrument without any framework: parameter registry, parts (engines), step
    sequencer, mixer with send effects and multi-out routing, MIDI map and learn.

    The plug-in wrapper (src/plugin) owns one Core, forwards host parameters into params(),
    calls process() from the audio callback and saveState() / loadState() for the host.
    Tests drive it directly.
*/
class Core
{
public:
    static constexpr int numParts = 1;          ///< phase 1: one part (IDs "p1.*"); more parts append "p2.*" ...
    static constexpr int maxChunk = 256;        ///< internal render chunk (also the event split granularity cap)

    Core();
    Core (const Core&) = delete;
    Core& operator= (const Core&) = delete;

    ParamRegistry& params() noexcept { return registry; }
    const ParamRegistry& params() const noexcept { return registry; }
    MidiMap& midiMap() noexcept { return map; }
    const MidiMap& midiMap() const noexcept { return map; }
    MidiMapper& midiMapper() noexcept { return mapper; }
    const MidiMapper& midiMapper() const noexcept { return mapper; }
    StepSequencer& stepSequencer() noexcept { return stepSeq; }
    Part& part (int index) noexcept { return *parts[(size_t) index]; }
    Mixer& mixer() noexcept { return mix; }

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    double sampleRate() const noexcept { return sr; }

    /**
        Audio thread. Renders numSamples into the output pairs (overwritten; null pairs are
        disabled buses whose channels fall back to the main mix). `midi` must be sorted by
        sampleOffset. `host` carries the host transport when hostDriven is true.
    */
    void process (const OutputBuffers& outputs, int numSamples, const MidiEvent* midi, int numMidi, const Transport& host) noexcept;

    /** Any thread: notes from the on-screen keyboard (applied at the next block). */
    void queueNote (int note, float velocity, bool on) noexcept;

    // ---- state (message thread) ----
    std::string saveState (const std::string& uiJson = {}) const;
    bool loadState (const std::string& json, std::string* uiJsonOut = nullptr);

    /** Republishes the MIDI map after editing it on the message thread. */
    void midiMapChanged() { mapper.publish (map); }

    // ---- UI feedback (any thread) ----
    int sequencerPosition() const noexcept { return stepSeq.playPosition(); }
    int lastNote() const noexcept { return lastNoteOn.load (std::memory_order_relaxed); }
    int activeVoices() const noexcept { return voicesActive.load (std::memory_order_relaxed); }
    double transportPpq() const noexcept { return ppqForUi.load (std::memory_order_relaxed); }

    /** Well-known global parameter IDs. */
    struct Ids
    {
        static constexpr const char* tempo = "tempo";
        static constexpr const char* seqRun = "seq.run";
        static constexpr const char* stepPrefix = "seq.step.";
    };

private:
    void applyLocks (const std::vector<ParamLockEvent>& locks, size_t& next, int upTo) noexcept;
    void dispatch (const MidiEvent& e) noexcept;

    ParamRegistry registry;
    ParamSnapshot snapshot;
    std::array<std::unique_ptr<Part>, numParts> parts;
    Mixer mix;
    StepSequencer stepSeq;
    MidiMap map;
    MidiMapper mapper { registry };

    int tempoIndex = -1, seqRunIndex = -1;
    std::vector<int> stepParamIndex;
    std::vector<float> stepParamLast;

    double sr = 48000.0;
    int maxBlock = 512;
    double internalPpq = 0.0;
    bool internalWasRunning = false;

    std::vector<float> partL, partR;
    std::vector<ParamLockEvent> lockStorage;
    SequencerOutput seqOut;
    EventList<1024> merged;

    struct UiNote { int note; float velocity; bool on; };
    SpscQueue<UiNote, 256> uiNotes;

    std::atomic<int> lastNoteOn { -1 }, voicesActive { 0 };
    std::atomic<double> ppqForUi { 0.0 };
};

/** Builds the complete, ordered parameter list (append-only, see docs/ARCHITECTURE.md). */
void buildParameterLayout (ParamRegistry& reg, const std::array<std::unique_ptr<Part>, Core::numParts>& parts, const Mixer& mixer,
                           const StepSequencer& stepSeq);
} // namespace ohpe
