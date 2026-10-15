// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "ISequencer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ohpe::seq
{
/**
    Note rates shared by the Euclid, Arp and Orbit sequencers (the value of their "rate"
    choice parameters; append-only).
*/
enum Rate
{
    R1_4 = 0, R1_8, R1_16, R1_32, R1_4T, R1_8T, R1_16T, R1_32T, R1_4D, R1_8D, R1_16D, numRates
};

inline double stepsPerQuarter (int rate) noexcept
{
    static constexpr double spq[numRates] = { 1.0, 2.0, 4.0, 8.0, 1.5, 3.0, 6.0, 12.0, 2.0 / 3.0, 4.0 / 3.0, 8.0 / 3.0 };
    return spq[std::clamp (rate, 0, numRates - 1)];
}

inline std::vector<std::string> rateNames()
{
    return { "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/32T", "1/4.", "1/8.", "1/16." };
}

/**
    Finds the step boundaries of a rate grid inside one block, from the transport's
    quarter-note position. Positions are absolute (step k starts at ppq = k / stepsPerQuarter),
    so any number of grids stays phase-locked to the host or the internal clock, and loops /
    jumps simply restart at the next boundary. Audio thread, no allocation.
*/
class GridClock
{
public:
    void reset() noexcept { last = -1.0; running = false; }

    /** Calls onStep (long long k, int offset, double samplesPerStep) for each boundary in the block. */
    template <typename F>
    void run (const Transport& tr, double spq, double sampleRate, int numSamples, F&& onStep) noexcept
    {
        if (! tr.playing || tr.bpm <= 0.0 || numSamples <= 0)
        {
            running = false;
            return;
        }
        const double samplesPerStep = sampleRate * 60.0 / tr.bpm / spq;
        const double startPos = tr.ppqAtBlockStart * spq;
        const double endPos = startPos + numSamples / samplesPerStep;
        if (! running || startPos < last - 0.5 || startPos > last + 2.0 + numSamples / samplesPerStep)
            last = std::ceil (startPos - 1.0e-6) - 1.0; // (re)start or jump
        running = true;
        for (double k = last + 1.0; k < endPos - 1.0e-9; k += 1.0)
        {
            last = k;
            if (k < startPos - 1.0e-6) continue;
            const int offset = std::clamp ((int) std::floor ((k - startPos) * samplesPerStep + 0.5), 0, numSamples - 1);
            onStep ((long long) std::llround (k), offset, samplesPerStep);
        }
    }

private:
    double last = -1.0;
    bool running = false;
};

/**
    Pending note events with absolute sample times (note-offs scheduled at gate end) and
    reference counts per note, so overlapping notes of one sequencer never cut each other.
*/
class NoteQueue
{
public:
    static constexpr int capacity = 512;

    void reset() noexcept
    {
        for (auto& p : pending) p.used = false;
        refs.fill (0);
    }

    bool schedule (int64_t time, const MidiEvent& e) noexcept
    {
        for (auto& p : pending)
            if (! p.used) { p.time = time; p.event = e; p.used = true; return true; }
        return false;
    }

    /** 0.5 (ties): moves the pending note-off of this channel / note to `time`; false when there is none. */
    bool extendOff (int channel, int note, int64_t time) noexcept
    {
        for (auto& p : pending)
            if (p.used && p.event.isNoteOff() && p.event.channel() == channel && p.event.data1 == note)
            {
                p.time = std::max (p.time, time);
                return true;
            }
        return false;
    }

    /** Moves every pending note-off to `time` and drops pending note-ons (stop / all off). */
    void releaseAll (int64_t time) noexcept
    {
        for (auto& p : pending)
        {
            if (! p.used) continue;
            if (p.event.isNoteOff()) p.time = std::min (p.time, time);
            else p.used = false;
        }
    }

    /** Emits all events due before blockStart + numSamples (offs first at equal times). */
    template <int Cap>
    void emit (int64_t blockStart, int numSamples, EventList<Cap>& out) noexcept
    {
        const int64_t blockEnd = blockStart + numSamples;
        for (int pass = 0; pass < 2; ++pass)
            for (auto& p : pending)
            {
                if (! p.used || p.time >= blockEnd) continue;
                const bool isOff = p.event.isNoteOff();
                if ((pass == 0) != isOff) continue;
                auto e = p.event;
                e.sampleOffset = (int) std::clamp<int64_t> (p.time - blockStart, 0, numSamples - 1);
                int& ref = refs[(size_t) ((e.channel() - 1) * 128 + e.data1)];
                if (isOff)
                {
                    if (ref > 0 && --ref == 0) out.add (e);
                }
                else
                {
                    ++ref;
                    out.add (e);
                }
                p.used = false;
            }
    }

    bool hasPending() const noexcept
    {
        for (const auto& p : pending)
            if (p.used) return true;
        return false;
    }

private:
    struct Pending
    {
        int64_t time = 0;
        MidiEvent event;
        bool used = false;
    };
    std::array<Pending, capacity> pending {};
    std::array<int, 16 * 128> refs {};
};

/**
    0.5: swing delay of step k of a grid with `spq` steps per quarter: notes on the off-beat sixteenths
    move late by swing / 2 of a sixteenth (0.75 = 3/8), as on drum machines. Grids at 1/8 or slower
    never land on an off-beat sixteenth, so a whole groove swings together.
*/
inline int64_t swingSamples (long long k, float swing, double samplesPerStep, double spq) noexcept
{
    if (swing <= 0.0f || spq <= 0.0) return 0;
    const double pos16 = (double) k * 4.0 / spq;
    const double r = std::round (pos16);
    if (std::abs (pos16 - r) > 1.0e-6 || (((long long) r) & 1) == 0) return 0;
    const double samplesPer16th = samplesPerStep * spq / 4.0;
    return (int64_t) std::llround (std::clamp ((double) swing, 0.0, 0.75) * 0.5 * samplesPer16th);
}

/**
    0.5: humanise for generated notes: at amount 1 a note starts up to 10 ms late and its velocity
    moves by up to +-16. Deterministic (fixed seed after reset), audio thread, no allocation.
*/
class Humaniser
{
public:
    static constexpr double maxLateSeconds = 0.010;
    static constexpr float maxVelocity = 16.0f;

    void reset() noexcept { rng = Xorshift {}; }

    /** Returns the extra delay in samples and varies `velocity` (1..127) in place. */
    int64_t apply (float amount, double sampleRate, int& velocity) noexcept
    {
        if (amount <= 0.0f) return 0;
        const float late = next01();
        const float vel = next01() * 2.0f - 1.0f;
        velocity = std::clamp ((int) std::lround ((float) velocity + vel * amount * maxVelocity), 1, 127);
        return (int64_t) std::llround (late * amount * maxLateSeconds * sampleRate);
    }

private:
    struct Xorshift
    {
        uint32_t state = 0x9e3779b9u;
    };
    float next01() noexcept
    {
        auto& s = rng.state;
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float) (s >> 8) * (1.0f / 16777216.0f);
    }
    Xorshift rng;
};

/** 0.5: the late "swing" and "human" parameters of the Orbit and Euclid sequencers. */
inline std::vector<ParamDescriptor> swingHumanDescriptors (const std::string& en, const std::string& de)
{
    using namespace pd;
    return { continuous ("swing", en + " Swing", de + " Swing", 0.0f, 0.75f, 0.0f, ParamUnit::Percent),
             continuous ("human", en + " Humanise", de + " Humanisieren", 0.0f, 1.0f, 0.0f, ParamUnit::Percent) };
}

/** Gate length in samples for a step: fraction of the step, at least 16 samples, ends 1 sample early unless tied. */
inline int64_t gateSamples (double samplesPerStep, float gate) noexcept
{
    const double g = std::clamp ((double) gate, 0.05, 1.0);
    return (int64_t) std::llround (std::max (16.0, samplesPerStep * g - (g >= 1.0 ? 0.0 : 1.0)));
}
} // namespace ohpe::seq
