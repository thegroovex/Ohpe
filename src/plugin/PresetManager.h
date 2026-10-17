// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "preset/Preset.h"
#include "preset/PresetLibrary.h"
#include "update/PackFormat.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <optional>

namespace ohpe
{
class OhpeProcessor;

/**
    The preset library behind the LIBRARY key (message thread only).

    * Factory bank: the *.ohpepreset files of resources/presets/factory, embedded as binary data
      (namespace OhpePresets, see cmake/OhpePlugin.cmake). Read-only.
    * User bank: *.ohpepreset files in the user folder (recursive), default
      <user application data>/groovex/Ohpe/Presets; the environment variable OHPE_PRESET_DIR
      overrides it (tests, portable setups). Favourites, ratings and the last author name live
      in library.json in that folder.
    * Browsing: `filter` + `view()`; `step()` moves through the current view (prev / next keys,
      MIDI-mappable "lib.prev" / "lib.next"). Selecting an entry loads it at once (preview);
      `beginPreview()` / `revertPreview()` restore what was there before the browser opened.
*/
class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager (OhpeProcessor& processor);

    static juce::File defaultUserFolder();
    void setUserFolder (const juce::File& folder);
    const juce::File& userFolder() const noexcept { return folder; }

    /** Re-reads the factory bank and the user folder (keeps favourites / ratings). */
    void rescan();
    const std::vector<preset::Entry>& entries() const noexcept { return list; }
    /** The installed content packs found by the last rescan (id, version, names EN / DE), sorted by id. */
    const std::vector<update::ContentIndex::Pack>& contentPacks() const noexcept { return packs; }

    preset::Filter filter;
    /** Entry indices that pass `filter`, sorted. */
    std::vector<int> view() const { return preset::filterAndSort (list, filter); }

    /** Text of an entry (factory resource or user file); empty on failure. */
    std::string read (const preset::Entry& e) const;
    /** Loads entry `index` into part `part` (0-based; -1 = the edit part "ui.part"). */
    bool load (int index, int part = -1);
    /** Loads a parsed preset (part presets into `part`, -1 = edit part). */
    bool apply (const preset::Preset& p, int part = -1);
    /** Loads the next / previous entry of the current view (wraps). */
    bool step (int delta, int part = -1);

    /** Index of the entry loaded last (-1 none) and its name ("Init" if none). */
    int current() const noexcept { return currentIndex; }
    juce::String currentName() const;

    /** Saves the edit part (or the whole project) as a user preset. Returns the file (or none on failure).
        A user preset of the same name and kind is replaced when `overwrite` is set; otherwise the new
        one gets a free name ("Name 2", "Name 3", ...), so the list never shows two presets of one name. */
    juce::File save (const preset::Meta& meta, bool project, int part = -1, bool overwrite = false);
    /** The file a user preset named `name` is saved to (exists if such a preset is there already). */
    juce::File userFileFor (const std::string& name, bool project) const;
    /** Index of the user entry stored in `file`, or -1. */
    int indexOfFile (const juce::File& file) const;
    bool remove (int index); ///< user presets only (moves the file to the trash if possible)

    void setFavourite (int index, bool fav);
    void setRating (int index, int stars);
    juce::String author() const { return juce::String::fromUTF8 (userData.author.c_str()); }

    /** Preview handling for the browser. */
    void beginPreview (int part = -1);
    void revertPreview();
    void endPreview() { previewBackup.reset(); }
    bool previewing() const noexcept { return previewBackup.has_value(); }

    int editPart() const; ///< 0-based part of "ui.part"

    /** Part (0-based) whose arpeggiator a part preset's arp block switched on and that nobody has
        changed since, or -1. A part preset without arp block loaded into that part switches the
        arp off again. Not saved: after reopening a project the arp counts as set by hand. */
    int arpPresetPart() const;

private:
    void saveUserData();
    void loadUserData();

    OhpeProcessor& proc;
    juce::File folder;
    std::vector<preset::Entry> list;
    std::vector<update::ContentIndex::Pack> packs;
    preset::UserData userData;
    int currentIndex = -1;
    std::optional<preset::Preset> previewBackup;
    int previewPart = 0, previewIndex = -1;
    int arpOwner = -1;              ///< part whose arp a preset switched on (-1 none)
    std::vector<float> arpByPreset; ///< arp values right after that preset loaded
    int previewArpOwner = -1;
    std::vector<float> previewArpByPreset;
};
} // namespace ohpe
