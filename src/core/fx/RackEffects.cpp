// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "RackEffects.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ohpe
{
namespace rackdsp
{
// ------------------------------------------------------------------------------------------------ biquad
void Biquad::set (int type, float hz, float q, float gainDb, float sr) noexcept
{
    const float w = dsp::twoPi * std::clamp (hz, 5.0f, sr * 0.49f) / sr;
    const float cw = std::cos (w), sw = std::sin (w);
    const float alpha = sw / (2.0f * std::max (0.05f, q));
    const float A = std::pow (10.0f, gainDb / 40.0f);
    float n0 = 1, n1 = 0, n2 = 0, d0 = 1, d1 = 0, d2 = 0;
    switch (type)
    {
        case LowShelf:
        {
            const float s = 2.0f * std::sqrt (A) * alpha;
            n0 = A * ((A + 1) - (A - 1) * cw + s);
            n1 = 2 * A * ((A - 1) - (A + 1) * cw);
            n2 = A * ((A + 1) - (A - 1) * cw - s);
            d0 = (A + 1) + (A - 1) * cw + s;
            d1 = -2 * ((A - 1) + (A + 1) * cw);
            d2 = (A + 1) + (A - 1) * cw - s;
            break;
        }
        case HighShelf:
        {
            const float s = 2.0f * std::sqrt (A) * alpha;
            n0 = A * ((A + 1) + (A - 1) * cw + s);
            n1 = -2 * A * ((A - 1) + (A + 1) * cw);
            n2 = A * ((A + 1) + (A - 1) * cw - s);
            d0 = (A + 1) - (A - 1) * cw + s;
            d1 = 2 * ((A - 1) - (A + 1) * cw);
            d2 = (A + 1) - (A - 1) * cw - s;
            break;
        }
        case LowPass:
            n0 = (1 - cw) * 0.5f; n1 = 1 - cw; n2 = n0; d0 = 1 + alpha; d1 = -2 * cw; d2 = 1 - alpha;
            break;
        case HighPass:
            n0 = (1 + cw) * 0.5f; n1 = -(1 + cw); n2 = n0; d0 = 1 + alpha; d1 = -2 * cw; d2 = 1 - alpha;
            break;
        default: // peak
            n0 = 1 + alpha * A; n1 = -2 * cw; n2 = 1 - alpha * A; d0 = 1 + alpha / A; d1 = -2 * cw; d2 = 1 - alpha / A;
            break;
    }
    b0 = n0 / d0; b1 = n1 / d0; b2 = n2 / d0; a1 = d1 / d0; a2 = d2 / d0;
}

float Biquad::magnitudeDb (float hz, float sr) const noexcept
{
    const double w = 2.0 * 3.14159265358979 * hz / sr;
    const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
    const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    const double m = std::sqrt ((nr * nr + ni * ni) / std::max (1.0e-30, dr * dr + di * di));
    return (float) (20.0 * std::log10 (std::max (1.0e-9, m)));
}

// ------------------------------------------------------------------------------------------------ oversampler
void Oversampler4::prepare (float sampleRate) noexcept
{
    // 8th-order Butterworth low-pass at 0.45 x the base rate, running at 4x (Q of the four sections)
    static constexpr float qs[4] = { 0.50979558f, 0.60134489f, 0.89997622f, 2.5629154f };
    const float fs = sampleRate * 4.0f, fc = sampleRate * 0.45f;
    for (int i = 0; i < 4; ++i)
    {
        upF[(size_t) i].set (Biquad::LowPass, fc, qs[i], 0.0f, fs);
        downF[(size_t) i].set (Biquad::LowPass, fc, qs[i], 0.0f, fs);
    }
    reset();
}

void Oversampler4::reset() noexcept
{
    for (auto& f : upF) f.reset();
    for (auto& f : downF) f.reset();
}

void Oversampler4::up (float x, float* out) noexcept
{
    for (int k = 0; k < 4; ++k)
    {
        float y = k == 0 ? x * 4.0f : 0.0f; // zero stuffing, gain 4 restores the level
        for (auto& f : upF) y = f.process (y);
        out[k] = y;
    }
}

float Oversampler4::down (const float* in) noexcept
{
    float y = 0.0f;
    for (int k = 0; k < 4; ++k)
    {
        float v = in[k];
        for (auto& f : downF) v = f.process (v);
        if (k == 0) y = v;
    }
    return y;
}

// ------------------------------------------------------------------------------------------------ delay line
void DelayLine::prepare (int maxSamples)
{
    int n = 4;
    while (n < maxSamples + 4) n <<= 1;
    buf.assign ((size_t) n, 0.0f);
    mask = n - 1;
    w = 0;
}

void DelayLine::reset() noexcept
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    w = 0;
}

float DelayLine::read (float d) const noexcept
{
    d = std::clamp (d, 1.0f, (float) (mask - 3));
    const float pos = (float) w - d;
    const int i = (int) std::floor (pos);
    const float f = pos - (float) i;
    const float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)], y2 = buf[(size_t) ((i + 1) & mask)],
                y3 = buf[(size_t) ((i + 2) & mask)];
    // 4-point Hermite
    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

double syncBeats (int division) noexcept
{
    static constexpr double beats[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 3.0, 1.5, 0.75, 0.375, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return division > 0 && division < (int) (sizeof (beats) / sizeof (beats[0])) ? beats[division] : 0.0;
}
std::vector<std::string> syncNamesEn()
{
    return { "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2.", "1/4.", "1/8.", "1/16.", "1/2T", "1/4T", "1/8T", "1/16T" };
}
std::vector<std::string> syncNamesDe()
{
    auto v = syncNamesEn();
    v[0] = "Frei";
    return v;
}
} // namespace rackdsp

using namespace rackdsp;

namespace
{
inline float dcBlock (float x, float& x1, float& y1, float c = 0.9995f) noexcept
{
    const float y = x - x1 + c * y1;
    x1 = x;
    y1 = dsp::flush (y);
    return y;
}
} // namespace

// ================================================================================================ Hyper
std::vector<ParamDescriptor> HyperEffect::getParamDescriptors() const
{
    using namespace pd;
    return { continuous ("rate", "Hyper Rate", "Hyper-Tempo", 0.05f, 5.0f, 0.6f, ParamUnit::Hertz, 0.4f),
             continuous ("detune", "Hyper Detune", "Hyper-Verstimmung", 0.0f, 1.0f, 0.35f),
             integer ("voices", "Hyper Voices", "Hyper-Stimmen", 2, maxVoices, 5),
             continuous ("spread", "Hyper Spread", "Hyper-Breite", 0.0f, 1.0f, 0.8f),
             continuous ("dimension", "Dimension", "Dimension", 0.0f, 1.0f, 0.4f),
             toggle ("retrig", "Hyper Retrigger", "Hyper-Neustart", false),
             continuous ("mix", "Hyper Mix", "Hyper-Mischung", 0.0f, 1.0f, 0.5f) };
}

void HyperEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    lineL.prepare ((int) (0.05f * sr));
    lineR.prepare ((int) (0.05f * sr));
    for (auto& d : dim) d.prepare ((int) (0.03f * sr));
    tapC = 1.0f - std::exp (-1.0f / (0.002f * sr));
    depthG.prepare (sr, 0.03f);
    mixG.prepare (sr, 0.02f);
    reset();
}

void HyperEffect::reset() noexcept
{
    lineL.reset();
    lineR.reset();
    for (auto& d : dim) d.reset();
    resetPhases();
    tapSnap = true;
    depthG.snap (detune);
    mixG.snap (mix);
}

void HyperEffect::noteOn() noexcept
{
    // 0.8 punch pass: the voices restart only with Retrigger on (every note used to jump the taps: a click at
    // each new chord), and the taps glide to their new position (see process) instead of jumping
    if (retrigger) resetPhases();
}

void HyperEffect::resetPhases() noexcept
{
    // fixed, irrational-ish rate ratios and spread phases: the voices never line up
    static constexpr float ratios[maxVoices] = { 1.0f, 1.13f, 0.87f, 1.31f, 0.71f, 1.53f, 0.61f };
    for (int v = 0; v < maxVoices; ++v)
    {
        phase[(size_t) v] = (float) v / (float) maxVoices;
        rateMul[(size_t) v] = ratios[v];
    }
}

void HyperEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Rate: rate = v; break;
        case Detune: detune = v; depthG.t = v; break;
        case Voices: voices = std::clamp ((int) std::lround (v), 2, maxVoices); break;
        case Spread: spread = v; break;
        case Dimension: dimension = v; break;
        case Retrigger: retrigger = v >= 0.5f; break;
        case Mix: mix = v; mixG.t = v; break;
        default: break;
    }
}

void HyperEffect::process (float* l, float* r, int n) noexcept
{
    // voice pans: alternate left / right, spread outward
    for (int v = 0; v < voices; ++v)
    {
        const float pos = voices > 1 ? ((float) v / (float) (voices - 1)) * 2.0f - 1.0f : 0.0f;
        const float p = std::clamp (((v % 2) ? -pos : pos) * spread, -1.0f, 1.0f);
        dsp::panGains (p, panL[(size_t) v], panR[(size_t) v]);
    }
    const float norm = 1.0f / std::sqrt ((float) voices);
    const float inc = rate / sr;
    const float dimMs = 7.0f + 9.0f * dimension;
    for (int i = 0; i < n; ++i)
    {
        const float depth = depthG.next(), m = mixG.next();
        const float inL = l[i], inR = r[i];
        lineL.push (inL);
        lineR.push (inR);
        float wl = 0.0f, wr = 0.0f;
        for (int v = 0; v < voices; ++v)
        {
            float& ph = phase[(size_t) v];
            ph += inc * rateMul[(size_t) v];
            if (ph >= 1.0f) ph -= 1.0f;
            // triangle motion of the tap: a constant pitch offset up, then down (Doppler detune)
            const float tri = 1.0f - 4.0f * std::abs (ph - 0.5f);
            const float dMs = 8.0f + 3.0f * (float) v + depth * 6.0f * tri;
            float& tap = tapSm[(size_t) v];
            tap = tapSnap ? dMs * 0.001f * sr : tap + (dMs * 0.001f * sr - tap) * tapC; // ~2 ms glide
            const float dSamples = tap;
            const float x = (v % 2 ? lineR : lineL).read (dSamples) * 0.6f + (v % 2 ? lineL : lineR).read (dSamples) * 0.4f;
            wl += x * panL[(size_t) v];
            wr += x * panR[(size_t) v];
        }
        tapSnap = false;
        wl *= norm * 1.41f;
        wr *= norm * 1.41f;
        // dimension: cross-fed short delays, opposite polarity - width without a hole in the middle
        if (dimension > 0.0f)
        {
            const float dl = dim[0].read (dimMs * 0.001f * sr), dr = dim[1].read (dimMs * 0.00137f * sr);
            dim[0].push (wr);
            dim[1].push (wl);
            wl += 0.5f * dimension * (dl - dr);
            wr += 0.5f * dimension * (dr - dl);
        }
        l[i] = inL + (wl - inL) * m;
        r[i] = inR + (wr - inR) * m;
    }
}

// ================================================================================================ Distortion
std::vector<ParamDescriptor> DistortionEffect::getParamDescriptors() const
{
    using namespace pd;
    return { choice ("mode", "Distortion Mode", "Verzerrungs-Modus",
                     { "Tube", "Soft Clip", "Hard Clip", "Diode", "Linear Fold", "Sine Fold", "Asymmetric", "Downsample" },
                     { "Röhre", "Weich", "Hart", "Diode", "Linear falten", "Sinus falten", "Asymmetrisch", "Downsampling" }, Tube),
             continuous ("drive", "Distortion Drive", "Verzerrung Drive", 0.0f, 1.0f, 0.4f),
             continuous ("bias", "Distortion Bias", "Verzerrung Bias", -1.0f, 1.0f, 0.0f),
             choice ("prefilter", "Distortion Pre-Filter", "Verzerrung Vorfilter", { "Off", "Low-pass", "Band-pass", "High-pass" },
                     { "Aus", "Tiefpass", "Bandpass", "Hochpass" }, 0),
             continuous ("prefreq", "Distortion Pre-Filter Freq", "Verzerrung Vorfilter-Frequenz", 40.0f, 18000.0f, 800.0f, ParamUnit::Hertz, 0.3f),
             continuous ("tone", "Distortion Tone", "Verzerrung Klang", 0.0f, 1.0f, 0.8f),
             continuous ("output", "Distortion Output", "Verzerrung Ausgang", -24.0f, 12.0f, 0.0f, ParamUnit::Decibel),
             continuous ("mix", "Distortion Mix", "Verzerrung Mischung", 0.0f, 1.0f, 1.0f) };
}

float DistortionEffect::transfer (int mode, float drive, float bias, float x) noexcept
{
    const float g = std::pow (10.0f, drive * 36.0f / 20.0f); // 0 .. +36 dB
    float u = x * g + bias * 0.5f;
    float y;
    switch (mode)
    {
        case SoftClip: y = std::tanh (u); break;
        case HardClip: y = std::clamp (u, -1.0f, 1.0f); break;
        case Diode: y = u >= 0.0f ? 1.0f - std::exp (-u) : -0.2f * (1.0f - std::exp (u)); break; // conducting one way, leaking the other
        case LinearFold:
        {
            // triangle fold: reflects at +-1
            float t = (u + 1.0f) * 0.25f;
            t -= std::floor (t);
            y = 1.0f - 4.0f * std::abs (t - 0.5f);
            break;
        }
        case SineFold: y = std::sin (u * dsp::pi * 0.5f); break;
        case Asymmetric: y = u >= 0.0f ? std::tanh (u) : std::tanh (u * 0.45f) / 0.45f * 0.6f; break;
        case Downsample: y = std::clamp (u, -1.0f, 1.0f); break;
        default: // tube: asymmetric soft saturation with gentle even harmonics
            y = u >= 0.0f ? std::tanh (u) : std::tanh (u * 0.85f) * 1.05f;
            break;
    }
    return y; // (the static offset of bias / asymmetry is removed by the DC blocker in the signal path)
}

void DistortionEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    for (auto& c : ch) c.os.prepare (sr);
    driveG.prepare (sr, 0.02f);
    outG.prepare (sr, 0.02f);
    mixG.prepare (sr, 0.02f);
    preG.prepare (sr, 0.01f);
    postG.prepare (sr, 0.01f);
    reset();
}

void DistortionEffect::reset() noexcept
{
    for (auto& c : ch)
    {
        c.pre.reset();
        c.post.reset();
        c.os.reset();
        c.dcX = c.dcY = c.held = c.holdPhase = 0.0f;
    }
    driveG.v = driveG.t;
    outG.v = outG.t;
    mixG.v = mixG.t;
    preG.v = preG.t = std::log2 (preFreq);
    postG.v = postG.t = postTone;
}

void DistortionEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Mode: mode = std::clamp ((int) std::lround (v), 0, numCurves - 1); break;
        case Drive: driveG.t = dsp::clamp01 (v); break;
        case Bias: bias = std::clamp (v, -1.0f, 1.0f); break;
        case PreFilter: preFilter = std::clamp ((int) std::lround (v), 0, 3); break;
        case PreFreq: preFreq = v; preG.t = std::log2 (std::max (20.0f, v)); break;
        case PostTone: postTone = v; postG.t = dsp::clamp01 (v); break;
        case Output: outG.t = dsp::dbToGain (v); break;
        case Mix: mixG.t = dsp::clamp01 (v); break;
        default: break;
    }
}

void DistortionEffect::process (float* l, float* r, int n) noexcept
{
    float* io[2] = { l, r };
    // the filters are updated every 16 samples from glided values (smooth sweeps, cheap)
    for (int start = 0; start < n; start += 16)
    {
        const int len = std::min (16, n - start);
        const float preHz = std::exp2 (preG.v);
        const float toneHz = 400.0f * std::pow (50.0f, postG.v); // 400 Hz .. 20 kHz
        for (auto& c : ch)
        {
            c.pre.set (preHz, 0.9f, sr);
            c.post.set (std::min (toneHz, sr * 0.45f), 0.707f, sr);
        }
        float drive = driveG.v, outGain = outG.v, m = mixG.v;
        for (int i = start; i < start + len; ++i)
        {
            preG.next();
            postG.next();
            drive = driveG.next();
            outGain = outG.next();
            m = mixG.next();
            // a little compensation: louder drive, quieter output (the trim stays the user's)
            const float comp = 1.0f / (1.0f + 1.5f * drive);
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float dry = io[c][i];
                float x = dry;
                if (preFilter > 0)
                {
                    s.pre.tick (x);
                    x = preFilter == 1 ? s.pre.lp : preFilter == 2 ? s.pre.bp * 1.6f : s.pre.hp;
                }
                float y;
                if (mode == Downsample)
                {
                    // sample-rate reduction 1x .. 64x and bit depth 16 .. 3 bits from the drive (aliasing is the point)
                    const float factor = 1.0f + 63.0f * drive * drive;
                    s.holdPhase += 1.0f;
                    if (s.holdPhase >= factor)
                    {
                        s.holdPhase -= factor;
                        const float levels = std::exp2 (16.0f - 13.0f * drive);
                        s.held = std::round ((x + bias * 0.2f) * levels) / levels;
                    }
                    y = s.held;
                }
                else
                {
                    float up[4];
                    s.os.up (x, up);
                    for (float& u : up) u = transfer (mode, drive, bias, u);
                    y = s.os.down (up);
                }
                y = dcBlock (y, s.dcX, s.dcY);
                if (postG.v < 0.999f)
                {
                    s.post.tick (y);
                    y = s.post.lp;
                }
                y *= comp * outGain;
                io[c][i] = dry + (y - dry) * m;
            }
        }
    }
}

// ================================================================================================ Flanger
std::vector<ParamDescriptor> FlangerRackEffect::getParamDescriptors() const
{
    using namespace pd;
    return { continuous ("rate", "Flanger Rate", "Flanger-Tempo", 0.02f, 8.0f, 0.25f, ParamUnit::Hertz, 0.4f),
             choice ("sync", "Flanger Sync", "Flanger-Sync", syncNamesEn(), syncNamesDe(), 0),
             continuous ("depth", "Flanger Depth", "Flanger-Tiefe", 0.0f, 1.0f, 0.7f),
             continuous ("delay", "Flanger Delay", "Flanger-Verzögerung", 0.3f, 12.0f, 2.0f, ParamUnit::Milliseconds, 0.5f),
             continuous ("feedback", "Flanger Feedback", "Flanger-Feedback", -0.95f, 0.95f, 0.5f),
             continuous ("stereo", "Flanger Stereo", "Flanger-Stereo", 0.0f, 0.5f, 0.25f),
             continuous ("mix", "Flanger Mix", "Flanger-Mischung", 0.0f, 1.0f, 0.5f) };
}

void FlangerRackEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    lineL.prepare ((int) (0.03f * sr));
    lineR.prepare ((int) (0.03f * sr));
    for (auto* g : { &depthG, &delayG, &fbG, &mixG }) g->prepare (sr, 0.03f);
    reset();
}

void FlangerRackEffect::reset() noexcept
{
    lineL.reset();
    lineR.reset();
    fbL = fbR = 0.0f;
    phase = 0.0f;
    for (auto* g : { &depthG, &delayG, &fbG, &mixG }) g->v = g->t;
}

void FlangerRackEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Rate: rate = v; break;
        case Sync: sync = (int) std::lround (v); break;
        case Depth: depthG.t = dsp::clamp01 (v); break;
        case Delay: delayG.t = v; break;
        case Feedback: fbG.t = std::clamp (v, -0.95f, 0.95f); break;
        case Stereo: stereo = v; break;
        case Mix: mixG.t = dsp::clamp01 (v); break;
        default: break;
    }
}

void FlangerRackEffect::process (float* l, float* r, int n) noexcept
{
    const double beats = syncBeats (sync);
    const float hz = beats > 0.0 ? (float) (tempo / 60.0 / beats) : rate;
    const float inc = hz / sr;
    for (int i = 0; i < n; ++i)
    {
        const float depth = depthG.next(), base = delayG.next(), fb = fbG.next(), m = mixG.next();
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        auto tri = [] (float p) { p -= std::floor (p); return 1.0f - 4.0f * std::abs (p - 0.5f); };
        // sweep from the base delay up to base + 5 ms (log-ish feel through the squared triangle)
        const float tL = 0.5f + 0.5f * tri (phase), tR = 0.5f + 0.5f * tri (phase + stereo);
        const float dL = (base + depth * 5.0f * tL * tL) * 0.001f * sr, dR = (base + depth * 5.0f * tR * tR) * 0.001f * sr;
        const float inL = l[i], inR = r[i];
        const float wl = lineL.read (std::max (1.0f, dL)), wr = lineR.read (std::max (1.0f, dR));
        lineL.push (inL + std::tanh (fbL * fb));
        lineR.push (inR + std::tanh (fbR * fb));
        fbL = dsp::flush (wl);
        fbR = dsp::flush (wr);
        // mix 1 = the classic 50 / 50 comb of dry and delayed signal
        l[i] = inL + (0.5f * (inL + wl) - inL) * m;
        r[i] = inR + (0.5f * (inR + wr) - inR) * m;
    }
}

// ================================================================================================ Compressor
std::vector<ParamDescriptor> CompressorEffect::getParamDescriptors() const
{
    using namespace pd;
    return { continuous ("thresh", "Compressor Threshold", "Kompressor Schwelle", -60.0f, 0.0f, -18.0f, ParamUnit::Decibel),
             continuous ("ratio", "Compressor Ratio", "Kompressor Ratio", 1.0f, 20.0f, 4.0f, ParamUnit::None, 0.4f),
             continuous ("attack", "Compressor Attack", "Kompressor Attack", 0.1f, 200.0f, 10.0f, ParamUnit::Milliseconds, 0.4f),
             continuous ("release", "Compressor Release", "Kompressor Release", 5.0f, 2000.0f, 120.0f, ParamUnit::Milliseconds, 0.4f),
             continuous ("knee", "Compressor Knee", "Kompressor Knie", 0.0f, 24.0f, 6.0f, ParamUnit::Decibel),
             continuous ("makeup", "Compressor Makeup", "Kompressor Aufholverstärkung", 0.0f, 24.0f, 0.0f, ParamUnit::Decibel),
             continuous ("mix", "Compressor Mix", "Kompressor Mischung", 0.0f, 1.0f, 1.0f) };
}

float CompressorEffect::staticGainDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float over = levelDb - thresholdDb;
    const float slope = 1.0f / std::max (1.0f, ratio) - 1.0f;
    if (kneeDb > 0.0f && std::abs (over) <= 0.5f * kneeDb)
    {
        const float x = over + 0.5f * kneeDb;
        return slope * x * x / (2.0f * kneeDb);
    }
    return over > 0.0f ? slope * over : 0.0f;
}

void CompressorEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    makeupG.prepare (sr, 0.02f);
    mixG.prepare (sr, 0.02f);
    setParam (Attack, attackMs);
    setParam (Release, releaseMs);
    reset();
}

void CompressorEffect::reset() noexcept
{
    env = -120.0f;
    grDb = 0.0f;
    makeupG.v = makeupG.t;
    mixG.v = mixG.t;
    grUi.store (0.0f);
}

void CompressorEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Threshold: threshold = v; break;
        case Ratio: ratio = std::max (1.0f, v); break;
        case Attack: attackMs = std::max (0.05f, v); atkC = std::exp (-1.0f / (sr * attackMs * 0.001f)); break;
        case Release: releaseMs = std::max (1.0f, v); relC = std::exp (-1.0f / (sr * releaseMs * 0.001f)); break;
        case Knee: knee = std::max (0.0f, v); break;
        case Makeup: makeupG.t = dsp::dbToGain (v); break;
        case Mix: mixG.t = dsp::clamp01 (v); break;
        default: break;
    }
}

void CompressorEffect::process (float* l, float* r, int n) noexcept
{
    float worst = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // stereo-linked peak detector in dB, smoothed gain computer (branching attack / release)
        const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
        const float target = staticGainDb (dsp::gainToDb (peak), threshold, ratio, knee);
        const float c = target < grDb ? atkC : relC;
        grDb = target + (grDb - target) * c;
        if (grDb > -1.0e-6f) grDb = 0.0f;
        worst = std::min (worst, grDb);
        const float g = std::pow (10.0f, grDb * 0.05f) * makeupG.next();
        const float m = mixG.next();
        l[i] = l[i] * (1.0f - m) + l[i] * g * m;
        r[i] = r[i] * (1.0f - m) + r[i] * g * m;
    }
    grUi.store (-worst, std::memory_order_relaxed);
}

// ================================================================================================ Multiband
std::vector<ParamDescriptor> MultibandEffect::getParamDescriptors() const
{
    using namespace pd;
    return { continuous ("depth", "Multiband Depth", "Multiband Tiefe", 0.0f, 1.0f, 0.5f),
             continuous ("upward", "Multiband Upward", "Multiband aufwärts", 0.0f, 1.0f, 1.0f),
             continuous ("downward", "Multiband Downward", "Multiband abwärts", 0.0f, 1.0f, 1.0f),
             continuous ("time", "Multiband Time", "Multiband Zeit", 0.0f, 1.0f, 0.5f),
             continuous ("low", "Multiband Low", "Multiband Tiefen", -12.0f, 12.0f, 0.0f, ParamUnit::Decibel),
             continuous ("mid", "Multiband Mid", "Multiband Mitten", -12.0f, 12.0f, 0.0f, ParamUnit::Decibel),
             continuous ("high", "Multiband High", "Multiband Höhen", -12.0f, 12.0f, 0.0f, ParamUnit::Decibel),
             continuous ("lowsplit", "Multiband Low Split", "Multiband Trennung tief", 40.0f, 500.0f, 120.0f, ParamUnit::Hertz, 0.5f),
             continuous ("highsplit", "Multiband High Split", "Multiband Trennung hoch", 800.0f, 8000.0f, 2500.0f, ParamUnit::Hertz, 0.5f),
             continuous ("output", "Multiband Output", "Multiband Ausgang", -12.0f, 12.0f, 0.0f, ParamUnit::Decibel),
             continuous ("mix", "Multiband Mix", "Multiband Mischung", 0.0f, 1.0f, 1.0f) };
}

float MultibandEffect::downGainDb (float levelDb, float depth, float downward) noexcept
{
    // downward above -30 dBFS (ratio up to 1:6)
    const float d = dsp::clamp01 (depth);
    return levelDb > -30.0f ? -(levelDb + 30.0f) * (1.0f - 1.0f / (1.0f + 5.0f * d)) * downward : 0.0f;
}

float MultibandEffect::upGainDb (float levelDb, float depth, float upward) noexcept
{
    // upward below -40 dBFS (up to +30 dB, nothing under -80 dBFS)
    if (! (levelDb < -40.0f && levelDb > -80.0f)) return 0.0f;
    const float d = dsp::clamp01 (depth);
    const float below = -40.0f - levelDb;                              // 0 .. 40
    const float fadeOut = std::min (1.0f, (levelDb + 80.0f) / 10.0f); // no lift for the noise floor
    return std::min (30.0f * d, below * 0.75f * d) * upward * fadeOut;
}

float MultibandEffect::staticGainDb (float levelDb, float depth, float upward, float downward) noexcept
{
    return downGainDb (levelDb, depth, downward) + upGainDb (levelDb, depth, upward);
}

void MultibandEffect::setSplits() noexcept
{
    constexpr float q = 0.70710678f;
    for (int c = 0; c < 2; ++c)
    {
        for (auto* s : { &lowLp[(size_t) c], &lowHp[(size_t) c] }) { s->a.set (lowSplit, q, sr); s->b.set (lowSplit, q, sr); }
        for (auto* s : { &highLp[(size_t) c], &highHp[(size_t) c], &lowAp[(size_t) c] }) { s->a.set (highSplit, q, sr); s->b.set (highSplit, q, sr); }
    }
}

void MultibandEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    outG.prepare (sr, 0.02f);
    mixG.prepare (sr, 0.02f);
    setSplits();
    setParam (Time, time);
    reset();
}

void MultibandEffect::reset() noexcept
{
    for (auto* arr : { &lowLp, &lowHp, &highLp, &highHp, &lowAp })
        for (auto& s : *arr) { s.a.reset(); s.b.reset(); }
    env.fill (0.0f);
    envFast.fill (0.0f);
    gain.fill (0.0f);
    outG.v = outG.t;
    mixG.v = mixG.t;
    for (auto& b : bandUi) b.store (0.0f);
}

void MultibandEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Depth: depth = dsp::clamp01 (v); break;
        case Upward: upward = dsp::clamp01 (v); break;
        case Downward: downward = dsp::clamp01 (v); break;
        case Time:
            time = dsp::clamp01 (v);
            // attack 1 .. 30 ms, release 30 .. 600 ms; gain glide 2 ms
            atk = std::exp (-1.0f / (sr * (0.001f + 0.029f * time)));
            rel = std::exp (-1.0f / (sr * (0.03f + 0.57f * time)));
            gainA = 1.0f - std::exp (-1.0f / (sr * 0.002f));
            gainFall = 1.0f - std::exp (-1.0f / (sr * 0.0002f));
            fastRel = std::exp (-1.0f / (sr * 0.03f)); // slow enough not to flicker over a 40 Hz cycle
            break;
        case LowGain: bandLevel[0] = dsp::dbToGain (v); break;
        case MidGain: bandLevel[1] = dsp::dbToGain (v); break;
        case HighGain: bandLevel[2] = dsp::dbToGain (v); break;
        case LowSplit: lowSplit = v; setSplits(); break;
        case HighSplit: highSplit = v; setSplits(); break;
        case Output: outG.t = dsp::dbToGain (v); break;
        case Mix: mixG.t = dsp::clamp01 (v); break;
        default: break;
    }
}

void MultibandEffect::process (float* l, float* r, int n) noexcept
{
    float* io[2] = { l, r };
    std::array<float, numBands> worst {};
    for (int i = 0; i < n; ++i)
    {
        std::array<std::array<float, numBands>, 2> band {};
        for (int c = 0; c < 2; ++c)
        {
            const float x = io[c][i];
            auto& lp = lowLp[(size_t) c];
            lp.a.tick (x);
            lp.b.tick (lp.a.lp);
            auto& hp = lowHp[(size_t) c];
            hp.a.tick (x);
            hp.b.tick (hp.a.hp);
            const float low = lp.b.lp, rest = hp.b.hp;
            auto& ap = lowAp[(size_t) c]; // LR4 all-pass = LP4 + HP4 at the high split
            ap.a.tick (low);
            ap.b.tick (ap.a.lp);
            const float lowAligned = ap.b.lp; // low part of the all-pass ...
            auto& ml = highLp[(size_t) c];
            ml.a.tick (rest);
            ml.b.tick (ml.a.lp);
            auto& mh = highHp[(size_t) c];
            mh.a.tick (rest);
            mh.b.tick (mh.a.hp);
            band[(size_t) c][0] = lowAligned;
            band[(size_t) c][1] = ml.b.lp;
            band[(size_t) c][2] = mh.b.hp;
            // ... plus its high part: low frequencies sit far below the high split, so the high part of the
            // all-pass is negligible for the low band (< -40 dB) and is dropped
        }
        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < numBands; ++b)
        {
            const float pk = std::max (std::abs (band[0][(size_t) b]), std::abs (band[1][(size_t) b]));
            // 0.8: the envelopes follow the linear peak (release = a steady dB-per-second fall), not the dB value: in
            // dB a pause drops the detector by 40 dB in 100 ms, so the next note started uncompressed (6-10 dB onset
            // spikes). The lift follows a fast envelope (instant attack) and lets go as soon as a note starts.
            auto& e = env[(size_t) b];
            e = dsp::flush (pk > e ? pk + (e - pk) * atk : pk + (e - pk) * rel);
            auto& ef = envFast[(size_t) b];
            ef = dsp::flush (pk > ef ? pk : pk + (ef - pk) * fastRel);
            const float target = downGainDb (dsp::gainToDb (e), depth, downward)
                               + upGainDb (dsp::gainToDb (std::max (e, ef)), depth, upward);
            // the gain falls within ~0.2 ms (a note after a quiet tail is not lifted by the old gain) and rises over 2 ms
            gain[(size_t) b] += (target - gain[(size_t) b]) * (target < gain[(size_t) b] ? gainFall : gainA);
            worst[(size_t) b] = gain[(size_t) b];
            const float g = std::pow (10.0f, gain[(size_t) b] * 0.05f) * bandLevel[(size_t) b];
            outL += band[0][(size_t) b] * g;
            outR += band[1][(size_t) b] * g;
        }
        const float o = outG.next(), m = mixG.next();
        l[i] = l[i] + (outL * o - l[i]) * m;
        r[i] = r[i] + (outR * o - r[i]) * m;
    }
    for (int b = 0; b < numBands; ++b) bandUi[(size_t) b].store (worst[(size_t) b], std::memory_order_relaxed);
}

// ================================================================================================ EQ
std::vector<ParamDescriptor> EqEffect::getParamDescriptors() const
{
    using namespace pd;
    auto f = [] (const char* id, const char* en, const char* de, float mn, float mx, float def) {
        return continuous (id, en, de, mn, mx, def, ParamUnit::Hertz, 0.3f);
    };
    auto g = [] (const char* id, const char* en, const char* de) { return continuous (id, en, de, -18.0f, 18.0f, 0.0f, ParamUnit::Decibel); };
    auto q = [] (const char* id, const char* en, const char* de) { return continuous (id, en, de, 0.3f, 12.0f, 1.0f, ParamUnit::None, 0.4f); };
    return { f ("lowfreq", "EQ Low Shelf Freq", "EQ Tiefen-Kuhschwanz Frequenz", 20.0f, 1000.0f, 100.0f),
             g ("lowgain", "EQ Low Shelf Gain", "EQ Tiefen-Kuhschwanz Gain"),
             f ("p1freq", "EQ Band 1 Freq", "EQ Band 1 Frequenz", 30.0f, 18000.0f, 300.0f),
             g ("p1gain", "EQ Band 1 Gain", "EQ Band 1 Gain"),
             q ("p1q", "EQ Band 1 Q", "EQ Band 1 Güte"),
             f ("p2freq", "EQ Band 2 Freq", "EQ Band 2 Frequenz", 30.0f, 18000.0f, 1200.0f),
             g ("p2gain", "EQ Band 2 Gain", "EQ Band 2 Gain"),
             q ("p2q", "EQ Band 2 Q", "EQ Band 2 Güte"),
             f ("p3freq", "EQ Band 3 Freq", "EQ Band 3 Frequenz", 30.0f, 18000.0f, 4500.0f),
             g ("p3gain", "EQ Band 3 Gain", "EQ Band 3 Gain"),
             q ("p3q", "EQ Band 3 Q", "EQ Band 3 Güte"),
             f ("highfreq", "EQ High Shelf Freq", "EQ Höhen-Kuhschwanz Frequenz", 1000.0f, 20000.0f, 8000.0f),
             g ("highgain", "EQ High Shelf Gain", "EQ Höhen-Kuhschwanz Gain"),
             continuous ("output", "EQ Output", "EQ Ausgang", -18.0f, 18.0f, 0.0f, ParamUnit::Decibel) };
}

void EqEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    if (target[LowFreq] == 0.0f)
    {
        const auto d = getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) target[i] = d[i].defaultValue;
    }
    update (true);
    reset();
}

void EqEffect::reset() noexcept
{
    for (auto& c : bands)
        for (auto& b : c) b.reset();
    update (true);
}

void EqEffect::setParam (int i, float v) noexcept
{
    if (i < 0 || i >= numParams) return;
    target[(size_t) i] = v;
    dirty = true;
}

void EqEffect::update (bool snap) noexcept
{
    // frequencies glide in octaves, gains in dB, Q directly (about 20 ms at the 32-sample update rate)
    const float a = snap ? 1.0f : 0.08f;
    bool moving = false;
    for (int i = 0; i < numParams; ++i)
    {
        const bool isFreq = i == LowFreq || i == P1Freq || i == P2Freq || i == P3Freq || i == HighFreq;
        float& c = current[(size_t) i];
        const float t = target[(size_t) i];
        if (isFreq && c > 0.0f && t > 0.0f) c = std::exp2 (std::log2 (c) + (std::log2 (t) - std::log2 (c)) * a);
        else c += (t - c) * a;
        if (std::abs (t - c) > 1.0e-4f * (1.0f + std::abs (t))) moving = true;
        else c = t;
    }
    for (auto& ch : bands)
    {
        ch[0].set (Biquad::LowShelf, current[LowFreq], 0.707f, current[LowGain], sr);
        ch[1].set (Biquad::Peak, current[P1Freq], current[P1Q], current[P1Gain], sr);
        ch[2].set (Biquad::Peak, current[P2Freq], current[P2Q], current[P2Gain], sr);
        ch[3].set (Biquad::Peak, current[P3Freq], current[P3Q], current[P3Gain], sr);
        ch[4].set (Biquad::HighShelf, current[HighFreq], 0.707f, current[HighGain], sr);
    }
    outGain = dsp::dbToGain (current[Output]);
    dirty = moving;
}

float EqEffect::responseDb (const float* v, float hz, float sampleRate) noexcept
{
    std::array<Biquad, numBands> b;
    b[0].set (Biquad::LowShelf, v[LowFreq], 0.707f, v[LowGain], sampleRate);
    b[1].set (Biquad::Peak, v[P1Freq], v[P1Q], v[P1Gain], sampleRate);
    b[2].set (Biquad::Peak, v[P2Freq], v[P2Q], v[P2Gain], sampleRate);
    b[3].set (Biquad::Peak, v[P3Freq], v[P3Q], v[P3Gain], sampleRate);
    b[4].set (Biquad::HighShelf, v[HighFreq], 0.707f, v[HighGain], sampleRate);
    float db = v[Output];
    for (const auto& f : b) db += f.magnitudeDb (hz, sampleRate);
    return db;
}

void EqEffect::process (float* l, float* r, int n) noexcept
{
    for (int start = 0; start < n; start += 32)
    {
        if (dirty) update (false);
        const int end = std::min (n, start + 32);
        for (int i = start; i < end; ++i)
        {
            float xl = l[i], xr = r[i];
            for (int b = 0; b < numBands; ++b)
            {
                xl = bands[0][(size_t) b].process (xl);
                xr = bands[1][(size_t) b].process (xr);
            }
            l[i] = xl * outGain;
            r[i] = xr * outGain;
        }
    }
}

// ================================================================================================ Filter
std::vector<ParamDescriptor> FilterRackEffect::getParamDescriptors() const
{
    using namespace pd;
    return { choice ("type", "Filter Type", "Filtertyp",
                     { "Low-pass 12", "Low-pass 24", "High-pass 12", "High-pass 24", "Band-pass", "Notch", "Comb +", "Comb -", "Formant" },
                     { "Tiefpass 12", "Tiefpass 24", "Hochpass 12", "Hochpass 24", "Bandpass", "Kerbfilter", "Kamm +", "Kamm -", "Formant" }, LP24),
             continuous ("cutoff", "Filter Cutoff", "Filter-Cutoff", 20.0f, 20000.0f, 2000.0f, ParamUnit::Hertz, 0.3f),
             continuous ("reso", "Filter Resonance", "Filter-Resonanz", 0.0f, 1.0f, 0.3f),
             continuous ("drive", "Filter Drive", "Filter-Drive", 0.0f, 1.0f, 0.0f),
             continuous ("mix", "Filter Mix", "Filter-Mischung", 0.0f, 1.0f, 1.0f) };
}

void FilterRackEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    for (auto& c : ch) c.comb.prepare ((int) (sr / 20.0f) + 8);
    octG.prepare (sr, 0.004f);
    driveG.prepare (sr, 0.02f);
    mixG.prepare (sr, 0.02f);
    reset();
}

void FilterRackEffect::reset() noexcept
{
    for (auto& c : ch)
    {
        c.a.reset();
        c.b.reset();
        for (auto& f : c.f) f.reset();
        c.comb.reset();
        c.damp = 0.0f;
    }
    octG.v = octG.t;
    driveG.v = driveG.t;
    mixG.v = mixG.t;
}

void FilterRackEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Type: kind = std::clamp ((int) std::lround (v), 0, numKinds - 1); break;
        case Cutoff: octG.t = std::log2 (std::clamp (v, 20.0f, 20000.0f)); break;
        case Resonance: reso = dsp::clamp01 (v); break;
        case Drive: driveG.t = dsp::clamp01 (v); break;
        case Mix: mixG.t = dsp::clamp01 (v); break;
        default: break;
    }
}

void FilterRackEffect::process (float* l, float* r, int n) noexcept
{
    float* io[2] = { l, r };
    // vowel formants a e i o u (F1, F2, F3)
    static constexpr float vowels[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2100, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
    // resonance 0 = maximally flat (Butterworth: 0.707 for 12 dB, 0.541 / 1.307 for the two 24 dB stages); the
    // resonance lifts the Q of the single / second stage, so the pass band stays flat until the peak is asked for
    const float q = 0.7071f + 20.0f * reso * reso;           // 0.71 .. 20.7
    const bool four = kind == LP24 || kind == HP24;
    const float qa = four ? 0.5412f : q, qb = 1.3066f + (q - 0.7071f) * 0.75f;
    for (int start = 0; start < n; start += 8)
    {
        const int end = std::min (n, start + 8);
        // coefficient update every 8 samples from the per-sample glided cutoff
        for (int i = start; i < end; ++i) octG.next();
        const float hz = std::exp2 (octG.v);
        for (auto& c : ch)
        {
            c.a.set (hz, qa, sr);
            c.b.set (hz, qb, sr);
            if (kind == Formant)
            {
                const float pos = std::clamp ((octG.v - std::log2 (20.0f)) / (std::log2 (20000.0f) - std::log2 (20.0f)), 0.0f, 1.0f) * 4.0f;
                const int v0 = std::min (3, (int) pos);
                const float t = pos - (float) v0;
                for (int k = 0; k < 3; ++k)
                    c.f[(size_t) k].set (vowels[v0][k] + (vowels[v0 + 1][k] - vowels[v0][k]) * t, 4.0f + 16.0f * reso, sr);
            }
        }
        for (int i = start; i < end; ++i)
        {
            const float drive = driveG.next(), m = mixG.next();
            const float pre = 1.0f + 8.0f * drive * drive, post = 1.0f / std::sqrt (pre);
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float dry = io[c][i];
                float x = drive > 0.0f ? std::tanh (dry * pre) * post * (1.0f + drive) : dry;
                float y = x;
                switch (kind)
                {
                    case LP12: s.a.tick (x); y = s.a.lp; break;
                    case LP24: s.a.tick (x); s.b.tick (s.a.lp); y = s.b.lp; break;
                    case HP12: s.a.tick (x); y = s.a.hp; break;
                    case HP24: s.a.tick (x); s.b.tick (s.a.hp); y = s.b.hp; break;
                    case BP: s.a.tick (x); y = s.a.bp * (s.a.k); break;
                    case Notch: s.a.tick (x); y = s.a.lp + s.a.hp; break;
                    case CombPlus:
                    case CombMinus:
                    {
                        const float d = sr / hz;
                        const float fb = (kind == CombPlus ? 1.0f : -1.0f) * (0.2f + 0.78f * reso);
                        const float delayed = s.comb.read (std::max (1.0f, d));
                        s.damp += 0.5f * (delayed - s.damp); // gentle damping in the loop
                        s.comb.push (x + std::tanh (s.damp * fb));
                        y = 0.5f * (x + delayed);
                        break;
                    }
                    case Formant:
                    {
                        y = 0.0f;
                        static constexpr float gains[3] = { 1.0f, 0.7f, 0.45f };
                        for (int k = 0; k < 3; ++k)
                        {
                            s.f[(size_t) k].tick (x);
                            y += s.f[(size_t) k].bp * s.f[(size_t) k].k * gains[k];
                        }
                        y *= 1.4f;
                        break;
                    }
                    default: break;
                }
                io[c][i] = dry + (y - dry) * m;
            }
        }
    }
}

// ================================================================================================ Echo
std::vector<ParamDescriptor> EchoEffect::getParamDescriptors() const
{
    using namespace pd;
    return { choice ("mode", "Echo Mode", "Echo-Modus", { "Stereo", "Ping-Pong", "Tap" }, { "Stereo", "Ping-Pong", "Tap" }, PingPong),
             choice ("sync", "Echo Sync", "Echo-Sync", syncNamesEn(), syncNamesDe(), 9),
             continuous ("time", "Echo Time", "Echo-Zeit", 1.0f, 2000.0f, 375.0f, ParamUnit::Milliseconds, 0.4f),
             continuous ("ratio", "Echo Right Ratio", "Echo Verhältnis rechts", 0.25f, 1.0f, 0.75f),
             continuous ("feedback", "Echo Feedback", "Echo-Feedback", 0.0f, 1.05f, 0.4f),
             continuous ("lowcut", "Echo Low Cut", "Echo Tiefensperre", 20.0f, 2000.0f, 120.0f, ParamUnit::Hertz, 0.3f),
             continuous ("highcut", "Echo High Cut", "Echo Höhensperre", 1000.0f, 20000.0f, 8000.0f, ParamUnit::Hertz, 0.3f),
             continuous ("width", "Echo Width", "Echo-Breite", 0.0f, 1.0f, 1.0f),
             continuous ("mix", "Echo Mix", "Echo-Mischung", 0.0f, 1.0f, 0.3f) };
}

void EchoEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    lineL.prepare ((int) (4.2f * sr));
    lineR.prepare ((int) (4.2f * sr));
    timeG.prepare (sr, 0.12f); // tape-like glide when the time changes
    fbG.prepare (sr, 0.02f);
    mixG.prepare (sr, 0.02f);
    reset();
}

void EchoEffect::reset() noexcept
{
    lineL.reset();
    lineR.reset();
    for (auto& f : lowCut) f.reset();
    for (auto& f : highCut) f.reset();
    timeG.snap (delaySamples());
    fbG.v = fbG.t;
    mixG.v = mixG.t;
}

float EchoEffect::delaySamples() const noexcept
{
    const double beats = syncBeats (sync);
    const float ms = beats > 0.0 ? (float) (beats * 60000.0 / std::max (20.0, tempo)) : timeMs;
    return std::clamp (ms * 0.001f * sr, 1.0f, 4.0f * sr);
}

void EchoEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Mode: mode = std::clamp ((int) std::lround (v), 0, numModes - 1); break;
        case Sync: sync = (int) std::lround (v); break;
        case Time: timeMs = v; break;
        case Ratio: ratio = std::clamp (v, 0.25f, 1.0f); break;
        case Feedback: fbG.t = std::clamp (v, 0.0f, 1.05f); break;
        case LowCut: lowHz = v; break;
        case HighCut: highHz = v; break;
        case Width: width = dsp::clamp01 (v); break;
        case Mix: mixG.t = dsp::clamp01 (v); break;
        default: break;
    }
}

void EchoEffect::process (float* l, float* r, int n) noexcept
{
    timeG.t = delaySamples();
    for (auto& f : lowCut) f.set (lowHz, 0.707f, sr);
    for (auto& f : highCut) f.set (std::min (highHz, sr * 0.45f), 0.707f, sr);
    for (int i = 0; i < n; ++i)
    {
        const float d = timeG.next(), fb = fbG.next(), m = mixG.next();
        const float dr = mode == Stereo ? d : (mode == Tap ? d * ratio : d);
        float wl = lineL.read (d), wr = lineR.read (dr);
        // feedback path: band-limited, softly saturated (feedback > 100 % blooms instead of exploding)
        lowCut[0].tick (wl);
        highCut[0].tick (lowCut[0].hp);
        lowCut[1].tick (wr);
        highCut[1].tick (lowCut[1].hp);
        const float fl = std::tanh (highCut[0].lp * fb), fr = std::tanh (highCut[1].lp * fb);
        const float inL = l[i], inR = r[i];
        switch (mode)
        {
            case PingPong: // mono in on the left, the repeats cross over
                lineL.push (0.5f * (inL + inR) + fr);
                lineR.push (fl);
                break;
            case Tap: // two taps of one line pair, each repeating on its own side
                lineL.push (0.5f * (inL + inR) + fl);
                lineR.push (0.5f * (inL + inR) + fr);
                break;
            default:
                lineL.push (inL + fl);
                lineR.push (inR + fr);
                break;
        }
        // width: blend the wet signal towards mono
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        wl = mid + side;
        wr = mid - side;
        const float dryG = std::min (1.0f, 2.0f * (1.0f - m)), wetG = std::min (1.0f, 2.0f * m);
        l[i] = inL * dryG + wl * wetG;
        r[i] = inR * dryG + wr * wetG;
    }
}

// ================================================================================================ Hall
std::vector<ParamDescriptor> HallEffect::getParamDescriptors() const
{
    using namespace pd;
    return { choice ("mode", "Hall Mode", "Raum-Modus", { "Plate", "Hall" }, { "Platte", "Halle" }, 1),
             continuous ("size", "Hall Size", "Raumgröße", 0.0f, 1.0f, 0.6f),
             continuous ("decay", "Hall Decay", "Raum Nachhall", 0.2f, 20.0f, 2.5f, ParamUnit::Seconds, 0.4f),
             continuous ("damping", "Hall Damping", "Raum Dämpfung", 0.0f, 1.0f, 0.4f),
             continuous ("predelay", "Hall Pre-Delay", "Raum Pre-Delay", 0.0f, 250.0f, 12.0f, ParamUnit::Milliseconds, 0.5f),
             continuous ("width", "Hall Width", "Raum-Breite", 0.0f, 1.0f, 1.0f),
             continuous ("mix", "Hall Mix", "Raum-Mischung", 0.0f, 1.0f, 0.3f) };
}

void HallEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    // mutually prime-ish line lengths (ms at size 1), scaled down by size
    static constexpr float ms[numLines] = { 47.3f, 53.9f, 61.7f, 68.3f, 76.1f, 83.9f, 91.3f, 101.9f };
    for (int i = 0; i < numLines; ++i)
    {
        baseLen[(size_t) i] = ms[i] * 0.001f * sr;
        lines[(size_t) i].prepare ((int) (baseLen[(size_t) i] * 1.05f) + 64);
    }
    static constexpr float dms[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
    for (int i = 0; i < 4; ++i)
    {
        diffuseLen[(size_t) i] = dms[i] * 0.001f * sr;
        diffuse[(size_t) i].prepare ((int) diffuseLen[(size_t) i] + 8);
    }
    pre.prepare ((int) (0.26f * sr));
    mixG.prepare (sr, 0.02f);
    sizeG.prepare (sr, 0.2f);
    reset();
}

void HallEffect::reset() noexcept
{
    for (auto& l : lines) l.reset();
    for (auto& d : diffuse) d.reset();
    pre.reset();
    lpState.fill (0.0f);
    for (int i = 0; i < numLines; ++i) modPhase[(size_t) i] = (float) i / numLines;
    mixG.v = mixG.t;
    sizeG.snap (size);
    loopDirty = true;
}

void HallEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Mode: mode = std::clamp ((int) std::lround (v), 0, 1); loopDirty = true; break;
        case Size: size = dsp::clamp01 (v); sizeG.t = size; loopDirty = true; break;
        case Decay: decay = std::max (0.1f, v); loopDirty = true; break;
        case Damping: damping = dsp::clamp01 (v); loopDirty = true; break;
        case PreDelay: preMs = v; break;
        case Width: width = dsp::clamp01 (v); break;
        case Mix: mixG.t = dsp::clamp01 (v); break;
        default: break;
    }
}

void HallEffect::updateLoop() noexcept
{
    const float scale = 0.25f + 0.75f * sizeG.v;
    for (int i = 0; i < numLines; ++i)
    {
        // gain per pass so that the loop decays 60 dB in `decay` seconds
        const float len = baseLen[(size_t) i] * scale;
        gainL[(size_t) i] = std::pow (10.0f, -3.0f * len / (decay * sr));
    }
    // damping: one-pole low-pass in the loop (plate brighter than hall)
    const float hz = (mode == 0 ? 12000.0f : 9000.0f) * std::pow (0.08f, damping);
    lpCoeff = 1.0f - std::exp (-dsp::twoPi * std::min (hz, sr * 0.45f) / sr);
    loopDirty = ! sizeG.settled();
}

void HallEffect::process (float* l, float* r, int n) noexcept
{
    const float preSamples = std::max (1.0f, preMs * 0.001f * sr);
    const float scaleNow = 0.25f + 0.75f * sizeG.v;
    const float modDepth = (mode == 0 ? 3.0f : 8.0f) * 0.0001f * sr; // ~0.3 / 0.8 ms of slow wander
    const float modInc = 0.6f / sr;
    if (loopDirty) updateLoop();
    for (int i = 0; i < n; ++i)
    {
        sizeG.next();
        const float m = mixG.next();
        const float inL = l[i], inR = r[i];
        pre.push (0.5f * (inL + inR));
        float x = pre.read (preSamples);
        // input diffusion: four all-passes
        for (int d = 0; d < 4; ++d)
        {
            auto& ap = diffuse[(size_t) d];
            const float delayed = ap.read (diffuseLen[(size_t) d]);
            const float g = d < 2 ? 0.7f : 0.6f;
            const float v = x + g * delayed;
            ap.push (dsp::flush (v));
            x = delayed - g * v;
        }
        // FDN: read the lines, damp, mix with a Hadamard matrix, write back with the input
        std::array<float, numLines> o {};
        for (int k = 0; k < numLines; ++k)
        {
            float& ph = modPhase[(size_t) k];
            ph += modInc * (1.0f + 0.13f * (float) k);
            if (ph >= 1.0f) ph -= 1.0f;
            const float len = baseLen[(size_t) k] * scaleNow + modDepth * std::sin (dsp::twoPi * ph);
            float y = lines[(size_t) k].read (len);
            lpState[(size_t) k] = dsp::flush (lpState[(size_t) k] + (y - lpState[(size_t) k]) * lpCoeff);
            o[(size_t) k] = lpState[(size_t) k] * gainL[(size_t) k];
        }
        // fast Walsh-Hadamard transform (orthogonal, energy preserving), normalised by 1/sqrt(8)
        for (int h = 1; h < numLines; h <<= 1)
            for (int a = 0; a < numLines; a += h << 1)
                for (int b = a; b < a + h; ++b)
                {
                    const float u = o[(size_t) b], v = o[(size_t) (b + h)];
                    o[(size_t) b] = u + v;
                    o[(size_t) (b + h)] = u - v;
                }
        const float norm = 0.35355339f;
        float wl = 0.0f, wr = 0.0f;
        for (int k = 0; k < numLines; ++k)
        {
            const float v = o[(size_t) k] * norm;
            lines[(size_t) k].push (v + x * (k % 2 ? -0.5f : 0.5f));
            if (k % 2) wr += v;
            else wl += v;
        }
        wl *= 0.5f;
        wr *= 0.5f;
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        // equal-loudness blend: the dry level stays up to 50 %, the wet level is full from 50 %
        const float dryG = std::min (1.0f, 2.0f * (1.0f - m)), wetG = std::min (1.0f, 2.0f * m);
        l[i] = inL * dryG + (mid + side) * wetG;
        r[i] = inR * dryG + (mid - side) * wetG;
    }
    if (! sizeG.settled()) loopDirty = true;
}
} // namespace ohpe
