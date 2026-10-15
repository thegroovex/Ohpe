// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "VocalEngine.h"

#include <cmath>

namespace ohpe
{
namespace vocal
{
const FormantSet& vowelFormants (int vowel) noexcept
{
    // classic low-voice formant data: frequency (Hz), level (dB), bandwidth (Hz)
    static const FormantSet table[numVowels] = {
        { { 600, 1040, 2250, 2450, 2750 }, { 0, -7, -9, -9, -20 }, { 60, 70, 110, 120, 130 } },   // A
        { { 400, 1620, 2400, 2800, 3100 }, { 0, -12, -9, -12, -18 }, { 40, 80, 100, 120, 120 } }, // E
        { { 250, 1750, 2600, 3050, 3340 }, { 0, -30, -16, -22, -28 }, { 60, 90, 100, 120, 120 } }, // I
        { { 400, 750, 2400, 2600, 2900 }, { 0, -11, -21, -20, -40 }, { 40, 80, 100, 120, 120 } },  // O
        { { 350, 600, 2400, 2675, 2950 }, { 0, -20, -32, -28, -36 }, { 40, 80, 100, 120, 120 } },  // U
    };
    return table[std::clamp (vowel, 0, numVowels - 1)];
}

FormantSet morphVowel (float position) noexcept
{
    const float p = std::clamp (position, 0.0f, (float) (numVowels - 1));
    const int a = std::min ((int) p, numVowels - 2);
    const float t = p - (float) a;
    const auto& va = vowelFormants (a);
    const auto& vb = vowelFormants (a + 1);
    FormantSet out {};
    for (int k = 0; k < numFormants; ++k)
    {
        out.freq[k] = va.freq[k] * std::pow (vb.freq[k] / va.freq[k], t); // geometric: even in pitch
        out.gainDb[k] = dsp::lerp (va.gainDb[k], vb.gainDb[k], t);
        out.bandwidth[k] = dsp::lerp (va.bandwidth[k], vb.bandwidth[k], t);
    }
    return out;
}

void Resonator::set (float freq, float bandwidth, float sr) noexcept
{
    const float f = std::clamp (freq, 30.0f, sr * 0.45f);
    const float q = std::max (0.5f, f / std::max (10.0f, bandwidth));
    const float w0 = dsp::twoPi * f / sr;
    const float alpha = std::sin (w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    a1 = -2.0f * std::cos (w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

float Resonator::process (float x) noexcept
{
    const float y = b0 * x + z1;
    z1 = dsp::flush (-a1 * y + z2);
    z2 = dsp::flush (-b0 * x - a2 * y);
    return y;
}

float Voice::glottalFlow (float ph, float oq) noexcept
{
    // Rosenberg pulse: opening (rising cosine), closing (quarter cosine), closed phase
    const float tp = 0.66f * oq, tn = 0.34f * oq;
    if (ph < tp) return 0.5f * (1.0f - std::cos (dsp::pi * ph / tp));
    if (ph < tp + tn) return std::cos (0.5f * dsp::pi * (ph - tp) / tn);
    return 0.0f;
}

void Voice::prepare (float sampleRate) noexcept { prepareBase (sampleRate); }

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    const bool wasActive = isActive();
    startBase (midiNote, vel, legato, c);
    if (! wasActive)
    {
        phase = 0.0;
        lastFlow = 0.0f;
        vibPhase = 0.0f;
        vibFade = 0.0f;
        jitterValue = 0.0f;
        noiseLp = 0.0f;
        for (auto& r : res) r.reset();
        tilt.reset();
        air.reset();
        rng.seed ((uint32_t) (midiNote * 40503 + 7));
    }
}

void Voice::kill() noexcept
{
    killBase();
    for (auto& r : res) r.reset();
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        float pitch = controlTick (c, n);

        vibPhase += dsp::twoPi * p.vibratoRate * (float) n / sr;
        if (vibPhase > dsp::twoPi) vibPhase -= dsp::twoPi;
        jitterValue += (rng.nextBipolar() - jitterValue) * 0.05f; // slow random walk
        vibFade = std::min (1.0f, vibFade + (float) n / (0.45f * sr)); // vibrato sets in after the attack
        pitch += vibFade * p.vibratoDepth * 0.5f * std::sin (vibPhase) + p.jitter * 0.3f * jitterValue;
        const double inc = std::min (0.45, (double) dsp::midiToHz (pitch) / (double) sr);

        float vowel = p.vowel + c.modEnvAmount * modEnv.getLevel() * 2.0f;
        if (c.lfoDest == LfoDest::Timbre) vowel += lfoValue * 2.0f;
        const FormantSet fs = morphVowel (vowel);
        const float shift = dsp::semitonesToRatio (p.formantShift);
        for (int k = 0; k < numFormants; ++k)
        {
            res[(size_t) k].set (fs.freq[k] * shift, fs.bandwidth[k] * std::sqrt (shift), sr);
            gains[(size_t) k] = dsp::dbToGain (fs.gainDb[k]);
        }

        const float oq = 0.85f - 0.5f * p.tension;
        tilt.setCutoff (500.0f * std::pow (2.0f, p.tension * 5.0f), sr); // lax = darker, pressed = brighter
        air.setCutoff (6500.0f * std::pow (2.0f, p.tension), sr);           // tames fizz above the formants
        const float voiced = 1.0f - 0.8f * p.breath;
        const float breathy = p.breath * 0.6f;
        float panL, panR;
        lfoPan (c, panL, panR);
        const float gain = 0.9f * velocityGain (c) * lfoAmp (c);
        const auto incF = (float) inc;

        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            const float flow = glottalFlow ((float) phase, oq);
            float d = flow - lastFlow;
            if (d > 0.5f || d < -0.5f) d = 0.0f; // period wrap
            lastFlow = flow;
            const float pulse = d / incF * 0.12f;

            const float white = rng.nextBipolar();
            noiseLp += (white - noiseLp) * 0.5f;
            const float noise = (white - noiseLp) * (0.35f + 0.65f * flow);

            const float exc = voiced * tilt.lowpass (pulse) + breathy * noise;
            float y = 0.0f;
            for (int k = 0; k < numFormants; ++k) y += gains[(size_t) k] * res[(size_t) k].process (exc);

            modEnv.next();
            const float out = air.lowpass (dsp::softClip (y * 2.5f)) * ampEnv.next() * gain;
            l[done + i] += out * panL;
            r[done + i] += out * panR;
        }
        done += n;
    }
}
} // namespace vocal

VocalEngine::VocalEngine()
{
    for (auto& v : voices) v.shared = &shared;
}

std::vector<ParamDescriptor> VocalEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("vowel", "Vowel", "Vokal", 0.0f, 4.0f, 0.0f));
    d.push_back (continuous ("formant", "Formant Shift", "Formant-Verschiebung", -12.0f, 12.0f, 0.0f, ParamUnit::Semitones));
    d.push_back (continuous ("breath", "Breath", "Atem", 0.0f, 1.0f, 0.15f));
    d.push_back (continuous ("tension", "Tension", "Spannung", 0.0f, 1.0f, 0.5f));
    d.push_back (continuous ("vibdepth", "Vibrato Depth", "Vibrato-Tiefe", 0.0f, 1.0f, 0.2f));
    d.push_back (continuous ("vibrate", "Vibrato Rate", "Vibrato-Tempo", 0.5f, 10.0f, 5.5f, ParamUnit::Hertz));
    d.push_back (continuous ("jitter", "Jitter", "Jitter", 0.0f, 1.0f, 0.1f));
    return d;
}

void VocalEngine::setParam (int i, float v) noexcept
{
    using namespace vocal;
    switch (i)
    {
        case Vowel: shared.vowel = std::clamp (v, 0.0f, 4.0f); break;
        case Formant: shared.formantShift = std::clamp (v, -12.0f, 12.0f); break;
        case Breath: shared.breath = dsp::clamp01 (v); break;
        case Tension: shared.tension = dsp::clamp01 (v); break;
        case VibratoDepth: shared.vibratoDepth = dsp::clamp01 (v); break;
        case VibratoRate: shared.vibratoRate = std::clamp (v, 0.5f, 10.0f); break;
        case Jitter: shared.jitter = dsp::clamp01 (v); break;
        default: break;
    }
}
} // namespace ohpe
