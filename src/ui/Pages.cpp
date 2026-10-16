// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pages.h"

#include "ModFxViews.h"
#include "SpaceView.h"

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
    const auto p = partPrefix (part) + "fx.";
    if (core.params().indexOf (p + "type") < 0) return {};
    Ids ids { p + "type" };
    for (const auto& id : modfx::slotEffectParams (core, p)) ids.push_back (id); // 0.7: the block of the type's effect ID
    return ids;
}

/** 0.8: the layer page of a part (layer engine, level, tuning, cross-modulation, own envelope). */
Ids layerParams (int part)
{
    const auto p = partPrefix (part) + "layer.";
    return { p + "engine", p + "level", p + "tune", p + "fine", p + "xmode", p + "xmod", p + "width", p + "ownenv",
             p + "a", p + "d", p + "s", p + "r" };
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
            if (core.params().indexOf (p + "layer.engine") >= 0) // 0.8: the layer engine pages
                for (const auto& id : layerParams (partIndex)) ids.push_back (id);
            return ids;
        }
        case Mode::Envelope:
            return { p + "amp.a", p + "amp.d", p + "amp.s", p + "amp.r", p + "mod.a", p + "mod.d", p + "mod.s", p + "mod.r",
                     p + "mod.amt", p + "velsens", p + "glide", p + "voices", p + "polyglide", "", "", "" };
        case Mode::Lfo:
            if (ctx.lfoView == modfx::LfoViewMod) return modfx::modParams (core, partIndex, ctx.modRouting); // 0.7
            return { p + "lfo.rate", p + "lfo.depth", p + "lfo.shape", p + "lfo.dest", p + "tune", p + "fine", p + "bend", p + "vol",
                     p + "midich", p + "quant", p + "chord", "key.root", "key.scale", p + "mpe", p + "mpe.bend", p + "mpe.press",
                     p + "mpe.timbre", p + "mpe.pressamt", p + "mpe.timbreamt", p + "mpe.smooth", p + "mpe.slide", "", "", "" }; // 0.8
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
            if (ctx.fxView == modfx::FxViewSpace) return space3d::spaceParams (partIndex); // 1.0: the 3D stage
            if (modfx::isRackView (ctx.fxView)) // 0.7: the part's FX rack / the master rack
                return modfx::rackParams (core, modfx::rackPrefixes (ctx.fxView, partIndex), ctx.fxSlot);
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
        case Mode::Lfo:
            t = ctx.lfoView == modfx::LfoViewMod ? modfx::modTitles (core, ctx.part, ctx.modRouting)
                                                 : std::vector<std::string> { "LFO", "Pitch & Level", "MIDI & Key", "Scale & MPE", "MPE" };
            break;
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
        const size_t layerPages = core.params().indexOf (core.part (juce::jlimit (0, Core::numParts - 1, ctx.part)).id ("layer.engine")) >= 0 ? 3 : 0;
        if (t.size() >= 3 + layerPages)
        {
            const size_t g = t.size() - layerPages;
            t[g - 2] = "Glide";
            t[g - 1] = "Pitch Envelope"; // 0.6
            for (size_t i = g; i < t.size(); ++i) t[i] = "Layer"; // 0.8
        }
    }
    if (m == Mode::Effects && ctx.fxView == modfx::FxViewSpace)
        t = space3d::spaceTitles();
    else if (m == Mode::Effects && modfx::isRackView (ctx.fxView))
        t = modfx::rackTitles (core, modfx::rackPrefixes (ctx.fxView, ctx.part), ctx.fxSlot);
    else if (m == Mode::Effects)
        for (size_t i = 4; i < t.size(); ++i) t[i] = "FX Slot";
    while (t.size() < (size_t) pages) t.push_back (t.empty() ? std::string() : t.back());
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
            auto lfo = ctx;
            lfo.lfoView = modfx::LfoViewLfo;
            addPages (Mode::Lfo, lfo);
            return groups;
        }
        case Mode::Lfo:
        {
            // 0.7: LFO pages, then the modulation matrix: macros, LFO 2 / 3, envelope 3, every routing
            auto c = ctx;
            c.lfoView = modfx::LfoViewLfo;
            addPages (Mode::Lfo, c);
            const std::string p = core.part (juce::jlimit (0, Core::numParts - 1, ctx.part)).prefix();
            groups.push_back ({ "Macros", 0, { p + "macro1", p + "macro2", p + "macro3", p + "macro4", p + "seqlane" } });
            for (const char* l : { "lfo2.", "lfo3." })
                groups.push_back ({ l[3] == '2' ? "LFO 2" : "LFO 3", 0,
                                    { p + l + "rate", p + l + "sync", p + l + "shape", p + l + "depth", p + l + "mode", p + l + "phase", p + l + "fade" } });
            groups.push_back ({ "Env 3", 0, { p + "env3.delay", p + "env3.a", p + "env3.hold", p + "env3.d", p + "env3.s", p + "env3.r" } });
            for (int k = 1; k <= mod::numSlots; ++k)
            {
                const auto r = p + "mod.s" + std::to_string (k) + ".";
                groups.push_back ({ "Mod", k, { r + "src", r + "via", r + "dst", r + "amt", r + "curve" } });
            }
            return groups;
        }
        case Mode::Effects:
        {
            // 0.7: sends, then every slot of the part's rack and of the master rack (type, mix, bypass, effect)
            auto c = ctx;
            c.fxView = modfx::FxViewSends;
            const auto ids = modeParams (Mode::Effects, core, c);
            groups.push_back ({ "Delay (Send A)", 0, { ids[0], ids[1], ids[2], ids[3], ids[8] } });
            groups.push_back ({ "Reverb (Send B)", 0, { ids[4], ids[5], ids[6], ids[7] } });
            const auto addRack = [&] (const std::vector<std::string>& prefixes, const char* title) {
                for (size_t s = 0; s < prefixes.size(); ++s)
                {
                    ParamGroup g { title, (int) s + 1, { prefixes[s] + "type", prefixes[s] + "mix", prefixes[s] + "bypass" } };
                    for (const auto& id : modfx::slotEffectParams (core, prefixes[s])) g.ids.push_back (id);
                    groups.push_back (std::move (g));
                }
            };
            addRack (modfx::rackPrefixes (modfx::FxViewRack, ctx.part), "FX");
            addRack (modfx::rackPrefixes (modfx::FxViewMaster, ctx.part), "Master FX");
            {
                auto sp = space3d::spaceParams (ctx.part); // 1.0: the part's 3D stage and the monitoring
                sp.erase (std::remove (sp.begin(), sp.end(), std::string()), sp.end());
                groups.push_back ({ "3D Space", 0, sp });
            }
            return groups;
        }
        default: addPages (m, ctx); return groups;
    }
}
} // namespace ohpe::ui
