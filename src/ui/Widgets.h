// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Pictograms.h"
#include "plugin/PluginProcessor.h"

#include <functional>
#include <memory>

namespace ohpe::ui
{
/** Opens the right-click menu of a control: MIDI learn, clear mapping, reset. */
void showControlMenu (OhpeProcessor& proc, const juce::String& paramId, juce::Component* target);

// ---------------------------------------------------------------- 0.3: direct manipulation
/** Shift, Cmd (macOS) or Ctrl held: fine control (a tenth of the normal speed). */
inline bool isFineDrag (const juce::ModifierKeys& m) noexcept { return m.isShiftDown() || m.isCommandDown() || m.isCtrlDown(); }
constexpr float fineFactor = 0.1f;

/**
    The value a drag produces: dragging right or up by `lengthPixels` covers the whole range
    (a tenth of it with fine control). Result clamped to 0..1.
*/
float dragToNormalised (float startNorm, float deltaPixels, float lengthPixels, bool fine) noexcept;

/** Relative drag on a bar; re-anchors when fine control is switched during the drag. */
struct BarDrag
{
    void begin (float norm, juce::Point<float> pos, bool fineMode) noexcept;
    float update (juce::Point<float> pos, float lengthPixels, bool fineMode) noexcept;
    float startNorm = 0.0f, lastNorm = 0.0f;
    juce::Point<float> anchor;
    bool fine = false;
};

/**
    A control that is not a host parameter (0.3): a step field of the sequencer or a parameter
    lock. Encoders and display rows can show one instead of a parameter.
*/
struct VirtualControl
{
    juce::String paramId;                     ///< the parameter it stands for (locks), may be empty
    juce::String name;                        ///< already translated
    std::function<float()> get;               ///< normalised 0..1
    std::function<void (float)> set;          ///< normalised 0..1
    std::function<juce::String (float)> text; ///< value text for a normalised value
    std::function<void()> reset;              ///< double-click: default / remove the lock
    bool locked = false;                      ///< shows the lock marker (value is a step's lock)
};
using VirtualPtr = std::shared_ptr<VirtualControl>;

/** Writes a normalised value to a host parameter as one gesture. */
void setParameterNormalised (OhpeProcessor& proc, const juce::String& paramId, float norm);

/** Round flat encoder with a coloured ring; bound to one host parameter at a time. */
class Encoder : public juce::Component
{
public:
    Encoder (OhpeProcessor& p, int index);
    ~Encoder() override;

    void bind (const juce::String& paramId); ///< empty = free (encoder dimmed)
    /** 0.3: shows a virtual control (step field, lock) instead of a parameter; nullptr = back to bind(). */
    void bindVirtual (VirtualPtr v);
    const juce::String& boundId() const noexcept { return paramId; }
    const VirtualPtr& boundVirtual() const noexcept { return virt; }
    void refreshVirtual(); ///< re-reads the virtual control's value (timer)
    /** 0.7: modulation ring - the reach of the matrix routings (normalised lo..hi) and the live position. */
    void setModulation (bool any, float lo, float hi, float live);

    void paint (juce::Graphics& g) override;
    void resized() override;

    float normalisedValue() const;

private:
    class Knob : public juce::Slider
    {
    public:
        explicit Knob (Encoder& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        Encoder& owner;
    };

    OhpeProcessor& proc;
    int index;
    juce::String paramId;
    VirtualPtr virt;
    Knob knob { *this };
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

/** Matte key with a pictogram and a legend printed below (p14: light square cap, dark pill or round key). */
class PictoKey : public juce::Button
{
public:
    enum class Style
    {
        Cap,   ///< matte light square key (engines, modes, transport, octave)
        Pill,  ///< dark pill (LIBRARY, PRO in the header)
        Round  ///< round key with the four-colour ring (RANDOM, SAVE in the header)
    };

    PictoKey (Picto picto, juce::String legendEnglish);
    void setStyle (Style s) { style = s; repaint(); }
    /** p14: the editor's scale (component pixels per design unit): legends share one size on every key. */
    void setUiScale (float s) { if (s != uiScale) { uiScale = s; repaint(); } }
    Style getStyle() const noexcept { return style; }

    void setActive (bool a) { if (a != active) { active = a; repaint(); } }
    bool isActive() const noexcept { return active; }
    void setAvailable (bool a) { available = a; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setLegend (juce::String englishText) { legend = std::move (englishText); repaint(); }
    /** p11: the pictogram keeps this colour even when the key is not active (record = red). */
    void setPictoColour (juce::Colour c) { pictoInk = c; hasPictoInk = true; repaint(); }

    std::function<void()> onSecondaryClick;

    void paintButton (juce::Graphics& g, bool over, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    Picto picto;
    juce::String legend;
    Style style = Style::Cap;
    float uiScale = 1.0f;
    bool active = false, available = true;
    juce::Colour accent, pictoInk;
    bool hasPictoInk = false;
};

/** Draws a legend in engraved small caps, shrinking the font (down to 70 %) until it fits the width. */
void drawFittedLegend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height, juce::Justification j = juce::Justification::centred);

/** p14: three-octave keyboard with matte keys. */
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    static constexpr int numOctaves = 3;
    explicit Keyboard (juce::MidiKeyboardState& s);
    void setBaseOctave (int octave); ///< lowest C = 12 * (octave + 1)
    int getBaseOctave() const noexcept { return baseOctave; }
    void resized() override;

    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour) override;
    void drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour) override;

private:
    int baseOctave = 3;
};

/**
    p14: performance wheel left of the keyboard - a vertical thumb wheel in a recessed slot, bound to
    a host parameter (MIDI-learnable with a right-click). The pitch wheel springs back to the centre
    when released; the mod wheel stays where it is left. Shift / Cmd / Ctrl = fine.
*/
class Wheel : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Wheel (OhpeProcessor& p, juce::String paramId, juce::String legendEnglish, bool springBack);
    void refresh();                  ///< re-reads the parameter (editor timer); repaints only on change
    float value() const;             ///< normalised 0..1
    bool springsBack() const noexcept { return spring; }
    const juce::String& paramId() const noexcept { return id; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    juce::Rectangle<float> slotArea() const;

private:
    void timerCallback() override;
    void set (float norm);
    OhpeProcessor& proc;
    juce::String id, legend;
    bool spring = false, dragging = false;
    float shown = -1.0f;
    BarDrag drag;
};

/** p14: three-position slide switch for a choice parameter (glide mode Auto / Legato / Always). */
class SlideSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SlideSwitch (OhpeProcessor& p);
    void bind (const juce::String& paramId);
    const juce::String& boundId() const noexcept { return id; }
    void refresh();
    int position() const;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    juce::Rectangle<float> slotArea() const;
    juce::Rectangle<float> labelArea (int i) const;

private:
    OhpeProcessor& proc;
    juce::String id;
    int shown = -1;
};

/** p14: small knob with a graphite ring and its legend + value at the right (GLIDE). */
class MiniKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    MiniKnob (OhpeProcessor& p, juce::String legendEnglish);
    ~MiniKnob() override;
    void bind (const juce::String& paramId);
    const juce::String& boundId() const noexcept { return id; }
    juce::Slider& slider() noexcept { return knob; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Knob : public juce::Slider
    {
    public:
        explicit Knob (MiniKnob& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        MiniKnob& owner;
    };
    OhpeProcessor& proc;
    juce::String id, legend;
    Knob knob { *this };
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};
} // namespace ohpe::ui
