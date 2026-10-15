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
