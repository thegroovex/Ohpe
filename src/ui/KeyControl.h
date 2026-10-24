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
    /** p45: compact form for a crowded header ("F#m", "Cdor", "Hmoll" -> "Hm"): what is painted while compact. */
    juce::String compactText() const;
    /** p75: the full form the header paints: key and scale ("C# PHRYG"); a host key is violet instead of "HOST". */
    juce::String headerText() const;
    juce::String paintedText() const { return compact ? compactText() : headerText(); }
    /** p75: header text sizes (fraction of the header height), shared with the display's layout and its tests. */
    static constexpr float headerKeyFont = 0.44f, headerKeyCompactFont = 0.42f;
    /** p75: the text size in use (the display lowers it a little on a crowded header so the full key still fits). */
    void setTextSize (float fraction) { if (fraction != textSize) { textSize = fraction; repaint(); } }
    float textSizeFraction (bool compactForm) const noexcept { return compactForm ? juce::jmin (textSize, headerKeyCompactFont) : textSize; }
    /** Width the readout needs at header height h for the text it shows now (full or compact). */
    float neededWidth (float h, bool compactForm) const;
    void setCompact (bool c) { if (c != compact) { compact = c; repaint(); } }
    bool isCompact() const noexcept { return compact; }
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
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override; ///< p83: the key (read-only)

private:
    OhpeProcessor& proc;
    keys::KeyScale shown;
    int dragStartRoot = 0, dragStartScale = 0;
    bool dragging = false, moved = false, onLock = false, compact = false;
    float textSize = headerKeyFont;
};

/**
    p70: Key Gravity on the display's header, right next to the key and its lock - a small ring that shows the edit
    part's gravity amount (pN.grav.amt; empty at 0 %, the accent colour of its encoder).
      click                opens the Key Gravity encoder page (onOpen)
      drag up / down       the amount (1 % per 1.5 pixels; Shift: fine)
      mouse wheel          the amount +- 5 % (Shift: 1 %)
      right-click          MIDI learn / clear / reset, as every control
    Hidden when the part has no Key Gravity stage. No parameters of its own.
*/
class GravityControl : public juce::Component, public juce::TooltipClient
{
public:
    explicit GravityControl (OhpeProcessor& p);
    void setPart (int part);
    int part() const noexcept { return editPart; }
    juce::String paramId() const; ///< "p1.grav.amt"
    bool available() const;       ///< the part has the stage
    float amount() const;         ///< 0 .. 1
    float shownAmount() const noexcept { return shown; } ///< what the ring shows (after refresh)
    /** Repaints when the amount changed (the editor's timer). */
    void refresh();
    /** Width the ring needs at header height h. */
    static float preferredWidth (float h) noexcept { return h * 0.72f; } // p75: was 0.8
    std::function<void()> onOpen;

    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override; ///< p83: the amount
    bool keyPressed (const juce::KeyPress& key) override;                             ///< p83: arrows / Page, Enter opens the page

private:
    void setAmount (float a);
    OhpeProcessor& proc;
    int editPart = 0;
    float shown = 0.0f, dragStart = 0.0f;
    bool dragging = false, moved = false;
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
