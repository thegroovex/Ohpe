// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p71: the binaural factory presets (tools/presets/make_binaural.py; their files are listed in
// tools/presets/binaural_files.json). Every one carries a musical category first and the "3D" category (so the library's
// 3D chip and the search for "3D" / "binaural" find it), switches the part's 3D stage on, has a mod wheel routing and
// renders clean with a real binaural image (the two ears differ). Loudness and the wheel's effect are checked for the
// whole bank by FactorySoundTests.

#include "OhpeCore.h"
#include "../TestSlice.h"
#include "mod/ModMatrix.h"
#include "preset/Preset.h"
#include "preset/PresetLibrary.h"
#include "render/PresetRender.h"
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

using namespace ohpe;
namespace fs = std::filesystem;

namespace
{
fs::path bankDir() { return fs::path (OHPE_SOURCE_DIR) / "resources" / "presets" / "factory"; }

std::string slurp (const fs::path& p)
{
    std::ifstream in (p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::string> binauralFiles()
{
    const auto j = nlohmann::json::parse (slurp (fs::path (OHPE_SOURCE_DIR) / "tools" / "presets" / "binaural_files.json"), nullptr, false);
    std::vector<std::string> out;
    if (j.is_array())
        for (const auto& v : j)
            if (v.is_string()) out.push_back (v.get<std::string>());
    return out;
}

preset::Preset loadPreset (const std::string& file)
{
    preset::Preset p;
    REQUIRE_MESSAGE (preset::fromJson (slurp (bankDir() / file), p), file);
    return p;
}

float param (const preset::Preset& p, const std::string& id, float fallback = 0.0f)
{
    const auto i = p.params.find (id);
    return i != p.params.end() ? i->second : fallback;
}
} // namespace

TEST_CASE ("p71 binaural presets: at least 40, tagged category + 3D, 3D stage on, wheel routed, found by the library")
{
    const auto files = binauralFiles();
    REQUIRE (files.size() >= 40);
    const std::set<std::string> musical { "Bass", "Keys", "Pads", "Leads", "Plucks", "Drums", "FX", "Arp", "Textures", "Vocal" };
    std::set<std::string> categories, engines;
    const int wheel = mod::sourceIndexOfKey ("modwheel");
    for (const auto& f : files)
    {
        CAPTURE (f);
        const auto p = loadPreset (f);
        REQUIRE (p.meta.kind == preset::Kind::Part);
        REQUIRE (p.meta.tags.size() >= 2);
        CHECK (musical.count (p.meta.tags[0]) == 1);
        CHECK (p.meta.tags[1] == "3D");
        CHECK (std::find (p.meta.tags.begin(), p.meta.tags.end(), std::string ("Binaural")) == p.meta.tags.end());
        categories.insert (p.meta.tags[0]);
        engines.insert (p.meta.engine);
        CHECK (param (p, "space.on") > 0.5f);
        bool wheelRoute = false;
        for (int k = 1; k <= mod::numSlots; ++k)
        {
            const auto s = "mod.s" + std::to_string (k) + ".";
            if ((int) param (p, s + "src") == wheel && (int) param (p, s + "dst") > mod::DstOff && param (p, s + "amt") != 0.0f)
                wheelRoute = true;
        }
        CHECK (wheelRoute);

        preset::Entry e;
        e.uid = "factory:" + f;
        e.meta = p.meta;
        e.factory = true;
        preset::Filter chip;
        chip.tags = { "3D" };
        CHECK (preset::matches (e, chip));
        CHECK (preset::matchesText (e, "3D"));
        CHECK (preset::matchesText (e, "binaural"));
    }
    CHECK (categories.size() >= 9);
    CHECK (engines.size() == 8); // every engine
}

TEST_CASE ("p71 binaural presets: render clean, non-silent, with a binaural image" * doctest::test_suite ("sliced"))
{
    const auto files = binauralFiles();
    const auto slice = test::testSlice();
    long long item = 0;
    for (const auto& f : files)
    {
        const auto p = loadPreset (f);
        if (! slice.has (item++)) continue;
        CAPTURE (f);
        Core core;
        core.prepare (48000.0, 256);
        const auto a = render::renderPart (core, p, 0, 100.0, 4.0, 1.0);
        const auto m = render::measure (a);
        CAPTURE (m.activeRmsDb);
        CAPTURE (m.peakDb);
        CHECK (m.nonFinite == 0);
        CHECK (m.denormals == 0);
        CHECK (m.activeRmsDb > -30.0);
        double side = 0.0, mid = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            const double s = 0.5 * ((double) a.l[i] - a.r[i]), c = 0.5 * ((double) a.l[i] + a.r[i]);
            side += s * s;
            mid += c * c;
        }
        CAPTURE (10.0 * std::log10 ((side + 1.0e-20) / (mid + 1.0e-20)));
        CHECK (side > mid * 1.0e-3); // the ears differ: the source sits somewhere off the centre line
    }
}
