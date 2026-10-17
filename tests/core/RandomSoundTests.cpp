// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p37: the RANDOM key - constrained rolls (no glide, little noise, musical ranges), offline check and loudness
// normalisation to the factory median. Prints "OHPE-STATS" / "OHPE-R" lines (scripts/ci/run-tests.py shows them
// for passing tests too) with the before / after numbers: before = the old roll (every engine parameter anywhere in
// the middle 80 % of its knob, nothing measured).
#include "OhpeCore.h"
#include "../TestSlice.h"
#include "breed/RandomSound.h"
#include "engines/EngineFactory.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

using namespace ohpe;
namespace fs = std::filesystem;
using breed::Genome;
using breed::Schema;

namespace
{
struct Base
{
    std::string name;
    int engine = 0;
    std::map<std::string, float> params;
};

/** Factory part presets that play no factory recording (the core tests have no sample bank). */
std::vector<Base> factoryBases()
{
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator (fs::path (OHPE_SOURCE_DIR) / "resources" / "presets" / "factory"))
        if (e.path().extension() == ".ohpepreset") files.push_back (e.path());
    std::sort (files.begin(), files.end());
    std::vector<Base> out;
    for (const auto& f : files)
    {
        std::ifstream in (f);
        std::stringstream ss;
        ss << in.rdbuf();
        preset::Preset p;
        if (! preset::fromJson (ss.str(), p) || p.meta.kind != preset::Kind::Part) continue;
        const auto inst = [&] (const char* id) { const auto i = p.params.find (id); return i != p.params.end() && i->second > 0.5f; };
        if (inst ("sampler.inst") || inst ("grain.inst") || inst ("sampler.inst2") || inst ("grain.inst2")) continue;
        const auto e = p.params.find ("engine");
        if (e == p.params.end()) continue;
        out.push_back ({ f.filename().string(), (int) e->second, p.params });
    }
    return out;
}

/** The RANDOM key before p37: every engine parameter (but level / tuning / voices / sample) anywhere in the middle
    80 % of its knob, discrete ones anywhere; glide, pitch envelope and gain untouched. */
Genome legacyRoll (const Schema& s, const Genome& base, int engine, uint64_t seed)
{
    Genome g = base;
    breed::Rng rng (seed);
    const std::string pre = std::string (engineTypeInfo ((EngineType) engine).id) + ".";
    for (int k = 0; k < s.size(); ++k)
    {
        const auto& gene = s.gene (k);
        if (gene.id.rfind (pre, 0) != 0) continue;
        const auto tail = gene.id.substr (pre.size());
        bool keep = false;
        for (const char* w : { "vol", "level", "gain", "tune", "fine", "oct", "sample", "voices", "root" })
            keep |= tail.find (w) != std::string::npos;
        if (keep) continue;
        g[(size_t) k] = gene.discrete() ? gene.fromNorm (rng.uniform()) : gene.fromNorm (0.1f + 0.8f * rng.uniform());
    }
    return g;
}

const Schema& schema()
{
    static const Schema s = [] {
        auto core = std::make_unique<Core>();
        return Schema::forPart (core->params(), 0);
    }();
    return s;
}

/** Base genome for engine `t`: a factory preset of that engine (cycled by `i`), or the default part on it. */
Genome baseFor (const std::vector<Base>& bases, int t, int i, std::string* name = nullptr)
{
    std::vector<const Base*> mine;
    for (const auto& b : bases)
        if (b.engine == t) mine.push_back (&b);
    if (mine.empty())
    {
        if (name != nullptr) *name = std::string ("init-") + engineTypeInfo ((EngineType) t).id;
        return breed::fromParams (schema(), { { "engine", (float) t } });
    }
    const auto* b = mine[(size_t) i % mine.size()];
    if (name != nullptr) *name = b->name;
    return breed::fromParams (schema(), b->params);
}

std::vector<int> engines()
{
    std::vector<int> e;
    for (int t = 0; t < numEngineTypes; ++t)
        if (engineTypeInfo ((EngineType) t).implemented) e.push_back (t);
    return e;
}

float median (std::vector<float> v)
{
    if (v.empty()) return 0.0f;
    std::sort (v.begin(), v.end());
    return v.size() % 2 ? v[v.size() / 2] : 0.5f * (v[v.size() / 2 - 1] + v[v.size() / 2]);
}
} // namespace

TEST_CASE ("random sound: loudness, true peak and flatness measure as expected")
{
    const double sr = 32000.0;
    const size_t n = (size_t) (0.95 * sr);
    std::vector<float> l (n), r (n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = 0.1f * (float) std::sin (2.0 * 3.14159265358979 * 997.0 * (double) i / sr);
    auto m = randomsound::measure (l, r, sr);
    CHECK (m.lufs == doctest::Approx (-20.0f).epsilon (0.03)); // stereo sine at -20 dBFS reads about -20 LUFS
    CHECK (m.flatness < 0.05f);
    CHECK (m.nonFinite == 0);
    // a sine at a quarter of the sample rate, sampled 45 degrees off its peaks: true peak 0 dB, sample peak -3 dB
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float) std::sin (3.14159265358979 * 0.5 * (double) i + 0.785398);
    m = randomsound::measure (l, r, sr);
    CHECK (m.truePeakDb > -0.6f);
    CHECK (m.truePeakDb < 0.3f);
    // white noise is flat, silence is silent
    breed::Rng rng (7);
    for (size_t i = 0; i < n; ++i) { l[i] = 0.2f * (rng.uniform() - 0.5f); r[i] = 0.2f * (rng.uniform() - 0.5f); }
    m = randomsound::measure (l, r, sr);
    CHECK (m.flatness > 0.6f);
    std::fill (l.begin(), l.end(), 0.0f);
    std::fill (r.begin(), r.end(), 0.0f);
    m = randomsound::measure (l, r, sr);
    CHECK (m.lufs < -100.0f);
}

TEST_CASE ("random sound: rolls are deterministic and valid, rarely glide, keep noise low and leave level / tuning alone")
{
    const auto& s = schema();
    const auto bases = factoryBases();
    REQUIRE (bases.size() >= 50);
    int total = 0, glideNew = 0, glideOld = 0, noisyNew = 0, noisyOld = 0;
    for (int t : engines())
        for (int i = 0; i < 250; ++i)
        {
            const auto base = baseFor (bases, t, i);
            const auto seed = (uint64_t) (1000 + 7919 * i + t);
            const auto g = randomsound::roll (s, base, t, seed);
            CHECK (breed::valid (s, g));
            CHECK (g == randomsound::roll (s, base, t, seed));
            for (const char* fixed : { "vol", "gain", "tune", "fine", "voices", "engine", "amp.a", "amp.s" })
                CHECK (g[(size_t) s.indexOf (fixed)] == base[(size_t) s.indexOf (fixed)]);
            const auto old = legacyRoll (s, base, t, seed);
            ++total;
            glideNew += randomsound::glideAtStart (s, g) ? 1 : 0;
            glideOld += randomsound::glideAtStart (s, old) ? 1 : 0;
            noisyNew += randomsound::noiseParamLevel (s, g, t) > 0.2f ? 1 : 0;
            noisyOld += randomsound::noiseParamLevel (s, old, t) > 0.2f ? 1 : 0;
            CHECK (s.gene (s.indexOf ("glide.press")).clamp (g[(size_t) s.indexOf ("glide.press")]) == 0.0f);
            if (g[(size_t) s.indexOf ("glide")] > 0.0f)
            {
                CHECK (g[(size_t) s.indexOf ("glide")] <= 0.08f + 1e-4f);
                CHECK (std::lround (g[(size_t) s.indexOf ("glide.mode")]) == 1); // legato only
            }
        }
    std::printf ("OHPE-STATS rolls n=%d glideAtStart old=%.1f%% new=%.1f%% noiseParam>0.2 old=%.1f%% new=%.1f%%\n", total,
                 100.0 * glideOld / total, 100.0 * glideNew / total, 100.0 * noisyOld / total, 100.0 * noisyNew / total);
    CHECK (glideNew * 10 <= total);      // <= 10 % slide into their first note (a short pitch blip, deliberately rare)
    CHECK (noisyNew * 100 <= total * 5); // >= 95 % without a noticeable noise source
}

TEST_CASE ("random sound: the loudness target is the factory median")
{
    auto au = std::make_unique<breed::Audition>();
    const auto bases = factoryBases();
    REQUIRE (bases.size() >= 50);
    std::vector<float> l, r, lufs, integrated;
    for (const auto& b : bases)
    {
        au->renderAudio (breed::fromParams (au->schema(), b.params), l, r);
        const auto m = randomsound::measure (l, r, au->sampleRate());
        CHECK (m.nonFinite == 0);
        if (m.momentaryMax > -70.0f) lufs.push_back (m.momentaryMax);
        if (m.lufs > -70.0f) integrated.push_back (m.lufs);
    }
    std::sort (lufs.begin(), lufs.end());
    const float med = median (lufs);
    const float p10 = lufs[lufs.size() / 10], p90 = lufs[lufs.size() * 9 / 10];
    std::printf ("OHPE-STATS factory n=%d momentary-max median=%.2f LUFS p10=%.2f p90=%.2f min=%.2f max=%.2f target=%.2f; integrated "
                 "median=%.2f\n",
                 (int) lufs.size(), med, p10, p90, lufs.front(), lufs.back(), randomsound::targetLufs, median (integrated));
    CHECK (std::abs (med - randomsound::targetLufs) <= 1.5f);
}

TEST_CASE ("random sound: 500 rolled sounds play at factory loudness, peak-safe, tonal, without a slide" * doctest::test_suite ("sliced"))
{
    auto au = std::make_unique<breed::Audition>();
    const auto& s = au->schema();
    const auto bases = factoryBases();
    REQUIRE (bases.size() >= 50);
    const auto eng = engines();
    const auto slice = test::testSlice();
    int n = 0, near = 0, exact = 0, tonal = 0, glide = 0, noisy = 0, usable = 0;
    std::vector<double> ms;
    for (int i = 0; i < 500; ++i)
    {
        if (! slice.has (i)) continue;
        const int t = eng[(size_t) (i + i / 8) % eng.size()]; // every slice sees every engine
        std::string name;
        const auto base = baseFor (bases, t, i / (int) eng.size(), &name);
        const auto seed = (uint64_t) (424242 + 104729ULL * (uint64_t) i);
        CAPTURE (name);
        CAPTURE (i);

        // before: the old roll as it was played (no level change)
        std::vector<float> l, r;
        const auto old = legacyRoll (s, base, t, seed);
        au->renderAudio (old, l, r);
        const auto before = randomsound::measure (l, r, au->sampleRate());

        // after
        const auto t0 = std::chrono::steady_clock::now();
        const auto res = randomsound::make (*au, base, t, seed);
        const double took = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        ms.push_back (took);
        const auto& m = res.loudness;
        ++n;
        CHECK (breed::valid (s, res.genome));
        CHECK (m.nonFinite == 0);
        CHECK (m.momentaryMax >= randomsound::targetLufs - 12.0f);     // never near-silent
        CHECK (m.truePeakDb <= randomsound::truePeakCeilingDb + 0.5f); // never clips
        const bool onTarget = std::abs (m.momentaryMax - randomsound::targetLufs) <= 3.0f;
        // gain at its top (the part's own chain caps the level): as loud as the sound it was rolled from
        const bool capped = ! onTarget && res.baseMomentary > -100.0f && m.momentaryMax >= res.baseMomentary - 3.0f;
        CHECK ((onTarget || capped || m.momentaryMax >= randomsound::targetLufs - 9.0f));
        near += onTarget || capped ? 1 : 0;
        exact += onTarget ? 1 : 0;
        tonal += m.flatness <= randomsound::maxFlatness ? 1 : 0;
        const bool gl = randomsound::glideAtStart (s, res.genome);
        const float nz = randomsound::noiseParamLevel (s, res.genome, t);
        glide += gl ? 1 : 0;
        noisy += nz > 0.2f ? 1 : 0;
        usable += res.usable ? 1 : 0;
        // columns: momentary max, integrated, true peak, flatness, glide at start, noise parameter level
        std::printf ("OHPE-R %d %s %s | before %.2f %.2f %.2f %.3f %d %.2f | after %.2f %.2f %.2f %.3f %d %.2f a=%d r=%d g=%.1f base %.2f %.0fms\n",
                     i, engineTypeInfo ((EngineType) t).id, name.c_str(), before.momentaryMax, before.lufs, before.truePeakDb,
                     before.flatness, randomsound::glideAtStart (s, old) ? 1 : 0, randomsound::noiseParamLevel (s, old, t),
                     m.momentaryMax, m.lufs, m.truePeakDb, m.flatness, gl ? 1 : 0, nz, res.attempts, res.renders, res.gainDb, res.baseMomentary, took);
    }
    if (n == 0) return;
    std::sort (ms.begin(), ms.end());
    std::printf ("OHPE-STATS slice n=%d near3dB=%d (target %d, capped-at-base %d) tonal=%d glide=%d noisy=%d usable=%d ms median=%.0f "
                 "max=%.0f\n",
                 n, near, exact, near - exact, tonal, glide, noisy, usable, ms[ms.size() / 2], ms.back());
    CHECK (near * 10 >= n * 9);      // >= 90 % within +-3 dB of the factory median
    CHECK (tonal * 100 >= n * 95);   // >= 95 % tonal (spectral flatness below the noise threshold)
    CHECK (noisy * 100 <= n * 5);    // >= 95 % without a noticeable noise source
    CHECK (glide * 10 <= n + 20);    // slides are rare (the 2000-roll test checks <= 10 % exactly)
}
