// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace ohpe
{
class OhpeProcessor;
}

namespace ohpe::ui::a11y
{
/**
    p83: accessibility and keyboard operation.

    Every control of the plate has an accessibility handler with a title (and value) in the current language
    (English / German, read when the screen reader asks - a language change needs no refresh). Controls take the
    keyboard focus from Tab / Shift+Tab (and F6 / Shift+F6 between the areas of the plate), never from a mouse click:
    a click leaves the focus on the keyboard, so the computer keyboard keeps playing notes after using the mouse.

    Keys on a focused control:
      - value controls (encoders, wheels, LIFE, GLIDE, PORTAMENTO, tempo): arrows one step, Page Up / Down ten steps,
        Shift finer, Delete / Backspace back to the default;
      - keys: Enter or Space press, Shift+F10 (or the context menu key) opens the key's menu (MIDI learn);
      - Esc gives the focus back to the keyboard (notes on the computer keyboard again).
*/

/** A control with a normalised value (0..1). Every function is optional except get / set. */
struct ValueAccess
{
    std::function<juce::String()> title; ///< translated
    std::function<float()> get;          ///< normalised 0..1
    std::function<void (float)> set;     ///< normalised 0..1 (one gesture)
    std::function<juce::String()> text;  ///< value text as the plate shows it
    std::function<int()> steps;          ///< number of positions (choices); 0 or less = continuous
    std::function<void()> reset;         ///< Delete / Backspace: the default
    std::function<bool()> enabled;
};

/** Value controls with up to this many positions step one position per arrow key. */
inline constexpr int maxSteps = 1024;
/** One key step of a value control in normalised units: a stepped value moves one position (Page: three, or ten on
    more than 24 positions), a continuous value 1 % (fine 0.1 %, Page 10 %). */
float keyStep (int steps, bool fine, bool page) noexcept;

/** Arrows, Page Up / Down, Delete / Backspace on a value control; true = used. */
bool handleValueKey (const juce::KeyPress& key, const ValueAccess& v);

/** A key / button: Enter and Space press it, Shift+F10 opens its secondary menu. */
struct PressAccess
{
    std::function<juce::String()> title;  ///< translated
    std::function<juce::String()> value;  ///< optional state text ("on", the shown value ...)
    std::function<void()> press;
    std::function<void()> secondary;      ///< optional: the right-click menu
    std::function<int()> checked;         ///< optional: -1 not a toggle, 0 off, 1 on
    std::function<bool()> enabled;
};

bool handlePressKey (const juce::KeyPress& key, const PressAccess& p);

std::unique_ptr<juce::AccessibilityHandler> valueHandler (juce::Component& c, ValueAccess v);
std::unique_ptr<juce::AccessibilityHandler> pressHandler (juce::Component& c, PressAccess p);
/** A read-only element (display, readouts): its title and the text it shows. */
std::unique_ptr<juce::AccessibilityHandler> textHandler (juce::Component& c, std::function<juce::String()> title,
                                                         std::function<juce::String()> value);

/** Takes the keyboard focus from Tab, never from a mouse click. */
void makeFocusable (juce::Component& c);

/** The host parameter's name in the current language (empty when unknown). */
juce::String paramTitle (OhpeProcessor& proc, const juce::String& paramId);
/** The host parameter's value as text (the plate's formatting). */
juce::String paramText (OhpeProcessor& proc, const juce::String& paramId);
/** ValueAccess for a host parameter. */
ValueAccess paramAccess (OhpeProcessor& proc, std::function<juce::String()> paramId, std::function<juce::String()> title = {});

/** fg moved towards black or white (whichever reaches it) until its contrast on bg is at least `ratio`. */
juce::Colour ensureContrast (juce::Colour fg, juce::Colour bg, double ratio);

/**
    The focus ring: a two-tone rounded outline (a dark line outside, the yellow edit light inside, so it stands out on
    the light housing, the night housing and the dark display alike) around the control that has the keyboard focus.
    Shown only when the focus came from the keyboard (Tab / F6). A full-size overlay that ignores the mouse.
*/
class FocusRing final : public juce::Component, private juce::FocusChangeListener
{
public:
    FocusRing();
    ~FocusRing() override;
    /** Tab / F6 moved the focus: show the ring. A mouse click hides it. */
    void setKeyboardNavigation (bool on);
    bool keyboardNavigation() const noexcept { return navigating; }
    /** The control the ring is drawn around (nullptr = none). */
    juce::Component* target() const;
    void setScale (float s) { scale = s; repaint(); }
    /** Draws the ring around `c` without a real keyboard focus (screenshots, tests; nullptr = the focus again). */
    void setTargetForTests (juce::Component* c) { forced = c; repaint(); }
    std::function<bool()> blocked; ///< true while an overlay covers the plate (no ring)
    void paint (juce::Graphics& g) override;

private:
    void globalFocusChanged (juce::Component*) override;
    bool navigating = false;
    float scale = 1.0f;
    juce::Component::SafePointer<juce::Component> forced;
};
} // namespace ohpe::ui::a11y
