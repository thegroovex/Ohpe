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

/** Chunky graphite key with a pictogram and a legend printed below. */
class PictoKey : public juce::Button
{
public:
    PictoKey (Picto picto, juce::String legendEnglish);

    void setActive (bool a) { active = a; repaint(); }
    void setAvailable (bool a) { available = a; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setLegend (juce::String englishText) { legend = std::move (englishText); repaint(); }

    std::function<void()> onSecondaryClick;

    void paintButton (juce::Graphics& g, bool over, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    Picto picto;
    juce::String legend;
    bool active = false, available = true;
    juce::Colour accent;
};

/** Two-octave keyboard with chunky flat keys. */
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    explicit Keyboard (juce::MidiKeyboardState& s);
    void setBaseOctave (int octave); ///< lowest C = 12 * (octave + 1)
    int getBaseOctave() const noexcept { return baseOctave; }
    void resized() override;

    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour) override;
    void drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour) override;

private:
    int baseOctave = 3;
};
} // namespace ohpe::ui
