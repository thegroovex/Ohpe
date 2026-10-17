// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AnalogEngine.h"

namespace ohpe
{
namespace analog
{
void Voice::prepare (float sampleRate) noexcept
{
    prepareBase (sampleRate);
    ladder.prepare (sampleRate);
    svf.prepare (sampleRate);
}

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    const bool wasActive = isActive();
    startBase (midiNote, vel, legato, c);
    if (! wasActive)
    {
        osc1.reset (0.0f);
        osc2.reset (0.37f);
        subOsc.reset (0.0f);
        ladder.reset();
        svf.reset();
        noise.seed ((uint32_t) (midiNote * 7919 + 1));
        crackleRng.seed ((uint32_t) (midiNote * 104729 + 7));
        crackleState = crackleDust = crackleLp = 0.0f;
    }
}

void Voice::kill() noexcept
{
    killBase();
    ladder.reset();
    svf.reset();
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        const float pitch = controlTick (c, n);
        const float detuneCents = p.detune * 30.0f;
        const float f1 = dsp::midiToHz (pitch - detuneCents * 0.005f);
        const float f2 = dsp::midiToHz (pitch + 12.0f * p.osc2Octave + detuneCents * 0.01f);
        const float fSub = dsp::midiToHz (pitch - 12.0f);

        // filter cutoff: base * key tracking * mod envelope (+-6 octaves) * LFO timbre (+-2 octaves)
        float octaves = p.keyTrack * (pitch - 60.0f) / 12.0f + c.modEnvAmount * modEnv.getLevel() * 6.0f;
        if (c.lfoDest == LfoDest::Timbre) octaves += lfoValue * 2.0f;
        octaves += (expr.timbre + expr.cutoff) * 3.0f; // per-note expression (MPE; 0.8: cutoff target)
        const float cutoff = std::clamp (p.cutoff * std::pow (2.0f, octaves), 20.0f, 20000.0f);
        if (p.filterType == LadderLP) ladder.set (cutoff, p.resonance);
        else svf.set (cutoff, p.resonance);

        float panL, panR;
        lfoPan (c, panL, panR);
        const float gain = 0.3f * velocityGain (c) * lfoAmp (c);
        const float drive = 1.0f + std::clamp (p.drive + expr.drive, 0.0f, 1.5f) * 4.0f; // 0.8: per-note drive
        const float mix1 = (1.0f - p.oscMix) * p.oscLevel, mix2 = p.oscMix * p.oscLevel;
        // 0.6 crackle: pops (rare, louder) and dust (frequent, tiny), both as short decaying clicks
        const float popChance = p.crackle > 0.0f ? (3.0f + 22.0f * p.crackle) / sr : 0.0f;
        const float dustChance = p.crackle > 0.0f ? (40.0f + 260.0f * p.crackle) / sr : 0.0f;
        const float popLevel = 2.2f * std::sqrt (p.crackle), dustLevel = 0.35f * std::sqrt (p.crackle);

        for (int i = 0; i < n; ++i)
        {
            float x = mix1 * osc1.next (f1, sr, p.shape) + mix2 * osc2.next (f2, sr, p.shape);
            x += p.sub * subOsc.next (fSub, sr);
            x += p.noise * noise.nextBipolar();
            if (subOn && ! subDirectNow) x += 1.48f * subNext(); // p31: SUB layer into the filter (same loudness as direct)
            if (popChance > 0.0f)
            {
                crackleState *= 0.55f;
                crackleDust *= 0.3f;
                if (crackleRng.next01() < popChance)
                {
                    const float a = crackleRng.next01();
                    crackleState += (crackleRng.next01() < 0.5f ? -1.0f : 1.0f) * popLevel * (0.25f + 0.75f * a * a);
                }
                if (crackleRng.next01() < dustChance)
                    crackleDust += (crackleRng.next01() < 0.5f ? -1.0f : 1.0f) * dustLevel * crackleRng.next01();
                crackleLp += 0.55f * (crackleState + crackleDust - crackleLp); // soften the edges (~6 kHz)
                x += crackleLp;
            }
            x *= 0.7f;
            float y;
            switch (p.filterType)
            {
                case SvfLP: y = dsp::softClip (svf.process (x * drive, dsp::Svf::LowPass)); break;
                case SvfBP: y = dsp::softClip (svf.process (x * drive, dsp::Svf::BandPass)); break;
                case SvfHP: y = dsp::softClip (svf.process (x * drive, dsp::Svf::HighPass)); break;
                default: y = ladder.process (x, drive); break;
            }
            modEnv.next();
            const float ae = ampEnv.next();
            const float out = y * ae * gain;
            l[done + i] += out * panL;
            r[done + i] += out * panR;
            if (subOn && subDirectNow) subAddDirect (ae, l[done + i], r[done + i]); // p31: SUB layer after the filter
        }
        done += n;
    }
}
} // namespace analog

AnalogEngine::AnalogEngine()
{
    for (auto& v : voices) v.shared = &shared;
}

std::vector<ParamDescriptor> AnalogEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("shape", "Shape", "Form", 0.0f, 1.0f, 0.2f));
    d.push_back (continuous ("detune", "Detune", "Verstimmung", 0.0f, 1.0f, 0.15f));
    d.push_back (continuous ("cutoff", "Cutoff", "Cutoff", 20.0f, 20000.0f, 3000.0f, ParamUnit::Hertz, 0.25f));
    d.push_back (continuous ("reso", "Resonance", "Resonanz", 0.0f, 1.0f, 0.2f));
    d.push_back (integer ("osc2oct", "Osc 2 Octave", "Osz. 2 Oktave", -2, 2, 0));
    d.push_back (continuous ("oscmix", "Osc Mix", "Osz.-Mix", 0.0f, 1.0f, 0.5f));
    d.push_back (continuous ("sub", "Sub", "Sub", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("noise", "Noise", "Rauschen", 0.0f, 1.0f, 0.0f));
    d.push_back (choice ("ftype", "Filter Type", "Filtertyp", { "Ladder LP", "SVF LP", "SVF BP", "SVF HP" },
                         { "Ladder TP", "SVF TP", "SVF BP", "SVF HP" }, 0));
    d.push_back (continuous ("drive", "Drive", "Drive", 0.0f, 1.0f, 0.2f));
    d.push_back (continuous ("keytrack", "Key Track", "Key-Tracking", 0.0f, 1.0f, 0.5f));
    // 0.6 (late): oscillator level and vinyl crackle
    d.push_back (continuous ("osclevel", "Osc Level", "Osz.-Pegel", 0.0f, 1.0f, 1.0f));
    d.push_back (continuous ("crackle", "Crackle", "Knistern", 0.0f, 1.0f, 0.0f));
    return d;
}

void AnalogEngine::setParam (int i, float v) noexcept
{
    using namespace analog;
    switch (i)
    {
        case Shape: shared.shape = v; break;
        case Detune: shared.detune = v; break;
        case Cutoff: shared.cutoff = v; break;
        case Resonance: shared.resonance = v; break;
        case Osc2Octave: shared.osc2Octave = v; break;
        case OscMix: shared.oscMix = v; break;
        case Sub: shared.sub = v; break;
        case Noise: shared.noise = v; break;
        case FilterType: shared.filterType = (int) v; break;
        case Drive: shared.drive = v; break;
        case KeyTrack: shared.keyTrack = v; break;
        case OscLevel: shared.oscLevel = v; break;
        case Crackle: shared.crackle = v; break;
        default: break;
    }
}
} // namespace ohpe
