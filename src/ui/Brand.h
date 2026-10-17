// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

/**
    p47: the Ohpe wordmark and the logo mark, drawn as vectors (header-only, used by the housing and the About page).

    Wordmark: "OHPE" in the brand font with the red dot after it set like a full stop - the dot's bottom sits on the
    text's baseline, a short gap after the E (the dot never floats at mid-height).

    Logo mark: the mark of the app icon (resources/icon/ohpe-icon.svg) without its tile - a ring with a small gap at
    the top holding the red dot, two periods of a sine inside that touch the ring left and right. Drawn with
    juce::Path from the same geometry as the SVG master (ring r = 300, sine amplitude 120, period 300 on the 1024 grid),
    so it stays crisp at every scale. Lines in `ink`, dot in `dot`; the line weight grows a little at small sizes.
*/
namespace ohpe::ui::brand
{
/** Baseline of a single line of `font` vertically centred on `centreY` (the way drawText centres a line). */
inline float baselineCentredOn (const juce::Font& font, float centreY)
{
    return centreY - font.getHeight() * 0.5f + font.getAscent();
}

/** Width of "OHPE" plus its dot (for layout). */
inline float wordmarkWidth (const juce::Font& font)
{
    const float h = font.getHeight();
    return juce::GlyphArrangement::getStringWidth (font, "OHPE") + h * 0.08f + h * 0.19f;
}

/** Draws "OHPE" from `x` on `baseline` and the dot after it, its bottom on the baseline. Returns the right edge. */
inline float drawWordmark (juce::Graphics& g, const juce::Font& font, float x, float baseline, juce::Colour text, juce::Colour dot)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, "OHPE", x, baseline);
    g.setColour (text);
    ga.draw (g);
    // the dot like a full stop: a short gap after the E's advance, diameter about the bold stroke, bottom on the baseline
    const float h = font.getHeight();
    const float d = h * 0.19f, dx = x + juce::GlyphArrangement::getStringWidth (font, "OHPE") + h * 0.08f;
    g.setColour (dot);
    g.fillEllipse (dx, baseline - d, d, d);
    return dx + d;
}

/** The sine of the mark in unit coordinates (ring radius 1, centre 0/0): y = -0.4 sin (2 pi (x + 1)), x = -1..1,
    one cubic per quarter period - the same control points as the SVG master. */
inline juce::Path markSine()
{
    juce::Path p;
    const float q = 0.25f, c = q / 3.0f, a = 0.4f, k = 0.2093f; // k: the master's 62.8 / 300
    p.startNewSubPath (-1.0f, 0.0f);
    float x = -1.0f;
    for (int i = 0; i < 4; ++i) // four half periods: up, down, up, down
    {
        const float s = (i % 2 == 0) ? -1.0f : 1.0f; // screen y: up is negative
        p.cubicTo (x + c, s * k, x + q - c, s * a, x + q, s * a);
        p.cubicTo (x + q + c, s * a, x + 2 * q - c, s * k, x + 2 * q, 0.0f);
        x += 2 * q;
    }
    return p;
}

/** Draws the logo mark centred in `area` (the ring's outer diameter fills its smaller side). */
inline void drawMark (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, juce::Colour dot)
{
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    // line weight: about the master's 26 / 300 of the radius, a touch heavier to stand next to the bold wordmark, heavier when small (stays >= ~1.6 px)
    const float stroke = juce::jmax (size * 0.09f, juce::jmin (1.6f, size * 0.12f));
    const float r = (size - stroke) * 0.5f;
    const auto c = area.getCentre();
    const float gap = 0.30f; // half-angle of the gap at the top (radians, ~17 degrees: room for the dot at small sizes)

    juce::Path ring;
    ring.addCentredArc (c.x, c.y, r, r, 0.0f, gap, juce::MathConstants<float>::twoPi - gap, true);
    juce::Path sine = markSine();
    sine.applyTransform (juce::AffineTransform::scale (r).translated (c.x, c.y));

    g.setColour (ink);
    const juce::PathStrokeType st (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.strokePath (ring, st);
    g.strokePath (sine, juce::PathStrokeType (stroke * 0.93f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float d = juce::jmax (stroke * 1.6f, size * 0.135f);
    g.setColour (dot);
    g.fillEllipse (c.x - d * 0.5f, c.y - r - d * 0.5f, d, d);
}
} // namespace ohpe::ui::brand
