// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"
#include "model/ParamRegistry.h"

#include <doctest/doctest.h>

#include <fstream>
#include <set>
#include <string>

using namespace ohpe;

TEST_CASE ("descriptor normalisation round-trips and matches the skew formula")
{
    auto d = pd::continuous ("x", "X", "X", 20.0f, 20000.0f, 1000.0f, ParamUnit::Hertz, 0.25f);
    for (float v : { 20.0f, 100.0f, 1000.0f, 5000.0f, 20000.0f })
        CHECK (d.fromNormalised (d.toNormalised (v)) == doctest::Approx (v).epsilon (1e-4));
    CHECK (d.toNormalised (20.0f) == doctest::Approx (0.0f));
    CHECK (d.toNormalised (20000.0f) == doctest::Approx (1.0f));
    CHECK (d.toNormalised (20.0f + (20000.0f - 20.0f) * 0.5f) == doctest::Approx (std::pow (0.5f, 0.25f)));

    auto c = pd::choice ("c", "C", "C", { "a", "b", "c" }, { "a", "b", "c" }, 1);
    CHECK (c.fromNormalised (0.0f) == 0.0f);
    CHECK (c.fromNormalised (0.5f) == 1.0f);
    CHECK (c.fromNormalised (1.0f) == 2.0f);
    CHECK (c.clamp (7.0f) == 2.0f);
    CHECK (c.clamp (NAN) == 0.0f);
}

TEST_CASE ("registry rejects duplicates and late additions")
{
    ParamRegistry r;
    r.add (pd::continuous ("a", "A", "A", 0, 1, 0.5f));
    CHECK_THROWS (r.add (pd::continuous ("a", "A", "A", 0, 1, 0.5f)));
    r.freeze();
    CHECK_THROWS (r.add (pd::continuous ("b", "B", "B", 0, 1, 0.5f)));
    CHECK (r.get (0) == 0.5f);
    r.set (0, 3.0f);
    CHECK (r.get (0) == 1.0f); // clamped
    CHECK (r.indexOf ("nope") == ParamRegistry::invalidIndex);
    CHECK_FALSE (r.set ("nope", 1.0f));
}

TEST_CASE ("full layout: unique IDs, valid ranges, bilingual names")
{
    Core core;
    const auto& reg = core.params();
    CHECK (reg.size() > 150);
    std::set<std::string> ids;
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        CAPTURE (d.id);
        CHECK (ids.insert (d.id).second);
        CHECK (d.minValue < d.maxValue);
        CHECK (d.defaultValue >= d.minValue);
        CHECK (d.defaultValue <= d.maxValue);
        CHECK_FALSE (d.nameEn.empty());
        CHECK_FALSE (d.nameDe.empty());
        if (d.kind == ParamKind::Choice) CHECK (d.choices.size() == d.choicesDe.size());
        for (char ch : d.id) CHECK ((std::islower ((unsigned char) ch) || std::isdigit ((unsigned char) ch) || ch == '.'));
    }
}

TEST_CASE ("parameter IDs are append-only (golden prefix)")
{
    Core core;
    std::ifstream in (std::string (OHPE_TEST_DATA_DIR) + "/golden/param_ids_v1.txt");
    REQUIRE (in.good());
    std::string line;
    int i = 0;
    while (std::getline (in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        REQUIRE_MESSAGE (i < core.params().size(), "parameter removed: " << line);
        CHECK_MESSAGE (core.params().descriptor (i).id == line, "parameter " << i << " changed/reordered; expected " << line);
        ++i;
    }
    CHECK (i > 0);
    // new parameters may follow; then extend the golden file (see CONTRIBUTING.md)
}
