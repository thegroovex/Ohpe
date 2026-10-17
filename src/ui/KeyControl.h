// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Widgets.h"

namespace ohpe::ui
{
/**
    p45: the key on the display's header, left of TAP and the tempo - root, scale and Scale Lock ("F# DOR" + lock).
      drag up / down       root by semitones (1 per 12 pixels); Shift / Cmd / Ctrl: the scale
      mouse wheel          root +- 1 (Shift: the scale)
      click                menu: root, scale, Scale Lock on / off, Nearest / White Keys, MIDI learn of all four
      click on the lock    Scale Lock on / off; right-click on the lock: MIDI learn / clear / reset of "key.lock"
      right-click          the same menu as a click
    When the host gives the key (KeyScale::fromHost) root and scale are read-only and HOST is shown; the lock stays editable.
*/
class KeyReadout : public juce::Component, public juce::TooltipClient
{
public:
    explicit KeyReadout (OhpeProcessor& p);
    /** Repaints when the shown key changed (the editor's timer). */
    void refresh();
    /** "F# DOR" (root spelled for the key; German: H = B, B = B flat, "H DUR"); HOST before it while the host gives it. */
    juce::String shownText() const;
    static juce::String shortScaleName (int scale); ///< "DOR", "MAJ", "HMOLL", ... (current language, upper case)
    /** Width the readout needs at header height h for any key (the widest root and scale name; HOST). */
    static float preferredWidth (float h, bool host);
    juce::Rectangle<float> lockArea() const;

    // menu (public for tests): ids
    enum MenuId { RootBase = 100, ScaleBase = 200, LockToggle = 300, LockModeBase = 310, LearnBase = 400 };
    void showMenu();
    void applyMenuResult (int id);

    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    OhpeProcessor& proc;
    keys::KeyScale shown;
    int dragStartRoot = 0, dragStartScale = 0;
    bool dragging = false, moved = false, onLock = false;
};

/** p45: draws a padlock (closed = on) into `area`. */
void drawLockSign (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, bool closed);

/**
    p45: the voice strip of the default display pages - 16 voice dots, the VOICES legend over the last note (they
    never overlap: the note's room is the widest note name, the dots shrink to the rest) and the chord that sounds,
    under the dots. Pure layout, used by Display and the tests.
*/
struct VoiceStripLayout
{
    juce::Rectangle<float> dots;  ///< the row of 16 dots
    float dot = 0.0f, step = 0.0f; ///< dot diameter, distance of the dot centres
    juce::Rectangle<float> legend; ///< "VOICES"
    juce::Rectangle<float> note;   ///< the last note
    juce::Rectangle<float> chord;  ///< the chord name (under the dots)
    float legendFont = 0.0f, noteFont = 0.0f, chordFont = 0.0f;
};
VoiceStripLayout layoutVoiceStrip (juce::Rectangle<float> area, bool softKeys, const juce::String& legend);
} // namespace ohpe::ui
