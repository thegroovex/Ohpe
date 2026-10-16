// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace ohpe
{
/**
    Wait-free single-producer / single-consumer ring of stereo frames (0.7, scope data from the
    audio thread to the UI). Fixed capacity (power of two), allocated once in the constructor:
    push() / pop() never allocate or lock. push() keeps what fits and drops the rest (the reader is behind);
    pop() reads up to `max` frames in order.
*/
template <size_t CapacityFrames>
class StereoFifo
{
    static_assert (CapacityFrames >= 2 && (CapacityFrames & (CapacityFrames - 1)) == 0, "capacity must be a power of two");

public:
    static constexpr size_t capacity = CapacityFrames;

    StereoFifo() : bufL (new float[CapacityFrames]()), bufR (new float[CapacityFrames]()) {}

    /** Producer: appends up to n frames; returns the number written. */
    size_t push (const float* left, const float* right, size_t n) noexcept
    {
        const uint64_t w = writePos.load (std::memory_order_relaxed);
        const uint64_t r = readPos.load (std::memory_order_acquire);
        const size_t room = CapacityFrames - (size_t) (w - r);
        const size_t count = std::min (n, room);
        for (size_t i = 0; i < count; ++i)
        {
            const size_t k = (size_t) ((w + i) & (CapacityFrames - 1));
            bufL[k] = left[i];
            bufR[k] = right[i];
        }
        writePos.store (w + count, std::memory_order_release);
        if (count < n) dropped.fetch_add (n - count, std::memory_order_relaxed);
        return count;
    }

    /** Consumer: reads up to max frames; returns the number read. */
    size_t pop (float* left, float* right, size_t max) noexcept
    {
        const uint64_t r = readPos.load (std::memory_order_relaxed);
        const uint64_t w = writePos.load (std::memory_order_acquire);
        const size_t count = std::min (max, (size_t) (w - r));
        for (size_t i = 0; i < count; ++i)
        {
            const size_t k = (size_t) ((r + i) & (CapacityFrames - 1));
            left[i] = bufL[k];
            right[i] = bufR[k];
        }
        readPos.store (r + count, std::memory_order_release);
        return count;
    }

    /** Producer: frames written so far (its own write position). */
    uint64_t writtenFrames() const noexcept { return writePos.load (std::memory_order_relaxed); }

    /** Consumer: skips forward to the absolute position `pos` (no-op when already there or beyond). */
    void skipTo (uint64_t pos) noexcept
    {
        const uint64_t r = readPos.load (std::memory_order_relaxed);
        const uint64_t w = writePos.load (std::memory_order_acquire);
        const uint64_t target = std::min (pos, w);
        if (target > r) readPos.store (target, std::memory_order_release);
    }

    /** Consumer: drops everything written so far. */
    void clear() noexcept { readPos.store (writePos.load (std::memory_order_acquire), std::memory_order_release); }

    /** Frames waiting to be read (approximate from any other thread). */
    size_t available() const noexcept
    {
        return (size_t) (writePos.load (std::memory_order_acquire) - readPos.load (std::memory_order_acquire));
    }

    uint64_t droppedFrames() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    std::unique_ptr<float[]> bufL, bufR;
    std::atomic<uint64_t> writePos { 0 }, readPos { 0 }, dropped { 0 };
};

/**
    The scope tap of the core (0.7): the UI chooses one source, the audio thread copies that
    signal into the FIFO - a part after its FX rack, or the main output after the master chain.

    A source change is noticed by the producer: it marks the write position where the new source
    starts (position and source in one atomic word). The reader skips everything before that mark,
    so frames of the previous source never reach the display and no frame of the new one is lost.
*/
class ScopeTap
{
public:
    enum Source : int { None = -1, Part1 = 0, Part2, Part3, Part4, Master = 100 };

    /** UI thread. */
    void setSource (int s) noexcept { source.store (s, std::memory_order_release); }
    int getSource() const noexcept { return source.load (std::memory_order_acquire); }

    /** Audio thread: offers a block of `src`; copied when it is the selected source. */
    void offer (int src, const float* left, const float* right, int n) noexcept
    {
        const int selected = source.load (std::memory_order_acquire);
        if (selected != producerSource)
        {
            producerSource = selected;
            startMark.store (pack (fifo.writtenFrames(), selected), std::memory_order_release);
        }
        if (left == nullptr || right == nullptr || n <= 0 || src != selected) return;
        fifo.push (left, right, (size_t) n);
    }

    /** UI thread: reads frames of the selected source (frames of a previous source are skipped). */
    size_t read (float* left, float* right, size_t max) noexcept
    {
        const int selected = source.load (std::memory_order_acquire);
        const uint64_t mark = startMark.load (std::memory_order_acquire);
        if (sourceOf (mark) == selected) fifo.skipTo (positionOf (mark)); // the producer has switched: from its mark on
        else fifo.clear();                                               // not yet: all that is there is the old source
        return fifo.pop (left, right, max);
    }

    StereoFifo<8192>& buffer() noexcept { return fifo; }

private:
    static uint64_t pack (uint64_t pos, int src) noexcept { return (pos << 8) | (uint64_t) (uint8_t) (src + 1); }
    static uint64_t positionOf (uint64_t mark) noexcept { return mark >> 8; }
    static int sourceOf (uint64_t mark) noexcept { return (int) (mark & 0xffu) - 1; }

    std::atomic<int> source { None };
    std::atomic<uint64_t> startMark { pack (0, None) };
    int producerSource = None; ///< audio thread only
    StereoFifo<8192> fifo;
};
} // namespace ohpe
