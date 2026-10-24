// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "KeyGravity.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ohpe
{
namespace
{
constexpr float twoPi = 6.283185307179586f;
constexpr int baseFrame[KeyGravity::numQualities] = { 512, 1024, 2048 };

inline float wrapPi (float x) noexcept
{
    x = std::fmod (x + 3.14159265f, twoPi);
    if (x < 0.0f) x += twoPi;
    return x - 3.14159265f;
}
inline float clean (float x) noexcept { return std::isfinite (x) ? x : 0.0f; }
inline int rateScale (double sr) noexcept { return sr >= 176400.0 ? 4 : (sr >= 88200.0 ? 2 : 1); }
} // namespace

// ------------------------------------------------------------------------------------------------ helpers
float KeyGravity::pull (float a, float c) noexcept
{
    a = std::clamp (a, 0.0f, 1.0f);
    if (a <= 0.0f) return 0.0f;
    return std::pow (a, std::exp2 (-2.0f * std::clamp (c, -1.0f, 1.0f)));
}

float KeyGravity::distanceToTarget (float midi, uint16_t mask, uint16_t chord, float bonus) noexcept
{
    mask &= 0xfff;
    if (mask == 0 || ! std::isfinite (midi)) return 0.0f;
    const int base = (int) std::floor (midi);
    float best = 0.0f, bestScore = 1.0e9f;
    for (int o = -7; o <= 7; ++o)
    {
        const int n = base + o;
        const int pc = ((n % 12) + 12) % 12;
        if (((mask >> pc) & 1u) == 0) continue;
        const float d = (float) n - midi;
        const float score = std::abs (d) - ((bonus > 0.0f && ((chord >> pc) & 1u) != 0) ? bonus : 0.0f);
        if (score < bestScore) { bestScore = score; best = d; }
    }
    return best;
}

uint16_t KeyGravity::targetMask() const noexcept
{
    const uint16_t s = scaleMask != 0 ? scaleMask : (uint16_t) 0xfff;
    switch (target)
    {
        case TargetScale: return s;
        case TargetChord: return chordMask != 0 ? chordMask : s;
        default: return (uint16_t) (s | chordMask);
    }
}

int KeyGravity::frameLatency (int q) const noexcept
{
    const int n = baseFrame[std::clamp (q, 0, numQualities - 1)] * rateScale (sr);
    return n; // frame-based overlap-add: the output of a sample leaves one frame later
}

// ------------------------------------------------------------------------------------------------ setup
void KeyGravity::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxFrame = baseFrame[QualityHigh] * rateScale (sr);
    ringSize = 1;
    while (ringSize < 2 * maxFrame + 8) ringSize <<= 1;
    ringMask = ringSize - 1;
    for (auto* v : { &dryL, &dryR, &wetL, &wetR }) v->assign ((size_t) ringSize, 0.0f);
    for (auto* v : { &inL, &inR, &outL, &outR, &accL, &accR, &window, &re, &im }) v->assign ((size_t) maxFrame, 0.0f);
    const size_t half = (size_t) maxFrame / 2 + 2;
    for (auto* v : { &mag, &env, &curPhase, &binMidi, &prevPhase, &prevMag, &aRe, &aIm, &bRe, &bIm, &oaRe, &oaIm, &obRe, &obIm, &binW,
                     &envPrefix, &binWeight })
        v->assign (half + 1, 0.0f);
    bwHalf = -1; // p79: weights recomputed
    twRe.assign ((size_t) maxFrame / 2, 0.0f);
    twIm.assign ((size_t) maxFrame / 2, 0.0f);
    for (int k = 0; k < maxFrame / 2; ++k)
    {
        const double a = -2.0 * 3.14159265358979323846 * k / maxFrame;
        twRe[(size_t) k] = (float) std::cos (a);
        twIm[(size_t) k] = (float) std::sin (a);
    }
    bitrev.assign ((size_t) maxFrame, 0);
    fadeLen = std::max (1, (int) (0.02 * sr));
    wetStep = 1.0f / (float) fadeLen;
    offHoldSamples = (int) sr; // 1 s at amount 0 before the stage stops (modulation around 0 never toggles the latency)
    activeQuality = quality;
    reset();
}

void KeyGravity::reset() noexcept
{
    running = stopping = ringsLive = false;
    wetGain = 0.0f;
    warmup = zeroSamples = 0;
    alignNow = alignFrom = 0;
    fadeLeft = 0;
    ringPos = 0;
    mixSmoothed = mix;
    for (auto* v : { &dryL, &dryR, &wetL, &wetR })
        std::fill (v->begin(), v->end(), 0.0f);
    startAnalysis();
}

void KeyGravity::startAnalysis() noexcept
{
    if (maxFrame <= 0) return;
    N = std::min (maxFrame, baseFrame[std::clamp (activeQuality, 0, numQualities - 1)] * rateScale (sr));
    hop = N / 4;
    fifoLat = N - hop;
    lat = N;
    rover = fifoLat;
    for (auto* v : { &inL, &inR, &outL, &outR, &accL, &accR })
        std::fill (v->begin(), v->end(), 0.0f);
    for (int i = 0; i < N; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (twoPi * (float) i / (float) N); // periodic Hann
    int bits = 0;
    while ((1 << bits) < N) ++bits;
    for (int i = 0; i < N; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitrev[(size_t) i] = r;
    }
    const float binHz = (float) sr / (float) N;
    for (int k = 0; k <= N / 2; ++k)
        binMidi[(size_t) k] = k == 0 ? -100.0f : 69.0f + 12.0f * std::log2 ((float) k * binHz / 440.0f);
    std::fill (prevPhase.begin(), prevPhase.end(), 0.0f);
    std::fill (prevMag.begin(), prevMag.end(), 0.0f);
    notePhase.fill (0.0);
    numPrev = 0;
    havePrev = false;
    prevEnergy = 0.0f;
}

// ------------------------------------------------------------------------------------------------ parameters
void KeyGravity::setAmount (float a) noexcept { amount = std::isfinite (a) ? std::clamp (a, 0.0f, 1.0f) : 0.0f; }
void KeyGravity::setTarget (int t) noexcept { target = std::clamp (t, 0, numTargets - 1); }
void KeyGravity::setGlide (float s) noexcept { glideSec = std::isfinite (s) ? std::clamp (s, 0.001f, 10.0f) : 0.15f; }
void KeyGravity::setCurve (float c) noexcept { curve = std::isfinite (c) ? std::clamp (c, -1.0f, 1.0f) : 0.0f; }
void KeyGravity::setFormant (float f) noexcept { formant = std::isfinite (f) ? std::clamp (f, 0.0f, 1.0f) : 0.0f; }
void KeyGravity::setMix (float m) noexcept { mix = std::isfinite (m) ? std::clamp (m, 0.0f, 1.0f) : 1.0f; }
void KeyGravity::setQuality (int q) noexcept { quality = std::clamp (q, 0, numQualities - 1); }
void KeyGravity::setTargets (uint16_t s, uint16_t c) noexcept
{
    scaleMask = (uint16_t) (s & 0xfff);
    chordMask = (uint16_t) (c & 0xfff);
}
void KeyGravity::setAlignLatency (int samples) noexcept { alignWanted = std::clamp (samples, 0, std::max (0, ringSize - 4)); }

// ------------------------------------------------------------------------------------------------ audio
void KeyGravity::process (float* l, float* r, int n) noexcept
{
    if (n <= 0 || maxFrame <= 0) return;
    if (! running && amount <= 0.0f && alignWanted == 0 && alignNow == 0 && fadeLeft == 0)
    {
        ringsLive = false; // bypass: bit-exact, no work
        return;
    }
    // run state
    if (amount > 0.0f)
    {
        zeroSamples = 0;
        if (! running || stopping)
        {
            if (! running || activeQuality != quality)
            {
                activeQuality = quality;
                startAnalysis();
                wetGain = 0.0f;
                warmup = lat;
            }
            running = true;
            stopping = false;
        }
    }
    else if (running && ! stopping)
    {
        zeroSamples += n;
        if (zeroSamples >= offHoldSamples) stopping = true;
    }
    if (running && quality != activeQuality) // a new frame size: restart the analysis (the wet fades in again)
    {
        activeQuality = quality;
        startAnalysis();
        wetGain = 0.0f;
        warmup = lat;
    }
    if (! ringsLive)
    {
        for (auto* v : { &dryL, &dryR, &wetL, &wetR })
            std::fill (v->begin(), v->end(), 0.0f);
        ringsLive = true;
    }
    const int want = std::max (alignWanted, running ? lat : 0);
    if (fadeLeft == 0 && want != alignNow)
    {
        alignFrom = alignNow;
        alignNow = want;
        fadeLeft = fadeLen;
    }
    const float wetTarget = running && ! stopping ? 1.0f : 0.0f;
    const float mixStep = (mix - mixSmoothed) / (float) n;

    auto readOut = [this] (int d, float& yl, float& yr) noexcept {
        const int i = (ringPos - d) & ringMask;
        float ol = dryL[(size_t) i], orr = dryR[(size_t) i];
        if (wetGain > 0.0f && d >= lat)
        {
            const int j = (ringPos - (d - lat)) & ringMask;
            const float g = wetGain * mixSmoothed;
            ol += g * (wetL[(size_t) j] - ol);
            orr += g * (wetR[(size_t) j] - orr);
        }
        yl = ol;
        yr = orr;
    };

    for (int s = 0; s < n; ++s)
    {
        const float xl = clean (l[s]), xr = clean (r[s]);
        dryL[(size_t) ringPos] = xl;
        dryR[(size_t) ringPos] = xr;
        float wl = 0.0f, wr = 0.0f;
        if (running) wl = stftSample (xl, xr, wr);
        wetL[(size_t) ringPos] = wl;
        wetR[(size_t) ringPos] = wr;
        mixSmoothed += mixStep;
        float yl, yr;
        readOut (alignNow, yl, yr);
        if (fadeLeft > 0)
        {
            float fl, fr;
            readOut (alignFrom, fl, fr);
            const float t = 1.0f - (float) fadeLeft / (float) fadeLen;
            yl = fl + (yl - fl) * t;
            yr = fr + (yr - fr) * t;
            if (--fadeLeft == 0) alignFrom = alignNow;
        }
        l[s] = yl;
        r[s] = yr;
        if (warmup > 0) --warmup;
        else if (wetGain != wetTarget) wetGain = wetGain < wetTarget ? std::min (wetTarget, wetGain + wetStep) : std::max (wetTarget, wetGain - wetStep);
        ringPos = (ringPos + 1) & ringMask;
    }
    mixSmoothed = mix;
    if (stopping && wetGain <= 0.0f) // the wet has faded out: the stage stops (the latency follows via the core)
    {
        running = false;
        stopping = false;
    }
}

void KeyGravity::tickSilent (int n) noexcept
{
    if (amount <= 0.0f && running) // silence: nothing to fade, the stage may stop at once after the hold
    {
        zeroSamples += n;
        if (stopping || zeroSamples >= offHoldSamples)
        {
            running = stopping = false;
            wetGain = 0.0f;
        }
    }
    const int want = std::max (alignWanted, running ? lat : 0);
    if (want != alignNow || fadeLeft > 0)
    {
        alignNow = alignFrom = want;
        fadeLeft = 0;
        ringsLive = false; // the rings hold silence anyway; cleared when audio comes back
    }
}

float KeyGravity::stftSample (float xl, float xr, float& yr) noexcept
{
    inL[(size_t) rover] = xl;
    inR[(size_t) rover] = xr;
    const float yl = outL[(size_t) (rover - fifoLat)];
    yr = outR[(size_t) (rover - fifoLat)];
    if (++rover >= N)
    {
        rover = fifoLat;
        processFrame();
    }
    return yl;
}

void KeyGravity::fft (float* xr, float* xi, int n, bool inverse) const noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const int j = bitrev[(size_t) i];
        if (i < j)
        {
            std::swap (xr[i], xr[j]);
            std::swap (xi[i], xi[j]);
        }
    }
    const float sgn = inverse ? -1.0f : 1.0f;
    for (int len = 2; len <= n; len <<= 1)
    {
        const int halfLen = len >> 1;
        const int step = maxFrame / len;
        for (int i = 0; i < n; i += len)
        {
            for (int k = 0; k < halfLen; ++k)
            {
                const float wr = twRe[(size_t) (k * step)], wi = sgn * twIm[(size_t) (k * step)];
                const int a = i + k, b = a + halfLen;
                const float vr = xr[b] * wr - xi[b] * wi;
                const float vi = xr[b] * wi + xi[b] * wr;
                xr[b] = xr[a] - vr;
                xi[b] = xi[a] - vi;
                xr[a] += vr;
                xi[a] += vi;
            }
        }
    }
}

void KeyGravity::processFrame() noexcept
{
    const int half = N / 2;
    float* R = re.data();
    float* I = im.data();
    for (int i = 0; i < N; ++i)
    {
        R[i] = inL[(size_t) i] * window[(size_t) i];
        I[i] = inR[(size_t) i] * window[(size_t) i];
    }
    fft (R, I, N, false);

    // the two real spectra out of the packed one; magnitude and phase for the analysis
    float energy = 0.0f, maxMag = 0.0f;
    for (int k = 0; k <= half; ++k)
    {
        const int m = (N - k) & (N - 1);
        const float ar = 0.5f * (R[k] + R[m]), ai = 0.5f * (I[k] - I[m]);
        const float br = 0.5f * (I[k] + I[m]), bi = 0.5f * (R[m] - R[k]);
        aRe[(size_t) k] = ar; aIm[(size_t) k] = ai; bRe[(size_t) k] = br; bIm[(size_t) k] = bi;
        const float ma = std::sqrt (ar * ar + ai * ai), mb = std::sqrt (br * br + bi * bi);
        const float mk = ma + mb;
        mag[(size_t) k] = mk;
        energy += mk * mk;
        maxMag = std::max (maxMag, mk);
        const float sr2 = ar + br, si = ai + bi;
        if (sr2 * sr2 + si * si > 0.01f * mk * mk) curPhase[(size_t) k] = std::atan2 (si, sr2);
        else curPhase[(size_t) k] = ma >= mb ? std::atan2 (ai, ar) : std::atan2 (bi, br);
    }
    if (! std::isfinite (energy)) // NaN / Inf got in somewhere: start clean
    {
        startAnalysis();
        return;
    }
    // an attack (or the first frame, no phase history yet) passes as it is, with its own phases
    const bool transient = ! havePrev || (energy > 1.0e-8f && energy > 4.0f * prevEnergy);
    prevEnergy = energy;

    std::fill (oaRe.begin(), oaRe.begin() + half + 1, 0.0f);
    std::fill (oaIm.begin(), oaIm.begin() + half + 1, 0.0f);
    std::fill (obRe.begin(), obRe.begin() + half + 1, 0.0f);
    std::fill (obIm.begin(), obIm.begin() + half + 1, 0.0f);

    // spectral envelope (box smoothing) for formant keep
    const int W = std::max (2, half / 48);
    envPrefix[0] = 0.0f;
    for (int k = 0; k <= half; ++k) envPrefix[(size_t) k + 1] = envPrefix[(size_t) k] + mag[(size_t) k];
    for (int k = 0; k <= half; ++k)
    {
        const int a = std::max (0, k - W), b = std::min (half, k + W);
        env[(size_t) k] = (envPrefix[(size_t) b + 1] - envPrefix[(size_t) a]) / (float) (b - a + 1) + 1.0e-12f;
    }

    // peaks
    const float binHz = (float) sr / (float) N;
    const float thr = std::max (maxMag * 3.0e-4f, 1.0e-9f);
    const int kmin = std::max (2, (int) std::ceil (30.0f / binHz));
    int np = 0;
    for (int k = kmin; k <= half - 2 && np < maxPeaks; ++k)
    {
        const float m = mag[(size_t) k];
        if (m > thr && m > mag[(size_t) k - 1] && m >= mag[(size_t) k + 1] && m > mag[(size_t) k - 2] && m >= mag[(size_t) k + 2])
            peakBin[(size_t) np++] = k;
    }

    const uint16_t mask = targetMask();
    const float bonus = target == TargetChordScale ? 0.5f : 0.0f;
    const float g = pull (amount, curve);
    const float hopSec = (float) hop / (float) sr;
    const float cg = 1.0f - std::exp (-hopSec / std::max (0.001f, glideSec));
    const float phaseAdv = twoPi * (float) hop / (float) N; // expected phase advance per bin

    auto copyRegion = [&] (int lo, int hi, int off, float c, float s, float gain) {
        for (int k = lo; k <= hi; ++k)
        {
            const int t = k + off;
            if (t < 0 || t > half) continue;
            const float ar = aRe[(size_t) k], ai = aIm[(size_t) k], br = bRe[(size_t) k], bi = bIm[(size_t) k];
            oaRe[(size_t) t] += (ar * c - ai * s) * gain;
            oaIm[(size_t) t] += (ar * s + ai * c) * gain;
            obRe[(size_t) t] += (br * c - bi * s) * gain;
            obIm[(size_t) t] += (br * s + bi * c) * gain;
        }
    };

    if (np == 0)
        copyRegion (0, half, 0, 1.0f, 0.0f, 1.0f);
    int j = 0;
    for (int p = 0; p < np; ++p)
    {
        const int kp = peakBin[(size_t) p];
        // region: from the lowest bin between this peak and the one before, to the lowest before the next one
        int lo = 0, hi = half;
        if (p > 0)
        {
            lo = peakBin[(size_t) p - 1] + 1;
            for (int k = lo + 1; k < kp; ++k)
                if (mag[(size_t) k] < mag[(size_t) lo]) lo = k;
        }
        if (p + 1 < np)
        {
            const int nk = peakBin[(size_t) p + 1];
            int lowest = kp + 1;
            for (int k = kp + 2; k < nk; ++k)
                if (mag[(size_t) k] < mag[(size_t) lowest]) lowest = k;
            hi = lowest - 1;
        }
        // true frequency from the phase advance
        const float dphi = wrapPi (curPhase[(size_t) kp] - prevPhase[(size_t) kp] - phaseAdv * (float) kp);
        float fbin = (float) kp + dphi / phaseAdv;
        if (! havePrev || std::abs (fbin - (float) kp) > 1.5f) // no phase history yet: parabolic fit of the log magnitude
        {
            const float a = std::log (mag[(size_t) kp - 1] + 1.0e-20f), b = std::log (mag[(size_t) kp] + 1.0e-20f),
                        c = std::log (mag[(size_t) kp + 1] + 1.0e-20f);
            const float den = a - 2.0f * b + c;
            fbin = (float) kp + (std::abs (den) > 1.0e-12f ? std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f) : 0.0f);
        }
        const float f = fbin * binHz;
        const float midi = 69.0f + 12.0f * std::log2 (std::max (f, 1.0f) / 440.0f);
        const float d = f > 25.0f ? distanceToTarget (midi, mask, chordMask, bonus) : 0.0f;
        // partial track: the nearest one from the last frame glides to its new target pitch (a chord change flows)
        while (j + 1 < numPrev && std::abs (prevTracks[(size_t) j + 1].midi - midi) <= std::abs (prevTracks[(size_t) j].midi - midi)) ++j;
        const bool matched = numPrev > 0 && std::abs (prevTracks[(size_t) j].midi - midi) < 0.5f;
        float tgt = midi + d;
        if (matched && f > 25.0f && std::abs (prevTracks[(size_t) j].midi - midi) < 0.15f) // a steady partial (not an onset or a new note)
        {
            const float pt = prevTracks[(size_t) j].tgt;
            if (std::abs (tgt - pt) > 0.001f) tgt = pt + (tgt - pt) * cg;
        }
        const float shift = (tgt - midi) * g;
        const float fTgt = f * std::exp2 (shift / 12.0f);
        const float phIn = curPhase[(size_t) kp];
        float phOut = phIn;
        bool identity = transient || std::abs (shift) < 0.003f;
        const bool snapped = ! identity && g >= 0.999f && std::abs (tgt - std::round (tgt)) < 0.02f && f > 25.0f;
        if (! transient && snapped)
        {
            const int note = std::clamp ((int) std::lround (tgt), 0, 127);
            phOut = (float) notePhase[(size_t) note];
            identity = false;
        }
        else if (! identity && matched)
            phOut = prevTracks[(size_t) j].outPhase + twoPi * fTgt * hopSec;
        curTracks[(size_t) p] = { midi, tgt, wrapPi (phOut) };
        if (identity)
        {
            copyRegion (lo, hi, 0, 1.0f, 0.0f, 1.0f);
            continue;
        }
        const int off = (int) std::lround ((fTgt - f) / binHz);
        float gain = 1.0f;
        if (formant > 0.0f)
        {
            const int kt = std::clamp (kp + off, 0, half);
            gain = std::clamp (std::pow (env[(size_t) kt] / env[(size_t) kp], formant), 0.25f, 4.0f);
        }
        const float rot = phOut - phIn;
        copyRegion (lo, hi, off, std::cos (rot), std::sin (rot), gain);
    }

    // high amounts: the bins between the target pitches fade - noise becomes pitched texture in key
    const float tz = std::clamp ((amount - 0.5f) * 2.0f, 0.0f, 1.0f);
    if (tz > 0.0f && ! transient && np > 0)
    {
        float eb = 0.0f, ea = 0.0f;
        if (! (bwHalf == half && bwMask == mask && bwChord == chordMask && bwBonus == bonus && bwTz == tz && bwBinHz == binHz))
        {
            bwHalf = half;
            bwMask = mask;
            bwChord = chordMask;
            bwBonus = bonus;
            bwTz = tz;
            bwBinHz = binHz;
            const float sigma = 0.5f - 0.36f * tz;
            const float minMidi = 69.0f + 12.0f * std::log2 (std::max (1.0f, 1.5f * binHz / 0.0595f) / 440.0f); // a semitone >= 1.5 bins
            for (int k = 1; k <= half; ++k)
            {
                float w = 1.0f;
                if (binMidi[(size_t) k] > minMidi)
                {
                    const float dk = distanceToTarget (binMidi[(size_t) k], mask, chordMask, bonus) / sigma;
                    w = 1.0f - tz * (1.0f - std::exp (-dk * dk));
                }
                binWeight[(size_t) k] = w;
            }
        }
        for (int k = 1; k <= half; ++k)
        {
            const float pr = oaRe[(size_t) k] * oaRe[(size_t) k] + oaIm[(size_t) k] * oaIm[(size_t) k] + obRe[(size_t) k] * obRe[(size_t) k]
                             + obIm[(size_t) k] * obIm[(size_t) k];
            eb += pr;
            const float w = binWeight[(size_t) k];
            binW[(size_t) k] = w;
            ea += pr * w * w;
        }
        const float comp = ea > 1.0e-20f ? std::clamp (std::sqrt (eb / ea), 1.0f, 4.0f) : 1.0f;
        for (int k = 1; k <= half; ++k)
        {
            const float w = binW[(size_t) k] * comp;
            oaRe[(size_t) k] *= w; oaIm[(size_t) k] *= w; obRe[(size_t) k] *= w; obIm[(size_t) k] *= w;
        }
    }

    // back to one packed spectrum: Z = A + iB (and its mirror), inverse FFT, overlap-add
    for (int k = 0; k <= half; ++k)
    {
        const float ar = oaRe[(size_t) k], ai = oaIm[(size_t) k], br = obRe[(size_t) k], bi = obIm[(size_t) k];
        R[k] = ar - bi;
        I[k] = ai + br;
        if (k > 0 && k < half)
        {
            R[N - k] = ar + bi;
            I[N - k] = br - ai;
        }
    }
    fft (R, I, N, true);
    const float scale = 1.0f / (1.5f * (float) N);
    for (int i = 0; i < N; ++i)
    {
        const float w = window[(size_t) i] * scale;
        accL[(size_t) i] += R[i] * w;
        accR[(size_t) i] += I[i] * w;
    }
    for (int i = 0; i < hop; ++i)
    {
        const float a = accL[(size_t) i], b = accR[(size_t) i];
        outL[(size_t) i] = std::abs (a) < 1.0e-20f ? 0.0f : a;
        outR[(size_t) i] = std::abs (b) < 1.0e-20f ? 0.0f : b;
    }
    std::memmove (accL.data(), accL.data() + hop, sizeof (float) * (size_t) (N - hop));
    std::memmove (accR.data(), accR.data() + hop, sizeof (float) * (size_t) (N - hop));
    std::fill (accL.begin() + (N - hop), accL.begin() + N, 0.0f);
    std::fill (accR.begin() + (N - hop), accR.begin() + N, 0.0f);
    std::memmove (inL.data(), inL.data() + hop, sizeof (float) * (size_t) fifoLat);
    std::memmove (inR.data(), inR.data() + hop, sizeof (float) * (size_t) fifoLat);

    std::copy (curPhase.begin(), curPhase.begin() + half + 1, prevPhase.begin());
    for (int note = 0; note < 128; ++note)
    {
        const double fn = 440.0 * std::exp2 ((note - 69) / 12.0);
        notePhase[(size_t) note] = std::fmod (notePhase[(size_t) note] + 6.283185307179586 * fn * hop / sr, 6.283185307179586);
    }
    for (int p = 0; p < np; ++p) prevTracks[(size_t) p] = curTracks[(size_t) p];
    numPrev = havePrev ? np : 0; // the first frame's frequencies are rough: no tracks from it
    havePrev = true;
}
} // namespace ohpe
