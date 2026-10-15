// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Pictograms.h"
#include "plugin/PluginProcessor.h"

#include <functional>

namespace ohpe::ui
{
/** Opens the right-click menu of a control: MIDI learn, clear mapping, reset. */
void showControlMenu (OhpeProcessor& proc, const juce::String& paramId, juce::Component* target);

/** Round flat encoder with a coloured ring; bound to one host parameter at a time. */
class Encoder : public juce::Component
{
public:
    Encoder (OhpeProcessor& p, int index);
    ~Encoder() override;

    void bind (const juce::String& paramId); ///< empty = free (encoder dimmed)
    const juce::String& boundId() const noexcept { return paramId; }

    void paint (juce::Graphics& g) override;
    void resized() override;

    float normalisedValue() const;

private:
    class Knob : public juce::Slider
    {
    public:
        explicit Knob (Encoder& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        Encoder& owner;
    };

    OhpeProcessor& proc;
    int index;
    juce::String paramId;
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
