// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "drum/DrumKit.h"
#include "engines/Part.h"
#include "fx/FxRack.h"
#include "fx/InsertSlot.h"
#include "midi/MidiClock.h"
#include "midi/MidiMap.h"
#include "midi/MpeInput.h"
#include "mixer/Mixer.h"
#include "model/Events.h"
#include "model/ParamRegistry.h"
#include "model/ParamSnapshot.h"
#include "model/SpscQueue.h"
#include "model/StereoFifo.h"
#include "seq/Arpeggiator.h"
#include "seq/EuclidSequencer.h"
#include "seq/OrbitSequencer.h"
#include "seq/ScaleChord.h"
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
    The whole instrument without any framework: parameter registry, four parts (engines),
    sequencers (Step with patterns / song, Euclid, Arp, Orbit), scale / chord mode, mixer with
    send effects and multi-out routing, MIDI map and learn, MIDI clock in / out and MPE.

    The plug-in wrapper (src/plugin) owns one Core, forwards host parameters into params(),
    calls process() from the audio callback and saveState() / loadState() for the host.
    Tests drive it directly.
*/
class Core
{
public:
    static constexpr int numParts = 4;          ///< parts "p1.*" .. "p4.*" (p2-p4 appended in 0.2)
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
    EuclidSequencer& euclidSequencer() noexcept { return euclid; }
    Arpeggiator& arpeggiator() noexcept { return arp; }
    OrbitSequencer& orbitSequencer() noexcept { return orbit; }
    Part& part (int index) noexcept { return *parts[(size_t) index]; }
    Mixer& mixer() noexcept { return mix; }
    drum::DrumKit& drumKit() noexcept { return drums; }   ///< mixer source "drum", MIDI channel 10
    TapeMachine& tape() noexcept { return tapeMachine; }             ///< 0.2: eight-track tape
    InsertSlot& insertFx (int part) noexcept { return parts[(size_t) part]->fxRack().slot (0); } ///< 0.2: FX slot of a part (0.7: rack slot 1)
    FxRack& fxRack (int part) noexcept { return parts[(size_t) part]->fxRack(); } ///< 0.7: FX rack of a part
    FxRack& masterFxRack() noexcept { return masterRack; }                      ///< 0.7: master FX rack ("mfx1." ...)
    /** 0.7: scope tap for the display (a part after its FX rack, or the main output). */
    ScopeTap& scope() noexcept { return scopeTap; }

    /**
        0.7, message thread: prepares the effect types selected in FX rack slots that are not ready yet (slots 2-6,
        master). Call after changing a slot type from the message thread; the core calls it after prepare() and
        loadState(), the plug-in also from a timer (host automation of a slot type). True when it prepared anything.
    */
    bool prepareEffects();
    /** 0.7, any thread: a rack slot waits for prepareEffects() (its type passes dry until then). */
    bool effectsWaiting() const noexcept;

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

    /**
        Audio thread, after process(): MIDI to send out (standalone MIDI output): clock ticks,
        start / stop / continue / song position when "sync.clockout" is on, and the
        sequencers' notes on each part's MIDI channel when "sync.notesout" is on.
    */
    const EventList<2048>& midiOutput() const noexcept { return midiOut; }

    /** Sources of the transport when no host drives it ("sync.src"; append-only). */
    enum SyncSource { SyncInternal = 0, SyncMidiClock, SyncLink };

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

    /** MIDI channel that plays the drum kit (parts listen on their own channels, "pN.midich"). */
    static constexpr int drumChannel = 10;
    static constexpr int drumPart = -1;       ///< RoutedEvent::part of events for the drum kit

    /** Republishes the MIDI map after editing it on the message thread. */
    void midiMapChanged() { mapper.publish (map); }

    // ---- UI feedback (any thread) ----
    int sequencerPosition() const noexcept { return stepSeq.playPosition(); }
    int lastNote() const noexcept { return lastNoteOn.load (std::memory_order_relaxed); }
    int activeVoices() const noexcept { return voicesActive.load (std::memory_order_relaxed); }
    double transportPpq() const noexcept { return ppqForUi.load (std::memory_order_relaxed); }
    double transportBpm() const noexcept { return bpmForUi.load (std::memory_order_relaxed); }
    bool transportPlaying() const noexcept { return playingForUi.load (std::memory_order_relaxed); }

    /** Well-known global parameter IDs. */
    struct Ids
    {
        static constexpr const char* tempo = "tempo";
        static constexpr const char* seqRun = "seq.run";
        static constexpr const char* stepPrefix = "seq.step.";
        static constexpr const char* euclidPrefix = "seq.euclid.";
        static constexpr const char* arpPrefix = "seq.arp.";
        static constexpr const char* orbitPrefix = "seq.orbit.";
        static constexpr const char* syncSource = "sync.src";
        static constexpr const char* syncSmoothing = "sync.smooth";
        static constexpr const char* clockOut = "sync.clockout";
        static constexpr const char* notesOut = "sync.notesout";
        static constexpr const char* keyRoot = "key.root";
        static constexpr const char* keyScale = "key.scale";
        // 0.3 (package E): remote controls (MIDI-mappable UI functions, see docs/en/PRESETS.md)
        static constexpr const char* libOpen = "lib.open";
        static constexpr const char* libPrev = "lib.prev";
        static constexpr const char* libNext = "lib.next";
        static constexpr const char* editPart = "ui.part";
        static constexpr const char* tapeEditTrack = "tape.edit.track";
        static constexpr const char* tapeLift = "tape.edit.lift";
        static constexpr const char* tapeCut = "tape.edit.cut";
        static constexpr const char* tapeDrop = "tape.edit.drop";
        static constexpr const char* tapeSplice = "tape.edit.splice";
        static constexpr const char* tapeUndo = "tape.edit.undo";
        static constexpr const char* tapeRedo = "tape.edit.redo";
        static constexpr const char* tapeRtz = "tape.edit.rtz";
        // 0.4 (package F): a sequencer can play the drum kit instead of its target part
        static constexpr const char* stepDrums = "seq.step.drums";
        static constexpr const char* euclidDrums = "seq.euclid.drums";
        static constexpr const char* orbitDrums = "seq.orbit.drums";
    };

private:
    void applyLocks (const std::vector<ParamLockEvent>& locks, size_t& next, int upTo) noexcept;
    /** A MIDI event on its way to one part (index 0..numParts-1). */
    struct RoutedEvent
    {
        MidiEvent event;
        int part = 0;
    };
    /** Parameters of one sequencer, pushed into it when they change. */
    struct SeqBinding
    {
        ISequencer* seq = nullptr;
        std::vector<int> index;
        std::vector<float> last;
    };
    /** Per-part MIDI settings ("pN.midich", scale / chord, MPE). */
    struct PartRouting
    {
        int idxChannel = -1, idxQuant = -1, idxChord = -1, idxMpe = -1, idxMpeBend = -1, idxMpePress = -1, idxMpeTimbre = -1;
        int channel = 1;
        bool mpe = false;
    };

    void bindSequencer (SeqBinding& b, ISequencer& s, const std::string& prefix, const std::vector<ParamDescriptor>& descriptors);
    void pullSequencerParams (SeqBinding& b) noexcept;
    Transport makeTransport (const Transport& host, const MidiEvent* midi, int numMidi, int numSamples) noexcept;
    void routeLive (const MidiEvent& e) noexcept;
    void routeGenerated (const MidiEvent& e, bool shape, bool toDrums = false) noexcept;
    void addRouted (const MidiEvent& e, int part) noexcept;
    void dispatch (const RoutedEvent& e) noexcept;
    void pullPhase2Params() noexcept;
    void renderTapeChunk (SourceBuffers& src, int pos, int len) noexcept; ///< part FX slots, tape -> sources

    ParamRegistry registry;
    ParamSnapshot snapshot;
    std::array<std::unique_ptr<Part>, numParts> parts;
    Mixer mix;
    drum::DrumKit drums;
    StepSequencer stepSeq;
    EuclidSequencer euclid;
    Arpeggiator arp;
    OrbitSequencer orbit;
    MidiMap map;
    MidiMapper mapper { registry };

    int tempoIndex = -1, seqRunIndex = -1;
    std::vector<int> stepParamIndex;
    std::vector<float> stepParamLast;

    double sr = 48000.0;
    int maxBlock = 512;
    double internalPpq = 0.0;
    bool internalWasRunning = false;

    std::array<std::vector<float>, numParts> partL, partR;
    std::vector<float> drumL, drumR;
    std::map<std::string, std::string> extraSections;
    std::vector<ParamLockEvent> lockStorage;
    SequencerOutput seqOut, euclidOut, arpOut, orbitOut;

    // sequencers, routing, sync (0.2)
    SeqBinding euclidBinding, arpBinding, orbitBinding;
    std::array<PartRouting, numParts> routing {};
    std::array<MpeInput, numParts> mpeIn {};
    std::array<NoteShaper, numParts> liveShaper {}, seqShaper {};
    int stepDrumsIndex = -1, euclidDrumsIndex = -1, orbitDrumsIndex = -1; ///< 0.4: sequencers -> drum kit
    int syncSourceIndex = -1, syncSmoothIndex = -1, clockOutIndex = -1, notesOutIndex = -1, keyRootIndex = -1, keyScaleIndex = -1;
    bool arpWasOn = false;
    int arpLastPart = 0;
    bool clockOutOn = false, notesOutOn = false;
    int64_t sampleTime = 0; ///< samples since prepare (clock input time base)
    MidiClockFollower clockIn;
    MidiClockGenerator clockGen;
    std::array<RoutedEvent, 2048> routed {};
    int numRouted = 0;
    EventList<2048> midiOut;

    struct UiNote { int note; float velocity; bool on; };
    SpscQueue<UiNote, 256> uiNotes;

    // ---- 0.2: tape, FX slots, audio input ----
    TapeMachine tapeMachine;
    FxRack masterRack { FxRack::numMasterSlots }; ///< 0.7 (part racks: Part::fxRack)
    ScopeTap scopeTap;                            ///< 0.7: display scope
    std::array<std::vector<float>, 2 * TapeMachine::numTracks> tapeOut;
    const float* inputL = nullptr;
    const float* inputR = nullptr;
    const std::array<float*, 2 * TapeMachine::numTracks>* tapeCapture = nullptr;

    std::atomic<int> lastNoteOn { -1 }, voicesActive { 0 };
    std::atomic<double> ppqForUi { 0.0 }, bpmForUi { 120.0 };
    std::atomic<bool> playingForUi { false };
};

/** Builds the complete, ordered parameter list (append-only, see docs/ARCHITECTURE.md). */
void buildParameterLayout (ParamRegistry& reg, const std::array<std::unique_ptr<Part>, Core::numParts>& parts, const Mixer& mixer,
                           const StepSequencer& stepSeq);
} // namespace ohpe
