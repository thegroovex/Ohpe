// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UserSettings.h"
#include "model/JsonParse.h" // p63: depth-limited parse

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

using nlohmann::json;

namespace ohpe::settings
{
namespace
{
const char* const knownKeys[] = { "version", "language", "tooltips", "helpOverlay", "skin", "brightness", "animations", "windowScale", "controller", "tour" };

bool known (const std::string& k)
{
    for (const char* n : knownKeys)
        if (k == n) return true;
    return false;
}
} // namespace

const char* languageName (Language l) noexcept
{
    switch (l)
    {
        case Language::English: return "en";
        case Language::German: return "de";
        default: return "system";
    }
}

const char* skinName (Skin s) noexcept
{
    switch (s)
    {
        case Skin::Dark: return "dark";
        case Skin::Auto: return "auto";
        default: return "light";
    }
}

const char* animationsName (Animations a) noexcept
{
    switch (a)
    {
        case Animations::Reduced: return "reduced";
        case Animations::Off: return "off";
        default: return "full";
    }
}

float snapWindowScale (float s) noexcept
{
    if (! std::isfinite (s)) return 1.0f;
    float best = 1.0f, dist = 1.0e9f;
    for (float w : windowScales)
        if (std::abs (w - s) < dist) { dist = std::abs (w - s); best = w; }
    return best;
}

float animationRate (Animations a) noexcept
{
    switch (a)
    {
        case Animations::Reduced: return 0.4f;
        case Animations::Off: return 0.0f;
        default: return 1.0f;
    }
}

Skin effectiveSkin (Skin chosen, bool systemDark) noexcept
{
    if (chosen == Skin::Auto) return systemDark ? Skin::Dark : Skin::Light;
    return chosen;
}

std::string UserSettings::toJson() const
{
    json j = jsonutil::parse (extras);
    if (j.is_discarded() || ! j.is_object()) j = json::object();
    j["version"] = formats::settings; // p62
    j["language"] = languageName (language);
    j["tooltips"] = tooltips;
    j["helpOverlay"] = helpOverlay;
    j["skin"] = skinName (skin);
    j["brightness"] = std::round (brightness * 100.0f) / 100.0f;
    j["animations"] = animationsName (animations);
    j["windowScale"] = windowScale;
    j["controller"] = { { "profile", controllerProfile == ControllerGrid1 ? "grid1" : "off" },
                        { "in", controllerIn },
                        { "out", controllerOut },
                        { "master", controllerMaster == 1 ? "tempo" : "volume" },
                        { "strip", controllerStrip == 1 ? "mod" : "bend" },
                        { "auto", controllerAuto },
                        { "pattern", controllerPattern },
                        { "chosen", controllerChosen } };
    j["tour"] = { { "standalone", tourSeenStandalone }, { "plugin", tourSeenPlugin } }; // p64
    return j.dump (2);
}

UserSettings UserSettings::fromJson (const std::string& text)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    UserSettings s;
    const json j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object()) return s;
    // p62: files before 1.0 have no "version" (= 0)
    if (const auto v = j.find ("version"); v != j.end() && v->is_number_integer()) s.fileVersion = std::max (0, v->get<int>());
    else s.fileVersion = 0;
    auto str = [&j] (const char* k) {
        const auto it = j.find (k);
        return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
    };
    auto flag = [&j] (const char* k, bool fallback) {
        const auto it = j.find (k);
        return it != j.end() && it->is_boolean() ? it->get<bool>() : fallback;
    };
    auto num = [&j] (const char* k, float fallback) {
        const auto it = j.find (k);
        if (it == j.end() || ! it->is_number()) return fallback;
        const double v = it->get<double>();
        return std::isfinite (v) ? (float) v : fallback;
    };
    const auto lang = str ("language");
    if (lang == "en") s.language = Language::English;
    else if (lang == "de") s.language = Language::German;
    const auto skin = str ("skin");
    if (skin == "dark") s.skin = Skin::Dark;
    else if (skin == "auto") s.skin = Skin::Auto;
    const auto anim = str ("animations");
    if (anim == "reduced") s.animations = Animations::Reduced;
    else if (anim == "off") s.animations = Animations::Off;
    s.tooltips = flag ("tooltips", s.tooltips);
    s.helpOverlay = flag ("helpOverlay", s.helpOverlay);
    s.brightness = std::clamp (num ("brightness", s.brightness), minBrightness, 1.0f);
    s.windowScale = snapWindowScale (num ("windowScale", s.windowScale));
    if (const auto c = j.find ("controller"); c != j.end() && c->is_object())
    {
        auto sub = [&c] (const char* k) {
            const auto it = c->find (k);
            return it != c->end() && it->is_string() ? it->get<std::string>() : std::string();
        };
        s.controllerProfile = sub ("profile") == "grid1" ? ControllerGrid1 : ControllerOff;
        s.controllerIn = sub ("in");
        s.controllerOut = sub ("out");
        s.controllerMaster = sub ("master") == "tempo" ? 1 : 0;
        s.controllerStrip = sub ("strip") == "mod" ? 1 : 0;
        if (const auto a = c->find ("auto"); a != c->end() && a->is_boolean()) s.controllerAuto = a->get<bool>(); // p49
        if (const auto pat = sub ("pattern"); ! pat.empty()) s.controllerPattern = pat;
        // p53: set up by the user (else the defaults: the Grid-Controller profile, automatic ports, found when plugged in)
        if (const auto ch = c->find ("chosen"); ch != c->end() && ch->is_boolean()) s.controllerChosen = ch->get<bool>();
        if (! s.controllerChosen)
        {
            s.controllerProfile = ControllerGrid1;
            s.controllerAuto = true;
        }
    }
    if (const auto t = j.find ("tour"); t != j.end() && t->is_object()) // p64: the first-start tour
    {
        auto seen = [&t] (const char* k) {
            const auto it = t->find (k);
            return it != t->end() && it->is_boolean() && it->get<bool>();
        };
        s.tourSeenStandalone = seen ("standalone");
        s.tourSeenPlugin = seen ("plugin");
    }
    json extra = json::object();
    for (auto it = j.begin(); it != j.end(); ++it)
        if (! known (it.key())) extra[it.key()] = it.value();
    s.extras = extra.dump();
    return s;
}
catch (const std::exception&)
{
    return UserSettings {};
}

bool UserSettings::operator== (const UserSettings& o) const
{
    return language == o.language && tooltips == o.tooltips && helpOverlay == o.helpOverlay && skin == o.skin
           && std::abs (brightness - o.brightness) < 1.0e-4f && animations == o.animations && windowScale == o.windowScale
           && controllerProfile == o.controllerProfile && controllerIn == o.controllerIn && controllerOut == o.controllerOut
           && controllerMaster == o.controllerMaster && controllerStrip == o.controllerStrip
           && controllerAuto == o.controllerAuto && controllerPattern == o.controllerPattern && controllerChosen == o.controllerChosen
           && tourSeenStandalone == o.tourSeenStandalone && tourSeenPlugin == o.tourSeenPlugin
           && jsonutil::parse (extras) == jsonutil::parse (o.extras);
}

double contrastRatio (uint32_t a, uint32_t b) noexcept
{
    auto lum = [] (uint32_t c) {
        auto ch = [] (uint32_t v) {
            const double x = (double) v / 255.0;
            return x <= 0.03928 ? x / 12.92 : std::pow ((x + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * ch ((c >> 16) & 0xff) + 0.7152 * ch ((c >> 8) & 0xff) + 0.0722 * ch (c & 0xff);
    };
    const double la = lum (a), lb = lum (b);
    return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
}
} // namespace ohpe::settings
