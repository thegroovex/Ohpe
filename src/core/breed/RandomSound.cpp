// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "RandomSound.h"

#include "../dsp/Fft.h"
#include "../engines/EngineFactory.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <initializer_list>
#include <string>

namespace ohpe::randomsound
{
namespace
{
constexpr double pi = 3.14159265358979323846;

bool contains (const std::string& s, const char* k) { return s.find (k) != std::string::npos; }

// ------------------------------------------------------------------------------------------------ measuring
/** One biquad (direct form I, double precision). */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double process (double x) noexcept
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

/** The two K-weighting stages (high shelf + high pass) of the loudness standard, designed for any sample rate. */
std::array<Biquad, 2> kWeighting (double sr)
{
    std::array<Biquad, 2> k;
    {
        const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
        const double kk = std::tan (pi * f0 / sr), vh = std::pow (10.0, gainDb / 20.0), vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + kk / q + kk * kk;
        k[0].b0 = (vh + vb * kk / q + kk * kk) / a0;
        k[0].b1 = 2.0 * (kk * kk - vh) / a0;
        k[0].b2 = (vh - vb * kk / q + kk * kk) / a0;
        k[0].a1 = 2.0 * (kk * kk - 1.0) / a0;
        k[0].a2 = (1.0 - kk / q + kk * kk) / a0;
    }
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double kk = std::tan (pi * f0 / sr);
        const double a0 = 1.0 + kk / q + kk * kk;
        k[1].b0 = 1.0;
        k[1].b1 = -2.0;
        k[1].b2 = 1.0;
        k[1].a1 = 2.0 * (kk * kk - 1.0) / a0;
        k[1].a2 = (1.0 - kk / q + kk * kk) / a0;
    }
    return k;
}

/** Peak of the signal 4x oversampled (windowed-sinc interpolation of the three in-between points). */
double truePeak (const std::vector<float>& x)
{
    constexpr int half = 8;
    static const auto coeff = [] {
        std::array<std::array<double, 2 * half>, 3> c {};
        for (int ph = 0; ph < 3; ++ph)
        {
            const double frac = (ph + 1) * 0.25;
            for (int t = 0; t < 2 * half; ++t)
            {
                const double d = (double) (t - half + 1) - frac; // tap position relative to the interpolated point
                const double sinc = std::abs (d) < 1.0e-9 ? 1.0 : std::sin (pi * d) / (pi * d);
                const double w = 0.5 + 0.5 * std::cos (pi * d / (double) half); // Hann over +-half
                c[(size_t) ph][(size_t) t] = sinc * w;
            }
        }
        return c;
    }();
    double peak = 0.0;
    const int n = (int) x.size();
    for (int i = 0; i < n; ++i)
    {
        peak = std::max (peak, (double) std::abs (x[(size_t) i]));
        for (int ph = 0; ph < 3; ++ph)
        {
            double acc = 0.0;
            for (int t = 0; t < 2 * half; ++t)
            {
                const int j = i + t - half + 1;
                if (j >= 0 && j < n) acc += coeff[(size_t) ph][(size_t) t] * (double) x[(size_t) j];
            }
            peak = std::max (peak, std::abs (acc));
        }
    }
    return peak;
}

// ------------------------------------------------------------------------------------------------ rolling
float uniformIn (breed::Rng& rng, float lo, float hi) { return lo + (hi - lo) * rng.uniform(); }
float logUniformIn (breed::Rng& rng, float lo, float hi) { return std::exp (uniformIn (rng, std::log (lo), std::log (hi))); }
/** `chance` of a value in lo..hi, otherwise `off`. Always draws two numbers (stable sequences). */
float rare (breed::Rng& rng, float chance, float off, float lo, float hi)
{
    const float u = rng.uniform(), v = rng.uniform();
    return u < chance ? lo + (hi - lo) * v : off;
}
template <size_t N>
float pick (breed::Rng& rng, const std::array<float, N>& values) { return values[(size_t) rng.below ((int) N)]; }
int weighted (breed::Rng& rng, std::initializer_list<float> weights)
{
    float sum = 0.0f;
    for (float w : weights) sum += w;
    float u = rng.uniform() * sum;
    int i = 0;
    for (float w : weights)
    {
        if (u < w) return i;
        u -= w;
        ++i;
    }
    return i - 1;
}

/** Musical frequency ratios for FM operators and wavetable FM (mostly whole numbers). */
float musicalRatio (breed::Rng& rng, float maxRatio)
{
    static constexpr std::array<float, 10> ratios { 0.5f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };
    float r = 1.0f;
    for (int tries = 0; tries < 4; ++tries)
    {
        r = pick (rng, ratios);
        if (r <= maxRatio) break;
        r = 1.0f;
    }
    return r;
}

/** Engine leaves the roll never changes: level, tuning, voices and the sample choice (as the RANDOM key always did). */
bool keepLeaf (const std::string& leaf)
{
    for (const char* k : { "vol", "level", "gain", "tune", "fine", "oct", "sample", "voices", "root", "inst" })
        if (contains (leaf, k)) return true;
    return false;
}

struct Roller
{
    breed::Rng& rng;
    int analogFilterType = 0;

    /** A new plain value for engine leaf `leaf` of engine `engine`, or the base value. */
    float value (const std::string& engine, const std::string& leaf, const breed::Gene& g, float base)
    {
        auto norm = [&] (float lo, float hi) { return g.fromNorm (uniformIn (rng, lo, hi)); };
        if (engine == "analog")
        {
            if (leaf == "shape") return uniformIn (rng, 0.0f, 1.0f);
            if (leaf == "detune") return uniformIn (rng, 0.0f, 0.45f);
            if (leaf == "cutoff")
            {
                if (analogFilterType == 3) return logUniformIn (rng, 60.0f, 900.0f);    // high pass: keep the body
                if (analogFilterType == 2) return logUniformIn (rng, 500.0f, 3500.0f);  // band pass: the voice range
                return logUniformIn (rng, 500.0f, 12000.0f);                            // low pass: never closed
            }
            if (leaf == "reso") return uniformIn (rng, 0.0f, analogFilterType == 2 ? 0.5f : 0.7f);
            if (leaf == "oscmix") return uniformIn (rng, 0.1f, 0.9f);
            if (leaf == "sub") return uniformIn (rng, 0.0f, 0.55f);
            if (leaf == "noise") return rare (rng, 0.12f, 0.0f, 0.02f, 0.12f);
            if (leaf == "crackle") return rare (rng, 0.08f, 0.0f, 0.02f, 0.15f);
            if (leaf == "ftype") return (float) analogFilterType;
            if (leaf == "drive") return uniformIn (rng, 0.0f, 0.55f);
            if (leaf == "keytrack") return uniformIn (rng, 0.3f, 1.0f);
            if (leaf == "osclevel") return base < 0.5f ? 1.0f : base; // never sub / noise only
        }
        else if (engine == "fm")
        {
            if (leaf == "algo") return (float) rng.below (8);
            if (leaf == "modratio") return musicalRatio (rng, 8.0f);
            if (leaf == "index") return uniformIn (rng, 0.05f, 0.55f);
            if (leaf == "feedback") return rng.uniform() < 0.75f ? uniformIn (rng, 0.0f, 0.15f) : uniformIn (rng, 0.15f, 0.35f);
            if (contains (leaf, "ratio")) return musicalRatio (rng, 8.0f);
            if (leaf == "velidx") return uniformIn (rng, 0.0f, 0.4f);
        }
        else if (engine == "wavetable")
        {
            if (leaf == "pos") return uniformIn (rng, 0.0f, 1.0f);
            if (leaf == "warp") return uniformIn (rng, 0.0f, 0.65f);
            if (leaf == "tone") return uniformIn (rng, 0.35f, 1.0f);
            if (leaf == "unison") return uniformIn (rng, 0.0f, 0.55f);
            if (leaf == "table") return (float) weighted (rng, { 20.0f, 20.0f, 15.0f, 15.0f, 15.0f, 7.0f });
            if (leaf == "warpmode") return (float) rng.below (2);
            if (leaf == "envpos") return uniformIn (rng, -0.5f, 0.5f);
            if (leaf == "reso") return uniformIn (rng, 0.0f, 0.55f);
            if (leaf == "sub") return uniformIn (rng, 0.0f, 0.5f);
            if (leaf == "keytrack") return uniformIn (rng, 0.3f, 1.0f);
            if (leaf == "uniwidth") return uniformIn (rng, 0.2f, 0.9f);
            if (leaf == "fm") return rng.uniform() < 0.7f ? uniformIn (rng, 0.0f, 0.12f) : uniformIn (rng, 0.12f, 0.4f);
            if (leaf == "fmratio") return musicalRatio (rng, 4.0f);
            if (leaf == "lfopos") return uniformIn (rng, -0.3f, 0.3f);
            if (leaf == "envfm") return rare (rng, 0.15f, 0.0f, -0.2f, 0.2f); // an FM sweep sounds like a slide: rare, small
        }
        else if (engine == "pluck")
        {
            if (leaf == "material") return uniformIn (rng, 0.0f, 1.0f);
            if (leaf == "damping") return uniformIn (rng, 0.1f, 0.7f);
            if (leaf == "bright") return uniformIn (rng, 0.25f, 0.9f);
            if (leaf == "body") return uniformIn (rng, 0.0f, 0.7f);
            if (leaf == "excite") return uniformIn (rng, 0.3f, 0.85f);
            if (leaf == "pickpos") return uniformIn (rng, 0.05f, 0.4f);
            if (leaf == "bodysize") return uniformIn (rng, 0.0f, 1.0f);
        }
        else if (engine == "cluster")
        {
            if (leaf == "detune") return uniformIn (rng, 0.05f, 0.55f);
            if (leaf == "spread") return uniformIn (rng, 0.2f, 0.9f);
            if (leaf == "tone") return uniformIn (rng, 0.35f, 1.0f);
            if (leaf == "chord") return (float) rng.below ((int) std::lround (g.maxValue - g.minValue) + 1) + g.minValue;
            if (leaf == "stack") return (float) (3 + rng.below (std::max (1, (int) g.maxValue - 2)));
            if (leaf == "shape") return uniformIn (rng, 0.0f, 1.0f);
            if (leaf == "side") return uniformIn (rng, 0.3f, 0.9f);
            if (leaf == "reso") return uniformIn (rng, 0.0f, 0.55f);
            if (leaf == "keytrack") return uniformIn (rng, 0.3f, 1.0f);
            if (leaf == "drift") return uniformIn (rng, 0.0f, 0.35f);
        }
        else if (engine == "grain")
        {
            if (leaf == "pos") return uniformIn (rng, 0.0f, 0.6f);
            if (leaf == "size") return logUniformIn (rng, 30.0f, 300.0f);
            if (leaf == "density") return logUniformIn (rng, 8.0f, 60.0f);
            if (leaf == "spray") return rng.uniform() < 0.8f ? uniformIn (rng, 0.0f, 0.25f) : uniformIn (rng, 0.25f, 0.4f);
            if (leaf == "pitch")
            {
                static constexpr std::array<float, 8> steps { 0.0f, 0.0f, 0.0f, 12.0f, -12.0f, 7.0f, -5.0f, 0.0f };
                return pick (rng, steps);
            }
            if (leaf == "reverse") return uniformIn (rng, 0.0f, 0.4f);
            if (leaf == "spread") return uniformIn (rng, 0.2f, 0.9f);
            if (leaf == "scan") return uniformIn (rng, -0.5f, 0.5f);
            if (leaf == "source") return base; // tape may be empty: the source stays
            if (leaf == "tone") return uniformIn (rng, 0.4f, 1.0f);
        }
        else if (engine == "vocal")
        {
            if (leaf == "vowel") return uniformIn (rng, 0.0f, 4.0f);
            if (leaf == "formant") return uniformIn (rng, -5.0f, 5.0f);
            if (leaf == "breath") return rng.uniform() < 0.85f ? uniformIn (rng, 0.03f, 0.15f) : uniformIn (rng, 0.15f, 0.2f);
            if (leaf == "tension") return uniformIn (rng, 0.2f, 0.8f);
            if (leaf == "vibdepth") return uniformIn (rng, 0.0f, 0.35f);
            if (leaf == "vibrate") return uniformIn (rng, 3.5f, 7.0f);
            if (leaf == "jitter") return uniformIn (rng, 0.0f, 0.25f);
        }
        else if (engine == "sampler")
        {
            if (leaf == "start") return uniformIn (rng, 0.0f, 0.2f);
            if (leaf == "tone") return uniformIn (rng, 0.45f, 1.0f);
            if (leaf == "reverse") return rng.uniform() < 0.1f ? 1.0f : 0.0f;
            return base; // loop points, loop / slice mode, slice keys: how the sample is played stays
        }
        // any other (future) parameter: noise-like ones rare and low, risky ones in their lower part, the rest
        // in the musical middle
        if (contains (leaf, "noise") || contains (leaf, "hiss") || contains (leaf, "crackle") || contains (leaf, "breath"))
            return g.discrete() ? g.minValue : rare (rng, 0.12f, g.minValue, g.minValue, g.minValue + 0.12f * (g.maxValue - g.minValue));
        if (g.discrete())
            return g.fromNorm (rng.uniform());
        if (contains (leaf, "reso") || contains (leaf, "feedback") || contains (leaf, "drive") || contains (leaf, "jitter"))
            return norm (0.0f, 0.55f);
        return norm (0.15f, 0.85f);
    }
};

float geneValue (const breed::Schema& s, const breed::Genome& g, const char* id, float fallback = 0.0f)
{
    const int k = s.indexOf (id);
    return k >= 0 ? g[(size_t) k] : fallback;
}

void setGene (const breed::Schema& s, breed::Genome& g, const char* id, float v)
{
    const int k = s.indexOf (id);
    if (k >= 0) g[(size_t) k] = s.gene (k).clamp (v);
}
} // namespace

// ------------------------------------------------------------------------------------------------ measure
Loudness measure (const std::vector<float>& l, const std::vector<float>& r, double sr, float holdSeconds)
{
    Loudness m;
    const size_t n = std::min (l.size(), r.size());
    if (n == 0 || sr <= 0.0) return m;
    std::vector<float> kl (n), kr (n);
    auto fl = kWeighting (sr), fr = kWeighting (sr);
    for (size_t i = 0; i < n; ++i)
    {
        float a = l[i], b = r[i];
        if (! std::isfinite (a) || ! std::isfinite (b))
        {
            ++m.nonFinite;
            a = b = 0.0f;
        }
        kl[i] = (float) fl[1].process (fl[0].process ((double) a));
        kr[i] = (float) fr[1].process (fr[0].process ((double) b));
    }
    // gated loudness: 400 ms blocks, 75 % overlap, absolute gate -70 LUFS, relative gate -10 LU
    const size_t block = (size_t) (0.4 * sr), hop = std::max<size_t> (1, block / 4);
    std::vector<double> z;
    if (n >= block)
        for (size_t st = 0; st + block <= n; st += hop)
        {
            double e = 0.0;
            for (size_t i = st; i < st + block; ++i) e += (double) kl[i] * kl[i] + (double) kr[i] * kr[i];
            z.push_back (e / (double) block);
        }
    else
    {
        double e = 0.0;
        for (size_t i = 0; i < n; ++i) e += (double) kl[i] * kl[i] + (double) kr[i] * kr[i];
        z.push_back (e / (double) n);
    }
    auto lufsOf = [] (double power) { return -0.691 + 10.0 * std::log10 (std::max (power, 1.0e-30)); };
    for (double p : z) m.momentaryMax = std::max (m.momentaryMax, (float) lufsOf (p));
    double sum = 0.0;
    int count = 0;
    for (double p : z)
        if (lufsOf (p) > -70.0) { sum += p; ++count; }
    if (count > 0)
    {
        const double relGate = lufsOf (sum / count) - 10.0;
        double s2 = 0.0;
        int c2 = 0;
        for (double p : z)
            if (lufsOf (p) > -70.0 && lufsOf (p) > relGate) { s2 += p; ++c2; }
        if (c2 > 0) m.lufs = (float) lufsOf (s2 / c2);
    }

    // true peak (sanitised copies)
    std::vector<float> cl (l.begin(), l.begin() + (std::ptrdiff_t) n), cr (r.begin(), r.begin() + (std::ptrdiff_t) n);
    for (size_t i = 0; i < n; ++i)
    {
        if (! std::isfinite (cl[i])) cl[i] = 0.0f;
        if (! std::isfinite (cr[i])) cr[i] = 0.0f;
    }
    const double tp = std::max (truePeak (cl), truePeak (cr));
    m.truePeakDb = tp > 1.0e-10 ? (float) (20.0 * std::log10 (tp)) : -200.0f;

    // spectral flatness of the held chord (after the attack): averaged power spectrum, 100 Hz .. 10 kHz
    constexpr size_t fftSize = 2048;
    const size_t from = std::min (n, (size_t) (0.1 * sr)), to = std::min (n, (size_t) ((double) holdSeconds * sr));
    std::vector<double> power (fftSize / 2, 0.0);
    int frames = 0;
    std::vector<std::complex<double>> buf (fftSize);
    for (size_t st = from; st + fftSize <= to; st += fftSize / 2)
    {
        for (size_t i = 0; i < fftSize; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) fftSize);
            buf[i] = { 0.5 * ((double) cl[st + i] + (double) cr[st + i]) * w, 0.0 };
        }
        dsp::fft (buf, -1);
        for (size_t k = 0; k < fftSize / 2; ++k) power[k] += std::norm (buf[k]);
        ++frames;
    }
    if (frames > 0)
    {
        const size_t k0 = std::max<size_t> (1, (size_t) (100.0 * fftSize / sr));
        const size_t k1 = std::min (fftSize / 2 - 1, (size_t) (std::min (10000.0, 0.45 * sr) * fftSize / sr));
        double logSum = 0.0, linSum = 0.0;
        int bins = 0;
        for (size_t k = k0; k <= k1; ++k)
        {
            const double p = power[k] / frames + 1.0e-24;
            logSum += std::log (p);
            linSum += p;
            ++bins;
        }
        const double mean = linSum / std::max (1, bins);
        if (bins > 0 && mean > 1.0e-14) m.flatness = (float) std::clamp (std::exp (logSum / bins) / mean, 0.0, 1.0);
    }
    return m;
}

// ------------------------------------------------------------------------------------------------ roll
breed::Genome roll (const breed::Schema& s, const breed::Genome& base, int engineType, uint64_t seed)
{
    breed::Genome g = base;
    if ((int) g.size() != s.size()) return g;
    breed::Rng rng (breed::Rng::mix (seed, 0x0e1ce5eedULL));
    Roller roller { rng };
    const std::string engine = engineType >= 0 && engineType < numEngineTypes ? engineTypeInfo ((EngineType) engineType).id : "";
    if (engine == "analog") roller.analogFilterType = weighted (rng, { 45.0f, 35.0f, 12.0f, 8.0f });

    const std::string pre = engine + ".";
    for (int k = 0; k < s.size(); ++k)
    {
        const auto& gene = s.gene (k);
        if (gene.owner != breed::Gene::OwnEngine || gene.ownerA != engineType || gene.id.rfind (pre, 0) != 0) continue;
        const auto leaf = gene.id.substr (pre.size());
        if (keepLeaf (leaf)) continue;
        g[(size_t) k] = gene.clamp (roller.value (engine, leaf, gene, base[(size_t) k]));
    }

    // portamento and pitch envelope: a new sound never slides into its first note
    const float uGlide = rng.uniform(), glideTime = uniformIn (rng, 0.03f, 0.08f);
    if (uGlide < 0.08f)
    {
        setGene (s, g, "glide", glideTime); // a short glide between overlapping (legato) notes only
        setGene (s, g, "glide.mode", 1.0f);  // Legato
    }
    else
        setGene (s, g, "glide", 0.0f);
    setGene (s, g, "glide.press", 0.0f);
    setGene (s, g, "polyglide", 0.0f);
    const float uEnv = rng.uniform(), envAmt = uniformIn (rng, 1.0f, 5.0f), envSign = rng.uniform(), envTime = uniformIn (rng, 0.008f, 0.04f);
    if (uEnv < 0.04f)
    {
        setGene (s, g, "pitchenv.amt", envSign < 0.7f ? envAmt : -envAmt); // a short pitch blip on the attack
        setGene (s, g, "pitchenv.time", envTime);
    }
    else
        setGene (s, g, "pitchenv.amt", 0.0f);
    return g;
}

bool glideAtStart (const breed::Schema& s, const breed::Genome& g)
{
    const bool glide = geneValue (s, g, "glide") > 0.001f && std::lround (geneValue (s, g, "glide.mode")) != 1;
    return glide || std::abs (geneValue (s, g, "pitchenv.amt")) > 0.05f;
}

float noiseParamLevel (const breed::Schema& s, const breed::Genome& g, int engineType)
{
    const std::string engine = engineType >= 0 && engineType < numEngineTypes ? engineTypeInfo ((EngineType) engineType).id : "";
    if (engine == "analog") return std::max (geneValue (s, g, "analog.noise"), geneValue (s, g, "analog.crackle"));
    if (engine == "vocal") return geneValue (s, g, "vocal.breath");
    return 0.0f;
}

// ------------------------------------------------------------------------------------------------ make
Result make (breed::Audition& audition, const breed::Genome& base, int engineType, uint64_t seed)
{
    const auto& s = audition.schema();
    const int gainK = s.gainGene();
    const double sr = audition.sampleRate();
    std::vector<float> l, r;
    Result res;
    res.genome = base;

    // slow attacks are heard at their full level: the chord is held a little past the attack (0.7 .. 2 s)
    const float hold = std::clamp (geneValue (s, base, "amp.a", 0.0f) + 0.45f, 0.7f, 2.0f);
    breed::Genome best;
    Loudness bestM;
    double bestPenalty = 1.0e30;
    int tooQuiet = 0; // candidates that only failed by needing more gain than the part has
    float bestShortfall = 0.0f; // dB the best candidate stays under the target even with the gain at its top
    for (int a = 0; a < maxAttempts + maxExtraAttempts; ++a)
    {
        if (audition.aborted()) break;
        // past the normal attempts only while the best candidate would still play far too quiet
        if (a >= maxAttempts && (best.empty() || bestPenalty >= 1.0e6 || bestShortfall <= farTooQuietDb)) break;
        auto cand = roll (s, base, engineType, breed::Rng::mix (seed, (uint64_t) a));
        audition.renderAudio (cand, l, r, hold);
        ++res.renders;
        res.attempts = a + 1;
        const auto m = measure (l, r, sr, hold);
        const float gain = gainK >= 0 ? cand[(size_t) gainK] : 0.0f;
        const float needed = gain + (targetLufs - m.momentaryMax);
        double penalty = 0.0;
        if (m.nonFinite > 0) penalty += 1.0e9;
        if (m.momentaryMax < silentLufs) penalty += 1.0e6 + (silentLufs - m.momentaryMax);
        penalty += std::max (0.0f, m.flatness - maxFlatness) * 1000.0;
        penalty += std::max (0.0f, needed - maxGainDb) * 10.0;
        if (penalty < bestPenalty)
        {
            bestPenalty = penalty;
            best = std::move (cand);
            bestM = m;
            bestShortfall = std::max (0.0f, needed - maxGainDb);
        }
        if (penalty <= 0.0) break;
        // the part's own chain (a compressor first in the rack, say) caps it: more rolls rarely help - unless even
        // the best candidate would stay far under the target, then a few more (seeded) rolls look for a louder one
        if (penalty < 1.0e6 && m.flatness <= maxFlatness && ++tooQuiet >= 2 && bestShortfall <= farTooQuietDb) break;
    }
    if (best.empty() || bestM.nonFinite > 0 || bestM.momentaryMax < silentLufs - 30.0f)
    {
        res.loudness = bestM;
        return res; // nothing playable: the sound stays as it was
    }
    res.usable = bestPenalty <= 0.0;

    // level: the factory median, true peak below the ceiling. Effects after the gain (compressors, drive, gates)
    // need not follow it 1:1: each correction steps along the measured response (secant between the renders that
    // bracket the target, otherwise the slope of the last step), and the best render wins.
    auto m = bestM;
    if (gainK >= 0)
    {
        const auto& gene = s.gene (gainK);
        const float top = std::min (gene.maxValue, maxGainDb);
        struct Point
        {
            float gain;
            Loudness m;
        };
        std::vector<Point> pts { { best[(size_t) gainK], bestM } };
        auto peakOk = [] (const Loudness& x) { return x.truePeakDb <= truePeakCeilingDb; };
        for (int pass = 0; pass < 5; ++pass)
        {
            const auto& last = pts.back();
            if (peakOk (last.m) && std::abs (targetLufs - last.m.momentaryMax) < 0.75f) break;
            const Point* lo = nullptr; // loudest render under the target (peak fine)
            const Point* hi = nullptr; // quietest render over the target (or over the peak ceiling)
            for (const auto& q : pts)
            {
                const bool over = q.m.momentaryMax > targetLufs || ! peakOk (q.m);
                if (! over && (lo == nullptr || q.gain > lo->gain)) lo = &q;
                if (over && (hi == nullptr || q.gain < hi->gain)) hi = &q;
            }
            float next;
            if (lo != nullptr && hi != nullptr && hi->gain > lo->gain + 0.1f)
            {
                const float dl = hi->m.momentaryMax - lo->m.momentaryMax;
                next = peakOk (hi->m) && dl > 0.1f ? lo->gain + (targetLufs - lo->m.momentaryMax) * (hi->gain - lo->gain) / dl
                                                    : 0.5f * (lo->gain + hi->gain);
                next = std::clamp (next, lo->gain + 0.05f, hi->gain - 0.05f);
            }
            else
            {
                float slope = 1.0f;
                if (pts.size() >= 2)
                {
                    const auto& prev = pts[pts.size() - 2];
                    if (std::abs (last.gain - prev.gain) > 0.5f)
                        slope = std::clamp ((last.m.momentaryMax - prev.m.momentaryMax) / (last.gain - prev.gain), 0.25f, 1.5f);
                }
                float delta = (targetLufs - last.m.momentaryMax) / slope;
                delta = std::min (delta, (truePeakCeilingDb - 0.2f - last.m.truePeakDb) / slope);
                next = last.gain + delta;
            }
            next = std::clamp (next, gene.minValue, top);
            bool seen = false;
            for (const auto& q : pts) seen |= std::abs (q.gain - next) < 0.05f;
            if (seen) break;
            best[(size_t) gainK] = next;
            audition.renderAudio (best, l, r, hold);
            ++res.renders;
            pts.push_back ({ next, measure (l, r, sr, hold) });
        }
        // the best render: peak fine and nearest to the target (none fine: the lowest peak)
        const Point* pick = nullptr;
        for (const auto& q : pts)
        {
            if (pick == nullptr) { pick = &q; continue; }
            const bool qa = peakOk (q.m), pa = peakOk (pick->m);
            if (qa != pa) { if (qa) pick = &q; continue; }
            if (qa ? std::abs (q.m.momentaryMax - targetLufs) < std::abs (pick->m.momentaryMax - targetLufs)
                   : q.m.truePeakDb < pick->m.truePeakDb)
                pick = &q;
        }
        best[(size_t) gainK] = pick->gain;
        m = pick->m;
        res.gainDb = pick->gain;
        // gain at the top and still quiet: the sound it was rolled from is the reference (its chain caps the level)
        if (res.gainDb >= top - 0.05f && m.momentaryMax < targetLufs - 3.0f)
        {
            audition.renderAudio (base, l, r, hold);
            ++res.renders;
            res.baseMomentary = measure (l, r, sr, hold).momentaryMax;
        }
    }
    res.genome = std::move (best);
    res.loudness = m;
    return res;
}
} // namespace ohpe::randomsound
