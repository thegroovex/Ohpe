// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Real-time check: counts heap allocations made while Core::process() runs with the tape
// recording and playing, every FX slot type, the mixer dynamics and the master chain on.
// The global operator new of this test executable counts calls while `counting` is set.

#include "OhpeCore.h"
#include "fx/FxRack.h"
#include "TestHelpers.h"

#include <doctest/doctest.h>

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
std::atomic<bool> counting { false };
std::atomic<int> allocations { 0 };
} // namespace

void* operator new (std::size_t size)
{
    if (counting.load (std::memory_order_relaxed)) allocations.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size) { return operator new (size); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace ohpe;

TEST_CASE ("real-time: no allocation in Core::process with tape, FX slot and dynamics active")
{
    Core core;
    core.prepare (48000.0, 512);
    auto& p = core.params();
    for (auto [id, v] : std::initializer_list<std::pair<const char*, float>> {
             { "tape.tr1.arm", 1.0f }, { "tape.tr1.src", 0.0f }, { "tape.tr2.arm", 1.0f }, { "tape.tr2.src", 1.0f },
             { "tape.play", 1.0f }, { "tape.rec", 1.0f }, { "tape.sat", 0.5f }, { "tape.wow", 0.5f }, { "tape.flutter", 0.5f },
             { "tape.hiss", 0.5f }, { "tape.speed", 1.3f }, { "tape.tr3.out", 2.0f }, { "mix.ch1.comp", 0.7f },
             { "mix.master.comp", 1.0f }, { "mix.master.tapesat", 0.5f }, { "seq.run", 1.0f } })
        p.set (id, v);

    std::vector<float> inL (512, 0.25f), inR (512, -0.25f);
    std::array<std::vector<float>, numOutputPairs> l, r;
    OutputBuffers out;
    for (int o = 0; o < numOutputPairs; ++o)
    {
        l[(size_t) o].assign (512, 0.0f);
        r[(size_t) o].assign (512, 0.0f);
        out.left[(size_t) o] = l[(size_t) o].data();
        out.right[(size_t) o] = r[(size_t) o].data();
    }
    std::vector<MidiEvent> midi { MidiEvent::noteOn (1, 60, 100, 10), MidiEvent::noteOff (1, 60, 400) };

    int total = 0;
    for (int fxType = 0; fxType < InsertSlot::numTypes; ++fxType)
    {
        p.set ("p1.fx.type", (float) fxType);
        p.set ("tape.reverse", fxType % 2 ? 1.0f : 0.0f);
        for (int b = 0; b < 30; ++b)
        {
            counting = true;
            core.setAudioInput (inL.data(), inR.data());
            core.process (out, 512, midi.data(), (int) midi.size(), Transport {});
            counting = false;
            total += allocations.exchange (0);
            core.tape().housekeeping(); // message thread work, allowed to allocate
        }
    }
    CHECK (total == 0);
    CHECK (core.tape().dropouts() == 0);
    p.set ("tape.rec", 0.0f);
    counting = true;
    core.process (out, 512, nullptr, 0, Transport {});
    counting = false;
    CHECK (allocations.exchange (0) == 0);
    CHECK (core.tape().housekeeping()); // the pass becomes one undo step
}

TEST_CASE ("real-time: no allocation in Core::process with FX racks and the modulation matrix active (0.7)")
{
    Core core;
    core.prepare (48000.0, 512);
    auto& p = core.params();
    const int types[6] = { 1, 2, 3, 4, 5, 6 };
    for (int part = 1; part <= 4; ++part)
    {
        const auto pp = "p" + std::to_string (part) + ".";
        for (int s = 0; s < 6; ++s) p.set (FxRack::partSlotPrefix (pp, s) + "type", (float) types[(s + part) % 6]);
        p.set (pp + "fx3.mix", 0.5f);
        for (int k = 1; k <= 16; ++k)
        {
            const auto s = pp + "mod.s" + std::to_string (k) + ".";
            p.set (s + "src", (float) (k % 20 + 1));
            p.set (s + "via", (float) (k % 3 == 0 ? mod::SrcModWheel : 0));
            p.set (s + "dst", (float) (k % 3 == 0 ? mod::DstLevel : (k % 3 == 1 ? mod::DstEngine1 + k % 6 : mod::DstFx1Param1 + k % 12)));
            p.set (s + "amt", 0.3f);
            p.set (s + "curve", (float) (k % 5));
        }
        p.set (pp + "lfo2.sync", 5.0f);
        p.set (pp + "lfo3.shape", 6.0f);
    }
    for (int s = 0; s < 4; ++s) p.set (FxRack::masterSlotPrefix (s) + "type", (float) (s + 1));
    core.prepareEffects();
    std::vector<float> l (512), r (512);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    std::vector<MidiEvent> midi { MidiEvent::noteOn (1, 60, 100, 10), MidiEvent::controller (1, 1, 90, 20), MidiEvent::noteOff (1, 60, 400) };
    Transport t;
    t.playing = true;
    t.hostDriven = true;
    int total = 0;
    for (int b = 0; b < 40; ++b)
    {
        t.ppqAtBlockStart = b * 0.0213;
        counting = true;
        core.process (out, 512, midi.data(), (int) midi.size(), t);
        counting = false;
        total += allocations.exchange (0);
    }
    CHECK (total == 0);
}
