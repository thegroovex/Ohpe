// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "WavetableEngine.h"

#include <complex>

namespace ohpe
{
namespace wavetable
{
namespace
{
constexpr int maxHarmonics = Bank::maxHarmonics;

/** In-place iterative radix-2 FFT; sign = +1 for the inverse direction (no scaling). */
void fft (std::vector<std::complex<double>>& x, int sign)
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

/** Harmonic spectrum of one frame: x(t) = sum a[h] cos (2 pi h t) + b[h] sin (2 pi h t), h = 1 .. maxHarmonics. */
struct Spectrum
{
    std::vector<double> a = std::vector<double> (maxHarmonics + 1, 0.0);
    std::vector<double> b = std::vector<double> (maxHarmonics + 1, 0.0);
};

constexpr double piD = 3.14159265358979323846;

Spectrum makeSpectrum (int table, int frameIndex)
{
    Spectrum s;
    const double t = (double) frameIndex / (double) (Bank::numFrames - 1);
    auto saw = [] (int h) { return (2.0 / piD) * ((h % 2) ? 1.0 : -1.0) / h; };
    auto tri = [] (int h) { return (h % 2) ? (8.0 / (piD * piD)) * (((h - 1) / 2) % 2 ? -1.0 : 1.0) / ((double) h * h) : 0.0; };
    auto square = [] (int h) { return (h % 2) ? 4.0 / (piD * h) : 0.0; };
    auto pulse = [] (int h, double duty) { return 2.0 / (piD * h) * std::sin (piD * h * duty); };

    switch (table)
    {
        case Classic: // sine -> triangle -> saw -> square -> narrowing pulse
        {
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                switch (frameIndex)
                {
                    case 0: s.b[(size_t) h] = h == 1 ? 1.0 : 0.0; break;
                    case 1: s.b[(size_t) h] = tri (h); break;
                    case 2: s.b[(size_t) h] = 0.5 * tri (h) + 0.5 * saw (h); break;
                    case 3: s.b[(size_t) h] = saw (h); break;
                    case 4: s.b[(size_t) h] = square (h); break;
                    case 5: s.a[(size_t) h] = pulse (h, 0.3); break;
                    case 6: s.a[(size_t) h] = pulse (h, 0.17); break;
                    default: s.a[(size_t) h] = pulse (h, 0.08); break;
                }
            }
            break;
        }
        case Harmonic: // drawbar-like additive stack that grows frame by frame
        {
            static const int bars[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            for (int j = 0; j <= frameIndex + 1 && j < 9; ++j)
                s.b[(size_t) bars[j]] += std::pow (0.78, j) * (j == frameIndex + 1 ? 0.6 : 1.0);
            break;
        }
        case Formant: // vowel formants a - e - i - o - u over a bright source
        {
            static const double f[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                            { 570, 840, 2410 }, { 300, 870, 2240 } };
            const double x = t * 4.0;
            const int i0 = std::min (3, (int) x);
            const double fr = x - i0;
            double F[3];
            for (int k = 0; k < 3; ++k) F[k] = f[i0][k] + (f[i0 + 1][k] - f[i0][k]) * fr;
            const double bw[3] = { 90.0, 120.0, 170.0 }, g[3] = { 1.0, 0.6, 0.35 };
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const double hz = 110.0 * h; // nominal source pitch for the formant envelope
                double env = 0.015;
                for (int k = 0; k < 3; ++k) env += g[k] * std::exp (-std::pow ((hz - F[k]) / bw[k], 2.0));
                s.b[(size_t) h] = env * std::pow ((double) h, -0.6) * ((h % 2) ? 1.0 : -1.0);
            }
            break;
        }
        case Digital: // sparse, comb-like spectra stepping through harmonic patterns
        {
            const int limit = 8 + 24 * frameIndex;
            for (int h = 1; h <= std::min (limit, maxHarmonics); ++h)
            {
                const bool on = h == 1 || ((h * (frameIndex + 3)) % 7) < 3;
                if (on) s.b[(size_t) h] = std::pow ((double) h, -0.75) * ((h % 3) ? 1.0 : -1.0);
            }
            break;
        }
        case Glass: // energy on square-numbered harmonics, getting brighter
        {
            const double decay = 1.25 - 0.14 * frameIndex;
            s.b[1] = 1.0;
            s.b[2] = 0.15 * t;
            for (int n = 2; n * n <= maxHarmonics; ++n)
                s.b[(size_t) (n * n)] += std::exp (-(double) (n - 1) * decay);
            break;
        }
        default: // Grit: seeded random spectrum, rising brightness
        {
            dsp::Random rnd (0x9e3779b9u + (uint32_t) frameIndex * 7919u);
            const double tilt = -1.6 + 1.1 * t;
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const double amp = (0.25 + rnd.next01()) * std::pow ((double) h, tilt);
                const double ph = 2.0 * piD * rnd.next01();
                s.a[(size_t) h] = amp * std::cos (ph);
                s.b[(size_t) h] = amp * std::sin (ph);
            }
            break;
        }
    }
    return s;
}
} // namespace

const Bank& Bank::get()
{
    static const Bank bank;
    return bank;
}

Bank::Bank()
{
    data.assign ((size_t) (numTables * numFrames * numLevels * stride), 0.0f);
    std::vector<std::complex<double>> buf ((size_t) size);
    std::vector<double> level ((size_t) size);
    for (int tb = 0; tb < numTables; ++tb)
    {
        for (int fr = 0; fr < numFrames; ++fr)
        {
            const auto spec = makeSpectrum (tb, fr);
            double norm = 1.0;
            for (int lv = 0; lv < numLevels; ++lv)
            {
                const int harmonics = maxHarmonics >> lv;
                std::fill (buf.begin(), buf.end(), std::complex<double> (0.0, 0.0));
                for (int h = 1; h <= harmonics; ++h)
                    buf[(size_t) h] = std::complex<double> (spec.a[(size_t) h], -spec.b[(size_t) h]);
                fft (buf, +1);
                double pk = 0.0;
                for (int i = 0; i < size; ++i)
                {
                    level[(size_t) i] = buf[(size_t) i].real();
                    pk = std::max (pk, std::abs (level[(size_t) i]));
                }
                if (lv == 0) norm = pk > 1.0e-9 ? 1.0 / pk : 1.0; // one scale for all levels: no jumps between them
                auto* dst = data.data() + ((size_t) ((tb * numFrames + fr) * numLevels + lv)) * (size_t) stride;
                for (int i = 0; i < size; ++i) dst[i] = (float) (level[(size_t) i] * norm);
                dst[size] = dst[0];
            }
        }
    }
}

namespace
{
/** Smooth, periodic phase distortion (p -> p - c / (2 pi m) sin (2 pi m p)), applied twice; slope <= (1 + c)^2. */
inline float warpPhase (float p, float c, float m, const float* sine) noexcept
{
    for (int k = 0; k < 2; ++k)
    {
        float q = p * m;
        q -= std::floor (q);
        p -= c / (dsp::twoPi * m) * readLinear (sine, q);
        p -= std::floor (p);
    }
    return p;
}

} // namespace

void Voice::prepare (float sampleRate) noexcept
{
    prepareBase (sampleRate);
    filterL.prepare (sampleRate);
    filterR.prepare (sampleRate);
}

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    const bool wasActive = isActive();
    startBase (midiNote, vel, legato, c);
    if (! wasActive)
    {
        phase[0] = 0.0f;
        phase[1] = 0.07f; // close start phases: the unison never starts with a cancellation dip
        phase[2] = 0.13f;
        for (int o = 3; o < maxUnison; ++o) phase[o] = std::fmod (0.31f * (float) o + 0.05f, 1.0f);
        if (shared != nullptr && shared->uniVoices != 3)
            for (int o = 0; o < maxUnison; ++o) phase[o] = std::fmod (0.137f * (float) (o * o) + 0.021f * (float) o, 1.0f);
        fmPhase = 0.0f;
        subOsc.reset (0.0f);
        filterL.reset();
        filterR.reset();
        cutoffOct = -1.0f;
        lastPos = lastWarp = -1.0f;
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
    }
}

void Voice::kill() noexcept
{
    killBase();
    filterL.reset();
    filterR.reset();
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    const Bank& bk = *bank;
    const int table = std::clamp (p.table, 0, numTables - 1);
    const float* sine = bk.frame (Classic, 0, Bank::numLevels - 1); // pure one-harmonic sine
    const float lobes = p.warpMode == Twist ? 2.0f : 1.0f;

    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        const float pitch = controlTick (c, n);
        const float f0 = dsp::midiToHz (pitch);
        const float modLevel = modEnv.getLevel();

        // targets for this control block (ramped per sample from the previous block)
        const float pos = dsp::clamp01 (p.position + p.envPosition * modLevel + p.lfoPosition * lfoValue + 0.5f * expr.position);
        // phase distortion widens the spectrum beyond the steepest-slope estimate, so the warp depth
        // fades out over the top octaves (full up to f0 = sr / 50, none from sr / 12)
        const float warpRoom = std::clamp ((1.0f / 12.0f - f0 / sr) / (1.0f / 12.0f - 1.0f / 50.0f), 0.0f, 1.0f);
        const float warp = 0.9f * dsp::clamp01 (p.warp + 0.5f * expr.formant) * warpRoom;
        if (lastPos < 0.0f) { lastPos = pos; lastWarp = warp; }
        const float dPos = (pos - lastPos) / (float) n, dWarp = (warp - lastWarp) / (float) n;
        const float maxWarp = std::max (warp, lastWarp);

        // unison: centre + two detuned side oscillators (classic), or 0.8: a stack of 1, 2, 4 .. 8
        const bool classic = p.uniVoices == 3;
        const float sideGain = 0.75f * std::min (1.0f, p.unison * 3.0f);
        const float cents = 3.0f + 30.0f * p.unison * std::sqrt (p.unison);
        const float ratio = std::pow (2.0f, cents / 1200.0f);
        float freq[maxUnison] = { f0, f0 / ratio, f0 * ratio };
        float oscGain[maxUnison] {}, oscToL[maxUnison] {};
        int numOsc = sideGain > 0.0f ? 3 : 1;
        if (! classic)
        {
            numOsc = std::clamp (p.uniVoices, 1, maxUnison);
            const float g = 1.1f / std::sqrt ((float) numOsc);
            for (int o = 0; o < numOsc; ++o)
            {
                // symmetric spread -1 .. +1, slightly uneven so the beats never line up
                const float d = numOsc == 1 ? 0.0f : ((float) o / (float) (numOsc - 1)) * 2.0f - 1.0f;
                const float dd = d * (1.0f + 0.07f * (float) ((o * 5) % 3 - 1));
                freq[o] = f0 * std::pow (2.0f, cents * dd / 1200.0f);
                oscGain[o] = g;
                oscToL[o] = 0.5f + 0.5f * p.uniWidth * (o % 2 == 0 ? d : -d) * (numOsc == 2 ? -1.0f : 1.0f);
                if (numOsc == 2) oscToL[o] = 0.5f + 0.5f * p.uniWidth * (o == 0 ? 1.0f : -1.0f);
            }
        }
        // 0.8 FM: sine modulator at f0 * ratio, depth in cycles (mod envelope can add), faded out at the top
        const float fmRoom = std::clamp ((1.0f / 10.0f - f0 / sr) / (1.0f / 10.0f - 1.0f / 60.0f), 0.0f, 1.0f);
        const float fmDepth = 0.8f * dsp::clamp01 (p.fm + p.envFm * modLevel + 0.5f * expr.index) * fmRoom;
        const float fmInc = std::min (0.45f, f0 * p.fmRatio / sr);
        const float fmSlope = 1.0f + dsp::twoPi * fmDepth * p.fmRatio;
        int la[maxUnison], lb[maxUnison];
        float lt[maxUnison], inc[maxUnison];
        for (int o = 0; o < numOsc; ++o)
        {
            inc[o] = std::min (0.45f, freq[o] / sr);
            // steepest point of the warped phase, plus room for the sidebands of the phase modulation
            const float slope = (1.0f + maxWarp) * (1.0f + maxWarp) * (1.0f + lobes * maxWarp) * fmSlope;
            pickLevels (inc[o] * slope, la[o], lb[o], lt[o]);
        }
        const float width = 0.4f + 0.6f * p.unison;
        const float norm = classic ? 1.0f / (1.0f + 1.4f * sideGain) : 1.0f;

        // tone: low-pass cutoff with key tracking, mod envelope (+-5 octaves) and LFO timbre (+-2 octaves)
        float octaves = p.keyTrack * (pitch - 60.0f) / 12.0f + c.modEnvAmount * modLevel * 5.0f;
        if (c.lfoDest == LfoDest::Timbre) octaves += lfoValue * 2.0f;
        octaves += (expr.timbre + expr.cutoff) * 3.0f; // per-note expression / modulation matrix (0.7), cutoff target (0.8)
        // cutoff in octaves above 20 Hz, glided over ~4 ms so jumps never ring the filter hard
        const float target = std::clamp (std::log2 (2.0f * std::pow (500.0f, dsp::clamp01 (p.tone))) + octaves, 0.0f, 9.97f);
        cutoffOct = cutoffOct < 0.0f ? target : cutoffOct + (target - cutoffOct) * (1.0f - std::exp (-(float) n / (sr * 0.004f)));
        const float cutoff = 20.0f * std::exp2 (cutoffOct);
        filterL.set (cutoff, 0.92f * p.resonance);
        filterR.set (cutoff, 0.92f * p.resonance);
        const float dcCoeff = 1.0f - dsp::twoPi * 8.0f / sr;
        const float fSub = f0 * 0.5f;

        float panL, panR;
        lfoPan (c, panL, panR);
        const float gain = 0.3f * velocityGain (c) * lfoAmp (c);
        const float exprDrive = std::clamp (expr.drive, 0.0f, 1.0f);

        for (int i = 0; i < n; ++i)
        {
            const float ps = lastPos + dPos * (float) (i + 1);
            const float wp = lastWarp + dWarp * (float) (i + 1);
            const float fpos = ps * (float) (Bank::numFrames - 1);
            const int f0i = std::min ((int) fpos, Bank::numFrames - 2);
            const float ff = fpos - (float) f0i;

            float sL = 0.0f, sR = 0.0f;
            float fmOff = 0.0f;
            if (fmDepth > 0.0f)
            {
                fmOff = fmDepth * readLinear (sine, fmPhase);
                fmPhase += fmInc;
                if (fmPhase >= 1.0f) fmPhase -= 1.0f;
            }
            for (int o = 0; o < numOsc; ++o)
            {
                float ph = wp > 0.0f ? warpPhase (phase[o], wp, lobes, sine) : phase[o];
                if (fmDepth > 0.0f) { ph += fmOff; ph -= std::floor (ph); }
                const float a0 = readLinear (bk.frame (table, f0i, la[o]), ph);
                const float a1 = readLinear (bk.frame (table, f0i + 1, la[o]), ph);
                const float b0 = readLinear (bk.frame (table, f0i, lb[o]), ph);
                const float b1 = readLinear (bk.frame (table, f0i + 1, lb[o]), ph);
                const float va = a0 + ff * (a1 - a0), vb = b0 + ff * (b1 - b0);
                const float v = va + lt[o] * (vb - va);
                phase[o] += inc[o];
                if (phase[o] >= 1.0f) phase[o] -= 1.0f;
                if (! classic) { sL += v * oscGain[o] * oscToL[o] * 2.0f; sR += v * oscGain[o] * (1.0f - oscToL[o]) * 2.0f; }
                else if (o == 0) { sL += v; sR += v; }
                else
                {
                    const float side = v * sideGain * 1.4f;
                    const float toL = o == 1 ? 0.5f + 0.5f * width : 0.5f - 0.5f * width;
                    sL += side * toL;
                    sR += side * (1.0f - toL);
                }
            }
            const float sub = p.sub * subOsc.next (fSub, sr);
            sL = (sL * norm + sub) * 0.8f;
            sR = (sR * norm + sub) * 0.8f;
            if (exprDrive > 0.0f) // 0.8: per-note drive
            {
                sL = dsp::softClip (sL * (1.0f + 6.0f * exprDrive)) / (1.0f + exprDrive);
                sR = dsp::softClip (sR * (1.0f + 6.0f * exprDrive)) / (1.0f + exprDrive);
            }
            // DC blocker (warping a wave can shift its average), then the low-pass
            const float dL = sL - dcX[0] + dcCoeff * dcY[0];
            const float dR = sR - dcX[1] + dcCoeff * dcY[1];
            dcX[0] = sL; dcX[1] = sR;
            dcY[0] = dsp::flush (dL); dcY[1] = dsp::flush (dR);
            const float yL = filterL.process (dL, dsp::Svf::LowPass);
            const float yR = filterR.process (dR, dsp::Svf::LowPass);
            modEnv.next();
            const float env = ampEnv.next() * gain;
            l[done + i] += yL * env * panL * 1.41421356f;
            r[done + i] += yR * env * panR * 1.41421356f;
        }
        lastPos = pos;
        lastWarp = warp;
        done += n;
    }
}
} // namespace wavetable

WavetableEngine::WavetableEngine()
{
    const auto& bank = wavetable::Bank::get();
    for (auto& v : voices)
    {
        v.shared = &shared;
        v.bank = &bank;
    }
}

std::vector<ParamDescriptor> WavetableEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("pos", "Position", "Position", 0.0f, 1.0f, 0.25f));
    d.push_back (continuous ("warp", "Warp", "Warp", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("tone", "Tone", "Klang", 0.0f, 1.0f, 0.75f));
    d.push_back (continuous ("unison", "Unison", "Unisono", 0.0f, 1.0f, 0.2f));
    d.push_back (choice ("table", "Table", "Tabelle", { "Classic", "Harmonic", "Formant", "Digital", "Glass", "Grit" },
                         { "Klassisch", "Harmonisch", "Formant", "Digital", "Glas", "Körnig" }, 0));
    d.push_back (choice ("warpmode", "Warp Mode", "Warp-Art", { "Bend", "Twist" }, { "Biegen", "Drehen" }, 0));
    d.push_back (continuous ("envpos", "Env > Position", "Hüllk. > Position", -1.0f, 1.0f, 0.0f));
    d.push_back (continuous ("reso", "Resonance", "Resonanz", 0.0f, 1.0f, 0.1f));
    d.push_back (continuous ("sub", "Sub", "Sub", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("keytrack", "Key Track", "Key-Tracking", 0.0f, 1.0f, 0.5f));
    // 0.8 (late parameters)
    d.push_back (integer ("univoices", "Unison Voices", "Unisono-Stimmen", 1, wavetable::maxUnison, 3));
    d.push_back (continuous ("uniwidth", "Unison Width", "Unisono-Breite", 0.0f, 1.0f, 0.7f));
    d.push_back (continuous ("fm", "FM Amount", "FM-Menge", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("fmratio", "FM Ratio", "FM-Verhältnis", 0.25f, 8.0f, 1.0f, ParamUnit::None, 0.4f));
    d.push_back (continuous ("lfopos", "LFO > Position", "LFO > Position", -1.0f, 1.0f, 0.0f));
    d.push_back (continuous ("envfm", "Env > FM", "Hüllk. > FM", -1.0f, 1.0f, 0.0f));
    return d;
}

void WavetableEngine::setParam (int i, float v) noexcept
{
    using namespace wavetable;
    switch (i)
    {
        case Position: shared.position = v; break;
        case Warp: shared.warp = v; break;
        case Tone: shared.tone = v; break;
        case Unison: shared.unison = v; break;
        case Table: shared.table = (int) v; break;
        case WarpMode: shared.warpMode = (int) v; break;
        case EnvPosition: shared.envPosition = v; break;
        case Resonance: shared.resonance = v; break;
        case Sub: shared.sub = v; break;
        case KeyTrack: shared.keyTrack = v; break;
        case UniVoices: shared.uniVoices = std::clamp ((int) std::lround (v), 1, maxUnison); break;
        case UniWidth: shared.uniWidth = dsp::clamp01 (v); break;
        case Fm: shared.fm = dsp::clamp01 (v); break;
        case FmRatio: shared.fmRatio = std::clamp (v, 0.25f, 8.0f); break;
        case LfoPosition: shared.lfoPosition = std::clamp (v, -1.0f, 1.0f); break;
        case EnvFm: shared.envFm = std::clamp (v, -1.0f, 1.0f); break;
        default: break;
    }
}
} // namespace ohpe
