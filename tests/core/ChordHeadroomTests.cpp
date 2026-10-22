// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p74: chord headroom - the voice-load-aware gain of a part (pN.chordroom), right after the engines.

#include "OhpeCore.h"
#include "TestHelpers.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

using namespace ohpe;

namespace
{
/** Plays `notes` on part 1 (held for `seconds`), master limiter off; returns the left channel. */
std::vector<float> play (float roomDb, const std::vector<int>& notes, double seconds, int velocity = 127, int block = 256,
                         double releaseAfter = -1.0)
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    core.prepare (48000.0, block);
    core.params().set ("mix.master.limit", 0.0f);
    core.params().set ("p1.chordroom", roomDb);
    const auto total = (size_t) (seconds * 48000.0);
    std::vector<float> l (total), r (total);
    std::vector<MidiEvent> ev;
    for (size_t pos = 0; pos < total; pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, total - pos);
        ev.clear();
        if (pos == 0)
            for (int k : notes) ev.push_back (MidiEvent::noteOn (1, k, velocity, 0));
        if (releaseAfter >= 0.0 && pos == (size_t) (releaseAfter * 48000.0) / (size_t) block * (size_t) block)
            for (int k : notes) ev.push_back (MidiEvent::noteOff (1, k, 0));
        OutputBuffers out;
        out.left[0] = l.data() + pos;
        out.right[0] = r.data() + pos;
        core.process (out, n, ev.empty() ? nullptr : ev.data(), (int) ev.size(), Transport {});
    }
    return l;
}

double db (double x) { return 20.0 * std::log10 (std::max (1.0e-12, x)); }
} // namespace

TEST_CASE ("p74 chord headroom: parameters appended after Key Gravity, 0..24 dB, default 4.5 dB, EN / DE")
{
    auto owner = std::make_unique<Core>();
    const auto& reg = owner->params();
    int expect = reg.indexOf ("p4.grav.quality") + 1;
    REQUIRE (expect > 0);
    for (int p = 1; p <= Core::numParts; ++p)
    {
        const auto id = "p" + std::to_string (p) + ".chordroom";
        CHECK_MESSAGE (reg.indexOf (id) == expect++, id);
    }
    const auto& d = reg.descriptor (reg.indexOf ("p2.chordroom"));
    CHECK (d.minValue == doctest::Approx (0.0f));
    CHECK (d.maxValue == doctest::Approx (24.0f));
    CHECK (reg.get ("p1.chordroom") == doctest::Approx (Part::defaultChordHeadroomDb));
    CHECK (d.nameEn == "Part 2 Chord Headroom");
    CHECK (d.nameDe == "Part 2 Akkord-Reserve");
}

TEST_CASE ("p74 chord headroom: a single note is bit-identical with and without it")
{
    const auto a = play (0.0f, { 60 }, 1.0);
    const auto b = play (12.0f, { 60 }, 1.0);
    REQUIRE (test::peak (a) > 1.0e-3f);
    CHECK (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0);
}

TEST_CASE ("p74 chord headroom: 8 notes take the full amount, 4 notes two thirds, 2 notes a third; output clean")
{
    const std::vector<int> eight { 36, 43, 48, 52, 55, 60, 64, 67 };
    const size_t from = 24000, to = 48000; // the held part
    for (int count : { 2, 4, 8 })
    {
        CAPTURE (count);
        const std::vector<int> notes (eight.begin(), eight.begin() + count);
        const auto off = play (0.0f, notes, 1.0);
        const auto on = play (6.0f, notes, 1.0);
        const double d = db (test::rms (on, from, to)) - db (test::rms (off, from, to));
        CHECK (d == doctest::Approx (-6.0 * std::log2 ((double) count) / 3.0).epsilon (0.05));
        CHECK (test::allFinite (on));
        CHECK (test::noDenormals (on));
    }
}

TEST_CASE ("p74 chord headroom: settles at the amount and comes back once the notes have faded")
{
    const auto on = play (12.0f, { 48, 52, 55, 60, 64, 67 }, 4.0, 127, 64, 0.5); // small blocks, release after 0.5 s
    CHECK (test::allFinite (on));
    CHECK (test::noDenormals (on));

    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    core.prepare (48000.0, 256);
    core.params().set ("p1.chordroom", 12.0f);
    std::vector<float> l (256), r (256);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    std::vector<MidiEvent> ev;
    for (int k : { 48, 52, 55, 60, 64, 67, 72, 76 }) ev.push_back (MidiEvent::noteOn (1, k, 100, 0));
    core.process (out, 256, ev.data(), (int) ev.size(), Transport {});
    for (int b = 0; b < 40; ++b) core.process (out, 256, nullptr, 0, Transport {});
    CHECK (db (core.part (0).chordHeadroomGain()) == doctest::Approx (-12.0).epsilon (0.05));
    ev.clear();
    for (int k : { 48, 52, 55, 60, 64, 67, 72, 76 }) ev.push_back (MidiEvent::noteOff (1, k, 0));
    core.process (out, 256, ev.data(), (int) ev.size(), Transport {});
    for (int b = 0; b < 3 * 48000 / 256; ++b) core.process (out, 256, nullptr, 0, Transport {});
    CHECK (core.part (0).chordHeadroomGain() > 0.95f); // the tails faded out of the count, the gain is back
}
