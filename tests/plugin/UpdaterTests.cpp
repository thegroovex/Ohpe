// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p27: the in-app updater with local files instead of the network (file:// release list, manifest, installer and
// content pack): check, verified download, checksum fail path, skip / remind, content pack install / backup /
// rollback (user files untouched, unsafe packs refused), the update view in the editor, both languages.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Updater.h"
#include "ui/PluginEditor.h"
#include "update/Sha256.h"

using namespace ohpe;

namespace
{
std::string std8 (const juce::String& s) { return s.toStdString(); }
juce::String url (const juce::File& f) { return juce::URL (f).toString (false); }

std::string sha (const juce::File& f)
{
    juce::MemoryBlock mb;
    f.loadFileAsData (mb);
    return update::Sha256::of (std::string ((const char*) mb.getData(), mb.getSize()));
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

/** A folder with a release list (one newer release with a manifest, one older), an installer and two content packs. */
struct Fixture
{
    juce::TemporaryFile tmpDir;
    juce::File root, data, srv;
    juce::File installer, pack1, pack2, evil;
    juce::String goodSha;

    Fixture()
    {
        root = tmpDir.getFile();
        root.createDirectory();
        data = root.getChildFile ("data");
        srv = root.getChildFile ("server");
        srv.createDirectory();
        installer = srv.getChildFile ("Ohpe-99.0.0-Installer.bin");
        juce::String big;
        for (int i = 0; i < 20000; ++i) big << "Ohpe update payload line " << i << "\n";
        installer.replaceWithText (big);
        goodSha = sha (installer);
        pack1 = makeZip (srv.getChildFile ("pack-1.0.0.zip"), { { "presets/Night Keys.ohpepreset", "{}" }, { "readme.txt", "v1" } });
        pack2 = makeZip (srv.getChildFile ("pack-1.1.0.zip"), { { "presets/Night Keys.ohpepreset", "{}" }, { "readme.txt", "v2" } });
        evil = makeZip (srv.getChildFile ("evil.zip"), { { "../escape.txt", "x" } });
    }
    ~Fixture() { root.deleteRecursively(); }

    void writeServer (const juce::String& installerSha, const juce::String& packVersion, const juce::File& packFile, const juce::String& packId = "keys-plus")
    {
        update::Release r;
        r.version = update::Version::of ("99.0.0");
        r.tag = "v99.0.0";
        r.channel = update::Channel::Stable;
        r.notesEn = "## New\n- **Faster** things";
        r.notesDe = "## Neu\n- **Schnellere** Dinge";
        update::Asset a;
        a.platform = update::currentPlatform();
        a.kind = "archive"; // opened in the file manager, not launched (tests never run it)
        a.part = "all";
        a.name = std8 (installer.getFileName());
        a.url = std8 (url (installer));
        a.size = (uint64_t) installer.getSize();
        a.sha256 = std8 (installerSha);
        r.assets.push_back (a);
        update::ContentPack p;
        p.id = std8 (packId);
        p.version = std8 (packVersion);
        p.kind = "presets";
        p.nameEn = "Keys Plus";
        p.nameDe = "Tasten Plus";
        p.url = std8 (url (packFile));
        p.size = (uint64_t) packFile.getSize();
        p.sha256 = sha (packFile);
        r.content.push_back (p);
        srv.getChildFile ("ohpe-update.json").replaceWithText (juce::String::fromUTF8 (update::manifestToJson (r).c_str()));
        juce::String list;
        list << "[ { \"tag_name\": \"v99.0.0\", \"prerelease\": false, \"draft\": false, \"assets\": [ { \"name\": \"ohpe-update.json\", "
             << "\"browser_download_url\": \"" << url (srv.getChildFile ("ohpe-update.json")) << "\" } ] },"
             << "  { \"tag_name\": \"v0.0.1\", \"prerelease\": false, \"draft\": false, \"body\": \"old\", \"assets\": [] } ]";
        srv.getChildFile ("releases.json").replaceWithText (list);
    }
};
} // namespace

TEST_CASE ("updater: check, verified download, skip and remind (local files)")
{
    Fixture fx;
    fx.writeServer (fx.goodSha, "1.0.0", fx.pack1);
    Updater u;
    u.setDataFolderForTests (fx.data);
    u.setReleaseListUrl (url (fx.srv.getChildFile ("releases.json")));
    CHECK_FALSE (u.settings().autoCheck); // opt-in
    u.setChannel (update::Channel::Stable);
    u.checkNow();
    REQUIRE (u.waitIdle (20000));
    auto s = u.snapshot();
    CHECK (s.state == Updater::State::Available);
    REQUIRE (s.hasRelease);
    CHECK (s.release.version == update::Version::of ("99.0.0"));
    CHECK (s.release.notesDe.find ("Schnellere") != std::string::npos);
    CHECK (s.hasPrevious);
    CHECK (s.previous.version == update::Version::of ("0.0.1"));
    REQUIRE (s.packs.size() == 1);
    CHECK (s.packs[0].state == update::PackState::NotInstalled);
    CHECK (u.settings().lastCheck > 0);
    CHECK (fx.data.getChildFile ("Settings/update.json").existsAsFile());

    u.downloadUpdate();
    REQUIRE (u.waitIdle (20000));
    s = u.snapshot();
    CHECK (s.state == Updater::State::Ready);
    CHECK (s.downloaded.existsAsFile());
    CHECK (sha (s.downloaded) == std8 (fx.goodSha));
    CHECK_FALSE (s.downloaded.withFileExtension (s.downloaded.getFileExtension() + ".part").exists());

    // remind later keeps it out of automatic offers, skip removes it until a newer version appears
    u.remindLater();
    CHECK (u.settings().remindVersion == "99.0.0");
    u.skipThisVersion();
    CHECK (u.settings().skipped == "99.0.0");
    CHECK_FALSE (u.snapshot().hasRelease);
    u.checkNow();
    REQUIRE (u.waitIdle (20000));
    CHECK_FALSE (u.snapshot().hasRelease);

    // settings survive a restart
    Updater again;
    again.setDataFolderForTests (fx.data);
    CHECK (again.settings().skipped == "99.0.0");
    CHECK (again.settings().channel == update::Channel::Stable);
}

TEST_CASE ("updater: checksum fail path - the download is deleted, nothing is opened")
{
    Fixture fx;
    fx.writeServer (juce::String::repeatedString ("0", 64), "1.0.0", fx.pack1);
    Updater u;
    u.setDataFolderForTests (fx.data);
    u.setReleaseListUrl (url (fx.srv.getChildFile ("releases.json")));
    u.checkNow();
    REQUIRE (u.waitIdle (20000));
    REQUIRE (u.snapshot().hasRelease);
    u.downloadUpdate();
    REQUIRE (u.waitIdle (20000));
    const auto s = u.snapshot();
    CHECK (s.state == Updater::State::Failed);
    CHECK (s.message == "The download failed the checksum test and was deleted");
    CHECK (fx.data.getChildFile ("Updates").findChildFiles (juce::File::findFiles, true).isEmpty());
    CHECK_FALSE (u.openDownloaded());
}

TEST_CASE ("updater: no connection is reported, not an error dialog")
{
    Fixture fx;
    Updater u;
    u.setDataFolderForTests (fx.data);
    u.setReleaseListUrl (url (fx.srv.getChildFile ("missing.json")));
    u.checkNow();
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().state == Updater::State::Offline);
    CHECK_FALSE (u.snapshot().hasRelease);
}

TEST_CASE ("updater: content packs - install, update with backup, rollback, user files untouched, unsafe pack refused")
{
    Fixture fx;
    const auto userPreset = fx.data.getChildFile ("Presets").getChildFile ("Mine.ohpepreset");
    userPreset.getParentDirectory().createDirectory();
    userPreset.replaceWithText ("{\"mine\":true}");

    fx.writeServer (fx.goodSha, "1.0.0", fx.pack1);
    Updater u;
    u.setDataFolderForTests (fx.data);
    u.setReleaseListUrl (url (fx.srv.getChildFile ("releases.json")));
    u.checkNow();
    REQUIRE (u.waitIdle (20000));
    const int gen0 = u.contentGeneration();
    u.installPack ("keys-plus");
    REQUIRE (u.waitIdle (20000));
    const auto content = fx.data.getChildFile ("Content");
    CHECK (content.getChildFile ("keys-plus/presets/Night Keys.ohpepreset").existsAsFile());
    CHECK (content.getChildFile ("keys-plus/readme.txt").loadFileAsString() == "v1");
    CHECK (u.snapshot().packs[0].state == update::PackState::Installed);
    CHECK (u.contentGeneration() == gen0 + 1);

    fx.writeServer (fx.goodSha, "1.1.0", fx.pack2);
    u.checkNow();
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().packs[0].state == update::PackState::UpdateAvailable);
    u.installPack ("keys-plus");
    REQUIRE (u.waitIdle (20000));
    CHECK (content.getChildFile ("keys-plus/readme.txt").loadFileAsString() == "v2");
    CHECK (content.getChildFile (".backup/keys-plus/readme.txt").loadFileAsString() == "v1");
    CHECK (u.snapshot().packs[0].canRollBack);

    CHECK (u.rollbackPack ("keys-plus"));
    CHECK (content.getChildFile ("keys-plus/readme.txt").loadFileAsString() == "v1");
    CHECK_FALSE (content.getChildFile (".backup/keys-plus").exists());
    CHECK (u.snapshot().packs[0].state == update::PackState::UpdateAvailable);
    CHECK_FALSE (u.rollbackPack ("keys-plus")); // one step only

    // a pack with a path outside its folder is refused as a whole
    fx.writeServer (fx.goodSha, "2.0.0", fx.evil, "evil");
    u.checkNow();
    REQUIRE (u.waitIdle (20000));
    u.installPack ("evil");
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().state == Updater::State::Failed);
    CHECK_FALSE (content.getChildFile ("evil").exists());
    CHECK_FALSE (content.getChildFile ("escape.txt").exists());
    CHECK_FALSE (fx.data.getChildFile ("escape.txt").exists());

    CHECK (userPreset.loadFileAsString() == "{\"mine\":true}");
}

TEST_CASE ("updater: the update view in the editor (both languages), notes as plain text")
{
    CHECK (ui::UpdateView::plainNotes ("## New\n\n- **Bold** item\n\n\n### More\ntext") == juce::String::fromUTF8 ("NEW\n\xe2\x80\xa2 Bold item\n\nMORE\ntext"));
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    update::Release r;
    r.version = update::Version::of ("99.0.0");
    r.channel = update::Channel::Preview;
    r.notesEn = "## English notes\n- one";
    r.notesDe = "## Deutsche Hinweise\n- eins";
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        e.showUpdates (true);
        CHECK (e.updatesVisible());
        CHECK (e.overlayOpen());
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        e.updaterInstance().showReleaseForTests (r);
        CHECK (e.updateComponent().primaryAction() == "update");
        e.advanceAnimation (0.1f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        e.showLibrary (true); // another overlay closes it
        CHECK_FALSE (e.updatesVisible());
        e.showLibrary (false);
    }
    e.showUpdates (true);
    e.showUpdates (false);
    CHECK_FALSE (e.overlayOpen());
    i18n::setCurrent (i18n::Language::German);
    CHECK (i18n::tr ("Check now") == juce::String::fromUTF8 ("Jetzt pr\xc3\xbc" "fen"));
    CHECK (i18n::tr ("Skip this version") == juce::String::fromUTF8 ("Diese Version \xc3\xbc" "berspringen"));
    i18n::setCurrent (i18n::Language::English);
}
