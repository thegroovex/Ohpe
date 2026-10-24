// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FirstStartTour.h"

#include "Palette.h"
#include "plugin/Localisation.h"

namespace ohpe::ui
{
using i18n::tr;

FirstStartTour::FirstStartTour()
{
    setVisible (false);
    setInterceptsMouseClicks (true, false); // while open the plate waits: only the card's keys act
    setWantsKeyboardFocus (true);
}

float FirstStartTour::unit() const noexcept { return (float) getWidth() / 1200.0f; } // design units (plate 1200 wide)

juce::Rectangle<float> FirstStartTour::scaled (juce::Rectangle<float> r) const noexcept
{
    const float u = unit();
    return { r.getX() * u, r.getY() * u, r.getWidth() * u, r.getHeight() * u };
}

void FirstStartTour::show (int s)
{
    current = juce::jlimit (0, numSteps - 1, s);
    hover = -1;
    setVisible (true);
    toFront (true);
    repaint();
}

void FirstStartTour::next()
{
    if (current >= numSteps - 1) { finish(); return; }
    ++current;
    hover = -1;
    repaint();
}

void FirstStartTour::back()
{
    if (current == 0) return;
    --current;
    hover = -1;
    repaint();
}

void FirstStartTour::finish()
{
    if (! isVisible()) return;
    setVisible (false);
    if (onFinished) onFinished();
}

juce::String FirstStartTour::title (int s)
{
    switch (s)
    {
        case 0: return tr ("Parts, presets and the library");
        case 1: return tr ("Encoders, soft keys and the FX editor");
        case 2: return tr ("Tape, sequencer, tempo and key");
        case 3: return tr ("Help, settings and the Grid-Controller");
        default: return {};
    }
}

juce::String FirstStartTour::text (int s)
{
    switch (s)
    {
        case 0: return tr ("Four parts play at once - choose the one you edit at the top. The preset strip steps through "
                           "the sounds; LIBRARY opens every preset with search, favourites and content packs.");
        case 1: return tr ("The four encoders below the display set the four values shown above them. The soft keys at the "
                           "foot of the display switch pages and functions. On the Effects page, click the effect picture "
                           "to open the FX editor.");
        case 2: return tr ("SEQUENCER and TAPE are pages of the mode column: steps for every part and an eight-track tape. "
                           "The transport plays, records and loops; the tempo sits next to the tape position, the key at "
                           "the top of the display.");
        case 3: return tr ("The gear key opens the settings: language, night skin, the help pages and the help overlay (F1), "
                           "MIDI and the Grid-Controller, found by itself when it is plugged in. This tour is there too.");
        default: return {};
    }
}

std::vector<juce::Rectangle<float>> FirstStartTour::highlights (int s)
{
    // design units of OhpeEditor::designArea / resized()
    switch (s)
    {
        case 0: return { { 304, 40, 152, 32 }, { 472, 40, 232, 32 }, { 720, 20, 64, 56 } };  // parts, preset strip, LIBRARY
        case 1: return { { 256, 112, 688, 336 }, { 280, 460, 640, 148 } };                    // display, the four encoders
        case 2: return { { 960, 112, 208, 336 }, { 32, 464, 208, 144 } };                     // mode column, transport
        case 3: return { { 1094, 20, 66, 56 } };                                              // the gear key
        default: return {};
    }
}

juce::Rectangle<float> FirstStartTour::cardDesign (int s)
{
    switch (s)
    {
        case 0: return { 340, 136, 520, 176 };
        case 1: return { 340, 616, 520, 168 };
        case 2: return { 340, 180, 520, 176 };
        case 3: return { 604, 104, 520, 176 };
        default: return { 340, 180, 520, 176 };
    }
}

juce::String FirstStartTour::keyText (int key) const
{
    switch (key)
    {
        case NextKey: return i18n::upper (current >= numSteps - 1 ? tr ("Done") : tr ("Next"));
        case BackKey: return i18n::upper (tr ("Back"));
        case SkipKey: return i18n::upper (tr ("Skip tour"));
        default: return {};
    }
}

juce::Rectangle<float> FirstStartTour::cardArea() const { return scaled (cardDesign (current)); }

juce::Rectangle<float> FirstStartTour::keyArea (int key) const
{
    const float u = unit();
    const auto row = cardArea().reduced (16.0f * u, 0.0f).removeFromBottom (44.0f * u).withTrimmedBottom (14.0f * u);
    const float w = 96.0f * u, gap = 8.0f * u;
    switch (key)
    {
        case NextKey: return row.withLeft (row.getRight() - w);
        case BackKey: return row.withLeft (row.getRight() - 2.0f * w - gap).withWidth (w);
        case SkipKey: return row.withWidth (w + 24.0f * u);
        default: return {};
    }
}

void FirstStartTour::press (int key)
{
    if (! hasKey (key)) return;
    if (key == NextKey) next();
    else if (key == BackKey) back();
    else if (key == SkipKey) finish();
}

void FirstStartTour::paint (juce::Graphics& g)
{
    const float u = unit();
    // dim the plate, leave the step's areas clear
    juce::Path veil;
    veil.addRectangle (getLocalBounds().toFloat());
    const auto marks = highlights (current);
    for (const auto& h : marks)
        veil.addRoundedRectangle (scaled (h).expanded (4.0f * u), radius::panel * u);
    veil.setUsingNonZeroWinding (false);
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillPath (veil);
    g.setColour (colour::sunYellow);
    for (const auto& h : marks)
        g.drawRoundedRectangle (scaled (h).expanded (4.0f * u), radius::panel * u, 2.0f * u);

    // the card: the display's frame and glass, as the other notices over the plate
    const auto card = cardArea();
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (card.translated (0.0f, 3.0f * u), radius::panel * u);
    g.setColour (colour::bezel);
    g.fillRoundedRectangle (card, radius::panel * u);
    const auto glass = card.reduced (3.0f * u);
    g.setColour (colour::display);
    g.fillRoundedRectangle (glass, radius::inset * u);

    auto inner = glass.reduced (16.0f * u, 12.0f * u);
    inner.removeFromBottom (32.0f * u); // the keys
    // step dots: yellow = this step
    {
        auto dots = inner.removeFromTop (14.0f * u);
        const float d = 6.0f * u;
        for (int i = 0; i < numSteps; ++i)
        {
            g.setColour (i == current ? colour::sunYellow : colour::displayDim);
            g.fillEllipse (dots.getX() + (float) i * 12.0f * u, dots.getCentreY() - d * 0.5f, d, d);
        }
        g.setColour (colour::displayDim.brighter (0.35f));
        g.setFont (labelFont (11.5f * u));
        g.drawText (juce::String (current + 1) + " / " + juce::String (numSteps), dots.withTrimmedLeft (56.0f * u),
                    juce::Justification::centredLeft, false);
    }
    inner.removeFromTop (4.0f * u);
    g.setColour (colour::displayText);
    g.setFont (brandFont (17.0f * u));
    g.drawFittedText (title (current), inner.removeFromTop (24.0f * u).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    inner.removeFromTop (4.0f * u);
    g.setColour (colour::displayText.withAlpha (0.82f));
    g.setFont (labelFont (13.0f * u));
    g.drawFittedText (text (current), inner.toNearestInt(), juce::Justification::topLeft, 4, 0.9f);

    for (int k = 0; k < numKeys; ++k)
    {
        if (! hasKey (k)) continue;
        const auto r = keyArea (k);
        if (k == SkipKey) // a quiet text key on the glass
        {
            g.setColour (hover == k ? colour::displayText : colour::displayDim.brighter (0.35f));
            g.setFont (labelFont (12.0f * u));
            g.drawFittedText (keyText (k), r.toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
            continue;
        }
        g.setGradientFill (juce::ColourGradient (colour::capTop, 0.0f, r.getY(), colour::capBottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius::inset * u);
        g.setColour (hover == k ? colour::sunYellow : colour::capEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), radius::inset * u, hover == k ? 1.5f : 1.0f);
        g.setColour (k == NextKey ? colour::ink : colour::capInk);
        g.setFont (labelFont (12.5f * u));
        g.drawFittedText (keyText (k), r.reduced (6.0f * u, 0).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }
}

void FirstStartTour::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int k = 0; k < numKeys; ++k)
        if (hasKey (k) && keyArea (k).contains (e.position)) h = k;
    if (h != hover) { hover = h; repaint(); }
}

void FirstStartTour::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void FirstStartTour::mouseUp (const juce::MouseEvent& e)
{
    for (int k = 0; k < numKeys; ++k)
        if (hasKey (k) && keyArea (k).contains (e.position)) { press (k); return; }
}

bool FirstStartTour::keyPressed (const juce::KeyPress& k)
{
    if (! isVisible()) return false;
    if (k == juce::KeyPress::escapeKey) { finish(); return true; }
    if (k == juce::KeyPress::rightKey || k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { next(); return true; }
    if (k == juce::KeyPress::leftKey) { back(); return true; }
    return true; // the plate waits while the tour is open
}
// p82: the keys' tooltips
std::vector<tips::Region> FirstStartTour::tipRegions()
{
    std::vector<tips::Region> r;
    if (hasKey (NextKey))
        r.push_back ({ keyArea (NextKey), current == numSteps - 1 ? tips::t ("Done: close the tour (Return)", "Fertig: die Tour schließen (Eingabe)")
                                                                  : tips::t ("Next step (Return, right arrow)", "Nächster Schritt (Eingabe, Pfeil rechts)") });
    if (hasKey (BackKey)) r.push_back ({ keyArea (BackKey), tips::t ("The step before (left arrow)", "Der Schritt davor (Pfeil links)") });
    if (hasKey (SkipKey)) r.push_back ({ keyArea (SkipKey), tips::t ("Skip the tour (Esc) - Settings > Help shows it again", "Die Tour überspringen (Esc) - Einstellungen > Hilfe zeigt sie wieder") });
    return r;
}

} // namespace ohpe::ui
