// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SpaceView.h"

#include "Palette.h"
#include "fx/Space3D.h"
#include "plugin/Localisation.h"

#include <algorithm>
#include <cmath>

namespace ohpe::ui::space3d
{
using i18n::tr;

namespace
{
constexpr float pi = 3.14159265358979f;

std::string partPrefix (int part) { return "p" + std::to_string (juce::jlimit (0, Core::numParts - 1, part) + 1) + "."; }

/** Camera behind and above the listener, looking ahead: front recedes into the display. */
struct Camera
{
    juce::Point<float> centre;
    float radius = 100.0f, yaw = 0.0f, pitch = 0.42f, eye = 3.2f;

    struct P
    {
        juce::Point<float> s;
        float depth = 0.0f, scale = 1.0f;
    };
    P project (space::Vec3 v) const noexcept // v in sphere units (1 = the outer ring)
    {
        const float cy = std::cos (yaw), sy = std::sin (yaw);
        const float x = v.x * cy - v.y * sy, y = v.x * sy + v.y * cy, z = v.z;
        const float cp = std::cos (pitch), sp = std::sin (pitch);
        const float fwd = y * cp - z * sp, up = y * sp + z * cp;
        const float k = eye / (eye + fwd);
        return { { centre.x + x * k * radius, centre.y - up * k * radius }, fwd, k };
    }
};

/** Display radius (0..1) of a distance in metres: logarithmic, 0.2 m near the head, 40 m at the outer ring. */
float radiusOf (float metres) noexcept
{
    const float t = std::log (std::max (metres, space::minDistance) / space::minDistance) / std::log (space::maxDistance / space::minDistance);
    return 0.16f + 0.84f * juce::jlimit (0.0f, 1.0f, t);
}

space::Vec3 toScene (space::Vec3 p) noexcept
{
    const float d = space::lengthOf (p);
    if (d < 1.0e-6f) return { 0.0f, 0.16f, 0.0f };
    const float r = radiusOf (d) / d;
    return { p.x * r, p.y * r, p.z * r };
}

void ring (juce::Graphics& g, const Camera& cam, float r, float z, juce::Colour c, float width, bool dashed)
{
    juce::Path path;
    const int n = 96;
    for (int i = 0; i <= n; ++i)
    {
        const float a = (float) i / (float) n * 2.0f * pi;
        const auto p = cam.project ({ r * std::sin (a), r * std::cos (a), z }).s;
        if (i == 0) path.startNewSubPath (p);
        else path.lineTo (p);
    }
    g.setColour (c);
    if (dashed)
    {
        const float dash[] = { 2.0f, 5.0f };
        juce::Path d;
        juce::PathStrokeType (width).createDashedStroke (d, path, dash, 2);
        g.fillPath (d);
    }
    else
        g.strokePath (path, juce::PathStrokeType (width));
}

void meridian (juce::Graphics& g, const Camera& cam, float r, float azimuth, juce::Colour c, float width)
{
    juce::Path path;
    const int n = 48;
    for (int i = 0; i <= n; ++i)
    {
        const float e = -pi / 18.0f + (float) i / (float) n * (10.0f / 18.0f) * pi; // -10 .. +90 degrees (stays above the summary rows)
        const float a = azimuth;
        const auto p = cam.project ({ r * std::cos (e) * std::sin (a), r * std::cos (e) * std::cos (a), r * std::sin (e) }).s;
        if (i == 0) path.startNewSubPath (p);
        else path.lineTo (p);
    }
    g.setColour (c);
    g.strokePath (path, juce::PathStrokeType (width));
}

void drawHead (juce::Graphics& g, const Camera& cam, bool headphones, float u)
{
    const auto c = cam.project ({ 0.0f, 0.0f, 0.0f });
    const float r = cam.radius * 0.085f * c.scale;
    // shadow on the floor, the head, the nose (front), the ears
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.4f, r * 0.9f).withCentre (c.s.translated (0, r * 1.25f)));
    g.setColour (colour::display.brighter (0.12f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.s));
    g.setColour (colour::displayText.withAlpha (0.85f));
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.s), juce::jmax (1.0f, u * 0.18f));
    const auto nose = cam.project ({ 0.0f, 0.13f, 0.0f }).s;
    juce::Path n;
    n.addTriangle (nose, cam.project ({ -0.03f, 0.075f, 0.0f }).s, cam.project ({ 0.03f, 0.075f, 0.0f }).s);
    g.fillPath (n);
    for (float side : { -1.0f, 1.0f })
    {
        const auto e = cam.project ({ side * 0.088f, 0.0f, 0.0f }).s;
        g.fillEllipse (juce::Rectangle<float> (r * 0.32f, r * 0.62f).withCentre (e));
    }
    if (headphones) // the band over the head and two cups, in ultraviolet
    {
        const auto l = cam.project ({ -0.1f, 0.0f, 0.0f }).s, rr = cam.project ({ 0.1f, 0.0f, 0.0f }).s;
        juce::Path band;
        band.startNewSubPath (l);
        band.cubicTo (l.translated (0, -r * 1.9f), rr.translated (0, -r * 1.9f), rr);
        g.setColour (colour::ultraviolet);
        g.strokePath (band, juce::PathStrokeType (juce::jmax (1.2f, u * 0.3f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (auto p : { l, rr }) g.fillRoundedRectangle (juce::Rectangle<float> (r * 0.42f, r * 0.85f).withCentre (p), r * 0.15f);
    }
}

void drawSpeakerBox (juce::Graphics& g, const Camera& cam, float azimuthDeg, float u)
{
    const float a = azimuthDeg * pi / 180.0f;
    const auto p = cam.project ({ 0.62f * std::sin (a), 0.62f * std::cos (a), 0.0f });
    const float w = cam.radius * 0.07f * p.scale;
    auto box = juce::Rectangle<float> (w, w * 1.4f).withCentre (p.s.translated (0, -w * 0.5f));
    g.setColour (colour::sunYellow.withAlpha (0.85f));
    g.drawRoundedRectangle (box, w * 0.15f, juce::jmax (1.0f, u * 0.16f));
    g.fillEllipse (juce::Rectangle<float> (w * 0.5f, w * 0.5f).withCentre (box.getCentre().translated (0, w * 0.15f)));
    g.fillEllipse (juce::Rectangle<float> (w * 0.22f, w * 0.22f).withCentre (box.getCentre().translated (0, -w * 0.38f)));
}

void drawSource (juce::Graphics& g, const Camera& cam, space::Vec3 scenePos, float level, juce::Colour c, float u, bool live,
                 const juce::String& label)
{
    const auto p = cam.project (scenePos);
    const auto floor = cam.project ({ scenePos.x, scenePos.y, 0.0f });
    // drop line to the floor plane (reads the elevation)
    g.setColour (c.withAlpha (live ? 0.45f : 0.2f));
    const float dash[] = { 2.0f, 3.0f };
    g.drawDashedLine ({ p.s, floor.s }, dash, 2, juce::jmax (1.0f, u * 0.12f));
    g.fillEllipse (juce::Rectangle<float> (u * 0.7f * floor.scale, u * 0.35f * floor.scale).withCentre (floor.s));
    const float r = u * (0.7f + 0.9f * level) * p.scale;
    if (live)
        for (int k = 3; k >= 1; --k) // glow
        {
            g.setColour (c.withAlpha (0.10f * (float) (4 - k) * (0.4f + level)));
            g.fillEllipse (juce::Rectangle<float> (r * (1.0f + 0.9f * (float) k), r * (1.0f + 0.9f * (float) k)).withCentre (p.s));
        }
    g.setColour (live ? c : c.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p.s));
    g.setColour (colour::displayText.withAlpha (live ? 0.9f : 0.4f));
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p.s), juce::jmax (0.8f, u * 0.1f));
    if (label.isNotEmpty())
    {
        g.setFont (labelFont (u * 1.4f));
        g.setColour (c.withAlpha (live ? 1.0f : 0.5f));
        g.drawText (label, juce::Rectangle<float> (u * 6.0f, u * 1.6f).withPosition (p.s.translated (r + u * 0.3f, -u * 1.7f)),
                    juce::Justification::centredLeft, false);
    }
}

Camera makeCamera (juce::Rectangle<float> area, float yaw, juce::Point<float> parallax)
{
    Camera cam;
    // sized so the overhead arc (+90 degrees, nearest the camera) and the back of the floor stay inside the area
    cam.radius = juce::jmin (area.getWidth() * 0.46f, area.getHeight() * 0.54f);
    cam.centre = { area.getCentreX(), area.getY() + area.getHeight() * 0.55f };
    cam.yaw = yaw + parallax.x * 0.25f;
    cam.pitch = 0.42f + parallax.y * 0.12f;
    return cam;
}

void paintStage (juce::Graphics& g, const Camera& cam, float u, bool speakers)
{
    // floor: outer ring (40 m), middle ring, near ring; the median arc; front / back / left / right marks
    ring (g, cam, 1.0f, 0.0f, colour::displayDim.withAlpha (0.55f), juce::jmax (1.0f, u * 0.14f), false);
    ring (g, cam, radiusOf (5.0f), 0.0f, colour::displayDim.withAlpha (0.35f), juce::jmax (1.0f, u * 0.1f), true);
    ring (g, cam, radiusOf (1.0f), 0.0f, colour::displayDim.withAlpha (0.3f), juce::jmax (1.0f, u * 0.1f), true);
    ring (g, cam, 0.7f, 0.7f, colour::displayDim.withAlpha (0.18f), juce::jmax (1.0f, u * 0.08f), true); // 45 degrees up
    meridian (g, cam, 1.0f, 0.0f, colour::displayDim.withAlpha (0.28f), juce::jmax (1.0f, u * 0.1f));
    meridian (g, cam, 1.0f, pi, colour::displayDim.withAlpha (0.2f), juce::jmax (1.0f, u * 0.1f));
    g.setFont (labelFont (u * 1.25f));
    const char* marks[4] = { "F", "R", "B", "L" };
    for (int i = 0; i < 4; ++i)
    {
        const float a = (float) i * 0.5f * pi;
        const auto p = cam.project ({ 1.1f * std::sin (a), 1.1f * std::cos (a), 0.0f }).s;
        g.setColour (colour::displayDim);
        g.drawText (tr (marks[i]), juce::Rectangle<float> (u * 2.0f, u * 1.6f).withCentre (p), juce::Justification::centred, false);
    }
    if (speakers)
    {
        drawSpeakerBox (g, cam, -30.0f, u);
        drawSpeakerBox (g, cam, 30.0f, u);
    }
}

/** p30: the edit part's motion path ahead of the source - one cycle (the random paths: the next cycle from now) as a
    faint dotted curve in the part's colour, so every path shape reads at a glance, even before a note plays. */
void paintPathPreview (juce::Graphics& g, const Camera& cam, Core& core, int part, float u, juce::Colour c, bool live)
{
    const auto& reg = core.params();
    const auto pre = partPrefix (part) + "space.";
    if (reg.get (pre + "on") < 0.5f) return;
    const int path = (int) std::lround (reg.get (pre + "path"));
    if (path <= space::PathOff || path >= space::numPaths) return;
    const float az = reg.get (pre + "azimuth"), el = reg.get (pre + "elevation"), dist = reg.get (pre + "distance");
    const float size = reg.get (pre + "pathsize");
    const auto& stage = core.part (part).spaceStage();
    const double now = stage.uiPhase();
    const double start = space::pathIsPeriodic (path) ? std::floor (now) : now;
    constexpr int n = 120;
    juce::Path curve;
    for (int i = 0; i <= n; ++i)
    {
        const auto local = space::pathPoint (path, start + (double) i / (double) n, size, stage.getSeed());
        const float k = std::max (0.05f, space::lengthOf (local));
        const auto o = space::orient (local, az, el);
        const float d = juce::jlimit (space::minDistance, space::maxDistance, dist * k);
        const auto pt = cam.project (toScene ({ o.x / k * d, o.y / k * d, o.z / k * d })).s;
        if (i == 0) curve.startNewSubPath (pt);
        else curve.lineTo (pt);
    }
    const float dash[] = { u * 0.25f, u * 0.45f };
    juce::Path dotted;
    juce::PathStrokeType (juce::jmax (1.0f, u * 0.16f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
        .createDashedStroke (dotted, curve, dash, 2);
    g.setColour (c.withAlpha (live ? 0.42f : 0.26f));
    g.fillPath (dotted);
}
} // namespace

// ------------------------------------------------------------------------------------------------- state
bool partUsesSpace (Core& core, int part)
{
    const auto& reg = core.params();
    const auto p = partPrefix (part);
    if (reg.get (p + "space.on") >= 0.5f) return true;
    for (const auto& pre : FxRack::partPrefixes (p))
        if ((int) std::lround (reg.get (pre + "type")) == InsertSlot::Space3D && reg.get (pre + "bypass") < 0.5f) return true;
    return false;
}

bool speakerMonitoring (Core& core) { return core.params().get (Core::Ids::spaceOut) >= 0.5f; }

State partState (Core& core, int part)
{
    if (! partUsesSpace (core, part)) return State::Off;
    return speakerMonitoring (core) ? State::Speakers : State::Binaural;
}

int partMode (Core& core, int part)
{
    const auto& reg = core.params();
    return reg.get (partPrefix (part) + "space.on") >= 0.5f ? (speakerMonitoring (core) ? 2 : 1) : 0;
}

bool isSegmentedSwitch (const std::string& id)
{
    return id.size() == 13 && id[0] == 'p' && id.compare (2, 11, ".space.mode") == 0;
}

juce::String badgeTooltip (State s)
{
    switch (s)
    {
        case State::Binaural: return tr ("Binaural 3D \xe2\x80\x93 best with headphones");
        case State::Speakers: return tr ("3D for speakers \xe2\x80\x93 choose Headphones on the 3D switch for binaural sound");
        case State::Off: break;
    }
    return tr ("3D Space is off for this part");
}

// ------------------------------------------------------------------------------------------------- pictograms
void drawHeadphones (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, float glow)
{
    const float s = juce::jmin (area.getWidth(), area.getHeight());
    auto b = area.withSizeKeepingCentre (s, s);
    const float w = juce::jmax (1.0f, s * 0.09f);
    auto paintIt = [&] (juce::Colour c, float width) {
        juce::Path band;
        band.addCentredArc (b.getCentreX(), b.getY() + s * 0.56f, s * 0.36f, s * 0.38f, 0.0f, -pi * 0.5f, pi * 0.5f, true);
        g.setColour (c);
        g.strokePath (band, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (float side : { -1.0f, 1.0f })
        {
            auto cup = juce::Rectangle<float> (s * 0.2f, s * 0.36f).withCentre ({ b.getCentreX() + side * s * 0.34f, b.getY() + s * 0.7f });
            g.fillRoundedRectangle (cup, s * 0.07f);
        }
    };
    if (glow > 0.0f)
        for (int k = 3; k >= 1; --k) paintIt (ink.withAlpha (0.12f * glow * (float) (4 - k) / 3.0f), w * (1.0f + 1.6f * (float) k));
    paintIt (ink, w);
    if (glow > 0.0f) // sound arcs inside the band: "it is playing in 3D"
    {
        g.setColour (ink.withAlpha (0.55f + 0.45f * glow));
        for (int k = 0; k < 2; ++k)
        {
            juce::Path arc;
            const float r = s * (0.08f + 0.08f * (float) k);
            arc.addCentredArc (b.getCentreX(), b.getY() + s * 0.66f, r, r, 0.0f, -pi * 0.3f, pi * 0.3f, true);
            g.strokePath (arc, juce::PathStrokeType (w * 0.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
}

void drawSpeakers (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink)
{
    const float s = juce::jmin (area.getWidth(), area.getHeight());
    auto b = area.withSizeKeepingCentre (s, s);
    const float w = juce::jmax (1.0f, s * 0.08f);
    g.setColour (ink);
    for (float side : { -1.0f, 1.0f })
    {
        auto box = juce::Rectangle<float> (s * 0.34f, s * 0.6f).withCentre ({ b.getCentreX() + side * s * 0.24f, b.getCentreY() + s * 0.04f });
        g.drawRoundedRectangle (box, s * 0.05f, w);
        g.fillEllipse (juce::Rectangle<float> (s * 0.18f, s * 0.18f).withCentre (box.getCentre().translated (0, s * 0.08f)));
        g.fillEllipse (juce::Rectangle<float> (s * 0.08f, s * 0.08f).withCentre (box.getCentre().translated (0, -s * 0.15f)));
    }
}

void drawBadge (juce::Graphics& g, juce::Rectangle<float> area, State st, float time, bool speakersWhenOff)
{
    const float h = area.getHeight();
    const auto label = juce::String ("3D");
    if (st == State::Binaural)
    {
        const float pulse = 0.5f + 0.5f * std::sin (time * 2.4f);
        for (int k = 3; k >= 1; --k) // halo
        {
            g.setColour (colour::ultraviolet.withAlpha (0.08f * (float) (4 - k) * (0.5f + 0.5f * pulse)));
            g.fillRoundedRectangle (area.expanded (h * 0.12f * (float) k), h * (0.3f + 0.12f * (float) k));
        }
        g.setColour (colour::ultraviolet);
        g.fillRoundedRectangle (area, h * 0.3f);
        drawHeadphones (g, area.withWidth (h).reduced (h * 0.14f).translated (h * 0.12f, 0), colour::displayText, 0.6f + 0.4f * pulse);
        g.setColour (colour::displayText);
    }
    else if (st == State::Speakers)
    {
        g.setColour (colour::sunYellow);
        g.drawRoundedRectangle (area.reduced (0.5f), h * 0.3f, juce::jmax (1.0f, h * 0.07f));
        drawSpeakers (g, area.withWidth (h).reduced (h * 0.16f).translated (h * 0.12f, 0), colour::sunYellow);
    }
    else // off: dim, with the pictogram of the monitoring a switch-on would use (p41)
    {
        g.setColour (colour::displayGrid.brighter (0.35f));
        g.drawRoundedRectangle (area.reduced (0.5f), h * 0.3f, juce::jmax (1.0f, h * 0.06f));
        if (speakersWhenOff)
            drawSpeakers (g, area.withWidth (h).reduced (h * 0.16f).translated (h * 0.12f, 0), colour::displayGrid.brighter (0.5f));
        else
            drawHeadphones (g, area.withWidth (h).reduced (h * 0.16f).translated (h * 0.12f, 0), colour::displayGrid.brighter (0.5f), 0.0f);
        g.setColour (colour::displayGrid.brighter (0.6f));
    }
    g.setFont (brandFont (h * 0.62f));
    g.drawText (label, area.withTrimmedLeft (h * 1.05f).withTrimmedRight (h * 0.2f), juce::Justification::centred, false);
}

// ------------------------------------------------------------------------------------------------- pages
std::vector<std::string> spaceParams (int part)
{
    const auto p = partPrefix (part) + "space.";
    return { p + "mode", p + "path", p + "pathlen", p + "pathrate",                    // p41: the 3D switch (Off / Headphones / Speakers), motion
             p + "azimuth", p + "elevation", p + "distance", p + "spread",              // position
             p + "pathsize", p + "pathphase", p + "glide", p + "doppler",              // motion shape
             p + "room", p + "roomsize", p + "air", p + "rolloff",                     // room and distance
             p + "lowkeep", Core::Ids::spaceOut, Core::Ids::spaceMonoCheck, "" };      // monitoring
}

std::vector<std::string> spaceTitles() { return { "3D Motion", "3D Position", "Motion Shape", "Room & Distance", "Monitoring" }; }

// ------------------------------------------------------------------------------------------------- the scene
void Scene::clear()
{
    for (auto& t : trails) t = {};
}

void Scene::update (Core& core, float seconds)
{
    time += seconds;
    yaw = 0.18f * std::sin (time * 0.11f); // the view drifts slowly: a living display
    for (int p = 0; p < Core::numParts; ++p)
    {
        auto& t = trails[(size_t) p];
        const auto& stage = core.part (p).spaceStage();
        t.active = core.part (p).spaceRunning();
        const auto pos = stage.uiPosition();
        t.level += (juce::jmin (1.0f, stage.uiLevel() * 2.0f) - t.level) * 0.3f;
        if (! t.active) { t.count = 0; t.p[(size_t) t.head] = pos; continue; }
        t.head = (t.head + 1) % trailLength;
        t.p[(size_t) t.head] = pos;
        t.count = juce::jmin (trailLength, t.count + 1);
    }
}

void Scene::paint (juce::Graphics& g, juce::Rectangle<float> area, Core& core, int focus, juce::Point<float> parallax, float correlation) const
{
    const float u = area.getWidth() / 42.0f;
    const bool speakers = speakerMonitoring (core);
    const auto cam = makeCamera (area.reduced (u * 0.5f), yaw, parallax);
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (area.toNearestInt()); // nothing of the scene (low sources, trails) runs into the tabs or the rows below
    paintStage (g, cam, u, speakers);
    const int focusPart = juce::jlimit (0, Core::numParts - 1, focus);
    paintPathPreview (g, cam, core, focusPart, u, colour::encoder (focusPart), trails[(size_t) focusPart].active);

    // sources behind the head first, then the head, then those in front (depth = further from the camera first)
    struct Item { int part; float depth; };
    std::vector<Item> order;
    for (int p = 0; p < Core::numParts; ++p)
        if (trails[(size_t) p].active || p == focus) order.push_back ({ p, cam.project (toScene (trails[(size_t) p].p[(size_t) trails[(size_t) p].head])).depth });
    std::sort (order.begin(), order.end(), [] (const Item& a, const Item& b) { return a.depth > b.depth; });
    bool headDrawn = false;
    for (const auto& it : order)
    {
        if (! headDrawn && it.depth < 0.0f) { drawHead (g, cam, ! speakers, u); headDrawn = true; }
        const auto& t = trails[(size_t) it.part];
        const auto c = colour::encoder (it.part);
        // trail: oldest faint, newest bright
        for (int k = t.count - 1; k >= 1; --k)
        {
            const auto a = cam.project (toScene (t.p[(size_t) ((t.head - k + trailLength) % trailLength)])).s;
            const auto b = cam.project (toScene (t.p[(size_t) ((t.head - k + 1 + trailLength) % trailLength)])).s;
            const float f = 1.0f - (float) k / (float) trailLength;
            g.setColour (c.withAlpha (0.55f * f * f));
            g.drawLine ({ a, b }, juce::jmax (1.0f, u * 0.32f * f));
        }
        drawSource (g, cam, toScene (t.p[(size_t) t.head]), t.level, c, u, t.active, "P" + juce::String (it.part + 1));
    }
    if (! headDrawn) drawHead (g, cam, ! speakers, u);

    // caption: what the view shows, monitoring mode, the edit part's position
    {   // p41: the monitoring's own pictogram in front (headphones / speakers), so a switch is visible at once
        auto cap = area.withHeight (u * 2.0f).withWidth (area.getWidth() * 0.6f);
        const auto icon = cap.removeFromLeft (u * 1.9f).withHeight (u * 1.7f);
        if (speakers) drawSpeakers (g, icon, colour::displayDim);
        else drawHeadphones (g, icon, colour::displayDim, 0.0f);
        cap.removeFromLeft (u * 0.5f);
        g.setFont (labelFont (u * 1.45f));
        g.setColour (colour::displayDim);
        g.drawText (i18n::upper (tr ("3D Space")) + "  " + i18n::upper (speakers ? tr ("Speakers") : tr ("Headphones")),
                    cap, juce::Justification::topLeft, true);
    }
    const auto& stage = core.part (juce::jlimit (0, Core::numParts - 1, focus)).spaceStage();
    const auto pos = stage.uiPosition();
    g.setFont (monoFont (u * 1.3f));
    g.setColour (colour::encoder (focus).withAlpha (trails[(size_t) juce::jlimit (0, Core::numParts - 1, focus)].active ? 1.0f : 0.5f));
    g.drawText (juce::String (juce::roundToInt (space::azimuthOf (pos))) + juce::String::fromUTF8 ("\xc2\xb0 ")
                    + juce::String (juce::roundToInt (space::elevationOf (pos))) + juce::String::fromUTF8 ("\xc2\xb0 ")
                    + juce::String (space::lengthOf (pos), 1) + " m",
                area.withHeight (u * 2.0f), juce::Justification::topRight, false);

    // p30: the edit part's motion path by name, bottom left (above the correlation bar)
    {
        const auto& reg = core.params();
        const int idx = reg.indexOf (partPrefix (focusPart) + "space.path");
        if (idx >= 0 && reg.get (partPrefix (focusPart) + "space.on") >= 0.5f)
        {
            const auto& d = reg.descriptor (idx);
            const int v = juce::jlimit (0, (int) d.choices.size() - 1, (int) std::lround (reg.get (idx)));
            if (v > 0)
            {
                g.setFont (labelFont (u * 1.25f));
                g.setColour (colour::encoder (focusPart).withAlpha (0.8f));
                g.drawText (i18n::upper (i18n::pick (d.choices[(size_t) v], v < (int) d.choicesDe.size() ? d.choicesDe[(size_t) v] : d.choices[(size_t) v])),
                            area.withTrimmedBottom (u * 1.6f).removeFromBottom (u * 1.8f).removeFromLeft (area.getWidth() * 0.4f), // above the mono bar
                            juce::Justification::bottomLeft, false);
            }
        }
    }

    // master correlation (mono compatibility): a thin bar at the bottom, red when negative
    if (std::isfinite (correlation))
    {
        auto bar = area.removeFromBottom (u * 1.0f).reduced (area.getWidth() * 0.2f, u * 0.3f);
        g.setColour (colour::displayGrid.brighter (0.2f));
        g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
        const float x = bar.getCentreX() + correlation * bar.getWidth() * 0.5f;
        g.setColour (correlation < 0.0f ? colour::signalRed : colour::displayText);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (x, bar.getCentreX()), bar.getY(), std::abs (x - bar.getCentreX()) + 1.0f, bar.getHeight()),
                                bar.getHeight() * 0.5f);
        g.setFont (labelFont (u * 1.1f));
        g.setColour (colour::displayDim);
        g.drawText (i18n::upper (tr ("Mono")), bar.withX (bar.getX() - u * 5.0f).withWidth (u * 4.6f), juce::Justification::centredRight, false);
    }
}

void Scene::paintSingle (juce::Graphics& g, juce::Rectangle<float> area, const space::Vec3& position, float level, juce::Colour c,
                         bool speakers, float t)
{
    const float u = area.getWidth() / 42.0f;
    const auto cam = makeCamera (area, 0.12f * std::sin (t * 0.11f), {});
    paintStage (g, cam, u, speakers);
    const auto sp = toScene (position);
    if (cam.project (sp).depth >= 0.0f)
    {
        drawSource (g, cam, sp, level, c, u, true, {});
        drawHead (g, cam, ! speakers, u);
    }
    else
    {
        drawHead (g, cam, ! speakers, u);
        drawSource (g, cam, sp, level, c, u, true, {});
    }
}
} // namespace ohpe::ui::space3d
