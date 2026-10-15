// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SamplerEngine.h"

#include <cmath>

namespace ohpe
{
namespace sampler
{
void Voice::prepare (float sampleRate) noexcept
{
    prepareBase (sampleRate);
    for (auto& f : filter) f.prepare (sampleRate);
}

bool Voice::pickRegion (const SampleMap& m) noexcept
{
    const Shared& p = *shared;
    slice = p.mode == ModeSlices && m.sliceSource != nullptr && ! m.slices.empty();
    if (slice)
    {
        const int idx = note - p.sliceBase;
        if (idx < 0 || idx >= (int) m.slices.size()) return false;
        sample = m.sliceSource.get();
        regionStart = m.slices[(size_t) idx];
        regionEnd = idx + 1 < (int) m.slices.size() ? m.slices[(size_t) idx + 1] : sample->numFrames();
        rootKey = note; // a slice plays at its original pitch on its key
        loopMode = LoopOff;
    }
    else
    {
        const int vel = std::clamp ((int) std::lround (velocity * 127.0f), 1, 127);
        const SampleMap::Zone* zone = nullptr;
        for (const auto& z : m.zones)
            if (note >= z.keyLo && note <= z.keyHi && vel >= z.velLo && vel <= z.velHi && z.sample != nullptr)
            {
                zone = &z;
                break;
            }
        if (zone == nullptr) return false;
        sample = zone->sample;
        rootKey = zone->rootKey >= 0 ? zone->rootKey : p.root;
        regionStart = 0;
        regionEnd = sample->numFrames();
        loopMode = p.loopMode;
        if (zone->loopStart >= 0 && zone->loopEnd > zone->loopStart)
        {
            loopStart = zone->loopStart;
            loopEnd = zone->loopEnd;
        }
        else if (sample->fileLoopStart >= 0 && sample->fileLoopEnd > sample->fileLoopStart)
        {
            loopStart = sample->fileLoopStart;
            loopEnd = sample->fileLoopEnd;
        }
        else
        {
            const auto len = (double) regionEnd;
            loopStart = (int64_t) (std::min (p.loopStart, p.loopEnd) * len);
            loopEnd = (int64_t) (std::max (p.loopStart, p.loopEnd) * len);
        }
        loopStart = std::clamp<int64_t> (loopStart, 0, regionEnd);
        loopEnd = std::clamp<int64_t> (loopEnd, 0, regionEnd);
        if (loopEnd - loopStart < 16) loopMode = LoopOff;
    }
    if (regionEnd - regionStart < 2) return false;

    const double offset = (double) p.start * (double) (regionEnd - regionStart - 1);
    direction = p.reverse ? -1 : 1;
    pos = p.reverse ? (double) (regionEnd - 1) - offset : (double) regionStart + offset;
    return true;
}

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    startBase (midiNote, vel, legato, c);
    const SampleMap* m = shared->map != nullptr ? shared->map->get() : nullptr;
    playingMap = m;
    if (m == nullptr || ! pickRegion (*m))
    {
        kill();
        return;
    }
    declick = 1.0f;
    for (auto& f : filter) f.reset();
}

void Voice::kill() noexcept
{
    killBase();
    sample = nullptr;
    playingMap = nullptr;
    for (auto& f : filter) f.reset();
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    if (sample == nullptr || shared->map == nullptr || shared->map->get() != playingMap)
    {
        kill(); // the map was replaced: never read a retired sample
        return;
    }
    const float* ch0 = sample->channel (0);
    const float* ch1 = sample->channel (1);
    const int64_t frames = sample->numFrames();
    const bool loopOn = loopMode != LoopOff;

    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        const float pitch = controlTick (c, n);
        const double ratio = std::pow (2.0, (double) (pitch - (float) rootKey) / 12.0) * sample->sampleRate() / (double) sr;

        float octaves = c.modEnvAmount * modEnv.getLevel() * 6.0f;
        if (c.lfoDest == LfoDest::Timbre) octaves += lfoValue * 2.0f;
        const float cutoff = std::clamp (40.0f * std::pow (2.0f, p.tone * 9.0f + octaves), 20.0f, 20000.0f);
        const bool useFilter = cutoff < 18000.0f;
        if (useFilter)
            for (auto& f : filter) f.set (cutoff, 0.1f);

        float panL, panR;
        lfoPan (c, panL, panR);
        const float gain = velocityGain (c) * lfoAmp (c);

        for (int i = 0; i < n; ++i)
        {
            float a = readHermite (ch0, frames, pos);
            float b = ch1 == ch0 ? a : readHermite (ch1, frames, pos);
            if (useFilter)
            {
                a = filter[0].process (a, dsp::Svf::LowPass);
                b = ch1 == ch0 ? a : filter[1].process (b, dsp::Svf::LowPass);
            }

            pos += ratio * (double) direction;
            bool ended = false;
            if (direction > 0)
            {
                if (loopOn && pos >= (double) loopEnd)
                {
                    if (loopMode == LoopPingPong) { pos = 2.0 * (double) loopEnd - pos; direction = -1; }
                    else pos -= (double) (loopEnd - loopStart);
                }
                else if (! loopOn && pos >= (double) regionEnd) ended = true;
            }
            else
            {
                if (loopOn && pos < (double) loopStart && (p.reverse || loopMode == LoopPingPong))
                {
                    if (loopMode == LoopPingPong) { pos = 2.0 * (double) loopStart - pos; direction = 1; }
                    else pos += (double) (loopEnd - loopStart);
                }
                else if (pos < (double) regionStart) ended = true;
            }

            // de-click the last ~1.5 ms before a one-shot region ends (slices end mid-signal)
            if (! loopOn)
            {
                const double remaining = (direction > 0 ? (double) regionEnd - pos : pos - (double) regionStart) / std::max (1.0e-6, ratio);
                declick = (float) std::clamp (remaining / 64.0, 0.0, 1.0);
            }

            modEnv.next();
            const float env = ampEnv.next() * gain * declick;
            l[done + i] += a * env * panL;
            r[done + i] += b * env * panR;
            if (ended)
            {
                kill();
                break;
            }
        }
        done += n;
    }
}
} // namespace sampler

SamplerEngine::SamplerEngine()
{
    builtInTone = sample::makeKeysTone (48000.0, 60.0f, 4.0f);
    shared.map = &map;
    for (auto& v : voices) v.shared = &shared;
    publish ({}, 0.5f);
}

std::vector<ParamDescriptor> SamplerEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("start", "Start", "Start", 0.0f, 1.0f, 0.0f, ParamUnit::Percent));
    d.push_back (continuous ("tone", "Tone", "Klang", 0.0f, 1.0f, 1.0f));
    d.push_back (continuous ("loopstart", "Loop Start", "Loop-Start", 0.0f, 1.0f, 0.0f, ParamUnit::Percent));
    d.push_back (continuous ("loopend", "Loop End", "Loop-Ende", 0.0f, 1.0f, 1.0f, ParamUnit::Percent));
    d.push_back (integer ("root", "Root Key", "Grundton", 0, 127, 60));
    d.push_back (choice ("loop", "Loop", "Loop", { "Off", "Forward", "Ping-Pong" }, { "Aus", "Vorwärts", "Ping-Pong" }, 0));
    d.push_back (toggle ("reverse", "Reverse", "Rückwärts", false));
    d.push_back (choice ("mode", "Mode", "Modus", { "Zones", "Slices" }, { "Zonen", "Slices" }, 0));
    d.push_back (continuous ("slicesens", "Slice Sensitivity", "Slice-Empfindlichkeit", 0.0f, 1.0f, 0.5f));
    d.push_back (integer ("slicebase", "First Slice Key", "Erste Slice-Taste", 0, 127, 36));
    return d;
}

void SamplerEngine::setParam (int i, float v) noexcept
{
    using namespace sampler;
    switch (i)
    {
        case Start: shared.start = dsp::clamp01 (v); break;
        case Tone: shared.tone = dsp::clamp01 (v); break;
        case LoopStart: shared.loopStart = dsp::clamp01 (v); break;
        case LoopEnd: shared.loopEnd = dsp::clamp01 (v); break;
        case Root: shared.root = std::clamp ((int) v, 0, 127); break;
        case LoopMode: shared.loopMode = std::clamp ((int) v, 0, (int) LoopPingPong); break;
        case Reverse: shared.reverse = v >= 0.5f; break;
        case Mode: shared.mode = std::clamp ((int) v, 0, (int) ModeSlices); break;
        case SliceSensitivity: requestedSensitivity.store (dsp::clamp01 (v), std::memory_order_relaxed); break;
        case SliceBaseKey: shared.sliceBase = std::clamp ((int) v, 0, 127); break;
        default: break;
    }
}

void SamplerEngine::publish (std::vector<sampler::ZoneSpec> specs, float sensitivity)
{
    using namespace sampler;
    auto m = std::make_shared<SampleMap>();
    specs.erase (std::remove_if (specs.begin(), specs.end(), [] (const ZoneSpec& z) { return z.sample == nullptr; }), specs.end());
    m->specs = std::move (specs);
    m->builtIn = m->specs.empty();
    auto addZone = [&] (const ZoneSpec& z) {
        SampleMap::Zone zone;
        zone.sample = z.sample.get();
        zone.keyLo = std::clamp (std::min (z.keyLo, z.keyHi), 0, 127);
        zone.keyHi = std::clamp (std::max (z.keyLo, z.keyHi), 0, 127);
        zone.velLo = std::clamp (std::min (z.velLo, z.velHi), 1, 127);
        zone.velHi = std::clamp (std::max (z.velLo, z.velHi), 1, 127);
        zone.rootKey = z.rootKey < 0 ? -1 : std::clamp (z.rootKey, 0, 127);
        zone.loopStart = z.loopStart;
        zone.loopEnd = z.loopEnd;
        m->zones.push_back (zone);
    };
    if (m->builtIn)
    {
        ZoneSpec tone;
        tone.sample = builtInTone;
        tone.rootKey = 60;
        addZone (tone);
        m->sliceSource = builtInTone;
    }
    else
    {
        for (const auto& z : m->specs) addZone (z);
        m->sliceSource = m->specs.front().sample;
    }
    sample::SliceSettings settings;
    settings.sensitivity = sensitivity;
    m->slices = sample::detectTransients (*m->sliceSource, settings);
    m->sliceSensitivity = sensitivity;
    map.set (std::move (m));
}

void SamplerEngine::setSample (SamplePtr s, int rootKey)
{
    std::vector<sampler::ZoneSpec> z;
    if (s != nullptr)
    {
        sampler::ZoneSpec spec;
        spec.sample = std::move (s);
        spec.rootKey = rootKey;
        z.push_back (std::move (spec));
    }
    publish (std::move (z), requestedSensitivity.load());
}

void SamplerEngine::addZone (const sampler::ZoneSpec& zone)
{
    auto z = zones();
    z.push_back (zone);
    publish (std::move (z), requestedSensitivity.load());
}

void SamplerEngine::setZones (std::vector<sampler::ZoneSpec> z) { publish (std::move (z), requestedSensitivity.load()); }

std::vector<sampler::ZoneSpec> SamplerEngine::zones() const
{
    const auto& m = map.owner();
    return m != nullptr ? m->specs : std::vector<sampler::ZoneSpec> {};
}

std::vector<int64_t> SamplerEngine::slicePoints() const
{
    const auto& m = map.owner();
    return m != nullptr ? m->slices : std::vector<int64_t> {};
}

bool SamplerEngine::updateSlicing()
{
    const float s = requestedSensitivity.load();
    const auto& m = map.owner();
    if (m != nullptr && std::abs (m->sliceSensitivity - s) < 1.0e-4f) return false;
    rebuildSlices (s);
    return true;
}

void SamplerEngine::rebuildSlices (float sensitivity)
{
    requestedSensitivity.store (dsp::clamp01 (sensitivity));
    publish (zones(), dsp::clamp01 (sensitivity));
}
} // namespace ohpe
