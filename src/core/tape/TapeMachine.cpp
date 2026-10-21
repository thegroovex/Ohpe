// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TapeMachine.h"
#include "model/JsonParse.h" // p63: depth-limited parse

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>

namespace ohpe
{
namespace
{
constexpr char magic[8] = { 'O', 'H', 'P', 'E', 'T', 'A', 'P', 'E' };
constexpr uint32_t audioFormatVersion = 1;
constexpr uint32_t endMarker = 0xffffffffu;
constexpr int64_t maxPagesEver = (int64_t) (TapeMachine::maxLengthSeconds * 192000.0) / TapeBlock::frames + 2;

std::vector<std::string> outChoices (bool de)
{
    std::vector<std::string> c { de ? "Nur Mixer" : "Mixer only" };
    for (int i = 1; i <= 8; ++i) c.push_back ((de ? "Ausgang " : "Out ") + std::to_string (i));
    return c;
}

template <typename T>
void put (std::vector<uint8_t>& out, const T& v)
{
    const auto* p = reinterpret_cast<const uint8_t*> (&v);
    out.insert (out.end(), p, p + sizeof (T));
}

template <typename T>
bool get (const uint8_t*& p, const uint8_t* end, T& v)
{
    if ((size_t) (end - p) < sizeof (T)) return false;
    std::memcpy (&v, p, sizeof (T));
    p += sizeof (T);
    return true;
}

bool blockIsSilent (const float* l, const float* r, int n) noexcept
{
    for (int i = 0; i < n; ++i)
        if (l[i] != 0.0f || r[i] != 0.0f) return false;
    return true;
}
} // namespace

// ---------------------------------------------------------------- exclusion
void TapeMachine::Exclusion::lock() noexcept
{
    pending.store (true, std::memory_order_seq_cst);
    while (busy.load (std::memory_order_seq_cst))
        std::this_thread::yield();
}

bool TapeMachine::Exclusion::tryEnterAudio() noexcept
{
    busy.store (true, std::memory_order_seq_cst);
    if (pending.load (std::memory_order_seq_cst))
    {
        busy.store (false, std::memory_order_seq_cst);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- construction
TapeMachine::TapeMachine()
{
    locateRequest.store (std::numeric_limits<double>::quiet_NaN());
    paramIndex.fill (-1);
    lastPulled.fill (NAN);
    const auto descs = describeParams();
    for (size_t i = 0; i < descs.size(); ++i)
    {
        paramValue[i] = descs[i].defaultValue;
        setParam ((int) i, descs[i].defaultValue);
    }
    Snapshot empty;
    for (auto& t : empty) t = std::make_shared<PageList>();
    history.push_back (empty);
    cursor = 0;
    levelCurrent = levelTarget;
}

TapeMachine::~TapeMachine()
{
    {
        std::lock_guard<std::mutex> g (msgMutex);
        ExclusionGuard ex (exclusion);
        // fresh blocks recorded but not committed yet are owned by the live tables
        const auto& cur = current();
        for (int t = 0; t < numTracks; ++t)
            for (size_t p = 0; p < live[(size_t) t].size(); ++p)
            {
                auto* b = live[(size_t) t][p];
                const auto& list = *cur[(size_t) t];
                if (b != nullptr && (p >= list.size() || list[p].get() != b)) delete b;
            }
    }
    TapeBlock* b = nullptr;
    while (pool.pop (b)) delete b;
}

// ---------------------------------------------------------------- parameters
std::vector<ParamDescriptor> TapeMachine::describeParams()
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (toggle ("play", "Tape Play", "Tape Wiedergabe", false));
    d.push_back (toggle ("rec", "Tape Record", "Tape Aufnahme", false));
    // 0.5 .. 2.0 with 1.0 in the middle of the control
    d.push_back (continuous ("speed", "Tape Speed", "Tape-Geschwindigkeit", 0.5f, 2.0f, 1.0f, ParamUnit::None, 0.63093f));
    d.push_back (toggle ("reverse", "Tape Reverse", "Tape rückwärts", false));
    d.push_back (toggle ("loop", "Tape Loop", "Tape-Schleife", false));
    d.push_back (continuous ("loopin", "Loop In", "Schleife Start", 0.0f, (float) maxLengthSeconds, 0.0f, ParamUnit::Seconds, 0.3f));
    d.push_back (continuous ("loopout", "Loop Out", "Schleife Ende", 0.0f, (float) maxLengthSeconds, 8.0f, ParamUnit::Seconds, 0.3f));
    d.push_back (toggle ("overdub", "Overdub", "Overdub", true));
    d.push_back (continuous ("sat", "Tape Saturation", "Tape-Sättigung", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("wow", "Wow", "Wow", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("flutter", "Flutter", "Flutter", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("hiss", "Tape Hiss", "Bandrauschen", 0.0f, 1.0f, 0.0f));
    for (int t = 0; t < numTracks; ++t)
    {
        const auto n = std::to_string (t + 1);
        const auto p = "tr" + n + ".";
        d.push_back (toggle (p + "arm", "Track " + n + " Arm", "Spur " + n + " Aufnahmebereit", false));
        d.push_back (choice (p + "src", "Track " + n + " Source", "Spur " + n + " Quelle",
                             { "Input", "Part 1", "Part 2", "Part 3", "Part 4", "Drum" },
                             { "Eingang", "Part 1", "Part 2", "Part 3", "Part 4", "Drum" }, tapesource::part1));
        d.push_back (continuous (p + "level", "Track " + n + " Level", "Spur " + n + " Pegel", -60.0f, 6.0f, 0.0f, ParamUnit::Decibel, 2.0f));
        d.push_back (choice (p + "out", "Track " + n + " Direct Out", "Spur " + n + " Direktausgang", outChoices (false), outChoices (true), 0));
    }
    return d;
}

std::vector<ParamDescriptor> TapeMachine::getParamDescriptors() const { return describeParams(); }

void TapeMachine::registerParams (ParamRegistry& reg)
{
    for (auto d : describeParams())
    {
        d.id = prefix + d.id;
        d.group = "tape";
        reg.add (std::move (d));
    }
}

void TapeMachine::bind (const ParamRegistry& reg)
{
    const auto descs = describeParams();
    for (size_t i = 0; i < descs.size(); ++i) paramIndex[i] = reg.indexOf (prefix + descs[i].id);
    lastPulled.fill (NAN);
}

void TapeMachine::pullParams (const ParamSnapshot& s) noexcept
{
    for (int i = 0; i < numParams; ++i)
    {
        if (paramIndex[(size_t) i] < 0) continue;
        const float v = s[paramIndex[(size_t) i]];
        if (! (v == lastPulled[(size_t) i]))
        {
            lastPulled[(size_t) i] = v;
            setParam (i, v);
        }
    }
}

void TapeMachine::setParam (int i, float v) noexcept
{
    if (i < 0 || i >= numParams) return;
    paramValue[(size_t) i] = v;
    switch (i)
    {
        case Play: play = v >= 0.5f; break;
        case Rec: rec = v >= 0.5f; break;
        case Speed: speed = std::clamp (v, 0.5f, 2.0f); break;
        case Reverse: reverse = v >= 0.5f; break;
        case Loop: loopOn = v >= 0.5f; break;
        case LoopIn: loopIn = std::max (0.0f, v); break;
        case LoopOut: loopOut = std::max (0.0f, v); break;
        case Overdub: overdub = v >= 0.5f; break;
        case Sat: sat = dsp::clamp01 (v); for (auto& s : saturators) s.setAmount (sat); break;
        case Wow: wow = dsp::clamp01 (v); break;
        case Flutter: flutter = dsp::clamp01 (v); break;
        case Hiss: hiss = dsp::clamp01 (v); break;
        default:
        {
            const int t = (i - numGlobalParams) / numTrackParams;
            switch ((i - numGlobalParams) % numTrackParams)
            {
                case Arm: armed[(size_t) t] = v >= 0.5f; break;
                case Src: trackSrc[(size_t) t] = std::clamp ((int) std::lround (v), 0, tapesource::numSources - 1); break;
                case Level: levelTarget[(size_t) t] = v <= -59.9f ? 0.0f : dsp::dbToGain (v); break;
                case Out: trackOut[(size_t) t] = std::clamp ((int) std::lround (v), 0, 8); break;
                default: break;
            }
        }
    }
}

// ---------------------------------------------------------------- set-up
int64_t TapeMachine::toFrames (double seconds) const noexcept
{
    if (! (seconds > 0.0)) return 0;
    return std::min (lenFrames, (int64_t) std::llround (seconds * sr));
}

void TapeMachine::resizeLiveLocked (size_t pages)
{
    for (auto& t : live)
        if (t.size() < pages) t.resize (pages, nullptr);
}

void TapeMachine::prepare (double sampleRate, int)
{
    std::unique_lock<std::mutex> g (msgMutex);
    {
        ExclusionGuard ex (exclusion);
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        lenFrames = (int64_t) std::ceil (lengthSec * sr);
        const size_t pages = (size_t) ((lenFrames + TapeBlock::frames - 1) / TapeBlock::frames);
        commitLocked();
        resizeLiveLocked (pages);
        installLocked (current());
        for (auto& s : saturators) s.prepare ((float) sr);
        speedCoeff = (float) (1.0 - std::exp (-1.0 / (sr * 0.05)));
        pos = std::min (pos, (double) lenFrames);
    }
    refillPool();
}

void TapeMachine::setLengthSeconds (double seconds)
{
    lengthSec = std::clamp (seconds, minLengthSeconds, maxLengthSeconds);
    prepare (sr, 512);
}

void TapeMachine::resetDsp() noexcept
{
    for (auto& s : saturators) s.reset();
    speedCurrent = speed * (reverse ? -1.0f : 1.0f);
    levelCurrent = levelTarget;
    wowPhase = flutterPhase = 0.0;
    flutterNoise = 0.0f;
}

void TapeMachine::refillPool()
{
    while (poolCount.load (std::memory_order_acquire) < poolTarget)
    {
        auto* b = new (std::nothrow) TapeBlock();
        if (b == nullptr) break;
        if (! pool.push (b)) { delete b; break; }
        poolCount.fetch_add (1, std::memory_order_acq_rel);
    }
}

bool TapeMachine::housekeeping()
{
    std::unique_lock<std::mutex> g (msgMutex);
    bool committed = false;
    if (dirty.load (std::memory_order_acquire) && ! recordingActive.load (std::memory_order_acquire))
    {
        ExclusionGuard ex (exclusion);
        committed = commitLocked();
    }
    refillPool();
    return committed;
}

// ---------------------------------------------------------------- history
bool TapeMachine::commitLocked()
{
    if (! dirty.exchange (false)) return false;
    Snapshot next = current();
    bool any = false;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& old = *next[(size_t) t];
        const auto& lv = live[(size_t) t];
        bool changed = false;
        for (size_t p = 0; p < lv.size() && ! changed; ++p)
            changed = lv[p] != (p < old.size() ? old[p].get() : nullptr);
        if (! changed) continue;
        auto list = std::make_shared<PageList> (std::max (old.size(), lv.size()));
        for (size_t p = 0; p < list->size(); ++p)
        {
            TapeBlock* raw = p < lv.size() ? lv[p] : nullptr;
            const BlockPtr* prev = p < old.size() ? &old[p] : nullptr;
            if (p >= lv.size()) (*list)[p] = old[p];        // beyond the current length: keep
            else if (prev != nullptr && prev->get() == raw) (*list)[p] = *prev;
            else if (raw != nullptr)
            {
                raw->frozen.store (true, std::memory_order_release);
                (*list)[p] = BlockPtr (raw);
            }
        }
        next[(size_t) t] = list;
        any = true;
    }
    // blocks written in place (fresh, not yet frozen) are part of the new snapshot: freeze them
    for (int t = 0; t < numTracks; ++t)
        for (auto* b : live[(size_t) t])
            if (b != nullptr) b->frozen.store (true, std::memory_order_release);
    if (any) pushSnapshotLocked (next);
    return any;
}

void TapeMachine::pushSnapshotLocked (Snapshot s)
{
    history.resize ((size_t) cursor + 1); // drops the redo branch
    history.push_back (std::move (s));
    cursor = (int) history.size() - 1;
    revision.fetch_add (1, std::memory_order_acq_rel);
}

void TapeMachine::installLocked (const Snapshot& s)
{
    for (int t = 0; t < numTracks; ++t)
    {
        auto& lv = live[(size_t) t];
        const auto& list = *s[(size_t) t];
        for (size_t p = 0; p < lv.size(); ++p) lv[p] = p < list.size() ? list[p].get() : nullptr;
    }
    dirty.store (false);
    revision.fetch_add (1, std::memory_order_acq_rel);
}

bool TapeMachine::beginEdit (std::unique_lock<std::mutex>&)
{
    // caller holds msgMutex and the exclusion
    if (recordingActive.load (std::memory_order_acquire)) return false;
    commitLocked();
    return true;
}

bool TapeMachine::undo()
{
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    if (! beginEdit (g) || cursor <= 0) return false;
    --cursor;
    installLocked (current());
    return true;
}

bool TapeMachine::redo()
{
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    if (! beginEdit (g) || cursor + 1 >= (int) history.size()) return false;
    ++cursor;
    installLocked (current());
    return true;
}

bool TapeMachine::canUndo() const { std::lock_guard<std::mutex> g (msgMutex); return cursor > 0 || dirty.load(); }
bool TapeMachine::canRedo() const { std::lock_guard<std::mutex> g (msgMutex); return cursor + 1 < (int) history.size(); }
int TapeMachine::undoSteps() const { std::lock_guard<std::mutex> g (msgMutex); return cursor; }
int64_t TapeMachine::clipboardFrames() const { std::lock_guard<std::mutex> g (msgMutex); return (int64_t) clipL.size(); }

void TapeMachine::resetContent()
{
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    dirty.store (true);
    commitLocked(); // takes ownership of uncommitted blocks, freed with the history below
    Snapshot empty;
    for (auto& t : empty) t = std::make_shared<PageList>();
    installLocked (empty);
    history.clear();
    history.push_back (empty);
    cursor = 0;
    clipL.clear();
    clipR.clear();
}

// ---------------------------------------------------------------- reading
void TapeMachine::readPages (const PageList& pages, int64_t start, int n, float* l, float* r) noexcept
{
    int done = 0;
    while (done < n)
    {
        const int64_t idx = start + done;
        if (idx < 0)
        {
            const int k = (int) std::min<int64_t> (-idx, n - done);
            std::fill (l + done, l + done + k, 0.0f);
            std::fill (r + done, r + done + k, 0.0f);
            done += k;
            continue;
        }
        const size_t page = (size_t) (idx >> TapeBlock::shift);
        const int off = (int) (idx & (TapeBlock::frames - 1));
        const int k = std::min (n - done, TapeBlock::frames - off);
        const TapeBlock* b = page < pages.size() ? pages[page].get() : nullptr;
        if (b != nullptr)
        {
            std::memcpy (l + done, b->l + off, sizeof (float) * (size_t) k);
            std::memcpy (r + done, b->r + off, sizeof (float) * (size_t) k);
        }
        else
        {
            std::fill (l + done, l + done + k, 0.0f);
            std::fill (r + done, r + done + k, 0.0f);
        }
        done += k;
    }
}

void TapeMachine::readCommitted (int track, int64_t start, int n, float* l, float* r) const
{
    std::lock_guard<std::mutex> g (msgMutex);
    readPages (*current()[(size_t) std::clamp (track, 0, numTracks - 1)], start, n, l, r);
}

int64_t TapeMachine::contentEndFrames (int track) const
{
    std::lock_guard<std::mutex> g (msgMutex);
    const auto& list = *current()[(size_t) std::clamp (track, 0, numTracks - 1)];
    for (size_t p = list.size(); p-- > 0;)
        if (const auto* b = list[p].get())
            for (int i = TapeBlock::frames; i-- > 0;)
                if (b->l[i] != 0.0f || b->r[i] != 0.0f)
                    return std::min (lenFrames, (int64_t) p * TapeBlock::frames + i + 1);
    return 0;
}

int64_t TapeMachine::contentEndFrames() const
{
    int64_t e = 0;
    for (int t = 0; t < numTracks; ++t) e = std::max (e, contentEndFrames (t));
    return e;
}

// ---------------------------------------------------------------- editing
void TapeMachine::copyToClipboard (const PageList& pages, int64_t start, int64_t end)
{
    const int64_t n = std::max<int64_t> (0, end - start);
    clipL.assign ((size_t) n, 0.0f);
    clipR.assign ((size_t) n, 0.0f);
    for (int64_t done = 0; done < n;)
    {
        const int k = (int) std::min<int64_t> (n - done, 1 << 20);
        readPages (pages, start + done, k, clipL.data() + done, clipR.data() + done);
        done += k;
    }
}

bool TapeMachine::editTrack (int track, int64_t affectStart, int64_t affectEnd, const std::vector<Segment>& segs)
{
    const auto& old = *current()[(size_t) track];
    const size_t numPages = live[(size_t) track].size();
    affectEnd = std::min (affectEnd, lenFrames);
    if (affectEnd <= affectStart || numPages == 0) return false;

    auto list = std::make_shared<PageList> (std::max (old.size(), numPages));
    for (size_t p = 0; p < old.size(); ++p) (*list)[p] = old[p];

    std::vector<float> bl (TapeBlock::frames), br (TapeBlock::frames);
    const int64_t firstPage = affectStart >> TapeBlock::shift;
    const int64_t lastPage = std::min<int64_t> ((int64_t) numPages - 1, (affectEnd - 1) >> TapeBlock::shift);
    for (int64_t p = firstPage; p <= lastPage; ++p)
    {
        const int64_t f0 = p << TapeBlock::shift, f1 = f0 + TapeBlock::frames;
        readPages (old, f0, TapeBlock::frames, bl.data(), br.data());
        for (const auto& s : segs)
        {
            const int64_t a = std::max (f0, s.dst), b = std::min (f1, s.dst + s.length);
            if (a >= b) continue;
            const int off = (int) (a - f0), n = (int) (b - a);
            if (s.kind == 0) readPages (old, s.src + (a - s.dst), n, bl.data() + off, br.data() + off);
            else if (s.kind == 1)
            {
                const int64_t c = s.src + (a - s.dst);
                std::memcpy (bl.data() + off, clipL.data() + c, sizeof (float) * (size_t) n);
                std::memcpy (br.data() + off, clipR.data() + c, sizeof (float) * (size_t) n);
            }
            else
            {
                std::fill (bl.begin() + off, bl.begin() + off + n, 0.0f);
                std::fill (br.begin() + off, br.begin() + off + n, 0.0f);
            }
        }
        if (blockIsSilent (bl.data(), br.data(), TapeBlock::frames)) { (*list)[(size_t) p] = nullptr; continue; }
        auto blk = std::make_shared<TapeBlock>();
        std::memcpy (blk->l, bl.data(), sizeof (blk->l));
        std::memcpy (blk->r, br.data(), sizeof (blk->r));
        blk->frozen.store (true);
        (*list)[(size_t) p] = std::move (blk);
    }
    Snapshot s = current();
    s[(size_t) track] = list;
    pushSnapshotLocked (s);
    installLocked (current());
    return true;
}

namespace
{
int64_t pageEnd (int64_t frames) { return ((frames + TapeBlock::frames - 1) >> TapeBlock::shift) << TapeBlock::shift; }
} // namespace

bool TapeMachine::liftFrames (int track, int64_t start, int64_t end)
{
    if (track < 0 || track >= numTracks) return false;
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    start = std::clamp<int64_t> (start, 0, lenFrames);
    end = std::clamp<int64_t> (end, 0, lenFrames);
    if (end <= start || ! beginEdit (g)) return false;
    copyToClipboard (*current()[(size_t) track], start, end);
    return editTrack (track, start, end, { { start, end - start, 2, 0 } });
}

bool TapeMachine::cutFrames (int track, int64_t start, int64_t end)
{
    if (track < 0 || track >= numTracks) return false;
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    start = std::clamp<int64_t> (start, 0, lenFrames);
    end = std::clamp<int64_t> (end, 0, lenFrames);
    if (end <= start || ! beginEdit (g)) return false;
    const auto& old = *current()[(size_t) track];
    copyToClipboard (old, start, end);
    const int64_t len = end - start;
    int64_t oldEnd = 0;
    for (size_t p = old.size(); p-- > 0;)
        if (old[p] != nullptr) { oldEnd = (int64_t) (p + 1) << TapeBlock::shift; break; }
    oldEnd = std::min (std::max (oldEnd, end), lenFrames);
    return editTrack (track, start, oldEnd, { { start, lenFrames - start - len, 0, end }, { lenFrames - len, len, 2, 0 } });
}

bool TapeMachine::dropFrames (int track, int64_t at)
{
    if (track < 0 || track >= numTracks) return false;
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    at = std::clamp<int64_t> (at, 0, lenFrames);
    const int64_t len = std::min<int64_t> ((int64_t) clipL.size(), lenFrames - at);
    if (len <= 0 || ! beginEdit (g)) return false;
    return editTrack (track, at, at + len, { { at, len, 1, 0 } });
}

bool TapeMachine::spliceFrames (int track, int64_t at)
{
    if (track < 0 || track >= numTracks) return false;
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    at = std::clamp<int64_t> (at, 0, lenFrames);
    const int64_t len = std::min<int64_t> ((int64_t) clipL.size(), lenFrames - at);
    if (len <= 0 || ! beginEdit (g)) return false;
    const auto& old = *current()[(size_t) track];
    int64_t oldEnd = 0;
    for (size_t p = old.size(); p-- > 0;)
        if (old[p] != nullptr) { oldEnd = (int64_t) (p + 1) << TapeBlock::shift; break; }
    const int64_t affectEnd = std::min (lenFrames, pageEnd (std::max (oldEnd, at) + len));
    return editTrack (track, at, affectEnd, { { at, len, 1, 0 }, { at + len, lenFrames - at - len, 0, at } });
}

bool TapeMachine::clearTrack (int track)
{
    if (track < 0 || track >= numTracks) return false;
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    if (! beginEdit (g)) return false;
    Snapshot s = current();
    s[(size_t) track] = std::make_shared<PageList>();
    pushSnapshotLocked (s);
    installLocked (current());
    return true;
}

bool TapeMachine::clearAll()
{
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    if (! beginEdit (g)) return false;
    Snapshot s;
    for (auto& t : s) t = std::make_shared<PageList>();
    pushSnapshotLocked (s);
    installLocked (current());
    return true;
}

bool TapeMachine::lift (int track, double start, double end) { return liftFrames (track, toFrames (start), toFrames (end)); }
bool TapeMachine::cut (int track, double start, double end) { return cutFrames (track, toFrames (start), toFrames (end)); }
bool TapeMachine::drop (int track, double at) { return dropFrames (track, toFrames (at)); }
bool TapeMachine::splice (int track, double at) { return spliceFrames (track, toFrames (at)); }

// ---------------------------------------------------------------- transport
TapeTransportState TapeMachine::transport() const noexcept
{
    TapeTransportState s;
    s.positionSeconds = (double) posForUi.load (std::memory_order_relaxed) / sr;
    s.speed = speedForUi.load (std::memory_order_relaxed);
    s.playing = playingForUi.load (std::memory_order_relaxed);
    s.recording = recordingActive.load (std::memory_order_relaxed);
    s.loopStart = paramValue[LoopIn];
    s.loopEnd = paramValue[LoopOut];
    s.loopOn = paramValue[Loop] >= 0.5f;
    return s;
}

void TapeMachine::locate (double seconds) noexcept { locateRequest.store (std::max (0.0, seconds) * sr); }
void TapeMachine::locateFrames (int64_t frame) noexcept { locateRequest.store ((double) std::max<int64_t> (0, frame)); }

// ---------------------------------------------------------------- audio
inline void TapeMachine::fetch (int t, int64_t idx, float& l, float& r) const noexcept
{
    if (idx < 0 || idx >= lenFrames) { l = r = 0.0f; return; }
    const auto& lv = live[(size_t) t];
    const size_t page = (size_t) (idx >> TapeBlock::shift);
    const TapeBlock* b = page < lv.size() ? lv[page] : nullptr;
    if (b == nullptr) { l = r = 0.0f; return; }
    const int off = (int) (idx & (TapeBlock::frames - 1));
    l = b->l[off];
    r = b->r[off];
}

inline void TapeMachine::write (int t, int64_t idx, float l, float r, bool add) noexcept
{
    if (idx < 0 || idx >= lenFrames) return;
    auto& lv = live[(size_t) t];
    const size_t page = (size_t) (idx >> TapeBlock::shift);
    if (page >= lv.size()) return;
    TapeBlock* b = lv[page];
    if (b == nullptr || b->frozen.load (std::memory_order_relaxed))
    {
        TapeBlock* fresh = nullptr;
        if (! pool.pop (fresh))
        {
            dropoutCount.fetch_add (1, std::memory_order_relaxed);
            return;
        }
        poolCount.fetch_sub (1, std::memory_order_acq_rel);
        fresh->frozen.store (false, std::memory_order_relaxed);
        if (b != nullptr)
        {
            std::memcpy (fresh->l, b->l, sizeof (fresh->l));
            std::memcpy (fresh->r, b->r, sizeof (fresh->r));
        }
        else
        {
            std::memset (fresh->l, 0, sizeof (fresh->l));
            std::memset (fresh->r, 0, sizeof (fresh->r));
        }
        lv[page] = b = fresh;
    }
    const int off = (int) (idx & (TapeBlock::frames - 1));
    if (add)
    {
        b->l[off] += l;
        b->r[off] += r;
    }
    else
    {
        b->l[off] = l;
        b->r[off] = r;
    }
}

void TapeMachine::process (const TapeInputs& in, float* const* outs, int n) noexcept
{
    for (int c = 0; c < 2 * numTracks; ++c)
        if (outs[c] != nullptr) std::memset (outs[c], 0, sizeof (float) * (size_t) n);

    if (! exclusion.tryEnterAudio())
        return; // an edit is being applied on the message thread: tape is silent for this block

    if (const double req = locateRequest.exchange (std::numeric_limits<double>::quiet_NaN()); ! std::isnan (req))
    {
        pos = std::clamp (req, 0.0, (double) lenFrames);
        parked = false;
    }

    bool anyArmed = false;
    for (bool a : armed) anyArmed |= a;
    const bool recording = play && rec && anyArmed;
    recordingActive.store (recording, std::memory_order_release);

    const float targetSpeed = speed * (reverse ? -1.0f : 1.0f);
    if (! play || ! wasPlaying) speedCurrent = targetSpeed; // varispeed glides only while rolling
    wasPlaying = play;
    if (parked && (targetSpeed > 0.0f) != parkedForward) parked = false; // direction changed: leave the end

    const double loopA = std::round ((double) loopIn * sr), loopB = std::min (std::round ((double) loopOut * sr), (double) lenFrames);
    const bool looping = loopOn && loopB > loopA + 64.0;
    if (recording && ! wasRecording) loopPassCount = 1; // p42: a new take starts at pass 1
    wasRecording = recording;
    const bool wf = wow > 0.0f || flutter > 0.0f;
    const double wowInc = 0.55 / sr, flutterInc = 6.3 / sr;
    const float noiseCoeff = (float) (1.0 - std::exp (-dsp::twoPi * 20.0 / sr));

    std::array<float, numTracks> levelStep {};
    for (int t = 0; t < numTracks; ++t) levelStep[(size_t) t] = (levelTarget[(size_t) t] - levelCurrent[(size_t) t]) / (float) std::max (1, n);

    bool wrote = false;
    for (int i = 0; i < n; ++i)
    {
        const int64_t ip = (int64_t) std::floor (pos);
        const float frac = (float) (pos - (double) ip);
        for (int t = 0; t < numTracks; ++t)
        {
            float pl = 0.0f, pr = 0.0f;
            if (play && ! parked)
            {
                if (frac == 0.0f)
                    fetch (t, ip, pl, pr);
                else
                {
                    // 4-point Hermite interpolation
                    float xm1l, xm1r, x0l, x0r, x1l, x1r, x2l, x2r;
                    fetch (t, ip - 1, xm1l, xm1r);
                    fetch (t, ip, x0l, x0r);
                    fetch (t, ip + 1, x1l, x1r);
                    fetch (t, ip + 2, x2l, x2r);
                    auto herm = [frac] (float xm1, float x0, float x1, float x2) noexcept {
                        const float c1 = 0.5f * (x1 - xm1);
                        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
                        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                        return ((c3 * frac + c2) * frac + c1) * frac + x0;
                    };
                    pl = herm (xm1l, x0l, x1l, x2l);
                    pr = herm (xm1r, x0r, x1r, x2r);
                }
            }
            const int src = trackSrc[(size_t) t];
            const float* il = in.left[(size_t) src];
            const float* ir = in.right[(size_t) src];
            const float xl = il != nullptr ? il[i] : 0.0f;
            const float xr = ir != nullptr ? ir[i] : (il != nullptr ? il[i] : 0.0f);
            if (recording && armed[(size_t) t])
            {
                write (t, ip, xl, xr, overdub);
                wrote = true;
                if (! overdub) pl = pr = 0.0f; // replaced material is not heard
            }
            if (armed[(size_t) t] && src == tapesource::input) { pl += xl; pr += xr; } // input monitoring
            const float g = levelCurrent[(size_t) t] + levelStep[(size_t) t] * (float) i;
            if (float* o = outs[2 * t]) o[i] = pl * g;
            if (float* o = outs[2 * t + 1]) o[i] = pr * g;
        }

        if (play && ! parked)
        {
            speedCurrent += (targetSpeed - speedCurrent) * speedCoeff;
            if (std::abs (targetSpeed - speedCurrent) < 1.0e-6f) speedCurrent = targetSpeed;
            double inc = speedCurrent;
            if (wf)
            {
                wowPhase += wowInc;
                if (wowPhase >= 1.0) wowPhase -= 1.0;
                flutterPhase += flutterInc;
                if (flutterPhase >= 1.0) flutterPhase -= 1.0;
                flutterNoise += (rng.nextBipolar() - flutterNoise) * noiseCoeff;
                inc *= 1.0 + wow * 0.006 * std::sin (dsp::twoPi * wowPhase)
                       + flutter * 0.0015 * (std::sin (dsp::twoPi * flutterPhase) + 2.0 * flutterNoise);
            }
            const double prev = pos;
            pos += inc;
            if (looping)
            {
                if (inc > 0.0 && prev < loopB && pos >= loopB) { pos -= (loopB - loopA); if (recording) ++loopPassCount; }
                else if (inc < 0.0 && prev >= loopA && pos < loopA) { pos += (loopB - loopA); if (recording) ++loopPassCount; }
            }
            if (pos <= 0.0 && inc < 0.0) { pos = 0.0; parked = true; parkedForward = false; }
            else if (pos >= (double) lenFrames && inc > 0.0) { pos = (double) lenFrames; parked = true; parkedForward = true; }
            pos = std::clamp (pos, 0.0, (double) lenFrames);
        }
    }
    for (int t = 0; t < numTracks; ++t) levelCurrent[(size_t) t] = levelTarget[(size_t) t];
    if (wrote) dirty.store (true, std::memory_order_release);

    // tape character: saturation, hiss
    if (sat > 0.0f)
        for (int t = 0; t < numTracks; ++t)
            if (outs[2 * t] != nullptr && outs[2 * t + 1] != nullptr) saturators[(size_t) t].process (outs[2 * t], outs[2 * t + 1], n);
    if (hiss > 0.0f && play)
    {
        const float amp = dsp::dbToGain (-84.0f + 36.0f * hiss);
        for (int c = 0; c < 2 * numTracks; ++c)
            if (float* o = outs[c])
                for (int i = 0; i < n; ++i) o[i] += rng.nextBipolar() * amp;
    }

    posForUi.store ((int64_t) pos, std::memory_order_relaxed);
    loopPassForUi.store (recording && looping ? loopPassCount : 0, std::memory_order_relaxed); // p42
    speedForUi.store (play && ! parked ? speedCurrent : 0.0f, std::memory_order_relaxed);
    playingForUi.store (play, std::memory_order_relaxed);
    exclusion.exitAudio();
}

// ---------------------------------------------------------------- persistence
void TapeMachine::saveAudio (std::vector<uint8_t>& out) const
{
    out.clear();
    auto* self = const_cast<TapeMachine*> (this);
    std::lock_guard<std::mutex> g (msgMutex);
    if (! recordingActive.load())
    {
        ExclusionGuard ex (self->exclusion);
        self->commitLocked();
    }
    out.insert (out.end(), magic, magic + 8);
    put (out, audioFormatVersion);
    put (out, (uint32_t) numTracks);
    put (out, (uint32_t) TapeBlock::frames);
    put (out, sr);
    put (out, (uint64_t) lenFrames);
    const auto& snap = current();
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& list = *snap[(size_t) t];
        for (size_t p = 0; p < list.size(); ++p)
            if (const auto* b = list[p].get())
            {
                if (blockIsSilent (b->l, b->r, TapeBlock::frames)) continue;
                put (out, (uint32_t) t);
                put (out, (uint32_t) p);
                const auto* lp = reinterpret_cast<const uint8_t*> (b->l);
                const auto* rp = reinterpret_cast<const uint8_t*> (b->r);
                out.insert (out.end(), lp, lp + sizeof (b->l));
                out.insert (out.end(), rp, rp + sizeof (b->r));
            }
    }
    put (out, endMarker);
}

bool TapeMachine::loadAudio (const void* data, size_t size, bool asUndoStep)
{
    const auto* p = static_cast<const uint8_t*> (data);
    const auto* end = p + size;
    if (size < 8 || std::memcmp (p, magic, 8) != 0) return false;
    p += 8;
    uint32_t version = 0, tracks = 0, frames = 0;
    double rate = 0.0;
    uint64_t length = 0;
    if (! get (p, end, version) || version != audioFormatVersion || ! get (p, end, tracks) || tracks != (uint32_t) numTracks
        || ! get (p, end, frames) || frames != (uint32_t) TapeBlock::frames || ! get (p, end, rate) || ! get (p, end, length))
        return false;

    Snapshot snap;
    std::array<std::shared_ptr<PageList>, numTracks> lists;
    for (auto& l : lists) l = std::make_shared<PageList>();
    for (;;)
    {
        uint32_t t = 0, page = 0;
        if (! get (p, end, t)) return false;
        if (t == endMarker) break;
        if (t >= (uint32_t) numTracks || ! get (p, end, page) || (int64_t) page >= maxPagesEver) return false;
        if ((size_t) (end - p) < 2 * sizeof (float) * TapeBlock::frames) return false;
        auto blk = std::make_shared<TapeBlock>();
        std::memcpy (blk->l, p, sizeof (blk->l));
        p += sizeof (blk->l);
        std::memcpy (blk->r, p, sizeof (blk->r));
        p += sizeof (blk->r);
        for (auto* ch : { blk->l, blk->r }) // p63: a damaged file may hold NaN / Inf / huge samples
            for (int i = 0; i < TapeBlock::frames; ++i)
                ch[i] = std::isfinite (ch[i]) ? std::clamp (ch[i], -8.0f, 8.0f) : 0.0f;
        blk->frozen.store (true);
        auto& list = *lists[t];
        if (list.size() <= page) list.resize ((size_t) page + 1);
        list[page] = std::move (blk);
    }
    for (int t = 0; t < numTracks; ++t) snap[(size_t) t] = lists[(size_t) t];

    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    if (asUndoStep) // p38: loading a tape file is an edit - refused while recording, undo brings the old tape back
    {
        if (! beginEdit (g)) return false;
        pushSnapshotLocked (snap);
        installLocked (current());
        return true;
    }
    dirty.store (true);
    commitLocked(); // adopt uncommitted live blocks so they are freed with the old history
    installLocked (snap);
    history.clear();
    history.push_back (snap);
    cursor = 0;
    return true;
}

std::string TapeMachine::settingsJson() const
{
    nlohmann::json j;
    j["lengthSeconds"] = lengthSec;
    return j.dump();
}

void TapeMachine::loadSettingsJson (const std::string& text)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    auto j = jsonutil::parse (text);
    if (j.is_object() && j.contains ("lengthSeconds") && j["lengthSeconds"].is_number())
        setLengthSeconds (j["lengthSeconds"].get<double>());
}
catch (const std::exception&)
{
}

void TapeMachine::shareContentFrom (const TapeMachine& other)
{
    Snapshot snap;
    {
        auto& o = const_cast<TapeMachine&> (other);
        std::lock_guard<std::mutex> g (o.msgMutex);
        if (! o.recordingActive.load())
        {
            ExclusionGuard ex (o.exclusion);
            o.commitLocked();
        }
        snap = o.current();
    }
    std::unique_lock<std::mutex> g (msgMutex);
    ExclusionGuard ex (exclusion);
    dirty.store (true);
    commitLocked();
    installLocked (snap);
    history.clear();
    history.push_back (snap);
    cursor = 0;
}
} // namespace ohpe
