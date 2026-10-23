// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ConfirmDialog.h"

#include "Palette.h"
#include "plugin/Localisation.h"

namespace ohpe::ui
{
ConfirmDialog::ConfirmDialog()
{
    setWantsKeyboardFocus (true);
    setVisible (false);
}

float ConfirmDialog::unit() const noexcept { return (float) getWidth() / 1200.0f; } // design units (the plate is 1200 wide)

void ConfirmDialog::show (Options o)
{
    opts = std::move (o);
    focus = 0;
    hover = -1;
    setVisible (true);
    toFront (true);
    if (isShowing()) grabKeyboardFocus();
    repaint();
}

void ConfirmDialog::confirm() { close (true); }
void ConfirmDialog::cancel() { close (false); }

void ConfirmDialog::close (bool confirmed)
{
    if (! isVisible()) return;
    setVisible (false);
    auto fn = confirmed ? opts.onConfirm : opts.onCancel; // the callback may open the dialog again
    opts.onConfirm = nullptr;
    opts.onCancel = nullptr;
    if (fn) fn();
}

juce::Rectangle<float> ConfirmDialog::panelArea() const
{
    const float u = unit();
    return getLocalBounds().toFloat().withSizeKeepingCentre (520.0f * u, 196.0f * u);
}

juce::Rectangle<float> ConfirmDialog::keyArea (int key) const
{
    const float u = unit();
    auto p = panelArea().reduced (24.0f * u);
    auto row = p.removeFromBottom (44.0f * u);
    const float w = 168.0f * u, gap = 12.0f * u;
    // CANCEL left of the destructive key, both at the right end of the panel
    return { row.getRight() - w * (float) (2 - key) - gap * (float) (1 - key), row.getY(), w, row.getHeight() };
}

void ConfirmDialog::paint (juce::Graphics& g)
{
    const float u = unit();
    // the plate stays visible behind the dimmed backdrop (black in both skins: the night skin's ink is light)
    g.fillAll (juce::Colours::black.withAlpha (currentSkin() == Skin::Dark ? 0.55f : 0.38f));

    const auto p = panelArea();
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (p.translated (0.0f, 4.0f * u), radius::panel * u);
    g.setColour (colour::body);
    g.fillRoundedRectangle (p, radius::panel * u);
    g.setColour (colour::panelEdge);
    g.drawRoundedRectangle (p.reduced (0.5f), radius::panel * u, 1.0f);

    auto text = p.reduced (24.0f * u);
    // a warning sign: the alert ink as a small filled triangle with an exclamation mark
    auto sign = text.removeFromTop (30.0f * u);
    {
        auto tri = sign.removeFromLeft (30.0f * u).reduced (2.0f * u);
        juce::Path t;
        t.addTriangle (tri.getCentreX(), tri.getY(), tri.getRight(), tri.getBottom(), tri.getX(), tri.getBottom());
        g.setColour (colour::alertInk);
        g.fillPath (t);
        g.setColour (colour::body);
        const float cx = tri.getCentreX();
        g.fillRoundedRectangle (cx - 1.4f * u, tri.getY() + tri.getHeight() * 0.36f, 2.8f * u, tri.getHeight() * 0.34f, 1.2f * u);
        g.fillEllipse (cx - 1.6f * u, tri.getBottom() - tri.getHeight() * 0.22f, 3.2f * u, 3.2f * u);
    }
    sign.removeFromLeft (10.0f * u);
    g.setColour (colour::ink);
    g.setFont (brandFont (19.0f * u));
    g.drawFittedText (opts.title, sign.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    text.removeFromTop (10.0f * u);
    g.setColour (colour::engraving);
    g.setFont (labelFont (14.0f * u));
    g.drawFittedText (opts.message, text.withTrimmedBottom (54.0f * u).toNearestInt(), juce::Justification::topLeft, 3, 0.9f);

    for (int k = 0; k < 2; ++k)
    {
        const auto r = keyArea (k);
        const bool destructive = k == 1;
        const auto ink = destructive ? colour::alertInk : colour::capInk;
        g.setGradientFill (juce::ColourGradient (colour::capTop, 0.0f, r.getY(), colour::capBottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius::inset * u);
        g.setColour (hover == k ? ink : colour::capEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), radius::inset * u, hover == k ? 1.5f : 1.0f);
        if (focus == k) // the focused key (Return presses it): a ring in its ink
        {
            g.setColour (ink.withAlpha (0.85f));
            g.drawRoundedRectangle (r.expanded (3.0f * u), radius::inset * u + 3.0f * u, 1.6f * u);
        }
        g.setColour (ink);
        g.setFont (labelFont (14.0f * u));
        g.drawFittedText (i18n::upper (destructive ? opts.confirmText : opts.cancelText), r.reduced (8.0f * u, 0).toNearestInt(),
                          juce::Justification::centred, 1, 0.8f);
    }
}

void ConfirmDialog::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int k = 0; k < 2; ++k)
        if (keyArea (k).contains (e.position)) h = k;
    if (h != hover) { hover = h; repaint(); }
}

void ConfirmDialog::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void ConfirmDialog::mouseUp (const juce::MouseEvent& e)
{
    if (keyArea (1).contains (e.position)) confirm();
    else if (keyArea (0).contains (e.position) || ! panelArea().contains (e.position)) cancel();
}

bool ConfirmDialog::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { cancel(); return true; }
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { close (focus == 1); return true; }
    if (k == juce::KeyPress::leftKey || k == juce::KeyPress::rightKey || k == juce::KeyPress::tabKey)
    {
        focus = 1 - focus;
        repaint();
        return true;
    }
    return true; // modal: nothing reaches the plate behind it
}
// p82: the keys' tooltips
std::vector<tips::Region> ConfirmDialog::tipRegions()
{
    return { { keyArea (0), opts.cancelText + " - " + tips::t ("keep everything as it is (Esc)", "alles lassen, wie es ist (Esc)") },
             { keyArea (1), opts.confirmText + " - " + tips::t ("do it (cannot be undone)", "ausführen (lässt sich nicht rückgängig machen)") },
             { panelArea(), tips::t ("Choose with a key - Return: the focused key, Esc: cancel", "Mit einer Taste wählen - Eingabe: die markierte Taste, Esc: abbrechen") },
             { getLocalBounds().toFloat(), tips::t ("A click outside the question cancels", "Ein Klick neben die Frage bricht ab") } };
}

} // namespace ohpe::ui
