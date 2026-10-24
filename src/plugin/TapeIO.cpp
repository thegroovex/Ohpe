// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TapeIO.h"

#include "Localisation.h"

#include <cstring>

namespace ohpe::tapeio
{
void appendTapeAudio (Core& core, juce::MemoryBlock& state)
{
    if (core.tape().contentEndFrames() <= 0) return;
    std::vector<uint8_t> bin;
    core.tape().saveAudio (bin);
    const char zero = 0;
    state.append (&zero, 1);
    state.append (bin.data(), bin.size());
}

void splitState (const void* data, int size, std::string& json, const uint8_t*& tape, size_t& tapeSize)
{
    const auto* p = static_cast<const char*> (data);
    const size_t n = (size_t) juce::jmax (0, size);
    const void* nul = n > 0 ? std::memchr (p, 0, n) : nullptr;
    const size_t jsonLen = nul != nullptr ? (size_t) (static_cast<const char*> (nul) - p) : n;
    json.assign (p, jsonLen);
    tape = nul != nullptr && jsonLen + 1 < n ? reinterpret_cast<const uint8_t*> (p + jsonLen + 1) : nullptr;
    tapeSize = tape != nullptr ? n - jsonLen - 1 : 0;
}

void restoreTape (Core& core, const uint8_t* tape, size_t tapeSize)
{
    if (tape == nullptr || tapeSize == 0 || ! core.tape().loadAudio (tape, tapeSize))
        core.tape().resetContent();
    // a restored session starts with the tape at its beginning (as a fresh instance does), wherever this
    // instance's tape was parked before: the original and a new instance then play the session identically
    core.tape().locateFrames (0);
}

namespace
{
std::unique_ptr<juce::AudioFormatWriter> makeWriter (const juce::File& file, double sampleRate, int bits)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk()) return nullptr;
    juce::WavAudioFormat wav;
    const auto opts = juce::AudioFormatWriterOptions().withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (bits);
    return wav.createWriterFor (stream, opts);
}
} // namespace

BounceResult bounce (Core& live, const BounceOptions& options)
{
    BounceResult result;
    const double sr = live.sampleRate();
    result.sampleRate = sr;
    const juce::int64 content = (juce::int64) live.tape().contentEndFrames();
    if (content <= 0)
    {
        result.error = i18n::tr ("The tape is empty - nothing to export.");
        return result;
    }
    if (! options.folder.createDirectory())
    {
        result.error = i18n::tr ("Could not create the export folder.");
        return result;
    }

    // a private copy of the instrument: same parameters, same (shared, read-only) tape content
    constexpr int block = 512;
    auto offline = std::make_unique<Core>();
    offline->loadState (live.saveState());
    offline->prepare (sr, block);
    offline->tape().shareContentFrom (live.tape());
    auto& reg = offline->params();
    for (auto [id, v] : std::initializer_list<std::pair<const char*, float>> {
             { "tape.play", 1.0f }, { "tape.rec", 0.0f }, { "tape.speed", 1.0f }, { "tape.reverse", 0.0f },
             { "tape.loop", 0.0f }, { Core::Ids::seqRun, 0.0f } })
        reg.set (id, v);
    offline->tape().locateFrames (0);

    const juce::int64 total = juce::jmin ((juce::int64) offline->tape().lengthFrames(), content) + (juce::int64) std::llround (options.tailSeconds * sr);
    result.frames = total;

    std::unique_ptr<juce::AudioFormatWriter> masterWriter;
    std::array<std::unique_ptr<juce::AudioFormatWriter>, TapeMachine::numTracks> stemWriters;
    auto open = [&] (const juce::String& suffix) -> std::unique_ptr<juce::AudioFormatWriter> {
        const auto file = options.folder.getChildFile (options.baseName + " " + suffix + ".wav");
        auto w = makeWriter (file, sr, options.bitDepth);
        if (w != nullptr) result.files.add (file);
        else result.error = i18n::tr ("Could not write the file:") + " " + file.getFullPathName();
        return w;
    };
    if (options.master && (masterWriter = open ("Master")) == nullptr) return result;
    if (options.stems)
        for (int t = 0; t < TapeMachine::numTracks; ++t)
            if ((stemWriters[(size_t) t] = open ("Track " + juce::String (t + 1))) == nullptr) return result;

    juce::AudioBuffer<float> outs (2 * numOutputPairs, block), stems (2 * TapeMachine::numTracks, block);
    std::array<float*, 2 * TapeMachine::numTracks> capture {};
    for (int c = 0; c < 2 * TapeMachine::numTracks; ++c) capture[(size_t) c] = stems.getWritePointer (c);
    offline->setTapeCapture (&capture);

    // 0.8: the master output is late by the limiter look-ahead (constant); render that much longer and drop the
    //      first frames so the master file lines up with the stems (and with the tape) sample for sample
    const juce::int64 latency = juce::jmax (0, offline->latencySamples());
    for (juce::int64 pos = 0; pos < total + latency; pos += block)
    {
        const int n = (int) juce::jmin ((juce::int64) block, total + latency - pos);
        OutputBuffers ob;
        for (int o = 0; o < numOutputPairs; ++o)
        {
            ob.left[(size_t) o] = outs.getWritePointer (2 * o);
            ob.right[(size_t) o] = outs.getWritePointer (2 * o + 1);
        }
        offline->process (ob, n, nullptr, 0, Transport {});
        if (masterWriter != nullptr)
        {
            const int skip = (int) juce::jlimit ((juce::int64) 0, (juce::int64) n, latency - pos);
            if (n > skip)
            {
                const float* ch[2] = { outs.getReadPointer (0) + skip, outs.getReadPointer (1) + skip };
                if (! masterWriter->writeFromFloatArrays (ch, 2, n - skip)) { result.error = i18n::tr ("Could not write the file:"); return result; }
            }
        }
        const int stemN = (int) juce::jlimit ((juce::int64) 0, (juce::int64) n, total - pos);
        if (stemN > 0)
            for (int t = 0; t < TapeMachine::numTracks; ++t)
                if (auto& w = stemWriters[(size_t) t])
                {
                    const float* ch[2] = { stems.getReadPointer (2 * t), stems.getReadPointer (2 * t + 1) };
                    if (! w->writeFromFloatArrays (ch, 2, stemN)) { result.error = i18n::tr ("Could not write the file:"); return result; }
                }
    }
    offline->setTapeCapture (nullptr);
    masterWriter.reset();
    for (auto& w : stemWriters) w.reset();
    result.ok = true;
    return result;
}
} // namespace ohpe::tapeio
