// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p18: preset breeding - genome schema, crossover, mutation, sessions (generations, undo), offline audition and
// level matching.
#include "OhpeCore.h"
#include "breed/SoundGenome.h"
#include "engines/EngineFactory.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>

using namespace ohpe;
using breed::Choice;
using breed::Gene;
using breed::Genome;
using breed::Options;
using breed::Rng;
using breed::Schema;
using breed::Session;

namespace
{
const Schema& schema()
{
    static const Schema s = [] {
        auto core = std::make_unique<Core>(); // on the heap (Windows stack)
        return Schema::forPart (core->params(), 0);
    }();
    return s;
}

preset::Preset factory (const char* file)
{
    std::ifstream in (std::string (OHPE_SOURCE_DIR) + "/resources/presets/factory/" + file);
    std::stringstream ss;
    ss << in.rdbuf();
    preset::Preset p;
    REQUIRE_MESSAGE (preset::fromJson (ss.str(), p), file);
    return p;
}

Genome factoryGenome (const char* file) { return breed::fromParams (schema(), factory (file).params); }

bool between (float v, float a, float b) { return v >= std::min (a, b) && v <= std::max (a, b); }

const char* analogFile = "arp-analog-arp.ohpepreset";
const char* fmFile = "arp-bell-cascade-arp.ohpepreset";
const char* vocalFile = "leads-vox-lead.ohpepreset";
const char* tableFile = "arp-figure-eight-arp.ohpepreset";
} // namespace

TEST_CASE ("breed: the schema sorts every sound parameter of a part into a group")
{
    const auto& s = schema();
    REQUIRE (s.size() > 1000);
    REQUIRE (s.engineGene() >= 0);
    REQUIRE (s.layerGene() >= 0);
    REQUIRE (s.gainGene() >= 0);
    CHECK (s.numSlots() == 6);
    CHECK (s.numRoutes() >= 8);
    for (int q = 0; q < s.numSlots(); ++q) CHECK (s.isSlotType (s.slotTypeGene (q)));
    auto group = [&] (const char* id) {
        const int k = s.indexOf (id);
        REQUIRE_MESSAGE (k >= 0, id);
        return s.gene (k).group;
    };
    CHECK (group ("analog.shape") == breed::Engine);
    CHECK (group ("analog.cutoff") == breed::Filter);
    CHECK (group ("wavetable.reso") == breed::Filter);
    CHECK (group ("amp.a") == breed::Envelope);
    CHECK (group ("fx3.delay.mix") == breed::Fx);
    CHECK (group ("lfo2.rate") == breed::Mod);
    CHECK (group ("mod.s1.amt") == breed::Mod);
    CHECK (group ("vol") == breed::Level);
    CHECK (group ("midich") == breed::Fixed);
    CHECK (group ("morph.x") == breed::Fixed);
    CHECK (s.gene (s.indexOf ("analog.reso")).risky);
    CHECK_FALSE (s.gene (s.indexOf ("sampler.inst")).mutable_);
    CHECK (s.gene (s.indexOf ("fx2.chorus.rate")).owner == Gene::OwnFx);
    CHECK (s.gene (s.indexOf ("fm.algo")).owner == Gene::OwnEngine);
    // a factory sound survives the round trip through a genome
    const auto p = factory (fmFile);
    const auto g = breed::fromParams (s, p.params);
    CHECK (breed::valid (s, g));
    const auto back = breed::toParams (s, g);
    for (const auto& [k, v] : p.params)
        if (const int i = s.indexOf (k); i >= 0) CHECK_MESSAGE (back.at (k) == doctest::Approx (s.gene (i).clamp (v)), k);
}

TEST_CASE ("breed: a seed gives the same brood every time, another seed another brood")
{
    const auto& s = schema();
    const auto a = factoryGenome (analogFile), b = factoryGenome (fmFile);
    const auto x = breed::makeBrood (s, a, b, 6, 0.4f, 1234);
    const auto y = breed::makeBrood (s, a, b, 6, 0.4f, 1234);
    const auto z = breed::makeBrood (s, a, b, 6, 0.4f, 1235);
    REQUIRE (x.size() == 6);
    int differ = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        CHECK (x[i].genome == y[i].genome);
        CHECK (x[i].seed == y[i].seed);
        differ += x[i].genome != z[i].genome ? 1 : 0;
    }
    CHECK (differ >= 5);
    int distinct = 0;
    for (size_t i = 1; i < x.size(); ++i) distinct += x[i].genome != x[0].genome ? 1 : 0;
    CHECK (distinct >= 4);
    // the number stream is the same everywhere
    Rng r (42), r2 (42);
    CHECK (r.next() == r2.next());
    Rng g (7);
    double sum = 0.0, sq = 0.0;
    for (int i = 0; i < 20000; ++i)
    {
        const double v = g.gauss();
        sum += v;
        sq += v * v;
    }
    CHECK (std::abs (sum / 20000.0) < 0.05);
    CHECK (std::abs (sq / 20000.0 - 1.0) < 0.05);
}

TEST_CASE ("breed: children stay inside every range, no NaN, levels and risky values bounded, at any mutation")
{
    const auto& s = schema();
    const char* files[] = { analogFile, fmFile, vocalFile, tableFile };
    for (int pa = 0; pa < 4; ++pa)
        for (int pb = 0; pb < 4; ++pb)
            for (float amount : { 0.0f, 0.5f, 1.0f })
            {
                CAPTURE (pa);
                CAPTURE (pb);
                CAPTURE (amount);
                const auto a = factoryGenome (files[pa]), b = factoryGenome (files[pb]);
                for (const auto& c : breed::makeBrood (s, a, b, 6, amount, (uint64_t) (pa * 100 + pb * 10) + (uint64_t) (amount * 7.0f)))
                {
                    CHECK (breed::valid (s, c.genome));
                    const int e = breed::engineOf (s, c.genome);
                    CHECK ((e == breed::engineOf (s, a) || e == breed::engineOf (s, b))); // the engine comes from a parent
                    for (const char* id : { "vol", "gain", "layer.level" })
                    {
                        if (c.inheritance.hybrid && std::string (id) == "layer.level") continue;
                        const int k = s.indexOf (id);
                        CHECK_MESSAGE (c.genome[(size_t) k] <= std::max (a[(size_t) k], b[(size_t) k]) + 1.0e-5f, id);
                    }
                    for (int i = 0; i < s.size(); ++i)
                        if (s.gene (i).risky && ! (c.inheritance.hybrid && s.gene (i).id.rfind ("layer.", 0) == 0))
                        {
                            const auto& gene = s.gene (i);
                            CHECK_MESSAGE (gene.toNorm (c.genome[(size_t) i])
                                               <= std::max (gene.toNorm (a[(size_t) i]), gene.toNorm (b[(size_t) i])) + 0.0501f,
                                           gene.id);
                        }
                }
            }
}

TEST_CASE ("breed: mutation 0 is the parent blend only")
{
    const auto& s = schema();
    Options o;
    o.hybridChance = 0.0f;
    const auto a = factoryGenome (analogFile), b = factoryGenome (tableFile);
    for (uint64_t seed = 1; seed <= 12; ++seed)
        for (const auto& c : breed::makeBrood (s, a, b, 6, 0.0f, seed, o))
            for (int i = 0; i < s.size(); ++i)
            {
                const float v = c.genome[(size_t) i], va = a[(size_t) i], vb = b[(size_t) i];
                if (s.gene (i).kind == ParamKind::Choice || s.gene (i).kind == ParamKind::Toggle)
                    CHECK_MESSAGE ((v == va || v == vb), s.gene (i).id);
                else
                    CHECK_MESSAGE (between (v, va, vb), s.gene (i).id);
            }

    // every group from A: the child is A (apart from the blended levels)
    Options allA;
    allA.choice.fill (Choice::A);
    for (const auto& c : breed::makeBrood (s, a, b, 3, 0.0f, 99, allA))
        for (int i = 0; i < s.size(); ++i)
            if (s.gene (i).group != breed::Level) CHECK_MESSAGE (c.genome[(size_t) i] == a[(size_t) i], s.gene (i).id);

    // a child of a sound with itself is that sound
    for (const auto& c : breed::makeBrood (s, a, a, 6, 0.0f, 5)) CHECK (c.genome == a);
}

TEST_CASE ("breed: cross-engine children are coherent, hybrids play the other engine as their layer")
{
    const auto& s = schema();
    const auto a = factoryGenome (analogFile), b = factoryGenome (fmFile);
    REQUIRE (breed::engineOf (s, a) == (int) EngineType::Analog);
    REQUIRE (breed::engineOf (s, b) == (int) EngineType::FM);
    REQUIRE (breed::layerOf (s, a) < 0);
    REQUIRE (breed::layerOf (s, b) < 0);
    Options o;
    o.choice[breed::Engine] = Choice::Blend;
    o.hybridChance = 1.0f;
    for (const auto& c : breed::makeBrood (s, a, b, 6, 0.0f, 77, o))
    {
        REQUIRE (c.inheritance.hybrid);
        const int e = breed::engineOf (s, c.genome), l = breed::layerOf (s, c.genome);
        CHECK (l >= 0);
        CHECK (l != e);
        // the analog engine's parameters are the analog parent's, the FM engine's the FM parent's
        for (int i = 0; i < s.size(); ++i)
        {
            const auto& gene = s.gene (i);
            if (gene.owner != Gene::OwnEngine) continue;
            if (gene.ownerA == (int) EngineType::Analog) CHECK_MESSAGE (c.genome[(size_t) i] == a[(size_t) i], gene.id);
            if (gene.ownerA == (int) EngineType::FM) CHECK_MESSAGE (c.genome[(size_t) i] == b[(size_t) i], gene.id);
        }
        CHECK (c.genome[(size_t) s.indexOf ("layer.level")] == doctest::Approx (o.hybridLevel));
    }
    o.hybridChance = 0.0f;
    for (const auto& c : breed::makeBrood (s, a, b, 6, 0.0f, 78, o))
    {
        CHECK_FALSE (c.inheritance.hybrid);
        CHECK (breed::layerOf (s, c.genome) < 0);
    }
}

TEST_CASE ("breed: effects keep their parameters with their slot type")
{
    const auto& s = schema();
    auto a = factoryGenome (analogFile), b = factoryGenome (analogFile);
    const int t2 = s.slotTypeGene (1);
    REQUIRE (t2 >= 0);
    const int rate = s.indexOf ("fx2.chorus.rate");
    REQUIRE (rate >= 0);
    const int chorus = s.gene (rate).ownerB;
    a[(size_t) t2] = (float) chorus;
    a[(size_t) rate] = s.gene (rate).minValue;
    b[(size_t) t2] = 0.0f; // B has no effect in slot 2
    b[(size_t) rate] = s.gene (rate).maxValue;
    Options o;
    o.choice[breed::Fx] = Choice::Blend;
    int withChorus = 0;
    for (uint64_t seed = 1; seed <= 4; ++seed)
        for (const auto& c : breed::makeBrood (s, a, b, 6, 0.0f, seed, o))
            if ((int) c.genome[(size_t) t2] == chorus)
            {
                ++withChorus;
                CHECK (c.genome[(size_t) rate] == a[(size_t) rate]); // only A plays it
            }
    CHECK (withChorus > 0);
}

TEST_CASE ("breed: a session breeds generations from kept children, with undo / redo and a JSON round trip")
{
    Session ses (schema());
    CHECK_FALSE (ses.breedParents (1));
    ses.setParent (0, "Glass Keys", factoryGenome (fmFile));
    ses.setParent (1, "Warm Arp", factoryGenome (analogFile));
    REQUIRE (ses.ready());
    ses.amount = 0.25f;
    REQUIRE (ses.breedParents (10));
    REQUIRE (ses.current() != nullptr);
    CHECK (ses.current()->number == 1);
    CHECK (ses.current()->children.size() == (size_t) Session::defaultBrood);
    CHECK (ses.current()->children[2].name == "Glass Warm 1.3");
    const auto gen1 = ses.current()->children;

    // keep two, breed again: the next generation descends from them
    ses.setKept (1, true);
    ses.setKept (4, true);
    REQUIRE (ses.breedAgain (11));
    CHECK (ses.current()->number == 2);
    CHECK (ses.parent (0).genome == gen1[1].genome);
    CHECK (ses.parent (1).genome == gen1[4].genome);
    CHECK (ses.numGenerations() == 2);
    // one kept: mutations of that child
    ses.setKept (0, true);
    REQUIRE (ses.breedAgain (12));
    CHECK (ses.current()->number == 3);
    for (const auto& c : ses.current()->children) CHECK (c.genome != ses.parent (0).genome);

    CHECK (ses.canUndo());
    ses.undo();
    CHECK (ses.current()->number == 2);
    ses.undo();
    CHECK (ses.current()->number == 1);
    CHECK (ses.current()->children[1].kept);
    CHECK (ses.canRedo());
    ses.redo();
    CHECK (ses.current()->number == 2);

    // round trip
    ses.options.choice[breed::Filter] = Choice::B;
    const auto text = ses.toJson();
    Session back (schema());
    REQUIRE (back.fromJson (text));
    CHECK (back.numGenerations() == 3);
    CHECK (back.currentIndex() == 1);
    CHECK (back.amount == doctest::Approx (0.25f));
    CHECK (back.options.choice[breed::Filter] == Choice::B);
    CHECK (back.toJson() == text);
    CHECK (back.current()->children[0].genome == ses.current()->children[0].genome);
    CHECK (back.parent (0).genome == ses.parent (0).genome);
    back.undo();
    CHECK (back.current()->number == 1);

    // a new brood after undo drops the redo branch
    ses.undo();
    REQUIRE (ses.breedParents (99));
    CHECK_FALSE (ses.canRedo());
    CHECK (ses.numGenerations() == 2);
    CHECK_FALSE (back.fromJson ("not json"));
}

TEST_CASE ("breed: audition renders children cleanly and level matching keeps the brood at its parents' loudness")
{
    auto au = std::make_unique<breed::Audition>();
    const auto& s = au->schema();
    const auto a = breed::fromParams (s, factory (analogFile).params), b = breed::fromParams (s, factory (fmFile).params);
    const auto sa = au->render (a), sb = au->render (b);
    CHECK (sa.nonFinite == 0);
    CHECK (sb.nonFinite == 0);
    CHECK (sa.rmsDb > -50.0f);
    CHECK (sb.rmsDb > -50.0f);
    CHECK (sa.brightness > 0.0f);
    CHECK (sa.brightness <= 1.0f);
    MESSAGE ("parents " << sa.rmsDb << " / " << sb.rmsDb << " dB, peaks " << sa.peakDb << " / " << sb.peakDb << ", brightness "
                        << sa.brightness << " / " << sb.brightness);
    const float target = 0.5f * (sa.rmsDb + sb.rmsDb);
    const float louder = std::max (sa.rmsDb, sb.rmsDb);
    int near = 0;
    for (float amount : { 0.3f, 1.0f })
        for (auto c : breed::makeBrood (s, a, b, 6, amount, 2024))
        {
            const auto sk = au->matchLevel (c.genome, target);
            CAPTURE (amount);
            CAPTURE (sk.rmsDb);
            CAPTURE (sk.peakDb);
            MESSAGE ("child rms " << sk.rmsDb << " peak " << sk.peakDb << " brightness " << sk.brightness);
            CHECK (breed::valid (s, c.genome));
            CHECK (sk.nonFinite == 0);
            CHECK (sk.peakDb < -0.9f);                            // never clips
            CHECK (sk.rmsDb <= std::max (target, louder) + 1.5f); // never louder than the parents
            float maxWave = 0.0f;
            for (float w : sk.wave)
            {
                CHECK (std::isfinite (w));
                maxWave = std::max (maxWave, w);
            }
            CHECK (maxWave <= 1.0f);
            if (std::abs (sk.rmsDb - target) < 3.0f) ++near;
        }
    CHECK (near >= 8); // most land on the parents' level (very quiet / slow ones are lifted by 6 dB at most)
}
