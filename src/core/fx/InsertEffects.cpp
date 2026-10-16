// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InsertEffects.h"

namespace ohpe
{
// ---------------------------------------------------------------- chorus
std::vector<ParamDescriptor> ChorusEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("rate", "Chorus Rate", "Chorus-Tempo", 0.05f, 5.0f, 0.6f, ParamUnit::Hertz, 0.5f),
        continuous ("depth", "Chorus Depth", "Chorus-Tiefe", 0.0f, 1.0f, 0.5f),
        continuous ("mix", "Chorus Mix", "Chorus-Mischung", 0.0f, 1.0f, 0.5f),
        // 0.8 punch pass (appended)
        integer ("voices", "Chorus Voices", "Chorus-Stimmen", 1, maxVoices, 1),
        continuous ("drift", "Chorus Drift", "Chorus-Drift", 0.0f, 1.0f, 0.0f),
        continuous ("spread", "Chorus Spread", "Chorus-Breite", 0.0f, 1.0f, 0.8f),
        continuous ("lowcut", "Chorus Low Cut", "Chorus-Tiefensperre", 20.0f, 1000.0f, 20.0f, ParamUnit::Hertz, 0.3f),
    };
}

void ChorusEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    const size_t len = (size_t) (0.05f * sr) + 4;
    bufL.assign (len, 0.0f);
    bufR.assign (len, 0.0f);
    setParam (LowCut, lowCutHz);
    reset();
}

void ChorusEffect::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    phase = 0.0f;
    driftPos.fill (0.0f);
    driftVel.fill (0.0f);
    driftTick = 0;
    seed = 0x2545f491u;
    hpL1 = hpL2 = hpR1 = hpR2 = hpXL1 = hpXL2 = hpXR1 = hpXR2 = 0.0f;
}

void ChorusEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Rate: rate = v; break;
        case Depth: depth = dsp::clamp01 (v); break;
        case Mix: mix = dsp::clamp01 (v); break;
        case Voices: voices = std::clamp ((int) std::lround (v), 1, maxVoices); break;
        case Drift: drift = dsp::clamp01 (v); break;
        case Spread: spread = dsp::clamp01 (v); break;
        case LowCut:
            lowCutHz = std::clamp (v, 20.0f, 1000.0f);
            hpC = std::exp (-dsp::twoPi * lowCutHz / sr);
            break;
        default: break;
    }
}

void ChorusEffect::process (float* l, float* r, int n) noexcept
{
    if (bufL.empty()) return;
    const int len = (int) bufL.size();
    const float inc = rate / sr;
    auto read = [len] (const std::vector<float>& b, float rp) noexcept {
        while (rp < 0.0f) rp += (float) len;
        const int i0 = (int) rp % len;
        const int i1 = (i0 + 1) % len;
        const float f = rp - std::floor (rp);
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
    };
    const bool ensemble = voices > 1;
    const bool lowCut = lowCutHz > 20.5f;
    // 0.2 chorus: one tap per side (12 ms +- 8 ms), quadrature LFOs. Ensemble: 2-4 taps 9 / 13.5 / 18 / 22.5 ms,
    // +- 3.5 ms, LFO phases spread evenly, panned across the field; drift adds a slow random walk to every tap
    const float base = 0.012f * sr, mod = 0.008f * sr * depth;
    const float eBase = 0.009f * sr, eStep = 0.0045f * sr, eMod = 0.0035f * sr * depth;
    const float driftAmt = 0.0018f * sr * drift;
    const float norm = ensemble ? 1.25f / std::sqrt ((float) voices) : 1.0f;
    std::array<float, maxVoices> gl {}, gr {};
    for (int v = 0; v < voices; ++v)
    {
        const float pos = voices > 1 ? ((float) v / (float) (voices - 1)) * 2.0f - 1.0f : 0.0f;
        float a, b;
        dsp::panGains (((v % 2) ? -pos : pos) * spread, a, b);
        gl[(size_t) v] = a * 1.41421356f;
        gr[(size_t) v] = b * 1.41421356f;
    }
    const float driftK = std::min (1.0f, 1.0f / (0.3f * sr));
    for (int i = 0; i < n; ++i)
    {
        bufL[(size_t) writePos] = l[i];
        bufR[(size_t) writePos] = r[i];
        float wl = 0.0f, wr = 0.0f;
        if (drift > 0.0f) // drift: a new random target every ~85 ms, glided to over ~0.3 s - per sample (a delay time
        {                 // stepped every 32 samples was a staircase phase modulation: a faint HF hash under held notes)
            const bool pick = (++driftTick & 4095) == 0;
            for (int v = 0; v < voices; ++v)
            {
                if (pick)
                {
                    seed = seed * 1664525u + 1013904223u;
                    driftVel[(size_t) v] = (float) (seed >> 8) * (1.0f / 8388608.0f) - 1.0f;
                }
                driftPos[(size_t) v] += (driftVel[(size_t) v] - driftPos[(size_t) v]) * driftK;
            }
        }
        if (! ensemble)
        {
            const float a = dsp::twoPi * phase;
            const float dl = base + mod * std::sin (a) + driftAmt * driftPos[0];
            const float dr = base + mod * std::cos (a) - driftAmt * driftPos[0];
            wl = read (bufL, (float) writePos - dl);
            wr = read (bufR, (float) writePos - dr);
        }
        else
            for (int v = 0; v < voices; ++v)
            {
                const float a = dsp::twoPi * (phase + (float) v / (float) voices);
                const float d = eBase + eStep * (float) v + eMod * std::sin (a) + driftAmt * driftPos[(size_t) v];
                const float x = 0.5f * (read (bufL, (float) writePos - d) + read (bufR, (float) writePos - d * 1.07f));
                wl += x * gl[(size_t) v];
                wr += x * gr[(size_t) v];
            }
        wl = wl * norm - l[i]; // wet - dry: out = dry + mix * (wet - dry); the low cut acts on the difference, so the
        wr = wr * norm - r[i]; // low end stays the dry signal at every mix (mono, full level)
        if (lowCut) // 12 dB/oct. high-pass
        {
            const float c = hpC;
            const float yl1 = c * (hpL1 + wl - hpXL1); hpXL1 = wl; hpL1 = yl1;
            const float yl2 = c * (hpL2 + yl1 - hpXL2); hpXL2 = yl1; hpL2 = yl2;
            const float yr1 = c * (hpR1 + wr - hpXR1); hpXR1 = wr; hpR1 = yr1;
            const float yr2 = c * (hpR2 + yr1 - hpXR2); hpXR2 = yr1; hpR2 = yr2;
            wl = yl2;
            wr = yr2;
        }
        l[i] += wl * mix;
        r[i] += wr * mix;
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        if (++writePos >= len) writePos = 0;
    }
    hpL1 = dsp::flush (hpL1); hpL2 = dsp::flush (hpL2); hpR1 = dsp::flush (hpR1); hpR2 = dsp::flush (hpR2);
}

// ---------------------------------------------------------------- phaser
std::vector<ParamDescriptor> PhaserEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("rate", "Phaser Rate", "Phaser-Tempo", 0.05f, 8.0f, 0.4f, ParamUnit::Hertz, 0.5f),
        continuous ("depth", "Phaser Depth", "Phaser-Tiefe", 0.0f, 1.0f, 0.7f),
        continuous ("feedback", "Phaser Feedback", "Phaser-Feedback", 0.0f, 0.95f, 0.5f),
        continuous ("mix", "Phaser Mix", "Phaser-Mischung", 0.0f, 1.0f, 0.5f),
    };
}

void PhaserEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    reset();
}

void PhaserEffect::reset() noexcept
{
    zl.fill (0.0f);
    zr.fill (0.0f);
    fbL = fbR = 0.0f;
    phase = 0.0f;
}

void PhaserEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Rate: rate = v; break;
        case Depth: depth = dsp::clamp01 (v); break;
        case Feedback: feedback = std::clamp (v, 0.0f, 0.95f); break;
        case Mix: mix = dsp::clamp01 (v); break;
        default: break;
    }
}

void PhaserEffect::process (float* l, float* r, int n) noexcept
{
    const float inc = rate / sr;
    for (int i = 0; i < n; ++i)
    {
        const float a = dsp::twoPi * phase;
        // exponential sweep, left / right a quarter period apart
        const float ml = 0.5f + 0.5f * std::sin (a), mr = 0.5f + 0.5f * std::cos (a);
        const float fl = 150.0f * std::pow (33.0f, ml * depth + (1.0f - depth) * 0.3f);
        const float fr = 150.0f * std::pow (33.0f, mr * depth + (1.0f - depth) * 0.3f);
        const float tl = std::tan (dsp::pi * std::min (fl, sr * 0.45f) / sr);
        const float tr = std::tan (dsp::pi * std::min (fr, sr * 0.45f) / sr);
        const float cl = (tl - 1.0f) / (tl + 1.0f), cr = (tr - 1.0f) / (tr + 1.0f);

        float xl = l[i] + fbL * feedback, xr = r[i] + fbR * feedback;
        for (int s = 0; s < numStages; ++s)
        {
            const float yl = cl * xl + zl[(size_t) s];
            zl[(size_t) s] = dsp::flush (xl - cl * yl);
            xl = yl;
            const float yr = cr * xr + zr[(size_t) s];
            zr[(size_t) s] = dsp::flush (xr - cr * yr);
            xr = yr;
        }
        fbL = dsp::flush (std::clamp (xl, -4.0f, 4.0f));
        fbR = dsp::flush (std::clamp (xr, -4.0f, 4.0f));
        l[i] = l[i] * (1.0f - 0.5f * mix) + xl * 0.5f * mix;
        r[i] = r[i] * (1.0f - 0.5f * mix) + xr * 0.5f * mix;
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

// ---------------------------------------------------------------- bitcrush
std::vector<ParamDescriptor> BitcrushEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("bits", "Bit Depth", "Bittiefe", 1.0f, 16.0f, 8.0f),
        continuous ("downsample", "Downsample", "Downsampling", 1.0f, 64.0f, 1.0f, ParamUnit::None, 0.4f),
        continuous ("mix", "Bitcrush Mix", "Bitcrusher-Mischung", 0.0f, 1.0f, 1.0f),
    };
}

void BitcrushEffect::prepare (double, int) { reset(); }

void BitcrushEffect::reset() noexcept
{
    holdL = holdR = 0.0f;
    counter = 1.0f;
}

void BitcrushEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Bits: bits = std::clamp (v, 1.0f, 16.0f); break;
        case Downsample: downsample = std::clamp (v, 1.0f, 64.0f); break;
        case Mix: mix = dsp::clamp01 (v); break;
        default: break;
    }
}

void BitcrushEffect::process (float* l, float* r, int n) noexcept
{
    const float steps = std::pow (2.0f, bits - 1.0f);
    const float step = 1.0f / downsample;
    for (int i = 0; i < n; ++i)
    {
        counter += step;
        if (counter >= 1.0f)
        {
            counter -= 1.0f;
            holdL = std::round (std::clamp (l[i], -1.0f, 1.0f) * steps) / steps;
            holdR = std::round (std::clamp (r[i], -1.0f, 1.0f) * steps) / steps;
        }
        l[i] = l[i] * (1.0f - mix) + holdL * mix;
        r[i] = r[i] * (1.0f - mix) + holdR * mix;
    }
}

// ---------------------------------------------------------------- filter drive
std::vector<ParamDescriptor> FilterDriveEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        continuous ("cutoff", "Drive Cutoff", "Drive-Cutoff", 20.0f, 20000.0f, 2000.0f, ParamUnit::Hertz, 0.3f),
        continuous ("reso", "Drive Resonance", "Drive-Resonanz", 0.0f, 1.0f, 0.3f),
        continuous ("drive", "Drive", "Drive", 0.0f, 1.0f, 0.3f),
        choice ("mode", "Drive Filter Type", "Drive-Filtertyp", { "Low-pass", "Band-pass", "High-pass" },
                { "Tiefpass", "Bandpass", "Hochpass" }, 0),
        continuous ("mix", "Drive Mix", "Drive-Mischung", 0.0f, 1.0f, 1.0f),
    };
}

void FilterDriveEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    svfL.prepare (sr);
    svfR.prepare (sr);
    cutoffSmooth.prepare (sr, 10.0f);
    cutoffSmooth.reset (cutoff);
    update();
    reset();
}

void FilterDriveEffect::reset() noexcept
{
    svfL.reset();
    svfR.reset();
}

void FilterDriveEffect::update() noexcept
{
    svfL.set (cutoffSmooth.value(), resonance * 0.85f); // Q up to ~4: drive, not self-oscillation
    svfR.set (cutoffSmooth.value(), resonance * 0.85f);
}

void FilterDriveEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Cutoff: cutoff = std::clamp (v, 20.0f, 20000.0f); break;
        case Resonance: resonance = dsp::clamp01 (v); break;
        case Drive: drive = dsp::clamp01 (v); break;
        case Mode: mode = std::clamp ((int) std::lround (v), 0, 2); break;
        case Mix: mix = dsp::clamp01 (v); break;
        default: break;
    }
}

void FilterDriveEffect::process (float* l, float* r, int n) noexcept
{
    static constexpr int modes[3] = { dsp::Svf::LowPass, dsp::Svf::BandPass, dsp::Svf::HighPass };
    const float pre = 1.0f + drive * 15.0f;
    const float post = 1.0f / (1.0f + drive * 2.0f);
    cutoffSmooth.setTarget (cutoff);
    for (int i = 0; i < n; ++i)
    {
        if ((i & 15) == 0) { for (int k = 0; k < 16; ++k) cutoffSmooth.next(); update(); }
        const float wl = svfL.process (dsp::softClip (l[i] * pre), modes[mode]) * post;
        const float wr = svfR.process (dsp::softClip (r[i] * pre), modes[mode]) * post;
        l[i] = l[i] * (1.0f - mix) + wl * mix;
        r[i] = r[i] * (1.0f - mix) + wr * mix;
    }
}
} // namespace ohpe
