// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InsertEffects2.h"

#include <cstring>

namespace ohpe
{
namespace
{
// resonance values of dsp::Svf (k = 2 - 1.96 r) for Butterworth sections
constexpr float butterQ707 = 0.29900f; ///< Q 0.707 (two of them = 4th-order Linkwitz-Riley)

inline float svfK (float res) noexcept { return 2.0f - 1.96f * dsp::clamp01 (res); }

float readFrac (const std::vector<float>& b, float rp) noexcept
{
    const int len = (int) b.size();
    while (rp < 0.0f) rp += (float) len;
    const int i0 = (int) rp % len;
    const int i1 = (i0 + 1) % len;
    const float f = rp - std::floor (rp);
    return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
}
} // namespace

// ---------------------------------------------------------------- sync
namespace fxsync
{
namespace
{
struct Div
{
    const char* en;
    const char* de;
    double beats;
};
constexpr Div divisions[] = { { "Free", "Frei", 0.0 },  { "1/1", "1/1", 4.0 },       { "1/2", "1/2", 2.0 },         { "1/4", "1/4", 1.0 },
                              { "1/8", "1/8", 0.5 },    { "1/16", "1/16", 0.25 },    { "1/32", "1/32", 0.125 },     { "1/2.", "1/2.", 3.0 },
                              { "1/4.", "1/4.", 1.5 },  { "1/8.", "1/8.", 0.75 },    { "1/16.", "1/16.", 0.375 },   { "1/2T", "1/2T", 4.0 / 3.0 },
                              { "1/4T", "1/4T", 2.0 / 3.0 }, { "1/8T", "1/8T", 1.0 / 3.0 }, { "1/16T", "1/16T", 1.0 / 6.0 },
                              { "2/1", "2/1", 8.0 },    { "4/1", "4/1", 16.0 } }; // the rack's divisions (RackEffects.h), then 2/1, 4/1
constexpr int numDivisions = (int) (sizeof (divisions) / sizeof (divisions[0]));
} // namespace

std::vector<std::string> namesEn()
{
    std::vector<std::string> v;
    for (const auto& d : divisions) v.emplace_back (d.en);
    return v;
}

std::vector<std::string> namesDe()
{
    std::vector<std::string> v;
    for (const auto& d : divisions) v.emplace_back (d.de);
    return v;
}

double beats (int d) noexcept { return divisions[std::clamp (d, 0, numDivisions - 1)].beats; }

float rateHz (int division, float hz, double bpm) noexcept
{
    const double b = beats (division);
    if (b <= 0.0) return hz;
    return (float) (std::clamp (bpm, 20.0, 400.0) / 60.0 / b);
}
} // namespace fxsync

float fxLfoShape (int shape, float p, float held) noexcept
{
    switch (shape)
    {
        case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);                 // triangle: -1 at 0, +1 at 0.5
        case 2: return 1.0f - 2.0f * p;                                 // saw down
        case 3: return 2.0f * p - 1.0f;                                 // saw up
        case 4: return p < 0.5f ? 1.0f : -1.0f;                         // square
        case 5: return held;                                            // sample & hold
        default: return -std::cos (dsp::twoPi * p);                     // sine, starting at the bottom
    }
}

// ---------------------------------------------------------------- comb
std::vector<ParamDescriptor> CombEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("freq", "Comb Frequency", "Kamm-Frequenz", 20.0f, 5000.0f, 300.0f, ParamUnit::Hertz, 0.3f),
        continuous ("feedback", "Comb Feedback", "Kamm-Feedback", -0.95f, 0.95f, 0.6f),
        continuous ("damp", "Comb Damping", "Kamm-Dämpfung", 0.0f, 1.0f, 0.3f),
        continuous ("rate", "Comb Rate", "Kamm-Tempo", 0.01f, 10.0f, 0.2f, ParamUnit::Hertz, 0.4f),
        continuous ("depth", "Comb Depth", "Kamm-Tiefe", 0.0f, 1.0f, 0.3f),
        continuous ("spread", "Comb Spread", "Kamm-Breite", 0.0f, 1.0f, 0.3f),
        continuous ("mix", "Comb Mix", "Kamm-Mischung", 0.0f, 1.0f, 0.5f),
    };
}

void CombEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    const size_t len = (size_t) (0.11f * sr) + 4; // 20 Hz an octave down + margin
    bufL.assign (len, 0.0f);
    bufR.assign (len, 0.0f);
    reset();
}

void CombEffect::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    zl = zr = phase = 0.0f;
}

void CombEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Freq: freq = std::clamp (v, 20.0f, 5000.0f); break;
        case Feedback: feedback = std::clamp (v, -0.95f, 0.95f); break;
        case Damp: damp = dsp::clamp01 (v); break;
        case Rate: rate = std::clamp (v, 0.01f, 10.0f); break;
        case Depth: depth = dsp::clamp01 (v); break;
        case Spread: spread = dsp::clamp01 (v); break;
        case Mix: mix = dsp::clamp01 (v); break;
        default: break;
    }
}

void CombEffect::process (float* l, float* r, int n) noexcept
{
    if (bufL.empty()) return;
    const int len = (int) bufL.size();
    const float inc = rate / sr;
    const float dampA = 1.0f - 0.85f * damp; // one-pole low-pass in the loop
    const float norm = 1.0f - 0.7f * std::abs (feedback);
    for (int i = 0; i < n; ++i)
    {
        const float a = dsp::twoPi * phase;
        const float fL = freq * std::exp2 (depth * std::sin (a));
        const float fR = freq * (1.0f + 0.03f * spread) * std::exp2 (depth * std::sin (a + spread * dsp::pi));
        const float dL = std::clamp (sr / fL, 2.0f, (float) len - 2.0f), dR = std::clamp (sr / fR, 2.0f, (float) len - 2.0f);
        const float yl = readFrac (bufL, (float) writePos - dL), yr = readFrac (bufR, (float) writePos - dR);
        zl = dsp::flush (zl + dampA * (yl - zl));
        zr = dsp::flush (zr + dampA * (yr - zr));
        const float vl = dsp::flush (std::clamp (l[i] + feedback * zl, -8.0f, 8.0f));
        const float vr = dsp::flush (std::clamp (r[i] + feedback * zr, -8.0f, 8.0f));
        bufL[(size_t) writePos] = vl;
        bufR[(size_t) writePos] = vr;
        l[i] = l[i] * (1.0f - mix) + vl * norm * mix;
        r[i] = r[i] * (1.0f - mix) + vr * norm * mix;
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        if (++writePos >= len) writePos = 0;
    }
}

// ---------------------------------------------------------------- formant
std::vector<ParamDescriptor> FormantEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("vowel", "Vowel", "Vokal", 0.0f, 1.0f, 0.0f),
        continuous ("shift", "Formant Shift", "Formant-Verschiebung", -12.0f, 12.0f, 0.0f, ParamUnit::Semitones),
        continuous ("reso", "Formant Resonance", "Formant-Resonanz", 0.0f, 1.0f, 0.5f),
        continuous ("rate", "Formant Rate", "Formant-Tempo", 0.01f, 20.0f, 1.0f, ParamUnit::Hertz, 0.4f),
        choice ("sync", "Formant Sync", "Formant-Sync", fxsync::namesEn(), fxsync::namesDe(), 0),
        continuous ("depth", "Formant LFO Depth", "Formant-LFO-Tiefe", 0.0f, 1.0f, 0.0f),
        continuous ("mix", "Formant Mix", "Formant-Mischung", 0.0f, 1.0f, 1.0f),
    };
}

void FormantEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    for (auto& f : bp) f.prepare (sr);
    reset();
}

void FormantEffect::reset() noexcept
{
    for (auto& f : bp) f.reset();
    phase = 0.0f;
}

void FormantEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Vowel: vowel = dsp::clamp01 (v); break;
        case Shift: shift = std::clamp (v, -12.0f, 12.0f); break;
        case Resonance: reso = dsp::clamp01 (v); break;
        case Rate: rate = std::clamp (v, 0.01f, 20.0f); break;
        case Sync: sync = (int) std::lround (v); break;
        case Depth: depth = dsp::clamp01 (v); break;
        case Mix: mix = dsp::clamp01 (v); break;
        default: break;
    }
}

void FormantEffect::process (float* l, float* r, int n) noexcept
{
    static const float table[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
    static const float gains[3] = { 1.0f, 0.7f, 0.45f };
    const float hz = fxsync::rateHz (sync, rate, tempo);
    const float res = 0.55f + 0.42f * reso;
    const float k = svfK (res);
    const float ratio = dsp::semitonesToRatio (shift);
    for (int start = 0; start < n; start += 16)
    {
        const int len = std::min (16, n - start);
        const float v = dsp::clamp01 (vowel + depth * 0.5f * fxLfoShape (0, phase, 0.0f));
        const float x = v * 4.0f;
        const int i0 = std::min (3, (int) x);
        const float fr = x - (float) i0;
        for (int f = 0; f < 3; ++f)
        {
            const float F = (table[i0][f] + (table[i0 + 1][f] - table[i0][f]) * fr) * ratio;
            bp[(size_t) f].set (F, res);
            bp[(size_t) (3 + f)].set (F, res);
        }
        for (int i = start; i < start + len; ++i)
        {
            float wl = 0.0f, wr = 0.0f;
            for (int f = 0; f < 3; ++f)
            {
                wl += gains[f] * bp[(size_t) f].process (l[i], dsp::Svf::BandPass);
                wr += gains[f] * bp[(size_t) (3 + f)].process (r[i], dsp::Svf::BandPass);
            }
            // band-pass peak gain is 1 / k: scale back to unity, plus a little for the narrow bands
            const float g = k * (1.6f + 0.8f * reso);
            l[i] = l[i] * (1.0f - mix) + wl * g * mix;
            r[i] = r[i] * (1.0f - mix) + wr * g * mix;
        }
        phase += hz * (float) len / sr;
        phase -= std::floor (phase);
    }
}

// ---------------------------------------------------------------- wobble
std::vector<ParamDescriptor> WobbleEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("rate", "Wobble Rate", "Wobble-Tempo", 0.05f, 20.0f, 2.0f, ParamUnit::Hertz, 0.4f),
        choice ("sync", "Wobble Sync", "Wobble-Sync", fxsync::namesEn(), fxsync::namesDe(), 4),
        choice ("shape", "Wobble Shape", "Wobble-Form", { "Sine", "Triangle", "Saw Down", "Saw Up", "Square", "S&H" },
                { "Sinus", "Dreieck", "Sägezahn ab", "Sägezahn auf", "Rechteck", "S&H" }, 0),
        continuous ("low", "Wobble Low", "Wobble tief", 30.0f, 8000.0f, 150.0f, ParamUnit::Hertz, 0.3f),
        continuous ("high", "Wobble High", "Wobble hoch", 100.0f, 18000.0f, 3000.0f, ParamUnit::Hertz, 0.3f),
        continuous ("reso", "Wobble Resonance", "Wobble-Resonanz", 0.0f, 1.0f, 0.5f),
        choice ("mode", "Wobble Filter", "Wobble-Filter", { "Low-pass", "Band-pass" }, { "Tiefpass", "Bandpass" }, 0),
        continuous ("drive", "Wobble Drive", "Wobble-Drive", 0.0f, 1.0f, 0.2f),
        toggle ("retrig", "Wobble Retrigger", "Wobble-Neustart", true),
        continuous ("mix", "Wobble Mix", "Wobble-Mischung", 0.0f, 1.0f, 1.0f),
    };
}

void WobbleEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    fl.prepare (sr);
    fr.prepare (sr);
    reset();
}

void WobbleEffect::reset() noexcept
{
    fl.reset();
    fr.reset();
    phase = 0.0f;
    held = 0.0f;
    smoothOct = -1.0f;
}

void WobbleEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Rate: rate = std::clamp (v, 0.05f, 20.0f); break;
        case Sync: sync = (int) std::lround (v); break;
        case Shape: shape = std::clamp ((int) std::lround (v), 0, 5); break;
        case Low: lowHz = std::clamp (v, 30.0f, 8000.0f); break;
        case High: highHz = std::clamp (v, 100.0f, 18000.0f); break;
        case Resonance: reso = dsp::clamp01 (v); break;
        case Mode: mode = std::clamp ((int) std::lround (v), 0, 1); break;
        case Drive: drive = dsp::clamp01 (v); break;
        case Retrigger: retrigger = v >= 0.5f; break;
        case Mix: mix = dsp::clamp01 (v); break;
        default: break;
    }
}

void WobbleEffect::process (float* l, float* r, int n) noexcept
{
    const float hz = fxsync::rateHz (sync, rate, tempo);
    const float pre = 1.0f + drive * 8.0f, post = 1.0f / (1.0f + drive * 2.0f);
    const float loOct = std::log2 (lowHz), hiOct = std::log2 (highHz);
    const int m = mode == 1 ? dsp::Svf::BandPass : dsp::Svf::LowPass;
    const float bpGain = mode == 1 ? svfK (0.92f * reso) * 1.5f : 1.0f;
    for (int start = 0; start < n; start += 16)
    {
        const int len = std::min (16, n - start);
        const float u = 0.5f + 0.5f * fxLfoShape (shape, phase, held);
        const float target = loOct + (hiOct - loOct) * u;
        // about 2 ms smoothing in octaves: square and S&H steps do not click
        smoothOct = smoothOct < 0.0f ? target : smoothOct + (target - smoothOct) * (1.0f - std::exp (-(float) len / (0.002f * sr)));
        const float fc = std::exp2 (smoothOct);
        fl.set (fc, 0.92f * reso);
        fr.set (fc, 0.92f * reso);
        for (int i = start; i < start + len; ++i)
        {
            const float wl = fl.process (dsp::softClip (l[i] * pre), m) * post * bpGain;
            const float wr = fr.process (dsp::softClip (r[i] * pre), m) * post * bpGain;
            l[i] = l[i] * (1.0f - mix) + wl * mix;
            r[i] = r[i] * (1.0f - mix) + wr * mix;
        }
        phase += hz * (float) len / sr;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rnd.nextBipolar();
        }
    }
}

// ---------------------------------------------------------------- tremolo / autopan
std::vector<ParamDescriptor> TremoloEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("rate", "Tremolo Rate", "Tremolo-Tempo", 0.05f, 20.0f, 4.0f, ParamUnit::Hertz, 0.4f),
        choice ("sync", "Tremolo Sync", "Tremolo-Sync", fxsync::namesEn(), fxsync::namesDe(), 0),
        continuous ("depth", "Tremolo Depth", "Tremolo-Tiefe", 0.0f, 1.0f, 0.5f),
        choice ("mode", "Tremolo Mode", "Tremolo-Modus", { "Tremolo", "Autopan" }, { "Tremolo", "Autopan" }, 0),
        choice ("shape", "Tremolo Shape", "Tremolo-Form", { "Sine", "Triangle", "Square" }, { "Sinus", "Dreieck", "Rechteck" }, 0),
    };
}

void TremoloEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    reset();
}

void TremoloEffect::reset() noexcept
{
    phase = 0.0f;
    smooth = 0.0f;
}

void TremoloEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Rate: rate = std::clamp (v, 0.05f, 20.0f); break;
        case Sync: sync = (int) std::lround (v); break;
        case Depth: depth = dsp::clamp01 (v); break;
        case Mode: mode = std::clamp ((int) std::lround (v), 0, 1); break;
        case Shape: shape = std::clamp ((int) std::lround (v), 0, 2); break;
        default: break;
    }
}

void TremoloEffect::process (float* l, float* r, int n) noexcept
{
    const float inc = fxsync::rateHz (sync, rate, tempo) / sr;
    const float a = 1.0f - std::exp (-1.0f / (0.003f * sr));
    static constexpr int shapes[3] = { 0, 1, 4 };
    for (int i = 0; i < n; ++i)
    {
        const float v = fxLfoShape (shapes[shape], phase, 0.0f);
        smooth = dsp::flush (smooth + (v - smooth) * a);
        if (mode == 0)
        {
            const float g = 1.0f - depth * (0.5f + 0.5f * smooth);
            l[i] *= g;
            r[i] *= g;
        }
        else
        {
            l[i] *= 1.0f - depth * (0.5f + 0.5f * smooth);
            r[i] *= 1.0f - depth * (0.5f - 0.5f * smooth);
        }
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

// ---------------------------------------------------------------- width
std::vector<ParamDescriptor> WidthEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("width", "Stereo Width", "Stereobreite", 0.0f, 2.0f, 1.0f),
        continuous ("spread", "Stereo Spread", "Stereo-Spreizung", 0.0f, 1.0f, 0.0f),
        continuous ("haas", "Spread Time", "Spreizungszeit", 1.0f, 25.0f, 12.0f, ParamUnit::None, 0.6f),
        continuous ("mono", "Mono Below", "Mono unter", 20.0f, 500.0f, 20.0f, ParamUnit::Hertz, 0.5f),
        continuous ("output", "Width Output", "Breite-Ausgang", -24.0f, 12.0f, 0.0f, ParamUnit::Decibel),
    };
}

void WidthEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    buf.assign ((size_t) (0.03f * sr) + 4, 0.0f);
    lp1.prepare (sr);
    lp2.prepare (sr);
    lp1.set (monoHz, butterQ707);
    lp2.set (monoHz, butterQ707);
    reset();
}

void WidthEffect::reset() noexcept
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    writePos = 0;
    lp1.reset();
    lp2.reset();
}

void WidthEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Width: width = std::clamp (v, 0.0f, 2.0f); break;
        case Spread: spread = dsp::clamp01 (v); break;
        case Haas: haasMs = std::clamp (v, 1.0f, 25.0f); break;
        case MonoBelow:
            monoHz = std::clamp (v, 20.0f, 500.0f);
            lp1.set (monoHz, butterQ707);
            lp2.set (monoHz, butterQ707);
            break;
        case Output: outGain = dsp::dbToGain (std::clamp (v, -24.0f, 12.0f)); break;
        default: break;
    }
}

void WidthEffect::process (float* l, float* r, int n) noexcept
{
    if (buf.empty()) return;
    const int len = (int) buf.size();
    const float d = std::clamp (haasMs * 0.001f * sr, 1.0f, (float) len - 2.0f);
    const bool mono = monoHz > 20.5f;
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (l[i] + r[i]);
        float s = 0.5f * (l[i] - r[i]);
        buf[(size_t) writePos] = m;
        if (spread > 0.0f) s += spread * 0.7f * readFrac (buf, (float) writePos - d); // cancels in the mono sum
        s *= width;
        if (mono) s -= lp2.process (lp1.process (s, dsp::Svf::LowPass), dsp::Svf::LowPass);
        l[i] = (m + s) * outGain;
        r[i] = (m - s) * outGain;
        if (++writePos >= len) writePos = 0;
    }
}
} // namespace ohpe
