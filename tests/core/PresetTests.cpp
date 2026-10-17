// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// 0.3 (package E): preset format, part / project round-trip, library filtering.
#include "OhpeCore.h"
#include "model/StateSerializer.h"
#include "preset/Preset.h"
#include "preset/PresetLibrary.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

using namespace ohpe;

namespace
{
std::vector<std::string> allIds (const ParamRegistry& reg)
{
    std::vector<std::string> ids;
    for (int i = 0; i < reg.size(); ++i) ids.push_back (reg.descriptor (i).id);
    return ids;
}

void scramble (ParamRegistry& reg, const std::string& prefix, float norm)
{
    for (int i = 0; i < reg.size(); ++i)
        if (reg.descriptor (i).id.compare (0, prefix.size(), prefix) == 0) reg.setNormalised (i, norm);
}

preset::Entry entry (const std::string& name, std::vector<std::string> tags, const std::string& engine, bool factory, int rating = 0,
                     bool fav = false, int64_t modified = 0)
{
    preset::Entry e;
    e.uid = (factory ? "factory:" : "user:") + name;
    e.meta.name = name;
    e.meta.tags = std::move (tags);
    e.meta.engine = engine;
    e.factory = factory;
    e.rating = rating;
    e.favourite = fav;
    e.modified = modified;
    return e;
}
} // namespace

TEST_CASE ("preset: part preset round-trip (save -> JSON -> load into another part)")
{
    Core a;
    auto& reg = a.params();
    reg.set ("p1.engine", (float) EngineType::FM);
    scramble (reg, "p1.", 0.73f);
    reg.set ("p1.engine", (float) EngineType::FM);
    preset::Meta meta;
    meta.name = "Glas Tasten";
    meta.author = "groovex";
    meta.tags = { "Keys", "bright" };
    const auto p = preset::capturePart (reg, 0, meta);
    CHECK (p.meta.engine == "fm");
    CHECK (p.params.count ("engine") == 1);
    CHECK (p.params.count ("p1.engine") == 0); // local IDs
    const auto json = preset::toJson (p);

    preset::Preset back;
    std::string err;
    REQUIRE (preset::fromJson (json, back, &err));
    CHECK (back.meta.name == "Glas Tasten");
    CHECK (back.meta.author == "groovex");
    CHECK (back.meta.tags == std::vector<std::string> { "Keys", "bright" });
    CHECK (back.meta.kind == preset::Kind::Part);
    CHECK (back.meta.version == preset::formatVersion);

    Core b;
    const int ch3 = (int) b.params().get ("p3.midich");
    const auto r = preset::applyPart (back, b.params(), 2);
    REQUIRE (r.ok);
    // since 0.7 every part has the same FX rack: a part preset of part 1 has no unknown IDs in part 3
    int onlyInPart1 = 0;
    for (const auto& kv : back.params)
        if (b.params().indexOf ("p3." + kv.first) < 0) ++onlyInPart1;
    CHECK (onlyInPart1 == 0);
    CHECK (r.unknown == onlyInPart1);
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& id = reg.descriptor (i).id;
        if (id.rfind ("p1.", 0) != 0 || id == "p1.midich" || b.params().indexOf ("p3." + id.substr (3)) < 0) continue;
        if (id.rfind ("p1.motion.", 0) == 0) continue; // p22: the motion block is not part of a part preset
        CAPTURE (id);
        CHECK (b.params().get ("p3." + id.substr (3)) == doctest::Approx (reg.get (i)));
    }
    CHECK ((int) b.params().get ("p3.midich") == ch3); // setup value stays
    // the other parts are untouched
    CHECK (b.params().get ("p1.analog.cutoff") == b.params().descriptor (b.params().indexOf ("p1.analog.cutoff")).defaultValue);
}

TEST_CASE ("preset: sparse part presets reset the rest of the part to defaults")
{
    Core c;
    auto& reg = c.params();
    scramble (reg, "p2.", 0.9f);
    preset::Preset p;
    p.meta.name = "Sparse";
    p.params["engine"] = (float) EngineType::Analog;
    p.params["analog.cutoff"] = 800.0f;
    p.params["does.not.exist"] = 1.0f;
    const auto r = preset::applyPart (p, reg, 1);
    CHECK (r.ok);
    CHECK (r.applied == 2);
    CHECK (r.unknown == 1);
    CHECK (reg.get ("p2.analog.cutoff") == doctest::Approx (800.0f));
    const int i = reg.indexOf ("p2.analog.reso");
    CHECK (reg.get (i) == reg.descriptor (i).defaultValue);
}

TEST_CASE ("preset: version-1 presets (state::savePreset) load as part presets")
{
    Core a;
    a.params().set ("p1.engine", (float) EngineType::Wavetable);
    a.params().set ("p1.wavetable.pos", 0.8f);
    const auto v1 = state::savePreset (a.params(), "p1.", "Old");
    preset::Preset p;
    REQUIRE (preset::fromJson (v1, p));
    CHECK (p.meta.version == 1);
    CHECK (p.meta.kind == preset::Kind::Part);
    CHECK (p.meta.engine == "wavetable");
    CHECK (p.meta.name == "Old");
    Core b;
    REQUIRE (preset::applyPart (p, b.params(), 0).ok);
    CHECK (b.params().get ("p1.wavetable.pos") == doctest::Approx (0.8f));
}

TEST_CASE ("preset: project preset round-trip with patterns; MIDI map, remote and sync stay")
{
    Core a;
    auto& reg = a.params();
    for (int i = 0; i < reg.size(); ++i)
        reg.setNormalised (i, reg.descriptor (i).toNormalised (reg.descriptor (i).defaultValue) > 0.5f ? 0.25f : 0.75f);
    StepSequencer::Step s;
    s.on = true;
    s.note = 52;
    s.locks[0] = { reg.indexOf ("p1.analog.cutoff"), 0.3f };
    a.stepSequencer().setStep (5, s);
    preset::Meta meta;
    meta.name = "Song";
    meta.tags = { "Arp" };
    const auto json = preset::toJson (preset::captureProject (a, meta, { { "samples", "{\"x\":1}" } }));

    Core b;
    MidiMapping m;
    m.paramId = "tempo";
    m.number = 20;
    b.midiMap().add (m);
    b.params().set ("sync.src", 1.0f);
    b.params().set ("ui.part", 3.0f);
    preset::Preset p;
    REQUIRE (preset::fromJson (json, p));
    CHECK (p.meta.kind == preset::Kind::Project);
    REQUIRE (preset::applyProject (p, b).ok);
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& id = reg.descriptor (i).id;
        if (preset::isExcludedFromProject (id)) continue;
        CAPTURE (id);
        CHECK (b.params().get (i) == doctest::Approx (reg.get (i)));
    }
    CHECK (b.params().get ("sync.src") == 1.0f);
    CHECK (b.params().get ("ui.part") == 3.0f);
    CHECK (b.midiMap().mappings().size() == 1);
    const auto got = b.stepSequencer().getStep (5);
    CHECK (got.on);
    CHECK (got.note == 52);
    CHECK (got.locks[0].paramIndex == reg.indexOf ("p1.analog.cutoff"));
    CHECK (got.locks[0].normValue == doctest::Approx (0.3f));
    CHECK (b.stateSection ("samples").find ("\"x\"") != std::string::npos);
    // a part preset is refused by applyProject and vice versa
    preset::Preset part;
    CHECK_FALSE (preset::applyProject (part, b).ok);
    CHECK_FALSE (preset::applyPart (p, b.params(), 0).ok);
}

TEST_CASE ("preset: loading presets never alters the parameter IDs")
{
    Core c;
    const auto before = allIds (c.params());
    preset::Preset p;
    p.params["engine"] = 3.0f;
    p.params["brand.new.param"] = 0.5f;
    preset::applyPart (p, c.params(), 0);
    preset::Preset q;
    q.meta.kind = preset::Kind::Project;
    q.params["p9.unknown"] = 1.0f;
    preset::applyProject (q, c);
    CHECK (allIds (c.params()) == before);
    CHECK (c.params().indexOf ("brand.new.param") < 0);
    // remote controls exist and are excluded from project presets
    for (const char* id : { "lib.open", "lib.prev", "lib.next", "ui.part", "tape.edit.track", "tape.edit.lift", "tape.edit.cut",
                            "tape.edit.drop", "tape.edit.splice", "tape.edit.undo", "tape.edit.redo", "tape.edit.rtz" })
    {
        CAPTURE (id);
        CHECK (c.params().indexOf (id) >= 0);
        CHECK (preset::isExcludedFromProject (id));
    }
}

TEST_CASE ("preset: malformed documents are rejected")
{
    preset::Preset p;
    std::string err;
    CHECK_FALSE (preset::fromJson ("not json", p, &err));
    CHECK_FALSE (preset::fromJson ("{\"format\":\"ohpe-state\"}", p, &err));
    CHECK_FALSE (preset::fromJson ("[1,2]", p, &err));
    CHECK (preset::fromJson ("{\"format\":\"ohpe-preset\",\"tags\":[1,\"Bass\",\"\"],\"params\":{\"a\":\"x\",\"b\":2}}", p, &err));
    CHECK (p.meta.tags == std::vector<std::string> { "Bass" });
    CHECK (p.params.size() == 1);
}

TEST_CASE ("preset library: tag, engine, bank, favourite, rating and text filters")
{
    std::vector<preset::Entry> e;
    e.push_back (entry ("Deep Bass", { "Bass" }, "analog", true, 3));
    e.push_back (entry ("Glass Keys", { "Keys", "Plucks" }, "fm", true, 5, true));
    e.push_back (entry ("Slow Pad", { "Pads", "Textures" }, "wavetable", false, 0, false, 200));
    e.push_back (entry ("Choir", { "Vocal", "Pads" }, "vocal", false, 4, true, 100));
    e[3].meta.author = "Lieke";

    auto names = [&] (const preset::Filter& f) {
        std::vector<std::string> r;
        for (int i : preset::filterAndSort (e, f)) r.push_back (e[(size_t) i].meta.name);
        return r;
    };
    preset::Filter f;
    CHECK (names (f) == std::vector<std::string> { "Choir", "Deep Bass", "Glass Keys", "Slow Pad" });
    f.tags = { "Pads" };
    CHECK (names (f) == std::vector<std::string> { "Choir", "Slow Pad" });
    f.tags = { "Bass", "Keys" }; // any of the selected tags
    CHECK (names (f) == std::vector<std::string> { "Deep Bass", "Glass Keys" });
    f.tags.clear();
    f.engine = "fm";
    CHECK (names (f) == std::vector<std::string> { "Glass Keys" });
    f.engine.clear();
    f.bank = preset::Bank::User;
    CHECK (names (f) == std::vector<std::string> { "Choir", "Slow Pad" });
    f.bank = preset::Bank::Factory;
    CHECK (names (f) == std::vector<std::string> { "Deep Bass", "Glass Keys" });
    f.bank = preset::Bank::All;
    f.favouritesOnly = true;
    CHECK (names (f) == std::vector<std::string> { "Choir", "Glass Keys" });
    f.favouritesOnly = false;
    f.minRating = 4;
    CHECK (names (f) == std::vector<std::string> { "Choir", "Glass Keys" });
    f.minRating = 0;
    f.text = "PAD slow";
    CHECK (names (f) == std::vector<std::string> { "Slow Pad" });
    f.text = "lieke";
    CHECK (names (f) == std::vector<std::string> { "Choir" });
    f.text = "flächen"; // German category name
    CHECK (names (f) == std::vector<std::string> { "Choir", "Slow Pad" });
    f.text = "FLÄCHEN";
    CHECK (names (f).size() == 2);
    f.text = "zupfen"; // German engine name not present -> nothing
    CHECK (names (f).empty());
    f.text.clear();
    f.sort = preset::Sort::Rating;
    CHECK (names (f) == std::vector<std::string> { "Glass Keys", "Choir", "Deep Bass", "Slow Pad" });
    f.sort = preset::Sort::Newest;
    CHECK (names (f).front() == "Slow Pad");
    f.sort = preset::Sort::Engine;
    CHECK (names (f) == std::vector<std::string> { "Deep Bass", "Glass Keys", "Choir", "Slow Pad" });
    CHECK (preset::usedTags (e) == std::vector<std::string> { "Bass", "Keys", "Pads", "Plucks", "Textures", "Vocal" });
}

TEST_CASE ("preset library: a freshly saved user part preset passes the User bank filter (no tags, any engine)")
{
    // regression: a sound saved under a new name in the library did not show under "User"
    Core core;
    core.prepare (48000.0, 256);
    preset::Meta meta;
    meta.name = "My Own Sound";
    const auto p = preset::capturePart (core.params(), 0, meta);
    preset::Preset back;
    REQUIRE (preset::fromJson (preset::toJson (p), back));
    preset::Entry user;
    user.uid = "user:Parts/My Own Sound.ohpepreset";
    user.meta = back.meta;
    std::vector<preset::Entry> e { entry ("Deep Bass", { "Bass" }, "analog", true), user };

    preset::Filter f;
    f.bank = preset::Bank::User;
    REQUIRE (preset::filterAndSort (e, f) == std::vector<int> { 1 });
    f.kind = 0; // parts
    CHECK (preset::filterAndSort (e, f) == std::vector<int> { 1 });
    f.kind = 1; // projects
    CHECK (preset::filterAndSort (e, f).empty());
    f.kind = -1;
    f.text = "own sound";
    CHECK (preset::filterAndSort (e, f) == std::vector<int> { 1 });
    // only a category filter hides an untagged preset (the browser clears it after saving)
    f.text.clear();
    f.tags = { "Bass" };
    CHECK (preset::filterAndSort (e, f).empty());
}

TEST_CASE ("preset library: user data (favourites, ratings) round-trip")
{
    preset::UserData d;
    d.author = "groovex";
    d.set ("factory:a", true, 3);
    d.set ("user:b.ohpepreset", false, 9); // clamped
    d.set ("user:c", false, 0);            // nothing to keep
    preset::UserData back;
    REQUIRE (back.fromJson (d.toJson()));
    CHECK (back.author == "groovex");
    CHECK (back.items.size() == 2);
    std::vector<preset::Entry> e { entry ("a", {}, "", true), entry ("b.ohpepreset", {}, "", false) };
    back.applyTo (e);
    CHECK (e[0].favourite);
    CHECK (e[0].rating == 3);
    CHECK_FALSE (e[1].favourite);
    CHECK (e[1].rating == 5);
    CHECK_FALSE (back.fromJson ("{}"));
}

TEST_CASE ("preset: safe file names")
{
    CHECK (preset::safeFileName ("Bass/Lead: 1?") == "Bass_Lead_ 1_");
    CHECK (preset::safeFileName ("  ..  ") == "Preset");
    CHECK (preset::safeFileName ("Flächen") == "Flächen");
    CHECK (preset::safeFileName (std::string (100, 'a')).size() == 80);
}

// ------------------------------------------------------------------ 0.7: arp blocks in part presets
namespace
{
preset::Preset arpPreset()
{
    preset::Preset p;
    p.meta.name = "Test Arp";
    p.meta.tags = { "Arp" };
    p.params["engine"] = (float) EngineType::Analog;
    p.params["analog.cutoff"] = 900.0f;
    p.arp = std::map<std::string, float> { { "mode", 2.0f }, { "oct", 2.0f }, { "gate", 0.45f }, { "rate", 2.0f }, { "latch", 1.0f } };
    return p;
}
} // namespace

TEST_CASE ("preset arp: a part preset with an arp block switches the arp on for the part it loads into")
{
    Core c;
    auto& reg = c.params();
    REQUIRE (reg.get ("seq.arp.on") == 0.0f);
    REQUIRE ((int) reg.get ("seq.arp.part") == 3);
    reg.set ("seq.arp.mode", 4.0f);
    const auto r = preset::applyPart (arpPreset(), reg, 0);
    REQUIRE (r.ok);
    CHECK (r.arpApplied);
    CHECK (reg.get ("seq.arp.on") == 1.0f);
    CHECK ((int) reg.get ("seq.arp.part") == 1);
    CHECK ((int) reg.get ("seq.arp.mode") == 2);
    CHECK ((int) reg.get ("seq.arp.oct") == 2);
    CHECK (reg.get ("seq.arp.gate") == doctest::Approx (0.45f));
    CHECK ((int) reg.get ("seq.arp.rate") == 2);
    CHECK (reg.get ("seq.arp.latch") == 0.0f); // p28: a preset's latch is not applied - the arp stops on release
    CHECK (reg.get ("p1.analog.cutoff") == doctest::Approx (900.0f));
    reg.set ("seq.arp.latch", 1.0f); // the player's own latch stays when a preset loads
    preset::applyPart (arpPreset(), reg, 0);
    CHECK (reg.get ("seq.arp.latch") == 1.0f);
    reg.set ("seq.arp.latch", 0.0f);
    CHECK_FALSE (preset::capturePart (reg, 0, {}).arp->count ("latch")); // not captured either

    // into part 4: the arp follows; unlisted keys return to their defaults
    auto p = arpPreset();
    p.arp->erase ("oct");
    preset::applyPart (p, reg, 3);
    CHECK ((int) reg.get ("seq.arp.part") == 4);
    CHECK ((int) reg.get ("seq.arp.oct") == 1);

    // the core really arpeggiates part 4. p28: by default (no latch) the arpeggio stops with the key;
    // with the player's latch on a short key press keeps playing; the sound without its arp is silent
    // long after the key was released
    auto lateSound = [] (Core& core) {
        core.prepare (48000.0, 256);
        std::vector<float> l (256), rr (256);
        int late = 0;
        for (int b = 0; b < 240; ++b)
        {
            OutputBuffers out;
            out.left[0] = l.data();
            out.right[0] = rr.data();
            Transport tr;
            MidiEvent ev = b == 0 ? MidiEvent::noteOn (4, 60, 100) : MidiEvent::noteOff (4, 60);
            core.process (out, 256, b <= 1 ? &ev : nullptr, b <= 1 ? 1 : 0, tr);
            float peak = 0.0f;
            for (float s : l) peak = std::max (peak, std::abs (s));
            if (b >= 160 && peak > 1.0e-4f) ++late;
        }
        return late;
    };
    {
        Core held;
        preset::applyPart (p, held.params(), 3);
        CHECK (held.params().get ("seq.arp.latch") == 0.0f);
        CHECK (lateSound (held) == 0);
    }
    reg.set ("seq.arp.latch", 1.0f);
    CHECK (lateSound (c) > 0);
    Core silent;
    preset::PartOptions soundOnly;
    soundOnly.arp = false;
    preset::applyPart (p, silent.params(), 3, soundOnly);
    CHECK (lateSound (silent) == 0);
}

TEST_CASE ("preset arp: part presets without arp block leave the arpeggiator untouched")
{
    Core c;
    auto& reg = c.params();
    reg.set ("seq.arp.on", 1.0f);
    reg.set ("seq.arp.part", 2.0f);
    reg.set ("seq.arp.mode", 3.0f);
    reg.set ("seq.arp.gate", 0.8f);
    const auto before = preset::arpSnapshot (reg);
    preset::Preset plain;
    plain.params["engine"] = (float) EngineType::FM;
    for (int part = 0; part < Core::numParts; ++part)
    {
        const auto r = preset::applyPart (plain, reg, part);
        CHECK (r.ok);
        CHECK_FALSE (r.arpApplied);
        CHECK_FALSE (r.arpReleased);
        CHECK (preset::arpSnapshot (reg) == before);
    }
    // releaseArp only acts on the part the arp plays
    preset::PartOptions release;
    release.releaseArp = true;
    CHECK_FALSE (preset::applyPart (plain, reg, 0, release).arpReleased);
    CHECK (reg.get ("seq.arp.on") == 1.0f);
    const auto r = preset::applyPart (plain, reg, 1, release);
    CHECK (r.arpReleased);
    CHECK (reg.get ("seq.arp.on") == 0.0f);
    CHECK ((int) reg.get ("seq.arp.mode") == 3); // only switched off, the settings stay

    // arp = false (level measurements): the sound loads, the arp block is ignored
    Core d;
    preset::PartOptions soundOnly;
    soundOnly.arp = false;
    CHECK_FALSE (preset::applyPart (arpPreset(), d.params(), 0, soundOnly).arpApplied);
    CHECK (d.params().get ("seq.arp.on") == 0.0f);
}

TEST_CASE ("preset arp: the arp block round-trips through JSON and through capturePart")
{
    const auto json = preset::toJson (arpPreset());
    CHECK (json.find ("\"arp\"") != std::string::npos);
    preset::Preset back;
    REQUIRE (preset::fromJson (json, back));
    REQUIRE (back.arp.has_value());
    CHECK (*back.arp == *arpPreset().arp);

    // capture: the arp belongs to the part it plays
    Core c;
    preset::applyPart (back, c.params(), 1);
    const auto cap2 = preset::capturePart (c.params(), 1, {});
    REQUIRE (cap2.arp.has_value());
    CHECK (cap2.arp->at ("on") == 1.0f);
    CHECK (cap2.arp->at ("mode") == 2.0f);
    CHECK (cap2.arp->count ("part") == 0);
    CHECK_FALSE (preset::capturePart (c.params(), 0, {}).arp.has_value());
    c.params().set ("seq.arp.on", 0.0f);
    CHECK_FALSE (preset::capturePart (c.params(), 1, {}).arp.has_value());

    // without arp block nothing is written; booleans and unknown keys are accepted / ignored
    preset::Preset plain;
    plain.params["engine"] = 0.0f;
    CHECK (preset::toJson (plain).find ("\"arp\"") == std::string::npos);
    preset::Preset q;
    REQUIRE (preset::fromJson (R"({"format":"ohpe-preset","version":2,"kind":"part","params":{"engine":2},
                                   "arp":{"on":true,"latch":false,"part":4,"rate":1,"bogus":3}})", q));
    REQUIRE (q.arp.has_value());
    CHECK (q.arp->at ("on") == 1.0f);
    CHECK (q.arp->at ("latch") == 0.0f);
    CHECK (q.arp->count ("part") == 0);
    CHECK (q.arp->count ("bogus") == 0);
    // project presets carry the arp in their parameters, an "arp" block there is ignored
    preset::Preset proj;
    REQUIRE (preset::fromJson (R"({"format":"ohpe-preset","version":2,"kind":"project","params":{},"arp":{"on":1}})", proj));
    CHECK_FALSE (proj.arp.has_value());
}
