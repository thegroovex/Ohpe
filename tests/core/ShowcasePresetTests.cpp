// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p44: the showcase factory presets (tools/presets/factory_parts_showcase.py, showcase_extra.py; the list of their files is
// tools/presets/showcase_files.json). Every one loads, uses at least one of the newer features, renders non-silent and
// clean, plays at factory loudness (the RANDOM key's measure: the loudest 400 ms K-weighted block of C4 + G4, compared
// with the factory median) and does something on the mod wheel. Prints "OHPE-STATS showcase ..." lines.
#include "OhpeCore.h"
#include "../TestSlice.h"
#include "breed/RandomSound.h"
#include "mod/ModMatrix.h"
#include "morph/MorphSpace.h"
#include "preset/Preset.h"
#include "render/PresetRender.h"

#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>

using namespace ohpe;
namespace fs = std::filesystem;

namespace
{
fs::path factoryDir() { return fs::path (OHPE_SOURCE_DIR) / "resources" / "presets" / "factory"; }

std::string readFile (const fs::path& p)
{
    std::ifstream in (p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::string> showcaseFiles()
{
    const auto j = nlohmann::json::parse (readFile (fs::path (OHPE_SOURCE_DIR) / "tools" / "presets" / "showcase_files.json"), nullptr, false);
    std::vector<std::string> out;
    if (j.is_array())
        for (const auto& v : j)
            if (v.is_string()) out.push_back (v.get<std::string>());
    return out;
}

preset::Preset load (const std::string& file)
{
    preset::Preset p;
    REQUIRE_MESSAGE (preset::fromJson (readFile (factoryDir() / file), p), file);
    return p;
}

float get (const preset::Preset& p, const std::string& id, float fallback = 0.0f)
{
    const auto i = p.params.find (id);
    return i != p.params.end() ? i->second : fallback;
}

bool playsRecording (const preset::Preset& p)
{
    for (const char* id : { "sampler.inst", "grain.inst", "sampler.inst2", "grain.inst2" })
        if (get (p, id) > 0.5f) return true;
    return false;
}

/** The newer features a part preset uses (life, morph, user wavetable, resonator stage / rack type, character, 3D,
    SUB, expression, pressure glide, tempo-synced LFO 2 / 3). */
std::set<std::string> featuresOf (const preset::Preset& p)
{
    std::set<std::string> f;
    if (get (p, "life") > 0.0f) f.insert ("life");
    if (p.morph && get (p, "morph.on") > 0.5f) f.insert ("morph");
    if (p.userTables && get (p, "usertable.wt") > 0.5f) f.insert ("usertable");
    if (get (p, "reso.on") > 0.5f) f.insert ("reso");
    for (int k = 1; k <= 6; ++k)
        if ((int) get (p, (k == 1 ? std::string ("fx.") : "fx" + std::to_string (k) + ".") + "type") == 23) f.insert ("reso");
    for (const char* c : { "char.bright", "char.hard", "char.alive", "char.wide" })
        if (get (p, c) != 0.0f) f.insert ("char");
    if (get (p, "space.on") > 0.5f && get (p, "space.path") > 0.5f) f.insert ("space");
    if (get (p, "sub.level") > 0.0f) f.insert ("sub");
    for (const char* e : { "expr.vel.amt1", "expr.vel.amt2", "expr.at.amt1", "expr.at.amt2" })
        if (get (p, e) != 0.0f) f.insert ("expr");
    if (get (p, "glide.press") != 0.0f) f.insert ("glidepress");
    if ((get (p, "lfo2.sync") > 0.5f && get (p, "lfo2.depth") > 0.0f) || (get (p, "lfo3.sync") > 0.5f && get (p, "lfo3.depth") > 0.0f))
        f.insert ("tempo");
    return f;
}

bool wheelRouting (const preset::Preset& p)
{
    const int wheel = mod::sourceIndexOfKey ("modwheel");
    for (int k = 1; k <= mod::numSlots; ++k)
    {
        const auto s = "mod.s" + std::to_string (k) + ".";
        const int src = (int) get (p, s + "src"), via = (int) get (p, s + "via"), dst = (int) get (p, s + "dst");
        if ((src == wheel || (via == wheel && src != mod::SrcOff)) && dst > mod::DstOff && get (p, s + "amt") != 0.0f) return true;
    }
    return get (p, "morph.on") > 0.5f && (int) get (p, "morph.xsrc") == (int) MorphSpace::CtrlModWheel;
}

float median (std::vector<float> v)
{
    if (v.empty()) return -200.0f;
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}
} // namespace

TEST_CASE ("p44 showcase presets: at least 40 new presets in the bank, each loads and uses a newer feature")
{
    const auto files = showcaseFiles();
    REQUIRE (files.size() >= 40);
    std::set<std::string> names, used;
    int parts = 0, projects = 0;
    for (const auto& f : files)
    {
        CAPTURE (f);
        REQUIRE (fs::exists (factoryDir() / f));
        const auto p = load (f);
        CHECK (p.meta.author == "groovex");
        CHECK (p.meta.comment.size() > 40);
        CHECK (names.insert (p.meta.name).second);
        REQUIRE (! p.meta.tags.empty());
        CHECK (preset::isCategory (p.meta.tags.front()));
        if (p.meta.kind == preset::Kind::Project)
        {
            ++projects;
            // a project shows what only a project carries: Follow, motion lanes, pad settings or several 3D parts
            const bool follow = get (p, "harmony.p2.follow") > 0.5f || get (p, "harmony.p3.follow") > 0.5f;
            const bool lanes = p.sections.count ("motion") != 0;
            const bool pads = get (p, "pads.sweep.len", -1.0f) >= 0.0f && p.params.count ("pads.quant") != 0;
            int spaced = 0;
            for (int n = 1; n <= Core::numParts; ++n) spaced += get (p, "p" + std::to_string (n) + ".space.on") > 0.5f ? 1 : 0;
            CHECK ((follow || lanes || pads || spaced >= 3));
            continue;
        }
        ++parts;
        const auto feat = featuresOf (p);
        CAPTURE (feat.size());
        CHECK (! feat.empty());
        used.insert (feat.begin(), feat.end());
        CHECK (wheelRouting (p));                                  // every preset does something on the mod wheel
        CHECK (get (p, "velsens", 1.0f) > 0.0f);                   // and answers velocity
        if (feat.count ("glidepress") == 0) CHECK (get (p, "glide") == 0.0f); // glide off unless the preset is about it
    }
    CHECK (parts >= 36);
    CHECK (projects >= 3);
    for (const char* f : { "life", "morph", "usertable", "reso", "char", "space", "sub", "expr", "glidepress", "tempo" })
    {
        CAPTURE (f);
        CHECK (used.count (f) == 1); // every newer feature has presets of its own
    }
}

TEST_CASE ("p44 showcase presets: the start sound stays the first factory preset by name")
{
    // p40: a new instance plays the first factory preset by name; the showcase presets are appended and sort after it
    std::vector<std::string> bank, mine;
    const auto files = showcaseFiles();
    const std::set<std::string> showcase (files.begin(), files.end());
    for (const auto& e : fs::directory_iterator (factoryDir()))
    {
        if (e.path().extension() != ".ohpepreset") continue;
        preset::Preset p;
        REQUIRE (preset::fromJson (readFile (e.path()), p));
        (showcase.count (e.path().filename().string()) ? mine : bank).push_back (p.meta.name);
    }
    auto fold = [] (std::string s) { std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return (char) std::tolower (c); }); return s; };
    std::string first = fold (bank.front());
    for (const auto& n : bank) first = std::min (first, fold (n));
    for (const auto& n : mine)
    {
        CAPTURE (n);
        CHECK (fold (n) > first);
    }
}

TEST_CASE ("p44 showcase presets: render clean, non-silent and change with the mod wheel" * doctest::test_suite ("sliced"))
{
    const auto files = showcaseFiles();
    const auto slice = test::testSlice();
    long long item = 0;
    for (const auto& f : files)
    {
        const auto p = load (f);
        if (! slice.has (item++)) continue;
        CAPTURE (f);
        auto render = [&] (int wheel) {
            Core core;
            core.prepare (48000.0, 256);
            if (p.meta.kind == preset::Kind::Project) return render::renderProject (core, p, 3.0, true, wheel);
            return render::renderPart (core, p, 0, 100.0, 4.0, 1.0, 0.0, {}, 0, wheel);
        };
        const auto a0 = render (0), a1 = render (127);
        const auto m0 = render::measure (a0), m1 = render::measure (a1);
        CAPTURE (m0.activeRmsDb);
        CAPTURE (m0.peakDb);
        CHECK (m0.nonFinite == 0);
        CHECK (m1.nonFinite == 0);
        CHECK (m0.denormals == 0);
        CHECK (std::abs (m0.dc) < 0.01);
        CHECK (m0.activeRmsDb > -30.0); // non-silent (the core has no recordings: their built-in stand-ins play)
        CHECK (m0.hfDb < -3.0);
        if (! playsRecording (p)) CHECK (m0.peakDb < 0.0);
        if (p.meta.kind == preset::Kind::Project || get (p, "morph.on") > 0.5f) continue; // morph: the morph tests move the pad
        double diff = 0.0, ref = 0.0;
        for (size_t i = 0; i < std::min (a0.size(), a1.size()); ++i)
            for (int c = 0; c < 2; ++c)
            {
                const double x = c == 0 ? a0.l[i] : a0.r[i], y = c == 0 ? a1.l[i] : a1.r[i];
                diff += (y - x) * (y - x);
                ref += x * x;
            }
        CAPTURE (10.0 * std::log10 ((diff + 1.0e-20) / (ref + 1.0e-20)));
        CHECK (diff > ref * 1.0e-4);                     // the wheel is audible
        CHECK (m1.activeRmsDb < m0.activeRmsDb + 3.0);  // and level-safe
    }
}

TEST_CASE ("p44 showcase presets: factory loudness (the RANDOM key's measure), true peak at most -1 dBTP")
{
    auto au = std::make_unique<breed::Audition>();
    std::vector<float> l, r, mine;
    const auto files = showcaseFiles();
    for (const auto& f : files)
    {
        const auto p = load (f);
        if (p.meta.kind != preset::Kind::Part || playsRecording (p) || p.userTables) continue; // no bank / table in the audition
        CAPTURE (f);
        au->renderAudio (breed::fromParams (au->schema(), p.params), l, r);
        const auto m = randomsound::measure (l, r, au->sampleRate());
        CAPTURE (m.momentaryMax);
        CAPTURE (m.truePeakDb);
        CAPTURE (m.flatness);
        CHECK (m.nonFinite == 0);
        CHECK (m.momentaryMax > randomsound::targetLufs - 14.0f); // a slow swell is quieter in the short audition
        CHECK (m.momentaryMax < randomsound::targetLufs + 6.0f);
        CHECK (m.truePeakDb <= -1.0f);
        std::printf ("OHPE-STATS showcase %s momentary-max=%.2f LUFS lufs=%.2f tp=%.2f flatness=%.3f\n", f.c_str(), m.momentaryMax,
                     m.lufs, m.truePeakDb, m.flatness);
        mine.push_back (m.momentaryMax);
    }
    REQUIRE (mine.size() >= 25);
    const float med = median (mine);
    std::printf ("OHPE-STATS showcase n=%d momentary-max median=%.2f LUFS (factory target %.2f)\n", (int) mine.size(), med,
                 randomsound::targetLufs);
    CHECK (std::abs (med - randomsound::targetLufs) <= 3.0f);
}
