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
    const float ringR = size * 0.47f, ringW = size * 0.055f, capR = size * 0.36f;
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

    // flat cap with a soft shadow, a fine machined edge and an indicator line
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c.translated (0, size * 0.025f)));
    g.setGradientFill (juce::ColourGradient (colour::bodyLight, c.x, c.y - capR, colour::body.darker (0.06f), c.x, c.y + capR, false));
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
    g.setColour (colour::bodyShade.darker (0.1f));
    g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), size * 0.008f);
    const float a = start + (end - start) * pos;
    const auto p1 = c + juce::Point<float> (std::sin (a), -std::cos (a)) * capR * 0.35f;
    const auto p2 = c + juce::Point<float> (std::sin (a), -std::cos (a)) * capR * 0.85f;
    g.setColour (enabled ? colour::ink : colour::engraving.withAlpha (0.4f));
    g.drawLine ({ p1, p2 }, size * 0.035f);
}

void OhpeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colour::keyDarkTop : colour::keyDark);
    if (down) g.setColour (colour::keyDark.brighter (0.2f));
    else if (over) g.setColour (colour::keyDarkTop);
    g.fillRoundedRectangle (r, r.getHeight() * 0.18f);
    if (on)
    {
        g.setColour (colour::signalRed);
        g.fillRoundedRectangle (r.withTop (r.getBottom() - 3.0f).reduced (r.getWidth() * 0.3f, 0), 1.5f);
    }
}

juce::Font OhpeLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return labelFont (juce::jmin (15.0f, (float) h * 0.5f)); }
juce::Font OhpeLookAndFeel::getPopupMenuFont() { return labelFont (15.0f); }
juce::Font OhpeLookAndFeel::getComboBoxFont (juce::ComboBox& c) { return labelFont (juce::jmin (15.0f, (float) c.getHeight() * 0.55f)); }
juce::Font OhpeLookAndFeel::getLabelFont (juce::Label& l) { return labelFont (juce::jmin (15.0f, (float) l.getHeight() * 0.7f)); }
} // namespace ohpe::ui
