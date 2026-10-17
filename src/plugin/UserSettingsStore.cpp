// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UserSettingsStore.h"

#include "Updater.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace ohpe
{
UserSettingsStore::UserSettingsStore() { load(); }

juce::File UserSettingsStore::file() const
{
    const auto base = testFolder != juce::File() ? testFolder : Updater::dataFolder();
    return base.getChildFile ("Settings").getChildFile ("settings.json");
}

void UserSettingsStore::load()
{
    const auto f = file();
    values = f.existsAsFile() ? settings::UserSettings::fromJson (f.loadFileAsString().toStdString()) : settings::UserSettings {};
}

void UserSettingsStore::set (const settings::UserSettings& s)
{
    values = s;
    values.brightness = juce::jlimit (settings::minBrightness, 1.0f, values.brightness);
    values.windowScale = settings::snapWindowScale (values.windowScale);
    const auto f = file();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String::fromUTF8 (values.toJson().c_str()));
    sendChangeMessage();
}

bool UserSettingsStore::systemDark()
{
    if (! juce::MessageManager::getInstanceWithoutCreating()) return false;
    return juce::Desktop::getInstance().isDarkModeActive();
}

void UserSettingsStore::setFolderForTests (const juce::File& f)
{
    testFolder = f;
    load();
}
} // namespace ohpe
