// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Content packs - the shared loader on disk: "Install pack ..." from a local zip (old flat packs and pack.json packs),
// backup / rollback, refused packs leave nothing behind, only release URLs are downloaded, the library reads the
// kind folders (uid unchanged for old packs) and filters by pack, user wavetables resolve "<pack>/<table>" and say
// "content pack missing" when the pack is not there.

#include <doctest/doctest.h>

#include "plugin/ContentLibrary.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Updater.h"
#include "preset/Preset.h"

#include <cmath>

using namespace ohpe;

namespace
{
std::string std8 (const juce::String& s) { return s.toStdString(); }

void setEnv (const char* name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s (name, value.toRawUTF8());
#else
    if (value.isEmpty()) unsetenv (name);
    else setenv (name, value.toRawUTF8(), 1);
#endif
}

juce::File makeZip (const juce::File& f, const std::vector<std::pair<juce::String, juce::String>>& entries)
{
    juce::ZipFile::Builder b;
    for (const auto& [path, text] : entries)
        b.addEntry (new juce::MemoryInputStream (text.toRawUTF8(), text.getNumBytesAsUTF8(), true), 6, path, juce::Time::getCurrentTime());
    f.deleteFile();
    juce::FileOutputStream out (f);
    b.writeToStream (out, nullptr);
    return f;
}

juce::String presetText (const std::string& name)
{
    Core core;
    return juce::String::fromUTF8 (preset::toJson (preset::capturePart (core.params(), 0, { name })).c_str());
}

/** A single-cycle set: `frames` frames of 2048 samples (a sine with more harmonics per frame). */
void writeWaveSet (const juce::File& f, int frames)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::AudioBuffer<float> buf (1, frames * 2048);
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const int fr = i / 2048;
        const double ph = 2.0 * 3.14159265358979 * (double) (i % 2048) / 2048.0;
        float v = 0.0f;
        for (int h = 1; h <= fr + 1; ++h) v += (float) (0.5 / h * std::sin (ph * h));
        buf.setSample (0, i, v);
    }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (f);
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
    REQUIRE (w != nullptr);
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

struct Dir
{
    juce::TemporaryFile tmp;
    juce::File root;
    Dir() : root (tmp.getFile()) { root.createDirectory(); }
    ~Dir() { root.deleteRecursively(); }
};

const char* packJsonV11 = R"({"format":1,"id":"atmo","version":"1.1.0","kind":"presets","minApp":"0.0.1",
    "name":{"en":"Atmo","de":"Atmo"},"description":{"en":"Dark and wide","de":"Dunkel und weit"}})";
} // namespace

TEST_CASE ("content loader: install pack from a local zip - old flat pack, pack.json pack, backup and rollback")
{
    Dir d;
    const auto data = d.root.getChildFile ("data"), content = data.getChildFile ("Content");
    const auto userPreset = data.getChildFile ("Presets").getChildFile ("Mine.ohpepreset");
    userPreset.getParentDirectory().createDirectory();
    userPreset.replaceWithText ("{\"mine\":true}");

    Updater u;
    u.setDataFolderForTests (data);
    const int gen0 = u.contentGeneration();

    // the pack repository's first zips: presets at the root, no pack.json - named by the file name
    const auto flat = makeZip (d.root.getChildFile ("Ohpe-content-atmo-1.0.0.zip"),
                               { { "pads-night-tide.ohpepreset", presetText ("Night Tide") }, { "keys-crypt-organ.ohpepreset", presetText ("Crypt Organ") } });
    u.installPackFile (flat);
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().message == "Content pack installed");
    CHECK (content.getChildFile ("atmo/presets/pads-night-tide.ohpepreset").existsAsFile());
    CHECK_FALSE (content.getChildFile ("atmo/pads-night-tide.ohpepreset").exists());
    CHECK (content.getChildFile ("atmo/pack.json").existsAsFile()); // written for old packs
    CHECK (u.contentGeneration() == gen0 + 1);
    auto installed = u.installedPacks();
    REQUIRE (installed.size() == 1);
    CHECK (installed[0].id == "atmo");
    CHECK (installed[0].version == "1.0.0");

    // a new version with pack.json in the kind folder: the old one becomes the backup
    const auto v11 = makeZip (d.root.getChildFile ("atmo-new.zip"), { { "pack.json", packJsonV11 },
                                                                       { "presets/pads-night-tide.ohpepreset", presetText ("Night Tide 2") },
                                                                       { "readme.txt", "v1.1" },
                                                                       { "cover.png", "not content" } });
    u.installPackFile (v11);
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().message == "Content pack installed");
    CHECK (content.getChildFile ("atmo/readme.txt").loadFileAsString() == "v1.1");
    CHECK_FALSE (content.getChildFile ("atmo/cover.png").exists()); // not content of this kind: left out
    CHECK (content.getChildFile (".backup/atmo/presets/keys-crypt-organ.ohpepreset").existsAsFile());
    installed = u.installedPacks();
    REQUIRE (installed.size() == 1);
    CHECK (installed[0].version == "1.1.0");
    CHECK (installed[0].descriptionDe == "Dunkel und weit");
    CHECK (u.rollbackPack ("atmo"));
    CHECK (content.getChildFile ("atmo/presets/keys-crypt-organ.ohpepreset").existsAsFile());
    CHECK (u.installedPacks()[0].version == "1.0.0");

    // refused packs leave nothing behind
    struct Bad { const char* file; std::vector<std::pair<juce::String, juce::String>> entries; const char* error; };
    const std::vector<Bad> bad {
        { "evil.zip", { { "pack.json", R"({"id":"evil","version":"1.0.0"})" }, { "../escape.ohpepreset", "{}" } }, "The content pack is damaged" },
        { "noname.zip", { { "a.ohpepreset", "{}" } }, "The content pack has no description (pack.json)" },
        { "empty-pack.zip", { { "pack.json", R"({"id":"empty","version":"1.0.0"})" }, { "readme.txt", "hi" } }, "The content pack holds no content" },
        { "future.zip", { { "pack.json", R"({"id":"future","version":"1.0.0","kind":"holograms"})" }, { "x.bin", "" } },
          "The pack description (pack.json) is not valid" },
    };
    for (const auto& b : bad)
    {
        u.installPackFile (makeZip (d.root.getChildFile (b.file), b.entries));
        REQUIRE (u.waitIdle (20000));
        CHECK_MESSAGE (u.snapshot().state == Updater::State::Failed, b.file);
        CHECK_MESSAGE (u.snapshot().message == b.error, b.file);
    }
    for (const char* id : { "evil", "empty", "future" }) CHECK_FALSE (content.getChildFile (id).exists());
    CHECK_FALSE (content.getChildFile ("escape.ohpepreset").exists());
    CHECK_FALSE (data.getChildFile ("escape.ohpepreset").exists());
    CHECK (u.installedPacks().size() == 1);
    // not a zip at all
    u.installPackFile (d.root.getChildFile ("missing.zip"));
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().state == Updater::State::Failed);

    CHECK (userPreset.loadFileAsString() == "{\"mine\":true}");
}

TEST_CASE ("content loader: downloads only from the release pages (SHA-256 stays mandatory)")
{
    Dir d;
    Updater u;
    u.setDataFolderForTests (d.root.getChildFile ("data"));
    update::Release r;
    r.version = update::Version::of ("99.0.0");
    update::ContentPack p;
    p.id = "atmo";
    p.version = "1.0.0";
    p.kind = "presets";
    p.nameEn = p.nameDe = "Atmo";
    p.url = "https://example.com/Ohpe-content-atmo-1.0.0.zip";
    p.sha256 = std::string (64, 'a');
    p.size = 10;
    r.content.push_back (p);
    u.showReleaseForTests (r);
    u.installPack ("atmo");
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().state == Updater::State::Failed);
    CHECK (u.snapshot().message == "The content pack is not from the Ohpe release pages - not installed");
    CHECK_FALSE (d.root.getChildFile ("data/Content/atmo").exists());
}

TEST_CASE ("content loader: the library reads kind folders and old flat packs, uid unchanged, filter by pack")
{
    Dir d;
    const auto previous = juce::SystemStats::getEnvironmentVariable ("OHPE_DATA_DIR", {});
    const auto previousPresets = juce::SystemStats::getEnvironmentVariable ("OHPE_PRESET_DIR", {});
    setEnv ("OHPE_DATA_DIR", d.root.getFullPathName());
    setEnv ("OHPE_PRESET_DIR", d.root.getChildFile ("Presets").getFullPathName());
    const auto content = d.root.getChildFile ("Content");
    // installed by an older Ohpe: flat
    content.getChildFile ("old-pack").createDirectory();
    content.getChildFile ("old-pack/Flat One.ohpepreset").replaceWithText (presetText ("Flat One"));
    // new layout
    content.getChildFile ("atmo/presets").createDirectory();
    content.getChildFile ("atmo/pack.json").replaceWithText (packJsonV11);
    content.getChildFile ("atmo/presets/pads-night-tide.ohpepreset").replaceWithText (presetText ("Night Tide"));
    content.getChildFile ("atmo/wavetables").createDirectory();
    content.getChildFile (".backup/atmo/presets").createDirectory();
    content.getChildFile (".backup/atmo/presets/Old.ohpepreset").replaceWithText (presetText ("Old"));
    {
        OhpeProcessor proc;
        auto& pm = proc.presets();
        pm.rescan();
        std::map<std::string, std::string> packOf;
        for (const auto& e : pm.entries())
            if (e.uid.rfind ("content:", 0) == 0) packOf[e.uid] = e.pack;
        CHECK (packOf.size() == 2); // backups are not listed
        CHECK (packOf["content:old-pack/Flat One.ohpepreset"] == "old-pack");
        CHECK (packOf["content:atmo/pads-night-tide.ohpepreset"] == "atmo");
        REQUIRE (pm.contentPacks().size() == 2);
        CHECK (pm.contentPacks()[0].id == "atmo");
        CHECK (pm.contentPacks()[0].nameEn == "Atmo");
        CHECK (pm.contentPacks()[1].nameEn == "old-pack");

        pm.filter.pack = "atmo";
        const auto v = pm.view();
        REQUIRE (v.size() == 1);
        CHECK (pm.entries()[(size_t) v[0]].meta.name == "Night Tide");
        CHECK (pm.load (v[0]));
        // the pack goes away: the filter falls back to all
        content.getChildFile ("atmo").deleteRecursively();
        pm.rescan();
        CHECK (pm.filter.pack.empty());
    }
    setEnv ("OHPE_DATA_DIR", previous);
    setEnv ("OHPE_PRESET_DIR", previousPresets);
}

TEST_CASE ("content loader: user wavetables resolve <pack>/<table>; a missing pack is named, not hidden")
{
    Dir d;
    const auto content = d.root.getChildFile ("Content");
    writeWaveSet (content.getChildFile ("glass/wavetables/choir.wav"), 4);
    writeWaveSet (content.getChildFile ("glass/wavetables/vowels/aah.wav"), 2);
    {
        const auto idx = content::scan (content);
        REQUIRE (idx.packs.size() == 1);
        CHECK (idx.resolve (update::ContentRef::parse ("glass/choir"), update::PackKind::Wavetables) == update::RefState::Found);
        CHECK (idx.resolve (update::ContentRef::parse ("glass/vowels/aah"), update::PackKind::Wavetables) == update::RefState::Found);
        CHECK (idx.resolve (update::ContentRef::parse ("glass/nope"), update::PackKind::Wavetables) == update::RefState::ItemMissing);
    }
    OhpeProcessor proc;
    auto& ut = proc.userTables();
    ut.setContentFolder (content);

    auto r = ut.applyPartJson (0, R"({"wavetable":{"name":"Choir","content":"glass/choir","frames":4}})");
    CHECK (r.loaded == 1);
    auto t = usertable::tableOf (proc.core(), 0, usertable::Target::Wavetable);
    REQUIRE (t != nullptr);
    CHECK (t->numFrames() == 4);
    CHECK (t->contentRef == "glass/choir");
    CHECK (ut.missingPacks().isEmpty());
    const auto saved = ut.partJson (0);
    CHECK (saved.find ("\"content\":\"glass/choir\"") != std::string::npos);
    CHECK (saved.find ("\"data\"") == std::string::npos);

    const int msgs = ut.messageCount();
    r = ut.applyPartJson (1, R"({"wavetable":{"name":"Far","content":"far-away/table","frames":8}})");
    CHECK (r.packMissing == 1);
    CHECK (usertable::tableOf (proc.core(), 1, usertable::Target::Wavetable) == nullptr);
    CHECK (ut.missingPacks() == juce::StringArray ("far-away"));
    CHECK (ut.messageCount() > msgs);
    CHECK (ut.lastMessage() == "Content pack missing - install it in Settings > Updates");
    CHECK (ut.lastDetail() == "far-away");

    // the pack is there, the table is not: a missing table, not a missing pack
    r = ut.applyPartJson (1, R"({"wavetable":{"content":"glass/gone"}})");
    CHECK (r.missing == 1);
    CHECK (r.packMissing == 0);
    CHECK (ut.missingPacks().isEmpty());
}
