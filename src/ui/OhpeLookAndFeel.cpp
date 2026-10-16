// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeLookAndFeel.h"

#include "Palette.h"

namespace ohpe::ui
{
OhpeLookAndFeel::OhpeLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colour::keyDark);
    setColour (juce::PopupMenu::textColourId, colour::keyLight);
    setColour (juce::PopupMenu::headerTextColourId, colour::sunYellow);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::ultraviolet);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, colour::keyDark);
    setColour (juce::TextButton::buttonOnColourId, colour::signalRed);
    setColour (juce::TextButton::textColourOffId, colour::keyLight);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, colour::keyDark);
    setColour (juce::ComboBox::textColourId, colour::keyLight);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, colour::sunYellow);
    setColour (juce::Label::textColourId, colour::ink);
    setColour (juce::ResizableWindow::backgroundColourId, colour::body);
    setColour (juce::DocumentWindow::textColourId, colour::ink);
    setColour (juce::AlertWindow::backgroundColourId, colour::body);
    setColour (juce::AlertWindow::textColourId, colour::ink);
    setColour (juce::ListBox::backgroundColourId, colour::bodyLight);
    setColour (juce::ToggleButton::textColourId, colour::ink);
    setColour (juce::ToggleButton::tickColourId, colour::signalRed);
}

void OhpeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float ringR = size * 0.465f, ringW = size * 0.06f, capR = size * 0.35f;
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled();

    // ring track + value arc
    juce::Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, start, end, true);
    g.setColour (colour::bodyShade);
    g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (enabled)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, start, start + (end - start) * pos, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 0.7: modulation ring inside the value arc - the reach of the matrix routings, the live position as a dot
    if (enabled && (bool) s.getProperties().getWithDefault ("modAny", false))
    {
        const float lo = (float) s.getProperties()["modLo"], hi = (float) s.getProperties()["modHi"], live = (float) s.getProperties()["modLive"];
        const float modR = ringR - ringW * 1.15f;
        juce::Path reach;
        reach.addCentredArc (c.x, c.y, modR, modR, 0.0f, start + (end - start) * lo, start + (end - start) * juce::jmax (hi, lo + 0.004f), true);
        g.setColour (accent.withAlpha (0.45f));
        g.strokePath (reach, juce::PathStrokeType (ringW * 0.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float la = start + (end - start) * live;
        g.setColour (colour::ink);
        g.fillEllipse (juce::Rectangle<float> (ringW * 0.8f, ringW * 0.8f).withCentre (c + juce::Point<float> (std::sin (la), -std::cos (la)) * modR));
    }

    // p14: a matte cap on a soft shadow, a fine edge, the lit upper half and an ink pointer line
    const auto capBox = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillEllipse (capBox.expanded (size * 0.03f).translated (0, size * 0.045f));
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillEllipse (capBox.translated (0, size * 0.02f));
    g.setGradientFill (juce::ColourGradient (colour::capTop, c.x, c.y - capR, colour::capBottom.darker (0.04f), c.x, c.y + capR, false));
    g.fillEllipse (capBox);
    g.setColour (colour::capEdge);
    g.drawEllipse (capBox.reduced (0.5f), juce::jmax (1.0f, size * 0.008f));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    juce::Path lit;
    lit.addCentredArc (c.x, c.y, capR - 1.5f, capR - 1.5f, 0.0f, -1.1f, 1.1f, true);
    g.strokePath (lit, juce::PathStrokeType (1.0f));
    const float a = start + (end - start) * pos;
    const auto p1 = c + juce::Point<float> (std::sin (a), -std::cos (a)) * capR * 0.30f;
    const auto p2 = c + juce::Point<float> (std::sin (a), -std::cos (a)) * capR * 0.84f;
    g.setColour (enabled ? colour::ink : colour::engraving.withAlpha (0.4f));
    g.drawLine ({ p1, p2 }, juce::jmax (1.5f, size * 0.035f));
}

void OhpeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    // p14: small matte pills (EN / DE); the selected one dark, the other light
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillRoundedRectangle (r.translated (0, down ? 0.5f : 1.2f), rad);
    if (on) g.setGradientFill (juce::ColourGradient (colour::pillTop, 0, r.getY(), colour::pill, 0, r.getBottom(), false));
    else g.setGradientFill (juce::ColourGradient (colour::capTop, 0, r.getY(), colour::capBottom, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? juce::Colours::white.withAlpha (0.06f) : colour::capEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    if (over && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (on ? 0.06f : 0.25f));
        g.fillRoundedRectangle (r, rad);
    }
}

void OhpeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? colour::keyLight : colour::engraving);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font OhpeLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return brandFont (juce::jmin (15.0f, (float) h * 0.46f)); }
juce::Font OhpeLookAndFeel::getPopupMenuFont() { return labelFont (15.0f); }
juce::Font OhpeLookAndFeel::getComboBoxFont (juce::ComboBox& c) { return labelFont (juce::jmin (15.0f, (float) c.getHeight() * 0.55f)); }
juce::Font OhpeLookAndFeel::getLabelFont (juce::Label& l) { return labelFont (juce::jmin (15.0f, (float) l.getHeight() * 0.7f)); }
} // namespace ohpe::ui
