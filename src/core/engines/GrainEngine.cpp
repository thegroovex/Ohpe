// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GrainEngine.h"

#include "../sample/FactoryBank.h"

#include <cmath>

namespace ohpe
{
namespace grain
{
void Voice::prepare (float sampleRate) noexcept
{
    prepareBase (sampleRate);
    for (auto& f : filter) f.prepare (sampleRate);
}

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    const bool wasActive = isActive();
    startBase (midiNote, vel, legato, c);
    if (! wasActive)
    {
        for (auto& g : grains) g.active = false;
        for (auto& f : filter) f.reset();
        rng.seed ((uint32_t) (midiNote * 2654435761u + 17u));
        scanOffset = 0.0;
        untilNext = 0.0;
    }
    source = shared->activeSource (sourceRoot);
}

const IAudioRegion* Shared::factorySource (int instrument, int& root) noexcept { return factory::grainSource (instrument, root); }

void Voice::kill() noexcept
{
    killBase();
    for (auto& g : grains) g.active = false;
    for (auto& f : filter) f.reset();
    source = nullptr;
}

int Voice::activeGrains() const noexcept
{
    int n = 0;
    for (const auto& g : grains) n += g.active ? 1 : 0;
    return n;
}

void Voice::spawn (const IAudioRegion& src, float pitch) noexcept
{
    const Shared& p = *shared;
    Grain* g = nullptr;
    for (auto& candidate : grains)
        if (! candidate.active) { g = &candidate; break; }
    if (g == nullptr) return;

    const auto frames = (double) src.numFrames();
    if (frames < 4.0) return;
    const double srcRate = src.sampleRate();
    const double ratio = std::pow (2.0, (double) (pitch - (float) sourceRoot + p.pitch) / 12.0) * srcRate / (double) sr;
    g->length = std::max (16, (int) (p.sizeMs * 0.001f * sr));

    double centre = (double) std::clamp (p.position + 0.5f * expr.position, 0.0f, 1.0f) * frames + scanOffset * srcRate; // 0.8: MPE position
    centre += (double) (p.spray * rng.nextBipolar()) * std::min (0.5 * srcRate, frames * 0.5);
    const bool reverse = rng.next01() < p.reverseProb;
    double startPos = reverse ? centre + ratio * g->length : centre;
    startPos = std::fmod (startPos, frames);
    if (startPos < 0.0) startPos += frames;

    g->pos = startPos;
    g->inc = reverse ? -ratio : ratio;
    g->age = 0;
    dsp::panGains (p.spread * rng.nextBipolar(), g->gainL, g->gainR);
    g->active = true;
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    int root = rootNote;
    const IAudioRegion* src = p.activeSource (root);
    if (src != source)
    {
        for (auto& g : grains) g.active = false; // never read a replaced source
        source = src;
    }
    sourceRoot = root;

    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        const float pitch = controlTick (c, n);

        float octaves = c.modEnvAmount * modEnv.getLevel() * 6.0f;
        if (c.lfoDest == LfoDest::Timbre) octaves += lfoValue * 2.0f;
        octaves += (expr.timbre + expr.cutoff) * 3.0f; // 0.8: per-note expression (MPE)
        const float cutoff = std::clamp (40.0f * std::pow (2.0f, p.tone * 9.0f + octaves), 20.0f, 20000.0f);
        const bool useFilter = cutoff < 18000.0f;
        if (useFilter)
            for (auto& f : filter) f.set (cutoff, 0.1f);

        float panL, panR;
        lfoPan (c, panL, panR);
        const float density = std::clamp (p.density * std::exp2 (2.0f * expr.density), 0.5f, 200.0f); // 0.8: MPE density
        const float overlap = std::max (1.0f, density * p.sizeMs * 0.001f);
        const float gain = velocityGain (c) * lfoAmp (c) * 1.2f / std::sqrt (overlap);
        const double interval = (double) sr / std::max (0.1f, density);

        const float* ch0 = source != nullptr ? source->channel (0) : nullptr;
        const float* ch1 = source != nullptr ? source->channel (1) : nullptr;
        const int64_t frames = source != nullptr ? source->numFrames() : 0;
        const auto framesD = (double) frames;

        for (int i = 0; i < n; ++i)
        {
            untilNext -= 1.0;
            if (untilNext <= 0.0)
            {
                if (source != nullptr) spawn (*source, pitch);
                untilNext += interval * (1.0 + 0.5 * (double) (p.spray * rng.nextBipolar()));
            }

            float a = 0.0f, b = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float w = 0.5f - 0.5f * std::cos (dsp::twoPi * (float) g.age / (float) g.length);
                const float s0 = readHermite (ch0, frames, g.pos);
                const float s1 = ch1 == ch0 ? s0 : readHermite (ch1, frames, g.pos);
                a += s0 * w * g.gainL;
                b += s1 * w * g.gainR;
                g.pos += g.inc;
                if (g.pos >= framesD) g.pos -= framesD;
                else if (g.pos < 0.0) g.pos += framesD;
                if (++g.age >= g.length) g.active = false;
            }
            if (useFilter)
            {
                a = filter[0].process (a, dsp::Svf::LowPass);
                b = filter[1].process (b, dsp::Svf::LowPass);
            }
            modEnv.next();
            const float env = ampEnv.next() * gain;
            l[done + i] += a * env * panL * 1.41421356f;
            r[done + i] += b * env * panR * 1.41421356f;
        }

        if (source != nullptr && frames > 0)
        {
            const double dur = framesD / source->sampleRate();
            scanOffset = std::fmod (scanOffset + (double) p.scan * n / (double) sr, dur);
        }
        done += n;
    }
    if (! isActive())
        for (auto& g : grains) g.active = false;
}
} // namespace grain

GrainEngine::GrainEngine()
{
    builtInTone = sample::makePadTexture (48000.0, (float) grain::rootNote, 6.0f);
    source.set (builtInTone);
    shared.sample = &source;
    shared.tapeRegion = &tapeRegion;
    shared.userLoaded = &userLoaded;
    for (auto& v : voices) v.shared = &shared;
}

std::vector<ParamDescriptor> GrainEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("pos", "Position", "Position", 0.0f, 1.0f, 0.3f, ParamUnit::Percent));
    d.push_back (continuous ("size", "Grain Size", "Korngröße", 5.0f, 500.0f, 120.0f, ParamUnit::None, 0.5f));
    d.push_back (continuous ("density", "Density", "Dichte", 1.0f, 100.0f, 24.0f, ParamUnit::Hertz, 0.5f));
    d.push_back (continuous ("spray", "Spray", "Streuung", 0.0f, 1.0f, 0.15f));
    d.push_back (continuous ("pitch", "Grain Pitch", "Korn-Tonhöhe", -24.0f, 24.0f, 0.0f, ParamUnit::Semitones));
    d.push_back (continuous ("reverse", "Reverse Chance", "Rückwärts-Anteil", 0.0f, 1.0f, 0.0f, ParamUnit::Percent));
    d.push_back (continuous ("spread", "Stereo Spread", "Stereobreite", 0.0f, 1.0f, 0.6f));
    d.push_back (continuous ("scan", "Scan Speed", "Scan-Tempo", -2.0f, 2.0f, 0.1f));
    d.push_back (choice ("source", "Source", "Quelle", { "Sample", "Tape" }, { "Sample", "Tape" }, 0));
    d.push_back (continuous ("tone", "Tone", "Klang", 0.0f, 1.0f, 1.0f));
    // 0.4 (late parameter, see firstLateParam())
    d.push_back (choice ("inst", "Instrument", "Instrument", factory::instrumentNamesEn(), factory::instrumentNamesDe(), 0));
    // 0.8 (late parameter): the keyboard instruments; when not Off it replaces "inst"
    d.push_back (choice ("inst2", "Keyboard Instrument", "Tasteninstrument", factory::keyboardChoiceEn(), factory::keyboardChoiceDe(), 0));
    return d;
}

void GrainEngine::setParam (int i, float v) noexcept
{
    using namespace grain;
    switch (i)
    {
        case Position: shared.position = dsp::clamp01 (v); break;
        case Size: shared.sizeMs = std::clamp (v, 5.0f, 500.0f); break;
        case Density: shared.density = std::clamp (v, 1.0f, 100.0f); break;
        case Spray: shared.spray = dsp::clamp01 (v); break;
        case Pitch: shared.pitch = std::clamp (v, -24.0f, 24.0f); break;
        case ReverseProb: shared.reverseProb = dsp::clamp01 (v); break;
        case Spread: shared.spread = dsp::clamp01 (v); break;
        case Scan: shared.scan = std::clamp (v, -2.0f, 2.0f); break;
        case Source: shared.source = std::clamp ((int) v, 0, (int) SourceTape); break;
        case Tone: shared.tone = dsp::clamp01 (v); break;
        case Instrument:
        case Instrument2:
            (i == Instrument ? shared.instSel : shared.inst2Sel) = std::max (0, (int) std::lround (v));
            shared.instrument = std::clamp (factory::effectiveInstrument (shared.instSel, shared.inst2Sel), 0, factory::numInstruments - 1);
            break;
        default: break;
    }
}

void GrainEngine::setSample (SamplePtr s)
{
    userSample = s;
    userLoaded.store (s != nullptr, std::memory_order_release);
    source.set (s != nullptr ? std::move (s) : builtInTone);
}
} // namespace ohpe
