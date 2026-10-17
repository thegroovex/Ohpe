// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p32: pressure glide - "pN.glide.press" lets aftertouch (channel / poly / MPE pressure) scale the glide time.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "engines/AnalogEngine.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct GlideRig
{
    Core core;
    static constexpr int block = 480;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> recL;

    GlideRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        auto& p = core.params();
        p.set ("p1.engine", (float) EngineType::Analog);
        p.set ("p1.voices", 1.0f);
        p.set ("p1.glide", 0.2f);
    }

    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, {});
            recL.insert (recL.end(), l[0].begin(), l[0].end());
        }
    }

    float glidePitch() { return static_cast<AnalogEngine*> (core.part (0).engine (EngineType::Analog))->voiceGlidePitch (0); }
};

MidiEvent channelPressure (int ch, int value)
{
    MidiEvent e;
    e.status = (uint8_t) (0xd0 | (ch - 1));
    e.data1 = (uint8_t) value;
    e.data2 = 0;
    return e;
}

MidiEvent polyPressure (int ch, int note, int value)
{
    MidiEvent e;
    e.status = (uint8_t) (0xa0 | (ch - 1));
    e.data1 = (uint8_t) note;
    e.data2 = (uint8_t) value;
    return e;
}

struct GlideResult
{
    float distance = 0.0f;   ///< semitones still to go 20 ms into a 60 -> 72 legato glide
    float seconds = 0.0f;    ///< effective glide time the part reported
    bool finite = true;
};

/** Note 60, then (pressure, if any) and a legato 72; how far the glide got after two blocks (20 ms). */
GlideResult glide (float amount, int pressure, bool poly = false, bool cc5 = false)
{
    GlideRig rig;
    rig.core.params().set ("p1.glide.press", amount);
    std::vector<MidiEvent> first { MidiEvent::noteOn (1, 60, 100) };
    if (cc5) first.push_back (MidiEvent::controller (1, 5, 64)); // CC 5 overrides the glide time (about 0.5 s)
    rig.run (4, first);
    std::vector<MidiEvent> ev;
    if (pressure >= 0 && ! poly) ev.push_back (channelPressure (1, pressure)); // held from before the new note
    ev.push_back (MidiEvent::noteOn (1, 72, 100));
    if (pressure >= 0 && poly) ev.push_back (polyPressure (1, 72, pressure)); // the new note's own pressure: the glide is running
    rig.run (2, ev);
    GlideResult g;
    g.distance = 72.0f - rig.glidePitch();
    g.seconds = rig.core.part (0).effectiveGlideSeconds();
    g.finite = allFinite (rig.recL) && std::isfinite (rig.glidePitch());
    return g;
}
} // namespace

TEST_CASE ("p32: pressure glide is appended as pN.glide.press (-1..+1, default 0, group partN)")
{
    Core core;
    const auto& reg = core.params();
    for (int n = 1; n <= Core::numParts; ++n)
    {
        const auto id = "p" + std::to_string (n) + ".glide.press";
        CAPTURE (id);
        const int i = reg.indexOf (id);
        REQUIRE (i >= 0);
        const auto& d = reg.descriptor (i);
        CHECK (d.minValue == doctest::Approx (-1.0f));
        CHECK (d.maxValue == doctest::Approx (1.0f));
        CHECK (d.defaultValue == doctest::Approx (0.0f));
        CHECK (d.group == "part" + std::to_string (n));
        CHECK (d.nameEn == "Part " + std::to_string (n) + " Glide Pressure");
        CHECK (d.nameDe == "Part " + std::to_string (n) + " Glide-Druck");
    }
    CHECK (reg.descriptor (reg.size() - 1).id == "p4.glide.press");
}

TEST_CASE ("p32: amount 0 (or no pressure) leaves the glide exactly as before")
{
    const auto plain = glide (0.0f, -1);
    const auto pressed = glide (0.0f, 127);
    const auto amountNoPressure = glide (0.8f, -1);
    CHECK (plain.distance > 1.0f); // it does glide
    CHECK (plain.seconds == doctest::Approx (0.2f));
    CHECK (pressed.distance == plain.distance);
    CHECK (pressed.seconds == plain.seconds);
    CHECK (amountNoPressure.distance == plain.distance);
    CHECK (amountNoPressure.seconds == plain.seconds);
}

TEST_CASE ("p32: a positive amount makes pressing harder glide slower, a negative one faster")
{
    const auto plain = glide (0.0f, -1);
    for (bool poly : { false, true })
    {
        CAPTURE (poly);
        const auto slower = glide (1.0f, 127, poly);
        const auto half = glide (0.5f, 127, poly);
        const auto faster = glide (-0.5f, 127, poly);
        const auto instant = glide (-1.0f, 127, poly);
        CHECK (slower.seconds == doctest::Approx (0.8f));  // x (1 + 3)
        CHECK (half.seconds == doctest::Approx (0.5f));    // x (1 + 1.5)
        CHECK (faster.seconds == doctest::Approx (0.1f));  // x (1 - 0.5)
        CHECK (instant.seconds == doctest::Approx (0.0f)); // full pressure at -1: no glide
        CHECK (slower.distance > plain.distance + 0.3f); // measurably further behind after 20 ms
        CHECK (half.distance > plain.distance + 0.1f);
        CHECK (half.distance < slower.distance);
        CHECK (faster.distance < plain.distance - 0.3f);
        CHECK (instant.distance == doctest::Approx (0.0f));
        for (const auto& g : { slower, half, faster, instant }) CHECK (g.finite);
    }
}

TEST_CASE ("p32: the pressure scales the CC 5 portamento time too, and releasing the pressure restores the time")
{
    const auto cc = glide (0.0f, -1, false, true);
    const auto ccPressed = glide (1.0f, 127, false, true);
    CHECK (ccPressed.seconds == doctest::Approx (4.0f * cc.seconds));
    CHECK (ccPressed.distance > cc.distance);

    GlideRig rig;
    rig.core.params().set ("p1.glide.press", 1.0f);
    rig.run (2, { MidiEvent::noteOn (1, 60, 100), channelPressure (1, 127) });
    CHECK (rig.core.part (0).effectiveGlideSeconds() == doctest::Approx (0.8f));
    rig.run (2, { channelPressure (1, 0) });
    CHECK (rig.core.part (0).effectiveGlideSeconds() == doctest::Approx (0.2f));
    rig.run (2, { MidiEvent::controller (1, 65, 0) }); // portamento off still wins
    CHECK (rig.core.part (0).effectiveGlideSeconds() == 0.0f);
}

TEST_CASE ("p32: sweeping amount and pressure while gliding stays finite and bounded")
{
    GlideRig rig;
    auto& p = rig.core.params();
    int note = 60;
    for (int i = 0; i < 40; ++i)
    {
        p.set ("p1.glide.press", -1.0f + 2.0f * (float) (i % 9) / 8.0f);
        note = note == 60 ? 84 : 60;
        rig.run (1, { channelPressure (1, (i * 37) % 128), MidiEvent::noteOn (1, note, 100), polyPressure (1, note, (i * 53) % 128) });
        const float s = rig.core.part (0).effectiveGlideSeconds();
        CHECK (std::isfinite (s));
        CHECK (s >= 0.0f);
        CHECK (s <= 4.0f * 0.2f + 1.0e-5f);
        CHECK (std::isfinite (rig.glidePitch()));
    }
    CHECK (allFinite (rig.recL));
}

TEST_CASE ("p32: state and part presets keep the amount (preset key \"glide.press\")")
{
    Core a;
    a.params().set ("p2.glide.press", -0.35f);
    const auto json = a.saveState();
    Core b;
    REQUIRE (b.loadState (json));
    CHECK (b.params().get (b.params().indexOf ("p2.glide.press")) == doctest::Approx (-0.35f));

    const auto preset = preset::capturePart (a.params(), 1, {});
    const auto it = preset.params.find ("glide.press");
    REQUIRE (it != preset.params.end());
    CHECK (it->second == doctest::Approx (-0.35f));
    Core c;
    const auto r = preset::applyPart (preset, c.params(), 3);
    CHECK (r.ok);
    CHECK (c.params().get (c.params().indexOf ("p4.glide.press")) == doctest::Approx (-0.35f));
}
