// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Widgets.h"

namespace ohpe::ui
{
/**
    p39: the tempo, directly editable wherever it is shown.
      drag up / down       1 BPM per 4 pixels (Shift / Cmd / Ctrl: 0.1 BPM)
      mouse wheel          +- 1 BPM (fine: 0.1)
      double-click         type a value (20 .. 300), Return sets it, Escape cancels
      right-click          MIDI learn / clear / reset of "tempo"
    When the tempo follows the host or an incoming MIDI clock it is read-only and shows HOST / CLOCK.
    Style Header: plain text on the display's header ("120.0 BPM"); Style Panel: a glass in the transport panel.
*/
class TempoReadout : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Style { Header, Panel };
    TempoReadout (OhpeProcessor& p, Style s);
    void setUiScale (float s) { if (s != uiScale) { uiScale = s; repaint(); } }
    /** Repaints when the shown value changed (the editor's timer). */
    void refresh();
    /** Opens the text field (double-click; public for tests). */
    void showTextEntry();
    bool isTextEntryOpen() const noexcept { return entry.isVisible(); }
    /** Text field: sets the tempo from text ("128", "92.5"); false = not a number in range. Public for tests. */
    bool commitText (const juce::String& text);
    juce::String shownText() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    void hideTextEntry();
    OhpeProcessor& proc;
    Style style;
    float uiScale = 1.0f;
    float dragStartBpm = 120.0f, shownBpm = -1.0f;
    bool dragging = false, shownLocked = false, shownRunning = false;
    juce::TextEditor entry;
};

/**
    p39: the click and count-in keys of the transport panel, on one glass:
      metronome   click on / off ("metro.on"); flashes on every beat while it clicks; right-click: level, accent,
                  click in a plug-in, MIDI learn
      count-in    Off -> 1 bar -> 2 bars ("metro.countin"); during a count-in the beats left in red;
                  right-click: MIDI learn
*/
class ClickStrip : public juce::Component, public juce::TooltipClient
{
public:
    explicit ClickStrip (OhpeProcessor& p);
    void setUiScale (float s) { if (s != uiScale) { uiScale = s; repaint(); } }
    void refresh(); ///< the editor's timer: flash, count-in
    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override { hover = fieldAt (e.position); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    enum Field { ClickField = 0, CountInField, numFields };
    juce::Rectangle<float> fieldArea (int f) const;
    int fieldAt (juce::Point<float> p) const;
    void clickField (int f); ///< what a left click does (public for tests)

private:
    void showMetronomeMenu();
    OhpeProcessor& proc;
    float uiScale = 1.0f, flash = 0.0f;
    uint32_t clicksSeen = 0;
    int hover = -1, shownState = -1;
    bool flashDown = false;
};

/** p39: the TAP key on the display's header (every tap measures; the tempo is set from the second tap on). */
class TapKey : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TapKey (OhpeProcessor& p);
    void refresh();
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    OhpeProcessor& proc;
    float lit = 0.0f;
    int tapsSeen = 0;
};

/** p39: draws the metronome sign (a pyramid with its pendulum) into `area`. */
void drawMetronomeSign (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, float swing);

/** p39: sets a host parameter from the screen (gesture, host notified, registry at once). */
void setParamFromScreen (OhpeProcessor& proc, const char* id, float plain);
} // namespace ohpe::ui
