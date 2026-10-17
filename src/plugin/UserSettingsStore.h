// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "settings/UserSettings.h"

#include <juce_events/juce_events.h>

namespace ohpe
{
/**
    p32: the user's settings file, shared by every open Ohpe (standalone and plug-in instances of one process) through
    juce::SharedResourcePointer<UserSettingsStore>.

    File: <user data>/groovex/Ohpe/Settings/settings.json (OHPE_DATA_DIR overrides <user data>/groovex/Ohpe, like the
    updater, whose own settings stay in update.json next to it). Read once when the first editor opens, written on every
    change; other open editors hear about it through the change broadcast. Message thread.
*/
class UserSettingsStore : public juce::ChangeBroadcaster
{
public:
    UserSettingsStore();

    const settings::UserSettings& get() const noexcept { return values; }
    /** Stores and saves `s` and tells every open editor (asynchronously). */
    void set (const settings::UserSettings& s);
    juce::File file() const;

    /** The operating system shows dark windows (Skin::Auto follows it). */
    static bool systemDark();

    // tests / screenshot tool: another folder (the settings are read from it at once); nothing else is touched
    void setFolderForTests (const juce::File& f);

private:
    void load();
    settings::UserSettings values;
    juce::File testFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UserSettingsStore)
};
} // namespace ohpe
