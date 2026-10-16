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
    signal into the FIFO - a part after its FX slot, or the main output after the master chain.
*/
class ScopeTap
{
public:
    enum Source : int { None = -1, Part1 = 0, Part2, Part3, Part4, Master = 100 };

    void setSource (int s) noexcept
    {
        if (source.exchange (s, std::memory_order_acq_rel) != s) changed.store (true, std::memory_order_release);
    }
    int getSource() const noexcept { return source.load (std::memory_order_acquire); }

    /** Audio thread: offers a block of `src`; copied when it is the selected source. */
    void offer (int src, const float* left, const float* right, int n) noexcept
    {
        if (left == nullptr || right == nullptr || n <= 0 || src != source.load (std::memory_order_relaxed)) return;
        fifo.push (left, right, (size_t) n);
    }

    /** UI thread: reads frames of the selected source (frames of a previous source are skipped). */
    size_t read (float* left, float* right, size_t max) noexcept
    {
        if (changed.exchange (false, std::memory_order_acq_rel)) fifo.clear();
        return fifo.pop (left, right, max);
    }

    StereoFifo<8192>& buffer() noexcept { return fifo; }

private:
    std::atomic<int> source { None };
    std::atomic<bool> changed { false };
    StereoFifo<8192> fifo;
};
} // namespace ohpe
