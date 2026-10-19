// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"

#include <functional>

namespace ohpe::ui
{
/**
    The preset library (0.3), opened with the LIBRARY key (MIDI: "lib.open").

    Left: category tags (any of the selected ones), the quick chips MPE / Fingerboard / Morph (p54: all of the selected
    ones) and the engine filter. Top: text search (p54: every word must match name, category, tags, engine, author, pack,
    bank, kind, favourite or rating; English and German),
    bank (all / factory / user), kind (part / project), favourites only, sort order, PACKS (filter by an installed
    content pack, install pack, manage packs). Centre:
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
    /** p18: "Breed" in the details - breed the current sound with library entry `index` (the editor closes the
        library, restoring the sound from before it was opened, and opens the breeding view). */
    std::function<void (int index)> onBreed;
    /** Content packs: "Install pack ..." and "Manage packs ..." from the PACKS menu (the editor opens the file dialog /
        the pack manager page of the settings). */
    std::function<void()> onInstallPack, onManagePacks;

    /** The PACKS chip: text as drawn (the filtered pack's name, or PACKS) and its menu items (tests). */
    juce::String packChipText() const;
    juce::StringArray packMenuItems() const;
    /** Picks a pack filter (empty = all packs) as the PACKS menu does. */
    void setPackFilter (const juce::String& packId);

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
    /** p54: the quick chips (MPE, Fingerboard, Morph): all selected ones must be tags of a preset. */
    void toggleQuickTag (const juce::String& tag);
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
    void showPackMenu();
    static juce::String packName (const update::ContentIndex::Pack& p);
public:
    /** p59: the label of a category / quick tag and of an engine id (the chips' texts; the controller's display). */
    static juce::String tagText (const std::string& tag);
    static juce::String engineText (const std::string& engineId);
    /** p59: the controller's browse mode changed the filter or the cursor: the list follows (no preset is loaded). */
    void followController (int entry);
private:
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
