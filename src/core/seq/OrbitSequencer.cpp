// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OrbitSequencer.h"
#include "../model/FormatVersions.h"

#include "../harmony/KeyScale.h"

#include <nlohmann/json.hpp>

namespace ohpe
{
std::vector<ParamDescriptor> OrbitSequencer::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d {
        toggle ("on", "Orbit On", "Orbit an", false),
        integer ("part", "Orbit Target Part", "Orbit Ziel-Part", 1, 4, 4),
        continuous ("gate", "Orbit Gate", "Orbit Gate", 0.05f, 1.0f, 0.5f, ParamUnit::Percent),
    };
    static constexpr int defaultLength[numLanes] = { 16, 12, 10, 7 };
    for (int l = 0; l < numLanes; ++l)
    {
        const auto n = std::to_string (l + 1);
        const auto p = "l" + n + ".";
        d.push_back (integer (p + "len", "Lane " + n + " Length", "Spur " + n + " Länge", 1, maxSteps, defaultLength[l]));
        d.push_back (choice (p + "rate", "Lane " + n + " Rate", "Spur " + n + " Raster", seq::rateNames(), seq::rateNames(), seq::R1_16));
        d.push_back (integer (p + "transpose", "Lane " + n + " Transpose", "Spur " + n + " Transponieren", -24, 24, 0, ParamUnit::Semitones));
    }
    return d;
}

std::vector<ParamDescriptor> OrbitSequencer::getLateParamDescriptors() const
{
    using namespace pd;
    auto d = seq::swingHumanDescriptors ("Orbit", "Orbit");
    for (int l = 0; l < numLanes; ++l)
    {
        const auto n = std::to_string (l + 1);
        d.push_back (integer ("l" + n + ".part", "Lane " + n + " Target Part", "Spur " + n + " Ziel-Part", 0, 4, 0));
    }
    return d;
}

void OrbitSequencer::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void OrbitSequencer::reset() noexcept
{
    queue.reset();
    humaniser.reset();
    for (auto& l : lanes) l.clock.reset();
    for (auto& p : positions) p.store (-1);
    sampleClock = 0;
    wasPlaying = false;
}

void OrbitSequencer::setParam (int i, float v) noexcept
{
    const int iv = (int) std::lround (v);
    switch (i)
    {
        case On: on = v >= 0.5f; return;
        case Part: part = std::clamp (iv, 1, 4); return;
        case Gate: gate = v; return;
        case Swing: swing = std::clamp (v, 0.0f, 0.75f); return;
        case Human: human = std::clamp (v, 0.0f, 1.0f); return;
        default: break;
    }
    if (i >= LanePart1 && i < LanePart1 + numLanes)
    {
        lanes[(size_t) (i - LanePart1)].part = std::clamp (iv, 0, 4);
        return;
    }
    const int l = (i - firstLaneParam) / numLaneParams;
    if (l < 0 || l >= numLanes) return;
    auto& lane = lanes[(size_t) l];
    switch ((i - firstLaneParam) % numLaneParams)
    {
        case Length: lane.length = std::clamp (iv, 1, maxSteps); break;
        case Rate: lane.rate = std::clamp (iv, 0, seq::numRates - 1); break;
        case Transpose: lane.transpose = std::clamp (iv, -24, 24); break;
        default: break;
    }
}

uint32_t OrbitSequencer::pack (const Step& s) noexcept
{
    return (s.on ? 1u : 0u) | ((uint32_t) std::clamp (s.note, 0, 127) << 1) | ((uint32_t) std::clamp (s.velocity, 1, 127) << 8) | (1u << 15);
}

OrbitSequencer::Step OrbitSequencer::unpack (uint32_t v) noexcept
{
    Step s;
    if ((v & (1u << 15)) == 0) return s; // never written
    s.on = (v & 1u) != 0;
    s.note = (int) ((v >> 1) & 0x7f);
    s.velocity = std::max (1, (int) ((v >> 8) & 0x7f));
    return s;
}

OrbitSequencer::Step OrbitSequencer::getStep (int lane, int index) const noexcept
{
    if (lane < 0 || lane >= numLanes || index < 0 || index >= maxSteps) return {};
    return unpack (steps[(size_t) lane][(size_t) index].load (std::memory_order_acquire));
}

void OrbitSequencer::setStep (int lane, int index, const Step& s) noexcept
{
    if (lane < 0 || lane >= numLanes || index < 0 || index >= maxSteps) return;
    steps[(size_t) lane][(size_t) index].store (pack (s), std::memory_order_release);
}

void OrbitSequencer::clearPattern() noexcept
{
    for (auto& lane : steps)
        for (auto& s : lane) s.store (0, std::memory_order_release);
}

void OrbitSequencer::process (const Transport& tr, int numSamples, SequencerOutput& out) noexcept
{
    const int64_t blockStart = sampleClock;
    if (! tr.playing || ! on)
    {
        if (wasPlaying)
        {
            queue.releaseAll (blockStart);
            for (auto& l : lanes) l.clock.reset();
            for (auto& p : positions) p.store (-1, std::memory_order_relaxed);
            wasPlaying = false;
        }
    }
    else
    {
        wasPlaying = true;
        const int ch = std::clamp (part, 1, 16);
        for (int l = 0; l < numLanes; ++l)
        {
            auto& lane = lanes[(size_t) l];
            lane.clock.run (tr, seq::stepsPerQuarter (lane.rate), sr, numSamples, [&] (long long k, int offset, double sps) {
                const int idx = (int) (((k % lane.length) + lane.length) % lane.length);
                positions[(size_t) l].store (idx, std::memory_order_relaxed);
                const auto s = getStep (l, idx);
                if (! s.on) return;
                const int note = shiftNote (s.note, lane.transpose);
                const int laneCh = lane.part > 0 ? lane.part + lanePartChannelOffset : ch;
                int vel = s.velocity;
                const double spq = seq::stepsPerQuarter (lane.rate);
                const int64_t t = blockStart + offset + seq::swingSamples (k, swing, sps, spq) + humaniser.apply (human, sr, vel);
                // 0.5: gate 100 % ties a step to the step before it when both play the same note (not on the drum kit)
                const bool tieable = gate >= 0.999f && (lane.part > 0 || ! drumsMode);
                // 0.6: ... and overlaps a different next note by one sample (legato)
                const int64_t off = t + seq::gateSamples (sps, gate) + (tieable ? 1 : 0);
                if (tieable)
                {
                    const auto prev = getStep (l, (idx + lane.length - 1) % lane.length);
                    if (prev.on && shiftNote (prev.note, lane.transpose) == note && queue.extendOff (laneCh, note, off)) return;
                }
                queue.schedule (t, MidiEvent::noteOn (laneCh, note, vel));
                queue.schedule (off, MidiEvent::noteOff (laneCh, note));
            });
        }
    }
    queue.emit (blockStart, numSamples, out.events);
    out.events.sortByTime();
    sampleClock = blockStart + numSamples;
}

std::string OrbitSequencer::toJson() const
{
    nlohmann::json j;
    j["format"] = "ohpe-orbit";
    j["version"] = formats::orbit;
    auto arr = nlohmann::json::array();
    for (int l = 0; l < numLanes; ++l)
        for (int i = 0; i < maxSteps; ++i)
        {
            if ((steps[(size_t) l][(size_t) i].load() & (1u << 15)) == 0) continue;
            const auto s = getStep (l, i);
            arr.push_back ({ { "lane", l }, { "i", i }, { "on", s.on }, { "note", s.note }, { "vel", s.velocity } });
        }
    j["steps"] = arr;
    return j.dump();
}

bool OrbitSequencer::fromJson (const std::string& text)
{
    const auto j = nlohmann::json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-orbit") return false;
    clearPattern();
    if (! j.contains ("steps") || ! j["steps"].is_array()) return true;
    for (const auto& js : j["steps"])
    {
        if (! js.is_object()) continue;
        Step s;
        s.on = js.value ("on", false);
        s.note = js.value ("note", 60);
        s.velocity = js.value ("vel", 100);
        setStep (js.value ("lane", -1), js.value ("i", -1), s);
    }
    return true;
}

int OrbitSequencer::shiftNote (int note, int by) const noexcept
{
    if (keyLock && ! drumsMode) return keys::transposeInKey (note, by, keyLockRoot, keyLockMask); // p45: in scale degrees
    return std::clamp (note + by, 0, 127);
}
} // namespace ohpe
