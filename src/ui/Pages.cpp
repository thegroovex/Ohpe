// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pages.h"

#include <cmath>

namespace ohpe::ui
{
const ModeInfo& modeInfo (Mode m) noexcept
{
    static const ModeInfo table[numModes] = {
        { "Synth", Picto::Synth, true },     { "Envelope", Picto::Envelope, true }, { "LFO", Picto::Lfo, true },
        { "Mixer", Picto::Mixer, true },     { "Effects", Picto::Effects, true },   { "Sequencer", Picto::Sequencer, true },
        { "Tape", Picto::Tape, false },      { "Drum", Picto::Drum, false },
    };
    return table[juce::jlimit (0, numModes - 1, (int) m)];
}

EngineType selectedEngine (Core& core)
{
    auto& part = core.part (0);
    const int t = (int) std::lround (core.params().get (part.id ("engine")));
    if (t >= 0 && t < numEngineTypes && part.engine ((EngineType) t) != nullptr)
        return (EngineType) t;
    return part.currentEngineType();
}

std::vector<std::string> modeParams (Mode m, Core& core, int ch)
{
    auto& part = core.part (0);
    const std::string p = part.prefix();
    switch (m)
    {
        case Mode::Synth:
        {
            std::vector<std::string> ids;
            const auto type = selectedEngine (core);
            if (auto* e = part.engine (type))
                for (const auto& d : e->getParamDescriptors())
                    ids.push_back (p + engineTypeInfo (type).id + "." + d.id);
            return ids;
        }
        case Mode::Envelope:
            return { p + "amp.a", p + "amp.d", p + "amp.s", p + "amp.r", p + "mod.a", p + "mod.d", p + "mod.s", p + "mod.r",
                     p + "mod.amt", p + "velsens", p + "glide", p + "voices" };
        case Mode::Lfo:
            return { p + "lfo.rate", p + "lfo.depth", p + "lfo.shape", p + "lfo.dest", p + "tune", p + "fine", p + "bend", p + "vol" };
        case Mode::Mixer:
        {
            const std::string c = "mix.ch" + std::to_string (juce::jlimit (0, 7, ch) + 1) + ".";
            return { c + "vol", c + "pan", c + "senda", c + "sendb", c + "eqlo", c + "eqmid", c + "eqhi", c + "out",
                     c + "src", c + "mute", c + "solo", "", "mix.master.vol", "mix.returna", "mix.returnb", "mix.master.limit" };
        }
        case Mode::Effects:
            return { "fxa.delay.time", "fxa.delay.feedback", "fxa.delay.tone", "fxa.delay.pingpong",
                     "fxb.reverb.size", "fxb.reverb.damping", "fxb.reverb.predelay", "mix.returnb" };
        case Mode::Sequencer:
            return { "seq.step.length", "seq.step.rate", "seq.step.swing", "seq.step.transpose", "tempo", "seq.run", "seq.step.channel", "" };
        default: return {};
    }
}
} // namespace ohpe::ui
