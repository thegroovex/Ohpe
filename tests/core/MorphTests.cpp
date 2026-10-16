// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p16: the morph space - XY pad per part between four complete sounds (corners), across engines.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "mod/ModMatrix.h"
#include "morph/MorphSpace.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct Rig
{
    Core core;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    Stereo rec;
    Rig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        set ("mix.master.limit", 0.0f);
        for (int p = 2; p <= 4; ++p) set ("p" + std::to_string (p) + ".vol", 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE_MESSAGE (core.params().set (id, v), id); }
    float get (const std::string& id) const { return core.params().get (id); }
    void run (int blocks, std::vector<MidiEvent> midi = {}, const std::function<void (int)>& perBlock = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            if (perBlock) perBlock (b);
            Transport t;
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, t);
            rec.l.insert (rec.l.end(), l[0].begin(), l[0].end());
            rec.r.insert (rec.r.end(), r[0].begin(), r[0].end());
        }
    }
    static std::vector<MidiEvent> chord()
    {
        return { MidiEvent::noteOn (1, 48, 100, 0), MidiEvent::noteOn (1, 55, 100, 0), MidiEvent::noteOn (1, 60, 100, 0) };
    }
};

/** Four corners on part 1: A, B Analog (other cutoff / shape: one lane), C FM, D Wavetable. */
void fourSounds (Core& core)
{
    auto& reg = core.params();
    auto& m = core.morph();
    reg.set ("p1.engine", (float) EngineType::Analog);
    reg.set ("p1.analog.cutoff", 600.0f);
    reg.set ("p1.analog.shape", 0.1f);
    reg.set ("p1.amp.a", 0.01f);
    reg.set ("p1.amp.s", 1.0f);
    m.captureCorner (0, MorphSpace::A, "Dark");
    reg.set ("p1.analog.cutoff", 6000.0f);
    reg.set ("p1.analog.shape", 0.8f);
    m.captureCorner (0, MorphSpace::B, "Bright");
    reg.set ("p1.engine", (float) EngineType::FM);
    m.captureCorner (0, MorphSpace::C, "Bell");
    reg.set ("p1.engine", (float) EngineType::Wavetable);
    m.captureCorner (0, MorphSpace::D, "Table");
    reg.set ("p1.engine", (float) EngineType::Analog);
}

std::vector<float> windowRms (const std::vector<float>& v, size_t from, size_t win)
{
    std::vector<float> out;
    for (size_t p = from; p + win <= v.size(); p += win) out.push_back (rms (v, p, p + win));
    return out;
}

float maxStep (const std::vector<float>& v, size_t from)
{
    float m = 0.0f;
    for (size_t i = std::max<size_t> (from, 1); i < v.size(); ++i) m = std::max (m, std::abs (v[i] - v[i - 1]));
    return m;
}
} // namespace

TEST_CASE ("p16 morph: parameters appended after the wheels; matrix sources and targets")
{
    Core core;
    const auto& reg = core.params();
    CHECK (reg.indexOf ("perf.mod") + 1 == reg.indexOf ("p1.morph.on"));
    const char* names[] = { "on", "x", "y", "glide", "xsrc", "ysrc" };
    int expect = reg.indexOf ("p1.morph.on");
    for (int p = 1; p <= 4; ++p)
        for (const char* n : names) CHECK (reg.indexOf ("p" + std::to_string (p) + ".morph." + n) == expect++);
    CHECK (reg.get ("p1.morph.on") == 0.0f);
    CHECK (reg.get ("p1.morph.x") == doctest::Approx (0.5f));
    CHECK (mod::sourceIndexOfKey ("morphx") == mod::SrcMorphX);
    CHECK (mod::sourceIndexOfKey ("morphy") == mod::SrcMorphY);
    CHECK (mod::destIndexOfKey ("morphx") == mod::DstMorphX);
    CHECK (core.part (0).destParamIndex (mod::DstMorphX) == reg.indexOf ("p1.morph.x"));
    CHECK (core.part (3).destParamIndex (mod::DstMorphY) == reg.indexOf ("p4.morph.y"));
    // corners keep the sound, not the morph block or the MIDI routing
    CHECK (MorphSpace::isExcluded ("morph.x"));
    CHECK (MorphSpace::isExcluded ("midich"));
    CHECK_FALSE (MorphSpace::isExcluded ("analog.cutoff"));
    CHECK_FALSE (MorphSpace::isExcluded ("mpe.pressamt"));
    const auto w = MorphSpace::weights (0.3f, 0.8f);
    CHECK (w[0] + w[1] + w[2] + w[3] == doctest::Approx (1.0f));
    CHECK (MorphSpace::weights (0.0f, 1.0f)[MorphSpace::A] == 1.0f);
    CHECK (MorphSpace::weights (1.0f, 0.0f)[MorphSpace::D] == 1.0f);
}

TEST_CASE ("p16 morph: lanes - same engine interpolates, other engines play in parallel lanes")
{
    Rig rig;
    fourSounds (rig.core);
    auto& m = rig.core.morph();
    CHECK (m.numLanes (0) == 3);
    CHECK (m.laneOfCorner (0, MorphSpace::A) == m.laneOfCorner (0, MorphSpace::B));
    CHECK (m.laneOfCorner (0, MorphSpace::C) != m.laneOfCorner (0, MorphSpace::A));
    CHECK (m.laneOfCorner (0, MorphSpace::D) != m.laneOfCorner (0, MorphSpace::C));
    // two corners of one engine only: one lane, the cutoff morphs in normalised units
    Rig two;
    auto& reg = two.core.params();
    reg.set ("p1.analog.cutoff", 200.0f);
    two.core.morph().captureCorner (0, MorphSpace::A, "a");
    two.core.morph().captureCorner (0, MorphSpace::C, "c");
    reg.set ("p1.analog.cutoff", 8000.0f);
    two.core.morph().captureCorner (0, MorphSpace::B, "b");
    two.core.morph().captureCorner (0, MorphSpace::D, "d");
    CHECK (two.core.morph().numLanes (0) == 1);
    two.set ("p1.morph.on", 1.0f);
    two.set ("p1.morph.glide", 0.0f);
    two.set ("p1.morph.x", 0.5f);
    two.set ("p1.morph.y", 0.5f);
    two.run (4, Rig::chord());
    const int ci = reg.indexOf ("p1.analog.cutoff");
    const auto& d = reg.descriptor (ci);
    const float expect = d.fromNormalised (0.5f * (d.toNormalised (200.0f) + d.toNormalised (8000.0f)));
    CHECK (two.core.morph().laneValue (0, MorphSpace::A, ci) == doctest::Approx (expect).epsilon (1e-4));
    CHECK (reg.get (ci) == 8000.0f); // the part's own value stays (the morph lies over it)
}

TEST_CASE ("p16 morph: a corner is its snapshot, exactly")
{
    Rig rig;
    fourSounds (rig.core);
    auto& m = rig.core.morph();
    const auto& reg = rig.core.params();
    rig.set ("p1.morph.on", 1.0f);
    rig.set ("p1.morph.glide", 0.0f);
    const std::pair<float, float> pos[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
    for (int c = 0; c < MorphSpace::numCorners; ++c)
    {
        CAPTURE (c);
        rig.set ("p1.morph.x", pos[c].first);
        rig.set ("p1.morph.y", pos[c].second);
        rig.run (3, c == 0 ? Rig::chord() : std::vector<MidiEvent> {});
        const auto params = m.cornerParams (0, c);
        REQUIRE (! params.empty());
        int mismatches = 0;
        for (const auto& [local, v] : params)
        {
            const int idx = reg.indexOf ("p1." + local);
            REQUIRE (idx >= 0);
            if (m.laneValue (0, c, idx) != v) ++mismatches;
        }
        CHECK (mismatches == 0);
        CHECK (m.uiX (0) == pos[c].first);
        CHECK (m.uiY (0) == pos[c].second);
    }
}

TEST_CASE ("p16 morph: state and part presets keep the corners")
{
    Core a;
    fourSounds (a);
    a.params().set ("p1.morph.on", 1.0f);
    a.params().set ("p1.morph.x", 0.25f);
    const auto state = a.saveState();
    Core b;
    REQUIRE (b.loadState (state));
    CHECK (b.params().get ("p1.morph.x") == doctest::Approx (0.25f));
    for (int c = 0; c < MorphSpace::numCorners; ++c)
    {
        CHECK (b.morph().cornerSet (0, c));
        CHECK (b.morph().cornerLabel (0, c) == a.morph().cornerLabel (0, c));
        CHECK (b.morph().cornerParams (0, c) == a.morph().cornerParams (0, c));
    }
    CHECK (b.morph().numLanes (0) == a.morph().numLanes (0));
    CHECK_FALSE (b.morph().anyCorner (1));

    // part preset: the "morph" block travels with the sound, into another part
    auto p = preset::capturePart (a.params(), 0, {});
    preset::captureMorph (a, 0, p);
    REQUIRE (p.morph.has_value());
    preset::Preset back;
    REQUIRE (preset::fromJson (preset::toJson (p), back));
    REQUIRE (back.morph.has_value());
    Core c;
    REQUIRE (preset::applyPart (back, c.params(), 2).ok);
    preset::applyPartMorph (back, c, 2);
    for (int k = 0; k < MorphSpace::numCorners; ++k) CHECK (c.morph().cornerParams (2, k) == a.morph().cornerParams (0, k));
    // a preset without corners clears them
    auto plain = preset::capturePart (c.params(), 1, {});
    preset::captureMorph (c, 1, plain);
    CHECK_FALSE (plain.morph.has_value());
    preset::applyPartMorph (plain, c, 2);
    CHECK_FALSE (c.morph().anyCorner (2));
    // a state without the section clears all corners
    Core fresh;
    REQUIRE (b.loadState (fresh.saveState()));
    CHECK_FALSE (b.morph().anyCorner (0));
}

TEST_CASE ("p16 morph: sweeping the pad across engines - finite, even level, no jumps")
{
    // reference: every corner on its own
    std::array<float, MorphSpace::numCorners> cornerRms {};
    float cornerStep = 0.0f;
    const std::pair<float, float> pos[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
    for (int c = 0; c < MorphSpace::numCorners; ++c)
    {
        Rig rig;
        fourSounds (rig.core);
        rig.set ("p1.morph.on", 1.0f);
        rig.set ("p1.morph.x", pos[c].first);
        rig.set ("p1.morph.y", pos[c].second);
        rig.run (120, Rig::chord());
        const size_t from = rig.rec.size() / 2;
        cornerRms[(size_t) c] = rms (rig.rec.l, from);
        cornerStep = std::max (cornerStep, maxStep (rig.rec.l, from));
    }
    const float lo = *std::min_element (cornerRms.begin(), cornerRms.end());
    const float hi = *std::max_element (cornerRms.begin(), cornerRms.end());
    REQUIRE (lo > 1.0e-4f);

    // a loop around the pad and through the middle, glided, with the chord held
    Rig rig;
    fourSounds (rig.core);
    rig.set ("p1.morph.on", 1.0f);
    rig.set ("p1.morph.glide", 0.05f);
    rig.set ("p1.morph.x", 0.0f);
    rig.set ("p1.morph.y", 1.0f);
    const int blocks = 900; // 4.8 s
    rig.run (blocks, Rig::chord(), [&] (int b) {
        const float t = (float) b / (float) blocks;
        const float x = 0.5f + 0.5f * std::sin (6.2831853f * 2.0f * t);
        const float y = 0.5f + 0.5f * std::cos (6.2831853f * 3.0f * t);
        rig.core.params().set ("p1.morph.x", x);
        rig.core.params().set ("p1.morph.y", y);
    });
    CHECK (allFinite (rig.rec.l));
    CHECK (allFinite (rig.rec.r));
    const size_t from = (size_t) (0.3 * sampleRate);
    const auto w = windowRms (rig.rec.l, from, (size_t) (0.05 * sampleRate));
    const float wMax = *std::max_element (w.begin(), w.end());
    const float wMin = *std::min_element (w.begin(), w.end());
    CHECK (wMax <= hi * 2.0f);  // at most +6 dB above the loudest corner
    CHECK (wMin >= lo * 0.25f); // never a hole
    for (size_t i = 1; i < w.size(); ++i)
    {
        CAPTURE (i);
        CHECK (w[i] <= w[i - 1] * 2.0f); // no level jump between 50 ms windows
        CHECK (w[i] >= w[i - 1] * 0.5f);
    }
    CHECK (maxStep (rig.rec.l, from) <= cornerStep * 3.0f + 0.01f); // no click
    CHECK (rig.core.morph().uiLanesAwake (0) >= 1);
}

TEST_CASE ("p16 morph: the matrix moves the pad and reads its position; off = the part's own sound")
{
    Rig rig;
    fourSounds (rig.core);
    rig.set ("p1.morph.on", 1.0f);
    rig.set ("p1.morph.glide", 0.0f);
    rig.set ("p1.morph.x", 0.0f);
    rig.set ("p1.morph.y", 0.0f);
    rig.set ("p1.mod.s1.src", (float) mod::SrcMacro1);
    rig.set ("p1.mod.s1.dst", (float) mod::DstMorphX);
    rig.set ("p1.mod.s1.amt", 0.5f);
    rig.set ("p1.macro1", 1.0f);
    rig.run (60, Rig::chord());
    CHECK (rig.core.morph().uiX (0) == doctest::Approx (0.5f).epsilon (0.02));
    CHECK (rig.core.part (0).modMatrix().sourceValue (mod::SrcMorphX) == doctest::Approx (rig.core.morph().uiX (0)));
    CHECK (rig.core.part (0).modMatrix().sourceValue (mod::SrcMorphY) == doctest::Approx (0.0f));
    CHECK (rig.get ("p1.morph.x") == 0.0f); // the parameter keeps its own value
    // switched off: the part plays its own parameters again, the lanes fade out and sleep
    rig.set ("p1.morph.on", 0.0f);
    rig.run (60);
    CHECK_FALSE (rig.core.morph().uiActive (0));
    CHECK (rig.core.morph().uiLanesAwake (0) == 1);
    CHECK (allFinite (rig.rec.l));
}
