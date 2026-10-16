// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ResoView.h"

#include "Palette.h"
#include "fx/Resonator.h"
#include "plugin/Localisation.h"

#include <algorithm>
#include <cmath>

namespace ohpe::ui::resoview
{
using i18n::tr;

namespace
{
constexpr float twoPi = 6.28318530718f;

std::string partPrefix (int part) { return "p" + std::to_string (part + 1) + "."; }

juce::String noteName (float hz)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = (int) std::lround (69.0f + 12.0f * std::log2 (std::max (1.0f, hz) / 440.0f));
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}

juce::String choiceOf (const Core& core, const std::string& id)
{
    const auto& reg = core.params();
    const int i = reg.indexOf (id);
    if (i < 0) return {};
    const auto& d = reg.descriptor (i);
    const int k = juce::jlimit (0, juce::jmax (0, (int) d.choices.size() - 1), (int) std::lround (reg.get (i)));
    if (d.choices.empty()) return {};
    return i18n::pick (d.choices[(size_t) k], k < (int) d.choicesDe.size() ? d.choicesDe[(size_t) k] : std::string());
}
} // namespace

std::vector<std::string> resoParams (int part)
{
    const auto p = partPrefix (part) + "reso.";
    return { p + "on", p + "model", p + "follow", p + "mix",                    // body
             p + "tune", p + "size", p + "material", p + "decay",              // tuning and material
             p + "position", p + "inharm", p + "sympathetic", p + "width",     // character
             p + "drive", p + "pressamt", p + "slideamt", "" };                // drive and expression
}

std::vector<std::string> resoTitles() { return { "Resonator", "Body Tuning", "Body Character", "Drive & Expression" }; }

BodyState stateOf (const ResonatorEffect& e, const Core& core, const std::string& pre, bool on)
{
    BodyState s;
    const auto& reg = core.params();
    s.model = e.uiModel();
    s.level = juce::jlimit (0.0f, 1.0f, e.uiLevel() * 3.0f);
    s.pitchHz = e.uiPitch();
    s.position = e.uiExcitePosition();
    if (reg.indexOf (pre + "model") >= 0) s.model = juce::jlimit (0, ResonatorEffect::numModels - 1, (int) std::lround (reg.get (pre + "model")));
    if (reg.indexOf (pre + "sympathetic") >= 0) s.sympathetic = reg.get (pre + "sympathetic");
    if (reg.indexOf (pre + "size") >= 0) s.size = reg.get (pre + "size");
    if (! on) s.position = reg.indexOf (pre + "position") >= 0 ? reg.get (pre + "position") : s.position;
    s.on = on;
    return s;
}

void paintBody (juce::Graphics& g, juce::Rectangle<float> area, const BodyState& s, float time, juce::Colour ink)
{
    const auto c = s.on ? ink : colour::displayDim;
    const float amp = s.on ? 0.15f + 0.85f * s.level : 0.0f;
    // the standing wave moves slowly on screen (a fraction of the real pitch, so it reads as motion, not a blur)
    const float phase = time * (1.5f + 2.5f * std::log2 (std::max (20.0f, s.pitchHz) / 20.0f) / 10.0f) * twoPi;
    auto box = area.reduced (area.getWidth() * 0.06f, area.getHeight() * 0.12f);
    const float stroke = juce::jmax (1.0f, area.getHeight() * 0.012f);

    if (s.model == ResonatorEffect::ModelPlate)
    {
        // a plate in perspective: an outline and a grid of dots that rise and fall with two modes
        const float aspect = 1.0f + 1.0f * s.size;
        auto plate = box.withSizeKeepingCentre (juce::jmin (box.getWidth(), box.getHeight() * 1.2f * aspect), box.getHeight() * 0.75f);
        const float skew = plate.getWidth() * 0.14f;
        juce::Path outline;
        outline.startNewSubPath (plate.getX() + skew, plate.getY());
        outline.lineTo (plate.getRight(), plate.getY());
        outline.lineTo (plate.getRight() - skew, plate.getBottom());
        outline.lineTo (plate.getX(), plate.getBottom());
        outline.closeSubPath();
        g.setColour (colour::displayGrid.brighter (0.25f));
        g.fillPath (outline);
        g.setColour (c.withAlpha (0.6f));
        g.strokePath (outline, juce::PathStrokeType (stroke));
        const int nx = 13, ny = 7;
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i)
            {
                const float u = ((float) i + 0.5f) / (float) nx, v = ((float) j + 0.5f) / (float) ny;
                const float z = std::sin (2.0f * 3.14159f * u) * std::sin (3.14159f * v) * std::cos (phase)
                                + 0.6f * std::sin (3.14159f * u) * std::sin (2.0f * 3.14159f * v) * std::sin (phase * 1.37f);
                const float x = plate.getX() + skew * (1.0f - v) + u * (plate.getWidth() - skew);
                const float y = plate.getY() + v * plate.getHeight() - z * amp * plate.getHeight() * 0.18f;
                const float r = stroke * (1.1f + 0.9f * std::abs (z) * amp);
                g.setColour (c.withAlpha (0.35f + 0.6f * std::abs (z) * amp));
                g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
            }
        // the exciter
        const float ex = plate.getX() + skew * 0.63f + (0.07f + 0.43f * s.position) * (plate.getWidth() - skew);
        const float ey = plate.getY() + 0.37f * plate.getHeight();
        g.setColour (colour::sunYellow);
        g.fillEllipse (ex - stroke * 2.2f, ey - stroke * 2.2f, stroke * 4.4f, stroke * 4.4f);
        return;
    }

    if (s.model == ResonatorEffect::ModelOpenTube || s.model == ResonatorEffect::ModelClosedTube)
    {
        const bool closed = s.model == ResonatorEffect::ModelClosedTube;
        const float h = box.getHeight() * (0.22f + 0.2f * s.size);
        auto tube = box.withSizeKeepingCentre (box.getWidth(), h);
        g.setColour (colour::displayGrid.brighter (0.25f));
        g.fillRoundedRectangle (tube, h * 0.18f);
        g.setColour (c.withAlpha (0.7f));
        g.drawRoundedRectangle (tube, h * 0.18f, stroke);
        if (closed) // the closed end: a solid cap on the left
            g.fillRoundedRectangle (tube.withWidth (stroke * 3.0f), stroke);
        // pressure standing wave inside (closed: a quarter wave - the antinode at the closed end)
        juce::Path wave;
        const int n = 60;
        for (int i = 0; i <= n; ++i)
        {
            const float u = (float) i / (float) n;
            const float shape = closed ? std::cos (u * 3.14159f * 0.5f) : std::cos (u * 3.14159f);
            const float y = tube.getCentreY() - shape * std::cos (phase) * amp * h * 0.36f;
            const float x = tube.getX() + u * tube.getWidth();
            if (i == 0) wave.startNewSubPath (x, y);
            else wave.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (wave, juce::PathStrokeType (stroke * 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // open ends radiate: short arcs
        g.setColour (c.withAlpha (0.25f + 0.5f * amp));
        for (int k = 1; k <= 2; ++k)
        {
            const float r = h * 0.25f * (float) k;
            juce::Path arc;
            arc.addCentredArc (tube.getRight(), tube.getCentreY(), r, r * 1.4f, 0.0f, 0.5f, 2.64f, true);
            g.strokePath (arc, juce::PathStrokeType (stroke));
            if (! closed)
            {
                juce::Path arcL;
                arcL.addCentredArc (tube.getX(), tube.getCentreY(), r, r * 1.4f, 0.0f, -0.5f, -2.64f, true);
                g.strokePath (arcL, juce::PathStrokeType (stroke));
            }
        }
        const float ex = tube.getX() + (0.04f + 0.46f * s.position) * tube.getWidth();
        g.setColour (colour::sunYellow);
        g.fillEllipse (ex - stroke * 2.2f, tube.getBottom() - stroke * 2.2f, stroke * 4.4f, stroke * 4.4f);
        return;
    }

    // string: the main pair between two bridges, the sympathetic strings below
    const float left = box.getX(), right = box.getRight();
    const float y0 = box.getY() + box.getHeight() * 0.32f;
    g.setColour (c.withAlpha (0.8f));
    for (float x : { left, right })
        g.fillRoundedRectangle (x - stroke * 1.5f, box.getY() + box.getHeight() * 0.12f, stroke * 3.0f, box.getHeight() * 0.76f, stroke);
    for (int k = 0; k < 2; ++k)
    {
        juce::Path str;
        const int n = 64;
        for (int i = 0; i <= n; ++i)
        {
            const float u = (float) i / (float) n;
            const float d = std::sin (u * 3.14159f) * std::cos (phase + (float) k * 0.6f)
                            + 0.35f * std::sin (2.0f * u * 3.14159f) * std::sin (phase * 2.0f + (float) k);
            const float x = left + u * (right - left);
            const float y = y0 + (float) k * stroke * 3.0f - d * amp * box.getHeight() * 0.2f;
            if (i == 0) str.startNewSubPath (x, y);
            else str.lineTo (x, y);
        }
        g.setColour (k == 0 ? c : c.withAlpha (0.55f));
        g.strokePath (str, juce::PathStrokeType (stroke * (k == 0 ? 1.4f : 1.0f)));
    }
    const int numSym = s.sympathetic > 0.0f ? 2 + (int) std::lround (s.size * 4.0f) : 0;
    for (int k = 0; k < numSym; ++k)
    {
        const float y = box.getY() + box.getHeight() * (0.62f + 0.05f * (float) k);
        const float d = std::sin (phase * (1.3f + 0.21f * (float) k)) * amp * s.sympathetic * box.getHeight() * 0.02f;
        g.setColour (c.withAlpha (0.25f + 0.35f * s.sympathetic));
        g.drawLine (left, y, right, y + d, juce::jmax (0.8f, stroke * 0.6f));
    }
    const float ex = left + (0.04f + 0.46f * s.position) * (right - left);
    g.setColour (colour::sunYellow);
    g.fillEllipse (ex - stroke * 2.2f, y0 - stroke * 2.2f, stroke * 4.4f, stroke * 4.4f);
}

juce::String statusLine (const Core& core, int part)
{
    const auto pre = partPrefix (part) + "reso.";
    const auto& reg = core.params();
    if (reg.indexOf (pre + "on") < 0) return {};
    if (reg.get (pre + "on") < 0.5f) return tr ("Resonator off - switch it on with the first encoder");
    auto& stage = const_cast<Core&> (core).part (part).resoStage();
    return choiceOf (core, pre + "model") + "  /  " + choiceOf (core, pre + "follow") + "  /  " + noteName (stage.uiPitch()) + " "
           + juce::String (stage.uiPitch(), 1) + " Hz";
}
} // namespace ohpe::ui::resoview
