// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p27: the in-app updater - version order, manifest and release list, channel / skip / remind, SHA-256, resumable
// download (interrupted, range ignored, checksum fail), content pack registry and path safety.
#include "update/ContentPacks.h"
#include "update/Download.h"
#include "update/Sha256.h"
#include "update/UpdateManifest.h"
#include "update/UpdateSettings.h"
#include "update/Version.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace ohpe::update;

TEST_CASE ("update: version parse and order")
{
    CHECK (Version::of ("v1.2.3").valid());
    CHECK (Version::of ("1.2.3").toString() == "1.2.3");
    CHECK (Version::of ("1.0.0-preview.2").toString() == "1.0.0-preview.2");
    CHECK (Version::of ("1.0.0+build.7") == Version::of ("1.0.0"));
    for (const char* bad : { "", "1", "1.2", "1.2.3.4", "a.b.c", "1.02.3", "1.2.3-", "1.2.3-a..b", "v", "-1.2.3" })
        CHECK_MESSAGE (! Version::of (bad).valid(), bad);
    CHECK (Version::of ("0.3.0") < Version::of ("0.4.0"));
    CHECK (Version::of ("0.9.9") < Version::of ("0.10.0"));
    CHECK (Version::of ("1.0.0-preview.2") < Version::of ("1.0.0"));
    CHECK (Version::of ("1.0.0-preview.9") < Version::of ("1.0.0-preview.10"));
    CHECK (Version::of ("1.0.0-alpha") < Version::of ("1.0.0-alpha.1"));
    CHECK (Version::of ("1.0.0-1") < Version::of ("1.0.0-alpha"));
    CHECK (Version::of ("1.0.0-beta") > Version::of ("1.0.0-alpha.9"));
    CHECK (Version::of ("2.0.0") > Version::of ("1.99.99"));
    CHECK (Version::of ("1.2.3") == Version::of ("v1.2.3"));
}

TEST_CASE ("update: SHA-256 test vectors")
{
    CHECK (Sha256::of ("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK (Sha256::of ("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK (Sha256::of ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
           == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // incremental in odd pieces = in one go (one million 'a')
    Sha256 h;
    const std::string chunk (997, 'a');
    size_t left = 1000000;
    while (left > 0)
    {
        const size_t n = std::min (left, chunk.size());
        h.update (chunk.data(), n);
        left -= n;
    }
    CHECK (h.finishHex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    CHECK (Sha256::sameDigest ("BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", Sha256::of ("abc")));
    CHECK_FALSE (Sha256::sameDigest ("", ""));
    CHECK_FALSE (Sha256::sameDigest (Sha256::of ("abd"), Sha256::of ("abc")));
}

namespace
{
const char* manifestText = R"({
  "format": 1, "app": "Ohpe", "version": "1.1.0", "tag": "v1.1.0", "channel": "stable", "date": "2026-11-02",
  "page": "https://example.invalid/releases/v1.1.0",
  "notes": { "en": "New things", "de": "Neues" },
  "assets": [
    { "platform": "macos", "kind": "installer", "part": "all", "name": "Ohpe-1.1.0-macOS-Installer.dmg",
      "url": "https://example.invalid/a.dmg", "size": 1234, "sha256": "AB12" },
    { "platform": "windows", "kind": "archive", "part": "app", "name": "Ohpe-1.1.0-Windows-app.zip",
      "url": "https://example.invalid/app.zip", "size": 10, "sha256": "cd" },
    { "platform": "windows", "kind": "archive", "part": "plugin", "name": "Ohpe-1.1.0-Windows-VST3.zip",
      "url": "https://example.invalid/vst3.zip", "size": 11, "sha256": "ef" },
    { "platform": "linux", "name": "no url" }
  ],
  "content": [
    { "id": "keys-plus", "version": "1.2.0", "kind": "presets", "name": { "en": "Keys Plus", "de": "Tasten Plus" },
      "minApp": "1.0.0", "url": "https://example.invalid/keys.zip", "size": 99, "sha256": "00" },
    { "id": "broken", "version": "x", "url": "https://example.invalid/b.zip" }
  ]
})";
} // namespace

TEST_CASE ("update: manifest parse, round trip, asset choice")
{
    Release r;
    std::string err;
    REQUIRE (parseManifest (manifestText, r, &err));
    CHECK (r.version == Version::of ("1.1.0"));
    CHECK (r.channel == Channel::Stable);
    CHECK (r.notesDe == "Neues");
    CHECK (r.assets.size() == 3); // the asset without url is dropped
    CHECK (r.assets[0].sha256 == "ab12"); // lower case
    REQUIRE (r.content.size() == 1);
    CHECK (r.content[0].nameDe == "Tasten Plus");
    REQUIRE (r.assetFor ("macos", "plugin") != nullptr);
    CHECK (r.assetFor ("macos", "plugin")->kind == "installer");
    CHECK (r.assetFor ("windows", "app")->name == "Ohpe-1.1.0-Windows-app.zip");
    CHECK (r.assetFor ("windows", "plugin")->name == "Ohpe-1.1.0-Windows-VST3.zip");
    CHECK (r.assetFor ("linux", "app") == nullptr);

    Release back;
    REQUIRE (parseManifest (manifestToJson (r), back));
    CHECK (back.version == r.version);
    CHECK (back.assets.size() == r.assets.size());
    CHECK (back.content.size() == 1);
    CHECK (back.content[0].minApp == "1.0.0");

    Release untouched;
    untouched.tag = "keep";
    CHECK_FALSE (parseManifest ("not json", untouched, &err));
    CHECK_FALSE (parseManifest (R"({"format":2,"app":"Ohpe","version":"1.0.0"})", untouched));
    CHECK_FALSE (parseManifest (R"({"format":1,"app":"Other","version":"1.0.0"})", untouched));
    CHECK_FALSE (parseManifest (R"({"format":1,"app":"Ohpe","version":"one"})", untouched));
    CHECK_FALSE (parseManifest ("[1,2,3]", untouched));
    CHECK (untouched.tag == "keep");
}

TEST_CASE ("update: release notes split, checksum list")
{
    std::string en, de;
    splitNotes ("**Preview** text\n\n## English\n\n### New\n- a\n\n## Deutsch\n\n### Neu\n- b\n", en, de);
    CHECK (en == "**Preview** text\n\n### New\n- a");
    CHECK (de == "**Preview** text\n\n### Neu\n- b");
    splitNotes ("just text\r\n", en, de);
    CHECK (en == "just text");
    CHECK (de == "just text");
    splitNotes ("## Deutsch\nnur deutsch", en, de);
    CHECK (de == "nur deutsch");
    CHECK (en == "nur deutsch");

    const std::string d1 (64, 'a'), d2 (64, 'B');
    const auto sums = parseSha256Sums (d1 + "  Ohpe.dmg\n" + d2 + " *Ohpe-app.zip\r\nnot a line\n" + std::string (63, 'c') + "  short\n");
    CHECK (sums.size() == 2);
    CHECK (sums.at ("Ohpe.dmg") == d1);
    CHECK (sums.at ("Ohpe-app.zip") == std::string (64, 'b'));
}

namespace
{
const char* releaseList = R"([
  { "tag_name": "v0.3.0", "prerelease": true, "draft": false, "html_url": "https://example.invalid/v0.3.0",
    "published_at": "2026-10-10T10:00:00Z", "body": "## English\nEN\n## Deutsch\nDE",
    "assets": [ { "name": "Ohpe-0.3.0-macOS-Installer.dmg", "browser_download_url": "https://example.invalid/0.3.0.dmg", "size": 5 },
                { "name": "Ohpe-0.3.0-Windows-app.zip", "browser_download_url": "https://example.invalid/0.3.0-app.zip", "size": 6 },
                { "name": "Ohpe-0.3.0-Windows-VST3.zip", "browser_download_url": "https://example.invalid/0.3.0-vst3.zip", "size": 7 },
                { "name": "SHA256SUMS.txt", "browser_download_url": "https://example.invalid/sums", "size": 8 } ] },
  { "tag_name": "v1.0.0", "prerelease": false, "draft": false,
    "assets": [ { "name": "ohpe-update.json", "browser_download_url": "https://example.invalid/m100.json" } ] },
  { "tag_name": "v1.1.0-preview.1", "prerelease": false, "draft": false, "assets": [] },
  { "tag_name": "v2.0.0", "prerelease": false, "draft": true, "assets": [] },
  { "tag_name": "nightly", "prerelease": true, "draft": false, "assets": [] }
])";
} // namespace

TEST_CASE ("update: release list, channels, skip, previous version")
{
    const auto list = parseReleaseList (releaseList);
    REQUIRE (list.size() == 3); // the draft and the non-version tag are left out
    CHECK (list[0].sumsUrl == "https://example.invalid/sums");
    CHECK (list[0].manifestUrl.empty());
    CHECK (list[0].fallback.notesEn == "EN");
    CHECK (list[0].fallback.notesDe == "DE");
    CHECK (list[0].fallback.date == "2026-10-10");
    CHECK (list[0].fallback.assetFor ("macos", "app")->url == "https://example.invalid/0.3.0.dmg");
    CHECK (list[0].fallback.assetFor ("windows", "plugin")->name == "Ohpe-0.3.0-Windows-VST3.zip");
    CHECK (list[1].manifestUrl == "https://example.invalid/m100.json");
    CHECK (list[2].prerelease); // a pre-release version counts as preview even when not flagged

    Release r = list[0].fallback;
    applySums (r, std::string (64, 'f') + "  Ohpe-0.3.0-Windows-app.zip\n");
    CHECK (r.assetFor ("windows", "app")->sha256 == std::string (64, 'f'));
    CHECK (r.assetFor ("macos", "app")->sha256.empty());

    const auto cur = Version::of ("0.3.0");
    CHECK (pickUpdate (list, Channel::Stable, cur) == 1);
    CHECK (pickUpdate (list, Channel::Preview, cur) == 2);
    CHECK (pickUpdate (list, Channel::Preview, cur, "1.1.0-preview.1") == -1); // skipped: not offered again
    CHECK (pickUpdate (list, Channel::Stable, cur, "1.1.0-preview.1") == 1);   // the skip only covers that version
    CHECK (pickUpdate (list, Channel::Preview, Version::of ("1.1.0-preview.1")) == -1); // up to date
    CHECK (pickUpdate ({}, Channel::Preview, cur) == -1);
    CHECK (pickPrevious (list, Version::of ("1.0.0")) == 0);
    CHECK (pickPrevious (list, Version::of ("0.3.0")) == -1);
    CHECK (parseReleaseList ("{\"message\":\"rate limited\"}").empty());
    CHECK (parseReleaseList ("").empty());
}

TEST_CASE ("update: settings round trip, daily auto check, never while playing, remind / skip")
{
    const auto d0 = Settings::defaults (Version::of ("0.3.0"));
    CHECK_FALSE (d0.autoCheck); // opt-in
    CHECK (d0.channel == Channel::Preview);
    CHECK (Settings::defaults (Version::of ("1.0.0")).channel == Channel::Stable);
    CHECK (Settings::defaults (Version::of ("1.1.0-preview.1")).channel == Channel::Preview);

    Settings s = Settings::defaults (Version::of ("1.0.0"));
    s.autoCheck = true;
    s.channel = Channel::Preview;
    s.lastCheck = 1000;
    s.skipped = "1.2.0";
    remindLater (s, Version::of ("1.3.0"), 5000);
    const auto back = Settings::fromJson (s.toJson(), Settings {});
    CHECK (back.autoCheck);
    CHECK (back.channel == Channel::Preview);
    CHECK (back.lastCheck == 1000);
    CHECK (back.skipped == "1.2.0");
    CHECK (back.remindVersion == "1.3.0");
    CHECK (back.remindAfter == 5000 + remindDelaySeconds);
    const auto broken = Settings::fromJson ("{\"autoCheck\":\"yes\",\"channel\":\"nightly\",\"lastCheck\":-5}", d0);
    CHECK_FALSE (broken.autoCheck);
    CHECK (broken.channel == Channel::Preview);
    CHECK (broken.lastCheck == 0);
    CHECK (Settings::fromJson ("garbage", d0).channel == d0.channel);

    const int64_t day = secondsPerDay;
    Settings a;
    CHECK_FALSE (autoCheckDue (a, 10 * day, false)); // off by default
    a.autoCheck = true;
    CHECK (autoCheckDue (a, 10 * day, false));
    CHECK_FALSE (autoCheckDue (a, 10 * day, true)); // never during playback
    a.lastCheck = 10 * day;
    CHECK_FALSE (autoCheckDue (a, 10 * day + day - 1, false)); // at most once a day
    CHECK (autoCheckDue (a, 11 * day, false));
    CHECK (autoCheckDue (a, 9 * day, false)); // clock went back

    Settings o;
    const auto v = Version::of ("1.3.0");
    CHECK (shouldOffer (o, v, 0));
    remindLater (o, v, 100);
    CHECK_FALSE (shouldOffer (o, v, 101));
    CHECK (shouldOffer (o, Version::of ("1.3.1"), 101)); // a newer version is offered at once
    CHECK (shouldOffer (o, v, 100 + remindDelaySeconds));
    skipVersion (o, v);
    CHECK_FALSE (shouldOffer (o, v, 100 + remindDelaySeconds));
    CHECK_FALSE (shouldOffer (o, Version(), 0));
}

namespace
{
/** A server holding `data` that can break off after `breakAfter` bytes per connection and ignore ranges. */
struct FakeNet : Transport
{
    std::string data;
    size_t breakAfter = 0;    ///< 0 = never
    bool honourRange = true;
    int failOpens = 0;        ///< the next n opens fail
    size_t pos = 0, sentThisConnection = 0;
    std::vector<uint64_t> offsets;

    bool open (const std::string&, uint64_t offset, uint64_t& total, bool& fromOffset) override
    {
        offsets.push_back (offset);
        if (failOpens > 0) { --failOpens; return false; }
        fromOffset = honourRange || offset == 0;
        pos = fromOffset ? (size_t) offset : 0;
        total = data.size();
        sentThisConnection = 0;
        return true;
    }
    int64_t read (uint8_t* buffer, size_t n) override
    {
        if (breakAfter > 0 && sentThisConnection >= breakAfter) return -1;
        if (pos >= data.size()) return 0;
        size_t k = std::min (n, data.size() - pos);
        if (breakAfter > 0) k = std::min (k, breakAfter - sentThisConnection);
        std::copy (data.begin() + (long) pos, data.begin() + (long) (pos + k), buffer);
        pos += k;
        sentThisConnection += k;
        return (int64_t) k;
    }
};

struct MemFile : PartialFile
{
    std::string bytes;
    bool failWrites = false;
    uint64_t size() override { return bytes.size(); }
    bool append (const uint8_t* d, size_t n) override
    {
        if (failWrites) return false;
        bytes.append ((const char*) d, n);
        return true;
    }
    bool clear() override { bytes.clear(); return true; }
    bool readAll (const std::function<void (const uint8_t*, size_t)>& sink) override
    {
        sink ((const uint8_t*) bytes.data(), bytes.size());
        return true;
    }
};

std::string payload (size_t n)
{
    std::string s (n, '\0');
    uint32_t x = 12345;
    for (auto& c : s) { x = x * 1664525u + 1013904223u; c = (char) (x >> 24); }
    return s;
}
} // namespace

TEST_CASE ("update: download completes and verifies")
{
    FakeNet net;
    net.data = payload (300000);
    MemFile f;
    DownloadOptions o;
    o.url = "x";
    o.sha256 = Sha256::of (net.data);
    o.expectedSize = net.data.size();
    uint64_t lastDone = 0, lastTotal = 0;
    o.progress = [&] (uint64_t d, uint64_t t) { CHECK (d >= lastDone); lastDone = d; lastTotal = t; };
    const auto r = download (net, f, o);
    CHECK (r.result == DownloadResult::Ok);
    CHECK (f.bytes == net.data);
    CHECK (r.attempts == 1);
    CHECK (lastDone == net.data.size());
    CHECK (lastTotal == net.data.size());
    CHECK (verifyFile (f, o.sha256, o.expectedSize));
    CHECK_FALSE (verifyFile (f, o.sha256, o.expectedSize + 1));
}

TEST_CASE ("update: interrupted download resumes from the partial file")
{
    FakeNet net;
    net.data = payload (250000);
    net.breakAfter = 70000; // every connection breaks after 70 kB
    MemFile f;
    DownloadOptions o;
    o.url = "x";
    o.sha256 = Sha256::of (net.data);
    int waits = 0;
    o.waitBeforeRetry = [&] (int) { ++waits; };
    const auto r = download (net, f, o);
    CHECK (r.result == DownloadResult::Ok);
    CHECK (f.bytes == net.data);
    CHECK (r.attempts == 4);
    CHECK (r.resumes == 3);
    CHECK (waits == 3);
    REQUIRE (net.offsets.size() == 4);
    CHECK (net.offsets[1] == 70000);
    CHECK (net.offsets[3] == 210000);

    // a later run continues a file left from before
    FakeNet net2;
    net2.data = net.data;
    MemFile f2;
    f2.bytes = net.data.substr (0, 123456);
    const auto r2 = download (net2, f2, o);
    CHECK (r2.result == DownloadResult::Ok);
    CHECK (net2.offsets.front() == 123456);
    CHECK (f2.bytes == net.data);

    // already complete: nothing is fetched
    FakeNet net3;
    net3.data = net.data;
    MemFile f3;
    f3.bytes = net.data;
    DownloadOptions o3 = o;
    o3.expectedSize = net.data.size();
    CHECK (download (net3, f3, o3).result == DownloadResult::Ok);
    CHECK (net3.offsets.empty());
}

TEST_CASE ("update: server ignoring the range starts over; network failure gives up")
{
    FakeNet net;
    net.data = payload (100000);
    net.honourRange = false;
    MemFile f;
    f.bytes = net.data.substr (0, 40000);
    DownloadOptions o;
    o.url = "x";
    o.sha256 = Sha256::of (net.data);
    const auto r = download (net, f, o);
    CHECK (r.result == DownloadResult::Ok);
    CHECK (f.bytes == net.data); // not 40000 bytes + the whole file

    FakeNet dead;
    dead.data = net.data;
    dead.failOpens = 100;
    MemFile g;
    DownloadOptions od = o;
    od.maxAttempts = 3;
    const auto rd = download (dead, g, od);
    CHECK (rd.result == DownloadResult::NetworkFailed);
    CHECK (rd.attempts == 3);

    // two failed opens, then it works (retry)
    FakeNet flaky;
    flaky.data = net.data;
    flaky.failOpens = 2;
    MemFile h;
    CHECK (download (flaky, h, o).result == DownloadResult::Ok);
}

TEST_CASE ("update: checksum fail path deletes the file; size mismatch; cancel; write failure")
{
    FakeNet net;
    net.data = payload (50000);
    MemFile f;
    DownloadOptions o;
    o.url = "x";
    o.sha256 = Sha256::of ("something else");
    const auto r = download (net, f, o);
    CHECK (r.result == DownloadResult::ChecksumMismatch);
    CHECK (f.bytes.empty());
    CHECK (r.digest == Sha256::of (net.data));

    MemFile g;
    DownloadOptions os = o;
    os.sha256 = Sha256::of (net.data);
    os.expectedSize = 40000; // the server has more than the manifest says
    CHECK (download (net, g, os).result == DownloadResult::SizeMismatch);
    CHECK (g.bytes.empty());

    std::atomic<bool> cancel { true };
    MemFile c;
    DownloadOptions oc = os;
    oc.expectedSize = 0;
    oc.cancel = &cancel;
    CHECK (download (net, c, oc).result == DownloadResult::Cancelled);

    MemFile w;
    w.failWrites = true;
    DownloadOptions ow = os;
    ow.expectedSize = 0;
    CHECK (download (net, w, ow).result == DownloadResult::WriteFailed);
}

TEST_CASE ("update: content packs - registry, backup / rollback, safe paths, state")
{
    ContentRegistry reg;
    reg.installed ("keys-plus", "1.0.0");
    CHECK (reg.find ("keys-plus")->previousVersion.empty());
    reg.installed ("keys-plus", "1.2.0");
    CHECK (reg.find ("keys-plus")->version == "1.2.0");
    CHECK (reg.find ("keys-plus")->previousVersion == "1.0.0");
    const auto back = ContentRegistry::fromJson (reg.toJson());
    REQUIRE (back.find ("keys-plus") != nullptr);
    CHECK (back.find ("keys-plus")->previousVersion == "1.0.0");
    CHECK (reg.rolledBack ("keys-plus"));
    CHECK (reg.find ("keys-plus")->version == "1.0.0");
    CHECK_FALSE (reg.rolledBack ("keys-plus")); // one step only
    reg.removed ("keys-plus");
    CHECK (reg.find ("keys-plus") == nullptr);
    CHECK (ContentRegistry::fromJson ("{\"packs\":[{\"id\":\"../x\",\"version\":\"1.0.0\"},{\"id\":\"ok\",\"version\":\"bad\"}]}").packs.empty());

    for (const char* ok : { "keys-plus", "a", "drums_2.1" }) CHECK_MESSAGE (isSafePackId (ok), ok);
    for (const char* bad : { "", ".hidden", "Keys", "a/b", "a b", ".." }) CHECK_MESSAGE (! isSafePackId (bad), bad);
    CHECK_FALSE (isSafePackId (std::string (65, 'a')));
    for (const char* ok : { "presets/Keys/Night.ohpepreset", "readme.txt", "presets/" }) CHECK_MESSAGE (isSafeEntryPath (ok), ok);
    for (const char* bad : { "", "/etc/passwd", "../Presets/x", "a/../../b", "C:\\Windows\\x", "a\\..\\b", "a//b", "./a", "a/b:c" })
        CHECK_MESSAGE (! isSafeEntryPath (bad), bad);

    ContentPack p;
    p.id = "keys-plus";
    p.version = "1.2.0";
    p.minApp = "1.1.0";
    ContentRegistry r2;
    CHECK (packState (p, r2, Version::of ("1.0.0")) == PackState::NeedsNewerApp);
    CHECK (packState (p, r2, Version::of ("1.1.0")) == PackState::NotInstalled);
    r2.installed ("keys-plus", "1.0.0");
    CHECK (packState (p, r2, Version::of ("1.1.0")) == PackState::UpdateAvailable);
    r2.installed ("keys-plus", "1.2.0");
    CHECK (packState (p, r2, Version::of ("1.1.0")) == PackState::Installed);
}
