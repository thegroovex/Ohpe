// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LivingView.h"

#include "Palette.h"
#include "mod/ModMatrix.h"
#include "plugin/Localisation.h"

#include <array>
#include <cmath>

namespace ohpe::ui::living
{
using i18n::tr;
using ohpe::living::Shape;

namespace
{
constexpr float pi = 3.14159265358979f;
constexpr float pitch = 0.38f, eye = 4.6f;

std::string partId (int part, const std::string& local) { return "p" + std::to_string (part + 1) + "." + local; }

/** The object's square inside the area (between the caption and the readouts) and its scale (pixels per unit). */
struct Frame
{
    juce::Point<float> centre;
    float scale = 1.0f;
};

Frame frameOf (juce::Rectangle<float> area)
{
    const auto body = area.withTrimmedTop (area.getHeight() * 0.1f).withTrimmedBottom (area.getHeight() * 0.16f);
    Frame f;
    f.centre = body.getCentre().translated (0.0f, body.getHeight() * 0.02f);
    f.scale = juce::jmin (body.getWidth() / (2.0f * 2.2f), body.getHeight() / (2.0f * 1.5f));
    return f;
}

struct Projected
{
    juce::Point<float> s;
    float depth = 0.0f; ///< < 0 = nearer than the centre
};

Projected project (const Frame& f, float x, float y, float z, juce::Point<float> parallax)
{
    const float cp = std::cos (pitch), sp = std::sin (pitch);
    const float fwd = y * cp - z * sp, up = y * sp + z * cp;
    const float k = eye / (eye + fwd);
    return { { f.centre.x + (x * k + parallax.x * 0.08f * fwd) * f.scale, f.centre.y - (up * k - parallax.y * 0.05f * fwd) * f.scale }, fwd };
}

/** The mesh: rings (latitude) x segments (azimuth, in the object's own frame so the lines turn with it). */
using Mesh = std::array<std::array<Projected, Shape::numSegments>, Shape::numRings>;

void buildMesh (const Shape& shape, const Frame& f, juce::Point<float> parallax, Mesh& mesh)
{
    const float stretch = shape.stretch();
    for (int i = 0; i < Shape::numRings; ++i)
    {
        const float polar = pi * (float) (i + 1) / (float) (Shape::numRings + 1);
        const float st = std::sin (polar), ct = std::cos (polar);
        for (int j = 0; j < Shape::numSegments; ++j)
        {
            const float az = 2.0f * pi * (float) j / (float) Shape::numSegments + shape.yaw();
            const float r = shape.radius (polar, az);
            mesh[(size_t) i][(size_t) j] = project (f, r * st * std::cos (az) * stretch, r * st * std::sin (az), r * ct, parallax);
        }
    }
}

juce::Rectangle<float> meshBounds (const Mesh& mesh)
{
    float x0 = 1.0e9f, y0 = 1.0e9f, x1 = -1.0e9f, y1 = -1.0e9f;
    for (const auto& ring : mesh)
        for (const auto& p : ring)
        {
            x0 = juce::jmin (x0, p.s.x); x1 = juce::jmax (x1, p.s.x);
            y0 = juce::jmin (y0, p.s.y); y1 = juce::jmax (y1, p.s.y);
        }
    return { x0, y0, juce::jmax (0.0f, x1 - x0), juce::jmax (0.0f, y1 - y0) };
}

float levelOf (float db) noexcept { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); }
} // namespace

ohpe::living::Inputs gather (Core& core, int part, const DisplayVisuals& visuals)
{
    ohpe::living::Inputs in;
    part = juce::jlimit (0, Core::numParts - 1, part);
    auto& pt = core.part (part);
    const auto& m = pt.modMatrix();
    const auto& reg = core.params();
    in.ampEnv = m.uiSourceValue (mod::SrcAmpEnv);
    in.modEnv = m.uiSourceValue (mod::SrcModEnv);

    // the LFOs count as much as they are used: the part LFO's depth, routings that take them as source or via
    std::array<float, 3> use {};
    use[0] = reg.get (partId (part, "lfo.depth"));
    float depth = 0.0f;
    for (int k = 0; k < mod::numSlots; ++k)
    {
        const auto pre = "mod.s" + std::to_string (k + 1) + ".";
        const int src = (int) std::lround (reg.get (partId (part, pre + "src")));
        const int via = (int) std::lround (reg.get (partId (part, pre + "via")));
        const int dst = (int) std::lround (reg.get (partId (part, pre + "dst")));
        const float amt = std::abs (reg.get (partId (part, pre + "amt")));
        if (dst == mod::DstOff || amt <= 0.0f) continue;
        for (int l = 0; l < 3; ++l)
            if (src == mod::SrcLfo1 + l || via == mod::SrcLfo1 + l) use[(size_t) l] += amt;
        depth += std::abs (m.uiRouteValue (k));
    }
    for (int l = 0; l < 3; ++l)
        in.lfo[(size_t) l] = m.uiSourceValue (mod::SrcLfo1 + l) * juce::jlimit (0.0f, 1.0f, 2.0f * use[(size_t) l]);
    in.modDepth = juce::jlimit (0.0f, 1.0f, depth);
    in.sounding = pt.activeVoiceCount() > 0;

    // the stereo picture: side share of the energy, correlation, level
    const float mid = visuals.midLevelDb(), side = visuals.sideLevelDb();
    in.level = levelOf (juce::jmax (mid, side));
    if (in.level > 0.0f)
    {
        const float pm = std::pow (10.0f, mid / 10.0f), ps = std::pow (10.0f, side / 10.0f);
        in.width = juce::jlimit (0.0f, 1.0f, 2.0f * ps / juce::jmax (1.0e-12f, pm + ps));
        in.correlation = visuals.correlation();
    }
    return in;
}

juce::Rectangle<float> readoutArea (juce::Rectangle<float> area)
{
    return area.withTrimmedTop (area.getHeight() * 0.86f).reduced (area.getWidth() * 0.04f, area.getHeight() * 0.015f);
}

juce::Rectangle<float> objectBounds (juce::Rectangle<float> area, const Shape& shape)
{
    const auto f = frameOf (area);
    Mesh mesh;
    buildMesh (shape, f, {}, mesh);
    // the glow and the floor shadow reach a little further; parallax shifts by a few pixels
    return meshBounds (mesh).expanded (f.scale * 0.5f, f.scale * 0.45f).withBottom (f.centre.y + f.scale * 2.0f).getIntersection (area);
}

void paint (juce::Graphics& g, juce::Rectangle<float> area, const Shape& shape, juce::Colour accent, const juce::String& source,
            juce::Point<float> parallax)
{
    const float u = area.getWidth() / 42.0f; // the visual area is 42 % of the display's width
    const auto f = frameOf (area);
    Mesh mesh;
    buildMesh (shape, f, parallax, mesh);
    const auto bounds = meshBounds (mesh);
    const float glow = shape.glow();

    // caption: the view and its source
    g.setFont (labelFont (u * 1.7f));
    g.setColour (colour::displayDim);
    g.drawText (i18n::upper (tr ("Living")) + "  " + source, area.reduced (u * 0.6f, u * 0.2f).withHeight (u * 2.4f),
                juce::Justification::topLeft, false);

    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (area.withTrimmedBottom (area.getHeight() * 0.14f).toNearestInt());

        // floor shadow (wider with the stereo width), then the glow behind the object
        const float floorY = f.centre.y + f.scale * 1.55f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (bounds.getWidth() * 0.9f, f.scale * 0.32f).withCentre ({ bounds.getCentreX(), floorY }));
        if (glow > 0.01f)
        {
            const auto c = bounds.getCentre();
            const float rad = juce::jmax (bounds.getWidth(), bounds.getHeight()) * 0.75f;
            juce::ColourGradient grad (accent.withAlpha (0.32f * glow), c, accent.withAlpha (0.0f), c.translated (rad, 0.0f), true);
            g.setGradientFill (grad);
            g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
        }

        // the mesh: three depth classes (far / middle / near), one path each - few strokes, cheap to draw
        std::array<juce::Path, 3> paths;
        auto add = [&] (const Projected& a, const Projected& b) {
            const float d = 0.5f * (a.depth + b.depth);
            const int cls = d > 0.25f ? 0 : d > -0.25f ? 1 : 2;
            paths[(size_t) cls].startNewSubPath (a.s);
            paths[(size_t) cls].lineTo (b.s);
        };
        for (int i = 0; i < Shape::numRings; ++i)
            for (int j = 0; j < Shape::numSegments; ++j)
                add (mesh[(size_t) i][(size_t) j], mesh[(size_t) i][(size_t) ((j + 1) % Shape::numSegments)]);
        for (int j = 0; j < Shape::numSegments; j += 2) // meridians: every other segment
            for (int i = 0; i + 1 < Shape::numRings; ++i)
                add (mesh[(size_t) i][(size_t) j], mesh[(size_t) (i + 1)][(size_t) j]);
        const float w = juce::jmax (0.8f, u * 0.14f);
        const float alpha[3] = { 0.22f, 0.5f, 0.9f };
        for (int c = 0; c < 3; ++c)
        {
            g.setColour (colour::displayText.withAlpha (alpha[c] * (0.65f + 0.35f * glow)));
            g.strokePath (paths[(size_t) c], juce::PathStrokeType (c == 2 ? w * 1.25f : w));
        }
        // the equator in the part's colour, the near half brighter
        juce::Path eq;
        const auto& ring = mesh[(size_t) (Shape::numRings / 2)];
        for (int j = 0; j <= Shape::numSegments; ++j)
        {
            const auto& p = ring[(size_t) (j % Shape::numSegments)];
            if (j == 0) eq.startNewSubPath (p.s);
            else eq.lineTo (p.s);
        }
        g.setColour (accent.withAlpha (0.55f + 0.45f * glow));
        g.strokePath (eq, juce::PathStrokeType (w * 1.6f));
    }

    // readouts: envelope, modulation, width - small bars in the encoder accent colours
    const auto ro = readoutArea (area);
    const juce::String names[3] = { tr ("Envelope"), tr ("Mod"), tr ("Width") };
    const float values[3] = { shape.envelope(), juce::jlimit (0.0f, 1.0f, shape.rippleValue() + 0.5f * (std::abs (shape.lobe (0)) + std::abs (shape.lobe (1)) + std::abs (shape.lobe (2))) / 1.5f),
                              shape.widthValue() };
    const float cw = ro.getWidth() / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = juce::Rectangle<float> (ro.getX() + cw * (float) i, ro.getY(), cw, ro.getHeight()).reduced (u * 0.4f, 0.0f);
        g.setFont (labelFont (u * 1.35f));
        g.setColour (colour::displayDim);
        g.drawText (i18n::upper (names[i]), cell.withHeight (cell.getHeight() * 0.55f), juce::Justification::centredLeft, true);
        const auto bar = cell.withTrimmedTop (cell.getHeight() * 0.68f).withHeight (juce::jmax (2.0f, cell.getHeight() * 0.18f));
        g.setColour (colour::displayGrid);
        g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
        g.setColour (colour::encoder (i == 2 ? 2 : i == 1 ? 1 : 0).withAlpha (0.9f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, values[i])), bar.getHeight() * 0.5f);
    }
    if (shape.isResting())
    {
        g.setFont (labelFont (u * 1.4f));
        g.setColour (colour::displayDim);
        g.drawText (tr ("Play to wake it"), area.withTrimmedTop (area.getHeight() * 0.72f).withHeight (area.getHeight() * 0.1f),
                    juce::Justification::centred, false);
    }
}
} // namespace ohpe::ui::living
