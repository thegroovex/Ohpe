// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UserTables.h"

#include "wavetable/TableFile.h"

namespace ohpe
{
namespace
{
constexpr double maxTapeSeconds = 30.0;
constexpr juce::int64 maxFileBytes = 256 * 1024 * 1024;
} // namespace

UserTableManager::UserTableManager (Core& c) : juce::Thread ("Ohpe wavetable analysis"), core (c)
{
    library = defaultLibraryFolder();
    formats.registerBasicFormats();
    startThread (juce::Thread::Priority::low);
}

UserTableManager::~UserTableManager()
{
    cancelFlag = true;
    signalThreadShouldExit();
    wake.signal();
    stopThread (10000);
}

juce::File UserTableManager::defaultLibraryFolder()
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("OHPE_WAVETABLE_DIR", {});
    if (env.isNotEmpty()) return juce::File (env);
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile ("Application Support");
#endif
    return base.getChildFile ("groovex").getChildFile ("Ohpe").getChildFile ("Wavetables");
}

juce::Array<juce::File> UserTableManager::libraryFiles() const
{
    auto files = library.findChildFiles (juce::File::findFiles, false, juce::String ("*") + usertable::fileExtension);
    files.sort();
    return files;
}

juce::String UserTableManager::filePatterns() const
{
    return const_cast<juce::AudioFormatManager&> (formats).getWildcardForAllFormats() + ";*" + usertable::fileExtension;
}

bool UserTableManager::isImportableFile (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg" || ext == usertable::fileExtension;
}

UserTableManager::Status UserTableManager::status() const
{
    Status s;
    s.recording = inputCapture.recording();
    s.busy = busy.load() || s.recording;
    s.progress = s.recording ? inputCapture.progress() : progressValue.load();
    const juce::ScopedLock sl (lock);
    s.part = busyPart;
    s.target = busyTarget;
    return s;
}

void UserTableManager::post (const juce::String& msg, const juce::String& det)
{
    {
        const juce::ScopedLock sl (lock);
        message = msg;
        detail = det;
    }
    ++messages;
}

bool UserTableManager::start (Job job)
{
    if (busy.load() || inputCapture.recording()) return false;
    job.options.frames = juce::jlimit (8, usertable::maxFrames, frameCount);
    job.options.smoothing = juce::jlimit (0.0f, 1.0f, smoothing);
    {
        const juce::ScopedLock sl (lock);
        busyPart = job.part;
        busyTarget = job.target;
        pending = std::move (job);
    }
    cancelFlag = false;
    progressValue = 0.0f;
    busy = true;
    wake.signal();
    return true;
}

bool UserTableManager::importFile (const juce::File& file, int part, Target target)
{
    if (! isImportableFile (file) || ! file.existsAsFile()) return false;
    Job j;
    j.file = file;
    j.name = file.getFileNameWithoutExtension();
    j.part = part;
    j.target = target;
    return start (std::move (j));
}

bool UserTableManager::importTapeTrack (int track, int part, Target target)
{
    if (busy.load() || inputCapture.recording()) return false;
    auto& tape = core.tape();
    if (track < 0 || track >= TapeMachine::numTracks) return false;
    const double sr = core.sampleRate() > 0.0 ? core.sampleRate() : 48000.0;
    const auto frames = (int) juce::jmin<int64_t> (tape.contentEndFrames (track), (int64_t) (maxTapeSeconds * sr));
    if (frames <= 0) return false;
    Job j;
    j.audio.assign ((size_t) frames, 0.0f);
    std::vector<float> l (4096), r (4096);
    for (int pos = 0; pos < frames; pos += 4096)
    {
        const int n = juce::jmin (4096, frames - pos);
        tape.readCommitted (track, pos, n, l.data(), r.data());
        for (int i = 0; i < n; ++i) j.audio[(size_t) (pos + i)] = 0.5f * (l[(size_t) i] + r[(size_t) i]);
    }
    j.rate = sr;
    j.name = "Tape " + juce::String (track + 1);
    j.part = part;
    j.target = target;
    return start (std::move (j));
}

bool UserTableManager::recordInput (double seconds, int part, Target target)
{
    if (busy.load() || inputCapture.recording()) return false;
    const double sr = core.sampleRate() > 0.0 ? core.sampleRate() : 48000.0;
    recordJob = {};
    recordJob.name = "Input";
    recordJob.part = part;
    recordJob.target = target;
    {
        const juce::ScopedLock sl (lock);
        busyPart = part;
        busyTarget = target;
    }
    inputCapture.arm (juce::jlimit (0.5, 10.0, seconds), sr);
    recordingInput = true;
    return true;
}

void UserTableManager::cancel()
{
    cancelFlag = true;
    if (recordingInput)
    {
        inputCapture.take();
        recordingInput = false;
        post ("Import cancelled", {});
    }
}

void UserTableManager::clear (int part, Target target) { usertable::setTable (core, part, target, nullptr); }

usertable::UserTablePtr UserTableManager::loadTableFile (const juce::File& file, juce::String* error) const
{
    juce::MemoryBlock mb;
    if (! file.existsAsFile() || file.getSize() > maxFileBytes || ! file.loadFileAsData (mb))
    {
        if (error != nullptr) *error = "The wavetable file was not found.";
        return nullptr;
    }
    std::string err;
    auto t = usertable::decode (static_cast<const uint8_t*> (mb.getData()), mb.getSize(), &err);
    if (t == nullptr)
    {
        if (error != nullptr) *error = juce::String (err);
        return nullptr;
    }
    std::const_pointer_cast<usertable::UserTable> (t)->path = file.getFullPathName().toStdString();
    return t;
}

juce::File UserTableManager::saveToLibrary (const usertable::UserTable& t) const
{
    if (! library.createDirectory()) return {};
    auto base = juce::File::createLegalFileName (juce::String::fromUTF8 (t.name().c_str())).trim();
    if (base.isEmpty()) base = "Wavetable";
    base = base.substring (0, 72);
    auto file = library.getChildFile (base + usertable::fileExtension);
    for (int n = 2; file.exists(); ++n) file = library.getChildFile (base + " " + juce::String (n) + usertable::fileExtension);
    const auto bytes = usertable::encode (t);
    if (! file.replaceWithData (bytes.data(), bytes.size())) return {};
    return file;
}

void UserTableManager::run()
{
    while (! threadShouldExit())
    {
        wake.wait (500);
        std::optional<Job> job;
        {
            const juce::ScopedLock sl (lock);
            job.swap (pending);
        }
        if (! job) continue;
        process (*job);
    }
}

void UserTableManager::finish (bool ok, const juce::String& msg, const juce::String& det, usertable::UserTablePtr t)
{
    {
        const juce::ScopedLock sl (lock);
        resultReady = true;
        result = ok ? std::move (t) : nullptr;
        message = msg;
        detail = det;
    }
    busy = false;
}

void UserTableManager::process (Job& job)
{
    auto progress = [this] (float p) {
        progressValue = p;
        return ! cancelFlag.load() && ! threadShouldExit();
    };
    {
        const juce::ScopedLock sl (lock);
        resultPart = job.part;
        resultTarget = job.target;
    }
    usertable::UserTablePtr table;
    bool fromTableFile = false;

    if (job.file != juce::File())
    {
        if (job.file.hasFileExtension (usertable::fileExtension))
        {
            juce::String err;
            table = loadTableFile (job.file, &err);
            if (table == nullptr) return finish (false, err, {}, nullptr);
            if (job.target == Target::Vocal && ! table->hasFormants()) return finish (false, "The table has no formant material.", {}, nullptr);
            if (job.target == Target::Wavetable && ! table->hasWave()) return finish (false, "The wavetable file is damaged.", {}, nullptr);
            fromTableFile = true;
        }
        else
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.file));
            if (reader == nullptr) return finish (false, "The file is not a supported audio file.", {}, nullptr);
            if (reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0) return finish (false, "The audio file is empty.", {}, nullptr);
            const auto frames = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (job.options.maxSeconds * reader->sampleRate));
            const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
            juce::AudioBuffer<float> buffer (chans, frames);
            if (! reader->read (&buffer, 0, frames, 0, true, chans > 1)) return finish (false, "The audio file could not be read.", {}, nullptr);
            job.audio.assign ((size_t) frames, 0.0f);
            for (int c = 0; c < chans; ++c)
                for (int i = 0; i < frames; ++i) job.audio[(size_t) i] += buffer.getSample (c, i) / (float) chans;
            job.rate = reader->sampleRate;

            // a single-cycle set? frame size from the "clm " chunk / cue points (WAV), else 2048
            int hint = 0;
            if (job.file.hasFileExtension (".wav") && job.file.getSize() < maxFileBytes)
            {
                juce::MemoryBlock mb;
                if (job.file.loadFileAsData (mb)) hint = usertable::wavFrameSizeHint (static_cast<const uint8_t*> (mb.getData()), mb.getSize());
            }
            if (job.target == Target::Wavetable && reader->lengthInSamples <= frames && usertable::looksLikeWavetableSet (frames, hint))
            {
                progress (0.5f);
                const auto set = usertable::framesFromSet (job.audio.data(), (int64_t) job.audio.size(), hint);
                table = usertable::UserTable::create (job.name.toStdString(), set);
                if (table == nullptr) return finish (false, "The wavetable file is damaged.", {}, nullptr);
            }
        }
    }
    juce::String det;
    if (table == nullptr) // audio: analyse
    {
        const auto r = usertable::analyse (job.audio.data(), (int64_t) job.audio.size(), job.rate, job.options, progress);
        if (! r.ok) return finish (false, juce::String (r.error), {}, nullptr);
        table = usertable::UserTable::create (job.name.toStdString(), r.frames, r.formants);
        if (table == nullptr) return finish (false, "The sound is silent.", {}, nullptr);
        if (r.pitched) det << ", " << juce::String (r.pitchHz, 1) << " Hz";
    }
    if (! fromTableFile) // analysed sounds and wavetable sets are kept in the library as Ohpe tables
    {
        const auto saved = saveToLibrary (*table);
        if (saved != juce::File()) std::const_pointer_cast<usertable::UserTable> (table)->path = saved.getFullPathName().toStdString();
    }
    progressValue = 1.0f;
    det = juce::String::fromUTF8 (table->name().c_str()) + " - " + juce::String (table->numFrames()) + " frames" + det;
    finish (true, job.target == Target::Vocal ? "Formants ready" : "Wavetable ready", det, table);
}

void UserTableManager::timerTick()
{
    if (recordingInput && inputCapture.finished())
    {
        recordingInput = false;
        auto job = recordJob;
        job.audio = inputCapture.take();
        job.rate = inputCapture.sampleRate();
        float pk = 0.0f;
        for (float v : job.audio) pk = juce::jmax (pk, std::abs (v));
        if (pk < 1.0e-4f) post ("No input signal - check the audio input", {});
        else start (std::move (job));
    }
    usertable::UserTablePtr t;
    int part = 0;
    Target target = Target::Wavetable;
    bool ready = false;
    {
        const juce::ScopedLock sl (lock);
        if (resultReady)
        {
            ready = true;
            resultReady = false;
            t = std::move (result);
            part = resultPart;
            target = resultTarget;
        }
    }
    if (! ready) return;
    if (t != nullptr)
    {
        usertable::setTable (core, part, target, t);
        if (onPublished) onPublished (part, target);
    }
    ++messages;
}

usertable::Resolver UserTableManager::resolver() const
{
    return [this] (const std::string& path, const std::string& fileName) -> usertable::UserTablePtr {
        if (! path.empty())
        {
            const auto p = juce::String::fromUTF8 (path.c_str());
            if (juce::File::isAbsolutePath (p))
                if (const juce::File f (p); f.existsAsFile())
                    if (auto t = loadTableFile (f)) return t;
        }
        if (! fileName.empty())
        {
            const auto f = library.getChildFile (juce::File::createLegalFileName (juce::String::fromUTF8 (fileName.c_str())));
            if (f.existsAsFile())
                if (auto t = loadTableFile (f)) return t;
        }
        return nullptr;
    };
}

void UserTableManager::storeInState() { core.setStateSection ("usertables", usertable::saveAssignments (core)); }

usertable::RestoreResult UserTableManager::restoreFromState()
{
    const auto r = usertable::restoreAssignments (core, core.stateSection ("usertables"), resolver());
    if (r.missing > 0) post ("A user wavetable was not found - the built-in table plays", {});
    return r;
}

std::string UserTableManager::partJson (int part) { return usertable::partToJson (core, part); }

usertable::RestoreResult UserTableManager::applyPartJson (int part, const std::string& json)
{
    const auto r = usertable::partFromJson (core, part, json, resolver());
    if (r.missing > 0) post ("A user wavetable was not found - the built-in table plays", {});
    return r;
}
} // namespace ohpe
