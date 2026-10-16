// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ohpe::usertable
{
/**
    Records a few seconds of the live input (mono sum) for the audio-to-wavetable analysis.
    arm() allocates on the message thread; push() runs on the audio thread (no allocation, no
    locks); the message thread polls finished() and takes the audio with take().
*/
class InputCapture
{
public:
    /** Message thread: prepares `seconds` of recording at `sampleRate` and starts it. */
    void arm (double seconds, double sampleRate)
    {
        quiesce();
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        buffer.assign ((size_t) std::max (1.0, seconds * rate), 0.0f);
        written.store (0, std::memory_order_relaxed);
        armed.store (true, std::memory_order_release);
    }
    /** Message thread: stops (keeps what was recorded). */
    void stop() noexcept { armed.store (false, std::memory_order_release); }

    /** Audio thread. Either pointer may be null (no input = silence is recorded). */
    void push (const float* l, const float* r, int n) noexcept
    {
        inPush.store (true, std::memory_order_seq_cst);
        if (! armed.load (std::memory_order_seq_cst)) { inPush.store (false, std::memory_order_release); return; }
        int64_t w = written.load (std::memory_order_relaxed);
        const int64_t cap = (int64_t) buffer.size();
        const int count = (int) std::min<int64_t> (n, cap - w);
        for (int i = 0; i < count; ++i)
        {
            float v = 0.0f;
            if (l != nullptr && r != nullptr) v = 0.5f * (l[i] + r[i]);
            else if (l != nullptr) v = l[i];
            else if (r != nullptr) v = r[i];
            buffer[(size_t) (w + i)] = v;
        }
        w += count;
        written.store (w, std::memory_order_release);
        if (w >= cap) armed.store (false, std::memory_order_release);
        inPush.store (false, std::memory_order_release);
    }

    bool recording() const noexcept { return armed.load (std::memory_order_acquire); }
    /** 0 .. 1 */
    float progress() const noexcept
    {
        return buffer.empty() ? 0.0f : (float) written.load (std::memory_order_acquire) / (float) buffer.size();
    }
    bool finished() const noexcept { return ! buffer.empty() && ! recording() && written.load (std::memory_order_acquire) > 0; }
    double sampleRate() const noexcept { return rate; }
    /** Message thread, when not recording: the recorded audio (the capture is empty afterwards). */
    std::vector<float> take()
    {
        quiesce();
        std::vector<float> out (buffer.begin(), buffer.begin() + (std::ptrdiff_t) written.load (std::memory_order_acquire));
        buffer.clear();
        written.store (0, std::memory_order_relaxed);
        return out;
    }

private:
    /** Stops recording and waits until the audio thread has left push() (it never blocks there). */
    void quiesce() noexcept
    {
        armed.store (false, std::memory_order_seq_cst);
        while (inPush.load (std::memory_order_seq_cst)) {}
    }
    std::vector<float> buffer;
    std::atomic<bool> inPush { false };
    std::atomic<int64_t> written { 0 };
    std::atomic<bool> armed { false };
    double rate = 48000.0;
};
} // namespace ohpe::usertable
