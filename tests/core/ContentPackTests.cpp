// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// Content packs - the shared loader base: pack.json, one folder per kind (old flat packs), size limits and quota,
// content references "<pack>/<item>" and the "content pack missing" state of user wavetables.
#include "EngineTestKit.h"
#include "OhpeCore.h"
#include "preset/PresetLibrary.h"
#include "update/PackFormat.h"
#include "wavetable/UserTableState.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace ohpe;
using namespace ohpe::update;

namespace
{
const char* atmoJson = R"({"format":1,"id":"atmo","version":"1.0.0","kind":"presets","minApp":"0.4.0",
    "name":{"en":"Atmo","de":"Atmo DE"},"description":{"en":"Dark and wide","de":"Dunkel und weit"}})";

PackInfo expectedOf (const std::string& id, const std::string& version, const std::string& kind = "presets")
{
    PackInfo e;
    e.id = id;
    e.version = version;
    e.kind = kind;
    return e;
}

const PackPlan::File* fileTo (const PackPlan& p, const std::string& target)
{
    for (const auto& f : p.files)
        if (f.target == target) return &f;
    return nullptr;
}

usertable::UserTablePtr ramp (const std::string& name, int frames)
{
    std::vector<float> v ((size_t) frames * usertable::frameSize);
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) std::sin (6.283185307 * (double) (i % usertable::frameSize) / usertable::frameSize * (1 + (int) (i / usertable::frameSize)));
    return usertable::UserTable::create (name, v);
}
} // namespace

TEST_CASE ("content packs: pack.json parse, defaults, round trip, invalid fields")
{
    PackInfo p;
    std::string err;
    REQUIRE (parsePackInfo (atmoJson, p, &err));
    CHECK (p.id == "atmo");
    CHECK (p.version == "1.0.0");
    CHECK (p.kind == "presets");
    CHECK (p.nameDe == "Atmo DE");
    CHECK (p.descriptionEn == "Dark and wide");
    CHECK (p.minApp == "0.4.0");
    PackInfo back;
    REQUIRE (parsePackInfo (packInfoToJson (p), back));
    CHECK (back.descriptionDe == "Dunkel und weit");

    // the pack repository's pack.json has no "format" key and the keys in another order
    REQUIRE (parsePackInfo (R"({"version":"1.0.0","kind":"presets","name":{"en":"Bass"},"id":"bass-music"})", p));
    CHECK (p.nameDe == "Bass"); // DE falls back to EN
    REQUIRE (parsePackInfo (R"({"version":"2.0.0","id":"x"})", p));
    CHECK (p.kind == "presets"); // no kind = presets
    CHECK (p.nameEn == "x");

    for (const char* bad : { "", "[]", "{", R"({"id":"Bad Id","version":"1.0.0"})", R"({"id":"a","version":"one"})",
                             R"({"id":"a","version":"1.0.0","kind":"spaceships"})", R"({"id":"a","version":"1.0.0","minApp":"soon"})",
                             R"({"format":9,"id":"a","version":"1.0.0"})" })
        CHECK_MESSAGE (! parsePackInfo (bad, p, &err), bad);

    CHECK_FALSE (parsePackInfo (R"({"id":"drums_2.1","version":"1.0.0"})", p)); // pack.json ids: a-z, 0-9, - only
    CHECK (isPackJsonId ("drum-and-bass"));
    CHECK_FALSE (isPackJsonId ("-x"));

    std::string id, ver;
    CHECK (parsePackFileName ("Ohpe-content-drum-and-bass-1.0.0.zip", id, ver));
    CHECK (id == "drum-and-bass");
    CHECK (ver == "1.0.0");
    CHECK (parsePackFileName ("Ohpe-content-atmo-1.2.0-beta.1.zip", id, ver));
    CHECK (ver == "1.2.0-beta.1");
    CHECK_FALSE (parsePackFileName ("atmo.zip", id, ver));
    CHECK_FALSE (parsePackFileName ("Ohpe-content-../x-1.0.0.zip", id, ver));

    CHECK (kindFromName ("wavetables") == PackKind::Wavetables);
    CHECK (std::string (kindFolder (PackKind::Presets)) == "presets");
    CHECK (std::string (kindFolder (PackKind::Wavetables)) == "wavetables");
    CHECK (kindFromName ("samples") == PackKind::Unknown);
}

TEST_CASE ("content packs: checksums per file, license and signature (read, not enforced), canonical pack.json")
{
    const std::string sumA (64, 'a'), sumB (64, 'b');
    const std::string json = R"({"format":1,"id":"atmo","version":"1.0.0","kind":"presets","minApp":"0.5.0",
        "name":{"en":"Atmo","de":"Atmo"},
        "files":{"presets/a.ohpepreset":")" + sumA + R"(","readme.txt":")" + std::string (64, 'B') + R"("},
        "license":{"type":"paid","productId":"ohpe-atmo"},
        "signature":{"alg":"ed25519","keyId":"groovex-1","value":"c2lnbmF0dXJl"}})";
    PackInfo p;
    std::string err;
    REQUIRE (parsePackInfo (json, p, &err));
    CHECK (p.files.size() == 2);
    CHECK (p.files["readme.txt"] == sumB); // lower case
    CHECK (p.license.paid());
    CHECK (p.license.productId == "ohpe-atmo");
    CHECK (p.signature.present());
    CHECK (p.signature.keyId == "groovex-1");
    PackInfo back;
    REQUIRE (parsePackInfo (packInfoToJson (p), back));
    CHECK (back.files == p.files);
    CHECK (back.license.productId == "ohpe-atmo");
    CHECK (back.signature.value == "c2lnbmF0dXJl");
    // no license = free
    REQUIRE (parsePackInfo (atmoJson, back));
    CHECK_FALSE (back.license.paid());
    CHECK_FALSE (back.signature.present());
    for (const char* bad : { R"({"id":"a","version":"1.0.0","files":{"x.ohpepreset":"123"}})",
                             R"({"id":"a","version":"1.0.0","files":{"../x":"aaaa"}})",
                             R"({"id":"a","version":"1.0.0","files":[]})",
                             R"({"id":"a","version":"1.0.0","license":{"type":"rented"}})",
                             R"({"id":"a","version":"1.0.0","signature":42})" })
        CHECK_MESSAGE (! parsePackInfo (bad, p, &err), bad);

    // the signature covers pack.json without "signature", keys sorted, no white space
    const auto canon = canonicalPackJson (json);
    CHECK (canon.find ("signature") == std::string::npos);
    CHECK (canon.find (' ') == std::string::npos);
    CHECK (canon.find ("\"files\"") < canon.find ("\"id\""));
    CHECK (canon == canonicalPackJson (R"({"signature":"other","version":"1.0.0","id":"atmo","name":{"de":"Atmo","en":"Atmo"},"minApp":"0.5.0",
        "license":{"productId":"ohpe-atmo","type":"paid"},"kind":"presets","format":1,
        "files":{"readme.txt":")" + std::string (64, 'B') + R"(","presets/a.ohpepreset":")" + sumA + R"("}})"));
    CHECK (canonicalPackJson ("[]").empty());
    // today's gate allows every pack
    CHECK (openGate().access (p, canon) == PackAccess::Allowed);

    // plan: checksums travel with the files; the list must match the zip exactly
    const std::vector<ZipItem> items { { "pack.json", 500 }, { "presets/a.ohpepreset", 10 }, { "readme.txt", 5 }, { "cover.png", 3 } };
    auto plan = planPack (items, json, {}, 0, true);
    REQUIRE (plan.ok);
    REQUIRE (fileTo (plan, "presets/a.ohpepreset") != nullptr);
    CHECK (fileTo (plan, "presets/a.ohpepreset")->sha256 == sumA);
    CHECK (fileTo (plan, "pack.json")->sha256.empty());
    CHECK (plan.info.license.paid());
    // a content file that is not listed
    auto more = items;
    more.push_back ({ "presets/b.ohpepreset", 10 });
    CHECK (planPack (more, json, {}, 0, true).error == "A file of the content pack is not in its checksum list");
    // a listed file that is not there
    CHECK (planPack ({ { "pack.json", 500 }, { "presets/a.ohpepreset", 10 } }, json, {}, 0, true).error == "The content pack is not complete");
    // .ohpepack: pack.json and checksums are required
    CHECK (planPack ({ { "presets/a.ohpepreset", 10 } }, {}, expectedOf ("atmo", "1.0.0"), 0, true).error
           == "The content pack has no description (pack.json)");
    CHECK (planPack ({ { "pack.json", 50 }, { "presets/a.ohpepreset", 10 } }, atmoJson, {}, 0, true).error == "The content pack has no checksums");
    CHECK (planPack ({ { "pack.json", 50 }, { "presets/a.ohpepreset", 10 } }, atmoJson, {}, 0, false).ok); // own content without sums
}

TEST_CASE ("content packs: plan - kind folder, old flat packs, notes kept, other files skipped")
{
    // new layout
    {
        const std::vector<ZipItem> items { { "pack.json", 300 }, { "presets/", 0, true }, { "presets/Keys/Night.ohpepreset", 900 },
                                           { "presets/Pad.ohpepreset", 800 }, { "readme.txt", 20 }, { "wavetables/x.wav", 5000 },
                                           { "presets/notes.txt", 10 }, { "art/cover.png", 99 } };
        const auto p = planPack (items, atmoJson, {});
        REQUIRE (p.ok);
        CHECK (p.hasPackInfo);
        CHECK_FALSE (p.flat);
        CHECK (p.kind == PackKind::Presets);
        CHECK (p.info.nameDe == "Atmo DE");
        CHECK (fileTo (p, "presets/Keys/Night.ohpepreset") != nullptr);
        CHECK (fileTo (p, "pack.json") != nullptr);
        CHECK (fileTo (p, "readme.txt") != nullptr);
        CHECK (p.skipped == 3); // a wavetable in a presets pack, a note inside the kind folder, a picture
        CHECK (p.totalBytes == 300 + 900 + 800 + 20);
    }
    // old flat pack (the pack repository's first zips): *.ohpepreset at the root, no pack.json
    {
        const std::vector<ZipItem> items { { "bass-abyss-reese.ohpepreset", 1000 }, { "pads-night-tide.ohpepreset", 1200 } };
        const auto p = planPack (items, {}, expectedOf ("atmo", "1.0.0"));
        REQUIRE (p.ok);
        CHECK (p.flat);
        CHECK_FALSE (p.hasPackInfo);
        CHECK (fileTo (p, "presets/bass-abyss-reese.ohpepreset") != nullptr);
        CHECK (p.info.nameEn == "atmo");
        // without a manifest entry or file name it has no id: refused
        CHECK_FALSE (planPack (items, {}, {}).ok);
    }
    // a flat file and the same file in the kind folder: refused (two files for one place)
    CHECK_FALSE (planPack ({ { "a.ohpepreset", 1 }, { "presets/a.ohpepreset", 1 } }, {}, expectedOf ("p", "1.0.0")).ok);
    // wavetable pack
    {
        const auto p = planPack ({ { "pack.json", 100 }, { "wavetables/glass-choir.wav", 300000 }, { "wavetables/vowels/aah.WAV", 300000 } },
                                 R"({"id":"glass","version":"1.0.0","kind":"wavetables"})", expectedOf ("glass", "1.0.0", "wavetables"));
        REQUIRE (p.ok);
        CHECK (p.kind == PackKind::Wavetables);
        CHECK (fileTo (p, "wavetables/vowels/aah.WAV") != nullptr);
    }
}

TEST_CASE ("content packs: plan refuses unsafe, mismatching, unknown, empty, too big packs and the quota")
{
    const std::vector<ZipItem> ok { { "presets/a.ohpepreset", 100 } };
    for (const char* bad : { "../escape.ohpepreset", "/abs.ohpepreset", "C:\\x.ohpepreset", "presets/../../x.ohpepreset", "a//b" })
    {
        auto items = ok;
        items.push_back ({ bad, 10 });
        const auto p = planPack (items, {}, expectedOf ("p", "1.0.0"));
        CHECK_MESSAGE (! p.ok, bad);
        CHECK (p.error == "The content pack is damaged");
        CHECK (p.files.empty());
    }
    CHECK_FALSE (planPack ({}, {}, expectedOf ("p", "1.0.0")).ok);
    // pack.json must agree with the manifest entry
    CHECK (planPack (ok, atmoJson, expectedOf ("other", "1.0.0")).error == "The content pack does not match its description");
    CHECK (planPack (ok, atmoJson, expectedOf ("atmo", "1.1.0")).error == "The content pack does not match its description");
    CHECK (planPack (ok, atmoJson, expectedOf ("atmo", "1.0.0", "wavetables")).error == "The content pack does not match its description");
    CHECK (planPack (ok, "{oops", {}).error == "The pack description (pack.json) is not valid");
    CHECK (planPack (ok, {}, expectedOf ("p", "1.0.0", "samples")).error == "This kind of content pack needs a newer Ohpe");
    CHECK (planPack ({ { "readme.txt", 10 } }, {}, expectedOf ("p", "1.0.0")).error == "The content pack holds no content");

    const auto lim = limitsFor (PackKind::Presets);
    CHECK (planPack ({ { "presets/huge.ohpepreset", lim.maxFileBytes + 1 } }, {}, expectedOf ("p", "1.0.0")).error
           == "A file of the content pack is too big");
    std::vector<ZipItem> many;
    for (int i = 0; i <= lim.maxFiles; ++i) many.push_back ({ "presets/p" + std::to_string (i) + ".ohpepreset", 10 });
    CHECK (planPack (many, {}, expectedOf ("p", "1.0.0")).error == "The content pack has too many files");
    std::vector<ZipItem> big;
    for (uint64_t n = 0; n * lim.maxFileBytes <= lim.maxPackBytes; ++n) big.push_back ({ "presets/b" + std::to_string (n) + ".ohpepreset", lim.maxFileBytes });
    CHECK (planPack (big, {}, expectedOf ("p", "1.0.0")).error == "The content pack is too big");
    // quota of Content/
    CHECK (planPack (ok, {}, expectedOf ("p", "1.0.0"), contentQuotaBytes - 50).error == "Not enough room for content packs - remove a pack first");
    CHECK (planPack (ok, {}, expectedOf ("p", "1.0.0"), contentQuotaBytes - 100).ok);
    // wavetable limits are larger than preset limits
    CHECK (limitsFor (PackKind::Wavetables).maxFileBytes > lim.maxFileBytes);
    CHECK (limitsFor (PackKind::Unknown).maxPackBytes == 0);
}

TEST_CASE ("content packs: library uid stays the same for flat and kind-folder packs, pack filter")
{
    CHECK (contentUidPath ("atmo", "presets/pads-night-tide.ohpepreset") == "atmo/pads-night-tide.ohpepreset");
    CHECK (contentUidPath ("atmo", "pads-night-tide.ohpepreset") == "atmo/pads-night-tide.ohpepreset");
    CHECK (contentUidPath ("glass", "wavetables/vowels/aah.wav") == "glass/vowels/aah.wav");

    std::vector<preset::Entry> list (4);
    list[0].meta.name = "A";
    list[0].pack = "atmo";
    list[0].factory = true;
    list[1].meta.name = "B";
    list[1].pack = "bass-music";
    list[1].factory = true;
    list[2].meta.name = "C";
    list[2].factory = true;
    list[3].meta.name = "D";
    preset::Filter f;
    CHECK (preset::filterAndSort (list, f).size() == 4);
    f.pack = "atmo";
    CHECK (preset::filterAndSort (list, f) == std::vector<int> { 0 });
    f.pack = "gone";
    CHECK (preset::filterAndSort (list, f).empty());
    CHECK (preset::usedPacks (list) == std::vector<std::string> { "atmo", "bass-music" });
}

TEST_CASE ("content packs: references <pack>/<item>, index resolve")
{
    const auto r = ContentRef::parse ("atmo/glass-choir");
    REQUIRE (r.valid());
    CHECK (r.pack == "atmo");
    CHECK (r.item == "glass-choir");
    CHECK (r.toString() == "atmo/glass-choir");
    CHECK (ContentRef::parse ("atmo/vowels/aah").item == "vowels/aah");
    for (const char* bad : { "", "atmo", "atmo/", "/x", "Atmo/x", "atmo/../x", "atmo/a\\b", "../x/y", "atmo//x" })
        CHECK_MESSAGE (! ContentRef::parse (bad).valid(), bad);

    ContentIndex idx;
    idx.packs.push_back ({ "atmo", "1.0.0", "Atmo", "Atmo", { { PackKind::Wavetables, "glass-choir" }, { PackKind::Presets, "pad" } } });
    CHECK (idx.resolve (r, PackKind::Wavetables) == RefState::Found);
    CHECK (idx.resolve (ContentRef::parse ("atmo/pad"), PackKind::Wavetables) == RefState::ItemMissing);
    CHECK (idx.resolve (ContentRef::parse ("atmo/nope"), PackKind::Wavetables) == RefState::ItemMissing);
    CHECK (idx.resolve (ContentRef::parse ("bass-music/x"), PackKind::Wavetables) == RefState::PackMissing);
    CHECK (idx.resolve (ContentRef {}, PackKind::Wavetables) == RefState::Invalid);
    CHECK (std::string (refStateName (RefState::PackMissing)) == "pack-missing");

}

TEST_CASE ("content packs: user wavetables keep pack references; a missing pack is reported, never silently replaced")
{
    using namespace usertable;
    auto packTable = std::const_pointer_cast<UserTable> (ramp ("glass-choir", 4));
    REQUIRE (packTable != nullptr);
    packTable->path = "/content/atmo/wavetables/glass-choir.wav";
    packTable->contentRef = "atmo/glass-choir";
    auto mine = std::const_pointer_cast<UserTable> (ramp ("Mine", 2));
    mine->path = "/nowhere/Mine.ohpewt";

    Core a;
    a.prepare (test::sampleRate, 256);
    setTable (a, 0, Target::Wavetable, packTable);
    setTable (a, 1, Target::Wavetable, mine);
    const auto saved = saveAssignments (a);
    CHECK (saved.find ("\"content\":\"atmo/glass-choir\"") != std::string::npos);
    CHECK (saved.find ("glass-choir.wav") == std::string::npos); // no path, no file name for pack content
    const auto partText = partToJson (a, 0);
    CHECK (partText.find ("\"data\"") == std::string::npos);     // never embedded

    // the pack is installed: the content resolver finds it, the file resolver is not asked for it
    Core b;
    b.prepare (test::sampleRate, 256);
    int fileAsks = 0;
    const Resolver files = [&] (const std::string&, const std::string&) -> UserTablePtr { ++fileAsks; return nullptr; };
    const ContentResolver found = [&] (const update::ContentRef& ref, update::RefState& st) -> UserTablePtr {
        st = ref.toString() == "atmo/glass-choir" ? update::RefState::Found : update::RefState::ItemMissing;
        return st == update::RefState::Found ? UserTablePtr (packTable) : nullptr;
    };
    auto r = restoreAssignments (b, saved, files, found);
    CHECK (r.loaded == 1);
    CHECK (r.embedded == 1); // the small user table
    CHECK (r.packMissing == 0);
    CHECK (fileAsks == 1);
    CHECK (tableOf (b, 0, Target::Wavetable) == packTable);

    // the pack is not installed: "content pack missing", the part plays the built-in table (no embedded copy)
    const ContentResolver none = [] (const update::ContentRef&, update::RefState& st) -> UserTablePtr {
        st = update::RefState::PackMissing;
        return nullptr;
    };
    r = restoreAssignments (b, saved, files, none);
    CHECK (r.packMissing == 1);
    CHECK (r.missing == 1);
    CHECK (r.missingPacks == std::vector<std::string> { "atmo" });
    CHECK (tableOf (b, 0, Target::Wavetable) == nullptr);
    // no content resolver at all counts the same way
    r = partFromJson (b, 0, partText, files);
    CHECK (r.packMissing == 1);
    // the pack is there but the table is not: missing, but not "pack missing"
    const ContentResolver noItem = [] (const update::ContentRef&, update::RefState& st) -> UserTablePtr {
        st = update::RefState::ItemMissing;
        return nullptr;
    };
    r = partFromJson (b, 0, partText, files, noItem);
    CHECK (r.missing == 1);
    CHECK (r.packMissing == 0);
    // a broken reference is missing, never a crash
    r = partFromJson (b, 0, R"({"wavetable":{"content":"../etc/passwd"}})", files, found);
    CHECK (r.missing == 1);
    CHECK (r.packMissing == 0);
    // missing packs are listed once over all parts
    RestoreResult t;
    t.add (r);
    RestoreResult m;
    m.missingPacks = { "atmo" };
    t.add (m);
    t.add (m);
    CHECK (t.missingPacks.size() == 1);
}
