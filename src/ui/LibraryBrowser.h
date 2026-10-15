// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"

#include <functional>

namespace ohpe::ui
{
/**
    The preset library (0.3), opened with the LIBRARY key (MIDI: "lib.open").

    Left: category tags (any of the selected ones) and the engine filter. Top: text search,
    bank (all / factory / user), kind (part / project), favourites only, sort order. Centre:
    the presets; selecting one loads it at once (preview, into the edit part). Right: details,
    favourite, star rating, save (name, author, tags; part or whole project), delete (user
    presets). "Keep" closes with the preset loaded, "Cancel" (Esc) restores the sound from
    before the library was opened.
*/
class LibraryBrowser : public juce::Component,
                       private juce::ListBoxModel,
                       private juce::ChangeListener
{
public:
    explicit LibraryBrowser (OhpeProcessor& p);
    ~LibraryBrowser() override;

    /** Opens the browser (starts the preview) / closes it (keep = keep the loaded preset). */
    void open();
    void close (bool keep);
    bool isOpen() const noexcept { return isVisible(); }

    std::function<void()> onClosed;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;

    // used by tests and the screenshot tool
    void selectRow (int row);
    int numRows() const noexcept { return (int) rows.size(); }
    void setSearchText (const juce::String& text);
    void toggleTag (const juce::String& tag);
    void showSavePanel (bool show);
    void setSaveFields (const juce::String& name, const juce::String& author, const juce::StringArray& tags, bool project);
    bool commitSave();

private:
    // ListBoxModel
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& e) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void rebuild();
    float unit() const;
    struct Hit
    {
        juce::Rectangle<float> area;
        std::function<void()> action;
    };
    void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent, std::function<void()> action);
    juce::Rectangle<float> leftArea() const;
    juce::Rectangle<float> rightArea() const;
    juce::Rectangle<float> topArea() const;
    int selectedEntry() const;

    OhpeProcessor& proc;
    PresetManager& presets;
    std::vector<int> rows; ///< entry indices shown
    juce::ListBox list { "presets", this };
    juce::TextEditor search, saveName, saveAuthor, saveFreeTags;
    juce::StringArray saveTags;
    bool saveProject = false, saving = false, ignoreSelection = false;
    std::vector<Hit> hits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryBrowser)
};
} // namespace ohpe::ui
