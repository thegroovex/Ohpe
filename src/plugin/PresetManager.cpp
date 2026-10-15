// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PresetManager.h"

#include "OhpePresets.h"
#include "PluginProcessor.h"

#include <cmath>

namespace ohpe
{
namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
std::string std8 (const juce::String& s) { return s.toStdString(); }
} // namespace

PresetManager::PresetManager (OhpeProcessor& p) : proc (p)
{
    folder = defaultUserFolder();
    rescan();
}

juce::File PresetManager::defaultUserFolder()
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("OHPE_PRESET_DIR", {});
    if (env.isNotEmpty()) return juce::File (env);
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile ("Application Support");
#endif
    return base.getChildFile ("groovex").getChildFile ("Ohpe").getChildFile ("Presets");
}

void PresetManager::setUserFolder (const juce::File& f)
{
    folder = f;
    rescan();
}

int PresetManager::editPart() const
{
    return juce::jlimit (0, Core::numParts - 1, (int) std::lround (proc.core().params().get (Core::Ids::editPart)) - 1);
}

void PresetManager::loadUserData()
{
    userData = {};
    const auto f = folder.getChildFile ("library.json");
    if (f.existsAsFile()) userData.fromJson (std8 (f.loadFileAsString()));
}

void PresetManager::saveUserData()
{
    if (! folder.createDirectory()) return;
    folder.getChildFile ("library.json").replaceWithText (str (userData.toJson()));
}

void PresetManager::rescan()
{
    const auto currentUid = currentIndex >= 0 && currentIndex < (int) list.size() ? list[(size_t) currentIndex].uid : std::string();
    list.clear();
    for (int i = 0; i < OhpePresets::namedResourceListSize; ++i)
    {
        int size = 0;
        const char* data = OhpePresets::getNamedResource (OhpePresets::namedResourceList[i], size);
        if (data == nullptr || size <= 0) continue;
        preset::Preset p;
        if (! preset::fromJson (std::string (data, (size_t) size), p)) continue;
        preset::Entry e;
        e.factory = true;
        e.location = OhpePresets::namedResourceList[i];
        e.uid = "factory:" + std::string (OhpePresets::originalFilenames[i]);
        e.meta = std::move (p.meta);
        list.push_back (std::move (e));
    }
    if (folder.isDirectory())
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*" + juce::String (preset::fileExtension)))
        {
            preset::Preset p;
            if (! preset::fromJson (std8 (f.loadFileAsString()), p)) continue;
            preset::Entry e;
            e.factory = false;
            e.location = std8 (f.getFullPathName());
            e.uid = "user:" + std8 (f.getRelativePathFrom (folder).replaceCharacter ('\\', '/'));
            e.modified = f.getLastModificationTime().toMilliseconds() / 1000;
            e.meta = std::move (p.meta);
            if (e.meta.name.empty()) e.meta.name = std8 (f.getFileNameWithoutExtension());
            list.push_back (std::move (e));
        }
    loadUserData();
    userData.applyTo (list);
    currentIndex = -1;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].uid == currentUid) currentIndex = i;
    sendChangeMessage();
}

std::string PresetManager::read (const preset::Entry& e) const
{
    if (e.factory)
    {
        int size = 0;
        const char* data = OhpePresets::getNamedResource (e.location.c_str(), size);
        return data != nullptr && size > 0 ? std::string (data, (size_t) size) : std::string();
    }
    const juce::File f (str (e.location));
    return f.existsAsFile() ? std8 (f.loadFileAsString()) : std::string();
}

bool PresetManager::apply (const preset::Preset& p, int part)
{
    if (part < 0) part = editPart();
    bool ok = false;
    proc.editRegistry ([&] (Core& core) {
        if (p.meta.kind == preset::Kind::Project) ok = preset::applyProject (p, core).ok;
        else ok = preset::applyPart (p, core.params(), part).ok;
    });
    if (ok && p.meta.kind == preset::Kind::Project && p.sections.count ("samples") > 0)
        proc.sampleLoader().restoreFromState();
    return ok;
}

bool PresetManager::load (int index, int part)
{
    if (index < 0 || index >= (int) list.size()) return false;
    preset::Preset p;
    if (! preset::fromJson (read (list[(size_t) index]), p)) return false;
    if (! apply (p, part)) return false;
    currentIndex = index;
    sendChangeMessage();
    return true;
}

bool PresetManager::step (int delta, int part)
{
    const auto v = view();
    if (v.empty()) return false;
    int pos = -1;
    for (int i = 0; i < (int) v.size(); ++i)
        if (v[(size_t) i] == currentIndex) pos = i;
    const int n = (int) v.size();
    pos = pos < 0 ? (delta >= 0 ? 0 : n - 1) : ((pos + delta) % n + n) % n;
    return load (v[(size_t) pos], part);
}

juce::String PresetManager::currentName() const
{
    if (currentIndex >= 0 && currentIndex < (int) list.size()) return str (list[(size_t) currentIndex].meta.name);
    return "Init";
}

juce::File PresetManager::save (const preset::Meta& metaIn, bool project, int part, bool overwrite)
{
    if (part < 0) part = editPart();
    auto meta = metaIn;
    if (meta.name.empty()) meta.name = "Preset";
    proc.editRegistry ([] (Core&) {}); // host values -> registry
    preset::Preset p;
    if (project)
    {
        proc.sampleLoader().storeInState();
        std::map<std::string, std::string> extra;
        const auto samples = proc.core().stateSection ("samples");
        if (! samples.empty()) extra["samples"] = samples;
        p = preset::captureProject (proc.core(), meta, extra);
    }
    else
        p = preset::capturePart (proc.core().params(), part, meta);

    const auto sub = folder.getChildFile (project ? "Projects" : "Parts");
    if (! sub.createDirectory()) return {};
    const auto base = str (preset::safeFileName (meta.name));
    auto file = sub.getChildFile (base + preset::fileExtension);
    if (file.exists() && ! overwrite)
        file = sub.getNonexistentChildFile (base, preset::fileExtension, false);
    if (! file.replaceWithText (str (preset::toJson (p)))) return {};
    if (! meta.author.empty() && meta.author != userData.author)
    {
        userData.author = meta.author;
        saveUserData();
    }
    rescan();
    for (int i = 0; i < (int) list.size(); ++i)
        if (! list[(size_t) i].factory && juce::File (str (list[(size_t) i].location)) == file) currentIndex = i;
    sendChangeMessage();
    return file;
}

bool PresetManager::remove (int index)
{
    if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory) return false;
    const juce::File f (str (list[(size_t) index].location));
    const auto uid = list[(size_t) index].uid;
    const bool ok = f.moveToTrash() || f.deleteFile();
    if (! ok) return false;
    userData.items.erase (uid);
    saveUserData();
    if (currentIndex == index) currentIndex = -1;
    rescan();
    return true;
}

void PresetManager::setFavourite (int index, bool fav)
{
    if (index < 0 || index >= (int) list.size()) return;
    auto& e = list[(size_t) index];
    e.favourite = fav;
    userData.set (e.uid, e.favourite, e.rating);
    saveUserData();
    sendChangeMessage();
}

void PresetManager::setRating (int index, int stars)
{
    if (index < 0 || index >= (int) list.size()) return;
    auto& e = list[(size_t) index];
    e.rating = juce::jlimit (0, 5, stars);
    userData.set (e.uid, e.favourite, e.rating);
    saveUserData();
    sendChangeMessage();
}

void PresetManager::beginPreview (int part)
{
    if (part < 0) part = editPart();
    proc.editRegistry ([] (Core&) {});
    previewPart = part;
    previewIndex = currentIndex;
    previewBackup = preset::captureProject (proc.core(), {});
}

void PresetManager::revertPreview()
{
    if (! previewBackup) return;
    auto backup = std::move (*previewBackup);
    previewBackup.reset();
    apply (backup, previewPart);
    currentIndex = previewIndex;
    sendChangeMessage();
}
} // namespace ohpe
