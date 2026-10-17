// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p33: expressive playing - the per-part Expression page (velocity / aftertouch -> two functions each). Every slot is a
// routing of the part's modulation matrix: amount 0 is neutral (bit-identical), the functions resolve on every engine,
// velocity / channel pressure / poly pressure move what they should, aftertouch -> glide time lengthens the glide, the
// output stays finite and bounded, and the page is part of the state.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "mod/Expression.h"
#include "mod/ModMatrix.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
namespace ex = mod::expr;

/** Renders part 1 for `blocks` blocks of 512 samples; `midiAt (block)` gives the MIDI of that block. */
Stereo renderMidi (Core& core, int blocks, const std::function<std::vector<MidiEvent> (int)>& midiAt = {})
{
    Stereo s ((size_t) blocks * 512);
    std::vector<float> l (512), r (512);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    for (int b = 0; b < blocks; ++b)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        std::vector<MidiEvent> m;
        if (midiAt) m = midiAt (b);
        core.process (out, 512, m.empty() ? nullptr : m.data(), (int) m.size(), Transport {});
        std::copy (l.begin(), l.end(), s.l.begin() + (ptrdiff_t) b * 512);
        std::copy (r.begin(), r.end(), s.r.begin() + (ptrdiff_t) b * 512);
    }
    return s;
}

void setAll (Core& core, std::initializer_list<std::pair<std::string, float>> values)
{
    for (const auto& [id, v] : values) REQUIRE_MESSAGE (core.params().set (id, v), id);
}

/** A held saw on part 1 (analog, sustain 1), limiter off, part 1 listening on every channel, velocity sens. off. */
void basicPatch (Core& core)
{
    setAll (core, { { "mix.master.limit", 0.0f }, { "p1.engine", (float) EngineType::Analog }, { "p1.amp.s", 1.0f }, { "p1.voices", 4.0f },
                    { "p1.midich", 0.0f }, { "p1.velsens", 0.0f }, { "p1.mod.amt", 0.0f }, { "p1.analog.cutoff", 400.0f },
                    { "p1.mpe.press", 0.0f } });
}

std::string slot (int input, int k, const char* field) { return "p1." + ex::slotSuffix (input, k, field); }

double brightnessOf (const Stereo& x, size_t from)
{
    double d = 0.0, e = 1.0e-12;
    for (size_t i = std::max<size_t> (from, 1); i < x.size(); ++i)
    {
        const double dx = (double) x.l[i] - (double) x.l[i - 1];
        d += dx * dx;
        e += (double) x.l[i] * (double) x.l[i];
    }
    return d / e;
}

bool identical (const Stereo& a, const Stereo& b) { return a.l == b.l && a.r == b.r; }

MidiEvent channelPressure (int value) { return MidiEvent { 0, (uint8_t) 0xd0, (uint8_t) value, 0 }; }
MidiEvent polyPressure (int note, int value) { return MidiEvent { 0, (uint8_t) 0xa0, (uint8_t) note, (uint8_t) value }; }
} // namespace

TEST_CASE ("p33: the Expression page IDs follow the pressure-glide block (p32), 12 per part in their documented order")
{
    Core core;
    std::vector<std::string> ids;
    for (const auto& d : core.params().all()) ids.push_back (d.id);
    const auto it = std::find (ids.begin(), ids.end(), "p1.expr.vel.dst1");
    REQUIRE (it != ids.end());
    const auto first = (size_t) (it - ids.begin());
    CHECK ((int) first == core.params().indexOf ("p4.glide.press") + 1);
    size_t k = first;
    for (int p = 1; p <= Core::numParts; ++p)
        for (const char* in : { "vel", "at" })
            for (int s = 1; s <= 2; ++s)
                for (const char* f : { "dst", "amt", "curve" })
                {
                    REQUIRE (k < ids.size());
                    CHECK (ids[k++] == "p" + std::to_string (p) + ".expr." + in + "." + f + std::to_string (s));
                }
    // the golden file lists them (append-only)
    std::ifstream in (std::string (OHPE_TEST_DATA_DIR) + "/golden/param_ids_v1.txt");
    std::vector<std::string> golden;
    for (std::string line; std::getline (in, line);)
        if (! line.empty() && line[0] != '#') golden.push_back (line);
    CHECK (std::find (golden.begin(), golden.end(), "p4.expr.at.curve2") != golden.end());
    // choices, defaults (amount 0 = neutral)
    const auto& reg = core.params();
    const auto& dst = reg.descriptor (reg.indexOf ("p1.expr.vel.dst1"));
    CHECK (dst.choices.size() == (size_t) ex::numTargets);
    CHECK (dst.defaultValue == (float) ex::Cutoff);
    CHECK (reg.descriptor (reg.indexOf ("p1.expr.at.dst1")).defaultValue == (float) ex::Vibrato);
    for (int p = 1; p <= Core::numParts; ++p)
        for (const char* in : { "vel", "at" })
            for (int s = 1; s <= 2; ++s)
                CHECK (reg.descriptor (reg.indexOf ("p" + std::to_string (p) + ".expr." + in + ".amt" + std::to_string (s))).defaultValue == 0.0f);
    // the new matrix entries
    CHECK (mod::sourceTable().size() == (size_t) mod::numSources);
    CHECK (mod::destTable().size() == (size_t) mod::numDests);
    CHECK (mod::sourceIndexOfKey ("vibrato") == mod::SrcVibrato);
    CHECK (mod::destIndexOfKey ("glide") == mod::DstGlide);
    CHECK (mod::destIndexOfKey ("ampattack") == mod::DstAmpAttack);
    CHECK (core.part (0).destParamIndex (mod::DstGlide) == reg.indexOf ("p1.glide"));
    CHECK (core.part (2).destParamIndex (mod::DstAmpRelease) == reg.indexOf ("p3.amp.r"));
    for (int t = 0; t < ex::numTargets; ++t) CHECK (ex::targetIndexOfKey (ex::targets()[(size_t) t].key) == t);
}

TEST_CASE ("p33: every function resolves on every engine (or is inert there); Cutoff, Glide, Vibrato everywhere")
{
    Core core;
    const auto& reg = core.params();
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        if (core.part (0).engine (type) == nullptr) continue;
        CAPTURE (engineTypeInfo (type).id);
        CHECK (core.part (0).expressionTarget (type, ex::Off) == -1);
        CHECK (core.part (0).expressionTarget (type, ex::Cutoff) > mod::DstOff);
        CHECK (core.part (0).expressionTarget (type, ex::Glide) == mod::DstGlide);
        CHECK (core.part (0).expressionTarget (type, ex::Vibrato) == mod::DstPitch);
        CHECK (core.part (0).expressionTarget (type, ex::Level) == mod::DstLevel);
        CHECK (core.part (0).expressionTarget (type, ex::Drive) > mod::DstOff);
        for (int f = 1; f < ex::numTargets; ++f)
        {
            const int d = core.part (0).expressionTarget (type, f);
            CHECK (d >= -1);
            CHECK (d < mod::numDests);
        }
    }
    // engine functions name the engine's own parameter
    const auto engineParam = [&] (EngineType type, int f) {
        const int d = core.part (0).expressionTarget (type, f);
        REQUIRE (d >= mod::DstEngine1);
        REQUIRE (d < mod::DstEngine1 + mod::numEngineDests);
        return core.part (0).engine (type)->getParamDescriptors()[(size_t) (d - mod::DstEngine1)].id;
    };
    CHECK (engineParam (EngineType::Analog, ex::Cutoff) == "cutoff");
    CHECK (engineParam (EngineType::Analog, ex::Resonance) == "reso");
    CHECK (engineParam (EngineType::FM, ex::FmAmount) == "index");
    CHECK (engineParam (EngineType::Wavetable, ex::FmAmount) == "fm");
    CHECK (engineParam (EngineType::Wavetable, ex::Position) == "pos");
    CHECK (engineParam (EngineType::Grain, ex::Spray) == "spray");
    CHECK (core.part (0).expressionTarget (EngineType::FM, ex::Cutoff) == mod::DstCharBright); // no filter: BRIGHT
    (void) reg;
}

TEST_CASE ("p33: amount 0 on every slot leaves the sound bit-identical, whatever the functions and curves")
{
    auto render = [] (bool configured) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        if (configured)
            setAll (core, { { slot (0, 0, "dst"), (float) ex::Glide }, { slot (0, 1, "dst"), (float) ex::Level }, { slot (1, 0, "dst"), (float) ex::Pitch },
                            { slot (1, 1, "dst"), (float) ex::Drive }, { slot (0, 0, "curve"), (float) mod::CurveExp }, { slot (1, 1, "curve"), (float) mod::CurveS } });
        return renderMidi (core, 24, [] (int b) {
            std::vector<MidiEvent> m;
            if (b == 0) m.push_back (MidiEvent::noteOn (1, 60, 100, 0));
            if (b == 4) m.push_back (channelPressure (120));
            if (b == 8) m.push_back (polyPressure (60, 40));
            if (b == 12) m.push_back (MidiEvent::noteOn (1, 64, 30, 0));
            return m;
        });
    };
    CHECK (identical (render (true), render (false)));
}

TEST_CASE ("p33: velocity -> Cutoff opens the filter for hard notes; a negative amount closes it")
{
    auto brightness = [] (float velocity, float amount) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        setAll (core, { { slot (0, 0, "dst"), (float) ex::Cutoff }, { slot (0, 0, "amt"), amount } });
        core.queueNote (48, velocity, true);
        return brightnessOf (renderMidi (core, 20), 6000);
    };
    const double hard = brightness (1.0f, 0.6f), soft = brightness (0.1f, 0.6f);
    CAPTURE (hard);
    CAPTURE (soft);
    CHECK (hard > 2.0 * soft);
    CHECK (brightness (1.0f, -0.6f) < brightness (1.0f, 0.0f));
}

TEST_CASE ("p33: aftertouch -> Cutoff follows channel pressure and poly pressure")
{
    auto brightness = [] (const std::vector<MidiEvent>& pressure) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        setAll (core, { { slot (1, 1, "dst"), (float) ex::Cutoff }, { slot (1, 1, "amt"), 0.6f } });
        return brightnessOf (renderMidi (core, 24, [&] (int b) {
                                 std::vector<MidiEvent> m;
                                 if (b == 0) m.push_back (MidiEvent::noteOn (1, 48, 100, 0));
                                 if (b == 2) m = pressure;
                                 return m;
                             }),
                             8000);
    };
    const double none = brightness ({}), channel = brightness ({ channelPressure (127) }), poly = brightness ({ polyPressure (48, 127) });
    CAPTURE (none);
    CAPTURE (channel);
    CAPTURE (poly);
    CHECK (channel > 2.0 * none);
    CHECK (poly > 2.0 * none);
}

TEST_CASE ("p33: aftertouch -> Vibrato wobbles the pitch at about 5.5 Hz only while pressed")
{
    auto spread = [] (int pressure) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        setAll (core, { { "p1.analog.cutoff", 3000.0f }, { "p1.analog.keytrack", 0.0f }, { "p1.analog.detune", 0.0f }, { "p1.analog.oscmix", 0.0f },
                        { "p1.analog.drive", 0.0f }, { "p1.analog.shape", 0.0f }, { slot (1, 0, "dst"), (float) ex::Vibrato }, { slot (1, 0, "amt"), 1.0f } });
        const auto s = renderMidi (core, 60, [&] (int b) {
            std::vector<MidiEvent> m;
            if (b == 0) m.push_back (MidiEvent::noteOn (1, 69, 100, 0));
            if (b == 1 && pressure > 0) m.push_back (channelPressure (pressure));
            return m;
        });
        double lo = 1.0e9, hi = 0.0;
        for (size_t from = 4000; from + 2400 < s.size(); from += 1200) // 50 ms windows over ~0.5 s
        {
            const double f = measureFrequency (s.l, sampleRate, from, from + 2400);
            lo = std::min (lo, f);
            hi = std::max (hi, f);
        }
        return centsBetween (hi, lo);
    };
    const double still = spread (0), pressed = spread (127);
    CAPTURE (still);
    CAPTURE (pressed);
    CHECK (still < 5.0);
    CHECK (pressed > 60.0);  // +-1 semitone at 100 %, averaged over the windows
    CHECK (pressed < 260.0);
}

TEST_CASE ("p33: aftertouch -> Glide Time - more pressure, longer glide; a CC 5 portamento time survives the modulation")
{
    // two legato notes an octave apart, glide 50 ms: 150 ms after the second note, how far is the pitch still away?
    auto lag = [] (int pressure) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        setAll (core, { { "p1.analog.cutoff", 3000.0f }, { "p1.analog.keytrack", 0.0f }, { "p1.analog.detune", 0.0f }, { "p1.analog.oscmix", 0.0f },
                        { "p1.analog.drive", 0.0f }, { "p1.analog.shape", 0.0f }, { "p1.voices", 1.0f }, { "p1.glide", 0.05f },
                        { "p1.glide.mode", 2.0f }, { slot (1, 0, "dst"), (float) ex::Glide }, { slot (1, 0, "amt"), 0.5f } });
        const auto s = renderMidi (core, 30, [&] (int b) {
            std::vector<MidiEvent> m;
            if (b == 0) m.push_back (MidiEvent::noteOn (1, 57, 100, 0));
            if (b == 2 && pressure > 0) m.push_back (channelPressure (pressure));
            if (b == 10) m.push_back (MidiEvent::noteOn (1, 69, 100, 0));
            return m;
        });
        const size_t at = 10 * 512 + 7200; // 150 ms after the second note
        const double f = measureFrequency (s.l, sampleRate, at, at + 960);
        return centsBetween (440.0, f);
    };
    const double light = lag (0), firm = lag (127);
    CAPTURE (light);
    CAPTURE (firm);
    CHECK (firm > light + 200.0);
    CHECK (std::abs (light) < 150.0);

    // CC 5 sets the portamento time; pressure moves it, and releasing the pressure returns to the CC time
    Core core;
    core.prepare (sampleRate, 512);
    basicPatch (core);
    setAll (core, { { slot (1, 0, "dst"), (float) ex::Glide }, { slot (1, 0, "amt"), 0.5f } });
    renderMidi (core, 4, [] (int b) {
        std::vector<MidiEvent> m;
        if (b == 0) { m.push_back (MidiEvent::noteOn (1, 60, 100, 0)); m.push_back (MidiEvent { 0, (uint8_t) 0xb0, 5, 64 }); }
        if (b == 1) m.push_back (channelPressure (127));
        if (b == 3) m.push_back (channelPressure (0));
        return m;
    });
    renderMidi (core, 4);
    const int idx = core.params().indexOf ("p1.glide");
    CHECK (std::abs (core.part (0).modulationOf (idx)) < 1.0e-3f);
    CHECK (core.params().get (idx) == doctest::Approx (0.0f)); // the parameter itself is untouched
}

TEST_CASE ("p33: every function on every engine at full velocity and pressure, both signs, every curve: finite, bounded")
{
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        Core core; // one core per engine (building a core is the slow part)
        core.prepare (sampleRate, 512);
        if (core.part (0).engine (type) == nullptr) continue;
        setAll (core, { { "p1.engine", (float) type }, { "p1.midich", 0.0f } });
        for (float sign : { 1.0f, -1.0f })
            for (int batch = 1; batch < ex::numTargets; batch += 4)
            {
                for (int k = 0; k < 4; ++k)
                {
                    const int f = std::min (ex::numTargets - 1, batch + k);
                    setAll (core, { { slot (k / 2, k % 2, "dst"), (float) f }, { slot (k / 2, k % 2, "amt"), sign },
                                    { slot (k / 2, k % 2, "curve"), (float) ((batch + k) % mod::numCurves) } });
                }
                CAPTURE (engineTypeInfo (type).id);
                CAPTURE (batch);
                CAPTURE (sign);
                const auto s = renderMidi (core, 10, [] (int b) {
                    std::vector<MidiEvent> m;
                    if (b == 0) { m.push_back (MidiEvent::noteOn (1, 48, 127, 0)); m.push_back (MidiEvent::noteOn (1, 60, 127, 0)); }
                    if (b == 1) m.push_back (channelPressure (127));
                    if (b == 4) m.push_back (polyPressure (60, 127));
                    if (b == 6) m.push_back (MidiEvent::noteOn (1, 67, 127, 0));
                    if (b == 9) { m.push_back (channelPressure (0)); m.push_back (MidiEvent { 0, (uint8_t) 0xb0, 123, 0 }); }
                    return m;
                });
                CHECK (allFinite (s.l));
                CHECK (allFinite (s.r));
                CHECK (noDenormals (s.l));
                CHECK (peak (s.l) < 8.0f);
                CHECK (peak (s.r) < 8.0f);
            }
    }
}

TEST_CASE ("p33: the Expression page is part of the state and of part presets")
{
    Core a;
    setAll (a, { { "p2.expr.vel.dst2", (float) ex::FmAmount }, { "p2.expr.vel.amt2", -0.4f }, { "p2.expr.at.dst1", (float) ex::Glide },
                 { "p2.expr.at.amt1", 0.25f }, { "p2.expr.at.curve1", (float) mod::CurveLog } });
    const auto json = a.saveState ("{}");
    Core b;
    REQUIRE (b.loadState (json, nullptr));
    CHECK (b.params().get ("p2.expr.vel.dst2") == (float) ex::FmAmount);
    CHECK (b.params().get ("p2.expr.vel.amt2") == doctest::Approx (-0.4f));
    CHECK (b.params().get ("p2.expr.at.dst1") == (float) ex::Glide);
    CHECK (b.params().get ("p2.expr.at.amt1") == doctest::Approx (0.25f));
    CHECK (b.params().get ("p2.expr.at.curve1") == (float) mod::CurveLog);
}

TEST_CASE ("p33: every gliding factory part has exactly one pressure -> glide mechanism (Expression or p32 pressure glide)")
{
    // the pressure glide of p32 ("glide.press") and an Expression aftertouch slot on Glide Time both stretch the glide
    // with aftertouch; a factory part uses one of them, never both (the glide would be stretched twice) and never none
    namespace fs = std::filesystem;
    int parts = 0;
    for (const auto& e : fs::directory_iterator (fs::path (OHPE_SOURCE_DIR) / "resources" / "presets" / "factory"))
    {
        if (e.path().extension() != ".ohpepreset") continue;
        std::ifstream f (e.path());
        REQUIRE (f.good());
        std::stringstream ss;
        ss << f.rdbuf();
        preset::Preset p;
        REQUIRE_MESSAGE (preset::fromJson (ss.str(), p), e.path().filename().string());
        auto get = [&] (const std::string& id) { const auto it = p.params.find (id); return it != p.params.end() ? it->second : 0.0f; };
        std::vector<std::string> prefixes;
        if (p.meta.kind == preset::Kind::Part) prefixes.push_back ("");
        else
            for (int n = 1; n <= Core::numParts; ++n)
                if (p.params.count ("p" + std::to_string (n) + ".engine") != 0) prefixes.push_back ("p" + std::to_string (n) + ".");
        for (const auto& pre : prefixes)
        {
            if (get (pre + "glide") <= 0.0f) continue;
            int mechanisms = get (pre + "glide.press") != 0.0f ? 1 : 0;
            for (const char* s : { "1", "2" })
                if (std::lround (get (pre + "expr.at.dst" + s)) == ex::Glide && get (pre + "expr.at.amt" + s) != 0.0f) ++mechanisms;
            CHECK_MESSAGE (mechanisms == 1, e.path().filename().string() + " " + pre);
            ++parts;
        }
    }
    CHECK (parts > 0);
}
