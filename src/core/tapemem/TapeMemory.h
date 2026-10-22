// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../bounce/Bounce.h"
#include "../sample/SampleData.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/**
    p67: Tape Memory (Tape-Gedächtnis). Ohpe always keeps the last seconds of its own output (the main output, or one
    part after its FX rack) in a preallocated ring buffer. One button (CAPTURE / MERKEN, "tapemem.go") turns that
    memory into a playable instrument in the Sampler of a part:

      * Chromatic: the strongest sound of the memory (one note between two onsets, or the whole memory when it has
        no onsets), its pitch detected (YIN) and corrected to the nearest semitone; the zone's root is that note, so
        the sound plays in tune across the keyboard.
      * Slices: the memory (silence trimmed, normalised) in the Sampler's slice mode - one slice per onset, from the
        first slice key (C2, the drum pads) upwards.

    The ring buffer stores 16-bit frames (headroom +6 dB): 30 s stereo at 48 kHz are 5.8 MB. push() runs on the audio
    thread (no allocation, no locks); snapshot(), the analysis and build() allocate and run on a worker / the message
    thread. Captures are saved in the state section "tapememory" (format "ohpe-tapememory", formats::tapeMemory):

        {"format":"ohpe-tapememory","version":1,
         "parts":{"p1":{"mode":0,"root":62,"name":"Capture 1","rate":48000,"channels":2,"frames":48000,
                        "pcm16":"<base64: little-endian int16 (full scale 32767), interleaved>"}}}

    Pure functions without JUCE; the plug-in (src/plugin/TapeMemoryManager) adds the worker thread and the WAV export.
*/
namespace ohpe
{
class Core;
class ParamRegistry;

namespace tapemem
{
/** "tapemem.src": what the memory records. */
enum Source
{
    SourceMain = 0, ///< the main output (before the metronome click)
    SourcePart1,    ///< Part 1 .. 4 after its FX rack = SourcePart1 + n
    numSources = SourcePart1 + 4
};

/** "tapemem.mode": how a capture plays. */
enum Mode
{
    ModeChromatic = 0,
    ModeSlices,
    numModes
};

/** "tapemem.len": how much of the memory a capture takes. */
enum Length
{
    Len5s = 0, Len10s, Len20s, Len30s, numLengths
};

/** "tapemem.to": the part that receives a capture. */
enum Target
{
    TargetEditPart = 0, ///< the edit part ("ui.part")
    TargetPart1,        ///< Part 1 .. 4 = TargetPart1 + n
    numTargets = TargetPart1 + 4
};

constexpr double maxSeconds = 30.0;        ///< the ring buffer's length
constexpr double guardSeconds = 0.5;       ///< extra frames so a snapshot is not overwritten while it is copied
constexpr double maxChromaticSeconds = 8.0; ///< a Chromatic sound is at most this long
constexpr float pcmScale = 16383.0f;       ///< 16-bit storage: full scale = +-2.0 (+6 dB headroom)
constexpr int sliceBaseKey = 36;           ///< first slice key (C2)
constexpr float sliceSensitivity = 0.5f;

/** Registers "tapemem.len", "tapemem.src", "tapemem.mode", "tapemem.to", "tapemem.go" (appended after "harmony.kb.fixed"). */
void registerParams (ParamRegistry& reg);
double lengthSeconds (int length) noexcept;

/** Preallocated stereo ring buffer of the most recent output. */
class RingBuffer
{
public:
    /** Message thread, not while push() can run (Core::prepare). Allocates (seconds + guard) at `rate`. */
    void prepare (double rate, double seconds = maxSeconds);

    /** Audio thread: appends n frames (null right = left; both null = nothing). No allocation, no locks. */
    void push (const float* l, const float* r, int n) noexcept
    {
        const int64_t cap = (int64_t) bufL.size();
        if (cap == 0 || n <= 0 || (l == nullptr && r == nullptr)) return;
        if (clearRequest.exchange (false, std::memory_order_acq_rel)) filled.store (0, std::memory_order_relaxed);
        if (l == nullptr) l = r;
        if (r == nullptr) r = l;
        int64_t w = writePos.load (std::memory_order_relaxed);
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = std::isfinite (l[i]) ? l[i] : 0.0f, b = std::isfinite (r[i]) ? r[i] : 0.0f;
            pk = std::max (pk, std::max (std::abs (a), std::abs (b)));
            const auto at = (size_t) (w % cap);
            bufL[at] = toPcm (a);
            bufR[at] = toPcm (b);
            ++w;
        }
        writePos.store (w, std::memory_order_release);
        filled.store (std::min<int64_t> (cap, filled.load (std::memory_order_relaxed) + n), std::memory_order_release);
        peakHold.store (std::max (pk, peakHold.load (std::memory_order_relaxed) * 0.97f), std::memory_order_relaxed);
    }

    /** Any thread: forget the memory (the next push() starts over). */
    void clear() noexcept { clearRequest.store (true, std::memory_order_release); }

    /** Frames that can be captured (the guard frames not counted). */
    int64_t available() const noexcept;
    int64_t capacityFrames() const noexcept { return (int64_t) bufL.size(); }
    double sampleRate() const noexcept { return rate; }
    size_t memoryBytes() const noexcept { return (bufL.capacity() + bufR.capacity()) * sizeof (int16_t); }
    /** Total frames pushed since prepare() (wraps never in practice). */
    int64_t framesWritten() const noexcept { return writePos.load (std::memory_order_acquire); }
    /** A slowly falling peak of the recent input, for the display. */
    float level() const noexcept { return peakHold.load (std::memory_order_relaxed); }

    /** Any thread but the audio thread: a copy of the last `seconds` (at most what is available). */
    bounce::Audio snapshot (double seconds) const;

    static int16_t toPcm (float x) noexcept
    {
        const float v = std::clamp (x, -2.0f, 2.0f) * pcmScale;
        return (int16_t) (v >= 0.0f ? v + 0.5f : v - 0.5f);
    }
    static float fromPcm (int16_t v) noexcept { return (float) v / pcmScale; }

private:
    std::vector<int16_t> bufL, bufR;
    double rate = 48000.0;
    int64_t guard = 0;
    std::atomic<int64_t> writePos { 0 }, filled { 0 };
    std::atomic<bool> clearRequest { false };
    std::atomic<float> peakHold { 0.0f };
};

/**
    Pitch of a monophonic stretch (YIN with parabolic interpolation, 50 Hz .. 2 kHz). Returns Hz, 0 = no pitch;
    `confidence` 0..1 (1 - the best normalised difference).
*/
float detectPitch (const float* x, int64_t n, double rate, float* confidence = nullptr);

/** The pitch of the material (median of several windows after the first onset); 0 = unpitched. */
float detectMaterialPitch (const bounce::Audio& a, const std::vector<int64_t>& onsets, float* confidence = nullptr);

/** Onset frames of the material (the Sampler's own slicing: sample::detectTransients, sensitivity sliceSensitivity). */
std::vector<int64_t> detectOnsets (const bounce::Audio& a);

/** [start, end) of the strongest note: the stretch between two onsets with the most energy (at most maxChromaticSeconds). */
std::pair<int64_t, int64_t> strongestSegment (const bounce::Audio& a, const std::vector<int64_t>& onsets);

/** Resamples by `step` (output frame i reads input frame i * step; step < 1 = lower and longer). */
bounce::Audio resample (const bounce::Audio& a, double step);

/** A finished capture: the sample and how it plays. */
struct Capture
{
    SamplePtr sample;   ///< null = nothing usable (silence)
    int mode = ModeChromatic;
    int root = 60;      ///< Chromatic: the zone's root key
    int slices = 0;     ///< Slices: the number of slices
    float pitchHz = 0.0f; ///< detected pitch before the correction (0 = unpitched)
    float cents = 0.0f;   ///< the correction applied (cents, negative = lowered)
    std::string name;
};

/** Trims, analyses and builds a capture from raw memory audio. Worker / message thread (allocates). */
Capture build (bounce::Audio raw, int mode, const std::string& name);

/**
    Puts a capture into part `part` (0-based): engine Sampler, mode Zones (Chromatic, root = capture root) or Slices
    (first slice key C2), start 0, no reverse, no loop. Message thread. False on a bad part / empty capture.
*/
bool apply (Core& core, int part, const Capture& capture, const bounce::Setter& set);

/** One stored capture per part. */
struct Stored
{
    int part = 0;
    Capture capture;
};

/** The "tapememory" section (empty captures are skipped). */
std::string saveCaptures (const std::vector<Stored>& captures);

/**
    Reads a "tapememory" section of any version (absent "version" = 0, read as 1; newer versions: the fields this
    build knows). Bad entries are skipped; never throws.
*/
std::vector<Stored> restoreCaptures (const std::string& json);
} // namespace tapemem
} // namespace ohpe
