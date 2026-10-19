// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Arpeggiator.h"

namespace ohpe
{
std::vector<ParamDescriptor> Arpeggiator::getParamDescriptors() const
{
    using namespace pd;
    return {
        toggle ("on", "Arp On", "Arp an", false),
        integer ("part", "Arp Target Part", "Arp Ziel-Part", 1, 4, 3),
        choice ("mode", "Arp Mode", "Arp Modus", { "Up", "Down", "Up/Down", "Random", "As Played", "Chord" },
                { "Aufwärts", "Abwärts", "Auf/Ab", "Zufall", "Wie gespielt", "Akkord" }, Up),
        integer ("oct", "Arp Octaves", "Arp Oktaven", 1, 4, 1),
        continuous ("gate", "Arp Gate", "Arp Gate", 0.05f, 1.0f, 0.5f, ParamUnit::Percent),
        choice ("rate", "Arp Rate", "Arp Raster", seq::rateNames(), seq::rateNames(), seq::R1_16),
        toggle ("latch", "Arp Latch", "Arp Halten", false),
    };
}

void Arpeggiator::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void Arpeggiator::reset() noexcept
{
    queue.reset();
    clock.reset();
    numHeld = 0;
    physical.fill (false);
    numPhysical = 0;
    clearPedals();
    numInputs = 0;
    sampleClock = 0;
    stepCount = 0;
    freeRunning = wasSynced = false;
    freePpq = 0.0;
    position.store (-1);
}

void Arpeggiator::setParam (int i, float v) noexcept
{
    const int iv = (int) std::lround (v);
    switch (i)
    {
        case On: on = v >= 0.5f; break;
        case Part: part = std::clamp (iv, 1, 4); break;
        case Mode: mode = std::clamp (iv, 0, numModes - 1); break;
        case Octaves: octaves = std::clamp (iv, 1, 4); break;
        case Gate: gate = v; break;
        case Rate: rate = std::clamp (iv, 0, seq::numRates - 1); break;
        case Latch:
        {
            const bool l = v >= 0.5f;
            const bool unlatch = latch && ! l && numPhysical == 0;
            latch = l;
            if (unlatch) dropReleased(); // unlatch with no key down: the set ends (p60: pedal-held keys stay)
            break;
        }
        default: break;
    }
}

void Arpeggiator::noteInput (const MidiEvent& e) noexcept
{
    if (numInputs < (int) inputs.size()) inputs[(size_t) numInputs++] = e;
}

void Arpeggiator::releaseAll() noexcept
{
    queue.releaseAll (sampleClock);
    numHeld = 0;
    physical.fill (false);
    numPhysical = 0;
    clearPedals(); // p60: a pedal still down holds nothing of the old set
    numInputs = 0;
    freeRunning = false;
    position.store (-1, std::memory_order_relaxed);
}

void Arpeggiator::clearPedals() noexcept
{
    sustain = sostenuto = false;
    sostLatched.fill (false);
}

bool Arpeggiator::keeps (int note) const noexcept
{
    return latch || sustain || (sostenuto && sostLatched[(size_t) (note & 0x7f)]);
}

void Arpeggiator::dropReleased() noexcept
{
    int k = 0;
    for (int i = 0; i < numHeld; ++i)
    {
        const int n = held[(size_t) i].note;
        if (physical[(size_t) (n & 0x7f)] || keeps (n)) held[(size_t) k++] = held[(size_t) i];
    }
    numHeld = k; // none left: process() ends the arpeggio, the queued note-offs still go out
}

void Arpeggiator::applyInput (const MidiEvent& e) noexcept
{
    if (isPedal (e)) // p60
    {
        const bool down = e.data2 >= 64;
        if (e.data1 == 64)
        {
            if (sustain == down) return;
            sustain = down;
        }
        else
        {
            if (sostenuto == down) return;
            sostenuto = down;
            if (down) sostLatched = physical; // only the keys down now
            else sostLatched.fill (false);
        }
        if (! down) dropReleased();
        return;
    }
    const int note = e.data1 & 0x7f;
    if (e.isNoteOn())
    {
        if (numPhysical == 0 && latch && ! sustain) numHeld = 0; // new set after a latched one (p60: sustain adds)
        if (! physical[(size_t) note]) { physical[(size_t) note] = true; ++numPhysical; }
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i].note == note) { held[(size_t) i].velocity = e.data2; return; }
        if (numHeld == 0) stepCount = 0; // a new arpeggio starts at its first step
        if (numHeld < maxHeld) held[(size_t) numHeld++] = { note, e.data2 };
    }
    else if (e.isNoteOff())
    {
        if (physical[(size_t) note]) { physical[(size_t) note] = false; --numPhysical; }
        if (keeps (note)) return; // latch, p60: sustain / sostenuto
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i].note == note)
            {
                for (int j = i + 1; j < numHeld; ++j) held[(size_t) j - 1] = held[(size_t) j];
                --numHeld;
                break;
            }
    }
}

int Arpeggiator::buildSorted (std::array<Held, maxHeld>& out) const noexcept
{
    for (int i = 0; i < numHeld; ++i) out[(size_t) i] = held[(size_t) i];
    std::sort (out.begin(), out.begin() + numHeld, [] (const Held& a, const Held& b) { return a.note < b.note; });
    return numHeld;
}

void Arpeggiator::playStep (int64_t time, double samplesPerStep) noexcept
{
    if (numHeld <= 0) return;
    const int ch = std::clamp (part, 1, 16);
    const int64_t len = seq::gateSamples (samplesPerStep, gate);
    const int64_t i = stepCount++;
    position.store ((int) (i % 64), std::memory_order_relaxed);

    auto play = [&] (int note, int vel) {
        if (note < 0 || note > 127) return;
        queue.schedule (time, MidiEvent::noteOn (ch, note, std::max (1, vel)));
        queue.schedule (time + len, MidiEvent::noteOff (ch, note));
    };

    std::array<Held, maxHeld> sorted {};
    const int n = mode == Order ? numHeld : buildSorted (sorted);
    const auto& src = mode == Order ? held : sorted;
    const int total = n * octaves; // notes per cycle
    if (mode == Chord)
    {
        const int oct = (int) (i % octaves);
        for (int k = 0; k < n; ++k) play (src[(size_t) k].note + 12 * oct, src[(size_t) k].velocity);
        return;
    }
    int idx = 0;
    switch (mode)
    {
        case Down: idx = total - 1 - (int) (i % total); break;
        case UpDown:
        {
            if (total <= 1) { idx = 0; break; }
            const int period = 2 * total - 2;
            const int j = (int) (i % period);
            idx = j < total ? j : period - j;
            break;
        }
        case Random: idx = (int) (rng.nextU32() % (uint32_t) total); break;
        default: idx = (int) (i % total); break; // Up, Order
    }
    const auto& h = src[(size_t) (idx % n)];
    play (h.note + 12 * (idx / n), h.velocity);
}

void Arpeggiator::process (const Transport& tr, int numSamples, SequencerOutput& out) noexcept
{
    const int64_t blockStart = sampleClock;
    // inputs in time order (stable)
    for (int i = 1; i < numInputs; ++i) // insertion sort: stable, no allocation
    {
        const auto e = inputs[(size_t) i];
        int j = i - 1;
        while (j >= 0 && inputs[(size_t) j].sampleOffset > e.sampleOffset) { inputs[(size_t) j + 1] = inputs[(size_t) j]; --j; }
        inputs[(size_t) j + 1] = e;
    }

    struct StepAt { int offset; double sps; };
    std::array<StepAt, 128> steps {};
    int numSteps = 0;
    auto collect = [&] (long long, int offset, double sps) {
        if (numSteps < (int) steps.size()) steps[(size_t) numSteps++] = { offset, sps };
    };

    if (! on)
    {
        numInputs = 0;
        queue.emit (blockStart, numSamples, out.events);
        sampleClock = blockStart + numSamples;
        return;
    }

    const double bpm = tr.bpm > 0.0 ? tr.bpm : 120.0;
    if (tr.playing)
    {
        if (! wasSynced) clock.reset();
        wasSynced = true;
        freeRunning = false;
        clock.run (tr, seq::stepsPerQuarter (rate), sr, numSamples, collect);
    }
    else
    {
        if (wasSynced) { clock.reset(); wasSynced = false; }
        // free-running grid; starts on the first note when nothing is held
        int startOffset = -1;
        if (! freeRunning || numHeld == 0)
        {
            for (int k = 0; k < numInputs; ++k)
                if (inputs[(size_t) k].isNoteOn()) { startOffset = std::clamp (inputs[(size_t) k].sampleOffset, 0, numSamples - 1); break; }
            if (startOffset >= 0)
            {
                clock.reset();
                freePpq = -startOffset * bpm / 60.0 / sr;
                freeRunning = true;
            }
        }
        if (freeRunning)
        {
            Transport ft;
            ft.bpm = bpm;
            ft.playing = true;
            ft.ppqAtBlockStart = freePpq;
            clock.run (ft, seq::stepsPerQuarter (rate), sr, numSamples, collect);
            freePpq += numSamples * bpm / 60.0 / sr;
        }
    }

    // walk inputs and steps in time order (inputs at the same sample come first)
    int in = 0;
    for (int s = 0; s < numSteps; ++s)
    {
        while (in < numInputs && inputs[(size_t) in].sampleOffset <= steps[(size_t) s].offset)
            applyInput (inputs[(size_t) in++]);
        playStep (blockStart + steps[(size_t) s].offset, steps[(size_t) s].sps);
    }
    while (in < numInputs) applyInput (inputs[(size_t) in++]);
    numInputs = 0;
    if (numHeld == 0)
    {
        freeRunning = false;
        if (! tr.playing) position.store (-1, std::memory_order_relaxed);
    }

    queue.emit (blockStart, numSamples, out.events);
    out.events.sortByTime();
    sampleClock = blockStart + numSamples;
}
} // namespace ohpe
