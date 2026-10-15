// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "StepSequencer.h"

#include <nlohmann/json.hpp>

#include <cmath>

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

StepSequencer::Step StepSequencer::getStep (int index) const noexcept
{
    Step s;
    if (index < 0 || index >= maxSteps) return s;
    const uint64_t v = steps[(size_t) index].load (std::memory_order_acquire);
    if (v == 0) { s = Step {}; }
    else unpack (v, s);
    for (int k = 0; k < maxLocksPerStep; ++k)
        s.locks[(size_t) k] = unpackLock (locks[(size_t) index][(size_t) k].load (std::memory_order_acquire));
    return s;
}

void StepSequencer::setStep (int index, const Step& s) noexcept
{
    if (index < 0 || index >= maxSteps) return;
    for (int k = 0; k < maxLocksPerStep; ++k)
        locks[(size_t) index][(size_t) k].store (packLock (s.locks[(size_t) k]), std::memory_order_release);
    steps[(size_t) index].store (pack (s), std::memory_order_release);
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
    for (int i = 0; i < maxSteps; ++i) setStep (i, Step {});
}

// ---------------------------------------------------------------- playback
void StepSequencer::schedule (int64_t time, const MidiEvent& e) noexcept
{
    for (auto& p : pending)
        if (! p.used) { p.time = time; p.event = e; p.used = true; return; }
    // full: drop (a note-off is never dropped silently in practice: 256 slots >> 64 steps x 8 ratchets / block)
}

void StepSequencer::triggerStep (int stepIndex, int offset, double samplesPerStep, int64_t blockStart, SequencerOutput& out) noexcept
{
    position.store (stepIndex, std::memory_order_relaxed);
    const Step s = getStep (stepIndex);

    // parameter locks: a new step releases the previous step's locks
    out.addLock ({ offset, -1, 0.0f, true });
    if (s.on && reg != nullptr)
        for (const auto& l : s.locks)
            if (l.paramIndex >= 0 && l.paramIndex < reg->size())
                out.addLock ({ offset, l.paramIndex, reg->descriptor (l.paramIndex).fromNormalised (l.normValue), false });

    if (! s.on) return;
    if (s.probability < 100 && rng.next01() * 100.0f >= (float) s.probability) return;

    const int note = std::clamp (s.note + transpose, 0, 127);
    const int hits = std::clamp (s.ratchet, 1, 8);
    const double hitLen = samplesPerStep / hits;
    const double gateLen = std::max (16.0, hitLen * std::clamp ((double) s.gate, 0.05, 1.0) - (s.gate >= 1.0f ? 0.0 : 1.0));
    const int ch = std::clamp (channel, 1, 16);
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
            lastStepPos = std::ceil (startPos - 1.0e-6) - 1.0; // (re)start or jump: trigger the first boundary at/after startPos
        wasPlaying = true;

        for (double k = lastStepPos + 1.0; k < endPos - 1.0e-9; k += 1.0)
        {
            if (k < startPos - 1.0e-6) { lastStepPos = k; continue; }
            int offset = (int) std::floor ((k - startPos) * samplesPerStep + 0.5);
            offset = std::clamp (offset, 0, numSamples - 1);
            const long long kk = (long long) std::llround (k);
            int stepIndex = (int) (kk % length);
            if (stepIndex < 0) stepIndex += length;
            int swingOffset = 0;
            if ((kk & 1) != 0) swingOffset = (int) std::lround (swing * 0.5 * samplesPerStep);
            triggerStep (stepIndex, offset + swingOffset, samplesPerStep, blockStart, out);
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
std::string StepSequencer::toJson() const
{
    nlohmann::json j;
    j["format"] = "ohpe-step-pattern";
    j["version"] = 1;
    auto arr = nlohmann::json::array();
    for (int i = 0; i < maxSteps; ++i)
    {
        const auto s = getStep (i);
        const bool hasLocks = std::any_of (s.locks.begin(), s.locks.end(), [] (const Lock& l) { return l.paramIndex >= 0; });
        // untouched steps are not written: never set (0) or reset to the default step
        const uint64_t raw = steps[(size_t) i].load();
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
    j["steps"] = arr;
    return j.dump();
}

bool StepSequencer::fromJson (const std::string& text)
{
    const auto j = nlohmann::json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-step-pattern")
        return false;
    clearPattern();
    if (! j.contains ("steps") || ! j["steps"].is_array()) return true;
    for (const auto& js : j["steps"])
    {
        const int i = js.value ("i", -1);
        if (i < 0 || i >= maxSteps) continue;
        Step s;
        s.on = js.value ("on", false);
        s.note = js.value ("note", 60);
        s.velocity = js.value ("vel", 100);
        s.gate = js.value ("gate", 0.5f);
        s.probability = js.value ("prob", 100);
        s.ratchet = js.value ("ratchet", 1);
        if (js.contains ("locks") && reg != nullptr)
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
        setStep (i, s);
    }
    return true;
}
} // namespace ohpe
