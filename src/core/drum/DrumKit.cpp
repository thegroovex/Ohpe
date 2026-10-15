// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DrumKit.h"

#include <cmath>
#include <cstring>

namespace ohpe::drum
{
namespace
{
struct DefaultPad
{
    PadType type;
    float tune, decay, pan;
    int choke;
    float tone, level;
};

// the factory kit: fully synthetic, no samples needed
constexpr DefaultPad defaultKit[DrumKit::numPads] = {
    { PadType::Kick, 0, 0.40f, 0.0f, 0, 0.5f, 0.9f },    //  1 kick
    { PadType::Kick, -3, 0.70f, 0.0f, 0, 0.3f, 0.9f },   //  2 long kick
    { PadType::Snare, 0, 0.40f, 0.0f, 0, 0.5f, 0.8f },   //  3 snare
    { PadType::Snare, 3, 0.25f, 0.0f, 0, 0.8f, 0.75f },  //  4 bright snare
    { PadType::Clap, 0, 0.40f, 0.0f, 0, 0.5f, 0.75f },   //  5 clap
    { PadType::Hat, 0, 0.08f, 0.2f, 1, 0.6f, 0.6f },     //  6 closed hat
    { PadType::Hat, 0, 0.60f, 0.2f, 1, 0.6f, 0.55f },    //  7 open hat
    { PadType::Hat, -2, 0.15f, 0.2f, 1, 0.4f, 0.55f },   //  8 pedal hat
    { PadType::Tom, -5, 0.50f, -0.4f, 0, 0.5f, 0.8f },   //  9 low tom
    { PadType::Tom, 0, 0.45f, 0.0f, 0, 0.5f, 0.8f },     // 10 mid tom
    { PadType::Tom, 5, 0.40f, 0.4f, 0, 0.5f, 0.8f },     // 11 high tom
    { PadType::Perc, 0, 0.20f, -0.3f, 0, 0.3f, 0.7f },   // 12 rim
    { PadType::Perc, 7, 0.35f, 0.3f, 0, 0.7f, 0.6f },    // 13 bell
    { PadType::Perc, 12, 0.15f, 0.4f, 0, 0.5f, 0.6f },   // 14 high perc
    { PadType::Kick, 4, 0.25f, 0.0f, 0, 0.8f, 0.9f },    // 15 tight kick
    { PadType::Snare, -3, 0.55f, 0.0f, 0, 0.3f, 0.8f },  // 16 deep snare
    { PadType::Clap, -2, 0.60f, 0.0f, 0, 0.3f, 0.7f },   // 17 wide clap
    { PadType::Hat, 5, 0.06f, -0.3f, 2, 0.8f, 0.55f },   // 18 closed hat 2
    { PadType::Hat, 5, 0.70f, -0.3f, 2, 0.8f, 0.5f },    // 19 open hat 2
    { PadType::Tom, -9, 0.60f, -0.5f, 0, 0.4f, 0.8f },   // 20 floor tom
    { PadType::Perc, -5, 0.30f, -0.2f, 0, 0.4f, 0.65f }, // 21 low perc
    { PadType::Perc, 3, 0.50f, 0.2f, 0, 0.9f, 0.55f },   // 22 metal perc
    { PadType::Snare, 7, 0.15f, 0.1f, 0, 1.0f, 0.7f },   // 23 snap
    { PadType::Kick, -7, 0.90f, 0.0f, 0, 0.2f, 0.9f },   // 24 sub kick
};

std::string padId (int pad, const char* name) { return "pad" + std::to_string (pad + 1) + "." + name; }
} // namespace

PadSettings DrumKit::defaultPad (int pad) noexcept
{
    const auto& d = defaultKit[std::clamp (pad, 0, numPads - 1)];
    PadSettings s;
    s.type = d.type;
    s.tune = d.tune;
    s.decay = d.decay;
    s.level = d.level;
    s.pan = d.pan;
    s.choke = d.choke;
    s.tone = d.tone;
    return s;
}

DrumKit::DrumKit()
{
    for (int i = 0; i < numPads; ++i) pads[(size_t) i] = defaultPad (i);
}

std::vector<ParamDescriptor> DrumKit::paramDescriptors()
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("vol", "Drum Volume", "Drum-Lautstärke", 0.0f, 1.0f, 0.8f));
    d.push_back (integer ("base", "Drum Base Note", "Drum-Grundnote", 0, 127 - numPads + 1, 36));
    for (int p = 0; p < numPads; ++p)
    {
        const auto def = defaultPad (p);
        const auto n = std::to_string (p + 1);
        d.push_back (choice (padId (p, "type"), "Pad " + n + " Sound", "Pad " + n + " Klang",
                             { "Kick", "Snare", "Hat", "Clap", "Tom", "Perc", "Sample" },
                             { "Bassdrum", "Snare", "Hi-Hat", "Klatschen", "Tom", "Perkussion", "Sample" }, (int) def.type));
        d.push_back (continuous (padId (p, "tune"), "Pad " + n + " Tune", "Pad " + n + " Stimmung", -24.0f, 24.0f, def.tune,
                                 ParamUnit::Semitones));
        d.push_back (continuous (padId (p, "decay"), "Pad " + n + " Decay", "Pad " + n + " Abklingen", 0.0f, 1.0f, def.decay));
        d.push_back (continuous (padId (p, "level"), "Pad " + n + " Level", "Pad " + n + " Pegel", 0.0f, 1.0f, def.level));
        d.push_back (continuous (padId (p, "pan"), "Pad " + n + " Pan", "Pad " + n + " Panorama", -1.0f, 1.0f, def.pan));
        d.push_back (integer (padId (p, "choke"), "Pad " + n + " Choke Group", "Pad " + n + " Choke-Gruppe", 0, numChokeGroups,
                              def.choke));
        d.push_back (continuous (padId (p, "tone"), "Pad " + n + " Tone", "Pad " + n + " Klangfarbe", 0.0f, 1.0f, def.tone));
    }
    return d;
}

void DrumKit::registerParams (ParamRegistry& reg)
{
    for (auto d : paramDescriptors())
    {
        d.id = std::string (prefix) + d.id;
        d.group = "drum";
        reg.add (std::move (d));
    }
}

void DrumKit::bind (const ParamRegistry& reg)
{
    paramIndex.clear();
    lastValue.clear();
    for (const auto& d : paramDescriptors())
    {
        paramIndex.push_back (reg.indexOf (std::string (prefix) + d.id));
        lastValue.push_back (NAN);
    }
}

void DrumKit::prepare (double sampleRate, int)
{
    for (auto& v : voices) v.prepare ((float) sampleRate);
    for (auto& v : lastValue) v = NAN;
    reset();
}

void DrumKit::reset() noexcept
{
    for (auto& v : voices) v.kill();
}

void DrumKit::setParam (int i, float v) noexcept
{
    if (i == 0) { volume = dsp::clamp01 (v); return; }
    if (i == 1) { base = std::clamp ((int) std::lround (v), 0, 127); return; }
    const int p = (i - 2) / numPadParams;
    if (p < 0 || p >= numPads) return;
    auto& s = pads[(size_t) p];
    switch ((i - 2) % numPadParams)
    {
        case pType: s.type = (PadType) std::clamp ((int) std::lround (v), 0, (int) PadType::numTypes - 1); break;
        case pTune: s.tune = std::clamp (v, -24.0f, 24.0f); break;
        case pDecay: s.decay = dsp::clamp01 (v); break;
        case pLevel: s.level = dsp::clamp01 (v); break;
        case pPan: s.pan = std::clamp (v, -1.0f, 1.0f); break;
        case pChoke: s.choke = std::clamp ((int) std::lround (v), 0, numChokeGroups); break;
        case pTone: s.tone = dsp::clamp01 (v); break;
        default: break;
    }
}

void DrumKit::pullParams (const ParamSnapshot& snap) noexcept
{
    for (size_t i = 0; i < paramIndex.size(); ++i)
    {
        if (paramIndex[i] < 0) continue;
        const float v = snap[paramIndex[i]];
        if (! (v == lastValue[i]))
        {
            lastValue[i] = v;
            setParam ((int) i, v);
        }
    }
}

void DrumKit::handleMidi (const MidiEvent& e) noexcept
{
    if (e.isNoteOn()) noteOn (e.data1, (float) e.data2 / 127.0f);
    else if (e.isController() && (e.data1 == 120 || e.data1 == 123)) allNotesOff (e.data1 == 120);
}

void DrumKit::noteOn (int note, float velocity) noexcept
{
    const int p = note - base;
    if (p >= 0 && p < numPads) triggerPad (p, velocity);
}

void DrumKit::triggerPad (int p, float velocity) noexcept
{
    if (p < 0 || p >= numPads) return;
    const auto& s = pads[(size_t) p];

    // a pad is monophonic; its choke group cuts the other pads of the group
    for (auto& v : voices)
        if (v.isActive() && (v.padIndex() == p || (s.choke > 0 && v.chokeGroup() == s.choke)))
            v.choke();

    DrumVoice* target = nullptr;
    for (auto& v : voices)
        if (! v.isActive()) { target = &v; break; }
    if (target == nullptr) // steal: a fading voice first, else the oldest
    {
        for (auto& v : voices)
            if (v.isChoking() && (target == nullptr || v.startOrder() < target->startOrder())) target = &v;
        if (target == nullptr)
            for (auto& v : voices)
                if (target == nullptr || v.startOrder() < target->startOrder()) target = &v;
    }
    target->trigger (p, s, velocity, samples[(size_t) p].get(), ++counter);
}

void DrumKit::allNotesOff (bool hard) noexcept
{
    for (auto& v : voices)
    {
        if (hard) v.kill();
        else v.choke();
    }
}

void DrumKit::render (float* l, float* r, int numSamples) noexcept
{
    std::memset (l, 0, sizeof (float) * (size_t) numSamples);
    std::memset (r, 0, sizeof (float) * (size_t) numSamples);
    for (auto& v : voices)
        if (v.isActive())
            v.render (l, r, numSamples, v.padIndex() >= 0 ? samples[(size_t) v.padIndex()].get() : nullptr);
    for (int i = 0; i < numSamples; ++i) { l[i] *= volume; r[i] *= volume; }
}

int DrumKit::activeVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : voices) n += v.isActive() ? 1 : 0;
    return n;
}

bool DrumKit::isPadSounding (int p) const noexcept
{
    for (const auto& v : voices)
        if (v.isActive() && v.padIndex() == p) return true;
    return false;
}

void DrumKit::setPadSample (int p, SamplePtr s)
{
    if (p < 0 || p >= numPads) return;
    samples[(size_t) p].set (std::move (s));
}

SamplePtr DrumKit::padSample (int p) const
{
    if (p < 0 || p >= numPads) return nullptr;
    return samples[(size_t) p].owner();
}
} // namespace ohpe::drum
