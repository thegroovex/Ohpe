// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "drum/DrumKit.h"
#include "engines/Part.h"
#include "fx/InsertSlot.h"
#include "midi/MidiMap.h"
#include "mixer/Mixer.h"
#include "model/Events.h"
#include "model/ParamRegistry.h"
#include "model/ParamSnapshot.h"
#include "model/SpscQueue.h"
#include "seq/StepSequencer.h"
#include "tape/TapeMachine.h"

#include <array>
#include <atomic>
#include <map>
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
    drum::DrumKit& drumKit() noexcept { return drums; }   ///< mixer source "drum", MIDI channel 10
    TapeMachine& tape() noexcept { return tapeMachine; }             ///< 0.2: eight-track tape
    InsertSlot& insertFx (int part) noexcept { return inserts[(size_t) part]; } ///< 0.2: FX slot of a part

    /** 0.2, audio thread: the audio input for the next process() call (plug-in input bus;
        null = silent). Valid for that one call only. */
    void setAudioInput (const float* left, const float* right) noexcept { inputL = left; inputR = right; }

    /** 0.2, offline rendering (bounce): when set, process() also copies the eight tape track
        outputs (L1, R1, L2, R2 ...) to these buffers. Message thread, not while playing live. */
    void setTapeCapture (const std::array<float*, 2 * TapeMachine::numTracks>* destination) noexcept { tapeCapture = destination; }

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

    /**
        Extra state sections owned by modules outside the core (e.g. "samples": the sample files
        the plug-in loaded). Stored with saveState(); loadState() replaces them with the sections
        of the document that the core does not own itself. Message thread.
    */
    void setStateSection (const std::string& name, std::string json) { extraSections[name] = std::move (json); }
    std::string stateSection (const std::string& name) const
    {
        const auto it = extraSections.find (name);
        return it != extraSections.end() ? it->second : std::string();
    }

    /** MIDI channel that plays the drum kit (all other channels play part 1). */
    static constexpr int drumChannel = 10;

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
    void pullPhase2Params() noexcept;
    void renderTapeChunk (SourceBuffers& src, int pos, int len) noexcept; ///< part FX slots, tape -> sources

    ParamRegistry registry;
    ParamSnapshot snapshot;
    std::array<std::unique_ptr<Part>, numParts> parts;
    Mixer mix;
    drum::DrumKit drums;
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

    std::vector<float> partL, partR, drumL, drumR;
    std::map<std::string, std::string> extraSections;
    std::vector<ParamLockEvent> lockStorage;
    SequencerOutput seqOut;
    EventList<1024> merged;

    struct UiNote { int note; float velocity; bool on; };
    SpscQueue<UiNote, 256> uiNotes;

    // ---- 0.2: tape, FX slots, audio input ----
    TapeMachine tapeMachine;
    std::array<InsertSlot, numParts> inserts;
    std::array<std::vector<float>, 2 * TapeMachine::numTracks> tapeOut;
    const float* inputL = nullptr;
    const float* inputR = nullptr;
    const std::array<float*, 2 * TapeMachine::numTracks>* tapeCapture = nullptr;

    std::atomic<int> lastNoteOn { -1 }, voicesActive { 0 };
    std::atomic<double> ppqForUi { 0.0 };
};

/** Builds the complete, ordered parameter list (append-only, see docs/ARCHITECTURE.md). */
void buildParameterLayout (ParamRegistry& reg, const std::array<std::unique_ptr<Part>, Core::numParts>& parts, const Mixer& mixer,
                           const StepSequencer& stepSeq);
} // namespace ohpe
