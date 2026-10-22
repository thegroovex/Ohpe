// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p69: learning breed - fitness signals, the per-engine taste profile, the bias it gives the brood (measurable against
// learning off, none at curiosity 100 %), reset, the profile file (fixture, damaged files).
#include "OhpeCore.h"
#include "breed/SoundGenome.h"
#include "breed/TasteProfile.h"
#include "engines/EngineFactory.h"
#include "model/FormatVersions.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>

using namespace ohpe;
using namespace ohpe::breed;

namespace
{
const Schema& schema()
{
    static const Schema s = [] {
        auto core = std::make_unique<Core>();
        return Schema::forPart (core->params(), 0);
    }();
    return s;
}

std::string readFixture (const char* rel)
{
    std::ifstream in (std::string (OHPE_SOURCE_DIR) + "/tests/formats/fixtures/" + rel, std::ios::binary);
    REQUIRE_MESSAGE (in.good(), "missing fixture " << rel);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

constexpr int analog = (int) EngineType::Analog;

Genome analogSound (float cutoffNorm)
{
    const auto& s = schema();
    auto g = fromParams (s, {});
    g[(size_t) s.engineGene()] = (float) analog;
    const int c = s.indexOf ("analog.cutoff");
    REQUIRE (c >= 0);
    g[(size_t) c] = s.gene (c).fromNorm (cutoffNorm);
    return g;
}

float cutoffNorm (const Genome& g)
{
    const int c = schema().indexOf ("analog.cutoff");
    return schema().gene (c).toNorm (g[(size_t) c]);
}

TasteProfile trainedHighCutoff()
{
    TasteProfile p;
    for (int i = 0; i < 8; ++i) p.record (schema(), "liked" + std::to_string (i), analogSound (0.92f), Signal::Favourite);
    for (int i = 0; i < 4; ++i) p.record (schema(), "skipped" + std::to_string (i), analogSound (0.12f), Signal::Skipped);
    return p;
}

double meanCutoff (const Options& o, float amount)
{
    double sum = 0.0;
    int n = 0;
    for (uint64_t seed = 1; seed <= 24; ++seed)
        for (const auto& c : makeBrood (schema(), analogSound (0.35f), analogSound (0.5f), 6, amount, seed, o))
        {
            REQUIRE (valid (schema(), c.genome));
            sum += cutoffNorm (c.genome);
            ++n;
        }
    return sum / n;
}
} // namespace

TEST_CASE ("taste: the signals change a sound's fitness and its engine's profile")
{
    const auto& s = schema();
    CHECK (fitnessDelta (Signal::Played, 30.0f) == doctest::Approx (0.5f));
    CHECK (fitnessDelta (Signal::Played, 1.0e9f) == doctest::Approx (2.0f)); // capped
    CHECK (fitnessDelta (Signal::Played, std::nanf ("")) == 0.0f);
    CHECK (fitnessDelta (Signal::Skipped) < 0.0f);
    CHECK (fitnessDelta (Signal::Unfavourite) < 0.0f);
    CHECK (fitnessDelta (Signal::Favourite) > fitnessDelta (Signal::Saved));

    TasteProfile p;
    CHECK (p.empty());
    p.record (s, "factory/A", analogSound (0.8f), Signal::Played, 60.0f);
    p.record (s, "factory/A", analogSound (0.8f), Signal::Saved);
    p.record (s, "factory/A", analogSound (0.8f), Signal::Reloaded);
    CHECK (p.fitness ("factory/A") == doctest::Approx (1.0f + 1.5f + 0.75f));
    REQUIRE (p.sound ("factory/A") != nullptr);
    CHECK (p.sound ("factory/A")->loads == 1);
    CHECK (p.sound ("factory/A")->engine == analog);
    const auto* st = p.engineStats (analog);
    REQUIRE (st != nullptr);
    REQUIRE (st->count ("analog.cutoff") == 1);
    CHECK (st->at ("analog.cutoff").like.mean == doctest::Approx (0.8f).epsilon (0.01));
    CHECK (st->at ("analog.cutoff").dislike.w == 0.0f);
    CHECK (st->count ("volume") == 0); // levels are never learned

    p.record (s, "breed:7", analogSound (0.1f), Signal::Skipped);
    CHECK (p.fitness ("breed:7") < 0.0f);
    CHECK (st->at ("analog.cutoff").dislike.mean == doctest::Approx (0.1f).epsilon (0.01));

    // the weight saturates: a long history fades instead of freezing
    for (int i = 0; i < 100; ++i) p.record (s, "x", analogSound (0.3f), Signal::Favourite);
    CHECK (st->at ("analog.cutoff").like.w <= TasteProfile::maxWeight);
    CHECK (st->at ("analog.cutoff").like.mean < 0.4f);

    // learning off: nothing is recorded
    p.learning = false;
    CHECK (p.record (s, "y", analogSound (0.9f), Signal::Favourite) == 0.0f);
    CHECK (p.sound ("y") == nullptr);
    CHECK (p.bias (s).engines.empty());

    // the sound list stays bounded
    TasteProfile q;
    for (int i = 0; i < TasteProfile::maxSounds + 40; ++i) q.recordKeyOnly ("k" + std::to_string (i), Signal::Saved);
    CHECK (q.numSounds() == TasteProfile::maxSounds);
    CHECK (q.sound ("k0") == nullptr);
    CHECK (q.sound ("k" + std::to_string (TasteProfile::maxSounds + 39)) != nullptr);
}

TEST_CASE ("taste: the brood follows the learned taste (measurable against learning off), curiosity 100 % = off")
{
    const auto& s = schema();
    auto p = trainedHighCutoff();
    p.curiosity = 0.0f;
    Options off;
    Options on;
    on.taste = std::make_shared<TasteBias> (p.bias (s));
    REQUIRE (on.taste->forEngine (analog) != nullptr);
    CHECK (on.taste->forEngine ((int) EngineType::FM) == nullptr);

    const double mOff = meanCutoff (off, 0.5f), mOn = meanCutoff (on, 0.5f);
    MESSAGE ("mean cutoff off " << mOff << " on " << mOn);
    CHECK (mOn > mOff + 0.05);

    // the score agrees with the taste
    CHECK (on.taste->score (s, analogSound (0.9f)) > on.taste->score (s, analogSound (0.5f)));
    CHECK (on.taste->score (s, analogSound (0.5f)) > on.taste->score (s, analogSound (0.12f)));

    // deterministic (seeded)
    const auto b1 = makeBrood (s, analogSound (0.35f), analogSound (0.5f), 6, 0.5f, 99, on);
    const auto b2 = makeBrood (s, analogSound (0.35f), analogSound (0.5f), 6, 0.5f, 99, on);
    REQUIRE (b1.size() == 6);
    for (size_t i = 0; i < b1.size(); ++i)
    {
        CHECK (b1[i].seed == b2[i].seed);
        CHECK (b1[i].genome == b2[i].genome);
    }

    // curiosity 100 %: exactly the unbiased brood
    p.curiosity = 1.0f;
    Options curious;
    curious.taste = std::make_shared<TasteBias> (p.bias (s));
    const auto u = makeBrood (s, analogSound (0.35f), analogSound (0.5f), 6, 0.5f, 99, off);
    const auto c = makeBrood (s, analogSound (0.35f), analogSound (0.5f), 6, 0.5f, 99, curious);
    REQUIRE (u.size() == c.size());
    for (size_t i = 0; i < u.size(); ++i) CHECK (u[i].genome == c[i].genome);

    // a middle curiosity lies between
    p.curiosity = 0.5f;
    Options half;
    half.taste = std::make_shared<TasteBias> (p.bias (s));
    const double mHalf = meanCutoff (half, 0.5f);
    CHECK (mHalf > mOff);
    CHECK (mHalf < mOn + 1.0e-6);

    // levels and risky caps are untouched by the bias: level genes stay the parents' blend
    for (const auto& ch : b1)
        for (int i = 0; i < s.size(); ++i)
            if (s.gene (i).group == Level)
            {
                const float a = analogSound (0.35f)[(size_t) i], b = analogSound (0.5f)[(size_t) i];
                CHECK (ch.genome[(size_t) i] >= std::min (a, b) - 1.0e-4f);
                CHECK (ch.genome[(size_t) i] <= std::max (a, b) + 1.0e-4f);
            }
}

TEST_CASE ("taste: kept children breed towards the taste too; parent pick by score")
{
    const auto& s = schema();
    auto p = trainedHighCutoff();
    p.curiosity = 0.0f;
    auto run = [&] (bool withTaste) {
        Session ses (s);
        ses.amount = 0.5f;
        if (withTaste) ses.options.taste = std::make_shared<TasteBias> (p.bias (s));
        ses.setParent (0, "A", analogSound (0.35f));
        ses.setParent (1, "B", analogSound (0.5f));
        double sum = 0.0;
        int n = 0;
        for (uint64_t seed = 1; seed <= 12; ++seed)
        {
            REQUIRE (ses.breedParents (seed));
            ses.setKept (0, true);
            ses.setKept (1, true);
            REQUIRE (ses.breedAgain (seed + 100));
            for (const auto& c : ses.current()->children)
            {
                sum += cutoffNorm (c.genome);
                ++n;
            }
        }
        return sum / n;
    };
    CHECK (run (true) > run (false) + 0.03);

    const std::vector<float> scores { -0.5f, -0.01f, -0.3f, -0.4f };
    int best = 0;
    for (int k = 0; k < 100; ++k) best += pickByTaste (scores, 0.0f, (float) k / 100.0f) == 1 ? 1 : 0;
    CHECK (best > 90);
    int first = 0;
    for (int k = 0; k < 100; ++k) first += pickByTaste (scores, 1.0f, (float) k / 100.0f) == 0 ? 1 : 0;
    CHECK (first == 25); // uniform
    CHECK (pickByTaste ({}, 0.5f, 0.5f) == -1);
    CHECK (pickByTaste ({ std::nanf ("") }, 0.0f, 0.9f) == 0);
}

TEST_CASE ("taste: why (top traits) and reset")
{
    const auto& s = schema();
    auto p = trainedHighCutoff();
    const auto t = p.traits (s, 5);
    REQUIRE (! t.empty());
    CHECK (t.front().gene == "analog.cutoff");
    CHECK (t.front().engine == analog);
    CHECK (t.front().liked > t.front().reference);
    CHECK (t.size() <= 5);

    p.curiosity = 0.2f;
    p.learning = true;
    p.reset();
    CHECK (p.empty());
    CHECK (p.traits (s).empty());
    CHECK (p.bias (s).engines.empty());
    CHECK (p.curiosity == doctest::Approx (0.2f)); // the settings stay
    Options o;
    o.taste = std::make_shared<TasteBias> (p.bias (s));
    const auto a = makeBrood (s, analogSound (0.35f), analogSound (0.5f), 6, 0.5f, 5, o);
    const auto b = makeBrood (s, analogSound (0.35f), analogSound (0.5f), 6, 0.5f, 5, {});
    for (size_t i = 0; i < a.size(); ++i) CHECK (a[i].genome == b[i].genome);
}

TEST_CASE ("taste: the profile file - v1 fixture, round trip, damaged files")
{
    const auto& s = schema();
    TasteProfile p;
    REQUIRE (p.fromJson (readFixture ("taste/taste-v1.json")));
    CHECK (p.fileVersion == 1);
    CHECK (p.curiosity == doctest::Approx (0.25f));
    CHECK (p.learning);
    CHECK (p.numSounds() == 3);
    CHECK (p.fitness ("factory/Warm Pad") == doctest::Approx (2.75f));
    CHECK (p.fitness ("factory/Harsh Lead") == doctest::Approx (-1.0f));
    const auto* st = p.engineStats (analog);
    REQUIRE (st != nullptr);
    CHECK (st->at ("analog.cutoff").like.mean == doctest::Approx (0.82f));
    CHECK (st->at ("analog.cutoff").dislike.w == doctest::Approx (1.0f));
    REQUIRE (p.bias (s).forEngine (analog) != nullptr);

    // round trip, version stamped
    const auto text = p.toJson();
    CHECK (nlohmann::json::parse (text).value ("version", 0) == formats::taste);
    TasteProfile q;
    REQUIRE (q.fromJson (text));
    CHECK (q.toJson() == text);
    // a newer build's records survive a later save
    p.record (s, "breed:1", analogSound (0.9f), Signal::Kept);
    TasteProfile r;
    REQUIRE (r.fromJson (p.toJson()));
    CHECK (r.fitness ("breed:1") == doctest::Approx (0.5f));
    CHECK (r.fitness ("factory/Warm Pad") == doctest::Approx (2.75f));

    // damaged files: an empty profile, the settings unchanged, never a throw
    TasteProfile d;
    d.curiosity = 0.6f;
    CHECK_FALSE (d.fromJson (readFixture ("taste/taste-v1-damaged.json")));
    CHECK (d.empty());
    CHECK (d.curiosity == doctest::Approx (0.6f));
    for (const char* bad : { "", "null", "[]", "{\"format\":\"ohpe-preset\"}", "{\"format\":7}", "\xff\xfe garbage" })
    {
        TasteProfile e;
        CHECK_FALSE (e.fromJson (bad));
        CHECK (e.empty());
    }
    // a readable file with wrong values: bad entries dropped, values clamped
    TasteProfile w;
    REQUIRE (w.fromJson (R"({"format":"ohpe-taste","version":"one","learning":"yes","curiosity":7,
        "sounds":{"a":"x","b":{"f":"high","n":-4,"t":-1},"":{"f":1}},
        "engines":{"two":{"analog.cutoff":[1,2,3,4]},"2":{"analog.cutoff":[1e99,-5,"x"],"amp.a":[1,0.5,0,0.5,9],
        "analog.reso":[100,2,-1,0.5]}}})"));
    CHECK (w.fileVersion == 0);
    CHECK (w.learning);
    CHECK (w.curiosity == doctest::Approx (1.0f));
    CHECK (w.numSounds() == 1); // only "b" (with defaults)
    CHECK (w.fitness ("b") == 0.0f);
    REQUIRE (w.engineStats (analog) != nullptr);
    CHECK (w.engineStats (analog)->size() == 1);
    CHECK (w.engineStats (analog)->at ("analog.reso").like.w == doctest::Approx (TasteProfile::maxWeight));
    CHECK (w.engineStats (analog)->at ("analog.reso").like.mean == doctest::Approx (1.0f));
    CHECK (w.engineStats (analog)->at ("analog.reso").dislike.w == 0.0f);
    // a profile naming genes this build does not know (or does not learn) gives no bias for them
    TasteProfile u;
    REQUIRE (u.fromJson (R"({"format":"ohpe-taste","version":9,"engines":{"2":{"nothere.x":[5,0.5,0,0.5],"volume":[5,0.9,0,0.5]}}})"));
    CHECK (u.bias (s).forEngine (analog) == nullptr);
}
