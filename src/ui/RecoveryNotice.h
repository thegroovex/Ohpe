// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Tooltips.h"

#include <functional>

namespace ohpe::ui
{
/**
    p63: the crash-safe start's notice. When the standalone app's last session did not end cleanly, or its saved
    state could not be loaded, Ohpe starts with the factory start sound and this small notice (a dark strip in the
    display's colours, at the top of the plate) says so. Unclean end: RESTORE loads the last session after all,
    START FRESH keeps the start sound. Damaged state: only OK (the state cannot be loaded). The plate stays usable
    around it; only the strip takes mouse clicks. English / German.
*/
class RecoveryNotice : public juce::Component, public juce::TooltipClient, public tips::RegionProvider
{
public:
    enum class Reason { None = 0, UncleanExit, StateDamaged, RestoreFailed };

    RecoveryNotice();

    /** Shows the notice. `onRestore` runs on RESTORE (UncleanExit only). */
    void show (Reason why, std::function<void()> onRestore = {});
    void restore();   ///< as if RESTORE was pressed (UncleanExit), otherwise dismiss()
    void dismiss();   ///< START FRESH / OK
    Reason reason() const noexcept { return why; }
    bool isOpen() const noexcept { return isVisible(); }
    int numKeys() const noexcept { return why == Reason::UncleanExit ? 2 : 1; }

    juce::String title() const;    ///< translated
    juce::String message() const;  ///< translated
    juce::String keyText (int key) const; ///< translated; key 0 = right-most (RESTORE / OK), 1 = START FRESH

    juce::Rectangle<float> panelArea() const;
    juce::Rectangle<float> keyArea (int key) const;

    void paint (juce::Graphics& g) override;
    juce::String getTooltip() override { return tips::regionTipAt (tipRegions(), getMouseXYRelative().toFloat()); } ///< p82
    std::vector<tips::Region> tipRegions() override; ///< p82: the keys
    bool hitTest (int x, int y) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    float unit() const noexcept;

    Reason why = Reason::None;
    std::function<void()> onRestoreFn;
    int hover = -1;
};
} // namespace ohpe::ui
