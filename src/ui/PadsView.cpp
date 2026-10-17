// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PadsView.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <cmath>

namespace ohpe::ui::padsview
{
using i18n::tr;

namespace
{
constexpr float twoPi = 6.2831853f;

juce::String valueText (Core& core, const char* id)
{
    const auto& reg = core.params();
    const int i = reg.indexOf (id);
    return i >= 0 ? formatValue (reg.descriptor (i), reg.get (i)) : juce::String();
}

juce::String settingText (Core& core, int pad)
{
    switch (pad)
    {
        case PerformancePads::Stutter: return valueText (core, "pads.stutter.div");
        case PerformancePads::TapeStop: return valueText (core, "pads.stop.len");
        case PerformancePads::Sweep:
            return (std::lround (core.params().get ("pads.sweep.mode")) == 0 ? juce::String ("LP ") : juce::String ("HP "))
                 + valueText (core, "pads.sweep.len");
        default: return valueText (core, "pads.freeze.mix");
    }
}

/** The picture of a pad: what the throw does to the sound. */
void picture (juce::Graphics& g, juce::Rectangle<float> r, int pad, const PerformancePads& pads, juce::Colour col, bool lit, float time, bool highPass)
{
    const float stroke = juce::jmax (1.2f, r.getHeight() * 0.035f);
    const auto pst = juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.36f;
    juce::Path p;
    switch (pad)
    {
        case PerformancePads::Stutter: // one slice, repeated four times; the playing repeat lights up
        {
            const float w = r.getWidth() / 4.0f;
            const float phase = pads.uiStutterPhase();
            const int current = lit ? (int) (time * 8.0f) % 4 : -1;
            for (int k = 0; k < 4; ++k)
            {
                juce::Path s;
                for (int i = 0; i <= 24; ++i)
                {
                    const float t = (float) i / 24.0f;
                    const float x = r.getX() + w * ((float) k + 0.08f + 0.84f * t);
                    const float y = mid - amp * std::sin (twoPi * 1.5f * t) * (1.0f - 0.6f * t) * std::sin (3.14159265f * t);
                    if (i == 0) s.startNewSubPath (x, y); else s.lineTo (x, y);
                }
                g.setColour (col.withAlpha (k == current ? 1.0f : 0.45f));
                g.strokePath (s, pst);
            }
            if (lit && phase > 0.0f)
            {
                g.setColour (colour::playGreenLit);
                g.fillRect (r.getX() + r.getWidth() * phase - 1.0f, r.getY(), 2.0f, r.getHeight());
            }
            return;
        }
        case PerformancePads::TapeStop: // the wave stretches and fades: a tape coming to rest
        {
            float ph = 0.0f;
            for (int i = 0; i <= 80; ++i)
            {
                const float t = (float) i / 80.0f;
                const float speed = 1.0f - t;
                ph += speed * 0.32f;
                const float x = r.getX() + r.getWidth() * t;
                const float y = mid - amp * std::sin (ph) * juce::jmin (1.0f, speed * 3.0f);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col.withAlpha (lit ? 1.0f : 0.6f));
            g.strokePath (p, pst);
            if (lit)
            {
                g.setColour (colour::playGreenLit);
                g.fillRect (r.getX() + r.getWidth() * (1.0f - pads.uiStopSpeed()) - 1.0f, r.getY(), 2.0f, r.getHeight());
            }
            return;
        }
        case PerformancePads::Sweep: // the filter's response; the corner moves with the sweep
        {
            const float s = pads.uiSweep();
            const float corner = highPass ? 0.08f + 0.8f * s : 0.92f - 0.8f * s;
            for (int i = 0; i <= 60; ++i)
            {
                const float t = (float) i / 60.0f;
                const float d = (t - corner) * 6.0f;
                const float bump = 0.55f * std::exp (-d * d * 2.0f) * (0.3f + s);
                const float beyond = highPass ? corner - t : t - corner;
                const float fall = beyond > 0.0f ? juce::jmin (1.4f, beyond * 5.0f) : 0.0f;
                const float x = r.getX() + r.getWidth() * t;
                const float y = r.getY() + r.getHeight() * (0.35f - bump * 0.4f + fall * 0.45f);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col.withAlpha (lit ? 1.0f : 0.6f));
            g.strokePath (p, pst);
            return;
        }
        default: // freeze: a crystal over a held wave
        {
            const float level = juce::jmax (lit ? 0.35f : 0.0f, pads.uiFreezeLevel());
            for (int i = 0; i <= 60; ++i)
            {
                const float t = (float) i / 60.0f;
                const float x = r.getX() + r.getWidth() * t;
                const float y = mid + amp * 0.5f * std::sin (twoPi * 3.0f * t + (lit ? time * 0.4f : 0.0f));
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col.withAlpha (0.35f + 0.65f * level));
            g.strokePath (p, pst);
            const auto c = r.getCentre();
            const float rad = r.getHeight() * 0.42f;
            juce::Path flake;
            for (int k = 0; k < 3; ++k)
            {
                const float a = 3.14159265f * (float) k / 3.0f + 0.5235988f;
                flake.startNewSubPath (c.x - rad * std::cos (a), c.y - rad * std::sin (a));
                flake.lineTo (c.x + rad * std::cos (a), c.y + rad * std::sin (a));
            }
            g.setColour (col.withAlpha (lit ? 0.9f : 0.5f));
            g.strokePath (flake, juce::PathStrokeType (stroke * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }
    }
}
} // namespace

std::vector<std::string> padsParams()
{
    return { "pads.stutter", "pads.stop", "pads.sweep", "pads.freeze",                 // the pads
             "pads.quant", "pads.stutter.div", "pads.stop.len", "pads.sweep.len",      // timing
             "pads.sweep.mode", "pads.sweep.res", "pads.freeze.mix", "" };             // sweep and freeze
}

std::vector<std::string> padsTitles() { return { "Pads", "Pad Timing", "Sweep & Freeze" }; }

const char* padName (int pad) noexcept
{
    static const char* names[] = { "Stutter", "Tape Stop", "Sweep", "Freeze" };
    return names[juce::jlimit (0, PerformancePads::numPads - 1, pad)];
}

juce::Rectangle<float> padArea (juce::Rectangle<float> v, int pad)
{
    const float gap = juce::jmax (3.0f, v.getWidth() * 0.025f);
    const float w = (v.getWidth() - gap) * 0.5f, h = (v.getHeight() - gap) * 0.5f;
    const int col = pad % 2, row = pad / 2;
    return { v.getX() + (float) col * (w + gap), v.getY() + (float) row * (h + gap), w, h };
}

int padAt (juce::Rectangle<float> v, juce::Point<float> p)
{
    for (int k = 0; k < PerformancePads::numPads; ++k)
        if (padArea (v, k).contains (p)) return k;
    return -1;
}

void paint (juce::Graphics& g, juce::Rectangle<float> v, Core& core, float time)
{
    const auto& pads = core.pads();
    for (int k = 0; k < PerformancePads::numPads; ++k)
    {
        const auto a = padArea (v, k);
        const float rad = juce::jmin (5.0f, a.getHeight() * 0.1f);
        const auto st = pads.uiState (k);
        const bool held = core.params().get (PerformancePads::padId (k)) >= 0.5f;
        const bool lit = st == PerformancePads::Engaged;
        const auto col = colour::encoder (k);

        // the pad: recessed field, the rim says what it does (violet blinking = waiting for the grid, green = on)
        g.setColour (lit ? colour::displayGrid.brighter (0.25f) : colour::displayGrid);
        g.fillRoundedRectangle (a, rad);
        juce::Colour rim = colour::displayDim.withAlpha (0.45f);
        float rimW = 1.0f;
        if (st == PerformancePads::Pending || (held && ! lit && st != PerformancePads::Releasing))
        {
            rim = colour::ultraviolet.withAlpha (0.5f + 0.5f * (std::sin (time * 18.0f) > 0.0f ? 1.0f : 0.0f));
            rimW = 2.0f;
        }
        else if (lit) { rim = colour::playGreenLit; rimW = 2.0f; }
        else if (st == PerformancePads::Releasing) { rim = colour::playGreen.withAlpha (0.6f); rimW = 1.5f; }
        g.setColour (rim);
        g.drawRoundedRectangle (a.reduced (rimW * 0.5f), rad, rimW);

        // name and the pad's main setting
        auto inner = a.reduced (a.getHeight() * 0.08f);
        auto head = inner.removeFromTop (inner.getHeight() * 0.24f);
        g.setFont (labelFont (head.getHeight() * 0.8f));
        g.setColour (lit ? colour::displayText : colour::displayText.withAlpha (0.75f));
        g.drawText (i18n::upper (tr (padName (k))), head, juce::Justification::centredLeft, true);
        g.setFont (monoFont (head.getHeight() * 0.7f));
        g.setColour (colour::displayDim);
        g.drawText (settingText (core, k), head, juce::Justification::centredRight, true);

        picture (g, inner.reduced (0.0f, inner.getHeight() * 0.08f), k, pads, col, lit, time,
                 std::lround (core.params().get ("pads.sweep.mode")) == 1);
    }
}

juce::String statusLine (Core& core)
{
    const auto& pads = core.pads();
    juce::StringArray on, waiting;
    for (int k = 0; k < PerformancePads::numPads; ++k)
    {
        const auto st = pads.uiState (k);
        if (st == PerformancePads::Engaged) on.add (tr (padName (k)));
        else if (st == PerformancePads::Pending) waiting.add (tr (padName (k)));
    }
    if (! on.isEmpty()) return on.joinIntoString (" + ");
    if (! waiting.isEmpty()) return tr ("Waiting for the grid") + " · " + waiting.joinIntoString (" + ");
    const auto q = valueText (core, "pads.quant");
    return tr ("Hold a pad - Shift-click latches") + " · " + tr ("Quantise") + " " + q;
}
} // namespace ohpe::ui::padsview
