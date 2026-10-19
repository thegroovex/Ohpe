// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p50: every part remembers the preset loaded into it. The name on the preset strip / controller display, the
// library highlight and the prev / next keys follow the edit part ("ui.part"); sessions keep all four names, and
// a session from before (one name) gives that name to every part.

#include <doctest/doctest.h>

#include "plugin/PluginProcessor.h"

#include <memory>

using namespace ohpe;

namespace
{
void setEditPart (OhpeProcessor& p, int oneBased)
{
    p.core().params().set (Core::Ids::editPart, (float) oneBased);
    p.parameter (Core::Ids::editPart)->setValueNotifyingHost (p.parameter (Core::Ids::editPart)->convertTo0to1 ((float) oneBased));
}

/** The first `n` factory entries of `kind` (indices into entries()). */
std::vector<int> factoryOfKind (PresetManager& pm, preset::Kind kind, int n)
{
    std::vector<int> found;
    for (int i = 0; i < (int) pm.entries().size() && (int) found.size() < n; ++i)
        if (pm.entries()[(size_t) i].factory && pm.entries()[(size_t) i].meta.kind == kind) found.push_back (i);
    return found;
}

juce::String nameOf (PresetManager& pm, int i) { return juce::String::fromUTF8 (pm.entries()[(size_t) i].meta.name.c_str()); }
} // namespace

TEST_CASE ("part preset name: switching the edit part shows that part's preset (p50)")
{
    auto procPtr = std::make_unique<OhpeProcessor>(); // on the heap: a processor is large (Windows stack)
    auto& p = *procPtr;
    auto& pm = p.presets();
    const auto parts = factoryOfKind (pm, preset::Kind::Part, 2);
    REQUIRE (parts.size() == 2);
    const int a = parts[0], b = parts[1];
    REQUIRE (nameOf (pm, a) != nameOf (pm, b));

    setEditPart (p, 1);
    REQUIRE (pm.editPart() == 0);
    REQUIRE (pm.load (a));     // into the edit part (part 1)
    REQUIRE (pm.load (b, 2));  // into part 3 (e.g. from the library of another part)
    CHECK (pm.currentName() == nameOf (pm, a));
    CHECK (pm.current() == a);

    setEditPart (p, 3);
    CHECK (pm.editPart() == 2);
    CHECK (pm.currentName() == nameOf (pm, b));
    CHECK (pm.current() == b);
    CHECK (pm.currentUid() == pm.entries()[(size_t) b].uid);

    setEditPart (p, 2); // nothing loaded there
    CHECK (pm.current() == -1);
    CHECK (pm.currentName() == "Init");

    setEditPart (p, 1);
    CHECK (pm.currentName() == nameOf (pm, a));

    // per-part queries
    CHECK (pm.current (0) == a);
    CHECK (pm.current (1) == -1);
    CHECK (pm.current (2) == b);
    CHECK (pm.current (3) == -1);

    // prev / next step the edit part's preset and leave the others alone
    setEditPart (p, 3);
    REQUIRE (pm.step (1));
    CHECK (pm.current (2) != b);
    CHECK (pm.current (0) == a);
    CHECK (pm.current (1) == -1);
    setEditPart (p, 1);
    CHECK (pm.currentName() == nameOf (pm, a));

    // a project preset names all four parts
    const auto projects = factoryOfKind (pm, preset::Kind::Project, 1);
    if (! projects.empty())
    {
        REQUIRE (pm.load (projects[0]));
        for (int part = 0; part < Core::numParts; ++part) CHECK (pm.current (part) == projects[0]);
        setEditPart (p, 4);
        CHECK (pm.currentName() == nameOf (pm, projects[0]));
    }

    // a preview that is reverted restores every part's name
    setEditPart (p, 2);
    REQUIRE (pm.load (a, 1));
    const auto before = pm.currentUids();
    pm.beginPreview();
    REQUIRE (pm.load (b));
    CHECK (pm.current (1) == b);
    pm.revertPreview();
    CHECK (pm.currentUids() == before);
}

TEST_CASE ("part preset name: sessions keep the names of all four parts, old sessions apply theirs to every part (p50)")
{
    juce::MemoryBlock session;
    std::array<std::string, 4> saved;
    int editPartSaved = 0;
    {
        auto aPtr = std::make_unique<OhpeProcessor>();
        auto& a = *aPtr;
        auto& pm = a.presets();
        const auto parts = factoryOfKind (pm, preset::Kind::Part, 2);
        REQUIRE (parts.size() == 2);
        REQUIRE (pm.load (parts[0], 0));
        REQUIRE (pm.load (parts[1], 3));
        setEditPart (a, 4);
        editPartSaved = pm.editPart();
        saved = pm.currentUids();
        CHECK (saved[0] == pm.entries()[(size_t) parts[0]].uid);
        CHECK (saved[1].empty());
        CHECK (saved[3] == pm.entries()[(size_t) parts[1]].uid);
        a.getStateInformation (session);
    }
    auto bPtr = std::make_unique<OhpeProcessor>();
    auto& b = *bPtr;
    b.setStateInformation (session.getData(), (int) session.getSize());
    CHECK (b.presets().currentUids() == saved);
    CHECK (b.presets().editPart() == editPartSaved);
    CHECK (b.presets().currentUid() == saved[3]);
    setEditPart (b, 1);
    CHECK (b.presets().currentUid() == saved[0]);
    setEditPart (b, 2);
    CHECK (b.presets().currentName() == "Init");

    // a second round trip writes the same
    juce::MemoryBlock again;
    b.getStateInformation (again);
    auto cPtr = std::make_unique<OhpeProcessor>();
    cPtr->setStateInformation (again.getData(), (int) again.getSize());
    CHECK (cPtr->presets().currentUids() == saved);

    // the ui block: "presets" (one per part) wins; an old block with only "preset" gives it to every part
    auto legacy = juce::JSON::parse (R"({"preset":"factory:x.ohpepreset"})");
    const auto fromLegacy = OhpeProcessor::sessionPresetUids (legacy);
    for (const auto& u : fromLegacy) CHECK (u == "factory:x.ohpepreset");
    auto current = juce::JSON::parse (R"({"preset":"factory:b","presets":["factory:a","","factory:b",""]})");
    const auto fromCurrent = OhpeProcessor::sessionPresetUids (current);
    CHECK (fromCurrent[0] == "factory:a");
    CHECK (fromCurrent[1].empty());
    CHECK (fromCurrent[2] == "factory:b");
    CHECK (fromCurrent[3].empty());
    const auto fromNothing = OhpeProcessor::sessionPresetUids (juce::var());
    for (const auto& u : fromNothing) CHECK (u.empty());

    // restoring an old session's single uid names every part
    const auto uid = saved[0];
    b.presets().restoreCurrent (uid);
    for (int part = 0; part < Core::numParts; ++part) CHECK (b.presets().currentUid (part) == uid);
}
