// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "RecoveryNotice.h"

#include "Palette.h"
#include "plugin/Localisation.h"

namespace ohpe::ui
{
using i18n::tr;

RecoveryNotice::RecoveryNotice()
{
    setVisible (false);
    setInterceptsMouseClicks (true, false);
}

float RecoveryNotice::unit() const noexcept { return (float) getWidth() / 1200.0f; } // design units (the plate is 1200 wide)

void RecoveryNotice::show (Reason w, std::function<void()> onRestore)
{
    why = w;
    onRestoreFn = std::move (onRestore);
    hover = -1;
    setVisible (w != Reason::None);
    if (isVisible()) toFront (false);
    repaint();
}

void RecoveryNotice::restore()
{
    if (why != Reason::UncleanExit) { dismiss(); return; }
    auto fn = std::move (onRestoreFn);
    onRestoreFn = nullptr;
    why = Reason::None;
    setVisible (false);
    if (fn) fn(); // may show the notice again (RestoreFailed)
}

void RecoveryNotice::dismiss()
{
    onRestoreFn = nullptr;
    why = Reason::None;
    setVisible (false);
}

juce::String RecoveryNotice::title() const
{
    switch (why)
    {
        case Reason::UncleanExit: return tr ("The last session did not end properly");
        case Reason::StateDamaged: return tr ("The last session could not be loaded");
        case Reason::RestoreFailed: return tr ("The session could not be restored");
        case Reason::None: break;
    }
    return {};
}

juce::String RecoveryNotice::message() const
{
    switch (why)
    {
        case Reason::UncleanExit: return tr ("Ohpe started with the start sound. Restore the last session?");
        case Reason::StateDamaged: return tr ("Its saved state is damaged. Ohpe started with the start sound.");
        case Reason::RestoreFailed: return tr ("Ohpe keeps the start sound.");
        case Reason::None: break;
    }
    return {};
}

juce::String RecoveryNotice::keyText (int key) const
{
    if (why == Reason::UncleanExit) return key == 0 ? tr ("Restore") : tr ("Start fresh");
    return tr ("OK");
}

juce::Rectangle<float> RecoveryNotice::panelArea() const
{
    const float u = unit();
    const float w = 640.0f * u, h = 64.0f * u;
    return { ((float) getWidth() - w) * 0.5f, 14.0f * u, w, h };
}

juce::Rectangle<float> RecoveryNotice::keyArea (int key) const
{
    const float u = unit();
    auto row = panelArea().reduced (12.0f * u, 14.0f * u);
    const float w = 118.0f * u, gap = 8.0f * u;
    return { row.getRight() - w * (float) (key + 1) - gap * (float) key, row.getY(), w, row.getHeight() };
}

bool RecoveryNotice::hitTest (int x, int y) { return isVisible() && panelArea().contains ((float) x, (float) y); }

void RecoveryNotice::paint (juce::Graphics& g)
{
    if (why == Reason::None) return;
    const float u = unit();
    const auto p = panelArea();
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (p.translated (0.0f, 3.0f * u), radius::panel * u);
    g.setColour (colour::bezel);
    g.fillRoundedRectangle (p, radius::panel * u);
    const auto glass = p.reduced (3.0f * u);
    g.setColour (colour::display);
    g.fillRoundedRectangle (glass, radius::inset * u);

    // the light bar: violet = something other than the sound wants attention (the session)
    auto inner = glass.reduced (10.0f * u, 9.0f * u);
    const auto bar = inner.removeFromLeft (4.0f * u);
    g.setColour (colour::ultraviolet);
    g.fillRoundedRectangle (bar, 2.0f * u);
    inner.removeFromLeft (12.0f * u);
    inner.removeFromRight ((float) numKeys() * 126.0f * u);

    auto top = inner.removeFromTop (inner.getHeight() * 0.5f);
    g.setColour (colour::displayText);
    g.setFont (brandFont (15.0f * u));
    g.drawFittedText (title(), top.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    g.setColour (colour::displayDim.brighter (0.35f));
    g.setFont (labelFont (12.5f * u));
    g.drawFittedText (message(), inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

    for (int k = 0; k < numKeys(); ++k)
    {
        const auto r = keyArea (k);
        const bool primary = k == 0;
        g.setGradientFill (juce::ColourGradient (colour::capTop, 0.0f, r.getY(), colour::capBottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius::inset * u);
        g.setColour (hover == k ? colour::ultraviolet : colour::capEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), radius::inset * u, hover == k ? 1.5f : 1.0f);
        g.setColour (primary ? colour::ink : colour::capInk);
        g.setFont (labelFont (12.5f * u));
        g.drawFittedText (i18n::upper (keyText (k)), r.reduced (6.0f * u, 0).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }
}

void RecoveryNotice::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int k = 0; k < numKeys(); ++k)
        if (keyArea (k).contains (e.position)) h = k;
    if (h != hover) { hover = h; repaint(); }
}

void RecoveryNotice::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void RecoveryNotice::mouseUp (const juce::MouseEvent& e)
{
    if (keyArea (0).contains (e.position)) restore();
    else if (numKeys() > 1 && keyArea (1).contains (e.position)) dismiss();
}
// p82: the keys' tooltips
std::vector<tips::Region> RecoveryNotice::tipRegions()
{
    std::vector<tips::Region> r;
    if (numKeys() > 1)
    {
        r.push_back ({ keyArea (0), keyText (0) + " - " + tips::t ("bring back the session from before the crash", "die Sitzung von vor dem Absturz zurückholen") });
        r.push_back ({ keyArea (1), keyText (1) + " - " + tips::t ("start with a fresh instrument", "mit einem frischen Instrument beginnen") });
    }
    else
        r.push_back ({ keyArea (0), keyText (0) + " - " + tips::t ("close the notice", "den Hinweis schließen") });
    return r;
}

} // namespace ohpe::ui
