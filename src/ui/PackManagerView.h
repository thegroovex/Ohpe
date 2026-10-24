// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/Updater.h"
#include "Tooltips.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ohpe::ui
{
/**
    Content packs: the pack manager (Settings › Content packs; also reached from Settings › Updates and the library).

    INSTALL PACK ... (file dialog for .ohpepack files; dropping one on the window does the same), the space the packs
    use of the Content quota, and every installed pack: name (UI language), version, size, what it holds, license
    (free / paid) and signature (signed / unsigned) as pack.json says - shown, not enforced - and REMOVE (asks once:
    the chip turns into REMOVE? for a second click). Packs are read-only; user presets and tables are never touched.
    Draws in the display palette (follows the night skin).
*/
class PackManagerView : public juce::Component, public juce::TooltipClient, public tips::RegionProvider, private juce::ChangeListener
{
public:
    explicit PackManagerView (Updater& u);
    ~PackManagerView() override;

    /** Opens the file dialog (*.ohpepack, old *.zip packs) and installs the chosen pack. */
    void chooseAndInstall();
    void install (const juce::File& pack) { updater.installPackFile (pack); }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    juce::String getTooltip() override;              ///< p82: the painted control under the mouse
    std::vector<tips::Region> tipRegions() override; ///< p82: every painted control
    void visibilityChanged() override { armedRemove.clear(); }

    /** Size text "1.2 MB" / "640 KB". */
    static juce::String sizeText (uint64_t bytes);
    /** Clicks the chip drawn with `text` (upper case as drawn); false if there is none (tests). */
    bool clickChip (const juce::String& text);
    juce::StringArray chipTexts();
    /** The pack rows as drawn ("Atmo  1.1.0  ...", tests). */
    juce::StringArray rowTexts() const { return rows; }

private:
    struct Hit
    {
        juce::Rectangle<float> area;
        juce::String text;
        std::function<void()> action;
        juce::String tip; ///< p82
    };
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    float unit() const { return (float) getWidth() / 88.0f; }
    void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent, std::function<void()> action,
               bool enabled = true);

    Updater& updater;
    juce::String tipNow; ///< p82: the tooltip of the next chips
    void rowTip (const char* en, const char* de) { tipNow = tips::t (en, de); }
    std::vector<Hit> hits;
    juce::StringArray rows;
    juce::String armedRemove;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PackManagerView)
};
} // namespace ohpe::ui
