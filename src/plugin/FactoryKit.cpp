// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FactoryKit.h"

#include "OhpeDrumKit.h"

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
} // namespace ohpe::factorykit
