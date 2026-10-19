// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p53: a held note never starts again while the pitch bend moves or is held at one position - from a controller's
// bend (repeated messages of the same value, a touch strip held still), the on-screen wheel, in normal, MPE and
// fingerboard (pressure onset, HD) parts, with glide off and on. A restart would show as a block that falls back to
// the start of the slow attack.

#include "OhpeCore.h"
#include "TestHelpers.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr int block = 512;

MidiEvent bendEvent (int ch, int value)
{
    MidiEvent e;
    e.status = (uint8_t) (0xe0 | ((ch - 1) & 0x0f));
    e.data1 = (uint8_t) (value & 0x7f);
    e.data2 = (uint8_t) ((value >> 7) & 0x7f);
    return e;
}

/** Block RMS (left) of part 1 for `blocks` blocks; `midiAt (b)` = the block's MIDI, `before (b)` runs before it. */
std::vector<double> rmsBlocks (Core& core, int blocks, const std::function<std::vector<MidiEvent> (int)>& midiAt,
                               const std::function<void (int)>& before = {})
{
    std::vector<double> rms;
    std::vector<float> l (block), r (block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    for (int b = 0; b < blocks; ++b)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        if (before) before (b);
        auto m = midiAt (b);
        core.process (out, block, m.empty() ? nullptr : m.data(), (int) m.size(), Transport {});
        double s = 0.0;
        for (float x : l) s += (double) x * x;
        rms.push_back (std::sqrt (s / block));
        REQUIRE (allFinite (l));
    }
    return rms;
}

void patch (Core& core, bool mpe, bool fingerboard, int glideMode)
{
    core.prepare (48000.0, block);
    for (const auto& [id, v] : std::vector<std::pair<std::string, float>> {
             { "mix.master.limit", 0.0f }, { "p1.engine", (float) EngineType::Analog }, { "p1.amp.a", 0.4f }, { "p1.amp.s", 1.0f },
             { "p1.voices", 4.0f }, { "p1.midich", 0.0f }, { "p1.velsens", 0.0f }, { "p1.analog.cutoff", 3000.0f },
             { "p1.mpe", mpe ? 1.0f : 0.0f }, { "p1.glide", glideMode > 0 ? 0.2f : 0.0f } })
        REQUIRE_MESSAGE (core.params().set (id, v), id);
    if (glideMode > 0) core.params().set ("p1.glide.mode", (float) glideMode);
    if (fingerboard)
    {
        core.params().set ("p1.mpe.onset", 1.0f);
        core.params().set ("p1.mpe.bend", 96.0f);
        core.params().set ("p1.mpe.hd", 1.0f);
    }
}

/** After the attack (1 s): no block may fall back towards silence (a restarted attack starts at 0). */
void checkNoRestart (const std::vector<double>& rms, int from)
{
    double steady = 0.0;
    for (int b = from - 8; b < from; ++b) steady += rms[(size_t) b] / 8.0;
    REQUIRE (steady > 1.0e-3);
    for (size_t b = (size_t) from; b < rms.size(); ++b)
        CHECK_MESSAGE (rms[b] > 0.6 * steady, "block " << b << " rms " << rms[b] << " steady " << steady);
}
} // namespace

TEST_CASE ("p53 a held note + repeated / held pitch bend never retriggers (normal, MPE, fingerboard; glide off / on)")
{
    const int settle = 100; // ~1.07 s: the 0.4 s attack is done
    for (int variant = 0; variant < 3; ++variant)
        for (int glide : { 0, 1, 2 })
        {
            CAPTURE (variant);
            CAPTURE (glide);
            Core core;
            const bool mpe = variant > 0, fb = variant == 2;
            patch (core, mpe, fb, glide);
            const int ch = mpe ? 2 : 1; // MPE: the note on a member channel
            auto rms = rmsBlocks (core, settle + 160, [&] (int b) {
                std::vector<MidiEvent> m;
                if (b == 0)
                {
                    if (fb) m.push_back (MidiEvent { 0, (uint8_t) (0xd0 | (ch - 1)), 100, 0 }); // pressure: the level
                    m.push_back (MidiEvent::noteOn (ch, 60, 100, 0));
                    if (fb) m.push_back (MidiEvent { 1, (uint8_t) (0xd0 | (ch - 1)), 100, 0 });
                }
                if (b >= settle)
                {
                    // a strip / wheel held still: the same value again and again (several per block), then moving
                    const int v = b < settle + 80 ? 10000 : 8192 + (int) (3000.0 * std::sin ((double) b * 0.3));
                    for (int k = 0; k < 4; ++k)
                    {
                        auto e = bendEvent (ch, v);
                        e.sampleOffset = k * 128;
                        m.push_back (e);
                        if (mpe) // the master channel's bend as well
                        {
                            auto mb = bendEvent (1, v);
                            mb.sampleOffset = k * 128;
                            m.push_back (mb);
                        }
                    }
                }
                return m;
            });
            checkNoRestart (rms, settle);
        }
}

TEST_CASE ("p53 the on-screen pitch wheel held at one position never retriggers a held note")
{
    Core core;
    patch (core, false, false, 0);
    const int settle = 100;
    auto rms = rmsBlocks (
        core, settle + 120,
        [] (int b) { return b == 0 ? std::vector<MidiEvent> { MidiEvent::noteOn (1, 60, 100, 0) } : std::vector<MidiEvent> {}; },
        [&core, settle] (int b) {
            if (b >= settle) core.params().set (Core::Ids::perfBend, b < settle + 60 ? 0.5f : 0.5f + 0.001f * (float) (b % 7));
        });
    checkNoRestart (rms, settle);
}
