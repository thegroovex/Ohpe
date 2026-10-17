// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"
#include "../fx/TapeSaturator.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "../model/SpscQueue.h"
#include "ITapeTrack.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ohpe
{
/** One block of tape: 4096 stereo frames. Frozen blocks belong to the undo history and are never written again. */
struct TapeBlock
{
    static constexpr int frames = 4096;
    static constexpr int shift = 12;
    std::atomic<bool> frozen { false };
    float l[frames];
    float r[frames];
};

/**
    Ohpe's eight-track tape machine.

    Storage: every track is a page table of TapeBlock pointers (one page = 4096 frames;
    nullptr = silence), sized for the tape length in prepare(). Blocks are allocated on the
    message thread only: a pool of empty blocks is kept filled (housekeeping()) and handed
    to the audio thread through a wait-free queue. Recording writes copy-on-write: a frozen
    (committed) block is copied into a fresh block from the pool before it is written, so
    every committed state stays intact.

    Undo: the history is a list of snapshots; a snapshot holds, per track, a shared list of
    shared block pointers (tracks that did not change share the list of the previous
    snapshot, blocks that did not change are shared too). Unlimited undo / redo costs one
    page list per changed track and step plus the blocks that really changed. One finished
    recording pass (committed by housekeeping() after the record switch is released) or one
    edit (lift, cut, drop, splice, clear) is one undo step.

    Threads: process() and setParam() on the audio thread (no allocation, no locks, never
    blocks). Everything else on the message thread. The message thread swaps page tables
    under a two-flag (Dekker) exclusion: it raises editPending and waits until the audio
    thread is outside process(); the audio thread never waits - while an edit is applied it
    outputs silence for the tape (a few milliseconds). Edits are refused while recording.

    Parameters ("tape." + local ID): see getParamDescriptors() and docs/TAPE.md.
*/
class TapeMachine final : public ITapeMachine
{
public:
    enum GlobalParam { Play = 0, Rec, Speed, Reverse, Loop, LoopIn, LoopOut, Overdub, Sat, Wow, Flutter, Hiss, numGlobalParams };
    enum TrackParam { Arm = 0, Src, Level, Out, numTrackParams };
    static constexpr int numParams = numGlobalParams + numTracks * numTrackParams;
    static constexpr const char* prefix = "tape.";
    static constexpr double defaultLengthSeconds = 600.0;
    static constexpr double minLengthSeconds = 10.0, maxLengthSeconds = 3600.0;
    static constexpr int poolTarget = 256;  ///< free blocks kept ready for recording (8 MB)

    TapeMachine();
    ~TapeMachine() override;
    TapeMachine (const TapeMachine&) = delete;
    TapeMachine& operator= (const TapeMachine&) = delete;

    static int trackParam (int track, TrackParam p) noexcept { return numGlobalParams + track * numTrackParams + p; }

    // ---- parameters ----
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    static std::vector<ParamDescriptor> describeParams();
    static void registerParams (ParamRegistry& reg); ///< appends "tape.*" (group "tape")
    void bind (const ParamRegistry& reg);
    void pullParams (const ParamSnapshot& snap) noexcept;
    void setParam (int localIndex, float plainValue) noexcept override;

    // ---- set-up (message thread) ----
    void prepare (double sampleRate, int maxBlockSize) override;
    /** Tape length (default 10 minutes); takes effect immediately, content beyond is kept in the history. */
    void setLengthSeconds (double seconds);
    double lengthSeconds() const noexcept { return lengthSec; }
    int64_t lengthFrames() const noexcept { return lenFrames; }
    double sampleRate() const noexcept { return sr; }

    /** Message thread, regularly (the plug-in calls it at 20 Hz): refills the block pool and
        turns a finished recording pass into an undo step. Returns true if it committed one. */
    bool housekeeping();

    // ---- audio thread ----
    void process (const TapeInputs& inputs, float* const* trackOutputs, int numSamples) noexcept override;
    /** Resets DSP state (saturation filters, smoothing); keeps the tape content and position. */
    void resetDsp() noexcept;

    // ---- transport (any thread) ----
    TapeTransportState transport() const noexcept override;
    void locate (double seconds) noexcept;   ///< applied at the next audio block
    void locateFrames (int64_t frame) noexcept;
    int64_t positionFrames() const noexcept { return posForUi.load (std::memory_order_relaxed); }
    int directOut (int track) const noexcept { return trackOut[(size_t) track]; } ///< 0 = mixer only, 1..8 = aux output
    uint32_t dropouts() const noexcept { return dropoutCount.load (std::memory_order_relaxed); }

    // ---- editing (message thread); seconds ----
    bool lift (int track, double start, double end) override;
    bool drop (int track, double at) override;
    bool cut (int track, double start, double end) override;
    bool splice (int track, double at) override;
    bool undo() override;
    bool redo() override;

    // same in frames
    bool liftFrames (int track, int64_t start, int64_t end);
    bool cutFrames (int track, int64_t start, int64_t end);
    bool dropFrames (int track, int64_t at);
    bool spliceFrames (int track, int64_t at);
    bool clearTrack (int track);
    bool clearAll();
    bool canUndo() const;
    bool canRedo() const;
    int undoSteps() const;  ///< number of steps that can be undone
    int64_t clipboardFrames() const;

    /** Forgets everything (content and history); message thread. */
    void resetContent();

    // ---- reading the committed content (message thread) ----
    /** p28, any thread: changes whenever the committed content changes (a finished pass, an edit, undo / redo, load). */
    uint64_t contentRevision() const noexcept { return revision.load (std::memory_order_acquire); }
    int64_t contentEndFrames (int track) const;  ///< one past the last non-zero frame, 0 if empty
    int64_t contentEndFrames() const;            ///< over all tracks
    void readCommitted (int track, int64_t start, int numFrames, float* l, float* r) const;

    // ---- persistence (message thread) ----
    /** Binary tape audio: only blocks that hold audio, float32. See docs/TAPE.md. */
    void saveAudio (std::vector<uint8_t>& out) const;
    /** asUndoStep (p38, tape files): the loaded content becomes one undo step (refused while recording) instead of
        replacing the history. */
    bool loadAudio (const void* data, size_t size, bool asUndoStep = false);
    std::string settingsJson() const;               ///< {"lengthSeconds": ...}
    void loadSettingsJson (const std::string& json);

    /** Makes this machine play the committed content of another one (blocks are shared, read-only). */
    void shareContentFrom (const TapeMachine& other);

private:
    using BlockPtr = std::shared_ptr<TapeBlock>;
    using PageList = std::vector<BlockPtr>;
    using PageListPtr = std::shared_ptr<const PageList>;
    using Snapshot = std::array<PageListPtr, numTracks>;

    /** Message thread holds it while touching the live page tables. */
    struct Exclusion
    {
        std::atomic<bool> pending { false }, busy { false };
        void lock() noexcept;
        void unlock() noexcept { pending.store (false, std::memory_order_seq_cst); }
        bool tryEnterAudio() noexcept;
        void exitAudio() noexcept { busy.store (false, std::memory_order_seq_cst); }
    };
    struct ExclusionGuard
    {
        explicit ExclusionGuard (Exclusion& e) : ex (e) { ex.lock(); }
        ~ExclusionGuard() { ex.unlock(); }
        Exclusion& ex;
    };

    struct Segment { int64_t dst, length; int kind; int64_t src; }; // kind 0 = old, 1 = clipboard, 2 = silence

    // message thread helpers (call with msgMutex held)
    bool commitLocked();               ///< exclusion must be held
    void installLocked (const Snapshot& s); ///< exclusion must be held
    void pushSnapshotLocked (Snapshot s);
    bool editTrack (int track, int64_t affectStart, int64_t affectEnd, const std::vector<Segment>& segs);
    bool beginEdit (std::unique_lock<std::mutex>& lock);
    void copyToClipboard (const PageList& pages, int64_t start, int64_t end);
    void refillPool();
    void resizeLiveLocked (size_t pages);
    static void readPages (const PageList& pages, int64_t start, int n, float* l, float* r) noexcept;
    int64_t toFrames (double seconds) const noexcept;
    const Snapshot& current() const { return history[(size_t) cursor]; }

    // audio helpers
    inline void fetch (int t, int64_t idx, float& l, float& r) const noexcept;
    inline void write (int t, int64_t idx, float l, float r, bool overdub) noexcept;

    // ---- shared state ----
    double sr = 48000.0, lengthSec = defaultLengthSeconds;
    int64_t lenFrames = 0;
    std::atomic<uint64_t> revision { 1 }; ///< p28: contentRevision()
    std::array<std::vector<TapeBlock*>, numTracks> live;
    Exclusion exclusion;
    SpscQueue<TapeBlock*, 2048> pool;
    std::atomic<int> poolCount { 0 };
    std::atomic<bool> dirty { false }, recordingActive { false };
    std::atomic<double> locateRequest;
    std::atomic<int64_t> posForUi { 0 };
    std::atomic<float> speedForUi { 1.0f };
    std::atomic<bool> playingForUi { false };
    std::atomic<uint32_t> dropoutCount { 0 };

    // ---- message thread ----
    mutable std::mutex msgMutex;
    std::vector<Snapshot> history;
    int cursor = 0;
    std::vector<float> clipL, clipR;

    // ---- audio thread ----
    std::array<int, numParams> paramIndex {};
    std::array<float, numParams> paramValue {};
    std::array<float, numParams> lastPulled {};
    double pos = 0.0;
    float speedCurrent = 1.0f, speedCoeff = 0.001f;
    bool wasPlaying = false;
    bool parked = false, parkedForward = true; ///< the playhead ran into an end of the tape
    bool play = false, rec = false, reverse = false, loopOn = false, overdub = true;
    float speed = 1.0f, loopIn = 0.0f, loopOut = 0.0f, sat = 0.0f, wow = 0.0f, flutter = 0.0f, hiss = 0.0f;
    std::array<bool, numTracks> armed {};
    std::array<int, numTracks> trackSrc {}, trackOut {};
    std::array<float, numTracks> levelTarget {}, levelCurrent {};
    std::array<TapeSaturator, numTracks> saturators;
    double wowPhase = 0.0, flutterPhase = 0.0;
    float flutterNoise = 0.0f;
    dsp::Random rng { 0x7a9e5u };
};
} // namespace ohpe
