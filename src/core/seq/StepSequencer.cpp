// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "StepSequencer.h"
#include "model/JsonParse.h" // p63: depth-limited parse

#include "../harmony/KeyScale.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdlib>

namespace ohpe
{
std::vector<ParamDescriptor> StepSequencer::getParamDescriptors() const
{
    using namespace pd;
    return {
        integer ("length", "Step Length", "Schrittanzahl", 1, maxSteps, 16),
        choice ("rate", "Step Rate", "Schrittraster", { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" },
                { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, R1_16),
        continuous ("swing", "Swing", "Swing", 0.0f, 0.75f, 0.0f, ParamUnit::Percent),
        integer ("transpose", "Transpose", "Transponieren", -24, 24, 0, ParamUnit::Semitones),
        integer ("channel", "Target Part", "Ziel-Part", 1, 4, 1),
    };
}

std::vector<ParamDescriptor> StepSequencer::getChainParamDescriptors() const
{
    using namespace pd;
    return {
        integer ("pattern", "Pattern", "Pattern", 1, numPatterns, 1),
        toggle ("song", "Song Mode", "Song-Modus", false),
    };
}

double StepSequencer::stepsPerQuarter (int r) noexcept
{
    switch (r)
    {
        case R1_4: return 1.0;
        case R1_8: return 2.0;
        case R1_32: return 8.0;
        case R1_8T: return 3.0;
        case R1_16T: return 6.0;
        default: return 4.0;
    }
}

void StepSequencer::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void StepSequencer::reset() noexcept
{
    for (auto& p : pending) p.used = false;
    noteRefs.fill (0);
    lastStepPos = -1.0;
    sampleClock = 0;
    wasPlaying = false;
    position.store (-1);
    playPattern.store (-1);
    playEntry.store (-1);
    playingPattern = requestedPattern;
    patternStart = 0;
}

void StepSequencer::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Length: length = std::clamp ((int) v, 1, maxSteps); break;
        case Rate: rate = (int) v; break;
        case Swing: swing = v; break;
        case Transpose: transpose = (int) v; break;
        case Channel: channel = std::clamp ((int) v, 1, 16); break;
        case Pattern: requestedPattern = std::clamp ((int) std::lround (v) - 1, 0, numPatterns - 1); break;
        case Song: songMode = v >= 0.5f; break;
        default: break;
    }
}

// ---------------------------------------------------------------- packing
uint64_t StepSequencer::pack (const Step& s) noexcept
{
    const uint64_t on = s.on ? 1u : 0u;
    const uint64_t note = (uint64_t) std::clamp (s.note, 0, 127);
    const uint64_t vel = (uint64_t) std::clamp (s.velocity, 1, 127);
    const uint64_t gate = (uint64_t) std::lround (std::clamp (s.gate, 0.0f, 1.0f) * 1000.0f); // 10 bits
    const uint64_t prob = (uint64_t) std::clamp (s.probability, 0, 100);
    const uint64_t rat = (uint64_t) (std::clamp (s.ratchet, 1, 8) - 1);
    return on | (note << 1) | (vel << 8) | (gate << 15) | (prob << 25) | (rat << 32);
}

void StepSequencer::unpack (uint64_t v, Step& s) noexcept
{
    s.on = (v & 1u) != 0;
    s.note = (int) ((v >> 1) & 0x7f);
    s.velocity = std::max (1, (int) ((v >> 8) & 0x7f));
    s.gate = (float) ((v >> 15) & 0x3ff) / 1000.0f;
    s.probability = (int) ((v >> 25) & 0x7f);
    s.ratchet = (int) ((v >> 32) & 0x7) + 1;
}

uint32_t StepSequencer::packLock (const Lock& l) noexcept
{
    if (l.paramIndex < 0 || l.paramIndex > 0xfffe) return 0;
    const uint32_t value = (uint32_t) std::lround (std::clamp (l.normValue, 0.0f, 1.0f) * 65535.0f);
    return ((uint32_t) (l.paramIndex + 1)) | (value << 16);
}

StepSequencer::Lock StepSequencer::unpackLock (uint32_t v) noexcept
{
    Lock l;
    const int idx = (int) (v & 0xffffu);
    if (idx == 0) return l;
    l.paramIndex = idx - 1;
    l.normValue = (float) (v >> 16) / 65535.0f;
    return l;
}

StepSequencer::Step StepSequencer::getStep (int index) const noexcept { return getStep (editPattern.load(), index); }

void StepSequencer::setStep (int index, const Step& s) noexcept { setStep (editPattern.load(), index, s); }

StepSequencer::Step StepSequencer::getStep (int pattern, int index) const noexcept
{
    Step s;
    if (index < 0 || index >= maxSteps || pattern < 0 || pattern >= numPatterns) return s;
    const uint64_t v = steps[(size_t) pattern][(size_t) index].load (std::memory_order_acquire);
    if (v == 0) { s = Step {}; }
    else unpack (v, s);
    for (int k = 0; k < maxLocksPerStep; ++k)
        s.locks[(size_t) k] = unpackLock (locks[(size_t) pattern][(size_t) index][(size_t) k].load (std::memory_order_acquire));
    return s;
}

void StepSequencer::setStep (int pattern, int index, const Step& s) noexcept
{
    if (index < 0 || index >= maxSteps || pattern < 0 || pattern >= numPatterns) return;
    for (int k = 0; k < maxLocksPerStep; ++k)
        locks[(size_t) pattern][(size_t) index][(size_t) k].store (packLock (s.locks[(size_t) k]), std::memory_order_release);
    steps[(size_t) pattern][(size_t) index].store (pack (s), std::memory_order_release);
}

void StepSequencer::setPatternLength (int pattern, int len) noexcept
{
    if (pattern >= 0 && pattern < numPatterns) patternLength[(size_t) pattern].store (std::clamp (len, 0, maxSteps));
}

int StepSequencer::doubleSteps (int pattern) noexcept
{
    if (pattern < 0 || pattern >= numPatterns) return 0;
    const int len = lengthOf (pattern);
    if (len <= 0 || len * 2 > maxSteps) return 0;
    for (int i = 0; i < len; ++i) setStep (pattern, len + i, getStep (pattern, i));
    return len * 2;
}

void StepSequencer::copyPattern (int from, int to) noexcept
{
    if (from < 0 || from >= numPatterns || to < 0 || to >= numPatterns || from == to) return;
    for (int i = 0; i < maxSteps; ++i) setStep (to, i, getStep (from, i));
    setPatternLength (to, getPatternLength (from));
}

int StepSequencer::getPatternLength (int pattern) const noexcept
{
    return pattern >= 0 && pattern < numPatterns ? patternLength[(size_t) pattern].load() : 0;
}

void StepSequencer::setChain (const std::vector<ChainEntry>& entries) noexcept
{
    const int n = std::min ((int) entries.size(), maxChain);
    chainLength.store (0, std::memory_order_release); // the audio thread never reads a half-written chain
    for (int i = 0; i < n; ++i)
        chain[(size_t) i].store ((uint32_t) std::clamp (entries[(size_t) i].pattern, 0, numPatterns - 1)
                                     | ((uint32_t) std::clamp (entries[(size_t) i].repeats, 1, 16) << 8),
                                 std::memory_order_release);
    chainLength.store (n, std::memory_order_release);
}

std::vector<StepSequencer::ChainEntry> StepSequencer::getChain() const
{
    std::vector<ChainEntry> c;
    const int n = chainLength.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
    {
        const uint32_t v = chain[(size_t) i].load (std::memory_order_acquire);
        c.push_back ({ (int) (v & 0xff), (int) ((v >> 8) & 0xff) });
    }
    return c;
}

int StepSequencer::lengthOf (int pattern) const noexcept
{
    const int l = patternLength[(size_t) std::clamp (pattern, 0, numPatterns - 1)].load (std::memory_order_relaxed);
    return l > 0 ? l : length;
}

void StepSequencer::locate (long long k, int& pattern, int& stepIndex) noexcept
{
    const int n = chainLength.load (std::memory_order_acquire);
    if (songMode && n > 0)
    {
        // song: the chain loops from the song start; stateless, so loops / jumps land correctly
        long long total = 0;
        for (int i = 0; i < n; ++i)
        {
            const uint32_t v = chain[(size_t) i].load (std::memory_order_relaxed);
            total += (long long) lengthOf ((int) (v & 0xff)) * (long long) ((v >> 8) & 0xff);
        }
        long long pos = total > 0 ? ((k % total) + total) % total : 0;
        for (int i = 0; i < n; ++i)
        {
            const uint32_t v = chain[(size_t) i].load (std::memory_order_relaxed);
            const int p = (int) (v & 0xff);
            const long long span = (long long) lengthOf (p) * (long long) ((v >> 8) & 0xff);
            if (pos < span)
            {
                pattern = p;
                stepIndex = (int) (pos % lengthOf (p));
                playEntry.store (i, std::memory_order_relaxed);
                return;
            }
            pos -= span;
        }
    }
    playEntry.store (-1, std::memory_order_relaxed);
    // single pattern: a newly selected pattern starts when the playing one wraps
    int len = lengthOf (playingPattern);
    long long idx = (((k - patternStart) % len) + len) % len;
    if (idx == 0 && requestedPattern != playingPattern)
    {
        playingPattern = requestedPattern;
        patternStart = k;
        len = lengthOf (playingPattern);
    }
    pattern = playingPattern;
    stepIndex = (int) ((((k - patternStart) % len) + len) % len);
}

void StepSequencer::toggleStep (int index, int note) noexcept
{
    auto s = getStep (index);
    if (! s.on) s.note = note;
    s.on = ! s.on;
    setStep (index, s);
}

void StepSequencer::clearPattern() noexcept
{
    for (int p = 0; p < numPatterns; ++p)
    {
        for (int i = 0; i < maxSteps; ++i) setStep (p, i, Step {});
        patternLength[(size_t) p].store (0);
    }
    chainLength.store (0);
}

// ---------------------------------------------------------------- playback
void StepSequencer::schedule (int64_t time, const MidiEvent& e) noexcept
{
    for (auto& p : pending)
        if (! p.used) { p.time = time; p.event = e; p.used = true; return; }
    // full: drop (a note-off is never dropped silently in practice: 256 slots >> 64 steps x 8 ratchets / block)
}

bool StepSequencer::extendOff (int ch, int note, int64_t time) noexcept
{
    for (auto& p : pending)
        if (p.used && p.event.isNoteOff() && p.event.channel() == ch && p.event.data1 == note)
        {
            p.time = std::max (p.time, time);
            return true;
        }
    return false;
}

void StepSequencer::triggerStep (int pattern, int stepIndex, int offset, double samplesPerStep, int64_t blockStart, SequencerOutput& out) noexcept
{
    position.store (stepIndex, std::memory_order_relaxed);
    playPattern.store (pattern, std::memory_order_relaxed);
    const Step s = getStep (pattern, stepIndex);

    // parameter locks: a new step releases the previous step's locks
    out.addLock ({ offset, -1, 0.0f, true });
    if (s.on && reg != nullptr)
        for (const auto& l : s.locks)
            if (l.paramIndex >= 0 && l.paramIndex < reg->size())
                out.addLock ({ offset, l.paramIndex, reg->descriptor (l.paramIndex).fromNormalised (l.normValue), false });

    if (! s.on) return;
    const int note = shiftNote (s.note, transpose);
    const int hits = std::clamp (s.ratchet, 1, 8);
    const double hitLen = samplesPerStep / hits;
    // 0.6: a single hit at gate 100 % ends one sample after the next step starts - a different note there
    // overlaps it (legato: mono parts glide in glide mode Legato)
    const double gateLen = std::max (16.0, hitLen * std::clamp ((double) s.gate, 0.05, 1.0) - (s.gate >= 1.0f ? (hits == 1 ? -1.0 : 0.0) : 1.0));
    const int ch = std::clamp (channel, 1, 16);

    // 0.5: a step at gate 100 % ties into the next step when that plays the same note (unless the
    // sequencer plays the drum kit): the note continues and ends with the next step's gate
    if (! drumsMode && hits == 1)
    {
        const int len = lengthOf (pattern);
        const Step prev = getStep (pattern, (stepIndex + len - 1) % len);
        if (prev.on && prev.gate >= 1.0f && prev.ratchet <= 1 && shiftNote (prev.note, transpose) == note
            && extendOff (ch, note, blockStart + offset + (int64_t) std::llround (gateLen)))
            return;
    }
    if (s.probability < 100 && rng.next01() * 100.0f >= (float) s.probability) return;

    for (int h = 0; h < hits; ++h)
    {
        const int64_t t = blockStart + offset + (int64_t) std::llround (h * hitLen);
        schedule (t, MidiEvent::noteOn (ch, note, s.velocity));
        schedule (t + (int64_t) std::llround (gateLen), MidiEvent::noteOff (ch, note));
    }
}

void StepSequencer::process (const Transport& tr, int numSamples, SequencerOutput& out) noexcept
{
    const int64_t blockStart = sampleClock;
    const int64_t blockEnd = sampleClock + numSamples;

    if (! tr.playing || tr.bpm <= 0.0)
    {
        if (wasPlaying)
        {
            // stop: every sounding note ends now, locks are released
            for (auto& p : pending)
                if (p.used && p.event.isNoteOff()) { p.time = blockStart; }
                else p.used = false;
            out.addLock ({ 0, -1, 0.0f, true });
            position.store (-1, std::memory_order_relaxed);
            playPattern.store (-1, std::memory_order_relaxed);
            playEntry.store (-1, std::memory_order_relaxed);
            wasPlaying = false;
        }
    }
    else
    {
        const double spq = stepsPerQuarter (rate);
        const double samplesPerStep = sr * 60.0 / tr.bpm / spq;
        const double startPos = tr.ppqAtBlockStart * spq;
        const double endPos = startPos + numSamples / samplesPerStep;
        if (! wasPlaying || startPos < lastStepPos - 0.5 || startPos > lastStepPos + 2.0 + numSamples / samplesPerStep)
        {
            lastStepPos = std::ceil (startPos - 1.0e-6) - 1.0; // (re)start or jump: trigger the first boundary at/after startPos
            playingPattern = requestedPattern;
            patternStart = 0;
        }
        wasPlaying = true;

        for (double k = lastStepPos + 1.0; k < endPos - 1.0e-9; k += 1.0)
        {
            if (k < startPos - 1.0e-6) { lastStepPos = k; continue; }
            int offset = (int) std::floor ((k - startPos) * samplesPerStep + 0.5);
            offset = std::clamp (offset, 0, numSamples - 1);
            const long long kk = (long long) std::llround (k);
            int pattern = 0, stepIndex = 0;
            locate (kk, pattern, stepIndex);
            int swingOffset = 0;
            if ((kk & 1) != 0) swingOffset = (int) std::lround (swing * 0.5 * samplesPerStep);
            triggerStep (pattern, stepIndex, offset + swingOffset, samplesPerStep, blockStart, out);
            // locks belong to the visible block
            lastStepPos = k;
        }
    }

    // emit due events: note-offs first at equal time so a retrigger is not cut
    for (int pass = 0; pass < 2; ++pass)
        for (auto& p : pending)
        {
            if (! p.used || p.time >= blockEnd) continue;
            const bool isOff = p.event.isNoteOff();
            if ((pass == 0) != isOff) continue;
            auto e = p.event;
            e.sampleOffset = (int) std::clamp<int64_t> (p.time - blockStart, 0, numSamples - 1);
            int& ref = noteRefs[(size_t) e.data1];
            if (isOff)
            {
                if (ref > 0 && --ref == 0) out.events.add (e);
            }
            else
            {
                ++ref;
                out.events.add (e);
            }
            p.used = false;
        }
    out.events.sortByTime();
    sampleClock = blockEnd;
}

// ---------------------------------------------------------------- JSON
// Version 1 format: pattern 1 in "steps" (as in 0.1); patterns 2-16 in "patterns", own pattern
// lengths in "lengths", the song chain in "chain". Older versions read pattern 1 only.
nlohmann::json StepSequencer::patternToJson (int p) const
{
    auto arr = nlohmann::json::array();
    for (int i = 0; i < maxSteps; ++i)
    {
        const auto s = getStep (p, i);
        const bool hasLocks = std::any_of (s.locks.begin(), s.locks.end(), [] (const Lock& l) { return l.paramIndex >= 0; });
        // untouched steps are not written: never set (0) or reset to the default step
        const uint64_t raw = steps[(size_t) p][(size_t) i].load();
        if (! s.on && ! hasLocks && (raw == 0 || raw == pack (Step {}))) continue;
        nlohmann::json js { { "i", i }, { "on", s.on }, { "note", s.note }, { "vel", s.velocity }, { "gate", s.gate },
                            { "prob", s.probability }, { "ratchet", s.ratchet } };
        auto jl = nlohmann::json::array();
        for (const auto& l : s.locks)
            if (l.paramIndex >= 0 && reg != nullptr && l.paramIndex < reg->size())
                jl.push_back ({ { "param", reg->descriptor (l.paramIndex).id }, { "value", l.normValue } });
        if (! jl.empty()) js["locks"] = jl;
        arr.push_back (js);
    }
    return arr;
}

void StepSequencer::patternFromJson (int p, const nlohmann::json& arr)
{
    if (! arr.is_array()) return;
    for (const auto& js : arr)
    {
        if (! js.is_object()) continue;
        const int i = js.value ("i", -1);
        if (i < 0 || i >= maxSteps) continue;
        Step s;
        s.on = js.value ("on", false);
        s.note = js.value ("note", 60);
        s.velocity = js.value ("vel", 100);
        s.gate = js.value ("gate", 0.5f);
        s.probability = js.value ("prob", 100);
        s.ratchet = js.value ("ratchet", 1);
        if (js.contains ("locks") && js["locks"].is_array() && reg != nullptr)
        {
            int k = 0;
            for (const auto& jl : js["locks"])
            {
                if (k >= maxLocksPerStep) break;
                const int idx = reg->indexOf (jl.value ("param", ""));
                if (idx < 0) continue; // unknown parameter (newer / older version): skip
                s.locks[(size_t) k++] = { idx, jl.value ("value", 0.0f) };
            }
        }
        setStep (p, i, s);
    }
}

std::string StepSequencer::toJson() const
{
    nlohmann::json j;
    j["format"] = "ohpe-step-pattern";
    j["version"] = 1;
    j["steps"] = patternToJson (0);
    auto pats = nlohmann::json::array();
    for (int p = 1; p < numPatterns; ++p)
    {
        auto arr = patternToJson (p);
        if (! arr.empty()) pats.push_back ({ { "p", p + 1 }, { "steps", arr } });
    }
    if (! pats.empty()) j["patterns"] = pats;
    auto lens = nlohmann::json::object();
    for (int p = 0; p < numPatterns; ++p)
        if (const int l = getPatternLength (p); l > 0) lens[std::to_string (p + 1)] = l;
    if (! lens.empty()) j["lengths"] = lens;
    const auto c = getChain();
    if (! c.empty())
    {
        auto jc = nlohmann::json::array();
        for (const auto& e : c) jc.push_back ({ { "p", e.pattern + 1 }, { "repeat", e.repeats } });
        j["chain"] = jc;
    }
    return j.dump();
}

bool StepSequencer::fromJson (const std::string& text)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    const auto j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-step-pattern")
        return false;
    clearPattern();
    if (j.contains ("steps")) patternFromJson (0, j["steps"]);
    if (j.contains ("patterns") && j["patterns"].is_array())
        for (const auto& jp : j["patterns"])
            if (jp.is_object() && jp.contains ("steps"))
            {
                const int p = jp.value ("p", 0) - 1;
                if (p >= 1 && p < numPatterns) patternFromJson (p, jp["steps"]);
            }
    if (j.contains ("lengths") && j["lengths"].is_object())
        for (const auto& [k, v] : j["lengths"].items())
        {
            const int p = std::atoi (k.c_str()) - 1;
            if (v.is_number_integer()) setPatternLength (p, v.get<int>());
        }
    if (j.contains ("chain") && j["chain"].is_array())
    {
        std::vector<ChainEntry> c;
        for (const auto& e : j["chain"])
            if (e.is_object()) c.push_back ({ e.value ("p", 1) - 1, e.value ("repeat", 1) });
        setChain (c);
    }
    return true;
}
catch (const std::exception&)
{
    clearPattern();
    return false;
}

int StepSequencer::shiftNote (int note, int by) const noexcept
{
    if (keyLock && ! drumsMode) return keys::transposeInKey (note, by, keyLockRoot, keyLockMask); // p45: in scale degrees
    return std::clamp (note + by, 0, 127);
}
} // namespace ohpe
