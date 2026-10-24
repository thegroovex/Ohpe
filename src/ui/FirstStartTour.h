// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Tooltips.h"

#include <functional>
#include <vector>

namespace ohpe::ui
{
/**
    p64: the first-start tour - four short steps over the plate, English / German:
      1  parts, the preset strip and LIBRARY
      2  the four encoders, the display's soft keys and the FX editor
      3  tape and sequencer (mode column), the transport with the tempo, the key on the display
      4  the settings: help, help overlay, the Grid-Controller (and this tour again)
    The plate is dimmed except the areas of the step, which get a yellow outline (yellow = what is shown); a card in
    the display's colours carries the text and SKIP / BACK / NEXT (DONE on the last step). Esc skips, the arrow keys
    and Return step. It follows the skin and the language while open (everything is painted from the palette and
    translated at paint time).

    The editor shows it once on the first standalone start and on the first plug-in editor open (settings.json
    "tour"); Settings > General and the help pages open it again. `onFinished` runs on DONE and on SKIP.
*/
class FirstStartTour : public juce::Component, public juce::TooltipClient, public tips::RegionProvider
{
public:
    static constexpr int numSteps = 4;

    FirstStartTour();

    void show (int step = 0);
    void next();     ///< NEXT; DONE on the last step
    void back();
    void finish();   ///< DONE / SKIP: closes and runs onFinished
    bool isOpen() const noexcept { return isVisible(); }
    int step() const noexcept { return current; }

    std::function<void()> onFinished;

    static juce::String title (int step);  ///< translated
    static juce::String text (int step);   ///< translated
    /** The plate areas a step points at, in design units (the plate is 1200 x 800). */
    static std::vector<juce::Rectangle<float>> highlights (int step);
    /** The card of a step, in design units (never over the step's highlights). */
    static juce::Rectangle<float> cardDesign (int step);

    enum Key { NextKey = 0, BackKey, SkipKey, numKeys };
    juce::String keyText (int key) const; ///< translated, as drawn (upper case)
    bool hasKey (int key) const noexcept { return key != BackKey || current > 0; }
    juce::Rectangle<float> cardArea() const;
    juce::Rectangle<float> keyArea (int key) const;
    /** Clicks a key as the mouse does (tests). */
    void press (int key);

    void paint (juce::Graphics& g) override;
    juce::String getTooltip() override { return tips::regionTipAt (tipRegions(), getMouseXYRelative().toFloat()); } ///< p82
    std::vector<tips::Region> tipRegions() override; ///< p82: the keys
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;

private:
    float unit() const noexcept;
    juce::Rectangle<float> scaled (juce::Rectangle<float> r) const noexcept;

    int current = 0;
    int hover = -1;
};
} // namespace ohpe::ui
