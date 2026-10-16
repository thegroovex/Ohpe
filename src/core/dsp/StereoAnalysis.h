// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// 0.7: analysis for the display visuals (stereo scope, spectrum terrain). UI thread only; the
// classes allocate in their constructors, never while analysing.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace ohpe::dsp
{
/** Levels and phase correlation of a stereo block. */
struct StereoStats
{
    float midRms = 0.0f, sideRms = 0.0f; ///< RMS of M = (L + R) / 2 and S = (L - R) / 2
    float correlation = 0.0f;            ///< -1 (out of phase) .. 0 (unrelated) .. +1 (mono); 0 for silence
    float peak = 0.0f;
};

inline StereoStats analyseStereo (const float* l, const float* r, size_t n) noexcept
{
    StereoStats s;
    if (n == 0) return s;
    double ll = 0.0, rr = 0.0, lr = 0.0, mm = 0.0, ss = 0.0;
    float peak = 0.0f;
    for (size_t i = 0; i < n; ++i)
    {
        const double a = std::isfinite (l[i]) ? l[i] : 0.0, b = std::isfinite (r[i]) ? r[i] : 0.0;
        ll += a * a;
        rr += b * b;
        lr += a * b;
        const double m = 0.5 * (a + b), d = 0.5 * (a - b);
        mm += m * m;
        ss += d * d;
        peak = std::max (peak, (float) std::max (std::abs (a), std::abs (b)));
    }
    s.midRms = (float) std::sqrt (mm / (double) n);
    s.sideRms = (float) std::sqrt (ss / (double) n);
    const double denom = std::sqrt (ll * rr);
    s.correlation = denom > 1.0e-12 ? (float) std::clamp (lr / denom, -1.0, 1.0) : 0.0f;
    s.peak = peak;
    return s;
}

/** Radix-2 FFT magnitudes on log-spaced bands, normalised 0..1 over a fixed dB range. */
class SpectrumBands
{
public:
    explicit SpectrumBands (int fftOrder = 10, int numBands = 48, float minHz = 30.0f, float maxHz = 16000.0f)
        : size ((size_t) 1 << fftOrder), bands ((size_t) std::max (1, numBands)), loHz (minHz), hiHz (maxHz)
    {
        window.resize (size);
        for (size_t i = 0; i < size; ++i) window[i] = 0.5f - 0.5f * std::cos (6.28318530718f * (float) i / (float) (size - 1));
        work.resize (size);
        twiddle.resize (size / 2);
        for (size_t k = 0; k < size / 2; ++k) twiddle[k] = std::polar (1.0f, -6.28318530718f * (float) k / (float) size);
        out.assign (bands, 0.0f);
    }

    size_t fftSize() const noexcept { return size; }
    size_t numBands() const noexcept { return bands; }
    const std::vector<float>& values() const noexcept { return out; }

    /** Analyses the newest fftSize() samples of `mono` (fewer are zero-padded at the front);
        returns the band values 0..1 (-72 .. 0 dBFS for a full-scale sine). */
    const std::vector<float>& analyse (const float* mono, size_t n, double sampleRate) noexcept
    {
        const size_t take = std::min (n, size);
        const size_t pad = size - take;
        for (size_t i = 0; i < size; ++i)
        {
            const float x = i < pad ? 0.0f : mono[n - take + (i - pad)];
            work[i] = { (std::isfinite (x) ? x : 0.0f) * window[i], 0.0f };
        }
        fft();
        const float nyq = (float) sampleRate * 0.5f;
        const float binHz = (float) sampleRate / (float) size;
        for (size_t b = 0; b < bands; ++b)
        {
            const float f0 = loHz * std::pow (hiHz / loHz, (float) b / (float) bands);
            const float f1 = loHz * std::pow (hiHz / loHz, (float) (b + 1) / (float) bands);
            int k0 = (int) std::floor (std::min (f0, nyq) / binHz), k1 = (int) std::ceil (std::min (f1, nyq) / binHz);
            k0 = std::clamp (k0, 1, (int) size / 2 - 1);
            k1 = std::clamp (std::max (k1, k0 + 1), k0 + 1, (int) size / 2);
            float mag = 0.0f;
            for (int k = k0; k < k1; ++k) mag = std::max (mag, std::abs (work[(size_t) k]));
            // full-scale sine with a Hann window -> |X| = N / 4
            const float db = 20.0f * std::log10 (std::max (1.0e-9f, mag * 4.0f / (float) size));
            out[b] = std::clamp ((db + 72.0f) / 72.0f, 0.0f, 1.0f);
        }
        return out;
    }

private:
    void fft() noexcept
    {
        // bit reversal
        for (size_t i = 1, j = 0; i < size; ++i)
        {
            size_t bit = size >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (work[i], work[j]);
        }
        for (size_t len = 2; len <= size; len <<= 1)
        {
            const size_t step = size / len;
            for (size_t i = 0; i < size; i += len)
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const auto t = work[i + k + len / 2] * twiddle[k * step];
                    const auto u = work[i + k];
                    work[i + k] = u + t;
                    work[i + k + len / 2] = u - t;
                }
        }
    }

    size_t size, bands;
    float loHz, hiHz;
    std::vector<float> window, out;
    std::vector<std::complex<float>> work, twiddle;
};
} // namespace ohpe::dsp
