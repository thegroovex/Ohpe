// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Pages.h"
#include "Widgets.h"

#include <memory>
#include <vector>

namespace ohpe::ui
{
/**
    One parameter as a horizontal bar (pro view): name, value and a bar you drag (right / up =
    more; Shift, Cmd or Ctrl = fine), click on the bar to jump, double-click resets, right-click
    opens MIDI learn.
*/
class ParamBar : public juce::Component
{
public:
    ParamBar (OhpeProcessor& p, juce::String paramId, juce::Colour accent);

    const juce::String& paramId() const noexcept { return id; }
    void refresh();  ///< repaints if the value changed

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    juce::Rectangle<float> barBounds() const;

private:
    OhpeProcessor& proc;
    juce::String id;
    juce::Colour accent;
    juce::RangedAudioParameter* param = nullptr;
    int index = -1;
    float shown = -1.0f;
    bool dragging = false;
    BarDrag drag;
};

/**
    The pro view (0.3): every parameter of the current mode at once, grouped (for the tape all
    eight tracks, for the drum kit all 24 pads), laid out in as many columns as the editor's
    width allows; scrolls when it does not fit. Mode tabs on top switch the mode.
*/
class ProView : public juce::Component
{
public:
    explicit ProView (OhpeProcessor& p);
    ~ProView() override;

    void setContent (Mode mode, const std::vector<ParamGroup>& groups, const juce::String& subtitle);
    void refresh();

    std::function<void (Mode)> onModeSelected;
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    int numBars() const noexcept { return (int) bars.size(); }
    ParamBar& bar (int i) { return *bars[(size_t) i]; }
    Mode currentMode() const noexcept { return mode; }
    int numColumns() const noexcept { return columns; }

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (ProView& o) : owner (o) {}
        void paint (juce::Graphics& g) override;
        ProView& owner;
    };

    juce::Rectangle<float> tabArea (int m) const;
    juce::Rectangle<float> closeArea() const;
    float unit() const;
    void layoutContent();

    OhpeProcessor& proc;
    Mode mode = Mode::Synth;
    juce::String subtitle;
    std::vector<ParamGroup> groups;
    std::vector<std::unique_ptr<ParamBar>> bars;
    std::vector<juce::Rectangle<int>> groupBoxes; ///< in content coordinates
    juce::Viewport viewport;
    Content content { *this };
    int columns = 1;
};
} // namespace ohpe::ui
