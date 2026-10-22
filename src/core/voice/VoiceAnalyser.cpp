// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "VoiceAnalyser.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace ohpe::voice
{
namespace
{
constexpr double twoPi = 6.283185307179586;
// map ranges (log): F1 280..780 Hz -> y 0..1, F2 700..2400 Hz -> x 0..1
constexpr float f1Lo = 280.0f, f1Hi = 780.0f, f2Lo = 700.0f, f2Hi = 2400.0f;
// reference formants (adult average) of A E I O U
constexpr float refF1[numVowels] = { 730.0f, 530.0f, 290.0f, 520.0f, 310.0f };
constexpr float refF2[numVowels] = { 1150.0f, 1850.0f, 2250.0f, 880.0f, 800.0f };

float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
float finiteOr0 (float v) noexcept { return std::isfinite (v) ? v : 0.0f; }
float coef (double seconds, double rate) noexcept { return seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * rate))); }
} // namespace

void formantsToMap (float f1, float f2, float& x, float& y) noexcept
{
    f1 = std::max (f1, 1.0f);
    f2 = std::max (f2, 1.0f);
    x = clamp01 (std::log (f2 / f2Lo) / std::log (f2Hi / f2Lo));
    y = clamp01 (std::log (f1 / f1Lo) / std::log (f1Hi / f1Lo));
}

void vowelFormants (int v, float& f1, float& f2) noexcept
{
    v = std::clamp (v, 0, numVowels - 1);
    f1 = refF1[v];
    f2 = refF2[v];
}

void vowelCentre (int v, float& x, float& y) noexcept
{
    float f1, f2;
    vowelFormants (v, f1, f2);
    formantsToMap (f1, f2, x, y);
}

int classifyVowel (float x, float y) noexcept
{
    int best = 0;
    float bestD = 1.0e9f;
    for (int v = 0; v < numVowels; ++v)
    {
        float cx, cy;
        vowelCentre (v, cx, cy);
        const float d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
        if (d < bestD) { bestD = d; best = v; }
    }
    return best;
}

const char* vowelName (int v) noexcept
{
    static const char* const n[numVowels] = { "A", "E", "I", "O", "U" };
    return n[std::clamp (v, 0, numVowels - 1)];
}

void VoiceAnalyser::Biquad::lowpass (double rate, double f, double q) noexcept
{
    const double w = twoPi * std::min (f, rate * 0.49) / rate, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
    b0 = (float) ((1.0 - c) * 0.5 / a0);
    b1 = (float) ((1.0 - c) / a0);
    b2 = b0;
    a1 = (float) (-2.0 * c / a0);
    a2 = (float) ((1.0 - al) / a0);
    z1 = z2 = 0.0f;
}

void VoiceAnalyser::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    decim = std::max (1, (int) std::lround (sr / 11500.0));
    fsd = (float) (sr / decim);
    win = std::clamp ((int) std::lround (0.016 * fsd), 64, maxWindow);
    hop = std::max (8, (int) std::lround (0.0027 * fsd));
    latency = 1000.0f * (float) win * 0.5f / fsd;
    aa1.lowpass (sr, 0.45 * fsd, 0.5412);
    aa2.lowpass (sr, 0.45 * fsd, 1.3066);
    for (int i = 0; i < win; ++i) hann[(size_t) i] = 0.5f - 0.5f * (float) std::cos (twoPi * (i + 0.5) / win);
    featAlpha = coef (0.012, sr);
    fastAtk = coef (0.001, sr);
    fastRel = coef (0.020, sr);
    slowAtk = coef (0.060, sr);
    slowRel = coef (0.060, sr);
    onsetDecay = (float) std::exp (-1.0 / (0.060 * sr));
    reset();
}

void VoiceAnalyser::reset() noexcept
{
    aa1.z1 = aa1.z2 = aa2.z1 = aa2.z2 = 0.0f;
    ring.fill (0.0f);
    ringPos = decimPhase = sinceHop = hopCount = 0;
    preLast = hpBrightZ = hpBrightX = hpSibZ = hpSibX = 0.0f;
    eAll = eBright = eSib = zcr = lastSample = 0.0f;
    envFast = envSlow = onset = refractory = 0.0f;
    cBright = (float) std::exp (-twoPi * 2000.0 / sr);
    cSib = (float) std::exp (-twoPi * std::min (5000.0, 0.35 * sr) / sr);
    inDb = -120.0f;
    open = false;
    closedFor = 10.0f;
    target.fill (0.0f);
    out.fill (0.0f);
    f1Raw = f2Raw = voicing = 0.0f;
}

void VoiceAnalyser::process (const float* left, const float* right, int n) noexcept
{
    if (n <= 0) return;
    const float gain = std::pow (10.0f, std::clamp (settings.gainDb, -60.0f, 40.0f) / 20.0f);
    const float gateDb = std::clamp (settings.gateDb, -100.0f, 0.0f);
    const float gateLin = std::pow (10.0f, gateDb / 20.0f);
    for (int i = 0; i < n; ++i)
    {
        float x = 0.0f;
        if (left != nullptr) x = right != nullptr ? 0.5f * (finiteOr0 (left[i]) + finiteOr0 (right[i])) : finiteOr0 (left[i]);
        else if (right != nullptr) x = finiteOr0 (right[i]);
        x = std::clamp (x * gain, -4.0f, 4.0f);

        // full-rate features: energy, energy above ~2 kHz and ~5 kHz, zero crossings
        hpBrightZ = cBright * (hpBrightZ + x - hpBrightX);
        hpBrightX = x;
        hpSibZ = cSib * (hpSibZ + x - hpSibX);
        hpSibX = x;
        eAll += featAlpha * (x * x - eAll);
        eBright += featAlpha * (hpBrightZ * hpBrightZ - eBright);
        eSib += featAlpha * (hpSibZ * hpSibZ - eSib);
        const bool cross = (x >= 0.0f) != (lastSample >= 0.0f) && std::abs (x - lastSample) > 1.0e-6f;
        zcr += featAlpha * ((cross ? 1.0f : 0.0f) - zcr);
        lastSample = x;

        // onsets: a fast envelope jumping over a slow one
        const float a = std::abs (x);
        envFast += (a > envFast ? fastAtk : fastRel) * (a - envFast);
        envSlow += (a > envSlow ? slowAtk : slowRel) * (a - envSlow);
        onset *= onsetDecay;
        if (refractory > 0.0f) refractory -= 1.0f;
        else if (envFast > gateLin * 0.7f && envFast > 2.0f * envSlow + 1.0e-5f)
        {
            onset = 1.0f;
            refractory = (float) (0.09 * sr);
        }

        // decimation for the formant / pitch analysis
        const float lp = decim > 1 ? aa2.run (aa1.run (x)) : x;
        if (++decimPhase >= decim)
        {
            decimPhase = 0;
            ring[(size_t) ringPos] = lp;
            ringPos = (ringPos + 1) % maxWindow;
            if (++sinceHop >= hop)
            {
                sinceHop = 0;
                ++hopCount;
                if (open) analyseFrame();
                if (open && settings.pitch && (hopCount & 1) == 0) analysePitch();
            }
        }
    }
    // denormal guard
    if (std::abs (envSlow) < 1.0e-20f) envSlow = 0.0f;
    if (std::abs (envFast) < 1.0e-20f) envFast = 0.0f;
    if (eAll < 1.0e-24f) eAll = eBright = eSib = 0.0f;
    if (onset < 1.0e-6f) onset = 0.0f;

    inDb = eAll > 1.0e-12f ? 10.0f * std::log10 (2.0f * eAll) : -120.0f;
    if (! open && inDb > gateDb) open = true;
    else if (open && inDb < gateDb - 3.0f) open = false;

    if (open)
    {
        closedFor = 0.0f;
        target[Level] = clamp01 ((inDb - gateDb) / std::max (3.0f, -6.0f - gateDb));
        const float total = eAll + 1.0e-12f;
        target[Brightness] = clamp01 (std::sqrt (eBright / total) * 1.3f);
        const float sib = clamp01 (eSib / total * 2.5f);
        const float zk = zcr * (float) sr / 1000.0f; // zero crossings per ms
        target[Noisiness] = std::max (sib, clamp01 ((zk - 3.5f) / 6.0f));
    }
    else
    {
        closedFor += (float) (n / sr);
        target[Level] = 0.0f;
        if (closedFor > 0.4f) // a pause: the rest goes home (no input = the sound as programmed)
            for (int v : { (int) VowelX, (int) VowelY, (int) Brightness, (int) Noisiness, (int) Pitch }) target[(size_t) v] = 0.0f;
    }

    const float smoothSec = std::max (0.001f, (closedFor > 0.4f ? std::max (settings.smoothMs, 150.0f) : settings.smoothMs) * 0.001f);
    const float k = 1.0f - std::exp (-(float) n / (smoothSec * (float) sr));
    for (int v = 0; v < numValues; ++v)
    {
        if (v == Onset) continue;
        out[(size_t) v] += k * (target[(size_t) v] - out[(size_t) v]);
        if (std::abs (out[(size_t) v]) < 1.0e-7f) out[(size_t) v] = 0.0f;
        const float lo = v == VowelX || v == VowelY ? -1.0f : 0.0f;
        out[(size_t) v] = std::clamp (finiteOr0 (out[(size_t) v]), lo, 1.0f);
    }
    out[Onset] = open || onset > 0.0f ? clamp01 (onset) : 0.0f;
}

void VoiceAnalyser::analyseFrame() noexcept
{
    // windowed, pre-emphasised frame of the newest `win` decimated samples
    std::array<double, maxWindow> fr {};
    float prev = ring[(size_t) ((ringPos - win - 1 + 2 * maxWindow) % maxWindow)];
    for (int i = 0; i < win; ++i)
    {
        const float s = ring[(size_t) ((ringPos - win + i + maxWindow) % maxWindow)];
        fr[(size_t) i] = (double) ((s - 0.94f * prev) * hann[(size_t) i]);
        prev = s;
    }
    std::array<double, lpcOrder + 1> r {};
    for (int lag = 0; lag <= lpcOrder; ++lag)
    {
        double acc = 0.0;
        for (int i = lag; i < win; ++i) acc += fr[(size_t) i] * fr[(size_t) (i - lag)];
        // a light Gaussian lag window widens the peaks a little (stable roots at short windows)
        const double g = 2.0 * 3.141592653589793 * 40.0 * lag / fsd;
        r[(size_t) lag] = acc * std::exp (-0.5 * g * g);
    }
    if (! (r[0] > 1.0e-12)) return;
    r[0] *= 1.0001;

    // Levinson-Durbin
    std::array<double, lpcOrder + 1> a {}, tmp {};
    a[0] = 1.0;
    double err = r[0];
    for (int i = 1; i <= lpcOrder; ++i)
    {
        double acc = r[(size_t) i];
        for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
        const double kk = -acc / err;
        tmp = a;
        for (int j = 1; j < i; ++j) a[(size_t) j] = tmp[(size_t) j] + kk * tmp[(size_t) (i - j)];
        a[(size_t) i] = kk;
        err *= (1.0 - kk * kk);
        if (! (err > 0.0)) return;
    }

    // roots of z^p + a1 z^(p-1) + ... + ap (Durand-Kerner)
    using C = std::complex<double>;
    std::array<C, lpcOrder> z {};
    C seed (0.4, 0.9), pw (1.0, 0.0);
    for (int i = 0; i < lpcOrder; ++i) { pw *= seed; z[(size_t) i] = pw; }
    for (int it = 0; it < 80; ++it)
    {
        double change = 0.0;
        for (int i = 0; i < lpcOrder; ++i)
        {
            C num (1.0, 0.0);
            for (int j = 1; j <= lpcOrder; ++j) num = num * z[(size_t) i] + a[(size_t) j];
            C den (1.0, 0.0);
            for (int j = 0; j < lpcOrder; ++j)
                if (j != i) den *= (z[(size_t) i] - z[(size_t) j]);
            if (std::abs (den) < 1.0e-300) den = C (1.0e-12, 0.0);
            const C d = num / den;
            z[(size_t) i] -= d;
            change = std::max (change, std::abs (d));
        }
        if (change < 1.0e-9) break;
    }

    // formant candidates: upper half-plane roots with a narrow bandwidth
    std::array<float, lpcOrder> fq {};
    int nf = 0;
    for (int i = 0; i < lpcOrder; ++i)
    {
        const C& c = z[(size_t) i];
        const double mag = std::abs (c);
        if (! std::isfinite (mag) || c.imag() <= 0.0 || mag <= 0.0 || mag >= 1.0) continue;
        const float f = (float) (std::arg (c) * fsd / twoPi);
        const float bw = (float) (-std::log (mag) * fsd / 3.141592653589793);
        if (f < 150.0f || f > fsd * 0.5f - 150.0f || bw > 550.0f) continue;
        fq[(size_t) nf++] = f;
    }
    std::sort (fq.begin(), fq.begin() + nf);
    float f1 = 0.0f, f2 = 0.0f;
    int i1 = -1;
    for (int i = 0; i < nf; ++i)
        if (fq[(size_t) i] >= 180.0f && fq[(size_t) i] <= 1100.0f) { f1 = fq[(size_t) i]; i1 = i; break; }
    if (i1 < 0) return;
    for (int i = i1 + 1; i < nf; ++i)
        if (fq[(size_t) i] > f1 + 120.0f && fq[(size_t) i] <= 3200.0f) { f2 = fq[(size_t) i]; break; }
    if (f2 <= 0.0f) return;
    if (target[Noisiness] > 0.6f) return; // a fricative: keep the vowel of the syllable
    f1Raw = f1;
    f2Raw = f2;
    float x, y;
    formantsToMap (f1, f2, x, y);
    target[VowelX] = 2.0f * x - 1.0f; // bipolar: 0 = the centre of the map (and the rest position)
    target[VowelY] = 2.0f * y - 1.0f;
}

void VoiceAnalyser::analysePitch() noexcept
{
    // YIN-style cumulative mean normalised difference over the newest samples, 70 .. 900 Hz
    const int minLag = std::max (2, (int) (fsd / 900.0f));
    const int maxLag = std::min ((int) (fsd / 70.0f), maxWindow - pitchWindow - 2);
    const int w = std::min (pitchWindow, maxWindow - maxLag - 1);
    if (maxLag <= minLag + 2 || w < 32) return;
    const int start = ringPos - w - maxLag + 2 * maxWindow;
    auto at = [&] (int i) { return ring[(size_t) ((start + i) % maxWindow)]; };
    std::array<float, maxWindow> d {};
    float run = 0.0f;
    int best = -1;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        float acc = 0.0f;
        for (int j = 0; j < w; ++j)
        {
            const float diff = at (j) - at (j + tau);
            acc += diff * diff;
        }
        run += acc;
        d[(size_t) tau] = run > 0.0f ? acc * (float) tau / run : 1.0f;
    }
    for (int tau = minLag; tau < maxLag; ++tau)
        if (d[(size_t) tau] < 0.2f)
        {
            while (tau + 1 < maxLag && d[(size_t) (tau + 1)] < d[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0) { voicing = 0.0f; return; }
    voicing = 1.0f - d[(size_t) best];
    const float y0 = d[(size_t) (best - 1)], y1 = d[(size_t) best], y2 = d[(size_t) (best + 1)];
    const float den = y0 - 2.0f * y1 + y2;
    const float t = (float) best + (std::abs (den) > 1.0e-9f ? 0.5f * (y0 - y2) / den : 0.0f);
    const float f = fsd / std::max (1.0f, t);
    const float note = 69.0f + 12.0f * std::log2 (f / 440.0f);
    target[Pitch] = clamp01 ((note - 36.0f) / 48.0f);
}

void VoiceAnalyser::publishUi() noexcept
{
    for (int v = 0; v < numValues; ++v) ui[(size_t) v].store (out[(size_t) v], std::memory_order_relaxed);
    uiDb.store (inDb, std::memory_order_relaxed);
    uiOpen.store (open, std::memory_order_relaxed);
}
} // namespace ohpe::voice
