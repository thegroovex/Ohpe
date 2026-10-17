// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Content packs in the editor: "Install pack ..." (Settings › Updates, the pack manager page, the library's PACKS
// menu, dropping an .ohpepack on the window), the pack manager list (version, size, license / signature, remove with
// a second click), the library filter by pack - in both languages.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Updater.h"
#include "preset/Preset.h"
#include "ui/PluginEditor.h"
#include "update/Sha256.h"

using namespace ohpe;

namespace
{
void setEnv (const char* name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s (name, value.toRawUTF8());
#else
    if (value.isEmpty()) unsetenv (name);
    else setenv (name, value.toRawUTF8(), 1);
#endif
}

juce::String presetText (const std::string& name, const std::string& tag)
{
    Core core;
    preset::Meta m;
    m.name = name;
    m.tags = { tag };
    return juce::String::fromUTF8 (preset::toJson (preset::capturePart (core.params(), 0, m)).c_str());
}

/** An .ohpepack with checksums. */
juce::File makePack (const juce::File& f, const juce::String& id, const juce::String& nameEn, const juce::String& nameDe, bool paid,
                     const std::vector<std::pair<juce::String, juce::String>>& files)
{
    juce::String sums;
    for (const auto& [path, text] : files)
        sums << (sums.isEmpty() ? "" : ",") << "\"" << path << "\":\"" << juce::String (update::Sha256::of (text.toStdString())) << "\"";
    juce::String meta;
    meta << "{\"format\":1,\"id\":\"" << id << "\",\"version\":\"1.0.0\",\"kind\":\"presets\",\"minApp\":\"0.0.1\","
         << "\"name\":{\"en\":\"" << nameEn << "\",\"de\":\"" << nameDe << "\"},"
         << "\"description\":{\"en\":\"Dark and wide\",\"de\":\"Dunkel und weit\"},"
         << (paid ? "\"license\":{\"type\":\"paid\",\"productId\":\"ohpe-" + id + "\"},\"signature\":{\"alg\":\"ed25519\",\"keyId\":\"k1\",\"value\":\"c2ln\"}," : "")
         << "\"files\":{" << sums << "}}";
    juce::ZipFile::Builder b;
    b.addEntry (new juce::MemoryInputStream (meta.toRawUTF8(), meta.getNumBytesAsUTF8(), true), 6, "pack.json", juce::Time::getCurrentTime());
    for (const auto& [path, text] : files)
        b.addEntry (new juce::MemoryInputStream (text.toRawUTF8(), text.getNumBytesAsUTF8(), true), 6, path, juce::Time::getCurrentTime());
    f.deleteFile();
    juce::FileOutputStream out (f);
    b.writeToStream (out, nullptr);
    return f;
}

bool contains (const juce::StringArray& a, const juce::String& s) { return a.contains (s); }
} // namespace

TEST_CASE ("pack ui: install from Settings / drop, pack manager list, library filter, remove - EN and DE")
{
    juce::TemporaryFile tmp;
    const auto root = tmp.getFile();
    root.createDirectory();
    const auto prevData = juce::SystemStats::getEnvironmentVariable ("OHPE_DATA_DIR", {});
    const auto prevPresets = juce::SystemStats::getEnvironmentVariable ("OHPE_PRESET_DIR", {});
    setEnv ("OHPE_DATA_DIR", root.getChildFile ("data").getFullPathName());
    setEnv ("OHPE_PRESET_DIR", root.getChildFile ("data/Presets").getFullPathName());
    {
        OhpeProcessor p;
        p.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
        auto& u = e.updaterInstance();
        u.setDataFolderForTests (root.getChildFile ("data"));
        e.setLanguage (i18n::Language::English);

        // Settings › Updates leads to the pack manager page
        e.showSettings (true, ui::SettingsView::Updates);
        auto& sv = e.settingsComponent();
        const auto chips = sv.chipTexts();
        CHECK (contains (chips, juce::String::fromUTF8 ("INSTALL PACK \xe2\x80\xa6")));
        CHECK (sv.clickChip ("MANAGE PACKS"));
        CHECK (sv.page() == e.packPage());
        CHECK (sv.tabName (e.packPage()) == "Content packs");
        auto& pm = e.packComponent();
        CHECK (pm.isVisible());
        CHECK (pm.rowTexts().isEmpty());
        CHECK (contains (pm.chipTexts(), juce::String::fromUTF8 ("INSTALL PACK \xe2\x80\xa6")));

        // dropping an .ohpepack on the window installs it
        const auto atmo = makePack (root.getChildFile ("atmo-1.0.0.ohpepack"), "atmo", "Atmo", "Atmo", true,
                                    { { "presets/night-tide.ohpepreset", presetText ("Night Tide", "pads") },
                                      { "presets/crypt-organ.ohpepreset", presetText ("Crypt Organ", "keys") } });
        CHECK (e.isInterestedInFileDrag ({ atmo.getFullPathName() }));
        e.filesDropped ({ atmo.getFullPathName() }, 10, 10);
        REQUIRE (u.waitIdle (20000));
        e.advanceAnimation (0.1f); // the editor sees the new content generation: library rescan, display flash
        CHECK (u.snapshot().message == "Content pack installed");
        pm.chipTexts();
        REQUIRE (pm.rowTexts().size() == 1);
        CHECK (pm.rowTexts()[0].startsWith ("Atmo  1.0.0"));
        CHECK (pm.rowTexts()[0].contains ("2 presets"));
        CHECK (pm.rowTexts()[0].contains ("Paid  /  Signed"));
        // the pack manager's install (no dialog in tests) - a second, free pack
        pm.install (makePack (root.getChildFile ("keys.ohpepack"), "keys-plus", "Keys Plus", "Tasten Plus", false,
                              { { "presets/glass-keys.ohpepreset", presetText ("Glass Keys", "keys") } }));
        REQUIRE (u.waitIdle (20000));
        e.advanceAnimation (0.1f);
        pm.chipTexts();
        REQUIRE (pm.rowTexts().size() == 2);
        CHECK (pm.rowTexts()[1].contains ("Free  /  Unsigned"));
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());

        // library: PACKS menu, filter by pack, pack name in the details
        e.showLibrary (true);
        auto& lib = e.libraryComponent();
        CHECK (lib.packChipText() == juce::String::fromUTF8 ("PACKS \xe2\x96\xbe"));
        const auto items = lib.packMenuItems();
        CHECK (items.contains ("Atmo  1.0.0"));
        CHECK (items.contains ("Keys Plus  1.0.0"));
        CHECK (items.contains (juce::String::fromUTF8 ("Install pack \xe2\x80\xa6")));
        lib.setPackFilter ("atmo");
        CHECK (lib.numRows() == 2);
        CHECK (lib.packChipText() == juce::String::fromUTF8 ("ATMO \xe2\x96\xbe"));
        lib.setPackFilter ("keys-plus");
        CHECK (lib.numRows() == 1);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        lib.setPackFilter ({});
        CHECK (lib.numRows() > 3);
        e.showLibrary (false);

        // German
        e.setLanguage (i18n::Language::German);
        e.showSettings (true, e.packPage());
        CHECK (sv.tabName (e.packPage()) == "Inhaltspakete");
        CHECK (contains (pm.chipTexts(), juce::String::fromUTF8 ("PAKET INSTALLIEREN \xe2\x80\xa6")));
        CHECK (pm.rowTexts()[1].startsWith ("Tasten Plus  1.0.0"));
        CHECK (pm.rowTexts()[1].contains ("Kostenlos  /  Nicht signiert"));
        e.showLibrary (true);
        CHECK (lib.packChipText() == juce::String::fromUTF8 ("PAKETE \xe2\x96\xbe"));
        e.showLibrary (false);

        // remove asks once: REMOVE -> REMOVE? -> gone (and the library forgets it)
        e.setLanguage (i18n::Language::English);
        e.showSettings (true, e.packPage());
        lib.setPackFilter ("keys-plus");
        CHECK (pm.clickChip ("REMOVE"));
        CHECK (pm.rowTexts().size() == 2); // armed only
        CHECK (pm.clickChip ("REMOVE?"));
        pm.chipTexts();
        CHECK (pm.rowTexts().size() == 1);
        e.advanceAnimation (0.1f);
        CHECK_FALSE (root.getChildFile ("data/Content/keys-plus").exists());
        CHECK (root.getChildFile ("data/Content/atmo").isDirectory());
        e.showSettings (false);
    }
    i18n::setCurrent (i18n::Language::English);
    setEnv ("OHPE_DATA_DIR", prevData);
    setEnv ("OHPE_PRESET_DIR", prevPresets);
    root.deleteRecursively();
}
