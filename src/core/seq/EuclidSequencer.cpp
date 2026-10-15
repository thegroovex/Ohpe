// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EuclidSequencer.h"

namespace ohpe
{
uint32_t EuclidSequencer::pattern (int steps, int pulses) noexcept
{
    steps = std::clamp (steps, 1, maxSteps);
    pulses = std::clamp (pulses, 0, steps);
    if (pulses == 0) return 0u;
    if (pulses == steps) return steps == 32 ? 0xffffffffu : ((1u << steps) - 1u);

    // Bjorklund / Toussaint: start with `pulses` groups "1" and `steps - pulses` groups "0",
    // repeatedly append one remainder group to each front group until at most one remainder is left.
    struct Group { uint32_t bits; int len; };
    std::array<Group, maxSteps> a {}, b {}, na {}, nb {};
    int numA = pulses, numB = steps - pulses;
    for (int i = 0; i < numA; ++i) a[(size_t) i] = { 1u, 1 };
    for (int i = 0; i < numB; ++i) b[(size_t) i] = { 0u, 1 };
    bool first = true;
    while (numB > 1 || (first && numB == 1 && numA > 1))
    {
        first = false;
        const int n = std::min (numA, numB);
        for (int i = 0; i < n; ++i)
            na[(size_t) i] = { a[(size_t) i].bits | (b[(size_t) i].bits << a[(size_t) i].len), a[(size_t) i].len + b[(size_t) i].len };
        int numNb = 0;
        if (numA > n)
            for (int i = n; i < numA; ++i) nb[(size_t) numNb++] = a[(size_t) i];
        else
            for (int i = n; i < numB; ++i) nb[(size_t) numNb++] = b[(size_t) i];
        a = na;
        b = nb;
        numA = n;
        numB = numNb;
    }
    uint32_t out = 0;
    int pos = 0;
    for (int i = 0; i < numA; ++i) { out |= a[(size_t) i].bits << pos; pos += a[(size_t) i].len; }
    for (int i = 0; i < numB; ++i) { out |= b[(size_t) i].bits << pos; pos += b[(size_t) i].len; }
    return out;
}

uint32_t EuclidSequencer::rotated (uint32_t p, int steps, int rotation) noexcept
{
    steps = std::clamp (steps, 1, maxSteps);
    int r = rotation % steps;
    if (r < 0) r += steps;
    uint32_t out = 0;
    for (int i = 0; i < steps; ++i)
        if ((p >> i) & 1u) out |= 1u << ((i + r) % steps);
    return out;
}

std::vector<ParamDescriptor> EuclidSequencer::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d {
        toggle ("on", "Euclid On", "Euklid an", false),
        integer ("part", "Euclid Target Part", "Euklid Ziel-Part", 1, 4, 2),
        choice ("rate", "Euclid Rate", "Euklid Raster", seq::rateNames(), seq::rateNames(), seq::R1_16),
        continuous ("gate", "Euclid Gate", "Euklid Gate", 0.05f, 1.0f, 0.5f, ParamUnit::Percent),
        integer ("vel", "Euclid Velocity", "Euklid Anschlag", 1, 127, 100),
    };
    static constexpr int defaultPulses[numRings] = { 4, 0, 0, 0 };
    static constexpr int defaultNotes[numRings] = { 36, 38, 42, 46 };
    for (int r = 0; r < numRings; ++r)
    {
        const auto n = std::to_string (r + 1);
        const auto p = "r" + n + ".";
        d.push_back (integer (p + "steps", "Ring " + n + " Steps", "Ring " + n + " Schritte", 1, maxSteps, 16));
        d.push_back (integer (p + "pulses", "Ring " + n + " Pulses", "Ring " + n + " Impulse", 0, maxSteps, defaultPulses[r]));
        d.push_back (integer (p + "rot", "Ring " + n + " Rotation", "Ring " + n + " Rotation", 0, maxSteps - 1, 0));
        d.push_back (integer (p + "note", "Ring " + n + " Note", "Ring " + n + " Note", 0, 127, defaultNotes[r]));
    }
    return d;
}

void EuclidSequencer::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void EuclidSequencer::reset() noexcept
{
    queue.reset();
    clock.reset();
    sampleClock = 0;
    wasPlaying = false;
    position.store (-1);
    for (auto& p : ringPos) p.store (-1);
}

void EuclidSequencer::setParam (int i, float v) noexcept
{
    const int iv = (int) std::lround (v);
    switch (i)
    {
        case On: on = v >= 0.5f; return;
        case Part: part = std::clamp (iv, 1, 4); return;
        case Rate: rate = std::clamp (iv, 0, seq::numRates - 1); return;
        case Gate: gate = v; return;
        case Velocity: velocity = std::clamp (iv, 1, 127); return;
        default: break;
    }
    const int r = (i - firstRingParam) / numRingParams;
    if (r < 0 || r >= numRings) return;
    auto& ring = rings[(size_t) r];
    switch ((i - firstRingParam) % numRingParams)
    {
        case Steps: ring.steps = std::clamp (iv, 1, maxSteps); break;
        case Pulses: ring.pulses = std::clamp (iv, 0, maxSteps); break;
        case Rotation: ring.rotation = std::clamp (iv, 0, maxSteps - 1); break;
        case Note: ring.note = std::clamp (iv, 0, 127); break;
        default: break;
    }
    rebuild (ring);
}

void EuclidSequencer::process (const Transport& tr, int numSamples, SequencerOutput& out) noexcept
{
    const int64_t blockStart = sampleClock;
    const bool run = tr.playing && on;
    if (! run)
    {
        if (wasPlaying)
        {
            queue.releaseAll (blockStart);
            position.store (-1, std::memory_order_relaxed);
            for (auto& p : ringPos) p.store (-1, std::memory_order_relaxed);
            clock.reset();
            wasPlaying = false;
        }
    }
    else
    {
        wasPlaying = true;
        const int ch = std::clamp (part, 1, 16);
        clock.run (tr, seq::stepsPerQuarter (rate), sr, numSamples, [&] (long long k, int offset, double samplesPerStep) {
            position.store ((int) (((k % maxSteps) + maxSteps) % maxSteps), std::memory_order_relaxed);
            const int64_t t = blockStart + offset;
            const int64_t len = seq::gateSamples (samplesPerStep, gate);
            for (int r = 0; r < numRings; ++r)
            {
                const auto& ring = rings[(size_t) r];
                if (ring.pulses <= 0) continue;
                const int idx = (int) (((k % ring.steps) + ring.steps) % ring.steps);
                ringPos[(size_t) r].store (idx, std::memory_order_relaxed);
                if (((ring.mask >> idx) & 1u) == 0) continue;
                queue.schedule (t, MidiEvent::noteOn (ch, ring.note, velocity));
                queue.schedule (t + len, MidiEvent::noteOff (ch, ring.note));
            }
        });
    }
    queue.emit (blockStart, numSamples, out.events);
    out.events.sortByTime();
    sampleClock = blockStart + numSamples;
}
} // namespace ohpe
