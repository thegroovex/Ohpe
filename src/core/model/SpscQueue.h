// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace ohpe
{
/** Wait-free single-producer / single-consumer ring buffer (capacity - 1 usable slots). */
template <typename T, size_t Capacity>
class SpscQueue
{
    static_assert (Capacity >= 2, "capacity too small");

public:
    bool push (const T& item) noexcept
    {
        const auto w = writePos.load (std::memory_order_relaxed);
        const auto next = (w + 1) % Capacity;
        if (next == readPos.load (std::memory_order_acquire))
            return false;
        buffer[w] = item;
        writePos.store (next, std::memory_order_release);
        return true;
    }

    bool pop (T& out) noexcept
    {
        const auto r = readPos.load (std::memory_order_relaxed);
        if (r == writePos.load (std::memory_order_acquire))
            return false;
        out = buffer[r];
        readPos.store ((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept { return readPos.load (std::memory_order_acquire) == writePos.load (std::memory_order_acquire); }

private:
    std::array<T, Capacity> buffer {};
    std::atomic<size_t> writePos { 0 }, readPos { 0 };
};
} // namespace ohpe
