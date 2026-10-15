// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FactoryKit.h"

#include "OhpeDrumKit.h"
#include "OhpeSamples.h"
#include "sample/FactoryBank.h"

#include <nlohmann/json.hpp>

#include <map>
#include <mutex>

#include <juce_audio_formats/juce_audio_formats.h>

namespace ohpe::factorykit
{
int numRecordings() noexcept { return OhpeDrumKit::namedResourceListSize; }

int load (Core& core)
{
    juce::FlacAudioFormat flac;
    int loaded = 0;
    for (int i = 0; i < OhpeDrumKit::namedResourceListSize; ++i)
    {
        const char* resource = OhpeDrumKit::namedResourceList[i];
        const juce::String file (OhpeDrumKit::getNamedResourceOriginalFilename (resource));
        const int pad = file.getIntValue() - 1; // "NN-name.flac"
        if (pad < 0 || pad >= drum::DrumKit::numPads) continue;

        int size = 0;
        const char* data = OhpeDrumKit::getNamedResource (resource, size);
        if (data == nullptr || size <= 0) continue;
        std::unique_ptr<juce::AudioFormatReader> reader (
            flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
        if (reader == nullptr || reader->lengthInSamples <= 0) continue;

        const auto frames = (int) reader->lengthInSamples;
        const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
        juce::AudioBuffer<float> buffer (chans, frames);
        if (! reader->read (&buffer, 0, frames, 0, true, chans > 1)) continue;
        std::vector<std::vector<float>> ch ((size_t) chans);
        for (int c = 0; c < chans; ++c)
            ch[(size_t) c].assign (buffer.getReadPointer (c), buffer.getReadPointer (c) + frames);
        const auto name = file.upToLastOccurrenceOf (".", false, false);
        core.drumKit().setFactorySample (pad, std::make_shared<SampleData> (std::move (ch), reader->sampleRate,
                                                                            ("factory:" + name).toStdString(), name.toStdString()));
        ++loaded;
    }
    return loaded;
}

namespace
{
SamplePtr decodeFlac (const char* data, int size, const juce::String& name)
{
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
    if (reader == nullptr || reader->lengthInSamples <= 0) return nullptr;
    const auto frames = (int) reader->lengthInSamples;
    const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> buffer (chans, frames);
    if (! reader->read (&buffer, 0, frames, 0, true, chans > 1)) return nullptr;
    std::vector<std::vector<float>> ch ((size_t) chans);
    for (int c = 0; c < chans; ++c)
        ch[(size_t) c].assign (buffer.getReadPointer (c), buffer.getReadPointer (c) + frames);
    return std::make_shared<SampleData> (std::move (ch), reader->sampleRate, ("factory:" + name).toStdString(), name.toStdString());
}
} // namespace

int numBankFiles() noexcept
{
    int n = 0;
    for (int i = 0; i < OhpeSamples::namedResourceListSize; ++i)
        n += juce::String (OhpeSamples::originalFilenames[i]).endsWithIgnoreCase (".flac") ? 1 : 0;
    return n;
}

int loadBank()
{
    static std::once_flag once;
    int installed = 0;
    std::call_once (once, [&installed] {
        // resources by file name ("tight-01-kick.flac"); bank.json refers to them by path
        std::map<juce::String, std::pair<const char*, int>> files;
        for (int i = 0; i < OhpeSamples::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = OhpeSamples::getNamedResource (OhpeSamples::namedResourceList[i], size);
            if (data != nullptr && size > 0) files[OhpeSamples::originalFilenames[i]] = { data, size };
        }
        const auto bankIt = files.find ("bank.json");
        if (bankIt == files.end()) return;
        const auto j = nlohmann::json::parse (std::string (bankIt->second.first, (size_t) bankIt->second.second), nullptr, false);
        if (! j.is_object()) return;

        auto sampleFor = [&files] (const std::string& path) -> SamplePtr {
            const auto name = juce::String (path).fromLastOccurrenceOf ("/", false, false);
            const auto it = files.find (name);
            if (it == files.end()) return nullptr;
            return decodeFlac (it->second.first, it->second.second, name.upToLastOccurrenceOf (".", false, false));
        };

        if (j.contains ("instruments") && j["instruments"].is_object())
            for (int i = 1; i < factory::numInstruments; ++i)
            {
                const auto id = factory::instrumentId (i);
                if (! j["instruments"].contains (id)) continue;
                std::vector<sampler::ZoneSpec> zones;
                for (const auto& z : j["instruments"][id])
                {
                    sampler::ZoneSpec spec;
                    spec.sample = sampleFor (z.value ("file", std::string()));
                    if (spec.sample == nullptr) continue;
                    spec.keyLo = z.value ("lo", 0);
                    spec.keyHi = z.value ("hi", 127);
                    spec.velLo = z.value ("vlo", 1);
                    spec.velHi = z.value ("vhi", 127);
                    spec.rootKey = z.value ("root", 60);
                    spec.loopStart = z.value ("ls", (int64_t) -1);
                    spec.loopEnd = z.value ("le", (int64_t) -1);
                    zones.push_back (std::move (spec));
                    ++installed;
                }
                factory::setInstrument (i, std::move (zones));
            }
        if (j.contains ("kits") && j["kits"].is_object())
            for (int k = 1; k < factory::numKits; ++k)
            {
                const auto id = factory::kitId (k);
                if (! j["kits"].contains (id)) continue;
                for (const auto& [pad, file] : j["kits"][id].items())
                    if (file.is_string())
                        if (auto smp = sampleFor (file.get<std::string>()))
                        {
                            factory::setKitSample (k, std::stoi (pad) - 1, std::move (smp));
                            ++installed;
                        }
            }
    });
    return installed;
}
} // namespace ohpe::factorykit
