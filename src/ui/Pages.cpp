// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pages.h"

#include <cmath>

#include <algorithm>
#include <cctype>

namespace ohpe::ui
{
namespace
{
using Ids = std::vector<std::string>;

/** Every registry ID that starts with `prefix`, in registry order. */
Ids withPrefix (const Core& core, const std::string& prefix)
{
    Ids ids;
    const auto& reg = core.params();
    for (int i = 0; i < reg.size(); ++i)
        if (reg.descriptor (i).id.compare (0, prefix.size(), prefix) == 0) ids.push_back (reg.descriptor (i).id);
    return ids;
}

/** Pads a list to whole pages of four. */
Ids padded (Ids ids)
{
    while (ids.size() % 4 != 0) ids.push_back ({});
    return ids;
}

std::string partPrefix (int part) { return "p" + std::to_string (juce::jlimit (0, Core::numParts - 1, part) + 1) + "."; }

/** FX slot parameters of the selected slot type of a part ("pN.fx.type" + that effect's block). */
Ids fxSlotParams (const Core& core, int part)
{
    const auto p = partPrefix (part);
    const auto& reg = core.params();
    const int typeIdx = reg.indexOf (p + "fx.type");
    if (typeIdx < 0) return {};
    Ids ids { p + "fx.type" };
    const auto& d = reg.descriptor (typeIdx);
    const int t = (int) std::lround (reg.get (typeIdx));
    if (t > 0 && t < (int) d.choices.size())
    {
        std::string seg;
        for (char c : d.choices[(size_t) t])
            if (c != ' ') seg += (char) std::tolower ((unsigned char) c);
        for (const auto& id : withPrefix (core, p + "fx." + seg + ".")) ids.push_back (id);
    }
    return ids;
}

std::string mixCh (int ch) { return "mix.ch" + std::to_string (juce::jlimit (0, 7, ch) + 1) + "."; }
std::string tapeTr (int t) { return "tape.tr" + std::to_string (juce::jlimit (0, 7, t) + 1) + "."; }
std::string pad (int p) { return "drum.pad" + std::to_string (juce::jlimit (0, 23, p) + 1) + "."; }
} // namespace

const char* seqViewName (SeqView v) noexcept
{
    switch (v)
    {
        case SeqView::Step: return "Step";
        case SeqView::Euclid: return "Euclid";
        case SeqView::Arp: return "Arp";
        case SeqView::Orbit: return "Orbit";
        case SeqView::Sync: return "Sync";
        default: return "";
    }
}

const ModeInfo& modeInfo (Mode m) noexcept
{
    static const ModeInfo table[numModes] = {
        { "Synth", Picto::Synth, true },     { "Envelope", Picto::Envelope, true }, { "LFO", Picto::Lfo, true },
        { "Mixer", Picto::Mixer, true },     { "Effects", Picto::Effects, true },   { "Sequencer", Picto::Sequencer, true },
        { "Tape", Picto::Tape, true },       { "Drum", Picto::Drum, true },
    };
    return table[juce::jlimit (0, numModes - 1, (int) m)];
}

EngineType selectedEngine (Core& core, int partIndex)
{
    auto& part = core.part (juce::jlimit (0, Core::numParts - 1, partIndex));
    const int t = (int) std::lround (core.params().get (part.id ("engine")));
    if (t >= 0 && t < numEngineTypes && part.engine ((EngineType) t) != nullptr)
        return (EngineType) t;
    return part.currentEngineType();
}

std::vector<std::string> modeParams (Mode m, Core& core, int ch)
{
    PageContext ctx;
    ctx.mixerChannel = ch;
    return modeParams (m, core, ctx);
}

std::vector<std::string> modeParams (Mode m, Core& core, const PageContext& ctx)
{
    const int partIndex = juce::jlimit (0, Core::numParts - 1, ctx.part);
    auto& part = core.part (partIndex);
    const std::string p = part.prefix();
    switch (m)
    {
        case Mode::Synth:
        {
            Ids ids;
            const auto type = selectedEngine (core, partIndex);
            if (auto* e = part.engine (type))
                for (const auto& d : e->getParamDescriptors())
                    ids.push_back (p + engineTypeInfo (type).id + "." + d.id);
            // 0.6: every engine ends with the portamento page and the pitch-envelope page
            ids = padded (ids);
            for (const char* k : { "glide", "glide.mode", "glide.rate", "polyglide", "pitchenv.amt", "pitchenv.time", "pitchenv.curve", "voices" })
                ids.push_back (p + k);
            return ids;
        }
        case Mode::Envelope:
            return { p + "amp.a", p + "amp.d", p + "amp.s", p + "amp.r", p + "mod.a", p + "mod.d", p + "mod.s", p + "mod.r",
                     p + "mod.amt", p + "velsens", p + "glide", p + "voices", p + "polyglide", "", "", "" };
        case Mode::Lfo:
            return { p + "lfo.rate", p + "lfo.depth", p + "lfo.shape", p + "lfo.dest", p + "tune", p + "fine", p + "bend", p + "vol",
                     p + "midich", p + "quant", p + "chord", "key.root", "key.scale", p + "mpe", p + "mpe.bend", p + "mpe.press",
                     p + "mpe.timbre", "", "", "" };
        case Mode::Mixer:
        {
            const std::string c = mixCh (ctx.mixerChannel);
            return { c + "vol", c + "pan", c + "senda", c + "sendb", c + "eqlo", c + "eqmid", c + "eqhi", c + "out",
                     c + "src", c + "mute", c + "solo", c + "comp", "mix.master.vol", "mix.returna", "mix.returnb", "mix.master.limit",
                     "mix.master.comp", "mix.master.comp.thresh", "mix.master.comp.ratio", "mix.master.comp.attack",
                     "mix.master.comp.release", "mix.master.comp.makeup", "mix.master.ceiling", "mix.master.tapesat",
                     "mix.master.lowpass", "", "", "" };
        }
        case Mode::Effects:
        {
            Ids ids { "fxa.delay.time", "fxa.delay.sync", "fxa.delay.feedback", "fxa.delay.tone",
                      "fxb.reverb.size", "fxb.reverb.damping", "fxb.reverb.predelay", "mix.returnb",
                      "fxa.delay.pingpong", "", "", "" };
            for (const auto& id : padded (fxSlotParams (core, partIndex))) ids.push_back (id);
            return ids;
        }
        case Mode::Sequencer:
            switch (ctx.seqView)
            {
                case SeqView::Euclid:
                {
                    // 0.5: settings, groove (velocity, swing, humanise, drums), the rings, the ring velocities
                    const std::string e = Core::Ids::euclidPrefix;
                    Ids ids { e + "on", e + "part", e + "rate", e + "gate", e + "vel", e + "swing", e + "human", e + "drums" };
                    for (int r = 1; r <= 4; ++r)
                        for (const char* k : { "steps", "pulses", "rot", "note" }) ids.push_back (e + "r" + std::to_string (r) + "." + k);
                    for (int r = 1; r <= 4; ++r) ids.push_back (e + "r" + std::to_string (r) + ".vel");
                    return ids;
                }
                case SeqView::Arp: return padded (withPrefix (core, Core::Ids::arpPrefix));
                case SeqView::Orbit:
                {
                    // 0.5: settings, groove (swing, humanise), then each lane with its target part
                    const std::string o = Core::Ids::orbitPrefix;
                    Ids ids { o + "on", o + "part", o + "gate", o + "drums", o + "swing", o + "human", "", "" };
                    for (int l = 1; l <= 4; ++l)
                        for (const char* k : { "len", "rate", "transpose", "part" }) ids.push_back (o + "l" + std::to_string (l) + "." + k);
                    return ids;
                }
                case SeqView::Sync:
                    return { Core::Ids::tempo, Core::Ids::seqRun, Core::Ids::syncSource, Core::Ids::syncSmoothing,
                             Core::Ids::clockOut, Core::Ids::notesOut, "", "" };
                case SeqView::Step:
                case SeqView::numViews: break;
            }
            return { "seq.step.length", "seq.step.rate", "seq.step.swing", "seq.step.transpose", "tempo", "seq.run", "seq.step.channel",
                     "seq.step.pattern", "seq.step.song", "", "", "" };
        case Mode::Tape:
        {
            const std::string t = tapeTr (ctx.tapeTrack);
            return { "tape.play", "tape.rec", "tape.speed", "tape.reverse", t + "arm", t + "src", t + "level", t + "out",
                     "tape.loop", "tape.loopin", "tape.loopout", "tape.overdub", "tape.sat", "tape.wow", "tape.flutter", "tape.hiss" };
        }
        case Mode::Drum:
        {
            const std::string d = pad (ctx.drumPad);
            return { d + "type", d + "tune", d + "decay", d + "tone", d + "level", d + "pan", d + "choke", d + "src",
                     "drum.vol", "drum.base", "", "" };
        }
        case Mode::numModes: break;
    }
    return {};
}

std::vector<std::string> pageTitles (Mode m, Core& core, const PageContext& ctx)
{
    const int pages = numPages (modeParams (m, core, ctx));
    std::vector<std::string> t;
    switch (m)
    {
        case Mode::Synth: t = { "Macros" }; break;
        case Mode::Envelope: t = { "Amp Envelope", "Mod Envelope", "Voice" }; break;
        case Mode::Lfo: t = { "LFO", "Pitch & Level", "MIDI & Key", "Scale & MPE", "MPE" }; break;
        case Mode::Mixer: t = { "Channel", "EQ & Output", "Source & Dynamics", "Master", "Master Compressor", "Master Output", "Master Filter" }; break;
        case Mode::Effects: t = { "Delay (Send A)", "Reverb (Send B)", "Delay (Send A)", "FX Slot" }; break;
        case Mode::Sequencer:
            switch (ctx.seqView)
            {
                case SeqView::Step: t = { "Pattern", "Transport", "Song" }; break;
                case SeqView::Euclid: t = { "Euclid", "Groove", "Ring 1", "Ring 2", "Ring 3", "Ring 4", "Ring Velocity" }; break;
                case SeqView::Arp: t = { "Arp", "Arp" }; break;
                case SeqView::Orbit: t = { "Orbit", "Groove", "Lane 1", "Lane 2", "Lane 3", "Lane 4" }; break;
                case SeqView::Sync: t = { "Tempo & Sync", "MIDI Out" }; break;
                case SeqView::numViews: break;
            }
            break;
        case Mode::Tape: t = { "Transport", "Track", "Loop", "Tape Character" }; break;
        case Mode::Drum: t = { "Pad", "Pad Mix", "Kit" }; break;
        case Mode::numModes: break;
    }
    while ((int) t.size() < pages) t.push_back (t.empty() ? std::string ("Page") : t.back());
    if (m == Mode::Synth)
    {
        for (size_t i = 1; i < t.size(); ++i) t[i] = "Engine";
        if (t.size() >= 3) { t[t.size() - 2] = "Glide"; t[t.size() - 1] = "Pitch Envelope"; } // 0.6
    }
    if (m == Mode::Effects)
        for (size_t i = 4; i < t.size(); ++i) t[i] = "FX Slot";
    t.resize ((size_t) pages);
    return t;
}

std::vector<ParamGroup> proGroups (Mode m, Core& core, const PageContext& ctx)
{
    std::vector<ParamGroup> groups;
    auto addPages = [&] (Mode mode, const PageContext& c) {
        const auto ids = modeParams (mode, core, c);
        const auto titles = pageTitles (mode, core, c);
        for (size_t pg = 0; pg * 4 < ids.size(); ++pg)
        {
            ParamGroup g;
            g.title = titles[pg];
            for (size_t k = pg * 4; k < juce::jmin (ids.size(), pg * 4 + 4); ++k)
                if (! ids[k].empty()) g.ids.push_back (ids[k]);
            if (g.ids.empty()) continue;
            // consecutive pages with the same title merge into one group
            if (! groups.empty() && groups.back().title == g.title && groups.back().number == 0)
                groups.back().ids.insert (groups.back().ids.end(), g.ids.begin(), g.ids.end());
            else
                groups.push_back (std::move (g));
        }
    };
    switch (m)
    {
        case Mode::Tape:
        {
            groups.push_back ({ "Transport", 0, { "tape.play", "tape.rec", "tape.speed", "tape.reverse", "tape.overdub", Core::Ids::tapeEditTrack } });
            groups.push_back ({ "Loop", 0, { "tape.loop", "tape.loopin", "tape.loopout" } });
            groups.push_back ({ "Tape Character", 0, { "tape.sat", "tape.wow", "tape.flutter", "tape.hiss" } });
            for (int t = 0; t < TapeMachine::numTracks; ++t)
            {
                const auto tr = tapeTr (t);
                groups.push_back ({ "Track", t + 1, { tr + "arm", tr + "src", tr + "level", tr + "out" } });
            }
            return groups;
        }
        case Mode::Drum:
        {
            groups.push_back ({ "Kit", 0, { "drum.vol", "drum.base" } });
            for (int i = 0; i < 24; ++i)
            {
                const auto d = pad (i);
                groups.push_back ({ "Pad", i + 1, { d + "type", d + "tune", d + "decay", d + "tone", d + "level", d + "pan", d + "choke", d + "src" } });
            }
            return groups;
        }
        case Mode::Sequencer:
        {
            auto c = ctx;
            if (ctx.seqView == SeqView::Euclid)
            {
                groups.push_back ({ "Euclid", 0, { "seq.euclid.on", "seq.euclid.part", "seq.euclid.rate", "seq.euclid.gate", "seq.euclid.vel",
                                                   "seq.euclid.swing", "seq.euclid.human", "seq.euclid.drums" } });
                for (int r = 1; r <= 4; ++r)
                {
                    const auto rp = "seq.euclid.r" + std::to_string (r) + ".";
                    groups.push_back ({ "Ring", r, { rp + "steps", rp + "pulses", rp + "rot", rp + "note", rp + "vel" } });
                }
                return groups;
            }
            if (ctx.seqView == SeqView::Orbit)
            {
                groups.push_back ({ "Orbit", 0, { "seq.orbit.on", "seq.orbit.part", "seq.orbit.gate", "seq.orbit.swing", "seq.orbit.human",
                                                  "seq.orbit.drums" } });
                for (int l = 1; l <= 4; ++l)
                {
                    const auto lp = "seq.orbit.l" + std::to_string (l) + ".";
                    groups.push_back ({ "Lane", l, { lp + "len", lp + "rate", lp + "transpose", lp + "part" } });
                }
                return groups;
            }
            addPages (m, c);
            return groups;
        }
        case Mode::Synth:
        {
            // the whole voice of the part: engine, envelopes, LFO and pitch
            const auto ids = modeParams (Mode::Synth, core, ctx);
            ParamGroup macros { "Macros", 0, {} }, engine { "Engine", 0, {} };
            for (size_t i = 0; i < ids.size(); ++i)
                if (! ids[i].empty()) (i < 4 ? macros : engine).ids.push_back (ids[i]);
            if (! macros.ids.empty()) groups.push_back (macros);
            if (! engine.ids.empty()) groups.push_back (engine);
            addPages (Mode::Envelope, ctx);
            addPages (Mode::Lfo, ctx);
            return groups;
        }
        default: addPages (m, ctx); return groups;
    }
}
} // namespace ohpe::ui
