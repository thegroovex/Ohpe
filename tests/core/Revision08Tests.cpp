// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.8 (sound revision 3): the new insert effects (shaper, squash, comb, formant, flanger, wobble, tremolo,
// width), FX slots 2 / 3 of every part and slot 1 of parts 2-4, the layer engine with cross-modulation,
// the Wavetable unison stack / FM, FM velocity > index, the keyboard instruments, Hybrid / Piano categories.

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "engines/FmEngine.h"
#include "engines/WavetableEngine.h"
#include "fx/InsertEffects2.h"
#include "fx/InsertSlot.h"
#include "midi/MpeInput.h"
#include "preset/Preset.h"
#include "sample/FactoryBank.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <vector>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
Stereo signal (size_t n, float amp = 0.5f)
{
    Stereo s (n);
    dsp::Random rng (5);
    for (size_t i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sampleRate;
        s.l[i] = amp * (std::sin (dsp::twoPi * 55.0f * t) + 0.5f * std::sin (dsp::twoPi * 440.0f * t)) + 0.05f * rng.nextBipolar();
        s.r[i] = amp * (std::sin (dsp::twoPi * 55.0f * t) + 0.5f * std::sin (dsp::twoPi * 660.0f * t)) + 0.05f * rng.nextBipolar();
    }
    return s;
}

void runFx (IEffect& e, Stereo& s, int block = 256)
{
    for (size_t pos = 0; pos < s.size(); pos += (size_t) block)
        e.process (s.l.data() + pos, s.r.data() + pos, (int) std::min<size_t> ((size_t) block, s.size() - pos));
}

std::vector<std::unique_ptr<IEffect>> newEffects()
{
    std::vector<std::unique_ptr<IEffect>> v;
    v.push_back (std::make_unique<ShaperEffect>());
    v.push_back (std::make_unique<SquashEffect>());
    v.push_back (std::make_unique<CombEffect>());
    v.push_back (std::make_unique<FormantEffect>());
    v.push_back (std::make_unique<FlangerEffect>());
    v.push_back (std::make_unique<WobbleEffect>());
    v.push_back (std::make_unique<TremoloEffect>());
    v.push_back (std::make_unique<WidthEffect>());
    return v;
}

struct Rig
{
    Core core;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    Stereo rec;

    Rig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
        core.params().set ("mix.master.limit", 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE (core.params().set (id, v)); }
    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, {});
            rec.l.insert (rec.l.end(), l[0].begin(), l[0].end());
            rec.r.insert (rec.r.end(), r[0].begin(), r[0].end());
        }
    }
};
} // namespace

TEST_CASE ("0.8: the new insert effects stay finite and denormal-free at extreme settings, their tails decay")
{
    for (int corner = 0; corner < 3; ++corner)
        for (auto& e : newEffects())
        {
            CAPTURE (e->info().id);
            CAPTURE (corner);
            e->prepare (sampleRate, 256);
            const auto d = e->getParamDescriptors();
            for (size_t i = 0; i < d.size(); ++i)
                e->setParam ((int) i, corner == 0 ? d[i].minValue : corner == 1 ? d[i].maxValue : d[i].defaultValue);
            e->setTempo (174.0);
            e->noteOn();
            Stereo s = signal (24000, 0.8f);
            s.l.resize (24000 + 96000, 0.0f);
            s.r.resize (24000 + 96000, 0.0f);
            runFx (*e, s);
            CHECK (allFinite (s.l));
            CHECK (allFinite (s.r));
            CHECK (noDenormals (s.l));
            CHECK (noDenormals (s.r));
            CHECK (peak (s.l, 0, 24000) < 64.0f);
            // silent two seconds after the input stopped (a 20 Hz comb at full feedback rings longer, as it should)
            if (! (std::string (e->info().id) == "comb" && corner == 0)) CHECK (peak (s.l, s.size() - 4800) < 1.0e-3f);
        }
}

TEST_CASE ("0.8: squash at depth 0 gives the input back (exact band split); depth pulls loud bands down and lifts quiet ones")
{
    SquashEffect sq;
    sq.prepare (sampleRate, 256);
    sq.setParam (SquashEffect::Depth, 0.0f);
    const Stereo in = signal (48000);
    Stereo s = in;
    runFx (sq, s);
    float err = 0.0f;
    for (size_t i = 0; i < s.size(); ++i) err = std::max (err, std::abs (s.l[i] - in.l[i]));
    CHECK (err < 1.0e-4f);

    CHECK (SquashEffect::bandGainDb (-6.0f, 1.0f, 1.0f) < -12.0f);
    CHECK (SquashEffect::bandGainDb (-50.0f, 1.0f, 1.0f) > 6.0f);
    CHECK (SquashEffect::bandGainDb (-30.0f, 1.0f, 1.0f) == doctest::Approx (0.0f));
    CHECK (SquashEffect::bandGainDb (-100.0f, 1.0f, 1.0f) == doctest::Approx (0.0f)); // silence is never lifted
    CHECK (SquashEffect::bandGainDb (-50.0f, 1.0f, 0.0f) == doctest::Approx (0.0f));

    // a quiet signal comes out louder, a loud one quieter
    SquashEffect a, b;
    for (auto* e : { &a, &b })
    {
        e->prepare (sampleRate, 256);
        e->setParam (SquashEffect::Depth, 1.0f);
    }
    Stereo quiet = signal (48000, 0.01f), loud = signal (48000, 0.9f);
    const float qIn = rms (quiet.l, 24000), lIn = rms (loud.l, 24000);
    runFx (a, quiet);
    runFx (b, loud);
    CHECK (rms (quiet.l, 24000) > 1.5f * qIn);
    CHECK (rms (loud.l, 24000) < 0.7f * lIn);
}

TEST_CASE ("0.8: width keeps the mono sum and removes the side signal below Mono Below")
{
    WidthEffect w;
    w.prepare (sampleRate, 256);
    w.setParam (WidthEffect::Width, 2.0f);
    w.setParam (WidthEffect::Spread, 1.0f);
    w.setParam (WidthEffect::MonoBelow, 150.0f);
    const Stereo in = signal (48000);
    Stereo s = in;
    runFx (w, s);
    float err = 0.0f;
    for (size_t i = 0; i < s.size(); ++i) err = std::max (err, std::abs ((s.l[i] + s.r[i]) - (in.l[i] + in.r[i])));
    CHECK (err < 1.0e-4f); // mono-compatible: L + R untouched

    // a 50 Hz side-only signal is removed
    WidthEffect m;
    m.prepare (sampleRate, 256);
    m.setParam (WidthEffect::MonoBelow, 200.0f);
    Stereo side (48000);
    for (size_t i = 0; i < side.size(); ++i)
    {
        const float v = 0.5f * std::sin (dsp::twoPi * 50.0f * (float) i / (float) sampleRate);
        side.l[i] = v;
        side.r[i] = -v;
    }
    runFx (m, side);
    CHECK (rms (side.l, 24000) < 0.05f * 0.3535f);

    // defaults: (almost) unchanged
    WidthEffect d;
    d.prepare (sampleRate, 256);
    Stereo t = in;
    runFx (d, t);
    float e2 = 0.0f;
    for (size_t i = 0; i < t.size(); ++i) e2 = std::max (e2, std::abs (t.l[i] - in.l[i]));
    CHECK (e2 < 1.0e-5f);
}

TEST_CASE ("0.8: shaper curves are bounded, the synced wobble restarts with a note")
{
    for (int c = 0; c < ShaperEffect::numCurves; ++c)
        for (float x = -30.0f; x <= 30.0f; x += 0.37f)
        {
            const float y = ShaperEffect::shape (c, x);
            CHECK (std::isfinite (y));
            CHECK (std::abs (y) <= 1.6f);
        }
    CHECK (fxsync::rateHz (4, 1.0f, 120.0) == doctest::Approx (4.0f)); // 1/8 at 120 BPM = 4 Hz
    CHECK (fxsync::rateHz (0, 3.0f, 120.0) == doctest::Approx (3.0f));

    WobbleEffect w;
    w.prepare (sampleRate, 256);
    w.setParam (WobbleEffect::Sync, 3.0f); // 1/4
    w.setTempo (140.0);
    Stereo a = signal (9600), b = signal (9600);
    w.noteOn();
    runFx (w, a);
    w.reset();
    w.noteOn();
    runFx (w, b);
    for (size_t i = 0; i < a.size(); ++i) REQUIRE (a.l[i] == b.l[i]);
}

TEST_CASE ("0.8: every part has an FX slot with every type; the 0.8 types are appended")
{
    Core core;
    const auto& reg = core.params();
    for (int p = 1; p <= Core::numParts; ++p)
    {
        const auto n = "p" + std::to_string (p) + ".";
        REQUIRE (reg.indexOf (n + "fx.type") >= 0);
        CHECK (reg.descriptor (reg.indexOf (n + "fx.type")).choices.size() == (size_t) InsertSlot::numTypes);
        for (const char* id : { "shaper.drive", "squash.depth", "squash.monolow", "comb.freq", "formant.vowel", "flanger.rate",
                                "wobble.sync", "tremolo.depth", "width.mono", "chorus.rate", "reverb.mix", "delay.mix" })
            CHECK (reg.indexOf (n + "fx." + id) >= 0);
        for (const char* id : { "layer.engine", "layer.xmod", "wavetable.univoices", "wavetable.fm", "fm.velidx", "sampler.inst2",
                                "grain.inst2", "mpe.pressamt", "mpe.timbreamt", "mpe.smooth", "mpe.slide" })
            CHECK (reg.indexOf (n + id) >= 0);
        CHECK (reg.indexOf (n + "fx2.type") < 0); // no second chain: one slot per part
    }
    // the effect IDs match the English type names (the UI derives the block from them)
    const auto& d = reg.descriptor (reg.indexOf ("p1.fx.type"));
    InsertSlot slot;
    for (int t = 1; t < InsertSlot::numTypes; ++t)
    {
        std::string seg;
        for (char c : d.choices[(size_t) t])
            if (c != ' ') seg += (char) std::tolower ((unsigned char) c);
        REQUIRE (slot.effect (t) != nullptr);
        CHECK (seg == slot.effect (t)->info().id);
    }
}

TEST_CASE ("0.8: the FX slot of parts 2-4 works; Off is an exact bypass")
{
    auto render = [] (int type, int part) {
        Rig rig;
        const auto n = "p" + std::to_string (part + 1) + ".";
        rig.set (n + "engine", (float) EngineType::Analog);
        rig.set (n + "fx.type", (float) type);
        if (type == InsertSlot::Squash) { rig.set (n + "fx.squash.depth", 1.0f); rig.set (n + "fx.squash.monolow", 1.0f); }
        rig.run (80, { MidiEvent::noteOn (part + 1, 48, 110), MidiEvent::noteOn (part + 1, 55, 100) });
        return rig.rec;
    };
    for (int part : { 0, 2 })
    {
        CAPTURE (part);
        const auto off = render (0, part);
        CHECK (rms (off.l, 4000) > 0.001f);
        for (int t : { (int) InsertSlot::Shaper, (int) InsertSlot::Squash, (int) InsertSlot::Wobble, (int) InsertSlot::Tremolo })
        {
            CAPTURE (t);
            const auto on = render (t, part);
            CHECK (allFinite (on.l));
            double diff = 0.0;
            for (size_t i = 0; i < on.size(); ++i) diff += std::abs (on.l[i] - off.l[i]);
            CHECK (diff > 0.1);
        }
    }
}

TEST_CASE ("0.8: the layer engine plays with the main engine, tuned, and cross-modulates it; Off changes nothing")
{
    auto render = [] (int layer, float level, int xmode, float xmod, int tune = 0) {
        Rig rig;
        rig.set ("p1.engine", (float) EngineType::Wavetable);
        rig.set ("p1.layer.engine", (float) layer);
        rig.set ("p1.layer.level", level);
        rig.set ("p1.layer.tune", (float) tune);
        rig.set ("p1.layer.xmode", (float) xmode);
        rig.set ("p1.layer.xmod", xmod);
        rig.run (60, { MidiEvent::noteOn (1, 45, 110) });
        return rig.rec;
    };
    const auto plain = render (0, 0.7f, 0, 0.0f);
    const auto zero = render (1 + (int) EngineType::Analog, 0.0f, 0, 0.0f);
    for (size_t i = 0; i < plain.size(); ++i) REQUIRE (plain.l[i] == zero.l[i]); // level 0: the layer adds nothing
    const auto same = render (1 + (int) EngineType::Wavetable, 0.7f, 0, 0.5f);  // the main engine as layer = off
    for (size_t i = 0; i < plain.size(); ++i) REQUIRE (plain.l[i] == same.l[i]);

    const auto layered = render (1 + (int) EngineType::Analog, 0.7f, 0, 0.0f);
    CHECK (rms (layered.l, 4000) > rms (plain.l, 4000) * 1.05f);
    for (int x = 0; x < Part::numXMods; ++x)
    {
        CAPTURE (x);
        const auto mod = render (1 + (int) EngineType::FM, 0.8f, x, 1.0f, 7);
        CHECK (allFinite (mod.l));
        CHECK (noDenormals (mod.l));
        CHECK (peak (mod.l) < 4.0f);
        double diff = 0.0;
        const auto ref = render (1 + (int) EngineType::FM, 0.8f, x, 0.0f, 7);
        for (size_t i = 0; i < mod.size(); ++i) diff += std::abs (mod.l[i] - ref.l[i]);
        CHECK (diff > 1.0);
    }
}

TEST_CASE ("0.8: Wavetable unison stack spreads in stereo, 3 voices = the classic unison; FM amount adds harmonics")
{
    auto render = [] (int voices, float fm) {
        WavetableEngine e;
        e.prepare (sampleRate, 256);
        e.setParam (wavetable::Unison, 0.6f);
        e.setParam (wavetable::UniVoices, (float) voices);
        e.setParam (wavetable::UniWidth, 1.0f);
        e.setParam (wavetable::Fm, fm);
        e.setParam (wavetable::Tone, 1.0f);
        VoiceCommon c;
        e.setCommon (c);
        e.noteOn (40, 1.0f);
        return renderEngine (e, 24000);
    };
    auto side = [] (const Stereo& s) {
        std::vector<float> d (s.size());
        for (size_t i = 0; i < s.size(); ++i) d[i] = s.l[i] - s.r[i];
        return rms (d, 4000);
    };
    const auto stack = render (7, 0.0f), single = render (1, 0.0f), classic = render (3, 0.0f);
    CHECK (allFinite (stack.l));
    CHECK (side (single) < 1.0e-4f);
    CHECK (side (stack) > 0.02f);
    CHECK (rms (classic.l, 4000) > 0.01f);
    const auto fm = render (1, 0.8f);
    CHECK (allFinite (fm.l));
    double diff = 0.0;
    for (size_t i = 0; i < fm.size(); ++i) diff += std::abs (fm.l[i] - single.l[i]);
    CHECK (diff > 10.0);
}

TEST_CASE ("0.8: FM velocity > index brightens hard notes only when set")
{
    auto render = [] (float velIdx, float vel) {
        FmEngine e;
        e.prepare (sampleRate, 256);
        e.setParam (fm::VelocityIndex, velIdx);
        VoiceCommon c;
        c.velocitySens = 0.0f;
        e.setCommon (c);
        e.noteOn (60, vel);
        return renderEngine (e, 4800);
    };
    const auto a = render (0.0f, 0.2f), b = render (0.0f, 1.0f);
    for (size_t i = 0; i < a.size(); ++i) REQUIRE (a.l[i] == b.l[i]);
    const auto soft = render (1.0f, 0.2f), hard = render (1.0f, 1.0f);
    double diff = 0.0;
    for (size_t i = 0; i < soft.size(); ++i) diff += std::abs (soft.l[i] - hard.l[i]);
    CHECK (diff > 1.0);
}

TEST_CASE ("0.8: keyboard instruments and the Hybrid / Piano categories are appended")
{
    CHECK (factory::numInstruments == 15);
    CHECK (factory::instrumentNamesEn().size() == (size_t) factory::numClassicInstruments);
    CHECK (factory::keyboardChoiceEn().size() == (size_t) (factory::numInstruments - factory::numClassicInstruments + 1));
    CHECK (std::string (factory::instrumentId (11)) == "grandpiano");
    CHECK (factory::effectiveInstrument (3, 0) == 3);
    CHECK (factory::effectiveInstrument (3, 2) == 12);
    const auto& cats = preset::categories();
    REQUIRE (cats.size() == 13);
    CHECK (std::string (cats[11].key) == "Hybrid");
    CHECK (std::string (cats[12].key) == "Piano");
}

// ---------------------------------------------------------------- 0.8 MPE / aftertouch
namespace
{
WavetableEngine& sineWt (WavetableEngine& e, float slide, float smooth)
{
    e.prepare (sampleRate, 256);
    e.setParam (wavetable::Position, 0.0f);
    e.setParam (wavetable::Unison, 0.0f);
    e.setParam (wavetable::UniVoices, 1.0f);
    e.setParam (wavetable::Tone, 1.0f);
    VoiceCommon c;
    c.exprSlide = slide;
    c.exprSmooth = smooth;
    c.velocitySens = 0.0f;
    e.setCommon (c);
    return e;
}

/** Sum of absolute sample differences: a brightness proxy. */
double roughness (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0.0;
    for (size_t i = from + 1; i < std::min (to, v.size()); ++i) s += std::abs (v[i] - v[i - 1]);
    return s;
}
} // namespace

TEST_CASE ("0.8 MPE: per-note bends glide over the slide time (at least 2 ms, never a zipper step) and arrive exactly")
{
    for (float slide : { 0.0f, 0.15f })
    {
        CAPTURE (slide);
        WavetableEngine e;
        sineWt (e, slide, 0.0f);
        e.noteOn (57, 1.0f); // 220 Hz
        NoteExpression start;
        e.noteExpression (57, start);
        auto pre = renderEngine (e, 4800);
        CHECK (measureFrequency (pre.l, sampleRate, 960, 4800) == doctest::Approx (220.0).epsilon (0.01));
        NoteExpression up;
        up.pitch = 12.0f; // one octave
        e.noteExpression (57, up);
        auto first = renderEngine (e, 1440);   // first 30 ms
        auto later = renderEngine (e, 48000);  // then one second
        const double fEarly = measureFrequency (first.l, sampleRate, 0, 1440);
        const double fLate = measureFrequency (later.l, sampleRate, 38400, 48000);
        CHECK (fLate == doctest::Approx (440.0).epsilon (0.01));
        if (slide > 0.0f) CHECK (fEarly < 400.0); // still on its way after 30 ms
        CHECK (allFinite (first.l));
        CHECK (allFinite (later.l));
    }
}

TEST_CASE ("0.8 MPE: a note started with a bend starts at that pitch (no slide in)")
{
    WavetableEngine e;
    sineWt (e, 0.3f, 0.0f);
    e.noteOn (57, 1.0f);
    NoteExpression x;
    x.pitch = 7.0f;
    e.noteExpression (57, x);
    auto s = renderEngine (e, 2400);
    CHECK (measureFrequency (s.l, sampleRate, 240, 2400) == doctest::Approx (220.0 * std::pow (2.0, 7.0 / 12.0)).epsilon (0.01));
}

TEST_CASE ("0.8 MPE: pressure and timbre reach the new targets with their amounts; channel and poly aftertouch too")
{
    auto render = [] (bool mpe, int target, float amount, int pressure, bool poly) {
        Rig rig;
        rig.set ("p1.engine", (float) EngineType::Analog);
        rig.set ("p1.analog.cutoff", 400.0f);
        rig.set ("p1.analog.reso", 0.0f);
        rig.set ("p1.mpe", mpe ? 1.0f : 0.0f);
        rig.set ("p1.mpe.press", (float) target);
        rig.set ("p1.mpe.pressamt", amount);
        const int ch = mpe ? 2 : 1;
        std::vector<MidiEvent> ev { MidiEvent::noteOn (ch, 48, 100) };
        MidiEvent at;
        if (poly) { at.status = (uint8_t) (0xa0 | (ch - 1)); at.data1 = 48; at.data2 = (uint8_t) pressure; }
        else { at.status = (uint8_t) (0xd0 | (ch - 1)); at.data1 = (uint8_t) pressure; at.data2 = 0; }
        ev.push_back (at);
        rig.run (40, ev);
        return rig.rec;
    };
    for (bool mpe : { true, false })
        for (bool poly : { false, true })
        {
            CAPTURE (mpe);
            CAPTURE (poly);
            const auto none = render (mpe, MpeInput::Cutoff, 1.0f, 0, poly);
            const auto open = render (mpe, MpeInput::Cutoff, 1.0f, 127, poly);
            const auto closed = render (mpe, MpeInput::Cutoff, -1.0f, 127, poly);
            const double r0 = roughness (none.l, 2400, none.size()), r1 = roughness (open.l, 2400, open.size()),
                         r2 = roughness (closed.l, 2400, closed.size());
            CHECK (r1 > 1.3 * r0); // pressure opens the filter
            CHECK (r2 < 0.8 * r0); // a negative amount closes it
        }
}

TEST_CASE ("0.8 MPE: every target in every engine stays finite at full pressure and timbre, with smoothing and slide")
{
    for (int engine = 0; engine < numEngineTypes; ++engine)
        for (int target = 1; target < MpeInput::numTargets; ++target)
        {
            CAPTURE (engine);
            CAPTURE (target);
            Rig rig;
            rig.set ("p1.engine", (float) engine);
            rig.set ("p1.layer.engine", (float) (1 + (engine + 1) % numEngineTypes));
            rig.set ("p1.mpe", 1.0f);
            rig.set ("p1.mpe.press", (float) target);
            rig.set ("p1.mpe.timbre", (float) (MpeInput::numTargets - target));
            rig.set ("p1.mpe.smooth", 0.05f);
            rig.set ("p1.mpe.slide", 0.1f);
            MidiEvent at;
            at.status = 0xd1; at.data1 = 127; at.data2 = 0;
            MidiEvent cc;
            cc.status = 0xb1; cc.data1 = 74; cc.data2 = 127;
            MidiEvent bend;
            bend.status = 0xe1; bend.data1 = 0x7f; bend.data2 = 0x7f;
            rig.run (24, { MidiEvent::noteOn (2, 60, 120), at, cc, bend });
            CHECK (allFinite (rig.rec.l));
            CHECK (allFinite (rig.rec.r));
            CHECK (noDenormals (rig.rec.l));
            CHECK (peak (rig.rec.l) < 8.0f);
        }
}

TEST_CASE ("0.8 MPE: works with portamento - a new note glides, its per-note bend adds on top")
{
    Rig rig;
    rig.set ("p1.engine", (float) EngineType::Wavetable);
    rig.set ("p1.wavetable.pos", 0.0f);
    rig.set ("p1.wavetable.unison", 0.0f);
    rig.set ("p1.wavetable.univoices", 1.0f);
    rig.set ("p1.wavetable.tone", 1.0f);
    rig.set ("p1.voices", 1.0f);
    rig.set ("p1.glide", 0.3f);
    rig.set ("p1.mpe", 1.0f);
    rig.run (40, { MidiEvent::noteOn (2, 45, 100) }); // 110 Hz
    const size_t start = rig.rec.size();
    rig.run (20, { MidiEvent::noteOn (3, 57, 100) }); // to 220 Hz, legato (the first is still held)
    const double mid = measureFrequency (rig.rec.l, sampleRate, start + 480, start + 2400);
    CHECK (mid > 112.0);
    CHECK (mid < 215.0);
    rig.run (200);
    const double end = measureFrequency (rig.rec.l, sampleRate, rig.rec.size() - 4800, rig.rec.size());
    CHECK (end == doctest::Approx (220.0).epsilon (0.02));
}
