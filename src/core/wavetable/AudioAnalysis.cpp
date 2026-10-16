// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioAnalysis.h"

#include "../dsp/Fft.h"
#include "../sample/SampleData.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace ohpe::usertable
{
namespace
{
constexpr double piD = 3.14159265358979323846;
using cplx = std::complex<double>;

bool report (const Progress& p, float v) { return ! p || p (std::clamp (v, 0.0f, 1.0f)); }

/** Region of the sound above -50 dB of its peak: [first, last). */
void trimmed (const std::vector<float>& x, int64_t& first, int64_t& last, float& pk)
{
    pk = 0.0f;
    for (float v : x) pk = std::max (pk, std::abs (v));
    first = 0;
    last = (int64_t) x.size();
    if (pk <= 0.0f) return;
    const float thr = pk * 0.0032f;
    while (first < last && std::abs (x[(size_t) first]) < thr) ++first;
    while (last > first && std::abs (x[(size_t) last - 1]) < thr) --last;
}

/** YIN over tau in [tauMin, tauMax] on a window of w samples from x (x must hold w + tauMax samples). */
double yin (const float* x, int w, int tauMin, int tauMax, float& clarity)
{
    clarity = 0.0f;
    if (tauMax <= tauMin + 2 || w < 16) return 0.0;
    std::vector<double> d ((size_t) tauMax + 2, 0.0);
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        double s = 0.0;
        for (int j = 0; j < w; ++j)
        {
            const double e = (double) x[j] - (double) x[j + tau];
            s += e * e;
        }
        d[(size_t) tau] = s;
    }
    // cumulative mean normalised difference
    std::vector<double> c ((size_t) tauMax + 2, 1.0);
    double run = 0.0;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        run += d[(size_t) tau];
        c[(size_t) tau] = run > 0.0 ? d[(size_t) tau] * (double) tau / run : 1.0;
    }
    int best = -1;
    for (int tau = std::max (2, tauMin); tau <= tauMax; ++tau)
        if (c[(size_t) tau] < 0.15)
        {
            while (tau + 1 <= tauMax && c[(size_t) tau + 1] < c[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        double m = 1.0e9;
        for (int tau = std::max (2, tauMin); tau <= tauMax; ++tau)
            if (c[(size_t) tau] < m) { m = c[(size_t) tau]; best = tau; }
        if (best < 0 || m > 0.45) return 0.0;
    }
    clarity = (float) std::clamp (1.0 - c[(size_t) best], 0.0, 1.0);
    // parabolic interpolation
    const double a = c[(size_t) best - 1], b = c[(size_t) best], cc = c[(size_t) best + 1];
    const double den = a - 2.0 * b + cc;
    double off = std::abs (den) > 1.0e-12 ? 0.5 * (a - cc) / den : 0.0;
    off = std::clamp (off, -0.5, 0.5);
    return (double) best + off;
}

/** Hermite read of the mono sound at a fractional position (clamped to the sound). */
inline double at (const std::vector<float>& x, double pos)
{
    return (double) readHermite (x.data(), (int64_t) x.size(), pos);
}

void forward (const float* frame, std::vector<cplx>& spec)
{
    spec.assign ((size_t) frameSize, cplx (0.0, 0.0));
    for (int i = 0; i < frameSize; ++i) spec[(size_t) i] = cplx (frame[i], 0.0);
    dsp::fft (spec, -1);
}

void inverse (std::vector<cplx> spec, float* frame)
{
    std::vector<cplx> buf ((size_t) frameSize, cplx (0.0, 0.0));
    for (int h = 1; h < frameSize / 2; ++h) buf[(size_t) h] = spec[(size_t) h] * (2.0 / (double) frameSize);
    dsp::fft (buf, +1);
    for (int i = 0; i < frameSize; ++i) frame[i] = (float) buf[(size_t) i].real();
}
} // namespace

float detectPitch (const float* x, int64_t n, double sr, float minHz, float maxHz, float* clarityOut)
{
    if (clarityOut != nullptr) *clarityOut = 0.0f;
    if (x == nullptr || n < 64 || sr <= 0.0) return 0.0f;
    const int tauMax = (int) std::min<int64_t> ((int64_t) std::ceil (sr / std::max (1.0f, minHz)), n / 2);
    const int tauMin = std::max (2, (int) std::floor (sr / std::max (minHz + 1.0f, maxHz)));
    const int w = (int) std::min<int64_t> (n - tauMax - 2, std::max (tauMax, 1024));
    float clarity = 0.0f;
    const double tau = yin (x, w, tauMin, tauMax, clarity);
    if (clarityOut != nullptr) *clarityOut = clarity;
    return tau > 0.0 ? (float) (sr / tau) : 0.0f;
}

void alignFrames (std::vector<float>& frames, int numFrames)
{
    std::vector<cplx> spec;
    for (int f = 0; f < numFrames; ++f)
    {
        float* fr = frames.data() + (size_t) f * frameSize;
        forward (fr, spec);
        int ref = 1;
        double best = 0.0;
        for (int h = 1; h <= maxHarmonics; ++h) best = std::max (best, std::norm (spec[(size_t) h]));
        if (best <= 0.0) continue;
        if (std::norm (spec[1]) < 1.0e-3 * best) // weak fundamental: the strongest harmonic sets the phase
            for (int h = 2; h <= maxHarmonics; ++h)
                if (std::norm (spec[(size_t) h]) >= best) { ref = h; break; }
        // circular shift: X[h] * e^(i h theta); the reference harmonic ends at phase -pi/2 (a sine)
        const double theta = (-0.5 * piD - std::arg (spec[(size_t) ref])) / (double) ref;
        for (int h = 1; h < frameSize / 2; ++h) spec[(size_t) h] *= std::polar (1.0, (double) h * theta);
        inverse (spec, fr);
    }
}

void smoothFrames (std::vector<float>& frames, int numFrames, float amount)
{
    if (amount <= 0.001f || numFrames < 3) return;
    const double sigma = std::max (0.35, (double) std::clamp (amount, 0.0f, 1.0f) * (double) numFrames / 8.0);
    const int radius = std::max (1, (int) std::ceil (3.0 * sigma));
    std::vector<std::vector<cplx>> specs ((size_t) numFrames);
    for (int f = 0; f < numFrames; ++f) forward (frames.data() + (size_t) f * frameSize, specs[(size_t) f]);
    std::vector<cplx> acc;
    for (int f = 0; f < numFrames; ++f)
    {
        acc.assign ((size_t) frameSize, cplx (0.0, 0.0));
        double wsum = 0.0;
        for (int k = -radius; k <= radius; ++k)
        {
            const int g = std::clamp (f + k, 0, numFrames - 1);
            const double w = std::exp (-0.5 * (double) (k * k) / (sigma * sigma));
            wsum += w;
            for (int h = 1; h < frameSize / 2; ++h) acc[(size_t) h] += specs[(size_t) g][(size_t) h] * w;
        }
        for (auto& v : acc) v /= wsum;
        inverse (acc, frames.data() + (size_t) f * frameSize);
    }
}

AnalysisResult analyse (const float* mono, int64_t n, double sr, const AnalysisOptions& options, const Progress& progress)
{
    AnalysisResult r;
    if (mono == nullptr || n <= 0 || sr <= 0.0) { r.error = "The sound is empty."; return r; }
    n = std::min<int64_t> (n, (int64_t) (std::max (1.0, options.maxSeconds) * sr));
    std::vector<float> x (mono, mono + n);
    for (auto& v : x)
        if (! std::isfinite (v)) v = 0.0f;
    // DC removal (one-pole high-pass at about 5 Hz, forward)
    {
        const double c = 1.0 - 2.0 * piD * 5.0 / sr;
        double px = 0.0, py = 0.0;
        for (auto& v : x)
        {
            const double y = (double) v - px + c * py;
            px = v;
            py = y;
            v = (float) y;
        }
    }
    int64_t a = 0, b = 0;
    float pk = 0.0f;
    trimmed (x, a, b, pk);
    if (pk < 1.0e-4f || b - a < 64) { r.error = "The sound is silent."; return r; }
    if (! report (progress, 0.02f)) { r.error = "Cancelled."; return r; }

    // global pitch: median of the clear estimates at five places
    std::vector<float> pitches;
    const int tauMax = (int) std::ceil (sr / 30.0);
    const int win = std::max (tauMax, 1024) + tauMax + 4;
    for (int k = 0; k < 5; ++k)
    {
        const int64_t len = b - a;
        if (len < (int64_t) win) { if (k == 0 && len > 256) { float cl = 0.0f; const float f = detectPitch (x.data() + a, len, sr, 30.0f, 2000.0f, &cl); if (f > 0.0f && cl > 0.5f) pitches.push_back (f); } break; }
        const int64_t s = a + (int64_t) ((double) (len - win) * (0.1 + 0.2 * k));
        float cl = 0.0f;
        const float f = detectPitch (x.data() + s, win, sr, 30.0f, 2000.0f, &cl);
        if (f > 0.0f && cl > 0.5f) pitches.push_back (f);
    }
    double period = 2048.0;
    if (! pitches.empty())
    {
        std::sort (pitches.begin(), pitches.end());
        r.pitchHz = pitches[pitches.size() / 2];
        r.pitched = true;
        period = sr / (double) r.pitchHz;
    }
    if (! report (progress, 0.1f)) { r.error = "Cancelled."; return r; }

    // the sound needs two periods (the crossfade reads the period before the slice)
    if ((double) (b - a) < 2.0 * period + 4.0)
    {
        if (r.pitched && (double) n >= 2.0 * period + 4.0) { a = 0; b = n; }
        else { r.error = "The sound is too short."; return r; }
    }
    const int frames = std::clamp (options.frames, 8, maxFrames);
    r.numFrames = frames;
    r.frames.assign ((size_t) frames * frameSize, 0.0f);
    const double first = (double) a + period + 1.0, last = (double) b - period - 2.0;
    for (int f = 0; f < frames; ++f)
    {
        double s = frames > 1 ? first + (last - first) * (double) f / (double) (frames - 1) : first;
        s = std::clamp (s, (double) a + period + 1.0, std::max ((double) a + period + 1.0, last));
        double p = period;
        if (r.pitched) // local refinement within -20 % .. +25 %: follows a gliding pitch, no octave jumps
        {
            const int lo = std::max (2, (int) std::floor (period * 0.8)), hi = (int) std::ceil (period * 1.25);
            const int w = std::max (hi, 512);
            const int64_t st = (int64_t) s - (int64_t) period;
            if (st >= 0 && st + w + hi + 2 < (int64_t) x.size())
            {
                float cl = 0.0f;
                const double tau = yin (x.data() + st, w, lo, hi, cl);
                if (tau > 0.0 && cl > 0.4f) p = tau;
            }
        }
        float* dst = r.frames.data() + (size_t) f * frameSize;
        for (int i = 0; i < frameSize; ++i)
        {
            const double t = (double) i / (double) frameSize;
            const double va = at (x, s + t * p);
            const double vb = at (x, s + (t - 1.0) * p);
            const double u = std::clamp ((t - 0.875) / 0.125, 0.0, 1.0);
            const double w = u * u * (3.0 - 2.0 * u);
            dst[i] = (float) (va + w * (vb - va));
        }
        if ((f & 7) == 7 && ! report (progress, 0.1f + 0.5f * (float) (f + 1) / (float) frames)) { r.error = "Cancelled."; return r; }
    }
    if (options.alignPhase) alignFrames (r.frames, frames);
    if (! report (progress, 0.7f)) { r.error = "Cancelled."; return r; }
    smoothFrames (r.frames, frames, options.smoothing);
    if (! report (progress, 0.8f)) { r.error = "Cancelled."; return r; }

    if (options.formants)
    {
        const int nf = std::clamp (frames, 2, maxFormantFrames);
        auto sub = [&] (float v) { return report (progress, 0.8f + 0.2f * v); };
        r.formants = extractFormants (x.data() + a, b - a, sr, nf, sub);
    }
    if (! report (progress, 1.0f)) { r.error = "Cancelled."; return r; }
    r.ok = true;
    return r;
}

std::vector<FormantFrame> extractFormants (const float* mono, int64_t n, double sr, int numFrames, const Progress& progress)
{
    std::vector<FormantFrame> out;
    if (mono == nullptr || n <= 0 || sr <= 0.0 || numFrames <= 0) return out;
    numFrames = std::min (numFrames, maxFormantFrames);

    // decimate to about 11 kHz (windowed-sinc low-pass)
    const int D = std::max (1, (int) std::floor (sr / 11025.0 + 1.0e-9));
    const double fsd = sr / (double) D;
    const int half = 8 * D;
    std::vector<double> h ((size_t) (2 * half + 1));
    {
        const double fc = 0.45 / (double) D; // cycles per input sample
        double sum = 0.0;
        for (int k = -half; k <= half; ++k)
        {
            const double t = (double) k;
            const double sinc = k == 0 ? 2.0 * fc : std::sin (2.0 * piD * fc * t) / (piD * t);
            const double wnd = 0.42 + 0.5 * std::cos (piD * t / (double) half) + 0.08 * std::cos (2.0 * piD * t / (double) half);
            h[(size_t) (k + half)] = sinc * wnd;
            sum += sinc * wnd;
        }
        for (auto& v : h) v /= sum;
    }
    const int64_t m = n / D;
    std::vector<double> y ((size_t) std::max<int64_t> (m, 1), 0.0);
    for (int64_t i = 0; i < m; ++i)
    {
        double s = 0.0;
        const int64_t c = i * D;
        for (int k = -half; k <= half; ++k)
        {
            const int64_t j = c + k;
            if (j >= 0 && j < n) s += h[(size_t) (k + half)] * (double) mono[j];
        }
        y[(size_t) i] = s;
    }

    constexpr int order = 12;
    constexpr int points = 512;
    constexpr double preEmph = 0.94;
    const int nw = std::max (64, (int) std::lround (0.03 * fsd));
    std::vector<double> wbuf ((size_t) nw);
    std::vector<bool> silent ((size_t) numFrames, false);
    double maxEnergy = 0.0;
    std::vector<double> energy ((size_t) numFrames, 0.0);
    out.resize ((size_t) numFrames);

    for (int f = 0; f < numFrames; ++f)
    {
        const double span = (double) std::max<int64_t> (0, m - nw);
        const int64_t st = (int64_t) (numFrames > 1 ? span * (double) f / (double) (numFrames - 1) : span * 0.5);
        double e = 0.0;
        for (int i = 0; i < nw; ++i)
        {
            const int64_t j = st + i;
            const double v = j < m ? y[(size_t) j] : 0.0;
            const double vp = j - 1 >= 0 && j - 1 < m ? y[(size_t) (j - 1)] : 0.0;
            const double ham = 0.54 - 0.46 * std::cos (2.0 * piD * (double) i / (double) (nw - 1));
            wbuf[(size_t) i] = (v - preEmph * vp) * ham;
            e += v * v;
        }
        energy[(size_t) f] = e;
        maxEnergy = std::max (maxEnergy, e);

        // autocorrelation + Levinson-Durbin
        double rr[order + 1] {};
        for (int k = 0; k <= order; ++k)
            for (int i = k; i < nw; ++i) rr[k] += wbuf[(size_t) i] * wbuf[(size_t) (i - k)];
        if (rr[0] <= 1.0e-12) { silent[(size_t) f] = true; continue; }
        rr[0] *= 1.0001; // white-noise correction (stable filter)
        double lpc[order + 1] {};
        lpc[0] = 1.0;
        double err = rr[0];
        for (int i = 1; i <= order; ++i)
        {
            double acc = rr[i];
            for (int j = 1; j < i; ++j) acc += lpc[j] * rr[i - j];
            const double k = -acc / err;
            double tmp[order + 1];
            for (int j = 0; j <= order; ++j) tmp[j] = lpc[j];
            for (int j = 1; j < i; ++j) lpc[j] = tmp[j] + k * tmp[i - j];
            lpc[i] = k;
            err *= (1.0 - k * k);
            if (err <= 0.0) break;
        }
        // envelope in dB (de-emphasised), peaks between 150 Hz and 5 kHz
        std::vector<double> env ((size_t) points + 1);
        for (int j = 0; j <= points; ++j)
        {
            const double w = piD * (double) j / (double) points;
            cplx A (0.0, 0.0);
            for (int k = 0; k <= order; ++k) A += lpc[k] * std::polar (1.0, -w * (double) k);
            const cplx P = 1.0 - preEmph * std::polar (1.0, -w);
            env[(size_t) j] = -20.0 * std::log10 (std::max (1.0e-9, std::abs (A))) - 20.0 * std::log10 (std::max (1.0e-9, std::abs (P)));
        }
        struct Peak { double hz, db, bw; };
        std::vector<Peak> peaks;
        const double hzPer = fsd * 0.5 / (double) points;
        const int j0 = std::max (1, (int) (150.0 / hzPer)), j1 = std::min (points - 1, (int) (std::min (5000.0, fsd * 0.5 - 150.0) / hzPer));
        for (int j = j0; j <= j1; ++j)
            if (env[(size_t) j] > env[(size_t) j - 1] && env[(size_t) j] >= env[(size_t) j + 1])
            {
                const double l = env[(size_t) j - 1], c = env[(size_t) j], rgt = env[(size_t) j + 1];
                const double den = l - 2.0 * c + rgt;
                const double off = std::abs (den) > 1.0e-12 ? std::clamp (0.5 * (l - rgt) / den, -0.5, 0.5) : 0.0;
                const double pkDb = c - 0.25 * (l - rgt) * off;
                int lo = j, hi = j;
                while (lo > 0 && env[(size_t) lo] > pkDb - 3.0) --lo;
                while (hi < points && env[(size_t) hi] > pkDb - 3.0) ++hi;
                peaks.push_back ({ ((double) j + off) * hzPer, pkDb, (double) (hi - lo) * hzPer });
            }
        std::sort (peaks.begin(), peaks.end(), [] (const Peak& p, const Peak& q) { return p.db > q.db; });
        if (peaks.size() > (size_t) numFormants) peaks.resize ((size_t) numFormants);
        std::sort (peaks.begin(), peaks.end(), [] (const Peak& p, const Peak& q) { return p.hz < q.hz; });
        FormantFrame fr;
        double top = -1.0e9;
        for (const auto& p : peaks) top = std::max (top, p.db);
        double lastHz = 0.0;
        for (int k = 0; k < numFormants; ++k)
        {
            if (k < (int) peaks.size())
            {
                const auto& p = peaks[(size_t) k];
                fr.freq[(size_t) k] = (float) p.hz;
                fr.gainDb[(size_t) k] = (float) (p.db - top);
                fr.bandwidth[(size_t) k] = (float) p.bw;
                lastHz = p.hz;
            }
            else // fewer than five peaks: quiet formants above the last one
            {
                lastHz = std::max (lastHz + 700.0, 500.0 * (k + 1));
                fr.freq[(size_t) k] = (float) lastHz;
                fr.gainDb[(size_t) k] = -40.0f;
                fr.bandwidth[(size_t) k] = 150.0f;
            }
        }
        out[(size_t) f] = sanitise (fr);
        if (! report (progress, 0.95f * (float) (f + 1) / (float) numFrames)) return {};
    }
    // quiet frames (below -50 dB of the loudest) take a neighbour's formants
    for (int f = 0; f < numFrames; ++f)
        if (energy[(size_t) f] < maxEnergy * 1.0e-5) silent[(size_t) f] = true;
    int firstSound = -1;
    for (int f = 0; f < numFrames && firstSound < 0; ++f)
        if (! silent[(size_t) f]) firstSound = f;
    if (firstSound < 0) return {};
    for (int f = 0; f < numFrames; ++f)
        if (silent[(size_t) f]) out[(size_t) f] = f > firstSound ? out[(size_t) f - 1] : out[(size_t) firstSound];
    return out;
}
} // namespace ohpe::usertable
