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
#include "morph/MorphSpace.h"
#include "motion/MotionRecorder.h"
#include "voice/VoiceControl.h"
#include "perform/PerformancePads.h"
#include "sample/SampleData.h"
#include "harmony/HarmonyBrain.h"
#include "harmony/KeyScale.h"
#include "seq/Arpeggiator.h"
#include "seq/EuclidSequencer.h"
#include "seq/Metronome.h"
#include "seq/OrbitSequencer.h"
#include "seq/ScaleChord.h"
#include "seq/StepSequencer.h"
#include "tape/TapeMachine.h"

#include <array>
#include "tapemem/TapeMemory.h"

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
    /** 0.8: output latency in samples (after prepare), reported to the host. */
    int latencySamples() const noexcept { return mix.latencySamples() + gravityLatency.load (std::memory_order_relaxed); }
    /** p66: the part of the latency that comes from Key Gravity (the largest running stage; every part aligns to it). */
    int gravityLatencySamples() const noexcept { return gravityLatency.load (std::memory_order_relaxed); }
    /** p66: the chord Key Gravity pulls to now (absolute pitch-class mask) - the held chord, else the last one heard. */
    uint16_t gravityChordMask() const noexcept { return gravityChordPub.load (std::memory_order_relaxed); }
    drum::DrumKit& drumKit() noexcept { return drums; }   ///< mixer source "drum", MIDI channel 10
    TapeMachine& tape() noexcept { return tapeMachine; }             ///< 0.2: eight-track tape
    /** p39: the metronome (own synthesised click, "metro.*"); the core schedules its clicks. */
    /** p67: the tape memory ring buffer (audio thread writes, any other thread snapshots). */
    tapemem::RingBuffer& tapeMemory() noexcept { return tapeMem; }
    const tapemem::RingBuffer& tapeMemory() const noexcept { return tapeMem; }
    Metronome& metronome() noexcept { return metro; }
    const Metronome& metronome() const noexcept { return metro; }
    /** p39 (set-up thread): the standalone app clicks whenever the metronome is on, a plug-in only with
        "metro.plugin" on. Default: standalone. */
    void setStandaloneHost (bool standalone) noexcept { standaloneHost.store (standalone, std::memory_order_relaxed); }
    bool isStandaloneHost() const noexcept { return standaloneHost.load (std::memory_order_relaxed); }
    /** p39, any thread: a count-in for a tape recording is running (the tape and a starting sequencer wait for its end). */
    bool countInActive() const noexcept { return countInBeatsForUi.load (std::memory_order_relaxed) > 0; }
    /** p39, any thread: beats left in the running count-in (8 .. 1 for two bars, 0 = none). */
    int countInBeatsLeft() const noexcept { return countInBeatsForUi.load (std::memory_order_relaxed); }
    /** p39, any thread: counts every click (the UI flashes the metronome key) and whether the last was a downbeat. */
    uint32_t clickCount() const noexcept { return clicksForUi.load (std::memory_order_relaxed); }
    bool lastClickWasDownbeat() const noexcept { return lastClickDown.load (std::memory_order_relaxed); }
    /** p39, any thread: the sample (since prepare) at which the last count-in let the tape run; -1 = none yet. */
    int64_t lastCountInRelease() const noexcept { return countInReleasedAt.load (std::memory_order_relaxed); }
    /** p39, any thread: samples processed since prepare (tests, timing). */
    int64_t samplesProcessed() const noexcept { return sampleCounterForUi.load (std::memory_order_relaxed); }
    InsertSlot& insertFx (int part) noexcept { return parts[(size_t) part]->fxRack().slot (0); } ///< 0.2: FX slot of a part (0.7: rack slot 1)
    FxRack& fxRack (int part) noexcept { return parts[(size_t) part]->fxRack(); } ///< 0.7: FX rack of a part
    FxRack& masterFxRack() noexcept { return masterRack; }                      ///< 0.7: master FX rack ("mfx1." ...)
    /** p16: the morph space (XY pad per part, four corner sounds; corners and plans on the message thread). */
    MorphSpace& morph() noexcept { return morphSpace; }
    const MorphSpace& morph() const noexcept { return morphSpace; }
    /** p22: the motion recorder (knob movements per part as looping lanes; lanes are atomics, any thread). */
    MotionRecorder& motion() noexcept { return motionRec; }
    /** p22, tests: the values the last block played with (after locks, motion lanes; the matrix may have moved some). */
    float blockValue (int registryIndex) const noexcept { return snapshot[registryIndex]; }
    const MotionRecorder& motion() const noexcept { return motionRec; }
    /** p68: Voice Control - the live input analysed into matrix sources (UI values are atomics, any thread). */
    const voice::VoiceControl& voiceControl() const noexcept { return voiceCtl; }
    /** p25: the performance pads on the main mix (states and meters for the display are atomics, any thread). */
    const PerformancePads& pads() const noexcept { return perfPads; }
    /** p24: the harmony brain (scale and chord memory of all parts; published state is atomics, any thread). */
    HarmonyBrain& harmony() noexcept { return harmonyBrain; }
    const HarmonyBrain& harmony() const noexcept { return harmonyBrain; }
    /**
        p45: the key everything plays in (any thread): "key.root" / "key.scale", Scale Lock ("key.lock", "key.lock.mode");
        fromHost when a host key is set (setHostKey). For in-key layouts (pads, grids) and the display.
    */
    keys::KeyScale keyScale() const noexcept;
    /** p45: a key given by the host (root 0 .. 11, scale; root -1 = none). The plug-in formats pass no key today:
        nothing calls this but tests; the key control shows HOST and is read-only while it is set. Any thread. */
    void setHostKey (int root, int scale) noexcept
    {
        hostKey.store (root < 0 ? -1 : (root % 12) * 16 + std::clamp (scale, 0, scales::numScales - 1), std::memory_order_relaxed);
    }
    /** p45: the notes sounding now (after scale / chord / follow, i.e. including notes the harmony makes), all four parts
        (part -1) or one part; the drum kit is not included. Any thread (published once per block). */
    chords::NoteSet soundingNotes (int part = -1) const noexcept;
    /** p16, message thread (timer): rebuilds morph plans when a part's own sound changed under an empty corner. */
    bool updateMorph() { return morphSpace.update(); }
    /**
        p28, message thread (timer): Grain "Source = Tape". Hands the Grain engines of all parts (and their morph
        lanes) the tape's edit track ("tape.edit.track") as their source, or the mix of all tracks while the edit
        track is empty; normalised like a bounce, at most bounce::maxSeconds. Rebuilt when the tape content or the
        edit track changes, only while a Grain engine uses Source = Tape. True when it published a new region.
    */
    bool updateTapeGrain();
    /** p28: the region "Source = Tape" granulates (nullptr = none / the tape is empty). */
    const SampleData* tapeGrainRegion() const noexcept { return tapeGrain.get(); }

    /**
        p28, any thread: the last pitch bend (-1..+1) / mod wheel (CC 1, 0..1) that came in from host / device MIDI on
        the edit part's channel, and a counter that moves with every such message - the on-screen wheels follow it.
    */
    float midiWheelBend() const noexcept { return wheelMidiBend.load (std::memory_order_relaxed); }
    /** p50 (UI, any thread): what the part's MPE input has received from an MPE HD controller (MpeInput::Status). */
    const MpeInput& mpeInput (int part) const noexcept { return mpeIn[(size_t) std::clamp (part, 0, numParts - 1)]; }
    float midiWheelMod() const noexcept { return wheelMidiMod.load (std::memory_order_relaxed); }
    uint32_t midiWheelBendCount() const noexcept { return wheelMidiBendSeq.load (std::memory_order_acquire); }
    uint32_t midiWheelModCount() const noexcept { return wheelMidiModSeq.load (std::memory_order_acquire); }
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
    /** p86, any thread: the last process() call had an audio input (a device / host input bus delivered channels,
        silent or not). False = no input is connected - the Voice view tells "no input chosen" from "no signal". */
    bool inputConnected() const noexcept { return inputLive.load (std::memory_order_relaxed); }
    /** p86: input blocks up to this length are always taken, even beyond the prepared block size (a device that
        delivers longer blocks than announced, e.g. separate input and output devices). */
    static constexpr int maxInputBlock = 8192;

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
    /** p24, any thread: a message of the on-screen grid keyboard for one part (0 .. 3), applied at the next block.
        An MPE part gets it as it is (member channels 2 .. 16, per-note bend / pressure / slide); any other part gets
        the notes on its own channel (through scale / chord / follow and the arpeggiator), expression is dropped. */
    void queueGridEvent (int part, const MidiEvent& e) noexcept;
    /** p46, one producer thread (the grid controller's MIDI input): a message of an external pad controller for one
        part (0 .. 3) or the drum kit (drumPart), applied at the next block. An MPE part gets it as it is; any other part
        gets it on its own channel (notes through scale / chord / follow, poly aftertouch / pressure / bend / CC as
        expression of that part); the drum kit gets notes on the drum channel. */
    void queueControllerEvent (int part, const MidiEvent& e) noexcept;

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
    /** p63: blocks the output guard silenced (a NaN / Inf reached an output; the DSP state was reset). Any thread. */
    uint64_t nonFiniteBlocks() const noexcept { return guardedBlocks.load (std::memory_order_relaxed); }
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
        static constexpr const char* keyLock = "key.lock";          ///< p45: Scale Lock on / off
        static constexpr const char* keyLockMode = "key.lock.mode"; ///< p45: Nearest / White Keys (keys::LockMode)
        // 0.3 (package E): remote controls (MIDI-mappable UI functions, see docs/en/PRESETS.md)
        static constexpr const char* libOpen = "lib.open";
        static constexpr const char* libPrev = "lib.prev";
        static constexpr const char* libNext = "lib.next";
        // p18: preset breeding (remote controls: open / close the breeding view, breed again, hear the next child)
        static constexpr const char* libBreed = "lib.breed";
        static constexpr const char* libBreedGo = "lib.breed.go";
        static constexpr const char* libBreedNext = "lib.breed.next";
        // p23: bounce to engine (tape / live input -> Grain or Sampler of the edit part); not part of presets
        static constexpr const char* bounceSrc = "bounce.src";   ///< Edit Track, Tape 1-8, Tape Mix, Input
        static constexpr const char* bounceTo = "bounce.to";     ///< Grain, Sampler
        static constexpr const char* bounceLoop = "bounce.loop"; ///< endless loop (seamless loop) or one-shot
        static constexpr const char* bounceLen = "bounce.len";   ///< input length: 1, 2, 4, 8 bars
        static constexpr const char* bounceGo = "bounce.go";     ///< trigger: bounce now
        static constexpr const char* editPart = "ui.part";
        static constexpr const char* tapeEditTrack = "tape.edit.track";
        static constexpr const char* tapeLift = "tape.edit.lift";
        static constexpr const char* tapeCut = "tape.edit.cut";
        static constexpr const char* tapeDrop = "tape.edit.drop";
        static constexpr const char* tapeSplice = "tape.edit.splice";
        static constexpr const char* tapeUndo = "tape.edit.undo";
        static constexpr const char* tapeRedo = "tape.edit.redo";
        static constexpr const char* tapeRtz = "tape.edit.rtz";
        // p38: clear the edit track (one undo step) / the whole tape (asks on screen first; undoable)
        static constexpr const char* tapeClearTrack = "tape.edit.clear";
        static constexpr const char* tapeClearAll = "tape.edit.clearall";
        // p42: the tape loop by hand - set loop in / out at the playhead (snapped to "tape.loopsnap"), half / double
        // the loop, undo / redo of the loop range (triggers), and the snap grid (Off, Beat, Bar)
        static constexpr const char* tapeLoopSetIn = "tape.edit.loopin";
        static constexpr const char* tapeLoopSetOut = "tape.edit.loopout";
        static constexpr const char* tapeLoopUndo = "tape.edit.loopundo";
        static constexpr const char* tapeLoopRedo = "tape.edit.loopredo";
        static constexpr const char* tapeLoopHalf = "tape.edit.loophalf";
        static constexpr const char* tapeLoopDouble = "tape.edit.loopdouble";
        static constexpr const char* tapeLoopSnap = "tape.loopsnap";
        // 0.4 (package F): a sequencer can play the drum kit instead of its target part
        static constexpr const char* stepDrums = "seq.step.drums";
        static constexpr const char* euclidDrums = "seq.euclid.drums";
        static constexpr const char* orbitDrums = "seq.orbit.drums";
        // tape transport (p11): momentary transport keys for MIDI controllers and the "Play starts the tape"
        // setting; not part of presets (see preset::isExcludedFromProject)
        static constexpr const char* tapeTxPlay = "tape.tx.play";   ///< trigger: play / stop (the PLAY key)
        static constexpr const char* tapeTxTapePlay = "tape.tx.tplay"; ///< trigger: tape play / stop (tape page)
        static constexpr const char* tapeTxStop = "tape.tx.stop";   ///< trigger: stop; when stopped: to the start
        static constexpr const char* tapeTxRec = "tape.tx.rec";     ///< trigger: one-touch record / punch out
        static constexpr const char* tapeTxLoop = "tape.tx.loop";   ///< trigger: loop on / off
        static constexpr const char* tapeTxArm = "tape.tx.arm";     ///< trigger: arm / disarm the edit track
        static constexpr const char* tapeTxLink = "tape.tx.link";   ///< toggle: PLAY runs sequencer and tape together
        // 1.0: 3D spatialiser - master monitoring (headphones = binaural, speakers) and the mono check of the main output
        static constexpr const char* spaceOut = "space.out";
        static constexpr const char* spaceMonoCheck = "space.monocheck";
        // p14: the performance wheels left of the on-screen keyboard (MIDI-learnable host parameters). A change
        // is sent as pitch bend / mod wheel (CC 1) on the edit part's MIDI channel, like the keyboard's notes;
        // not part of presets (see preset::isExcludedFromProject)
        static constexpr const char* perfBend = "perf.bend"; ///< -1..+1, springs back to 0 on the screen
        static constexpr const char* perfMod = "perf.mod";   ///< 0..1
        // p16: the morph space - per part "pN.morph.on / x / y / glide / xsrc / ysrc" (MorphSpace::registerParams)
        // p22: the motion recorder - per part "pN.motion.on / rec / len / amt / clear" (MotionRecorder::registerParams)
        // p25: the performance pads - "pads.stutter / stop / sweep / freeze" and their settings (PerformancePads::registerParams)
        // p24: the harmony brain - "harmony.memory / hold / take / clear / kb.layout", per part "harmony.pN.follow"
        static constexpr const char* harmonyTake = "harmony.take";   ///< trigger (message thread: key.root / key.scale)
        static constexpr const char* harmonyClear = "harmony.clear"; ///< trigger (the brain forgets)
        // p67: tape memory - the last seconds of the output, CAPTURE turns them into a Sampler instrument
        static constexpr const char* tapememLen = "tapemem.len";   ///< 5 / 10 / 20 / 30 s
        static constexpr const char* tapememSrc = "tapemem.src";   ///< Main Out, Part 1-4
        static constexpr const char* tapememMode = "tapemem.mode"; ///< Chromatic, Slices
        static constexpr const char* tapememTo = "tapemem.to";     ///< Edit Part, Part 1-4
        static constexpr const char* tapememGo = "tapemem.go";     ///< trigger: capture now (message thread)
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
        int idxPressAmt = -1, idxTimbreAmt = -1; ///< 0.8: target amounts
        int idxMpeHd = -1, idxMpeOnset = -1, idxMpePCurve = -1; ///< p50: Fingerboard mode / MPE HD
        int channel = 1;
        bool mpe = false;
    };

    void bindSequencer (SeqBinding& b, ISequencer& s, const std::string& prefix, const std::vector<ParamDescriptor>& descriptors);
    void pullSequencerParams (SeqBinding& b) noexcept;
    Transport makeTransport (const Transport& host, const MidiEvent* midi, int numMidi, int numSamples) noexcept;
    void routeLive (const MidiEvent& e) noexcept;
    void followWheels (const MidiEvent& e) noexcept; ///< p28
    void routeGenerated (const MidiEvent& e, bool shape, bool toDrums = false) noexcept;
    void addRouted (const MidiEvent& e, int part) noexcept;
    void routeToPart (const MidiEvent& e, int part, bool mapWhiteKeys = true) noexcept; ///< p24: one part, as live MIDI on its channel
    void refreshShapers (int part) noexcept;                  ///< p24: scale / chord / follow of a part's shapers
    void dispatch (const RoutedEvent& e) noexcept;
    void pullPhase2Params() noexcept;
    void renderTapeChunk (SourceBuffers& src, int pos, int len) noexcept; ///< part FX slots, tape -> sources
    void updateCountIn (const Transport& host, int numSamples) noexcept;  ///< p39: start / cancel / release point
    void scheduleClicks (const Transport& tr, int numSamples) noexcept;   ///< p39: count-in and metronome clicks
    void clickAt (int offset, bool downbeat) noexcept;                     ///< p39

    ParamRegistry registry;
    ParamSnapshot snapshot;
    std::array<std::unique_ptr<Part>, numParts> parts;
    MorphSpace morphSpace; ///< p16 (after the parts: its lanes follow them)
    RtShared<SampleData> tapeGrain;  ///< p28: Grain "Source = Tape"
    uint64_t tapeGrainRevision = 0;  ///< p28: tape content the region was built from (0 = none)
    int tapeGrainTrack = -2;         ///< p28: track it was built from (-1 = mix)
    MotionRecorder motionRec; ///< p22
    voice::VoiceControl voiceCtl; ///< p68
    PerformancePads perfPads; ///< p25 (the mixer runs it in the master chain)
    HarmonyBrain harmonyBrain; ///< p24
    Mixer mix;
    drum::DrumKit drums;
    KeyGravity drumAlign;                      ///< p66: delays the drum kit like the parts while Key Gravity runs
    std::atomic<int> gravityLatency { 0 };     ///< p66
    std::atomic<uint16_t> gravityChordPub { 0 }; ///< p66
    void updateGravity (int root, int scale) noexcept; ///< p66: targets and alignment of every part (block start)
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
    int spaceOutIndex = -1, monoCheckIndex = -1, lastMonitor = -1; ///< 1.0
    tapemem::RingBuffer tapeMem; ///< p67
    int tapememSrcIndex = -1;    ///< p67
    int perfBendIndex = -1, perfModIndex = -1, editPartIndex = -1; ///< p14
    float perfBendSent = 0.0f, perfModSent = 0.0f;
    std::atomic<float> wheelMidiBend { 0.0f }, wheelMidiMod { 0.0f };          ///< p28
    std::atomic<uint32_t> wheelMidiBendSeq { 0 }, wheelMidiModSeq { 0 };       ///< p28
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
    struct GridEvent { int part; MidiEvent event; };
    SpscQueue<GridEvent, 512> gridEvents; ///< p24
    /** p46: the grid controller's pads - on the heap: tests keep cores on the stack (Windows: 1 MB main stack). */
    std::unique_ptr<SpscQueue<GridEvent, 512>> controllerEvents = std::make_unique<SpscQueue<GridEvent, 512>>();
    int blockRoot = 0, blockScale = 1;    ///< p24: the global key of this block
    bool blockLock = false;               ///< p45: Scale Lock in this block
    int blockLockMode = 0;
    int keyLockIndex = -1, keyLockModeIndex = -1;
    std::atomic<int> hostKey { -1 };      ///< p45: root * 16 + scale, -1 = none
    // p45: sounding notes per part (note-on count per note; the audio thread) and their published sets
    std::array<std::array<uint8_t, 128>, numParts> soundCount {};
    std::array<std::array<std::atomic<uint64_t>, 2>, numParts> pubSounding {};
    void publishSounding() noexcept;

    // ---- 0.2: tape, FX slots, audio input ----
    TapeMachine tapeMachine;
    FxRack masterRack { FxRack::numMasterSlots }; ///< 0.7 (part racks: Part::fxRack)
    ScopeTap scopeTap;                            ///< 0.7: display scope
    std::array<std::vector<float>, 2 * TapeMachine::numTracks> tapeOut;
    const float* inputL = nullptr;
    const float* inputR = nullptr;
    std::vector<float> safeInputL, safeInputR; ///< p63: the input with NaN / Inf removed (sized in prepare())
    std::atomic<bool> inputLive { false };     ///< p86: see inputConnected()
    std::atomic<uint64_t> guardedBlocks { 0 }; ///< p63: see nonFiniteBlocks()
    void sanitiseInput (int numSamples) noexcept;
    void guardOutputs (const OutputBuffers& outputs, int numSamples) noexcept;
    const std::array<float*, 2 * TapeMachine::numTracks>* tapeCapture = nullptr;

    std::atomic<int> lastNoteOn { -1 }, voicesActive { 0 };
    std::atomic<double> ppqForUi { 0.0 }, bpmForUi { 120.0 };
    std::atomic<bool> playingForUi { false };

    // ---- p39: metronome and count-in ----
    Metronome metro;
    int tapePlayIndex = -1, tapeRecIndex = -1;
    std::array<int, TapeMachine::numTracks> tapeArmIndex {};
    bool tapePlayPrev = false;
    struct CountIn
    {
        bool active = false, holdSeq = false;
        int64_t startSample = 0, endSample = 0; ///< sample times (since prepare)
        double beat0 = 0.0, endBeat = 0.0, samplesPerBeat = 24000.0; ///< beats on the grid it counts on (bar = 4)
    } countIn;
    int releaseOffset = -1; ///< this block: where the count-in ends (-1 = not in this block)
    std::atomic<bool> standaloneHost { true }, lastClickDown { false };
    std::atomic<int> countInBeatsForUi { 0 };
    std::atomic<uint32_t> clicksForUi { 0 };
    std::atomic<int64_t> countInReleasedAt { -1 }, sampleCounterForUi { 0 };
};

/** Builds the complete, ordered parameter list (append-only, see docs/ARCHITECTURE.md). */
void buildParameterLayout (ParamRegistry& reg, const std::array<std::unique_ptr<Part>, Core::numParts>& parts, const Mixer& mixer,
                           const StepSequencer& stepSeq);
} // namespace ohpe
