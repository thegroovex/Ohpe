// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p63 (1.0 phase A, item 3): robustness of every loader. Truncated, corrupted, oversized and wrong-type
// documents (state, presets, .ohpetape, .ohpewt, pack.json / content registry, settings, update settings,
// sample and user-table assignments, the sections of a session) must never crash, hang or throw: the
// loader refuses the document or falls back to defaults, and the core keeps rendering finite audio.
//
// The mutations are seeded (reproducible); OHPE_FUZZ_ITERATIONS scales the number of mutations per
// target (default 400) and OHPE_FUZZ_SEED changes the seed for a longer local run.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "breed/SoundGenome.h"
#include "model/StateSerializer.h"
#include "preset/Preset.h"
#include "preset/PresetLibrary.h"
#include "sample/SampleData.h"
#include "sample/SampleState.h"
#include "settings/UserSettings.h"
#include "tape/TapeFile.h"
#include "tape/TapeMachine.h"
#include "update/ContentPacks.h"
#include "update/PackFormat.h"
#include "update/UpdateManifest.h"
#include "update/UpdateSettings.h"
#include "wavetable/TableFile.h"
#include "wavetable/UserTable.h"
#include "wavetable/UserTableState.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace ohpe;

namespace
{
int envInt (const char* name, int fallback)
{
    if (const char* v = std::getenv (name); v != nullptr && *v != 0) return std::max (1, std::atoi (v));
    return fallback;
}

int fuzzIterations() { return envInt ("OHPE_FUZZ_ITERATIONS", 400); }
uint32_t fuzzSeed() { return (uint32_t) envInt ("OHPE_FUZZ_SEED", 6301); }

/** Seeded mutations of a valid document: truncation, flipped / inserted / deleted / repeated bytes, and
    (for JSON) values replaced by values of another type or out of range. */
struct Mutator
{
    explicit Mutator (uint32_t seed) : rng (seed) {}

    std::mt19937 rng;

    size_t pick (size_t n) { return n == 0 ? 0 : std::uniform_int_distribution<size_t> (0, n - 1) (rng); }

    std::string mutate (std::string s)
    {
        const int rounds = 1 + (int) pick (3);
        for (int r = 0; r < rounds; ++r) s = once (std::move (s));
        return s;
    }

    std::string once (std::string s)
    {
        static const char* values[] = { "\"x\"", "[]", "{}", "null", "true", "false", "-1", "0", "1e308", "-1e308", "1e-320",
                                        "99999999999999999999", "-2147483649", "4294967296", "\"\"", "[1,2,3]", "{\"a\":1}",
                                        "\"\\u0000\"", "0.5", "[[[[[]]]]]", "\"\\ud800\"", "18446744073709551615" };
        switch (pick (s.empty() ? 1 : 8))
        {
            case 0: // insert garbage
            {
                std::string g (1 + pick (16), ' ');
                for (auto& c : g) c = (char) pick (256);
                s.insert (pick (s.size() + 1), g);
                return s;
            }
            case 1: return s.substr (0, pick (s.size())); // truncate
            case 2: // flip bytes
                for (int i = 0, n = 1 + (int) pick (8); i < n; ++i) s[pick (s.size())] = (char) pick (256);
                return s;
            case 3: // delete a chunk
            {
                const size_t at = pick (s.size());
                return s.erase (at, 1 + pick (std::min<size_t> (64, s.size() - at)));
            }
            case 4: // repeat a chunk (grows the document)
            {
                const size_t at = pick (s.size()), len = 1 + pick (std::min<size_t> (32, s.size() - at));
                const auto chunk = s.substr (at, len);
                for (int i = 0, n = 1 + (int) pick (6); i < n; ++i) s.insert (at, chunk);
                return s;
            }
            default: // JSON: replace the value after a random ':' or '[' with one of another type
            {
                std::vector<size_t> marks;
                for (size_t i = 0; i < s.size(); ++i)
                    if (s[i] == ':' || s[i] == '[' || s[i] == ',') marks.push_back (i + 1);
                if (marks.empty()) return s;
                const size_t from = marks[pick (marks.size())];
                size_t to = from;
                if (to < s.size() && s[to] == '"')
                {
                    for (++to; to < s.size() && s[to] != '"'; ++to)
                        if (s[to] == '\\') ++to;
                    to = std::min (s.size(), to + 1);
                }
                else if (to < s.size() && (s[to] == '{' || s[to] == '['))
                {
                    int depth = 0;
                    bool inString = false;
                    for (; to < s.size(); ++to)
                    {
                        const char c = s[to];
                        if (inString) { if (c == '\\') ++to; else if (c == '"') inString = false; continue; }
                        if (c == '"') inString = true;
                        else if (c == '{' || c == '[') ++depth;
                        else if ((c == '}' || c == ']') && --depth == 0) { ++to; break; }
                    }
                }
                else
                    while (to < s.size() && s[to] != ',' && s[to] != '}' && s[to] != ']') ++to;
                s.replace (from, to - from, values[pick (std::size (values))]);
                return s;
            }
        }
    }
};

/** Runs `load` on the seed and on `fuzzIterations()` mutations of it. Nothing may escape (an exception
    here would terminate a host); the whole run must stay within a generous time (a hang never returns). */
void fuzz (const char* what, const std::string& seed, const std::function<void (const std::string&)>& load, uint32_t salt)
{
    Mutator m (fuzzSeed() ^ (salt * 2654435761u));
    const auto start = std::chrono::steady_clock::now();
    int escaped = 0;
    std::string firstEscape;
    auto run = [&] (const std::string& doc) {
        try { load (doc); }
        catch (const std::exception& e) { if (escaped++ == 0) firstEscape = e.what(); }
        catch (...) { if (escaped++ == 0) firstEscape = "unknown exception"; }
    };
    // fixed cases first: empty, wrong top-level types, deep nesting, a huge string
    for (const char* doc : { "", "{}", "[]", "null", "0", "\"text\"", "{\"format\":5}", "{\"format\":null,\"version\":\"1\"}" }) run (doc);
    run (std::string (100000, '[') + std::string (100000, ']'));
    run ("{\"a\":" + std::string (100000, '[') + std::string (100000, ']') + "}");
    run ("{\"format\":\"" + std::string (4 << 20, 'x') + "\"}");
    run (seed);
    for (int i = 0; i < fuzzIterations(); ++i) run (m.mutate (seed));
    const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    MESSAGE (what << ": " << fuzzIterations() << " mutations in " << seconds << " s");
    CHECK_MESSAGE (escaped == 0, what << ": " << escaped << " documents threw out of the loader, first: " << firstEscape);
    CHECK_MESSAGE (seconds < 240.0, what << ": the fuzz run took too long");
}

/** The core renders finite, bounded audio (a note on part 1, 0.2 s). */
void checkRenders (Core& core)
{
    while (core.prepareEffects()) {}
    constexpr int block = 256;
    std::vector<float> l (block), r (block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    float peak = 0.0f;
    bool finite = true;
    for (int b = 0; b < 40; ++b)
    {
        MidiEvent e[1] = { MidiEvent::noteOn (1, 60, 100, 0) };
        core.process (out, block, e, b == 0 ? 1 : 0, Transport {});
        for (int i = 0; i < block; ++i)
        {
            finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
            peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
        }
    }
    CHECK (finite);
    CHECK (peak < 16.0f);
}

std::unique_ptr<Core> richCore()
{
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, 256);
    std::mt19937 rng (7);
    auto& reg = core->params();
    for (int i = 0; i < reg.size(); i += 3) reg.setNormalised (i, std::uniform_real_distribution<float> (0.0f, 1.0f) (rng));
    MidiMapping m;
    m.paramId = "p1.vol";
    m.number = 74;
    core->midiMap().add (m);
    core->midiMapChanged();
    StepSequencer::Step s;
    s.on = true;
    s.note = 62;
    core->stepSequencer().setStep (2, s);
    return core;
}

usertable::UserTablePtr sawTable (int frames)
{
    std::vector<float> v ((size_t) frames * usertable::frameSize);
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (i % usertable::frameSize) / usertable::frameSize * 2.0f - 1.0f;
    return usertable::UserTable::create ("Saw", v);
}

std::string bytesToString (const std::vector<uint8_t>& b) { return std::string (b.begin(), b.end()); }
} // namespace

TEST_CASE ("robustness: damaged session states never crash the core, it keeps rendering")
{
    auto src = richCore();
    const auto seed = src->saveState ("{\"lang\":\"de\",\"part\":2}");
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, 256);
    int loaded = 0;
    fuzz ("state", seed, [&] (const std::string& doc) {
        std::string ui;
        if (core->loadState (doc, &ui)) ++loaded;
    }, 1);
    CHECK (loaded > 0);
    checkRenders (*core);
    // a garbage state is refused and leaves the parameters as they were
    core->params().set ("p1.vol", 0.25f);
    CHECK_FALSE (core->loadState ("{\"format\":\"ohpe-state\"" + std::string (10, '\x01')));
    CHECK (core->params().get ("p1.vol") == doctest::Approx (0.25f));
}

TEST_CASE ("robustness: damaged sections inside a valid state are skipped, not fatal")
{
    auto src = richCore();
    const auto seed = src->saveState ("{}");
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, 256);
    Mutator m (fuzzSeed() + 11);
    int escaped = 0;
    for (const char* section : { "midimap", "sequencer", "orbit", "morph", "motion", "harmony", "tape", "ui" })
        for (int i = 0; i < std::max (20, fuzzIterations() / 8); ++i)
        {
            // replace the section with a mutated copy of itself (or a value of another type)
            static const char* others[] = { "5", "\"x\"", "[]", "null", "{\"steps\":7}", "{\"lanes\":[1,{}]}", "[{}]" };
            const auto key = std::string ("\"") + section + "\":";
            auto doc = seed;
            const auto at = doc.find (key);
            if (at == std::string::npos) break;
            const size_t from = at + key.size();
            size_t to = from;
            int depth = 0;
            bool inString = false;
            for (; to < doc.size(); ++to)
            {
                const char c = doc[to];
                if (inString) { if (c == '\\') ++to; else if (c == '"') inString = false; continue; }
                if (c == '"') { inString = true; if (depth == 0 && to > from) break; continue; }
                if (c == '{' || c == '[') ++depth;
                else if (c == '}' || c == ']') { if (depth == 0) break; if (--depth == 0) { ++to; break; } }
                else if (c == ',' && depth == 0) break;
            }
            auto mutated = m.mutate (doc.substr (from, to - from));
            if (i % 3 == 0) mutated = others[(size_t) i % std::size (others)];
            // keep the document valid JSON around the section: a string section is quoted
            auto candidate = doc.substr (0, from) + mutated + doc.substr (to);
            try { core->loadState (candidate); }
            catch (...) { ++escaped; }
        }
    CHECK (escaped == 0);
    checkRenders (*core);
}

TEST_CASE ("robustness: damaged part and project presets never crash, apply stays in range")
{
    auto src = richCore();
    preset::Meta meta;
    meta.name = "Fuzz";
    const auto part = preset::toJson (preset::capturePart (src->params(), 0, meta));
    meta.kind = preset::Kind::Project;
    const auto project = preset::toJson (preset::captureProject (*src, meta));
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, 256);
    int parsed = 0;
    auto apply = [&] (const std::string& doc) {
        preset::Preset p;
        std::string err;
        if (! preset::fromJson (doc, p, &err)) return;
        ++parsed;
        if (p.meta.kind == preset::Kind::Project) preset::applyProject (p, *core);
        else preset::applyPart (p, core->params(), 1);
    };
    fuzz ("part preset", part, apply, 2);
    fuzz ("project preset", project, apply, 3);
    CHECK (parsed > 0);
    // every parameter stays inside its range
    auto& reg = core->params();
    int outside = 0;
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        const float v = reg.get (i);
        if (! std::isfinite (v) || v < std::min (d.minValue, d.maxValue) - 1.0e-3f || v > std::max (d.minValue, d.maxValue) + 1.0e-3f) ++outside;
    }
    CHECK (outside == 0);
    checkRenders (*core);
    // the low-level preset document of the state serializer too
    fuzz ("state preset", state::savePreset (src->params(), "p1.", "x"), [&] (const std::string& doc) { state::loadPreset (doc, core->params(), "p2."); }, 4);
}

TEST_CASE ("robustness: damaged .ohpetape files are refused or load clean audio")
{
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, 256);
    // a tape with audio on two tracks: record the input for a moment
    auto& reg = core->params();
    std::vector<float> in (256), l (256), r (256);
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * (float) std::sin ((double) i * 0.05);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    reg.set ("tape.tr1.arm", 1.0f);
    reg.set ("tape.tr1.src", (float) tapesource::input);
    reg.set ("tape.rec", 1.0f);
    reg.set ("tape.play", 1.0f);
    for (int b = 0; b < 40; ++b)
    {
        core->setAudioInput (in.data(), in.data());
        core->process (out, 256, nullptr, 0, Transport {});
    }
    reg.set ("tape.rec", 0.0f);
    reg.set ("tape.play", 0.0f);
    reg.set ("tape.tr1.arm", 0.0f);
    core->process (out, 256, nullptr, 0, Transport {});
    core->tape().housekeeping();
    tapefile::Settings s;
    s.loop = true;
    s.loopOut = 1.0;
    const auto file = tapefile::save (core->tape(), s);

    auto dst = std::make_unique<Core>();
    dst->prepare (44100.0, 256); // another rate: the loader resamples
    int loaded = 0;
    fuzz ("ohpetape", bytesToString (file), [&] (const std::string& doc) {
        tapefile::Settings got;
        tapefile::readSettings (doc.data(), doc.size(), got);
        if (tapefile::load (dst->tape(), doc.data(), doc.size(), got)) ++loaded;
    }, 5);
    CHECK (loaded > 0);
    // the tape in the state blob (host session) too
    fuzz ("tape audio blob", [&] { std::vector<uint8_t> b; core->tape().saveAudio (b); return bytesToString (b); }(),
          [&] (const std::string& doc) { dst->tape().loadAudio (doc.data(), doc.size(), false); }, 6);
    checkRenders (*dst);
}

TEST_CASE ("robustness: a tape blob holding NaN / Inf samples or absurd rates plays silence, not noise")
{
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, 256);
    std::vector<uint8_t> blob;
    core->tape().saveAudio (blob);
    // build a blob with one page of track 1 that holds NaN, Inf and huge values
    REQUIRE (blob.size() >= 36);
    std::vector<uint8_t> bad (blob.begin(), blob.begin() + 36);
    auto put32 = [&] (uint32_t v) { const auto* p = reinterpret_cast<const uint8_t*> (&v); bad.insert (bad.end(), p, p + 4); };
    put32 (0);
    put32 (0);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < TapeBlock::frames; ++i)
        {
            const float v = i % 3 == 0 ? NAN : (i % 3 == 1 ? INFINITY : 1.0e30f);
            const auto* p = reinterpret_cast<const uint8_t*> (&v);
            bad.insert (bad.end(), p, p + 4);
        }
    put32 (0xffffffffu);
    if (core->tape().loadAudio (bad.data(), bad.size(), false))
    {
        auto& reg = core->params();
        reg.set ("tape.play", 1.0f);
        std::vector<float> l (256), r (256);
        OutputBuffers out;
        out.left[0] = l.data();
        out.right[0] = r.data();
        bool finite = true;
        float peak = 0.0f;
        for (int b = 0; b < 40; ++b)
        {
            core->process (out, 256, nullptr, 0, Transport {});
            for (int i = 0; i < 256; ++i)
            {
                finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
                peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
            }
        }
        CHECK (finite);
        CHECK (peak < 16.0f);
    }
    // a tape file whose audio claims an absurd sample rate is refused quickly (no endless resampling)
    tapefile::Settings s;
    auto file = tapefile::save (core->tape(), s);
    const std::string text (file.begin(), file.end());
    const auto blobAt = text.find ("OHPETAPE");
    REQUIRE (blobAt != std::string::npos);
    for (double rate : { 1.0e-300, 1.0e-3, 1.0e300, -48000.0, (double) NAN, (double) INFINITY })
    {
        auto f = file;
        std::memcpy (f.data() + blobAt + 8 + 12, &rate, sizeof (double));
        const auto start = std::chrono::steady_clock::now();
        tapefile::Settings got;
        try { tapefile::load (core->tape(), f.data(), f.size(), got); }
        catch (...) { FAIL ("tape load threw"); }
        CHECK (std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count() < 5.0);
    }
}

TEST_CASE ("robustness: damaged .ohpewt files are refused or decode to finite tables")
{
    const auto bytes = usertable::encode (*sawTable (4));
    int decoded = 0, unclean = 0;
    fuzz ("ohpewt", bytesToString (bytes), [&] (const std::string& doc) {
        const auto t = usertable::decode (reinterpret_cast<const uint8_t*> (doc.data()), doc.size());
        if (t == nullptr) return;
        ++decoded;
        for (int f = 0; f < t->numFrames(); ++f)
            for (int lv = 0; lv < usertable::numLevels; ++lv)
            {
                const float* x = t->frame (f, lv);
                for (int i = 0; i <= usertable::frameSize; ++i)
                    if (! std::isfinite (x[i]) || std::abs (x[i]) > 64.0f) { ++unclean; return; }
            }
    }, 7);
    CHECK (decoded > 0);
    CHECK (unclean == 0);
    // base64 (the embedded copy in a session) never throws either
    fuzz ("ohpewt base64", usertable::base64Encode (bytes), [&] (const std::string& doc) { usertable::decode (usertable::base64Decode (doc)); }, 8);
}

TEST_CASE ("robustness: damaged pack.json, content registry and update files are refused cleanly")
{
    const std::string packJson = R"({"format":1,"id":"atmo","version":"1.0.0","kind":"presets","minApp":"0.4.0",)"
                                 R"("name":{"en":"Atmo","de":"Atmo"},"description":{"en":"Wide","de":"Weit"},)"
                                 R"("files":{"presets/a.ohpepreset":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},)"
                                 R"("license":{"type":"free"}})";
    fuzz ("pack.json", packJson, [] (const std::string& doc) {
        update::PackInfo p;
        std::string err;
        update::parsePackInfo (doc, p, &err);
        update::canonicalPackJson (doc);
        const std::vector<update::ZipItem> items { { "pack.json", 500 }, { "presets/a.ohpepreset", 10 }, { "../../evil", 5 } };
        const auto plan = update::planPack (items, doc, {}, 0, true);
        for (const auto& f : plan.files) CHECK (f.target.find ("..") == std::string::npos);
    }, 9);
    update::ContentRegistry reg;
    fuzz ("content registry", R"({"version":1,"packs":[{"id":"atmo","version":"1.0.0","kind":"presets","files":["presets/a.ohpepreset"]}]})",
          [] (const std::string& doc) { update::ContentRegistry::fromJson (doc).toJson(); }, 10);
    fuzz ("update settings", update::Settings {}.toJson(), [] (const std::string& doc) { update::Settings::fromJson (doc, update::Settings {}); }, 11);
    fuzz ("update manifest", R"({"version":"0.7.0","date":"2026-10-01","notes":{"en":"x","de":"y"},"assets":[{"platform":"linux","url":"https://example.org/a","size":10,"sha256":"aa"}]})",
          [] (const std::string& doc) {
              update::Release r;
              update::parseManifest (doc, r);
              update::parseReleaseList ("[" + doc + "]");
          }, 12);
}

TEST_CASE ("robustness: damaged settings.json falls back to defaults")
{
    settings::UserSettings s;
    const auto seed = s.toJson();
    fuzz ("settings.json", seed, [] (const std::string& doc) {
        const auto u = settings::UserSettings::fromJson (doc);
        CHECK (std::isfinite (u.windowScale));
        u.toJson();
    }, 13);
    fuzz ("preset library user data", R"({"favourites":["a","b"],"recent":["c"],"ratings":{"a":3}})",
          [] (const std::string& doc) { preset::UserData d; d.fromJson (doc); }, 14);
}

TEST_CASE ("robustness: damaged sample and user-table assignments fall back to the built-in sounds")
{
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, 256);
    std::vector<float> mono (4000);
    for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (float) std::sin ((double) i * 0.03);
    const auto smp = SampleData::fromMono (mono, 48000.0, "/x/a.wav");
    const std::string samples = R"({"format":"ohpe-samples","version":1,"parts":{"p1":{"sampler":{"zones":[{"path":"/x/a.wav","lo":0,"hi":127,"vlo":1,"vhi":127,"root":60,"ls":100,"le":3000}]},"grain":{"path":"/x/a.wav"}}},"drum":[{"pad":1,"path":"/x/a.wav"}]})";
    fuzz ("sample assignments", samples, [&] (const std::string& doc) {
        sample::restoreAssignments (*core, doc, [&] (const std::string&) { return smp; });
    }, 15);
    // play every part with whatever survived (sampler / grain engines on all parts)
    auto& reg = core->params();
    for (int p = 1; p <= 4; ++p) reg.set ("p" + std::to_string (p) + ".engine", (float) (p % 2 == 0 ? (int) EngineType::Grain : (int) EngineType::Sampler));
    checkRenders (*core);

    const auto table = sawTable (2);
    const std::string tables = R"({"format":"ohpe-usertables","version":1,"parts":{"p1":{"wavetable":{"path":"/x/a.ohpewt","file":"a.ohpewt","embed":")"
                               + usertable::base64Encode (usertable::encode (*table)) + R"("},"vocal":{"sameAs":"wavetable"}}}})";
    fuzz ("user-table assignments", tables, [&] (const std::string& doc) {
        usertable::restoreAssignments (*core, doc, [&] (const std::string&, const std::string&) { return table; });
    }, 16);
    checkRenders (*core);
}

TEST_CASE ("robustness: a damaged breeding session is refused")
{
    // the breeding session is stored in the UI state; any JSON must be refused or loaded without throwing
    auto core = std::make_unique<Core>();
    breed::Session session (breed::Schema::forPart (core->params(), 0));
    fuzz ("breed session (own)", session.toJson(), [&] (const std::string& doc) { session.fromJson (doc); }, 18);
    fuzz ("breed session", R"({"version":1,"generation":2,"parents":[[0.1,0.2],[0.3,0.4]],"children":[{"genes":[0.5,0.6],"score":1}]})",
          [&] (const std::string& doc) { session.fromJson (doc); }, 17);
}
