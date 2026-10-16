// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MorphView.h"

#include "Palette.h"
#include "plugin/Localisation.h"

#include <cmath>

namespace ohpe::ui::morphview
{
using i18n::tr;

namespace
{
std::string partId (int part, const char* local) { return "p" + std::to_string (part + 1) + "." + local; }

struct Layout
{
    juce::Rectangle<float> pad;
    float chipH = 0.0f, gap = 0.0f;
};

Layout layoutOf (juce::Rectangle<float> v)
{
    Layout l;
    l.chipH = juce::jmax (8.0f, v.getHeight() * 0.13f);
    l.gap = l.chipH * 0.25f;
    const float side = juce::jmax (10.0f, juce::jmin (v.getWidth() * 0.98f, v.getHeight() - 2.0f * (l.chipH + l.gap)));
    l.pad = juce::Rectangle<float> (side, side).withCentre (v.getCentre());
    return l;
}

bool isOn (Core& core, int part) { return core.params().get (partId (part, "morph.on")) >= 0.5f; }
} // namespace

juce::Rectangle<float> padArea (juce::Rectangle<float> visualArea) { return layoutOf (visualArea).pad; }

juce::Rectangle<float> cornerChip (juce::Rectangle<float> pad, int corner)
{
    const float chipH = juce::jmax (8.0f, pad.getHeight() * 0.16f), gap = chipH * 0.25f;
    const float w = pad.getWidth() * 0.485f;
    const bool top = corner == 0 || corner == 1, left = corner == 0 || corner == 2;
    const float x = left ? pad.getX() : pad.getRight() - w;
    const float y = top ? pad.getY() - gap - chipH : pad.getBottom() + gap;
    return { x, y, w, chipH };
}

int cornerAt (juce::Rectangle<float> pad, juce::Point<float> p)
{
    for (int c = 0; c < MorphSpace::numCorners; ++c)
        if (cornerChip (pad, c).contains (p)) return c;
    return -1;
}

juce::Point<float> positionAt (juce::Rectangle<float> pad, juce::Point<float> p)
{
    const float x = (p.x - pad.getX()) / juce::jmax (1.0f, pad.getWidth());
    const float y = 1.0f - (p.y - pad.getY()) / juce::jmax (1.0f, pad.getHeight());
    return { juce::jlimit (0.0f, 1.0f, x), juce::jlimit (0.0f, 1.0f, y) };
}

juce::Point<float> pointOf (juce::Rectangle<float> pad, float x, float y)
{
    return { pad.getX() + x * pad.getWidth(), pad.getBottom() - y * pad.getHeight() };
}

juce::String cornerTitle (Core& core, int part, int corner)
{
    if (! core.morph().cornerSet (part, corner)) return tr ("Own sound");
    const auto n = juce::String::fromUTF8 (core.morph().cornerLabel (part, corner).c_str());
    return n.isNotEmpty() ? n : tr ("Corner") + " " + MorphSpace::cornerName (corner);
}

std::vector<std::string> morphParams (int part)
{
    return { partId (part, "morph.on"), partId (part, "morph.x"), partId (part, "morph.y"), partId (part, "morph.glide"),
             partId (part, "morph.xsrc"), partId (part, "morph.ysrc"), partId (part, "mpe"), partId (part, "mpe.smooth") };
}

std::vector<std::string> morphTitles() { return { "Morph", "Control" }; }

void paint (juce::Graphics& g, juce::Rectangle<float> visualArea, Core& core, int part, float time)
{
    const auto lay = layoutOf (visualArea);
    const auto pad = lay.pad;
    auto& m = core.morph();
    const bool on = isOn (core, part);
    const auto& reg = core.params();
    const float px = reg.get (partId (part, "morph.x")), py = reg.get (partId (part, "morph.y"));
    const bool live = m.uiActive (part);
    const float x = live ? m.uiX (part) : px, y = live ? m.uiY (part) : py;
    const auto w = MorphSpace::weights (x, y);
    const float rad = pad.getWidth() * 0.045f;
    const float alpha = on ? 1.0f : 0.45f;

    // the pad: a recessed field with a fine dot grid
    g.setColour (colour::displayGrid.withAlpha (0.9f));
    g.fillRoundedRectangle (pad, rad);
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (pad, rad);
        g.reduceClipRegion (clip);
        // the corners' influence: a soft glow in each corner's colour, as strong as its weight
        static const juce::Point<float> at[MorphSpace::numCorners] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
        for (int c = 0; c < MorphSpace::numCorners; ++c)
        {
            const auto p = pointOf (pad, at[c].x, at[c].y);
            const auto col = colour::encoder (c);
            const float a = (m.cornerSet (part, c) ? 0.55f : 0.25f) * w[(size_t) c] * alpha;
            juce::ColourGradient grad (col.withAlpha (a), p.x, p.y, col.withAlpha (0.0f), p.x + pad.getWidth() * 0.75f, p.y, true);
            g.setGradientFill (grad);
            g.fillRect (pad);
        }
        g.setColour (colour::displayDim.withAlpha (0.35f * alpha));
        const float dot = juce::jmax (1.0f, pad.getWidth() * 0.008f);
        for (int i = 1; i < 8; ++i)
            for (int j = 1; j < 8; ++j)
                g.fillEllipse (pad.getX() + pad.getWidth() * (float) i / 8.0f - dot * 0.5f, pad.getY() + pad.getHeight() * (float) j / 8.0f - dot * 0.5f, dot, dot);
    }

    // edges: solid = the two corners morph their parameters (one sound), dotted = parallel sounds cross-fade
    const float edgeW = juce::jmax (1.0f, pad.getWidth() * 0.008f);
    auto edge = [&] (int c1, int c2, juce::Point<float> a, juce::Point<float> b) {
        const bool same = m.laneOfCorner (part, c1) == m.laneOfCorner (part, c2);
        g.setColour (colour::displayText.withAlpha ((same ? 0.45f : 0.3f) * alpha));
        if (same) g.drawLine ({ a, b }, edgeW);
        else
        {
            const float dash[] = { edgeW * 2.0f, edgeW * 3.0f };
            g.drawDashedLine ({ a, b }, dash, 2, edgeW);
        }
    };
    const auto inner = pad.reduced (rad * 0.5f);
    edge (0, 1, inner.getTopLeft(), inner.getTopRight());
    edge (2, 3, inner.getBottomLeft(), inner.getBottomRight());
    edge (0, 2, inner.getTopLeft(), inner.getBottomLeft());
    edge (1, 3, inner.getTopRight(), inner.getBottomRight());

    // crosshair, the target (pad value) and the glided / modulated position
    const auto pos = pointOf (pad, x, y);
    g.setColour (colour::displayDim.withAlpha (0.5f * alpha));
    g.drawLine (pad.getX(), pos.y, pad.getRight(), pos.y, 1.0f);
    g.drawLine (pos.x, pad.getY(), pos.x, pad.getBottom(), 1.0f);
    const float r = pad.getWidth() * 0.04f;
    if (live && (std::abs (px - x) > 0.01f || std::abs (py - y) > 0.01f))
    {
        const auto t = pointOf (pad, px, py);
        g.setColour (colour::displayText.withAlpha (0.5f));
        g.drawEllipse (t.x - r, t.y - r, 2.0f * r, 2.0f * r, edgeW);
    }
    const float pulse = on ? 0.75f + 0.25f * std::sin (time * 3.0f) : 0.0f;
    g.setColour (colour::displayText.withAlpha (0.18f * pulse));
    g.fillEllipse (pos.x - 2.2f * r, pos.y - 2.2f * r, 4.4f * r, 4.4f * r);
    g.setColour (on ? colour::displayText : colour::displayDim);
    g.fillEllipse (pos.x - r, pos.y - r, 2.0f * r, 2.0f * r);
    g.setColour (colour::display);
    g.fillEllipse (pos.x - r * 0.35f, pos.y - r * 0.35f, r * 0.7f, r * 0.7f);

    // corner chips: letter in the corner's colour, the sound's name, its share as a thin bar
    for (int c = 0; c < MorphSpace::numCorners; ++c)
    {
        const auto chip = cornerChip (pad, c);
        const bool set = m.cornerSet (part, c);
        const auto col = colour::encoder (c);
        const float cr = chip.getHeight() * 0.3f;
        g.setColour (colour::displayGrid.brighter (set ? 0.25f : 0.05f).withAlpha (alpha));
        g.fillRoundedRectangle (chip, cr);
        auto letter = chip.withWidth (chip.getHeight()).reduced (chip.getHeight() * 0.14f);
        if (set) { g.setColour (col.withAlpha (alpha)); g.fillRoundedRectangle (letter, cr * 0.7f); }
        else { g.setColour (col.withAlpha (0.8f * alpha)); g.drawRoundedRectangle (letter, cr * 0.7f, edgeW); }
        g.setColour (set ? colour::display : col.withAlpha (alpha));
        g.setFont (brandFont (letter.getHeight() * 0.8f));
        g.drawText (MorphSpace::cornerName (c), letter, juce::Justification::centred, false);
        auto text = chip.withTrimmedLeft (chip.getHeight() * 1.05f).withTrimmedRight (chip.getHeight() * 0.2f);
        g.setColour ((set ? colour::displayText : colour::displayDim).withAlpha (alpha));
        g.setFont (labelFont (chip.getHeight() * 0.48f));
        g.drawText (cornerTitle (core, part, c), text.withTrimmedBottom (chip.getHeight() * 0.12f), juce::Justification::centredLeft, true);
        const auto bar = juce::Rectangle<float> (text.getX(), chip.getBottom() - chip.getHeight() * 0.2f, text.getWidth() * w[(size_t) c], juce::jmax (1.0f, chip.getHeight() * 0.07f));
        g.setColour (col.withAlpha (0.9f * alpha));
        g.fillRect (bar);
    }

    if (! on)
    {
        g.setColour (colour::display.withAlpha (0.55f));
        const auto label = pad.withSizeKeepingCentre (pad.getWidth() * 0.8f, pad.getHeight() * 0.16f);
        g.fillRoundedRectangle (label, label.getHeight() * 0.3f);
        g.setColour (colour::displayText);
        g.setFont (labelFont (label.getHeight() * 0.5f));
        g.drawText (i18n::upper (tr ("Morph off")), label, juce::Justification::centred, true);
    }
}

juce::String statusLine (Core& core, int part)
{
    auto& m = core.morph();
    if (! isOn (core, part)) return tr ("Morph off - switch it on with the first encoder");
    if (! m.anyCorner (part)) return tr ("Click a corner to give it a sound");
    const int lanes = m.numLanes (part);
    if (lanes <= 1) return tr ("One sound - the pad morphs its parameters");
    return juce::String (lanes) + " " + tr ("sounds in parallel") + " - " + juce::String (m.uiLanesAwake (part)) + " " + tr ("playing");
}
} // namespace ohpe::ui::morphview
