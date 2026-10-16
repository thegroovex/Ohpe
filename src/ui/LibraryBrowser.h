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
    /** Re-reads the translated placeholder texts (after the UI language changed). */
    void languageChanged();
    void toggleTag (const juce::String& tag);
    void showSavePanel (bool show);
    void setSaveFields (const juce::String& name, const juce::String& author, const juce::StringArray& tags, bool project);
    /** What to do when a user preset of that name exists already. */
    enum class SaveConflict { Ask, Replace, KeepBoth };
    /** Saves the edit part / project as a user preset and shows it in the list (filters that would
        hide it are cleared). Returns false if nothing was saved (failure, or a name clash that is
        asked about first: the question saves when answered). */
    bool commitSave (SaveConflict conflict = SaveConflict::Ask);
    juce::StringArray saveTagSelection() const { return saveTags; }
    juce::String saveFreeTagText() const { return saveFreeTags.getText(); }
    /** Entry shown in list row `row` (-1 if none) and the row of entry `index` (-1 if hidden). */
    int entryAtRow (int row) const noexcept { return row >= 0 && row < (int) rows.size() ? rows[(size_t) row] : -1; }
    int rowOfEntry (int index) const noexcept;
    int selectedListRow() const { return list.getSelectedRow(); }
    /** The last save message ("Saved: ..." or why it failed), shown under the list. */
    const juce::String& statusMessage() const noexcept { return status; }

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
    void reveal (int index); ///< makes entry `index` visible (clears filters that hide it) and selects it

    OhpeProcessor& proc;
    PresetManager& presets;
    std::vector<int> rows; ///< entry indices shown
    juce::ListBox list { "presets", this };
    juce::TextEditor search, saveName, saveAuthor, saveFreeTags;
    juce::StringArray saveTags;
    bool saveProject = false, saving = false, ignoreSelection = false;
    std::vector<Hit> hits;
    juce::String status;
    bool statusError = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryBrowser)
};
} // namespace ohpe::ui
