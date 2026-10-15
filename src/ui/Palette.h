// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    The Ohpe design system (see docs/DESIGN.md): light anodised-aluminium body, graphite keys,
    a deep display, four functional accent colours - one per encoder - used sparingly.
*/
namespace ohpe::ui
{
namespace colour
{
// body
inline const juce::Colour body { 0xffd8dadd };          ///< "Alu 200" anodised light grey
inline const juce::Colour bodyLight { 0xffe6e8ea };     ///< "Alu 100" highlight
inline const juce::Colour bodyShade { 0xffc4c7cb };     ///< "Alu 300" grooves, shadows
inline const juce::Colour engraving { 0xff5d6168 };     ///< printed legends on the body
inline const juce::Colour ink { 0xff1d1f23 };           ///< strongest text
// keys
inline const juce::Colour keyDark { 0xff2a2c31 };       ///< graphite key cap
inline const juce::Colour keyDarkTop { 0xff34373d };
inline const juce::Colour keyLight { 0xffeef0f1 };      ///< natural keyboard key
inline const juce::Colour keyLightPressed { 0xffcfd2d6 };
// display
inline const juce::Colour display { 0xff0c0d10 };       ///< "Night" display glass
inline const juce::Colour displayGrid { 0xff1c1e23 };
inline const juce::Colour displayText { 0xffe9e6df };   ///< warm paper white on the display only
inline const juce::Colour displayDim { 0xff6b6f78 };
// accents: one per encoder, also used for the matching value on the display
inline const juce::Colour signalRed { 0xffe5243b };     ///< encoder 1
inline const juce::Colour sunYellow { 0xfff6c20f };     ///< encoder 2
inline const juce::Colour ultraviolet { 0xff6c4cf5 };   ///< encoder 3
inline const juce::Colour rose { 0xffff6fa5 };          ///< encoder 4

inline juce::Colour encoder (int i)
{
    switch (i & 3)
    {
        case 0: return signalRed;
        case 1: return sunYellow;
        case 2: return ultraviolet;
        default: return rose;
    }
}
} // namespace colour

/** Fonts: Space Grotesk (geometric, bundled, SIL OFL) and the platform monospace for values. */
juce::Font brandFont (float height, bool bold = true);
juce::Font labelFont (float height);
juce::Font monoFont (float height);
} // namespace ohpe::ui
