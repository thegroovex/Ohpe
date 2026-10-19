// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p49: the Grid-Controller in a plug-in - one owner per process (most recent claim, hand-over), the automatic port
// choice by name pattern, the settings round trip.
#include <doctest/doctest.h>

#include "control/ControllerPorts.h"
#include "settings/UserSettings.h"

using namespace ohpe;
namespace g = ohpe::gridctl;

TEST_CASE ("p49 grid controller in a plug-in: one owner - the most recent claim among the enabled ones")
{
    g::Ownership t;
    int a = 0, b = 0, c = 0;
    CHECK (t.owner() == nullptr);
    t.enrol (&a);
    CHECK (t.owner() == nullptr); // registered, profile off: nobody owns it
    t.setEnabled (&a, true);
    CHECK (t.owns (&a));
    t.enrol (&b);
    t.setEnabled (&b, true);
    CHECK (t.owns (&a)); // a new instance does not take over by itself
    CHECK (t.ownedElsewhere (&b));
    t.claim (&b); // its window opened / clicked / "Take over here"
    CHECK (t.owns (&b));
    CHECK_FALSE (t.owns (&a));
    CHECK (t.ownedElsewhere (&a));
    t.enrol (&c); // enabled later: still not the owner
    CHECK (t.owns (&b));
    t.claim (&c); // claimed while disabled: owner as soon as it is enabled
    CHECK (t.owns (&b));
    t.setEnabled (&c, true);
    CHECK (t.owns (&c));
    const auto gen = t.generation();
    t.remove (&c); // instance gone: the next most recent takes it
    CHECK (t.generation() > gen);
    CHECK (t.owns (&b));
    t.setEnabled (&b, false);
    CHECK (t.owns (&a));
    t.remove (&a);
    t.remove (&b);
    CHECK (t.owner() == nullptr);
    CHECK (t.size() == 0);
    t.claim (&a); // unknown: nothing happens
    t.remove (&a);
    CHECK (t.size() == 0);
    CHECK_FALSE (t.owns (nullptr));
    // the process-wide table is one object
    CHECK (&g::Ownership::shared() == &g::Ownership::shared());
}

TEST_CASE ("p49 grid controller in a plug-in: automatic ports by name pattern")
{
    const std::string pat = settings::defaultControllerPattern;
    CHECK (g::portMatches ("Pad Grid User Port", pat));
    CHECK (g::portMatches ("MIDIIN2 (Pad Grid)", pat));
    CHECK (g::portMatches ("midiout2 (Pad Grid)", pat));
    CHECK_FALSE (g::portMatches ("Pad Grid Live Port", pat));
    CHECK_FALSE (g::portMatches ("Pad Grid User Port", ""));
    CHECK_FALSE (g::portMatches ("Pad Grid User Port", " ; , "));
    CHECK (g::portMatches ("Pad Grid User Port", "nothing, grid"));
    CHECK (g::portKey ("MIDIIN2 (Pad Grid)") == g::portKey ("MIDIOUT2 (Pad Grid)"));
    CHECK (g::portKey ("Pad Grid User In") == g::portKey ("Pad Grid User Out"));
    CHECK (g::portKey ("Pad Grid User Port") != g::portKey ("Other Box User Port"));

    // macOS / Linux style: the live port first, then the user port; another device with a user port too
    const std::vector<std::string> ins { "Keys In", "Pad Grid Live Port", "Pad Grid User Port" };
    const std::vector<std::string> outs { "Keys Out", "Other Box User Port", "Pad Grid Live Port", "Pad Grid User Port" };
    auto pick = g::autoPorts (ins, outs, pat);
    REQUIRE (pick.found());
    CHECK (ins[(size_t) pick.in] == "Pad Grid User Port");
    CHECK (outs[(size_t) pick.out] == "Pad Grid User Port"); // the same device, not the first matching output

    // Windows style: the second port of the device is the user port
    const std::vector<std::string> winIn { "Pad Grid", "MIDIIN2 (Pad Grid)" };
    const std::vector<std::string> winOut { "Pad Grid", "MIDIOUT2 (Pad Grid)" };
    pick = g::autoPorts (winIn, winOut, pat);
    REQUIRE (pick.found());
    CHECK (pick.in == 1);
    CHECK (pick.out == 1);

    // no partner of the same key: first matching input and output
    pick = g::autoPorts ({ "A User In" }, { "B User Out" }, pat);
    CHECK (pick.in == 0);
    CHECK (pick.out == 0);
    // nothing (no ports at all, as in CI; or only one side)
    CHECK_FALSE (g::autoPorts ({}, {}, pat).found());
    CHECK_FALSE (g::autoPorts ({ "Pad Grid User Port" }, {}, pat).found());
    CHECK_FALSE (g::autoPorts ({ "Keys In" }, { "Keys Out" }, pat).found());
    CHECK (g::autoPorts ({ "Keys In" }, { "Keys Out" }, pat).in == -1);
}

TEST_CASE ("p49 grid controller in a plug-in: settings - automatic ports on by default, round trip")
{
    using settings::UserSettings;
    UserSettings s;
    CHECK (s.controllerAuto);
    CHECK (s.controllerPattern == settings::defaultControllerPattern);
    CHECK (s.controllerProfile == settings::ControllerOff);
    CHECK (UserSettings::fromJson (s.toJson()) == s);
    s.controllerAuto = false;
    s.controllerPattern = "Grid; Port 2";
    const auto back = UserSettings::fromJson (s.toJson());
    CHECK (back == s);
    CHECK_FALSE (back.controllerAuto);
    CHECK (back.controllerPattern == "Grid; Port 2");
    // files of 0.6 (no "auto" / "pattern"): the defaults
    const auto old = UserSettings::fromJson (R"({"controller":{"profile":"grid1","in":"A","out":"B"}})");
    CHECK (old.controllerAuto);
    CHECK (old.controllerPattern == settings::defaultControllerPattern);
    const auto junk = UserSettings::fromJson (R"({"controller":{"auto":"yes","pattern":7}})");
    CHECK (junk.controllerAuto);
    CHECK (junk.controllerPattern == settings::defaultControllerPattern);
}
