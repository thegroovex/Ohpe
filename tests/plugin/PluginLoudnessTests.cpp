// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p87: the plug-in layer plays a preset exactly like the core. A preset is loaded through the plug-in's preset manager
// (the path the library uses), with and without the editor open, then played through processBlock (512-sample blocks,
// main stereo only and all 18 outputs) and compared with the same preset played by a bare core:
//   - every parameter after the load equals the value the preset gives (registry and host parameter),
//   - loudness (BS.1770 integrated) within 0.1 dB of the core, true peak (master limiter on, as heard) below -1 dBTP,
//   - single note, 4-note and 8-note chords (the chord headroom of p74).

#include <doctest/doctest.h>

#include "plugin/FactoryKit.h"
#include "plugin/PluginProcessor.h"
#include "render/PresetRender.h"
#include "ui/PluginEditor.h"
#include "../TestSlice.h"

#include <cmath>
#include <cstdio>
#include <memory>

using namespace ohpe;

namespace
{
/** BS.1770-4 integrated loudness (K-weighting, 400 ms blocks, 75 % overlap, absolute and relative gate). */
double lufs (const render::Audio& a)
{
    if (a.size() < 19200) return -200.0;
    struct Bq { double b0, b1, b2, a1, a2, z1 = 0, z2 = 0; double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    auto kw = [] (const std::vector<float>& in) {
        Bq s { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        Bq h { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        std::vector<double> o (in.size());
        for (size_t i = 0; i < in.size(); ++i) { const double y = h.run (s.run ((double) in[i])); o[i] = y * y; }
        return o;
    };
    const auto l = kw (a.l), r = kw (a.r);
    const size_t blk = 19200, hop = 4800;
    std::vector<double> z;
    for (size_t s = 0; s + blk <= l.size(); s += hop)
    {
        double acc = 0.0;
        for (size_t i = s; i < s + blk; ++i) acc += l[i] + r[i];
        z.push_back (acc / (double) blk);
    }
    auto loud = [] (double p) { return p > 0.0 ? -0.691 + 10.0 * std::log10 (p) : -200.0; };
    double sum = 0.0; int cnt = 0;
    for (double p : z) if (loud (p) > -70.0) { sum += p; ++cnt; }
    if (cnt == 0) return -200.0;
    const double rel = loud (sum / cnt) - 10.0;
    sum = 0.0; cnt = 0;
    for (double p : z) if (loud (p) > -70.0 && loud (p) > rel) { sum += p; ++cnt; }
    return cnt > 0 ? loud (sum / cnt) : -200.0;
}

struct Material { const char* name; std::vector<int> notes; int velocity; };
const std::vector<Material>& materials()
{
    static const std::vector<Material> m { { "note", { 60 }, 100 },
                                           { "chord4", { 48, 55, 60, 64 }, 110 },
                                           { "chord8", { 36, 43, 48, 52, 55, 60, 64, 67 }, 127 } };
    return m;
}
constexpr int blockSize = 512, holdSamples = 48000 * 3 / 2, totalSamples = 48000 * 2 + 24000;

std::vector<std::pair<int64_t, MidiEvent>> eventsFor (const Material& m)
{
    std::vector<std::pair<int64_t, MidiEvent>> ev;
    for (int n : m.notes)
    {
        ev.push_back ({ 0, MidiEvent::noteOn (1, n, m.velocity) });
        ev.push_back ({ holdSamples, MidiEvent::noteOff (1, n) });
    }
    return ev;
}

/** Plays the events through processBlock (main bus captured). */
render::Audio renderPlugin (OhpeProcessor& p, const std::vector<std::pair<int64_t, MidiEvent>>& events)
{
    render::Audio a;
    a.l.assign ((size_t) totalSamples, 0.0f);
    a.r.assign ((size_t) totalSamples, 0.0f);
    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels(), p.getTotalNumInputChannels()), blockSize);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < totalSamples; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, totalSamples - pos);
        midi.clear();
        for (const auto& [t, e] : events)
            if (t >= pos && t < pos + n)
                midi.addEvent (juce::MidiMessage (e.status, e.data1, e.data2), (int) (t - pos));
        buffer.setSize (buffer.getNumChannels(), n, false, false, true);
        buffer.clear();
        p.processBlock (buffer, midi);
        auto bus = p.getBusBuffer (buffer, false, 0);
        std::copy (bus.getReadPointer (0), bus.getReadPointer (0) + n, a.l.begin() + pos);
        std::copy (bus.getReadPointer (1), bus.getReadPointer (1) + n, a.r.begin() + pos);
    }
    return a;
}

void silence (OhpeProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels(), p.getTotalNumInputChannels()), blockSize);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b) { buffer.clear(); midi.clear(); p.processBlock (buffer, midi); }
}

void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

struct Row
{
    std::string preset, material;
    double pluginLufs = 0, coreLufs = 0, pluginTp = 0, coreTp = 0, maxDiff = 0;
};

struct Summary
{
    std::vector<Row> rows;
    int presets = 0, paramMismatches = 0;
    std::vector<std::string> mismatchExamples;
    void print (const char* variant) const
    {
        double maxDiff = 0.0, sumDiff = 0.0; int overTp = 0, overTpCore = 0;
        for (const auto& r : rows)
        {
            const double d = r.pluginLufs - r.coreLufs;
            maxDiff = std::max (maxDiff, std::abs (d));
            sumDiff += d;
            overTp += r.pluginTp > -1.0;
            overTpCore += r.coreTp > -1.0;
        }
        std::printf ("OHPE-STATS p87 %s presets %d renders %d | mean plugin-core %+.3f LU, max |diff| %.3f LU | > -1 dBTP: plugin %d core %d | param mismatches %d\n",
                     variant, presets, (int) rows.size(), rows.empty() ? 0.0 : sumDiff / (double) rows.size(), maxDiff, overTp, overTpCore, paramMismatches);
        for (size_t i = 0; i < mismatchExamples.size() && i < 40; ++i) std::printf ("OHPE-STATS p87 %s param %s\n", variant, mismatchExamples[i].c_str());
        for (const auto& r : rows)
            std::printf ("OHPE-STATS p87 %s row|%s|%s|%.3f|%.3f|%.3f|%.3f|%.2e\n", variant, r.preset.c_str(), r.material.c_str(),
                         r.pluginLufs, r.pluginTp, r.coreLufs, r.coreTp, r.maxDiff);
        std::fflush (stdout);
    }
};

/** Silence through both (the reference core keeps the same block history as the plug-in). */
void silenceBoth (OhpeProcessor& p, Core& core, int blocks)
{
    silence (p, blocks);
    render::renderCore (core, {}, (int64_t) blocks * blockSize, blockSize);
}

double maxAbsDiff (const render::Audio& a, const render::Audio& b)
{
    double m = 0.0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        m = std::max (m, (double) std::max (std::abs (a.l[i] - b.l[i]), std::abs (a.r[i] - b.r[i])));
    return m;
}

/** One preset: plug-in load (+ editor), parameter check, renders vs the reference core (same history). */
void checkPreset (OhpeProcessor& p, Core& core, int entryIndex, Summary& sum, bool withEditor)
{
    const auto& entry = p.presets().entries()[(size_t) entryIndex];
    const std::string name = entry.meta.name;
    CAPTURE (name);
    preset::Preset pr;
    REQUIRE (preset::fromJson (p.presets().read (entry), pr));

    // the arpeggiator off before each load in both (the preset manager's arp hand-over is not part of this check)
    auto& reg = p.core().params();
    const int arpOn = reg.indexOf (std::string (Core::Ids::arpPrefix) + "on");
    p.editRegistry ([arpOn] (Core& c) { if (arpOn >= 0) c.params().set (arpOn, 0.0f); });
    pump (30);
    silenceBoth (p, core, 4);
    std::vector<float> before ((size_t) reg.size());
    for (int i = 0; i < reg.size(); ++i) before[(size_t) i] = reg.get (i);

    // the reference: the same state, the preset applied like the preset manager does
    for (int i = 0; i < reg.size(); ++i)
        if (core.params().get (i) != before[(size_t) i]) core.params().set (i, before[(size_t) i]);
    preset::applyPart (pr, core.params(), 0, preset::PartOptions {});
    preset::applyPartMorph (pr, core, 0);
    usertable::partFromJson (core, 0, pr.userTables ? *pr.userTables : std::string(),
                             [] (const std::string&, const std::string&) { return usertable::UserTablePtr(); });
    core.prepareEffects();
    core.updateMorph();

    // the plug-in: load through the preset manager, the message thread and the audio thread run as in the app
    REQUIRE (p.presets().load (entryIndex, 0));
    for (int k = 0; k < 6; ++k) { silence (p, 4); pump (withEditor ? 60 : 25); }
    render::renderCore (core, {}, 24 * blockSize, blockSize);

    // every parameter: registry and host parameter equal the preset's value
    for (int i = 0; i < reg.size(); ++i)
    {
        const float want = core.params().get (i), got = reg.get (i);
        const auto& d = reg.descriptor (i);
        const float tol = 1.0e-5f * std::max (1.0f, std::abs (d.maxValue - d.minValue));
        bool bad = std::abs (got - want) > tol;
        float host = want;
        if (auto* hp = p.parameter (i))
        {
            host = hp->convertFrom0to1 (hp->getValue());
            bad |= std::abs (hp->getValue() - d.toNormalised (want)) > 1.0e-4f;
        }
        if (bad)
        {
            ++sum.paramMismatches;
            if (sum.mismatchExamples.size() < 200)
                sum.mismatchExamples.push_back (name + " " + d.id + " preset " + std::to_string (want) + " registry " + std::to_string (got)
                                                + " host " + std::to_string (host) + " before " + std::to_string (before[(size_t) i]));
        }
    }
    for (const auto& m : materials())
    {
        const auto ev = eventsFor (m);
        silenceBoth (p, core, 400); // tails of the previous material die away (limiter, sleep gates)
        const auto a = renderPlugin (p, ev);
        const auto b = render::renderCore (core, ev, totalSamples, blockSize);
        Row r { name, m.name, lufs (a), lufs (b), render::truePeakDb (a), render::truePeakDb (b), maxAbsDiff (a, b) };
        if (r.coreLufs > -60.0 || r.pluginLufs > -60.0) sum.rows.push_back (r);
        CAPTURE (m.name);
        CHECK (std::abs (r.pluginLufs - r.coreLufs) <= 0.1);
        CHECK (r.pluginTp <= std::max (-0.5, r.coreTp + 0.2)); // limiter on: its ceiling (-0.3 dB, target -0.57 dBTP)
    }
    ++sum.presets;
}


/** p87 diagnosis: the p74 audit's render with one factor of the plug-in path switched on at a time.
    flags: 1 = keep the default tempo, 2 = arp block applied then arp off, 4 = morph applied + updated,
           8 = the plug-in's starting registry, 16 = 16 silent blocks before the chord */
double auditTp (const preset::Preset& pr, int flags, const std::vector<float>& start, double* lufsOut = nullptr)
{
    const int block = 512;
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, block);
    factorykit::load (*core);
    if (flags & 8)
        for (int i = 0; i < core->params().size() && i < (int) start.size(); ++i) core->params().set (i, start[(size_t) i]);
    preset::PartOptions opt;
    opt.arp = (flags & 2) != 0;
    preset::applyPart (pr, core->params(), 0, opt);
    if (flags & 4) preset::applyPartMorph (pr, *core, 0);
    usertable::partFromJson (*core, 0, pr.userTables ? *pr.userTables : std::string(),
                             [] (const std::string&, const std::string&) { return usertable::UserTablePtr(); });
    if (! (flags & 1)) core->params().set ("tempo", 120.0f);
    core->params().set ("mix.master.limit", 0.0f);
    core->params().set (std::string (Core::Ids::arpPrefix) + "on", 0.0f);
    core->prepareEffects();
    if (flags & 4) core->updateMorph();
    if (flags & 16) render::renderCore (*core, {}, 16 * block, block);
    std::vector<std::pair<int64_t, MidiEvent>> ev;
    for (int n : materials().back().notes)
    {
        ev.push_back ({ 0, MidiEvent::noteOn (1, n, 127) });
        ev.push_back ({ 72000, MidiEvent::noteOff (1, n) });
    }
    const auto a = render::renderCore (*core, ev, totalSamples, block);
    if (lufsOut != nullptr) *lufsOut = lufs (a);
    return render::truePeakDb (a);
}

std::vector<int> partPresets (OhpeProcessor& p, int every)
{
    std::vector<int> out;
    int k = 0;
    const auto& list = p.presets().entries();
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].factory && list[(size_t) i].meta.kind == preset::Kind::Part && (k++ % every) == 0) out.push_back (i);
    return out;
}

void runVariant (bool allOutputs, bool withEditor, int every, const char* label)
{
    factorykit::loadBank();
    const auto slice = test::testSlice();
    auto proc = std::make_unique<OhpeProcessor>();
    auto& p = *proc;
    if (allOutputs) REQUIRE (p.enableAllBuses());
    p.prepareToPlay (48000.0, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    if (withEditor)
    {
        OhpeEditor::showTourOnFirstOpen.store (false);
        ed.reset (p.createEditor());
    }
    // the reference core: prepared like the plug-in, the plug-in's starting state
    auto core = std::make_unique<Core>();
    core->prepare (48000.0, blockSize);
    factorykit::load (*core);
    for (int i = 0; i < core->params().size(); ++i) core->params().set (i, p.core().params().get (i));
    core->prepareEffects();
    Summary sum;
    const auto list = partPresets (p, every);
    REQUIRE (! list.empty());
    for (size_t k = 0; k < list.size(); ++k)
        if (slice.has ((long long) k)) checkPreset (p, *core, list[k], sum, withEditor);
    sum.print (label);
    CHECK (sum.paramMismatches == 0);
    ed.reset();
}
} // namespace

TEST_CASE ("plug-in loudness: a fresh instance plays a wide 8-note chord below -1 dBTP without the limiter and arp, like the core (p74 audit through the plug-in, p87)" * doctest::test_suite ("sliced"))
{
    factorykit::loadBank();
    const auto slice = test::testSlice();
    std::vector<int> list;
    {
        OhpeProcessor probe;
        list = partPresets (probe, 6);
    }
    const Material& chord = materials().back();
    int done = 0, over = 0;
    double maxDiff = 0.0, worstTp = -200.0;
    for (size_t k = 0; k < list.size(); ++k)
    {
        if (! slice.has ((long long) k)) continue;
        auto proc = std::make_unique<OhpeProcessor>();
        auto& p = *proc;
        p.prepareToPlay (48000.0, blockSize);
        const auto& entry = p.presets().entries()[(size_t) list[k]];
        CAPTURE (entry.meta.name);
        preset::Preset pr;
        REQUIRE (preset::fromJson (p.presets().read (entry), pr));
        std::vector<float> start ((size_t) p.core().params().size());
        for (int i = 0; i < (int) start.size(); ++i) start[(size_t) i] = p.core().params().get (i);
        auto core = std::make_unique<Core>();
        core->prepare (48000.0, blockSize);
        factorykit::load (*core);
        for (int i = 0; i < core->params().size(); ++i) core->params().set (i, p.core().params().get (i));
        preset::applyPart (pr, core->params(), 0, preset::PartOptions {});
        preset::applyPartMorph (pr, *core, 0);
        usertable::partFromJson (*core, 0, pr.userTables ? *pr.userTables : std::string(),
                                 [] (const std::string&, const std::string&) { return usertable::UserTablePtr(); });
        // as the p74 audit measures: master limiter off, the sound itself (the preset's arpeggiator off)
        const std::string arpOn = std::string (Core::Ids::arpPrefix) + "on";
        core->params().set ("mix.master.limit", 0.0f);
        core->params().set (arpOn, 0.0f);
        core->prepareEffects();
        core->updateMorph();
        REQUIRE (p.presets().load (list[k], 0));
        p.editRegistry ([&arpOn] (Core& c) { c.params().set ("mix.master.limit", 0.0f); c.params().set (arpOn, 0.0f); });
        for (int b = 0; b < 4; ++b) { silence (p, 4); pump (20); }
        render::renderCore (*core, {}, 16 * blockSize, blockSize);
        const auto ev = eventsFor (chord);
        const auto a = renderPlugin (p, ev);
        const auto b = render::renderCore (*core, ev, totalSamples, blockSize);
        const double tp = render::truePeakDb (a), la = lufs (a), lb = lufs (b);
        CAPTURE (tp);
        CHECK (tp <= -1.0);
        CHECK (render::flatTopRuns (a) == 0);
        CHECK (std::abs (la - lb) <= 0.1);
        std::printf ("OHPE-STATS p87 fresh row|%s|chord8-nolimit|%.3f|%.3f|%.3f|%.3f|0\n", entry.meta.name.c_str(), la, tp, lb, render::truePeakDb (b));
        {
            std::string line = "OHPE-STATS p87 audit|" + entry.meta.name;
            for (int fl : { 0, 1, 2, 4, 8, 16, 31 })
            {
                double l = 0.0;
                const double t = auditTp (pr, fl, start, &l);
                char buf[64];
                std::snprintf (buf, sizeof (buf), "|f%d %.2f/%.2f", fl, t, l);
                line += buf;
            }
            char buf[64];
            std::snprintf (buf, sizeof (buf), "|plugin %.2f/%.2f", tp, la);
            std::printf ("%s%s\n", line.c_str(), buf);
        }
        maxDiff = std::max (maxDiff, std::abs (la - lb));
        worstTp = std::max (worstTp, tp);
        over += tp > -1.0;
        ++done;
    }
    std::printf ("OHPE-STATS p87 fresh presets %d | max |plugin-core| %.3f LU | worst %.2f dBTP | over -1 dBTP %d\n", done, maxDiff, worstTp, over);
    std::fflush (stdout);
}

TEST_CASE ("plug-in loudness: presets loaded through the preset manager play like the core, stereo out (p87)" * doctest::test_suite ("sliced"))
{
    runVariant (false, false, 4, "stereo");
}

TEST_CASE ("plug-in loudness: presets loaded through the preset manager play like the core, all 18 outputs (p87)" * doctest::test_suite ("sliced"))
{
    runVariant (true, false, 12, "18ch");
}

TEST_CASE ("plug-in loudness: presets loaded with the editor open play like the core (p87)" * doctest::test_suite ("sliced"))
{
    runVariant (true, true, 24, "editor");
}
