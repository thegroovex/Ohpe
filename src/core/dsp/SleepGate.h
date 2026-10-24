// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
 #include <xmmintrin.h>
#endif

namespace ohpe::dsp
{
/**
    p79: lets an effect stage sleep while it only processes silence (the p35 rule of the parts, for the master stage:
    the send delay, the send reverb and the master FX rack).

    The stage sleeps once its input stayed below `threshold` (-120 dBFS) and its own output too, for `holdSeconds`
    on end. The hold is longer than the longest delay line of any effect, so no echo or reverb tail is cut. While it
    sleeps the stage is skipped (its buffer keeps the silent input); the first block with a louder input wakes it, and
    it carries on with the state it stopped with (every value in it was below the threshold).
*/
class SleepGate
{
public:
    static constexpr float threshold = 1.0e-6f;
    static constexpr float holdSeconds = 4.5f;
    /** Tests only: false keeps every stage awake (the reference the sleeping output is compared with). */
    inline static bool allowed = true;

    void prepare (double sampleRate) noexcept
    {
        hold = (int) std::ceil (holdSeconds * (sampleRate > 0.0 ? sampleRate : 48000.0));
        reset();
    }

    void reset() noexcept
    {
        quiet = 0;
        asleep = false;
    }

    /** Every sample of both channels below the threshold (NaN / Inf count as loud). */
    static bool silent (const float* l, const float* r, int n) noexcept
    {
        bool s = true;
        for (int i = 0; i < n; ++i) s &= std::abs (l[i]) < threshold && std::abs (r[i]) < threshold;
        return s;
    }

    /** Before the stage: true = process this block (wakes a sleeping stage when the input is not silent). */
    bool shouldProcess (bool inputSilent) noexcept
    {
        if (asleep)
        {
            if (inputSilent && allowed) return false;
            asleep = false;
        }
        return true;
    }

    /** After a processed block: counts silent blocks (input and output) towards sleep. */
    void afterProcess (bool inputSilent, const float* l, const float* r, int n) noexcept
    {
        if (! allowed || ! inputSilent || ! silent (l, r, n))
        {
            quiet = 0;
            return;
        }
        quiet += n;
        if (quiet >= hold) asleep = true;
    }

    bool isAsleep() const noexcept { return asleep; }

private:
    int hold = 216000, quiet = 0;
    bool asleep = false;
};

/**
    p79: flush-to-zero / denormals-are-zero for the scope of one audio block (x86 SSE: FTZ + DAZ, ARM64: FZ), restored
    afterwards. Denormal numbers (below about 1e-38) are far below hearing but can cost a hundred times more on some
    processors; the plug-in host wrapper already ran with this, now the core does too (standalone, tools, tests).
*/
class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals() noexcept
    {
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
        saved = getCsr();
        setCsr (saved | 0x8040u); // FTZ (bit 15) + DAZ (bit 6)
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        unsigned long long v;
        asm volatile ("mrs %0, fpcr" : "=r"(v));
        saved = v;
        v |= (1ull << 24); // FZ
        asm volatile ("msr fpcr, %0" : : "r"(v));
#endif
    }

    ~ScopedFlushDenormals()
    {
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
        setCsr ((unsigned int) saved);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        const unsigned long long v = saved;
        asm volatile ("msr fpcr, %0" : : "r"(v));
#endif
    }

    ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

private:
    unsigned long long saved = 0;
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    static unsigned int getCsr() noexcept { return _mm_getcsr(); }
    static void setCsr (unsigned int v) noexcept { _mm_setcsr (v); }
#endif
};
} // namespace ohpe::dsp
