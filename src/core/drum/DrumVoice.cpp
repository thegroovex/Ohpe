// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DrumVoice.h"

#include <cmath>

namespace ohpe::drum
{
namespace
{
constexpr double twoPiD = 6.283185307179586;
// inharmonic ratios for the metallic hat (six square oscillators)
constexpr float hatRatios[6] = { 1.0f, 1.3420f, 1.2312f, 1.6532f, 1.9523f, 2.1523f };
} // namespace

void DrumVoice::prepare (float sampleRate) noexcept
{
    sr = sampleRate > 0.0f ? sampleRate : 48000.0f;
    bp.prepare (sr);
    hp.prepare (sr);
    kill();
}

void DrumVoice::trigger (int padIndex, const PadSettings& s, float velocity, const SampleData* padSample, uint64_t startOrder) noexcept
{
    settings = s;
    pad = padIndex;
    order = startOrder;
    t = 0;
    chokeGain = 1.0f;
    chokeStep = 0.0f;
    for (auto& p : ph) p = 0.0;
    bp.reset();
    hp.reset();
    lp.reset();
    lp2.reset();
    noise.seed ((uint32_t) (padIndex * 7477 + 101) ^ (uint32_t) (startOrder * 2654435761u));

    const float vel = std::clamp (velocity, 0.0f, 1.0f);
    const float g = settings.level * (0.25f + 0.75f * vel);
    dsp::panGains (settings.pan, gainL, gainR);
    gainL *= g * 1.41421356f;
    gainR *= g * 1.41421356f;

    sample = nullptr;
    sampleMode = settings.source == PadSource::Sample && padSample != nullptr && padSample->numFrames() >= 2;
    amp = 1.0f;
    amp2 = 1.0f;
    pitchEnv = 1.0f;
    const float d = dsp::clamp01 (settings.decay);

    if (sampleMode)
    {
        // a little humanising per hit: +-4 cents, +-0.4 dB
        const float cents = 4.0f * noise.nextBipolar();
        const float gainVar = dsp::dbToGain (0.4f * noise.nextBipolar());
        gainL *= gainVar;
        gainR *= gainVar;
        sample = padSample;
        rate = (double) dsp::semitonesToRatio (settings.tune + cents * 0.01f) * sample->sampleRate() / (double) sr;
        pos = 0.0;
        ampCoeff = d >= 0.5f ? 1.0f : coeffFor (0.02f + 3.0f * (d / 0.5f) * (d / 0.5f));
        // tone below 0.5 darkens; soft hits are darker, like a real drum
        float cutoff = settings.tone < 0.5f ? 200.0f * std::pow (2.0f, settings.tone * 2.0f * 6.6f) : 20000.0f;
        cutoff *= std::pow (2.0f, -(1.0f - vel) * 2.5f);
        sampleFilter = cutoff < 18000.0f;
        lp.setCutoff (cutoff, sr);
        lp2.setCutoff (cutoff, sr);
        active = true;
        return;
    }

    // synth model: the pad's own voicing plus the tune / decay / tone parameters
    const float k = dsp::semitonesToRatio (settings.tune + settings.modelTune);
    const float dm = std::pow (4.0f, 2.0f * d - 1.0f);           // decay 0..1 -> 0.25x .. 4x
    const float md = dsp::clamp01 (settings.modelDecay);
    settings.tone = dsp::clamp01 (settings.modelTone + settings.tone - 0.5f);
    const float tone = settings.tone;
    auto decayFor = [&] (float seconds) { return coeffFor (seconds * dm); };

    switch (settings.type)
    {
        case PadType::Kick:
            baseFreq = 48.0f * k;
            ampCoeff = decayFor (0.12f + md * 1.6f);
            amp2Coeff = coeffFor (0.004f);                 // click
            pitchCoeff = coeffFor (0.04f + 0.1f * (1.0f - tone));
            break;
        case PadType::Snare:
            baseFreq = 185.0f * k;
            ampCoeff = decayFor (0.06f + md * 0.4f);       // body
            amp2Coeff = decayFor (0.08f + md * 0.7f);      // snares (noise)
            pitchCoeff = coeffFor (0.02f);
            hp.set (1200.0f + tone * 4000.0f, 0.1f);
            break;
        case PadType::Hat:
            baseFreq = 330.0f * k;
            ampCoeff = decayFor (0.02f + md * md * 1.6f);
            hp.set (6000.0f + 3000.0f * tone, 0.2f);
            bp.set (9000.0f + 2000.0f * tone, 0.3f);
            break;
        case PadType::Clap:
            baseFreq = 1100.0f * k;
            ampCoeff = coeffFor (0.009f);                  // each burst
            amp2Coeff = decayFor (0.08f + md * 0.8f);      // tail
            bp.set (baseFreq * (0.8f + 0.8f * tone), 0.45f);
            break;
        case PadType::Tom:
            baseFreq = 105.0f * k;
            ampCoeff = decayFor (0.12f + md * 1.3f);
            amp2Coeff = coeffFor (0.01f);
            pitchCoeff = coeffFor (0.08f);
            break;
        case PadType::Perc:
            baseFreq = 420.0f * k;
            ampCoeff = decayFor (0.04f + md * 0.8f);
            amp2Coeff = decayFor (0.01f + md * 0.15f);     // FM index
            break;
        case PadType::numTypes: kill(); return;
    }
    active = true;
}

void DrumVoice::choke() noexcept
{
    if (active && chokeStep == 0.0f) chokeStep = 1.0f / (0.003f * sr);
}

float DrumVoice::nextSynth() noexcept
{
    const auto fsr = (double) sr;
    float y = 0.0f;
    switch (settings.type)
    {
        case PadType::Kick:
        {
            const float f = baseFreq * (1.0f + 3.5f * pitchEnv);
            ph[0] += (double) f / fsr;
            if (ph[0] >= 1.0) ph[0] -= 1.0;
            const float body = std::sin ((float) (twoPiD * ph[0]));
            const float click = noise.nextBipolar() * amp2 * (0.2f + 0.6f * settings.tone);
            y = dsp::softClip ((body * amp + click) * (1.0f + settings.tone * 1.5f)) * 0.9f;
            pitchEnv *= pitchCoeff;
            amp *= ampCoeff;
            amp2 *= amp2Coeff;
            break;
        }
        case PadType::Snare:
        {
            const float f = baseFreq * (1.0f + 0.4f * pitchEnv);
            ph[0] += (double) f / fsr;
            ph[1] += (double) f * 1.78 / fsr;
            if (ph[0] >= 1.0) ph[0] -= 1.0;
            if (ph[1] >= 1.0) ph[1] -= 1.0;
            const float body = 0.6f * std::sin ((float) (twoPiD * ph[0])) + 0.4f * std::sin ((float) (twoPiD * ph[1]));
            const float snares = hp.process (noise.nextBipolar(), dsp::Svf::HighPass);
            y = 0.55f * body * amp + 0.5f * snares * amp2;
            pitchEnv *= pitchCoeff;
            amp *= ampCoeff;
            amp2 *= amp2Coeff;
            break;
        }
        case PadType::Hat:
        {
            float metal = 0.0f;
            for (int i = 0; i < 6; ++i)
            {
                ph[i] += (double) (baseFreq * hatRatios[i]) / fsr;
                if (ph[i] >= 1.0) ph[i] -= 1.0;
                metal += ph[i] < 0.5 ? 1.0f : -1.0f;
            }
            const float x = metal * (1.0f / 6.0f) * 0.7f + noise.nextBipolar() * 0.3f;
            const float h = hp.process (x, dsp::Svf::HighPass);
            y = (0.6f * h + 0.4f * bp.process (h, dsp::Svf::BandPass)) * amp * 0.8f;
            amp *= ampCoeff;
            break;
        }
        case PadType::Clap:
        {
            // three short bursts 10 ms apart, then the tail
            const auto burstLen = (int64_t) (0.010f * sr);
            if (t > 0 && t < 3 * burstLen && t % burstLen == 0) amp = 1.0f;
            const float burst = t < 3 * burstLen ? amp : 0.0f;
            const float tail = t >= 2 * burstLen ? amp2 * 0.5f : 0.0f;
            const float n = bp.process (noise.nextBipolar(), dsp::Svf::BandPass);
            y = n * (burst + tail) * 1.6f;
            amp *= ampCoeff;
            if (t >= 2 * burstLen) amp2 *= amp2Coeff;
            break;
        }
        case PadType::Tom:
        {
            const float f = baseFreq * (1.0f + 0.6f * pitchEnv);
            ph[0] += (double) f / fsr;
            if (ph[0] >= 1.0) ph[0] -= 1.0;
            const float body = std::sin ((float) (twoPiD * ph[0]));
            const float stick = noise.nextBipolar() * amp2 * 0.3f * (0.3f + settings.tone);
            y = dsp::softClip (body * amp * (1.0f + settings.tone) + stick) * 0.85f;
            pitchEnv *= pitchCoeff;
            amp *= ampCoeff;
            amp2 *= amp2Coeff;
            break;
        }
        case PadType::Perc:
        {
            // two-operator FM blip: modulator ratio from the tone, index decays fast
            const double ratio = 1.4 + 2.6 * (double) settings.tone;
            ph[1] += (double) baseFreq * ratio / fsr;
            if (ph[1] >= 1.0) ph[1] -= 1.0;
            const float mod = std::sin ((float) (twoPiD * ph[1])) * 3.0f * amp2;
            ph[0] += (double) baseFreq / fsr;
            if (ph[0] >= 1.0) ph[0] -= 1.0;
            y = std::sin ((float) (twoPiD * ph[0]) + mod) * amp * 0.7f;
            amp *= ampCoeff;
            amp2 *= amp2Coeff;
            break;
        }
        default: break;
    }
    return y;
}

void DrumVoice::render (float* l, float* r, int numSamples, const SampleData* currentSample) noexcept
{
    if (! active) return;
    if (sampleMode && currentSample != sample)
    {
        kill(); // the pad's sample was replaced: never read a retired one
        return;
    }
    for (int i = 0; i < numSamples; ++i)
    {
        float a, b;
        if (sampleMode)
        {
            const int64_t frames = sample->numFrames();
            a = readHermite (sample->channel (0), frames, pos);
            b = sample->numChannels() > 1 ? readHermite (sample->channel (1), frames, pos) : a;
            if (sampleFilter)
            {
                a = lp.lowpass (a);
                b = lp2.lowpass (b);
            }
            a *= amp;
            b *= amp;
            pos += rate;
            amp *= ampCoeff;
            if (pos >= (double) frames) amp = 0.0f;
        }
        else
            a = b = nextSynth();

        if (chokeStep > 0.0f)
        {
            chokeGain -= chokeStep;
            if (chokeGain <= 0.0f) { kill(); return; }
        }
        l[i] += a * gainL * chokeGain;
        r[i] += b * gainR * chokeGain;
        ++t;

        bool silent = amp < 1.0e-4f;
        if (! sampleMode && settings.type == PadType::Clap) silent = t > (int64_t) (0.03f * sr) && amp2 < 1.0e-4f;
        else if (! sampleMode && settings.type == PadType::Snare) silent = std::max (amp, amp2) < 1.0e-4f;
        if (silent) { kill(); return; }
    }
}
} // namespace ohpe::drum
