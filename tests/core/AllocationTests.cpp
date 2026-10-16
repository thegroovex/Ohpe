// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Real-time check: counts heap allocations made while Core::process() runs with the tape
// recording and playing, every FX slot type, the mixer dynamics and the master chain on.
// The global operator new of this test executable counts calls while `counting` is set.

#include "OhpeCore.h"
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
        p.set ("p1.layer.engine", (float) (1 + fxType % numEngineTypes)); // 0.8: the layer engine
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
