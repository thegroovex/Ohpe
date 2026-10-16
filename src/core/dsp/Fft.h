// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>
#include <complex>
#include <utility>
#include <vector>

namespace ohpe::dsp
{
/**
    In-place iterative radix-2 FFT (size a power of two). sign = -1: forward,
    X[k] = sum x[n] e^(-i 2 pi k n / N); sign = +1: inverse direction. No scaling.
    Message / worker thread only (wavetable building and analysis).
*/
inline void fft (std::vector<std::complex<double>>& x, int sign)
{
    const size_t n = x.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (x[i], x[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = sign * 2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = x[i + k], v = x[i + k + len / 2] * w;
                x[i + k] = u + v;
                x[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}
} // namespace ohpe::dsp
