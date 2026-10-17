// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Palette.h"

#include "BinaryData.h"

namespace ohpe::ui
{
namespace
{
juce::Typeface::Ptr typeface (bool bold)
{
    static juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGrotesk500_ttf,
                                                                                    (size_t) BinaryData::SpaceGrotesk500_ttfSize);
    static juce::Typeface::Ptr heavy = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGrotesk700_ttf,
                                                                                  (size_t) BinaryData::SpaceGrotesk700_ttfSize);
    return bold ? heavy : regular;
}
} // namespace

namespace
{
struct SkinColours
{
    juce::uint32 backdrop, body, bodyLight, bodyShade, panel, panelEdge, engraving, ink, capTop, capBottom, capEdge, capInk, pill, pillTop,
        bezel, bezelLow, keyDark, keyDarkTop, keyLight, keyLightPressed, display, displayGrid, displayText, displayDim, signalRed,
        sunYellow, ultraviolet, rose, playGreen, playGreenLit, sheen, keyInk;
};

// the light skin: the p14 design (matte warm white housing)
constexpr SkinColours lightSkin { 0xffd3d0ca, 0xffefede8, 0xfff8f7f3, 0xffd6d3cc, 0xffe3e0d9, 0xffcdc9c1, 0xff54524e, 0xff1c1c1b,
                                  0xfff3f1ec, 0xffe2dfd8, 0xffc3bfb7, 0xff383735, 0xff2c2c2f, 0xff3d3d41, 0xff38383b, 0xff1f1f21,
                                  0xff2a2c31, 0xff34373d, 0xfff6f5f1, 0xffdcd9d2, 0xff0c0d10, 0xff1c1e23, 0xffe9e6df, 0xff6b6f78,
                                  0xffe5243b, 0xfff6c20f, 0xff6c4cf5, 0xffff6fa5, 0xff19a66c, 0xff3fd08e, 0xffffffff, 0xff54524e };
// p32: the night skin - warm dark greys, matte; text never pure white; accents and light bars dimmed
constexpr SkinColours darkSkin { 0xff141312, 0xff2a2826, 0xff312e2b, 0xff1d1b19, 0xff221f1d, 0xff191716, 0xffa59e94, 0xffddd6ca,
                                 0xff3a3734, 0xff312e2b, 0xff1a1817, 0xffcfc8bc, 0xff1a1918, 0xff262422, 0xff1d1c1b, 0xff121110,
                                 0xff1b1a19, 0xff262422, 0xff8f897f, 0xff847e74, 0xff0b0a09, 0xff1d1b19, 0xffd8d1c4, 0xff847e75,
                                 0xffcf3a47, 0xffd3a822, 0xff8a74e8, 0xffd96a93, 0xff23946a, 0xff38b07e, 0xff48443f, 0xff0b0a09 };

Skin skinNow = Skin::Light;
} // namespace

void applySkin (Skin s)
{
    const auto& c = s == Skin::Dark ? darkSkin : lightSkin;
    skinNow = s;
    using juce::Colour;
    colour::backdrop = Colour (c.backdrop);       colour::body = Colour (c.body);
    colour::bodyLight = Colour (c.bodyLight);     colour::bodyShade = Colour (c.bodyShade);
    colour::panel = Colour (c.panel);             colour::panelEdge = Colour (c.panelEdge);
    colour::engraving = Colour (c.engraving);     colour::ink = Colour (c.ink);
    colour::capTop = Colour (c.capTop);           colour::capBottom = Colour (c.capBottom);
    colour::capEdge = Colour (c.capEdge);         colour::capInk = Colour (c.capInk);
    colour::pill = Colour (c.pill);               colour::pillTop = Colour (c.pillTop);
    colour::bezel = Colour (c.bezel);             colour::bezelLow = Colour (c.bezelLow);
    colour::keyDark = Colour (c.keyDark);         colour::keyDarkTop = Colour (c.keyDarkTop);
    colour::keyLight = Colour (c.keyLight);       colour::keyLightPressed = Colour (c.keyLightPressed);
    colour::display = Colour (c.display);         colour::displayGrid = Colour (c.displayGrid);
    colour::displayText = Colour (c.displayText); colour::displayDim = Colour (c.displayDim);
    colour::signalRed = Colour (c.signalRed);     colour::sunYellow = Colour (c.sunYellow);
    colour::ultraviolet = Colour (c.ultraviolet); colour::rose = Colour (c.rose);
    colour::playGreen = Colour (c.playGreen);     colour::playGreenLit = Colour (c.playGreenLit);
    colour::sheen = Colour (c.sheen);             colour::keyInk = Colour (c.keyInk);
}

Skin currentSkin() noexcept { return skinNow; }

juce::Font brandFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (typeface (bold)).withHeight (height));
}

juce::Font labelFont (float height) { return brandFont (height, false); }

juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}
} // namespace ohpe::ui
