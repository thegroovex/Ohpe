// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/FormatVersions.h"

#include <cstdint>
#include <string>

namespace ohpe::settings
{
/** p32: UI language: follow the system, or a fixed language. */
enum class Language { System = 0, English, German };
/** p32: the housing's skin. Auto follows the operating system's light / dark appearance. */
enum class Skin { Light = 0, Dark, Auto };
/** p32: how much the UI moves (light bars breathing, 3D pulse, display visuals). */
enum class Animations { Full = 0, Reduced, Off };

/** p49: the default name pattern of "Choose ports automatically": parts separated by ';', any part found in a port
    name (case does not matter) picks it - the controller's user port by its usual names on macOS / Linux and Windows. */
inline constexpr const char* defaultControllerPattern = "User; MIDIIN2; MIDIOUT2";

/**
    p32: the user's settings - per user (this computer), never per project or preset:
    <user data>/groovex/Ohpe/Settings/settings.json (next to update.json, which keeps the updater's own settings).
    None of them is a host parameter: nothing here is automatable or stored in a session.

    Unknown keys of the file are kept and written back unchanged (newer versions and other features may add their own).

    p62: the file carries "version" (formats::settings). Files without it (before 1.0) are version 0 and are migrated
    on read (the 0.6.3 controller defaults, see controllerChosen); a newer version's keys this build does not know
    are kept.
*/
struct UserSettings
{
    Language language = Language::System;
    bool tooltips = true;          ///< tooltips on keys, encoders and the display
    bool helpOverlay = false;      ///< the help overlay: names and short explanations over the plate
    Skin skin = Skin::Light;
    float brightness = 1.0f;       ///< display brightness 0.4 .. 1.0 (dims the dark display, lowers glare at night)
    Animations animations = Animations::Full;
    float windowScale = 1.0f;      ///< default window size: one of windowScales
    /** p46: the external controller of the standalone app (Settings > MIDI > Controller). JSON "controller":
        { "profile": "off" | "grid1", "in": port name, "out": port name, "master": "volume" | "tempo",
          "strip": "bend" | "mod" }. */
    int controllerProfile = 1;     ///< ControllerProfile (p53: the Grid-Controller by default - it connects when found)
    std::string controllerIn, controllerOut;
    int controllerMaster = 0;      ///< 0 = the master encoder sets the volume, 1 = the tempo
    int controllerStrip = 0;       ///< 0 = the touch strip bends, 1 = it is the mod wheel
    /** p49: in a plug-in, pick the controller's ports by name (controllerPattern) instead of controllerIn / Out.
        JSON "controller": { "auto": true, "pattern": "User; MIDIIN2; MIDIOUT2" } (the pattern is edited in the file). */
    bool controllerAuto = true;
    std::string controllerPattern = defaultControllerPattern;
    /** p53: the user has set the controller up in the settings (JSON "controller": { "chosen": true }). Files without
        it (0.6.2 and older) start with the defaults of 0.6.3: the Grid-Controller profile and automatic ports. */
    bool controllerChosen = false;
    std::string extras = "{}";     ///< unknown keys (a JSON object)
    /** p62: "version" of the file it was read from (0 = before 1.0, no version field); written as formats::settings.
        Not part of operator==. */
    int fileVersion = formats::settings;

    std::string toJson() const;
    /** Reads the JSON; missing, unknown or broken values keep the defaults. */
    static UserSettings fromJson (const std::string& json);
    bool operator== (const UserSettings& o) const;
    bool operator!= (const UserSettings& o) const { return ! (*this == o); }
};

/** p46: controller profiles (append-only). */
enum ControllerProfile { ControllerOff = 0, ControllerGrid1, numControllerProfiles };

constexpr float minBrightness = 0.4f;
constexpr float windowScales[] = { 0.75f, 1.0f, 1.25f, 1.5f };

/** The nearest allowed window scale. */
float snapWindowScale (float s) noexcept;
/** Animation speed factor: 1 (full), 0.4 (reduced), 0 (off - lights stand still, fully lit). */
float animationRate (Animations a) noexcept;
/** The skin actually shown: Auto resolves to Dark when the system is dark. */
Skin effectiveSkin (Skin chosen, bool systemDark) noexcept;

const char* languageName (Language l) noexcept;
const char* skinName (Skin s) noexcept;
const char* animationsName (Animations a) noexcept;

/** WCAG contrast ratio (1 .. 21) of two opaque colours given as 0xAARRGGBB (alpha ignored). */
double contrastRatio (uint32_t a, uint32_t b) noexcept;
} // namespace ohpe::settings
