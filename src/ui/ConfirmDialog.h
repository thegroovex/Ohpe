// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace ohpe::ui
{
/**
    p38: a confirmation over the whole plate before something destructive (clear the whole tape, load a tape over
    recorded material). A dimmed backdrop, one panel in the housing colours (light and night skin follow
    colour::*), a title, one line of text and two keys: CANCEL (the default - Return, Esc, a click on the backdrop)
    and the destructive key in colour::alertInk. Left / right / Tab move the focus between the keys.
*/
class ConfirmDialog : public juce::Component
{
public:
    struct Options
    {
        juce::String title, message;  ///< translated
        juce::String cancelText, confirmText; ///< translated key labels
        std::function<void()> onConfirm, onCancel;
    };

    ConfirmDialog();

    void show (Options o);   ///< makes it visible, in front, focus on CANCEL
    void confirm();          ///< as if the destructive key was pressed
    void cancel();           ///< as if CANCEL was pressed
    bool isOpen() const noexcept { return isVisible(); }
    const Options& options() const noexcept { return opts; }
    int focusedKey() const noexcept { return focus; } ///< 0 = cancel, 1 = confirm

    /** Areas (pixels, local): the panel and its two keys. */
    juce::Rectangle<float> panelArea() const;
    juce::Rectangle<float> keyArea (int key) const;

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;

private:
    void close (bool confirmed);
    float unit() const noexcept;

    Options opts;
    int focus = 0, hover = -1;
};
} // namespace ohpe::ui
