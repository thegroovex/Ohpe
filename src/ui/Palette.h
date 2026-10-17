// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    The Ohpe design system (see docs/DESIGN.md): a matte warm-white housing with grey recessed column
    panels, matte light keys, a deep black display and four functional accent colours - one per
    encoder - used sparingly. Functional minimalism: less, but better.
*/
namespace ohpe::ui
{
namespace colour
{
// housing (p14: matte warm white - no metal, no texture)
inline juce::Colour backdrop { 0xffd3d0ca };      ///< behind the housing (window margin)
inline juce::Colour body { 0xffefede8 };          ///< housing, matte warm white
inline juce::Colour bodyLight { 0xfff8f7f3 };     ///< housing highlight (upper edge)
inline juce::Colour bodyShade { 0xffd6d3cc };     ///< grooves, rings, shadows
inline juce::Colour panel { 0xffe3e0d9 };         ///< recessed column panels
inline juce::Colour panelEdge { 0xffcdc9c1 };     ///< the panels' upper inner edge
inline juce::Colour engraving { 0xff54524e };     ///< printed legends on the housing
inline juce::Colour ink { 0xff1c1c1b };           ///< strongest text
// p14: matte light keys on the housing
inline juce::Colour capTop { 0xfff3f1ec };
inline juce::Colour capBottom { 0xffe2dfd8 };
inline juce::Colour capEdge { 0xffc3bfb7 };
inline juce::Colour capInk { 0xff383735 };        ///< pictograms on the light keys
inline juce::Colour pill { 0xff2c2c2f };          ///< dark pill keys (LIBRARY, PRO)
inline juce::Colour pillTop { 0xff3d3d41 };
inline juce::Colour bezel { 0xff38383b };          ///< the display's dark frame (also the overlays' rim)
inline juce::Colour bezelLow { 0xff1f1f21 };
// keys
inline juce::Colour keyDark { 0xff2a2c31 };       ///< graphite key cap
inline juce::Colour keyDarkTop { 0xff34373d };
inline juce::Colour keyLight { 0xfff6f5f1 };      ///< natural keyboard key
inline juce::Colour keyLightPressed { 0xffdcd9d2 };
inline juce::Colour keyInk { 0xff54524e };        ///< p32: octave labels on the natural keys (>= 4.5:1 in both skins)
// display
inline juce::Colour display { 0xff0c0d10 };       ///< "Night" display glass
inline juce::Colour displayGrid { 0xff1c1e23 };
inline juce::Colour displayText { 0xffe9e6df };   ///< warm paper white on the display only
inline juce::Colour displayDim { 0xff6b6f78 };
// accents: one per encoder, also used for the matching value on the display
inline juce::Colour signalRed { 0xffe5243b };     ///< encoder 1
inline juce::Colour sunYellow { 0xfff6c20f };     ///< encoder 2
inline juce::Colour ultraviolet { 0xff6c4cf5 };   ///< encoder 3
inline juce::Colour rose { 0xffff6fa5 };          ///< encoder 4
/** p14: the one colour for "playing" (PLAY bar and triangle, the tape playing): a calm jade green, deep
    enough to read on the light keys at 1x, a touch brighter (playGreenLit) on the black display. */
inline juce::Colour playGreen { 0xff19a66c };
inline juce::Colour playGreenLit { 0xff3fd08e };
/** p32: highlights on the housing (lit edges, lips of the recesses): white in the light skin, a soft warm grey in the
    dark skin (no glare). */
inline juce::Colour sheen { 0xffffffff };

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

/**
    p32: skins. The colours above are the light skin's; applySkin() rewrites them (message thread, then repaint the
    editor and drop cached images). Dark ("night"): warm dark greys, no pure white anywhere, dimmed accents and light
    bars; every text colour keeps a WCAG contrast of at least 4.5 to its background (tests/plugin/SettingsUiTests.cpp).
*/
enum class Skin { Light = 0, Dark };
void applySkin (Skin s);
Skin currentSkin() noexcept;

/** p14: one radius system (design units at 1x): the housing, a matching smaller radius on every recessed
    panel and frame (columns, transport, octave, keyboard bed, display frame, header slots, overlays),
    the keys and strips that sit inside a panel a smaller one again - the curves read as concentric. */
namespace radius
{
inline constexpr float housing = 18.0f;
inline constexpr float panel = 8.0f;
inline constexpr float inset = 5.0f; ///< strips and keys inside a panel (part keys, preset strip, display glass)
} // namespace radius

/** p14: the gentle "breathing" of a running light (REC recording, PLAY playing): a smooth raised-cosine
    pulse with a period of 1.8 s between 0.35 and 1.0 - never off, no hard edge. */
inline float breath (float seconds) noexcept
{
    const float phase = std::fmod (juce::jmax (0.0f, seconds), 1.8f) / 1.8f;
    return 0.35f + 0.65f * (0.5f - 0.5f * std::cos (phase * juce::MathConstants<float>::twoPi));
}

/** Fonts: Space Grotesk (geometric, bundled, SIL OFL) and the platform monospace for values. */
juce::Font brandFont (float height, bool bold = true);
juce::Font labelFont (float height);
juce::Font monoFont (float height);
} // namespace ohpe::ui
