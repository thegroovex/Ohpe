// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "BounceToEngine.h"

#include "Localisation.h"
#include "tape/TapeMachine.h"

namespace ohpe
{
using i18n::tr;

BounceManager::BounceManager (Core& c) : core (c) { library = defaultLibraryFolder(); }

juce::File BounceManager::defaultLibraryFolder()
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("OHPE_BOUNCE_DIR", {});
    if (env.isNotEmpty()) return juce::File (env);
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile ("Application Support");
#endif
    return base.getChildFile ("groovex").getChildFile ("Ohpe").getChildFile ("Bounces");
}

void BounceManager::post (const juce::String& msg, bool ok)
{
    {
        const juce::ScopedLock sl (lock);
        message = msg;
    }
    succeeded = ok;
    ++messages;
}

bool BounceManager::bounce (int part, int target, int source)
{
    auto& reg = core.params();
    if (capture.recording()) return false;
    if (source < 0) source = (int) std::lround (reg.get (Core::Ids::bounceSrc));
    if (target < 0) target = (int) std::lround (reg.get (Core::Ids::bounceTo));
    if (part < 0) part = (int) std::lround (reg.get (Core::Ids::editPart)) - 1;
    source = juce::jlimit (0, (int) bounce::numSources - 1, source);
    target = juce::jlimit (0, (int) bounce::numTargets - 1, target);
    part = juce::jlimit (0, Core::numParts - 1, part);
    const bool endless = reg.get (Core::Ids::bounceLoop) >= 0.5f;

    if (source == bounce::SourceInput)
    {
        const double sr = core.sampleRate() > 0.0 ? core.sampleRate() : 48000.0;
        capture.arm (bounce::lengthSeconds ((int) std::lround (reg.get (Core::Ids::bounceLen)), reg.get (Core::Ids::tempo)), sr);
        pendingPart = part;
        pendingTarget = target;
        pendingEndless = endless;
        post (tr ("Recording the input for the bounce"), false);
        return true;
    }

    auto& tape = core.tape();
    const int editTrack = juce::jlimit (0, TapeMachine::numTracks - 1, (int) std::lround (reg.get (Core::Ids::tapeEditTrack)) - 1);
    const int track = source == bounce::SourceEditTrack ? editTrack : source == bounce::SourceMix ? -1 : source - bounce::SourceTrack1;
    int64_t start = 0, end = -1;
    const double sr = tape.sampleRate() > 0.0 ? tape.sampleRate() : 48000.0;
    const double in = reg.get ("tape.loopin"), out = reg.get ("tape.loopout");
    if (reg.get ("tape.loop") >= 0.5f && out > in + 0.05) // the loop range while the tape loops
    {
        start = (int64_t) (in * sr);
        end = (int64_t) (out * sr);
    }
    const auto name = bounce::sourceName (source, editTrack);
    auto audio = bounce::readTape (tape, track, start, end);
    if (audio.frames() == 0)
    {
        post (tr ("Nothing to bounce - the track is empty"), false);
        return false;
    }
    return finish (std::move (audio), part, target, endless, name);
}

bool BounceManager::finishRecording()
{
    if (! capture.recording()) return false;
    capture.stop();
    timerTick();
    return true;
}

void BounceManager::cancel()
{
    if (capture.recording() || pendingPart >= 0)
    {
        capture.take();
        pendingPart = -1;
        post (tr ("Bounce cancelled"), false);
    }
}

void BounceManager::timerTick()
{
    if (pendingPart < 0 || capture.recording() || ! capture.finished()) return;
    const int part = pendingPart;
    pendingPart = -1;
    finish (capture.take(), part, pendingTarget, pendingEndless, "Input");
}

bool BounceManager::finish (bounce::Audio audio, int part, int target, bool endless, const std::string& name)
{
    bounce::Options o;
    o.endless = endless;
    auto s = bounce::prepare (std::move (audio), o, {}, name);
    if (s == nullptr)
    {
        post (tr ("Nothing to bounce - the material is silent"), false);
        return false;
    }
    // into the library, so the session finds the file again; the sample then carries its path
    const auto file = writeWav (*s, name);
    if (file != juce::File())
    {
        std::vector<std::vector<float>> ch;
        for (int c = 0; c < s->numChannels(); ++c) ch.emplace_back (s->channel (c), s->channel (c) + s->numFrames());
        auto withPath = std::make_shared<SampleData> (std::move (ch), s->sampleRate(), file.getFullPathName().toStdString(),
                                                      file.getFileName().toStdString());
        withPath->fileLoopStart = s->fileLoopStart;
        withPath->fileLoopEnd = s->fileLoopEnd;
        withPath->endless = s->endless;
        s = std::move (withPath);
    }
    {
        const juce::ScopedLock sl (lock);
        written = file;
    }
    const bool isEndless = s->endless;
    const double seconds = (double) s->numFrames() / s->sampleRate();
    if (! setParam || ! bounce::apply (core, part, (bounce::Target) target, std::move (s), setParam))
    {
        post (tr ("The bounce could not be loaded"), false);
        return false;
    }
    donePart = part;
    doneTarget = target;
    auto msg = tr (target == bounce::TargetSampler ? "Bounced to the Sampler" : "Bounced to Grain") + ": " + tr (juce::String (name))
             + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (seconds, 1) + " s"
             + (isEndless ? juce::String::fromUTF8 (" \xc2\xb7 ") + tr ("endless loop") : juce::String());
    if (file == juce::File()) msg << " (" << tr ("this session only") << ")";
    post (msg, true);
    return true;
}

juce::File BounceManager::writeWav (const SampleData& s, const std::string& name)
{
    if (! library.createDirectory()) return {};
    auto base = juce::File::createLegalFileName (juce::String::fromUTF8 (name.c_str()) + " "
                                                 + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S"))
                    .trim();
    auto file = library.getChildFile (base + ".wav");
    for (int n = 2; file.exists(); ++n) file = library.getChildFile (base + " " + juce::String (n) + ".wav");

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk()) return {};
    juce::WavAudioFormat wav;
    auto opts = juce::AudioFormatWriterOptions()
                    .withSampleRate (s.sampleRate())
                    .withNumChannels (s.numChannels())
                    .withBitsPerSample (24)
                    .withMetadata ("MidiUnityNote", "60");
    if (s.endless) // the "smpl" loop end is the last loop frame (inclusive)
        opts = opts.withMetadata ("NumSampleLoops", "1")
                   .withMetadata ("Loop0Type", "0")
                   .withMetadata ("Loop0Start", juce::String (s.fileLoopStart))
                   .withMetadata ("Loop0End", juce::String (s.fileLoopEnd - 1));
    auto writer = wav.createWriterFor (stream, opts);
    if (writer == nullptr) { file.deleteFile(); return {}; }
    const float* ptrs[2] = { s.channel (0), s.channel (1) };
    const bool ok = writer->writeFromFloatArrays (ptrs, s.numChannels(), (int) s.numFrames());
    writer.reset();
    if (! ok) { file.deleteFile(); return {}; }
    return file;
}
} // namespace ohpe
