// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p32: the user's settings - defaults, JSON round trip, broken input, clamping, unknown keys kept, contrast helper.
#include "settings/UserSettings.h"

#include <doctest/doctest.h>

#include <string>

using namespace ohpe::settings;

TEST_CASE ("user settings: defaults")
{
    const UserSettings s;
    CHECK (s.language == Language::System);
    CHECK (s.tooltips);
    CHECK_FALSE (s.helpOverlay);
    CHECK (s.skin == Skin::Light);
    CHECK (s.brightness == doctest::Approx (1.0f));
    CHECK (s.animations == Animations::Full);
    CHECK (s.windowScale == doctest::Approx (1.0f));
    CHECK (UserSettings::fromJson ("") == s);
    CHECK (UserSettings::fromJson ("junk {") == s);
    CHECK (UserSettings::fromJson ("[1,2]") == s);
    CHECK (UserSettings::fromJson ("{}") == s);
}

TEST_CASE ("user settings: round trip")
{
    UserSettings s;
    s.language = Language::German;
    s.tooltips = false;
    s.helpOverlay = true;
    s.skin = Skin::Dark;
    s.brightness = 0.7f;
    s.animations = Animations::Reduced;
    s.windowScale = 1.25f;
    const auto back = UserSettings::fromJson (s.toJson());
    CHECK (back == s);
    s.skin = Skin::Auto;
    s.animations = Animations::Off;
    s.language = Language::English;
    CHECK (UserSettings::fromJson (s.toJson()) == s);
}

TEST_CASE ("user settings: broken and out-of-range values keep defaults or are clamped")
{
    const auto s = UserSettings::fromJson (R"({"language":"fr","skin":7,"brightness":5,"animations":"wild","windowScale":3.3,"tooltips":"yes"})");
    CHECK (s.language == Language::System);
    CHECK (s.skin == Skin::Light);
    CHECK (s.brightness == doctest::Approx (1.0f));
    CHECK (s.animations == Animations::Full);
    CHECK (s.windowScale == doctest::Approx (1.5f));
    CHECK (s.tooltips);
    CHECK (UserSettings::fromJson (R"({"brightness":0.0})").brightness == doctest::Approx (minBrightness));
    CHECK (UserSettings::fromJson (R"({"brightness":-1e300})").brightness == doctest::Approx (minBrightness));
    CHECK (snapWindowScale (0.9f) == doctest::Approx (1.0f));
    CHECK (snapWindowScale (0.1f) == doctest::Approx (0.75f));
}

TEST_CASE ("user settings: unknown keys survive a round trip")
{
    auto s = UserSettings::fromJson (R"({"skin":"dark","expression":{"curve":2},"future":true})");
    CHECK (s.skin == Skin::Dark);
    s.brightness = 0.5f;
    const auto text = s.toJson();
    CHECK (text.find ("\"expression\"") != std::string::npos);
    CHECK (text.find ("\"future\"") != std::string::npos);
    const auto back = UserSettings::fromJson (text);
    CHECK (back == s);
}

TEST_CASE ("user settings: helpers")
{
    CHECK (animationRate (Animations::Full) == doctest::Approx (1.0f));
    CHECK (animationRate (Animations::Reduced) > 0.0f);
    CHECK (animationRate (Animations::Reduced) < 1.0f);
    CHECK (animationRate (Animations::Off) == 0.0f);
    CHECK (effectiveSkin (Skin::Auto, true) == Skin::Dark);
    CHECK (effectiveSkin (Skin::Auto, false) == Skin::Light);
    CHECK (effectiveSkin (Skin::Dark, false) == Skin::Dark);
    CHECK (contrastRatio (0xff000000u, 0xffffffffu) == doctest::Approx (21.0).epsilon (0.001));
    CHECK (contrastRatio (0xff777777u, 0xff777777u) == doctest::Approx (1.0));
    CHECK (contrastRatio (0xffffffffu, 0xff000000u) == contrastRatio (0xff000000u, 0xffffffffu));
}

TEST_CASE ("p64 user settings: the first-start tour flags - absent = not seen, round trip, broken values ignored")
{
    const auto fresh = UserSettings::fromJson ("{}");
    CHECK_FALSE (fresh.tourSeenStandalone);
    CHECK_FALSE (fresh.tourSeenPlugin);
    const auto old = UserSettings::fromJson (R"({"version":1,"skin":"dark"})"); // a 1.0 file before the tour
    CHECK_FALSE (old.tourSeenStandalone);
    CHECK_FALSE (old.tourSeenPlugin);
    UserSettings s;
    s.tourSeenPlugin = true;
    const auto text = s.toJson();
    CHECK (text.find ("\"tour\"") != std::string::npos);
    CHECK (text.find ("\"version\": 1") != std::string::npos); // additive key: no version bump
    const auto back = UserSettings::fromJson (text);
    CHECK (back == s);
    CHECK (back.tourSeenPlugin);
    CHECK_FALSE (back.tourSeenStandalone);
    CHECK (back.extras.find ("tour") == std::string::npos); // a known key, not kept twice
    const auto broken = UserSettings::fromJson (R"({"tour":{"standalone":"yes","plugin":3}})");
    CHECK_FALSE (broken.tourSeenStandalone);
    CHECK_FALSE (broken.tourSeenPlugin);
    CHECK_FALSE (UserSettings::fromJson (R"({"tour":true})").tourSeenStandalone);
}
