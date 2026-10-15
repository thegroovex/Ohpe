// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleLoader.h"

#include "engines/GrainEngine.h"
#include "engines/SamplerEngine.h"

namespace ohpe
{
namespace
{
SamplerEngine* samplerOf (Core& core, int part)
{
    if (part < 0 || part >= Core::numParts) return nullptr;
    return dynamic_cast<SamplerEngine*> (core.part (part).engine (EngineType::Sampler));
}

GrainEngine* grainOf (Core& core, int part)
{
    if (part < 0 || part >= Core::numParts) return nullptr;
    return dynamic_cast<GrainEngine*> (core.part (part).engine (EngineType::Grain));
}
} // namespace

SampleLoader::SampleLoader (Core& c) : core (c) { formats.registerBasicFormats(); }

juce::String SampleLoader::filePatterns() const { return formats.getWildcardForAllFormats(); }

SamplePtr SampleLoader::decode (const juce::File& file, juce::String* errorOut, double maxSeconds)
{
    auto fail = [&] (const juce::String& text) -> SamplePtr {
        error = text;
        if (errorOut != nullptr) *errorOut = text;
        return nullptr;
    };
    if (! file.existsAsFile()) return fail ("The sample file was not found.");
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return fail ("The file is not a supported audio file.");
    if (reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0) return fail ("The audio file is empty.");

    const auto frames = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (maxSeconds * reader->sampleRate));
    const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> buffer (chans, frames);
    if (! reader->read (&buffer, 0, frames, 0, true, chans > 1)) return fail ("The audio file could not be read.");

    std::vector<std::vector<float>> data ((size_t) chans);
    for (int c = 0; c < chans; ++c)
        data[(size_t) c].assign (buffer.getReadPointer (c), buffer.getReadPointer (c) + frames);
    auto s = std::make_shared<SampleData> (std::move (data), reader->sampleRate, file.getFullPathName().toStdString(),
                                           file.getFileName().toStdString());

    // loop points stored in the file (WAV "smpl" chunk, AIFF instrument chunk)
    const auto& meta = reader->metadataValues;
    if (meta.getValue ("NumSampleLoops", "0").getIntValue() > 0)
    {
        const auto start = meta.getValue ("Loop0Start", "-1").getLargeIntValue();
        const auto end = meta.getValue ("Loop0End", "-1").getLargeIntValue();
        if (start >= 0 && end > start && end <= frames)
        {
            s->fileLoopStart = start;
            s->fileLoopEnd = end;
        }
    }
    error.clear();
    return s;
}

bool SampleLoader::loadSamplerSample (const juce::File& file, int rootKey, int part)
{
    auto* engine = samplerOf (core, part);
    auto s = engine != nullptr ? decode (file) : nullptr;
    if (s == nullptr) return false;
    engine->setSample (std::move (s), rootKey);
    return true;
}

bool SampleLoader::addSamplerZone (const juce::File& file, int keyLo, int keyHi, int rootKey, int velLo, int velHi, int part)
{
    auto* engine = samplerOf (core, part);
    auto s = engine != nullptr ? decode (file) : nullptr;
    if (s == nullptr) return false;
    sampler::ZoneSpec z;
    z.sample = std::move (s);
    z.keyLo = keyLo;
    z.keyHi = keyHi;
    z.rootKey = rootKey;
    z.velLo = velLo;
    z.velHi = velHi;
    engine->addZone (z);
    return true;
}

bool SampleLoader::loadGrainSample (const juce::File& file, int part)
{
    auto* engine = grainOf (core, part);
    auto s = engine != nullptr ? decode (file) : nullptr;
    if (s == nullptr) return false;
    engine->setSample (std::move (s));
    return true;
}

bool SampleLoader::loadDrumPad (int pad, const juce::File& file)
{
    if (pad < 0 || pad >= drum::DrumKit::numPads) return false;
    auto s = decode (file);
    if (s == nullptr) return false;
    core.drumKit().setPadSample (pad, std::move (s));
    core.params().set ("drum.pad" + std::to_string (pad + 1) + ".type", (float) drum::PadType::Sample);
    return true;
}

void SampleLoader::clearSampler (int part)
{
    if (auto* e = samplerOf (core, part)) e->clearZones();
}

void SampleLoader::clearGrain (int part)
{
    if (auto* e = grainOf (core, part)) e->setSample (nullptr);
}

void SampleLoader::clearDrumPad (int pad) { core.drumKit().setPadSample (pad, nullptr); }

void SampleLoader::storeInState() { core.setStateSection ("samples", sample::saveAssignments (core)); }

sample::RestoreResult SampleLoader::restoreFromState()
{
    return sample::restoreAssignments (core, core.stateSection ("samples"), [this] (const std::string& path) {
        return decode (juce::File (juce::String::fromUTF8 (path.c_str())));
    });
}

void SampleLoader::timerTick()
{
    for (int p = 0; p < Core::numParts; ++p)
        if (auto* e = samplerOf (core, p)) e->updateSlicing();
}
} // namespace ohpe
