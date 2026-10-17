// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PresetManager.h"
#include "Updater.h"

#include "OhpePresets.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace ohpe
{
namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
std::string std8 (const juce::String& s) { return s.toStdString(); }
// arp values still as a preset set them (host round trips may move a float by a hair)
bool sameArp (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::abs (a[i] - b[i]) > 1.0e-4f * std::max (1.0f, std::abs (b[i]))) return false;
    return true;
}
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
    // p27: content packs installed by the updater (Content/<pack>/...): read-only like the factory bank
    {
        const auto content = Updater::contentFolder();
        if (content.isDirectory())
            for (const auto& packDir : content.findChildFiles (juce::File::findDirectories, false))
            {
                if (packDir.getFileName().startsWithChar ('.')) continue; // .backup, .staging
                for (const auto& f : packDir.findChildFiles (juce::File::findFiles, true, "*" + juce::String (preset::fileExtension)))
                {
                    preset::Preset p;
                    if (! preset::fromJson (std8 (f.loadFileAsString()), p)) continue;
                    preset::Entry e;
                    e.factory = true;
                    e.location = std8 (f.getFullPathName());
                    e.uid = "content:" + std8 (f.getRelativePathFrom (content).replaceCharacter ('\\', '/'));
                    e.meta = std::move (p.meta);
                    if (e.meta.name.empty()) e.meta.name = std8 (f.getFileNameWithoutExtension());
                    list.push_back (std::move (e));
                }
            }
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
    if (e.factory && e.uid.rfind ("content:", 0) != 0)
    {
        int size = 0;
        const char* data = OhpePresets::getNamedResource (e.location.c_str(), size);
        return data != nullptr && size > 0 ? std::string (data, (size_t) size) : std::string();
    }
    const juce::File f (str (e.location));
    return f.existsAsFile() ? std8 (f.loadFileAsString()) : std::string();
}

int PresetManager::arpPresetPart() const
{
    if (arpOwner < 0) return -1;
    // the user changed the arp (or a project / host automation did) since the preset set it
    return sameArp (preset::arpSnapshot (proc.core().params()), arpByPreset) ? arpOwner : -1;
}

bool PresetManager::apply (const preset::Preset& p, int part)
{
    if (part < 0) part = editPart();
    bool ok = false;
    proc.editRegistry ([&] (Core& core) {
        if (p.meta.kind == preset::Kind::Project)
        {
            ok = preset::applyProject (p, core).ok;
            arpOwner = -1; // a project brings its own arp setup
            return;
        }
        preset::PartOptions opt;
        opt.releaseArp = arpOwner == part && sameArp (preset::arpSnapshot (core.params()), arpByPreset);
        const auto r = preset::applyPart (p, core.params(), part, opt);
        ok = r.ok;
        if (ok) preset::applyPartMorph (p, core, part); // p16: the morph corners travel with the sound
        if (r.arpApplied && core.params().get (std::string (Core::Ids::arpPrefix) + "on") > 0.5f)
        {
            arpOwner = part;
            arpByPreset = preset::arpSnapshot (core.params());
        }
        else if (r.arpApplied || r.arpReleased)
            arpOwner = -1;
    });
    if (ok && p.meta.kind == preset::Kind::Project && p.sections.count ("samples") > 0)
        proc.sampleLoader().restoreFromState();
    if (ok && p.meta.kind == preset::Kind::Project) // p20: the project's user wavetables (none = the built-in ones)
    {
        if (p.sections.count ("usertables") == 0) proc.core().setStateSection ("usertables", {});
        proc.userTables().restoreFromState();
    }
    else if (ok) // p20: a part preset brings its user material (or clears the part's)
        proc.userTables().applyPartJson (part, p.userTables ? *p.userTables : std::string());
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
        proc.userTables().storeInState(); // p20
        const auto tables = proc.core().stateSection ("usertables");
        if (! tables.empty() && tables.find ("\"parts\":{}") == std::string::npos) extra["usertables"] = tables;
        p = preset::captureProject (proc.core(), meta, extra);
    }
    else
    {
        p = preset::capturePart (proc.core().params(), part, meta);
        preset::captureMorph (proc.core(), part, p); // p16
        if (auto t = proc.userTables().partJson (part); ! t.empty()) p.userTables = t; // p20
    }

    auto file = userFileFor (meta.name, project);
    if (! file.getParentDirectory().createDirectory()) return {};
    if (file.exists() && ! overwrite)
    {
        // keep both: the new preset gets the first free name, in the file and in the list
        // short enough that the number survives the file name limit (80 bytes), no cut UTF-8 sequence
        auto base = meta.name;
        if (base.size() > 72)
        {
            base.resize (72);
            while (! base.empty() && (((unsigned char) base.back()) & 0xc0) == 0x80) base.pop_back();
            if (! base.empty() && ((unsigned char) base.back()) >= 0xc0) base.pop_back();
            while (! base.empty() && base.back() == ' ') base.pop_back();
        }
        for (int n = 2; file.exists(); ++n)
        {
            meta.name = base + " " + std::to_string (n);
            file = userFileFor (meta.name, project);
        }
        p.meta.name = meta.name;
    }
    if (! file.replaceWithText (str (preset::toJson (p)))) return {};
    if (! meta.author.empty() && meta.author != userData.author)
    {
        userData.author = meta.author;
        saveUserData();
    }
    rescan();
    if (const int i = indexOfFile (file); i >= 0) currentIndex = i;
    sendChangeMessage();
    return file;
}

juce::File PresetManager::userFileFor (const std::string& name, bool project) const
{
    return folder.getChildFile (project ? "Projects" : "Parts")
        .getChildFile (str (preset::safeFileName (name.empty() ? std::string ("Preset") : name)) + preset::fileExtension);
}

int PresetManager::indexOfFile (const juce::File& file) const
{
    for (int i = 0; i < (int) list.size(); ++i)
        if (! list[(size_t) i].factory && juce::File (str (list[(size_t) i].location)) == file) return i;
    return -1;
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
    previewArpOwner = arpOwner;
    previewArpByPreset = arpByPreset;
    proc.userTables().storeInState(); // p20: reverting brings the user wavetables back too
    previewBackup = preset::captureProject (proc.core(), {}, { { "usertables", proc.core().stateSection ("usertables") } });
}

void PresetManager::revertPreview()
{
    if (! previewBackup) return;
    auto backup = std::move (*previewBackup);
    previewBackup.reset();
    apply (backup, previewPart);
    currentIndex = previewIndex;
    arpOwner = previewArpOwner; // the arp is back as it was, and so is who set it
    arpByPreset = previewArpByPreset;
    sendChangeMessage();
}
} // namespace ohpe
