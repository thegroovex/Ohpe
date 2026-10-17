// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../sample/SampleData.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
    Bounce to engine (issue #15). One button moves audio from the tape (one track, the mix of all
    tracks, the loop range when the tape loop is on) or a few bars of the live input into the Grain or
    Sampler engine of a part.

    The material is prepared as a sample: normalised to -1 dBFS, and either
      * a one-shot (silence trimmed, 3 ms fades at both ends), or
      * an endless loop: the tail is cross-faded (equal power) into the head, so the last frame
        flows into the first one without a click. The sample carries a file loop over its whole
        length; the Sampler loops it forward, the Grain engine's grains wrap across the seam
        (IAudioRegion::loopsSeamlessly).

    Pure functions without JUCE; the plug-in (src/plugin/Bounce) adds the file I/O (the bounce is
    stored as a WAV in the user bounce library so the "samples" state restores it).
*/
namespace ohpe
{
class Core;
class TapeMachine;
class ParamRegistry;

namespace bounce
{
/** "bounce.src": where the audio comes from. */
enum Source
{
    SourceEditTrack = 0,                   ///< the tape page's edit track ("tape.edit.track")
    SourceTrack1,                          ///< Tape 1 .. Tape 8 = SourceTrack1 + n
    SourceMix = SourceTrack1 + 8,          ///< all tape tracks summed
    SourceInput,                           ///< the live input (records "bounce.len")
    numSources
};

/** "bounce.to": the engine that receives the bounce. */
enum Target
{
    TargetGrain = 0,
    TargetSampler,
    numTargets
};

/** "bounce.len": input recording length. */
enum Length
{
    Len1Bar = 0, Len2Bars, Len4Bars, Len8Bars, numLengths
};

constexpr double maxSeconds = 60.0;      ///< longer material is cut
constexpr double loopFadeSeconds = 0.05; ///< endless loop seam cross-fade (at most a quarter of the material)
constexpr double edgeFadeSeconds = 0.003;
constexpr float targetPeak = 0.891f;     ///< -1 dBFS
constexpr float maxGain = 15.85f;        ///< +24 dB
constexpr float silenceLevel = 0.001f;   ///< -60 dBFS

/** Stereo audio being prepared. */
struct Audio
{
    std::vector<float> l, r;
    double rate = 48000.0;
    int64_t frames() const noexcept { return (int64_t) l.size(); }
};

/** Registers "bounce.src", "bounce.to", "bounce.loop", "bounce.len", "bounce.go" (appended after "lib.breed.next"). */
void registerParams (ParamRegistry& reg);

/** English source name ("Tape 3", "Tape Mix", "Input"); for the edit track the resolved track. */
std::string sourceName (int source, int editTrack);
/** Input length in seconds at `bpm` (4/4), clamped to 0.5 .. maxSeconds. */
double lengthSeconds (int length, double bpm);

/**
    Reads tape material: `track` 0..7, or -1 for the mix of all tracks. Range [start, end) in frames;
    end <= start = the recorded content of the track(s). At most maxSeconds.
*/
Audio readTape (const TapeMachine& tape, int track, int64_t start = 0, int64_t end = -1);

float peakOf (const Audio& a) noexcept;
bool isSilent (const Audio& a) noexcept;
/** Scales to targetPeak (at most maxGain); returns the gain (1 for silence). */
float normalise (Audio& a) noexcept;
/** Removes leading / trailing frames below silenceLevel (keeps 5 ms before the first sound). */
void trimSilence (Audio& a);
/** Linear fades over `seconds` at both ends. */
void fadeEdges (Audio& a, double seconds = edgeFadeSeconds) noexcept;
/**
    Endless loop: cross-fades the last X frames (X = min (loopFadeSeconds, length / 4)) into the
    first X frames with an equal-power curve and drops them; the result loops seamlessly from its
    last frame to its first. Returns the cross-fade length.
*/
int64_t makeEndless (Audio& a);

struct Options
{
    bool endless = true;
    bool normalise = true;
};

/** Prepares the audio and builds the sample (endless: file loop over the whole length). nullptr when silent. */
SamplePtr prepare (Audio audio, const Options& options, const std::string& path = {}, const std::string& name = {});

/** Sets a host / registry parameter by ID (plain value); the plug-in keeps the host in sync. */
using Setter = std::function<void (const std::string& id, float plain)>;

/**
    Puts the sample into part `part` (0-based): switches the part's engine to the target, loads the
    sample (Sampler: one zone over the keyboard, root C4, start 0, forward loop over the whole sample
    when endless, else no loop; Grain: source Sample). Message thread. False on a bad part / sample.
*/
bool apply (Core& core, int part, Target target, SamplePtr sample, const Setter& set);

/**
    Records the live input in stereo for a bounce. arm() allocates on the message thread; push() runs
    on the audio thread (no allocation, no locks); the message thread polls finished() and take()s.
*/
class StereoCapture
{
public:
    void arm (double seconds, double sampleRate)
    {
        quiesce();
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        const auto n = (size_t) std::max (1.0, seconds * rate);
        bufL.assign (n, 0.0f);
        bufR.assign (n, 0.0f);
        written.store (0, std::memory_order_relaxed);
        armed.store (true, std::memory_order_release);
    }
    void stop() noexcept { armed.store (false, std::memory_order_release); }

    /** Audio thread. Either pointer may be null (silence is recorded for it / mono is copied). */
    void push (const float* l, const float* r, int n) noexcept
    {
        inPush.store (true, std::memory_order_seq_cst);
        if (! armed.load (std::memory_order_seq_cst)) { inPush.store (false, std::memory_order_release); return; }
        int64_t w = written.load (std::memory_order_relaxed);
        const int64_t cap = (int64_t) bufL.size();
        const int count = (int) std::min<int64_t> (n, cap - w);
        if (l == nullptr) l = r;
        if (r == nullptr) r = l;
        for (int i = 0; i < count; ++i)
        {
            bufL[(size_t) (w + i)] = l != nullptr ? l[i] : 0.0f;
            bufR[(size_t) (w + i)] = r != nullptr ? r[i] : 0.0f;
        }
        w += count;
        written.store (w, std::memory_order_release);
        if (w >= cap) armed.store (false, std::memory_order_release);
        inPush.store (false, std::memory_order_release);
    }

    bool recording() const noexcept { return armed.load (std::memory_order_acquire); }
    float progress() const noexcept
    {
        return bufL.empty() ? 0.0f : (float) written.load (std::memory_order_acquire) / (float) bufL.size();
    }
    bool finished() const noexcept { return ! bufL.empty() && ! recording() && written.load (std::memory_order_acquire) > 0; }
    /** Message thread: the recording (the capture is empty afterwards). */
    Audio take()
    {
        quiesce();
        Audio a;
        a.rate = rate;
        const auto n = (std::ptrdiff_t) written.load (std::memory_order_acquire);
        a.l.assign (bufL.begin(), bufL.begin() + n);
        a.r.assign (bufR.begin(), bufR.begin() + n);
        bufL.clear();
        bufR.clear();
        written.store (0, std::memory_order_relaxed);
        return a;
    }

private:
    void quiesce() noexcept
    {
        armed.store (false, std::memory_order_seq_cst);
        while (inPush.load (std::memory_order_seq_cst)) {}
    }
    std::vector<float> bufL, bufR;
    std::atomic<bool> inPush { false };
    std::atomic<int64_t> written { 0 };
    std::atomic<bool> armed { false };
    double rate = 48000.0;
};
} // namespace bounce
} // namespace ohpe
