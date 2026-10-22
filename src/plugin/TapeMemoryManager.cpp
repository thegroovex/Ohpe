// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TapeMemoryManager.h"

#include "Localisation.h"
#include "engines/SamplerEngine.h"

namespace ohpe
{
using i18n::tr;

TapeMemoryManager::TapeMemoryManager (Core& c) : core (c) { library = defaultLibraryFolder(); }

TapeMemoryManager::~TapeMemoryManager() { join(); }

juce::File TapeMemoryManager::defaultLibraryFolder()
{
    const auto dir = juce::SystemStats::getEnvironmentVariable ("OHPE_CAPTURE_DIR", {});
    if (dir.isNotEmpty()) return juce::File (dir);
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile ("Application Support");
#endif
    return base.getChildFile ("groovex").getChildFile ("Ohpe").getChildFile ("Captures");
}

void TapeMemoryManager::post (const juce::String& msg)
{
    {
        const juce::ScopedLock sl (lock);
        message = msg;
    }
    ++messages;
}

void TapeMemoryManager::join()
{
    if (worker.joinable()) worker.join();
}

double TapeMemoryManager::heldSeconds() const noexcept
{
    const auto& rb = core.tapeMemory();
    return rb.sampleRate() > 0.0 ? (double) rb.available() / rb.sampleRate() : 0.0;
}

bool TapeMemoryManager::capture (int part, int mode)
{
    auto& reg = core.params();
    if (working.load()) return false;
    if (part < 0)
    {
        const int to = (int) std::lround (reg.get (Core::Ids::tapememTo));
        part = to >= tapemem::TargetPart1 ? to - tapemem::TargetPart1 : (int) std::lround (reg.get (Core::Ids::editPart)) - 1;
    }
    part = juce::jlimit (0, Core::numParts - 1, part);
    if (mode < 0) mode = (int) std::lround (reg.get (Core::Ids::tapememMode));
    auto audio = core.tapeMemory().snapshot (tapemem::lengthSeconds ((int) std::lround (reg.get (Core::Ids::tapememLen))));
    if (audio.frames() < 64 || bounce::isSilent (audio))
    {
        post (tr ("Tape memory is empty - play something first"));
        return false;
    }
    join();
    working = true;
    ready = false;
    pendingPart = part;
    const std::string name = "Capture " + std::to_string (++counter);
    // the analysis (trim, onsets, pitch, resampling) runs off the audio and message threads
    worker = std::thread ([this, a = std::move (audio), mode, name]() mutable {
        pending = tapemem::build (std::move (a), mode, name);
        ready = true;
    });
    post (tr ("Capturing ..."));
    return true;
}

void TapeMemoryManager::finishNow()
{
    join();
    timerTick();
}

void TapeMemoryManager::timerTick()
{
    if (! working.load() || ! ready.load()) return;
    join();
    working = false;
    ready = false;
    auto c = std::move (pending);
    pending = {};
    if (c.sample == nullptr || ! tapemem::apply (core, pendingPart, c, setParam))
    {
        post (tr ("Nothing to capture - the memory is silent"));
        return;
    }
    juce::String msg = tr ("Captured into Part") + " " + juce::String (pendingPart + 1) + ": ";
    if (c.mode == tapemem::ModeSlices) msg << juce::String (c.slices) << " " << tr ("slices from C2");
    else if (c.pitchHz > 0.0f)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        msg << tr ("chromatic, root") << " " << names[c.root % 12] << juce::String (c.root / 12 - 1);
    }
    else msg << tr ("chromatic, no pitch found (root C4)");
    captures[(size_t) pendingPart] = std::move (c);
    lastPart = pendingPart;
    post (msg);
}

const tapemem::Capture* TapeMemoryManager::captureOf (int part) const
{
    if (part < 0 || part >= Core::numParts) return nullptr;
    const auto& c = captures[(size_t) part];
    if (c.sample == nullptr) return nullptr;
    auto* e = dynamic_cast<SamplerEngine*> (core.part (part).engine (EngineType::Sampler));
    if (e == nullptr) return nullptr;
    const auto z = e->zones();
    return z.size() == 1 && z[0].sample == c.sample ? &c : nullptr;
}

juce::File TapeMemoryManager::exportWav (int part)
{
    if (part < 0) part = lastPart.load();
    const auto* c = captureOf (part);
    if (c == nullptr || ! library.createDirectory())
    {
        post (tr ("No capture to export"));
        return {};
    }
    const auto& s = *c->sample;
    auto base = juce::File::createLegalFileName (juce::String::fromUTF8 (c->name.c_str()) + " "
                                                 + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S")).trim();
    auto file = library.getChildFile (base + ".wav");
    for (int n = 2; file.exists(); ++n) file = library.getChildFile (base + " " + juce::String (n) + ".wav");
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk()) return {};
    juce::WavAudioFormat wav;
    const auto opts = juce::AudioFormatWriterOptions()
                          .withSampleRate (s.sampleRate())
                          .withNumChannels (s.numChannels())
                          .withBitsPerSample (24)
                          .withMetadata ("MidiUnityNote", juce::String (c->mode == tapemem::ModeChromatic ? c->root : tapemem::sliceBaseKey));
    auto writer = wav.createWriterFor (stream, opts);
    if (writer == nullptr) { file.deleteFile(); return {}; }
    const float* ptrs[2] = { s.channel (0), s.channel (1) };
    const bool ok = writer->writeFromFloatArrays (ptrs, s.numChannels(), (int) s.numFrames());
    writer.reset();
    if (! ok) { file.deleteFile(); return {}; }
    post (tr ("Exported") + ": " + file.getFileName());
    return file;
}

void TapeMemoryManager::storeInState()
{
    std::vector<tapemem::Stored> keep;
    for (int p = 0; p < Core::numParts; ++p)
        if (const auto* c = captureOf (p)) keep.push_back ({ p, *c });
    core.setStateSection ("tapememory", tapemem::saveCaptures (keep));
}

int TapeMemoryManager::restoreFromState()
{
    for (auto& c : captures) c = {};
    int n = 0;
    for (auto& s : tapemem::restoreCaptures (core.stateSection ("tapememory")))
    {
        auto* e = dynamic_cast<SamplerEngine*> (core.part (s.part).engine (EngineType::Sampler));
        if (e == nullptr) continue;
        // the parameters (engine, mode, root) come with the state; only the audio is loaded here
        e->setSample (s.capture.sample, s.capture.mode == tapemem::ModeSlices ? tapemem::sliceBaseKey : s.capture.root);
        captures[(size_t) s.part] = std::move (s.capture);
        ++n;
    }
    return n;
}
} // namespace ohpe
