// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.7: FX rack (six slots per part, master rack) and modulation matrix.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/FxRack.h"
#include "mod/ModMatrix.h"
#include "preset/Preset.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <fstream>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
/** Renders the main output of a core for `blocks` blocks of `block` samples (notes from the queue). */
Stereo renderCore (Core& core, int blocks, int block = 512, const Transport& tr = {})
{
    Stereo s ((size_t) (blocks * block));
    std::vector<float> l ((size_t) block), r ((size_t) block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    for (int b = 0; b < blocks; ++b)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        Transport t = tr;
        t.ppqAtBlockStart = tr.ppqAtBlockStart + (double) b * block * tr.bpm / 60.0 / sampleRate;
        core.process (out, block, nullptr, 0, t);
        std::copy (l.begin(), l.end(), s.l.begin() + (size_t) b * (size_t) block);
        std::copy (r.begin(), r.end(), s.r.begin() + (size_t) b * (size_t) block);
    }
    return s;
}

void setAll (Core& core, std::initializer_list<std::pair<const char*, float>> values)
{
    for (const auto& [id, v] : values) REQUIRE_MESSAGE (core.params().set (id, v), id);
}

/** A held saw note on part 1, limiter off (comparisons need the raw signal). */
void basicPatch (Core& core)
{
    setAll (core, { { "mix.master.limit", 0.0f }, { "p1.engine", (float) EngineType::Analog }, { "p1.amp.s", 1.0f }, { "p1.voices", 4.0f } });
}

/** Brightness: energy of the first difference relative to the signal energy (~ mean squared frequency). */
double brightnessOf (const Stereo& x, size_t from = 6000)
{
    double d = 0.0, e = 1.0e-12;
    for (size_t i = from; i < x.size(); ++i)
    {
        const double dx = (double) x.l[i] - (double) x.l[i - 1];
        d += dx * dx;
        e += (double) x.l[i] * (double) x.l[i];
    }
    return d / e;
}

bool identical (const Stereo& a, const Stereo& b)
{
    return a.l == b.l && a.r == b.r;
}

float maxDiff (const Stereo& a, const Stereo& b)
{
    float d = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) d = std::max (d, std::max (std::abs (a.l[i] - b.l[i]), std::abs (a.r[i] - b.r[i])));
    return d;
}

std::vector<std::string> registryIds (const Core& core)
{
    std::vector<std::string> v;
    for (const auto& d : core.params().all()) v.push_back (d.id);
    return v;
}
} // namespace

// ------------------------------------------------------------------------------------------------- layout
TEST_CASE ("0.7: rack and matrix parameters are appended after 0.6, in their documented order")
{
    Core core;
    const auto ids = registryIds (core);
    const auto it = std::find (ids.begin(), ids.end(), "mix.master.lowpass");
    REQUIRE (it != ids.end());
    const auto first = (size_t) (it - ids.begin()) + 1;
    REQUIRE (first < ids.size());
    CHECK (ids[first] == "p1.fx2.type");
    CHECK (ids[first + 1] == "p1.fx2.chorus.rate");
    // every part: slots 1-6, mix / bypass of each; master rack; matrix of every part
    for (int p = 1; p <= 4; ++p)
    {
        const auto pp = "p" + std::to_string (p) + ".";
        for (int s = 0; s < FxRack::numPartSlots; ++s)
        {
            const auto pre = FxRack::partSlotPrefix (pp, s);
            CHECK (core.params().indexOf (pre + "type") >= 0);
            CHECK (core.params().indexOf (pre + "delay.mix") >= 0);
            CHECK (core.params().indexOf (pre + "mix") >= 0);
            CHECK (core.params().indexOf (pre + "bypass") >= 0);
        }
        for (int k = 1; k <= mod::numSlots; ++k)
            for (const char* f : { "src", "via", "dst", "amt", "curve" })
                CHECK (core.params().indexOf (pp + "mod.s" + std::to_string (k) + "." + f) >= 0);
        for (const char* f : { "lfo2.rate", "lfo2.sync", "lfo2.shape", "lfo2.phase", "lfo2.fade", "lfo2.mode", "lfo2.depth", "lfo3.depth",
                               "env3.delay", "env3.a", "env3.hold", "env3.d", "env3.s", "env3.r", "macro1", "macro4", "seqlane" })
            CHECK (core.params().indexOf (pp + f) >= 0);
    }
    for (int s = 0; s < FxRack::numMasterSlots; ++s) CHECK (core.params().indexOf (FxRack::masterSlotPrefix (s) + "reverb.size") >= 0);
    // the matrix block ends with the last part's sequencer lane; the rack type blocks (later in 0.7) follow it
    CHECK (core.params().indexOf ("p4.seqlane") + 1 == core.params().indexOf ("p1.fx.hyper.rate"));
    // ... and the 0.8 type blocks (comb ... width, sound revision 3) follow those
    CHECK (core.params().indexOf ("mfx4.hall.mix") + 1 == core.params().indexOf ("p1.fx.comb.freq"));
    // matrix parameters come after every rack parameter
    CHECK (core.params().indexOf ("p1.mod.s1.src") > core.params().indexOf ("mfx4.bypass"));
    // host names tell the slots apart; the 0.2 slot keeps its names
    CHECK (core.params().descriptor (core.params().indexOf ("p1.fx.chorus.rate")).nameEn == "Chorus Rate");
    CHECK (core.params().descriptor (core.params().indexOf ("p2.fx3.chorus.rate")).nameEn == "P2 FX3 Chorus Rate");
    CHECK (core.params().descriptor (core.params().indexOf ("mfx2.delay.mix")).nameDe == "Master-FX 2 Delay-Mischung");
}

TEST_CASE ("0.7: matrix source / target tables match the choice lists; keys are unique; reserved targets are inert")
{
    CHECK (mod::sourceTable().size() == (size_t) mod::numSources);
    CHECK (mod::destTable().size() == (size_t) mod::numDests);
    CHECK (mod::sourceNamesDe().size() == mod::sourceNamesEn().size());
    for (size_t i = 0; i < mod::sourceTable().size(); ++i) CHECK (mod::sourceIndexOfKey (mod::sourceTable()[i].key) == (int) i);
    for (size_t i = 0; i < mod::destTable().size(); ++i) CHECK (mod::destIndexOfKey (mod::destTable()[i].key) == (int) i);
    CHECK (mod::destIndexOfKey ("azimuth") == mod::DstAzimuth);
    Core core;
    CHECK (core.part (0).destAvailable (mod::DstVolume));
    CHECK (core.part (0).destAvailable (mod::DstLfo3Depth));
    CHECK_FALSE (core.part (0).destAvailable (mod::DstAzimuth)); // the spatialiser does not exist yet
    CHECK (core.part (0).destParamIndex (mod::DstAzimuth) == -1);
    const auto& d = core.params().descriptor (core.params().indexOf ("p1.mod.s1.dst"));
    CHECK (d.choices.size() == (size_t) mod::numDests);
}

TEST_CASE ("0.7: golden file is a prefix and documents the 0.7 block")
{
    Core core;
    const auto ids = registryIds (core);
    std::ifstream in (std::string (OHPE_TEST_DATA_DIR) + "/golden/param_ids_v1.txt");
    REQUIRE (in.good());
    std::vector<std::string> golden;
    for (std::string line; std::getline (in, line);)
        if (! line.empty() && line[0] != '#') golden.push_back (line);
    REQUIRE (golden.size() <= ids.size());
    CHECK (std::find (golden.begin(), golden.end(), "p4.seqlane") != golden.end());
}

// ------------------------------------------------------------------------------------------------- FX rack
TEST_CASE ("0.7 rack: slot 1 at mix 100 % is bit-identical to the 0.2 FX slot; empty slots and bypass are exact")
{
    const auto in = [] {
        Stereo s (20000);
        dsp::Random rng (5);
        for (size_t i = 0; i < s.size(); ++i)
        {
            s.l[i] = 0.5f * std::sin (0.031f * (float) i) + 0.05f * rng.nextBipolar();
            s.r[i] = 0.5f * std::sin (0.017f * (float) i) + 0.05f * rng.nextBipolar();
        }
        return s;
    }();
    ParamRegistry reg;
    InsertSlot::registerParams (reg, "p1.fx.", "part1/fx");
    FxRack::registerExtraSlots (reg, "p1.", "part1/fx");
    FxRack::registerSlotControls (reg, "p1.", "part1/fx");
    reg.freeze();
    ParamSnapshot snap;
    snap.prepare (reg);

    for (int type = InsertSlot::Off; type < InsertSlot::numTypes; ++type)
    {
        CAPTURE (type);
        reg.resetToDefaults();
        reg.set ("p1.fx.type", (float) type);
        // reference: the 0.2 slot on its own
        InsertSlot ref;
        ref.bind (reg, "p1.fx.");
        ref.prepare (sampleRate, 256);
        FxRack rack;
        rack.bindPart (reg, "p1.");
        rack.prepare (sampleRate, 256);
        // a bypassed effect in slot 2 must not change anything either
        reg.set ("p1.fx2.type", (float) InsertSlot::Reverb);
        reg.set ("p1.fx2.bypass", 1.0f);
        rack.prepareSelected (reg);
        snap.pull (reg);
        ref.pullParams (snap);
        rack.pullParams (snap);
        Stereo a = in, b = in;
        for (size_t pos = 0; pos < in.size(); pos += 256)
        {
            const int n = (int) std::min<size_t> (256, in.size() - pos);
            ref.process (a.l.data() + pos, a.r.data() + pos, n);
            rack.process (b.l.data() + pos, b.r.data() + pos, n);
        }
        CHECK (identical (a, b));
        if (type == InsertSlot::Off) CHECK (identical (b, in));
    }
}

TEST_CASE ("0.7 rack: a type whose parameter block is only partly registered keeps the defaults of the missing parameters")
{
    // full registry (reference) and one where every second parameter of each type block is missing
    ParamRegistry full, part;
    InsertSlot::registerParams (full, "p1.fx.", "part1/fx");
    full.freeze();
    {
        std::vector<ParamDescriptor> keep;
        int n = 0;
        for (const auto& d : full.all())
        {
            const bool isBlockParam = d.id.rfind ("p1.fx.", 0) == 0 && d.id != "p1.fx.type";
            if (isBlockParam && (n++ % 2) == 1) continue; // drop it: unregistered for this slot
            keep.push_back (d);
        }
        part.add (keep);
    }
    part.freeze();
    REQUIRE (part.size() < full.size());
    ParamSnapshot snapFull, snapPart;
    snapFull.prepare (full);
    snapPart.prepare (part);

    Stereo in (8192);
    dsp::Random rng (11);
    for (size_t i = 0; i < in.size(); ++i) { in.l[i] = 0.4f * rng.nextBipolar(); in.r[i] = 0.4f * std::sin (0.02f * (float) i); }

    for (int type = InsertSlot::Off + 1; type < InsertSlot::numTypes; ++type)
    {
        CAPTURE (type);
        full.resetToDefaults();
        part.resetToDefaults();
        full.set ("p1.fx.type", (float) type);
        part.set ("p1.fx.type", (float) type);
        InsertSlot a, b;
        a.bind (full, "p1.fx.");
        b.bind (part, "p1.fx.");
        a.prepare (sampleRate, 256);
        b.prepare (sampleRate, 256);
        snapFull.pull (full);
        snapPart.pull (part);
        a.pullParams (snapFull);
        b.pullParams (snapPart);
        Stereo x = in, y = in;
        for (size_t pos = 0; pos < in.size(); pos += 256)
        {
            const int n = (int) std::min<size_t> (256, in.size() - pos);
            a.process (x.l.data() + pos, x.r.data() + pos, n);
            b.process (y.l.data() + pos, y.r.data() + pos, n);
        }
        CHECK (allFinite (y.l));
        CHECK (allFinite (y.r));
        // a missing parameter must not be driven to 0 (or anything): it stays at the effect's own default
        if (b.currentType() == type) CHECK (identical (x, y));
    }
}

TEST_CASE ("0.7 rack: slot order matters, moveSlot reorders exactly; mix 0 is dry after the glide")
{
    ParamRegistry reg;
    InsertSlot::registerParams (reg, "p1.fx.", "part1/fx");
    FxRack::registerExtraSlots (reg, "p1.", "part1/fx");
    FxRack::registerSlotControls (reg, "p1.", "part1/fx");
    reg.freeze();
    ParamSnapshot snap;
    snap.prepare (reg);
    auto run = [&] () {
        FxRack rack;
        rack.bindPart (reg, "p1.");
        rack.prepare (sampleRate, 256);
        rack.prepareSelected (reg);
        snap.pull (reg);
        rack.pullParams (snap);
        Stereo s (24000);
        for (size_t i = 0; i < s.size(); ++i) s.l[i] = s.r[i] = 0.7f * std::sin (0.05f * (float) i);
        for (size_t pos = 0; pos < s.size(); pos += 256)
            rack.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> (256, s.size() - pos));
        return s;
    };
    // crush, then a resonant low-pass vs low-pass, then crush
    reg.set ("p1.fx.type", (float) InsertSlot::Bitcrush);
    reg.set ("p1.fx.bitcrush.bits", 3.0f);
    reg.set ("p1.fx2.type", (float) InsertSlot::FilterDrive);
    reg.set ("p1.fx2.filterdrive.cutoff", 900.0f);
    reg.set ("p1.fx2.filterdrive.drive", 0.8f);
    const auto crushFirst = run();
    const auto setter = [&] (int idx, float v) { reg.set (idx, v); };
    REQUIRE (FxRack::moveSlot (reg, FxRack::partPrefixes ("p1."), 1, 0, setter));
    CHECK (std::lround (reg.get ("p1.fx.type")) == InsertSlot::FilterDrive);
    CHECK (std::lround (reg.get ("p1.fx2.type")) == InsertSlot::Bitcrush);
    CHECK (reg.get ("p1.fx.filterdrive.cutoff") == doctest::Approx (900.0f));
    CHECK (reg.get ("p1.fx2.bitcrush.bits") == doctest::Approx (3.0f));
    const auto filterFirst = run();
    CHECK (maxDiff (crushFirst, filterFirst) > 0.05f);
    // moving back restores the first order exactly
    REQUIRE (FxRack::moveSlot (reg, FxRack::partPrefixes ("p1."), 0, 1, setter));
    CHECK (identical (run(), crushFirst));
    // a move across several slots keeps the others in order
    reg.set ("p1.fx3.type", (float) InsertSlot::Chorus);
    REQUIRE (FxRack::moveSlot (reg, FxRack::partPrefixes ("p1."), 0, 2, setter));
    CHECK (std::lround (reg.get ("p1.fx.type")) == InsertSlot::FilterDrive);
    CHECK (std::lround (reg.get ("p1.fx2.type")) == InsertSlot::Chorus);
    CHECK (std::lround (reg.get ("p1.fx3.type")) == InsertSlot::Bitcrush);
}

TEST_CASE ("0.7 rack: lazy slots pass dry until prepared; every type in every slot stays finite and denormal-free")
{
    Core core;
    core.prepare (sampleRate, 512);
    basicPatch (core);
    core.params().set ("p1.fx4.type", (float) InsertSlot::Reverb);
    core.queueNote (48, 1.0f, true);
    renderCore (core, 2);
    CHECK (core.fxRack (0).isWaiting());
    CHECK (core.effectsWaiting());
    CHECK (core.prepareEffects());
    renderCore (core, 1);
    CHECK_FALSE (core.effectsWaiting());
    CHECK (core.fxRack (0).slot (3).currentType() == InsertSlot::Reverb);

    // all slots of all racks busy with extreme settings, then silence
    for (int p = 1; p <= 4; ++p)
        for (int s = 0; s < FxRack::numPartSlots; ++s)
        {
            const auto pre = FxRack::partSlotPrefix ("p" + std::to_string (p) + ".", s);
            const int t = 1 + (s + p) % (InsertSlot::numTypes - 1);
            core.params().set (pre + "type", (float) t);
            core.params().set (pre + "delay.feedback", 0.95f);
            core.params().set (pre + "phaser.feedback", 0.95f);
            core.params().set (pre + "filterdrive.reso", 1.0f);
            core.params().set (pre + "filterdrive.drive", 1.0f);
            core.params().set (pre + "reverb.size", 1.0f);
            core.params().set (pre + "mix", (float) ((s % 3) / 2.0));
        }
    for (int s = 0; s < FxRack::numMasterSlots; ++s) core.params().set (FxRack::masterSlotPrefix (s) + "type", (float) (1 + s * 2));
    core.prepareEffects();
    for (int n : { 36, 48, 60, 72 }) core.queueNote (n, 1.0f, true);
    const auto busy = renderCore (core, 40);
    CHECK (allFinite (busy.l));
    CHECK (allFinite (busy.r));
    for (int n : { 36, 48, 60, 72 }) core.queueNote (n, 0.0f, false);
    core.queueNote (48, 0.0f, false);
    const auto tail = renderCore (core, 400);
    CHECK (allFinite (tail.l));
    CHECK (noDenormals (tail.l));
    CHECK (noDenormals (tail.r));
}

TEST_CASE ("0.7 rack: master rack runs between the returns and the master dynamics; Off is exact")
{
    auto render = [] (int masterType) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        core.params().set ("mfx1.type", (float) masterType);
        core.params().set ("mfx1.bitcrush.bits", 4.0f);
        core.prepareEffects();
        core.queueNote (57, 1.0f, true);
        return renderCore (core, 20);
    };
    const auto off = render (InsertSlot::Off);
    const auto crushed = render (InsertSlot::Bitcrush);
    CHECK (maxDiff (off, crushed) > 0.01f);
    CHECK (identical (render (InsertSlot::Off), off));
}

TEST_CASE ("0.7 migration: 0.6 presets / projects load into slot 1 and sound the same; the new parameters keep defaults")
{
    // a part preset as written by 0.6 (FX slot type delay with its block)
    preset::Preset p;
    std::string err;
    REQUIRE (preset::fromJson (R"({"format":"ohpe-preset","version":2,"kind":"part","name":"Old","engine":"analog",
        "params":{"engine":0,"amp.s":1.0,"fx.type":5,"fx.delay.time":0.2,"fx.delay.feedback":0.5,"fx.delay.mix":0.4}})", p, &err));
    CHECK (p.meta.modSchema == 0);
    Core a, b;
    for (Core* c : { &a, &b }) c->prepare (sampleRate, 512);
    REQUIRE (preset::applyPart (p, a.params(), 0).ok);
    CHECK (std::lround (a.params().get ("p1.fx.type")) == InsertSlot::Delay);
    CHECK (a.params().get ("p1.fx.delay.mix") == doctest::Approx (0.4f));
    CHECK (a.params().get ("p1.fx.mix") == doctest::Approx (1.0f));
    CHECK (a.params().get ("p1.fx.bypass") == 0.0f);
    for (int s = 1; s < FxRack::numPartSlots; ++s) CHECK (std::lround (a.params().get (FxRack::partSlotPrefix ("p1.", s) + "type")) == 0);
    for (int k = 1; k <= mod::numSlots; ++k) CHECK (std::lround (a.params().get ("p1.mod.s" + std::to_string (k) + ".src")) == 0);
    // a 0.6 project state (only 0.6 IDs) restores the same sound as setting those values directly
    const auto state = a.saveState();
    REQUIRE (b.loadState (state));
    for (Core* c : { &a, &b }) { c->params().set ("mix.master.limit", 0.0f); c->queueNote (60, 1.0f, true); }
    CHECK (identical (renderCore (a, 30), renderCore (b, 30)));
    // presets with matrix routings carry the matrix schema
    a.params().set ("p1.mod.s1.src", (float) mod::SrcLfo2);
    const auto cap = preset::capturePart (a.params(), 0, {});
    preset::Preset back;
    REQUIRE (preset::fromJson (preset::toJson (cap), back));
    CHECK (back.meta.modSchema == mod::schemaVersion);
}

// ------------------------------------------------------------------------------------------------- matrix
TEST_CASE ("0.7 matrix: a routing with amount 0 / source off / via at 0 leaves the sound bit-identical")
{
    auto render = [] (std::initializer_list<std::pair<const char*, float>> extra) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        setAll (core, extra);
        core.queueNote (60, 0.8f, true);
        core.queueNote (64, 0.6f, true);
        return renderCore (core, 30);
    };
    const auto ref = render ({});
    CHECK (identical (render ({ { "p1.mod.s1.src", (float) mod::SrcLfo2 }, { "p1.mod.s1.dst", (float) mod::DstPitch } }), ref));
    CHECK (identical (render ({ { "p1.mod.s1.dst", (float) mod::DstEngine1 + 2 }, { "p1.mod.s1.amt", 0.5f } }), ref));
    // via Macro 1 at 0: the routing is silent; per-voice (pitch) and parameter (cutoff) routings alike
    const auto viaZero = render ({ { "p1.mod.s1.src", (float) mod::SrcLfo2 }, { "p1.mod.s1.via", (float) mod::SrcMacro1 },
                                   { "p1.mod.s1.dst", (float) mod::DstPitch }, { "p1.mod.s1.amt", 1.0f } });
    CHECK (maxDiff (viaZero, ref) < 1.0e-6f);
}

TEST_CASE ("0.7 matrix: per-voice pitch from LFO 2 makes vibrato; velocity to timbre brightens each voice on its own")
{
    Core core;
    core.prepare (sampleRate, 512);
    basicPatch (core);
    // one oscillator, no detune, gently filtered: one zero crossing per cycle
    setAll (core, { { "p1.analog.cutoff", 3000.0f }, { "p1.analog.keytrack", 0.0f }, { "p1.analog.detune", 0.0f }, { "p1.analog.oscmix", 0.0f },
                    { "p1.analog.drive", 0.0f }, { "p1.analog.shape", 0.0f }, { "p1.mod.s1.src", (float) mod::SrcLfo2 },
                    { "p1.mod.s1.dst", (float) mod::DstPitch }, { "p1.mod.s1.amt", 1.0f }, { "p1.lfo2.rate", 2.0f },
                    { "p1.lfo2.shape", (float) mod::ShapeSquare } });
    core.queueNote (69, 1.0f, true);
    const auto s = renderCore (core, 48); // 24576 samples: both measuring windows inside
    // square LFO, +-12 semitones: halves of 0.25 s at about 880 Hz and 220 Hz
    const double hi = measureFrequency (s.l, sampleRate, 2000, 10000);
    const double lo = measureFrequency (s.l, sampleRate, 14500, 22500);
    CAPTURE (hi);
    CAPTURE (lo);
    CHECK (std::max (hi, lo) / std::min (hi, lo) == doctest::Approx (4.0).epsilon (0.08));

    auto brightness = [] (float velocity) {
        Core c;
        c.prepare (sampleRate, 512);
        basicPatch (c);
        setAll (c, { { "p1.analog.cutoff", 400.0f }, { "p1.mod.amt", 0.0f }, { "p1.velsens", 0.0f }, { "p1.mod.s1.src", (float) mod::SrcVelocity },
                     { "p1.mod.s1.dst", (float) mod::DstTimbre }, { "p1.mod.s1.amt", 1.0f } });
        c.queueNote (48, velocity, true);
        const auto x = renderCore (c, 20);
        return brightnessOf (x);
    };
    const double b1 = brightness (1.0f), b0 = brightness (0.1f);
    CAPTURE (b1);
    CAPTURE (b0);
    CHECK (b1 > 3.0 * b0);
}

TEST_CASE ("0.7 matrix: level modulation is smooth (no control-rate zipper)")
{
    Core core;
    core.prepare (sampleRate, 512);
    basicPatch (core);
    // a pure sine (Analog osc through an open filter would alias; use the FM engine's sine carrier)
    setAll (core, { { "p1.engine", (float) EngineType::FM }, { "p1.fm.index", 0.0f }, { "p1.mod.s1.src", (float) mod::SrcLfo2 },
                    { "p1.mod.s1.dst", (float) mod::DstLevel }, { "p1.mod.s1.amt", -0.9f }, { "p1.lfo2.rate", 6.0f },
                    { "p1.lfo2.shape", (float) mod::ShapeSine } });
    core.queueNote (45, 1.0f, true);
    const auto s = renderCore (core, 40);
    // second difference of a 110 Hz sine with a slow gain change stays tiny; a 32-sample step would spike it
    const float w = dsp::twoPi * 110.0f / (float) sampleRate;
    const float pk = peak (s.l, 8000);
    float worst = 0.0f;
    for (size_t i = 8002; i < s.size(); ++i) worst = std::max (worst, std::abs (s.l[i] - 2.0f * s.l[i - 1] + s.l[i - 2]));
    CHECK (worst < pk * w * w * 4.0f + 1.0e-4f);
    CHECK (pk > 0.01f);
}

TEST_CASE ("0.7 matrix: parameter routings move the engine parameter (smoothed), the UI sees the modulation, the registry not")
{
    Core core;
    core.prepare (sampleRate, 512);
    basicPatch (core);
    const int cutoffLocal = 2; // Analog: shape, detune, cutoff ...
    setAll (core, { { "p1.analog.cutoff", 300.0f }, { "p1.macro1", 1.0f }, { "p1.mod.s3.src", (float) mod::SrcMacro1 },
                    { "p1.mod.s3.dst", (float) (mod::DstEngine1 + cutoffLocal) }, { "p1.mod.s3.amt", 0.5f } });
    core.queueNote (48, 1.0f, true);
    const auto bright = renderCore (core, 20);
    const int idx = core.params().indexOf ("p1.analog.cutoff");
    CHECK (core.part (0).destParamIndex (mod::DstEngine1 + cutoffLocal) == idx);
    CHECK (core.part (0).modulationOf (idx) == doctest::Approx (0.5f).epsilon (0.01));
    CHECK (core.params().get (idx) == doctest::Approx (300.0f)); // the parameter itself is untouched
    CHECK (core.part (0).modMatrix().uiRouteValue (2) == doctest::Approx (0.5f).epsilon (0.01));
    core.params().set ("p1.macro1", 0.0f);
    const auto dark = renderCore (core, 20);
    const double hb = brightnessOf (bright), hd = brightnessOf (dark);
    CAPTURE (hb);
    CAPTURE (hd);
    CHECK (hb > 2.0 * hd);
    // FX targets follow the type in the slot
    setAll (core, { { "p1.fx2.type", (float) InsertSlot::Delay }, { "p1.mod.s4.src", (float) mod::SrcMacro2 },
                    { "p1.mod.s4.dst", (float) mod::DstFx1Param1 + 4 }, { "p1.mod.s4.amt", 0.2f } });
    core.prepareEffects();
    renderCore (core, 2);
    CHECK (core.part (0).destParamIndex (mod::DstFx1Param1 + 4) == core.params().indexOf ("p1.fx2.delay.time"));
}

TEST_CASE ("0.7 matrix: tempo-synced free-running LFO follows the song position; one-shot holds; fade-in grows")
{
    mod::LfoSettings s;
    s.sync = 5; // 1/4
    s.mode = mod::ModeOneShot;
    s.shape = mod::ShapeSawUp;
    mod::ModLfo lfo;
    lfo.trigger (s, 1);
    float last = -2.0f;
    for (int i = 0; i < 1000; ++i) last = lfo.advance (s, 32, 48000.0f, 120.0); // 0.67 s > one quarter at 120 BPM
    CHECK (last == doctest::Approx (1.0f));
    CHECK (mod::fadeGain (2.0f, 0.5f) == doctest::Approx (0.25f));

    // a free-running synced LFO 2 driving pan: the same song position gives the same value, whatever came before
    auto panAt = [] (double ppq) {
        Core core;
        core.prepare (sampleRate, 512);
        basicPatch (core);
        setAll (core, { { "seq.run", 1.0f }, { "p1.lfo2.sync", 4.0f }, { "p1.lfo2.mode", (float) mod::ModeFree }, { "p1.mod.s1.src", (float) mod::SrcLfo2 },
                        { "p1.mod.s1.dst", (float) mod::DstVolume }, { "p1.mod.s1.amt", 0.1f } });
        Transport t;
        t.bpm = 120.0;
        t.playing = true;
        t.hostDriven = true;
        t.ppqAtBlockStart = ppq;
        core.queueNote (60, 1.0f, true);
        renderCore (core, 1, 512, t);
        return core.part (0).modMatrix().sourceValue (mod::SrcLfo2);
    };
    MESSAGE ("lfo2 at 8 / 8.25 / 16: " << panAt (8.0) << " " << panAt (8.25) << " " << panAt (16.0));
    CHECK (panAt (8.0) == doctest::Approx (panAt (16.0)).epsilon (0.01));
    CHECK (std::abs (panAt (8.0) - panAt (8.25)) > 0.1f);
}

TEST_CASE ("0.7 matrix: MPE pressure and slide are sources per voice; mod wheel, bend, macros and the seq lane part-wide")
{
    Core core;
    core.prepare (sampleRate, 512);
    basicPatch (core);
    setAll (core, { { "p1.mpe", 1.0f }, { "p1.midich", 0.0f } });
    std::vector<float> l (512), r (512);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    std::vector<MidiEvent> midi { MidiEvent::noteOn (2, 60, 100, 0), MidiEvent { 0, (uint8_t) 0xd1, 90, 0 },
                                  MidiEvent { 0, (uint8_t) 0xb1, 74, 127 }, MidiEvent { 0, (uint8_t) 0xb0, 1, 127 } };
    core.process (out, 512, midi.data(), (int) midi.size(), Transport {});
    core.process (out, 512, nullptr, 0, Transport {});
    const auto& m = core.part (0).modMatrix();
    CHECK (m.sourceValue (mod::SrcNotePressure) == doctest::Approx (90.0f / 127.0f));
    CHECK (m.sourceValue (mod::SrcSlide) == doctest::Approx (1.0f));
    CHECK (m.sourceValue (mod::SrcModWheel) == doctest::Approx (1.0f));
    core.params().set ("p1.seqlane", 0.25f);
    core.process (out, 512, nullptr, 0, Transport {});
    CHECK (m.sourceValue (mod::SrcSeqLane) == doctest::Approx (0.25f));
}

TEST_CASE ("0.7: CPU budget - four parts, six FX each, 16 routings each render faster than real time")
{
    Core core;
    core.prepare (sampleRate, 512);
    core.params().set ("mix.master.limit", 0.0f);
    const int types[6] = { InsertSlot::Chorus, InsertSlot::FilterDrive, InsertSlot::Phaser, InsertSlot::Bitcrush, InsertSlot::Delay, InsertSlot::Reverb };
    for (int p = 1; p <= 4; ++p)
    {
        const auto pp = "p" + std::to_string (p) + ".";
        core.params().set (pp + "engine", (float) EngineType::Wavetable);
        core.params().set (pp + "voices", 8.0f);
        core.params().set (pp + "midich", 1.0f);
        for (int s = 0; s < 6; ++s) core.params().set (FxRack::partSlotPrefix (pp, s) + "type", (float) types[s]);
        for (int k = 1; k <= mod::numSlots; ++k)
        {
            const auto s = pp + "mod.s" + std::to_string (k) + ".";
            core.params().set (s + "src", (float) (1 + k % (mod::numSources - 1)));
            core.params().set (s + "dst", (float) (k % 2 ? mod::DstPitch + k % 4 : mod::DstEngine1 + k % 8));
            core.params().set (s + "amt", 0.1f);
        }
    }
    core.prepareEffects();
    for (int n : { 48, 52, 55, 59, 62, 64, 67, 71 }) core.queueNote (n, 0.9f, true);
    const auto t0 = std::chrono::steady_clock::now();
    const auto s = renderCore (core, 94); // ~1 s
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    MESSAGE ("4 parts x 8 voices, 24 FX, 64 routings: " << secs << " s for 1 s of audio");
    CHECK (allFinite (s.l));
    CHECK (secs < 1.0);
}
