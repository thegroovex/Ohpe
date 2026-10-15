// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Sample data model shared by the Grain, Sampler and drum engines.
//
//   * IAudioRegion   - read-only audio the engines play from: a loaded sample, or (through an
//                      adapter of the tape package) a region of a tape track.
//   * SampleData     - an immutable decoded sample (float, 1 or 2 channels, any rate). Decoding
//                      from disk happens in the plug-in layer (src/plugin/SampleLoader); the core
//                      only ever sees finished SampleData objects.
//   * RtShared<T>    - hands an immutable object from the message thread to the audio thread
//                      without locks or allocation on the audio side.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ohpe
{
/**
    Read-only audio for engines. Implementations must keep channel() pointers valid and the
    frame count stable while the region is published to an engine.
*/
class IAudioRegion
{
public:
    virtual ~IAudioRegion() = default;
    virtual int numChannels() const noexcept = 0;   ///< 1 or 2
    virtual int64_t numFrames() const noexcept = 0;
    virtual double sampleRate() const noexcept = 0;
    virtual const float* channel (int ch) const noexcept = 0; ///< ch is clamped to the available channels
};

/** An immutable decoded sample. Create it on the message thread, then publish it via RtShared. */
class SampleData final : public IAudioRegion
{
public:
    SampleData (std::vector<std::vector<float>> channelData, double rate, std::string sourcePath = {}, std::string displayName = {});

    int numChannels() const noexcept override { return (int) data.size(); }
    int64_t numFrames() const noexcept override { return frames; }
    double sampleRate() const noexcept override { return rate; }
    const float* channel (int ch) const noexcept override
    {
        return data[(size_t) std::clamp (ch, 0, (int) data.size() - 1)].data();
    }

    const std::string& path() const noexcept { return filePath; } ///< file it was loaded from (empty = built in)
    const std::string& name() const noexcept { return displayName; }
    float peak() const noexcept { return peakValue; }

    /** Sample loop points stored in the file (-1 = none). */
    int64_t fileLoopStart = -1, fileLoopEnd = -1;

    /** Mono (or stereo) helper for tests and built-in sounds. */
    static std::shared_ptr<SampleData> fromMono (std::vector<float> mono, double rate, std::string sourcePath = {});

private:
    std::vector<std::vector<float>> data;
    int64_t frames = 0;
    double rate = 48000.0;
    std::string filePath, displayName;
    float peakValue = 0.0f;
};

using SamplePtr = std::shared_ptr<const SampleData>;

/** 4-point cubic Hermite interpolation of x[i-1..i+2] at fraction t; indices are clamped to [0, n). */
inline float readHermite (const float* x, int64_t n, double pos) noexcept
{
    if (n <= 0) return 0.0f;
    const int64_t i = (int64_t) pos;
    const float t = (float) (pos - (double) i);
    auto at = [x, n] (int64_t k) { return k < 0 || k >= n ? 0.0f : x[k]; };
    const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

/**
    Publishes an immutable object to the audio thread. The message thread calls set(); the
    audio thread reads get() (a raw pointer, valid for the current audio block). Replaced
    objects stay alive for at least `graceSeconds` before they are released on a later set()
    or collect() - long enough for any audio block that may still read them. Audio-side users
    that keep a pointer across blocks must compare it with get() every block and drop it when
    it changed (the engines do).
*/
template <typename T>
class RtShared
{
public:
    static constexpr double graceSeconds = 2.0;

    RtShared() = default;
    RtShared (const RtShared&) = delete;
    RtShared& operator= (const RtShared&) = delete;

    /** Message thread. */
    void set (std::shared_ptr<const T> next)
    {
        collect();
        if (current != nullptr) retired.emplace_back (std::move (current), Clock::now());
        current = std::move (next);
        live.store (current.get(), std::memory_order_release);
    }

    /** Message thread: the published object (owner). */
    const std::shared_ptr<const T>& owner() const noexcept { return current; }

    /** Audio thread (or any thread): the published object, or nullptr. */
    const T* get() const noexcept { return live.load (std::memory_order_acquire); }

    /** Message thread: releases retired objects older than the grace time (all with force). */
    void collect (bool force = false)
    {
        const auto now = Clock::now();
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [&] (const auto& r) {
                                           return force || std::chrono::duration<double> (now - r.second).count() > graceSeconds;
                                       }),
                       retired.end());
    }

    size_t retiredCount() const noexcept { return retired.size(); }

private:
    using Clock = std::chrono::steady_clock;
    std::atomic<const T*> live { nullptr };
    std::shared_ptr<const T> current;
    std::vector<std::pair<std::shared_ptr<const T>, Clock::time_point>> retired;
};

namespace sample
{
/** Built-in sounds (message thread): a decaying harmonic tone at MIDI note `rootNote`, `seconds` long. */
SamplePtr makeHarmonicTone (double rate, float rootNote, float seconds, float decaySeconds, int numHarmonics);
} // namespace sample
} // namespace ohpe
