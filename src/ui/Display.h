// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Pages.h"
#include "plugin/PluginProcessor.h"

#include <functional>

namespace ohpe::ui
{
/**
    The central display: mode header, the animated pictogram character of the current engine /
    mode, the four encoder values in their colours and a context strip (voices, steps, mixer).
*/
class Display : public juce::Component
{
public:
    explicit Display (OhpeProcessor& p);

    void setMode (Mode m, int page, int numPagesInMode, const std::vector<juce::String>& encoderIds);
    void setMixerChannel (int ch) { mixerChannel = ch; repaint(); }
    void setStepPage (int p) { stepPage = p; repaint(); }
    void tick (float seconds); ///< advances the animation (called by the editor's timer)

    std::function<void (int delta)> onPageChange;
    std::function<void (int step)> onStepClicked;
    std::function<void (int page)> onStepPageClicked;
    std::function<void (int channel)> onChannelClicked;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> headerArea() const;
    juce::Rectangle<float> stripArea() const;
    juce::Rectangle<float> stepCell (int i) const;
    juce::Rectangle<float> stepPageTab (int i) const;
    juce::Rectangle<float> channelCell (int i) const;
    juce::Rectangle<float> pageArrow (bool right) const;

    void paintRows (juce::Graphics& g, juce::Rectangle<float> area);
    void paintStrip (juce::Graphics& g, juce::Rectangle<float> area);

    OhpeProcessor& proc;
    Mode mode = Mode::Synth;
    int page = 0, pages = 1, mixerChannel = 0, stepPage = 0;
    std::vector<juce::String> ids;
    float time = 0.0f, energy = 0.0f;
};
} // namespace ohpe::ui
