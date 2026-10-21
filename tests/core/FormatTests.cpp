// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p62 (1.0 format freeze): every persisted format has a version, older shapes load and migrate, newer shapes load
// what this build knows. The fixtures in tests/formats/fixtures are frozen (scripts/ci/check-formats.py); see
// docs/FORMATS.md / docs/de/FORMATE.md.
#include "OhpeCore.h"
#include "midi/MidiMap.h"
#include "model/FormatVersions.h"
#include "model/StateSerializer.h"
#include "preset/Preset.h"
#include "settings/UserSettings.h"
#include "tape/TapeFile.h"
#include "tape/TapeMachine.h"
#include "update/PackFormat.h"
#include "update/UpdateSettings.h"
#include "wavetable/TableFile.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace ohpe;
using nlohmann::json;

namespace
{
std::string fixturePath (const std::string& rel) { return std::string (OHPE_SOURCE_DIR) + "/tests/formats/fixtures/" + rel; }

std::vector<uint8_t> readBytes (const std::string& rel)
{
    std::ifstream in (fixturePath (rel), std::ios::binary);
    REQUIRE_MESSAGE (in.good(), "missing fixture " << rel);
    return std::vector<uint8_t> ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

std::string readText (const std::string& rel)
{
    const auto b = readBytes (rel);
    return std::string (b.begin(), b.end());
}

float get (const Core& c, const std::string& id) { return c.params().get (id); }

float defaultOf (const Core& c, const std::string& id)
{
    const int i = c.params().indexOf (id);
    REQUIRE (i >= 0);
    return c.params().descriptor (i).defaultValue;
}

struct TapeRig
{
    ParamRegistry reg;
    TapeMachine tape;
    explicit TapeRig (double rate = 48000.0)
    {
        TapeMachine::registerParams (reg);
        reg.freeze();
        tape.bind (reg);
        tape.setLengthSeconds (30.0);
        tape.prepare (rate, 256);
    }
};

/** The blocks of a tape audio blob ("OHPETAPE" ...): (track, page) -> L + R bytes. */
std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> blobBlocks (const uint8_t* p, size_t size)
{
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> blocks;
    constexpr size_t head = 8 + 4 + 4 + 4 + 8 + 8, blockBytes = 2 * 4 * 4096;
    REQUIRE (size >= head + 4);
    REQUIRE (std::memcmp (p, "OHPETAPE", 8) == 0);
    size_t pos = head;
    for (;;)
    {
        REQUIRE (pos + 4 <= size);
        uint32_t t = 0, page = 0;
        std::memcpy (&t, p + pos, 4);
        pos += 4;
        if (t == 0xffffffffu) break;
        REQUIRE (pos + 4 + blockBytes <= size);
        std::memcpy (&page, p + pos, 4);
        pos += 4;
        blocks[{ t, page }] = std::vector<uint8_t> (p + pos, p + pos + blockBytes);
        pos += blockBytes;
    }
    return blocks;
}

/** The tape audio blob inside an .ohpetape file. */
std::vector<uint8_t> tapeFileBlob (const std::vector<uint8_t>& file)
{
    REQUIRE (file.size() > 12);
    uint32_t len = 0;
    std::memcpy (&len, file.data() + 8, 4);
    return std::vector<uint8_t> (file.begin() + 12 + len, file.end());
}
} // namespace

// ------------------------------------------------------------------------------------------------ versions
TEST_CASE ("formats: the version constants match tests/formats/FORMATS.lock (no silent change)")
{
    std::ifstream in (std::string (OHPE_SOURCE_DIR) + "/tests/formats/FORMATS.lock");
    REQUIRE (in.good());
    std::vector<std::pair<std::string, int>> locked;
    std::string line;
    while (std::getline (in, line))
    {
        if (! line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls (line);
        std::string kind, key;
        int v = -1;
        if (ls >> kind >> key >> v && kind == "version") locked.emplace_back (key, v);
    }
    REQUIRE (locked.size() == formats::all.size());
    for (size_t i = 0; i < locked.size(); ++i)
    {
        CAPTURE (locked[i].first);
        CHECK (locked[i].first == formats::all[i].key);
        CHECK (locked[i].second == formats::all[i].version);
        CHECK (formats::all[i].version >= 1);
    }
    // the modules use the central constants
    CHECK (state::currentVersion == formats::state);
    CHECK (preset::formatVersion == formats::preset);
    CHECK (mod::schemaVersion == formats::modSchema);
    CHECK (update::packFormat == formats::pack);
}

TEST_CASE ("formats: every writer stamps its current version")
{
    Core c;
    const auto st = json::parse (c.saveState ("{}"));
    CHECK (st["format"] == "ohpe-state");
    CHECK (st["version"] == formats::state);
    // the shape of a state: params (every ID) + the module sections, each with its own version
    CHECK (st["params"].size() == (size_t) c.params().size());
    for (const char* s : { "midimap", "sequencer", "orbit", "tape", "ui" }) CHECK_MESSAGE (st["sections"].contains (s), s);
    CHECK (st["sections"]["midimap"]["version"] == formats::midiMap);
    CHECK (st["sections"]["sequencer"]["version"] == formats::sequencer);
    CHECK (st["sections"]["orbit"]["version"] == formats::orbit);

    preset::Meta meta;
    meta.name = "V";
    CHECK (json::parse (preset::toJson (preset::capturePart (c.params(), 0, meta)))["version"] == formats::preset);
    CHECK (json::parse (preset::toJson (preset::captureProject (c, meta)))["version"] == formats::preset);
    CHECK (json::parse (c.midiMap().toJson())["version"] == formats::midiMap);
    CHECK (json::parse (settings::UserSettings {}.toJson())["version"] == formats::settings);
    CHECK (json::parse (update::Settings {}.toJson())["version"] == formats::updateSettings);
    update::PackInfo pi;
    pi.id = "x";
    pi.version = "1.0.0";
    CHECK (json::parse (update::packInfoToJson (pi))["format"] == formats::pack);

    TapeRig rig;
    const auto file = tapefile::save (rig.tape, {});
    uint32_t len = 0;
    std::memcpy (&len, file.data() + 8, 4);
    CHECK (json::parse (std::string (file.begin() + 12, file.begin() + 12 + len))["version"] == formats::tapeFile);
    const auto blob = tapeFileBlob (file);
    uint32_t audioVersion = 0;
    std::memcpy (&audioVersion, blob.data() + 8, 4);
    CHECK (audioVersion == (uint32_t) formats::tapeAudio);

    auto table = usertable::decode (readBytes ("wavetable/table-v1.ohpewt"));
    REQUIRE (table != nullptr);
    const auto wt = usertable::encode (*table);
    CHECK ((int) (wt[6] | (wt[7] << 8)) == formats::wavetable);
}

// ------------------------------------------------------------------------------------------------ state / session
TEST_CASE ("formats: state v1 (current shape) loads, keeps In Key off with layout Sequential, round-trips")
{
    const auto text = readText ("state/state-v1.json");
    Core a;
    std::string ui;
    REQUIRE (a.loadState (text, &ui));
    CHECK (get (a, "tempo") == doctest::Approx (128.0f));
    CHECK (get (a, "p1.engine") == 0.0f);
    CHECK (get (a, "p1.vol") == doctest::Approx (0.625f));
    CHECK (get (a, "p1.tune") == doctest::Approx (7.0f));
    CHECK (get (a, "p1.amp.a") == doctest::Approx (0.25f));
    CHECK (get (a, "p1.amp.r") == doctest::Approx (1.5f));
    CHECK (get (a, "p2.vol") == doctest::Approx (0.5f));
    // written by p59 or later (harmony.kb.inkey present): 3 is Sequential, nothing migrates
    CHECK (get (a, "harmony.kb.layout") == 3.0f);
    CHECK (get (a, "harmony.kb.inkey") == 0.0f);
    CHECK (get (a, "harmony.kb.fixed") == 1.0f);
    CHECK (get (a, "p1.fine") == defaultOf (a, "p1.fine")); // not in the document: default
    REQUIRE (a.midiMap().mappings().size() == 1);
    CHECK (a.midiMap().mappings()[0].paramId == "p1.vol");
    CHECK (a.midiMap().mappings()[0].number == 74);
    const auto uids = state::sessionPresetUids (ui);
    CHECK (uids[0] == "factory:Glass Keys.ohpepreset");
    CHECK (uids[1].empty());
    CHECK (uids[2] == "user:Deep Bass.ohpepreset");
    CHECK (uids[3].empty());

    // round trip: save, load into a fresh instrument - every parameter, the map and the ui block are the same
    const auto saved = a.saveState (ui);
    Core b;
    std::string ui2;
    REQUIRE (b.loadState (saved, &ui2));
    for (int i = 0; i < a.params().size(); ++i)
    {
        CAPTURE (a.params().descriptor (i).id);
        CHECK (b.params().get (i) == a.params().get (i));
    }
    CHECK (b.midiMap().mappings().size() == 1);
    CHECK (state::sessionPresetUids (ui2) == uids);
    CHECK (json::parse (saved)["version"] == formats::state);
}

TEST_CASE ("formats: state from 0.6 - old In Key grid layout and the single 'preset' ui block migrate")
{
    Core a;
    std::string ui;
    REQUIRE (a.loadState (readText ("state/state-v1-0.6-in-key.json"), &ui));
    CHECK (get (a, "tempo") == doctest::Approx (96.0f));
    CHECK (get (a, "p1.engine") == 2.0f);
    CHECK (get (a, "p1.analog.cutoff") == doctest::Approx (800.0f));
    // p59: layout 3 without harmony.kb.inkey = the old In Key layout -> Fourths + In Key
    CHECK (get (a, "harmony.kb.layout") == 0.0f);
    CHECK (get (a, "harmony.kb.inkey") == 1.0f);
    // p50: only "preset" (the edit part's) -> every part names it
    for (const auto& u : state::sessionPresetUids (ui)) CHECK (u == "factory:Glass Keys.ohpepreset");
    // saved again, it is the current shape: the migration does not run twice
    const auto saved = json::parse (a.saveState (ui));
    CHECK (saved["params"]["harmony.kb.inkey"] == 1.0f);
    CHECK (saved["params"]["harmony.kb.layout"] == 0.0f);
    Core b;
    REQUIRE (b.loadState (saved.dump()));
    CHECK (get (b, "harmony.kb.layout") == 0.0f);
    CHECK (get (b, "harmony.kb.inkey") == 1.0f);
}

TEST_CASE ("formats: state from 0.1 (params only, no sections) loads with defaults for everything else")
{
    Core a;
    a.params().set ("p2.vol", 0.1f);
    MidiMapping m;
    m.paramId = "tempo";
    a.midiMap().add (m);
    std::string ui = "x";
    REQUIRE (a.loadState (readText ("state/state-v1-0.1-minimal.json"), &ui));
    CHECK (get (a, "tempo") == doctest::Approx (140.0f));
    CHECK (get (a, "p1.engine") == 1.0f);
    CHECK (get (a, "p1.wavetable.pos") == doctest::Approx (0.75f));
    CHECK (get (a, "p2.vol") == defaultOf (a, "p2.vol"));
    CHECK (a.midiMap().mappings().empty());
    CHECK (ui.empty());
    for (const auto& u : state::sessionPresetUids (ui)) CHECK (u.empty());
}

TEST_CASE ("formats: a state from a newer build loads what this build knows and ignores the rest")
{
    Core a;
    REQUIRE (a.loadState (readText ("state/state-v7-future.json")));
    CHECK (get (a, "tempo") == doctest::Approx (100.0f));
    CHECK (get (a, "p1.vol") == doctest::Approx (0.375f));
    CHECK (a.params().indexOf ("zz.future.param") < 0);
    const auto r = state::loadState (readText ("state/state-v7-future.json"), a.params());
    CHECK (r.ok);
    CHECK (r.unknown == 1);
    // an unknown section travels on unchanged (written back on the next save)
    CHECK (json::parse (a.saveState ({}))["sections"].contains ("futureSection"));
    // not a state: refused, nothing changes
    CHECK_FALSE (a.loadState (readText ("preset/part-v2.ohpepreset")));
    CHECK (get (a, "tempo") == doctest::Approx (100.0f));
}

TEST_CASE ("formats: the plug-in state container (state JSON, NUL, tape audio) restores the tape")
{
    const auto bytes = readBytes ("state/plugin-state-v1-with-tape.bin");
    const auto nul = std::find (bytes.begin(), bytes.end(), (uint8_t) 0);
    REQUIRE (nul != bytes.end());
    Core a;
    a.prepare (48000.0, 256);
    REQUIRE (a.loadState (std::string (bytes.begin(), nul)));
    CHECK (get (a, "tempo") == doctest::Approx (125.0f));
    CHECK (a.tape().lengthSeconds() == doctest::Approx (30.0));
    const size_t off = (size_t) (nul - bytes.begin()) + 1;
    REQUIRE (a.tape().loadAudio (bytes.data() + off, bytes.size() - off, false));
    CHECK (a.tape().contentEndFrames (0) > 2 * 4096);
    CHECK (a.tape().contentEndFrames (0) <= 3 * 4096);
}

// ------------------------------------------------------------------------------------------------ presets
TEST_CASE ("formats: part preset v2 (current) loads into any part and round-trips")
{
    preset::Preset p;
    REQUIRE (preset::fromJson (readText ("preset/part-v2.ohpepreset"), p));
    CHECK (p.meta.version == 2);
    CHECK (p.meta.kind == preset::Kind::Part);
    CHECK (p.meta.name == "Format Keys");
    CHECK (p.meta.engine == "fm");
    CHECK (p.meta.modSchema == 1);
    CHECK (p.meta.tags == std::vector<std::string> { "Keys", "bright" });
    REQUIRE (p.arp.has_value());
    Core c;
    const auto r = preset::applyPart (p, c.params(), 2);
    REQUIRE (r.ok);
    CHECK (r.unknown == 0);
    CHECK (r.arpApplied);
    CHECK (get (c, "p3.engine") == 0.0f);
    CHECK (get (c, "p3.vol") == doctest::Approx (0.625f));
    CHECK (get (c, "p3.tune") == doctest::Approx (-5.0f));
    CHECK (get (c, "p3.mod.s1.src") == 1.0f);
    CHECK (get (c, "p3.mod.s1.dst") == 1.0f);
    CHECK (get (c, "p3.mod.s1.amt") == doctest::Approx (0.5f));
    CHECK (get (c, "seq.arp.on") == 1.0f);
    CHECK (get (c, "seq.arp.part") == 3.0f);
    CHECK (get (c, "seq.arp.mode") == 2.0f);

    preset::Preset back;
    REQUIRE (preset::fromJson (preset::toJson (p), back));
    CHECK (back.params == p.params);
    CHECK (back.meta.name == p.meta.name);
    CHECK (back.meta.tags == p.meta.tags);
    CHECK (back.meta.engine == p.meta.engine);
    CHECK (back.arp == p.arp);
}

TEST_CASE ("formats: part preset v1 (0.1, 'prefix', no kind / engine) loads as a part and is saved as the current version")
{
    preset::Preset p;
    REQUIRE (preset::fromJson (readText ("preset/part-v1-0.1.ohpepreset"), p));
    CHECK (p.meta.version == 1);
    CHECK (p.meta.kind == preset::Kind::Part);
    CHECK (p.meta.engine == "analog"); // derived from the engine parameter
    CHECK (p.meta.name == "Old Bass");
    Core c;
    REQUIRE (preset::applyPart (p, c.params(), 1).ok);
    CHECK (get (c, "p2.engine") == 2.0f);
    CHECK (get (c, "p2.analog.cutoff") == doctest::Approx (800.0f));
    CHECK (get (c, "p2.amp.r") == doctest::Approx (0.75f));
    CHECK (get (c, "p1.vol") == defaultOf (c, "p1.vol")); // other parts untouched
    const auto j = json::parse (preset::toJson (p));
    CHECK (j["version"] == formats::preset);
    CHECK (j["kind"] == "part");
    CHECK (j["engine"] == "analog");
    CHECK_FALSE (j.contains ("prefix"));
}

TEST_CASE ("formats: part preset without modSchema (0.6) migrates its matrix routings")
{
    preset::Preset p;
    REQUIRE (preset::fromJson (readText ("preset/part-v2-no-modschema.ohpepreset"), p));
    CHECK (p.meta.modSchema == 0);
    Core c;
    REQUIRE (preset::applyPart (p, c.params(), 0).ok);
    CHECK (get (c, "p1.mod.s1.src") == 1.0f);
    CHECK (get (c, "p1.mod.s1.amt") == doctest::Approx (0.25f));
    CHECK (json::parse (preset::toJson (p))["modSchema"] == formats::modSchema);
}

TEST_CASE ("formats: project presets - current shape and the old In Key grid layout")
{
    {
        preset::Preset p;
        REQUIRE (preset::fromJson (readText ("preset/project-v2.ohpepreset"), p));
        CHECK (p.meta.kind == preset::Kind::Project);
        Core c;
        REQUIRE (preset::applyProject (p, c).ok);
        CHECK (get (c, "tempo") == doctest::Approx (110.0f));
        CHECK (get (c, "harmony.kb.layout") == 3.0f); // Sequential, In Key off
        CHECK (get (c, "harmony.kb.inkey") == 0.0f);
        preset::Preset back;
        REQUIRE (preset::fromJson (preset::toJson (p), back));
        CHECK (back.params == p.params);
    }
    {
        preset::Preset p;
        REQUIRE (preset::fromJson (readText ("preset/project-v2-0.6-in-key.ohpepreset"), p));
        CHECK (p.params.at ("harmony.kb.layout") == 0.0f);
        CHECK (p.params.at ("harmony.kb.inkey") == 1.0f);
        Core c;
        REQUIRE (preset::applyProject (p, c).ok);
        CHECK (get (c, "tempo") == doctest::Approx (90.0f));
        CHECK (get (c, "harmony.kb.layout") == 0.0f);
        CHECK (get (c, "harmony.kb.inkey") == 1.0f);
    }
}

TEST_CASE ("formats: a part preset from a newer build loads its known values")
{
    preset::Preset p;
    REQUIRE (preset::fromJson (readText ("preset/part-v3-future.ohpepreset"), p));
    CHECK (p.meta.version == 3);
    CHECK (p.meta.name == "From The Future");
    Core c;
    const auto r = preset::applyPart (p, c.params(), 0);
    REQUIRE (r.ok);
    CHECK (r.unknown == 1);
    CHECK (get (c, "p1.vol") == doctest::Approx (0.375f));
}

// ------------------------------------------------------------------------------------------------ .ohpetape
TEST_CASE ("formats: .ohpetape v1 loads bit-exact and saves the same audio")
{
    const auto file = readBytes ("tape/tape-v1.ohpetape");
    TapeRig rig;
    tapefile::Settings s;
    REQUIRE (tapefile::load (rig.tape, file.data(), file.size(), s));
    CHECK (s.sampleRate == 48000.0);
    CHECK (s.lengthSeconds == doctest::Approx (10.0));
    CHECK (s.loopIn == doctest::Approx (0.5));
    CHECK (s.loopOut == doctest::Approx (2.0));
    CHECK (s.loop);
    CHECK (s.editTrack == 3);
    CHECK (rig.tape.lengthSeconds() == doctest::Approx (10.0));
    CHECK (rig.tape.contentEndFrames (2) > 4096);
    CHECK (rig.tape.contentEndFrames (2) <= 2 * 4096);
    CHECK (rig.tape.contentEndFrames (0) == 0);
    // round trip: the saved file holds the same blocks, byte for byte
    const auto again = tapefile::save (rig.tape, s);
    const auto want = tapeFileBlob (file), got = tapeFileBlob (again);
    CHECK (blobBlocks (got.data(), got.size()) == blobBlocks (want.data(), want.size()));
    tapefile::Settings s2;
    REQUIRE (tapefile::readSettings (again.data(), again.size(), s2));
    CHECK (s2.loopOut == doctest::Approx (2.0));
    CHECK (s2.editTrack == 3);
}

TEST_CASE ("formats: .ohpetape - the smallest header (44.1 kHz) loads resampled; a newer version is refused")
{
    const auto file = readBytes ("tape/tape-v1-minimal-44k.ohpetape");
    TapeRig rig;
    tapefile::Settings s;
    REQUIRE (tapefile::load (rig.tape, file.data(), file.size(), s));
    CHECK (s.sampleRate == 44100.0);
    CHECK_FALSE (s.loop);
    CHECK (s.editTrack == 1);
    CHECK (s.lengthSeconds == doctest::Approx (20.0));
    const double end = (double) rig.tape.contentEndFrames (0);
    CHECK (end == doctest::Approx (4096.0 * 48000.0 / 44100.0).epsilon (0.01));

    const auto future = readBytes ("tape/tape-v2-future.ohpetape");
    tapefile::Settings f;
    CHECK_FALSE (tapefile::readSettings (future.data(), future.size(), f));
    CHECK_FALSE (tapefile::load (rig.tape, future.data(), future.size(), f));
    CHECK ((double) rig.tape.contentEndFrames (0) == end); // unchanged
}

// ------------------------------------------------------------------------------------------------ .ohpewt
TEST_CASE ("formats: .ohpewt v1 - wavetable frames and formant tracks load and round-trip")
{
    const auto bytes = readBytes ("wavetable/table-v1.ohpewt");
    CHECK (usertable::isTableFile (bytes.data(), bytes.size()));
    std::string err;
    auto t = usertable::decode (bytes, &err);
    REQUIRE_MESSAGE (t != nullptr, err);
    CHECK (t->name() == "Format Table");
    CHECK (t->numFrames() == 2);
    CHECK (t->formants().empty());
    // frame 0 is the sine it was written with (level 0, normalised)
    double dot = 0.0, n1 = 0.0, n2 = 0.0;
    for (int i = 0; i < 2048; ++i)
    {
        const double ref = std::sin (2.0 * 3.14159265358979323846 * i / 2048.0), v = t->frame (0, 0)[i];
        dot += ref * v;
        n1 += ref * ref;
        n2 += v * v;
    }
    CHECK (dot / std::sqrt (n1 * n2) > 0.999);
    // encode -> decode keeps name, frames and every level-0 sample
    const auto e1 = usertable::encode (*t);
    CHECK ((int) (e1[6] | (e1[7] << 8)) == formats::wavetable);
    auto t2 = usertable::decode (e1);
    REQUIRE (t2 != nullptr);
    CHECK (t2->name() == t->name());
    REQUIRE (t2->numFrames() == t->numFrames());
    float maxDiff = 0.0f;
    for (int f = 0; f < t->numFrames(); ++f)
        for (int i = 0; i < 2048; ++i) maxDiff = std::max (maxDiff, std::abs (t2->frame (f, 0)[i] - t->frame (f, 0)[i]));
    CHECK (maxDiff < 1.0e-4f);

    auto v = usertable::decode (readBytes ("wavetable/table-v1-formants.ohpewt"), &err);
    REQUIRE_MESSAGE (v != nullptr, err);
    CHECK (v->name() == "Format Vowels");
    CHECK (v->numFrames() == 0);
    REQUIRE (v->formants().size() == 2);
    CHECK (v->formants()[0].freq[0] == doctest::Approx (800.0f));
    CHECK (v->formants()[1].freq[1] == doctest::Approx (750.0f));

    // a damaged (truncated) file is refused
    auto cut = bytes;
    cut.resize (cut.size() - 100);
    CHECK (usertable::decode (cut) == nullptr);
}

// ------------------------------------------------------------------------------------------------ .ohpepack
TEST_CASE ("formats: .ohpepack pack.json - format 1, a minimal (no format field) and a newer format")
{
    update::PackInfo info;
    std::string err;
    REQUIRE_MESSAGE (update::parsePackInfo (readText ("pack/pack-json-minimal.json"), info, &err), err);
    CHECK (info.id == "minimal-pack");
    CHECK (info.kind == "presets");
    CHECK (info.nameEn == "minimal-pack");
    CHECK (info.nameDe == "minimal-pack");
    CHECK_FALSE (info.license.paid());
    // round trip writes format 1
    update::PackInfo back;
    REQUIRE (update::parsePackInfo (update::packInfoToJson (info), back));
    CHECK (back.id == info.id);
    CHECK (json::parse (update::packInfoToJson (info))["format"] == formats::pack);
    // a newer pack format is refused (its layout may differ) - the user is told, nothing is installed
    CHECK_FALSE (update::parsePackInfo (readText ("pack/pack-json-format2-future.json"), info, &err));
    CHECK (err == "pack.json has a newer format");
    // the old flat zip (no pack.json, presets at the root) still plans
    std::string id, ver;
    REQUIRE (update::parsePackFileName ("Ohpe-content-legacy-flat-1.0.0.zip", id, ver));
    update::PackInfo expected;
    expected.id = id;
    expected.version = ver;
    const auto plan = update::planPack ({ { "Flat Keys.ohpepreset", 300 } }, {}, expected);
    REQUIRE (plan.ok);
    CHECK (plan.flat);
    CHECK (std::any_of (plan.files.begin(), plan.files.end(), [] (const auto& f) { return f.target == "presets/Flat Keys.ohpepreset"; }));
}

// ------------------------------------------------------------------------------------------------ settings
TEST_CASE ("formats: settings.json v1 round-trips and keeps unknown keys; v0 (0.6.2) migrates")
{
    const auto s = settings::UserSettings::fromJson (readText ("settings/settings-v1.json"));
    CHECK (s.fileVersion == 1);
    CHECK (s.language == settings::Language::German);
    CHECK_FALSE (s.tooltips);
    CHECK (s.helpOverlay);
    CHECK (s.skin == settings::Skin::Dark);
    CHECK (s.brightness == doctest::Approx (0.7f));
    CHECK (s.animations == settings::Animations::Reduced);
    CHECK (s.windowScale == 1.25f);
    CHECK (s.controllerChosen);
    CHECK (s.controllerProfile == settings::ControllerOff);
    CHECK_FALSE (s.controllerAuto);
    CHECK (s.controllerMaster == 1);
    const auto out = json::parse (s.toJson());
    CHECK (out["version"] == formats::settings);
    CHECK (out["futureKey"]["kept"] == true);
    CHECK (settings::UserSettings::fromJson (s.toJson()) == s);
    CHECK (json::parse (s.extras).count ("version") == 0); // a known key, never an "extra"

    const auto old = settings::UserSettings::fromJson (readText ("settings/settings-v0-0.6.2.json"));
    CHECK (old.fileVersion == 0);
    CHECK (old.language == settings::Language::English);
    // p53: a controller set up before "chosen" existed starts with the 0.6.3 defaults
    CHECK (old.controllerProfile == settings::ControllerGrid1);
    CHECK (old.controllerAuto);
    CHECK_FALSE (old.controllerChosen);
    CHECK (old.controllerIn == "Port A");
    const auto migrated = json::parse (old.toJson());
    CHECK (migrated["version"] == formats::settings);
    CHECK (settings::UserSettings::fromJson (old.toJson()).fileVersion == formats::settings);
}

TEST_CASE ("formats: update.json v1 and v0 (before 1.0) load")
{
    update::Settings base;
    base.channel = update::Channel::Preview;
    const auto v1 = update::Settings::fromJson (readText ("settings/update-v1.json"), base);
    CHECK_FALSE (v1.autoCheck);
    CHECK (v1.channel == update::Channel::Stable);
    CHECK (v1.skipped == "0.7.0");
    CHECK (v1.lastCheck == 1760000000);
    const auto v0 = update::Settings::fromJson (readText ("settings/update-v0.json"), update::Settings {});
    CHECK (v0.autoCheck);
    CHECK (v0.channel == update::Channel::Preview);
    CHECK (json::parse (v0.toJson())["version"] == formats::updateSettings);
    const auto again = update::Settings::fromJson (v1.toJson(), base);
    CHECK (again.skipped == v1.skipped);
    CHECK (again.channel == v1.channel);
}

// ------------------------------------------------------------------------------------------------ .ohpemidi
TEST_CASE ("formats: .ohpemidi v1 loads and round-trips")
{
    MidiMap m;
    REQUIRE (m.fromJson (readText ("midimap/midimap-v1.ohpemidi")));
    REQUIRE (m.mappings().size() == 3);
    CHECK (m.mappings()[0].paramId == "p1.vol");
    CHECK (m.mappings()[0].number == 7);
    CHECK (m.mappings()[1].type == MidiSourceType::Note);
    CHECK (m.mappings()[1].mode == MidiMapMode::Toggle);
    CHECK (m.mappings()[2].mode == MidiMapMode::Relative);
    CHECK (m.mappings()[2].minNorm == doctest::Approx (0.25f));
    MidiMap back;
    REQUIRE (back.fromJson (m.toJson()));
    REQUIRE (back.mappings().size() == 3);
    for (size_t i = 0; i < 3; ++i)
    {
        CHECK (back.mappings()[i].paramId == m.mappings()[i].paramId);
        CHECK (back.mappings()[i].channel == m.mappings()[i].channel);
        CHECK (back.mappings()[i].mode == m.mappings()[i].mode);
    }
}
