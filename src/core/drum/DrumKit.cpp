// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DrumKit.h"

#include "../sample/FactoryBank.h"

#include <cmath>
#include <cstring>

namespace ohpe::drum
{
namespace
{
struct DefaultPad
{
    PadType type;
    float modelTune, modelDecay, modelTone; // voicing of the model on this pad
    int choke;
    float pan, level;
    const char* name;
};

// The factory kit. The plug-in loads a recording for every pad except the clap (see
// resources/drumkit); the model voicing below is what a pad plays without a sample.
constexpr DefaultPad defaultKit[DrumKit::numPads] = {
    { PadType::Kick, 0, 0.40f, 0.5f, 0, 0.0f, 0.95f, "kick" },               //  1
    { PadType::Kick, -2, 0.55f, 0.4f, 0, 0.0f, 0.90f, "kick, snares on" },   //  2
    { PadType::Snare, 0, 0.40f, 0.5f, 0, 0.0f, 0.85f, "snare" },             //  3
    { PadType::Snare, 3, 0.30f, 0.85f, 0, 0.0f, 0.75f, "rimshot" },          //  4
    { PadType::Clap, 0, 0.45f, 0.5f, 0, 0.0f, 0.70f, "clap" },               //  5
    { PadType::Hat, 0, 0.08f, 0.6f, 1, 0.0f, 0.60f, "closed hat" },          //  6
    { PadType::Hat, 0, 0.60f, 0.6f, 1, 0.0f, 0.50f, "open hat" },            //  7
    { PadType::Hat, -2, 0.12f, 0.4f, 1, 0.0f, 0.50f, "pedal hat" },          //  8
    { PadType::Tom, -2, 0.50f, 0.5f, 0, 0.0f, 0.80f, "low tom" },            //  9
    { PadType::Tom, 4, 0.45f, 0.5f, 0, 0.0f, 0.80f, "high tom" },            // 10
    { PadType::Tom, -6, 0.60f, 0.4f, 0, 0.0f, 0.80f, "floor tom" },          // 11
    { PadType::Perc, 0, 0.15f, 0.3f, 0, 0.0f, 0.70f, "cross-stick" },        // 12
    { PadType::Hat, 7, 0.75f, 0.9f, 0, 0.0f, 0.45f, "ride" },                // 13
    { PadType::Perc, 12, 0.60f, 0.8f, 0, 0.0f, 0.45f, "ride bell" },         // 14
    { PadType::Hat, -5, 0.95f, 0.7f, 0, 0.0f, 0.45f, "crash" },              // 15
    { PadType::Hat, -7, 1.00f, 0.8f, 0, 0.0f, 0.40f, "sizzle crash" },       // 16
    { PadType::Hat, 0, 0.30f, 0.6f, 1, 0.0f, 0.50f, "half-open hat" },       // 17
    { PadType::Snare, 0, 0.25f, 0.3f, 0, 0.0f, 0.40f, "ghost snare" },       // 18
    { PadType::Perc, 7, 0.35f, 0.7f, 0, 0.3f, 0.45f, "cowbell" },            // 19
    { PadType::Hat, 9, 0.30f, 0.9f, 0, -0.3f, 0.40f, "tambourine" },         // 20
    { PadType::Hat, 12, 0.15f, 1.0f, 0, 0.35f, 0.35f, "shaker" },            // 21
    { PadType::Perc, 17, 0.10f, 0.2f, 0, -0.25f, 0.45f, "claves" },          // 22
    { PadType::Tom, 9, 0.35f, 0.6f, 0, -0.35f, 0.65f, "conga" },             // 23
    { PadType::Tom, 16, 0.25f, 0.6f, 0, 0.35f, 0.60f, "bongo" },             // 24
};

std::string padId (int pad, const char* name) { return "pad" + std::to_string (pad + 1) + "." + name; }
} // namespace

PadSettings DrumKit::defaultPad (int pad) noexcept
{
    const auto& d = defaultKit[std::clamp (pad, 0, numPads - 1)];
    PadSettings s;
    s.type = d.type;
    s.level = d.level;
    s.pan = d.pan;
    s.choke = d.choke;
    s.modelTune = d.modelTune;
    s.modelDecay = d.modelDecay;
    s.modelTone = d.modelTone;
    return s;
}

const char* DrumKit::defaultPadName (int pad) noexcept { return defaultKit[std::clamp (pad, 0, numPads - 1)].name; }

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
        d.push_back (choice (padId (p, "src"), "Pad " + n + " Source", "Pad " + n + " Quelle", { "Sample", "Synth" },
                             { "Sample", "Synth" }, (int) def.source));
        d.push_back (choice (padId (p, "type"), "Pad " + n + " Synth Model", "Pad " + n + " Synth-Modell",
                             { "Kick", "Snare", "Hat", "Clap", "Tom", "Perc" },
                             { "Bassdrum", "Snare", "Hi-Hat", "Klatschen", "Tom", "Perkussion" }, (int) def.type));
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

std::vector<ParamDescriptor> DrumKit::lateParamDescriptors()
{
    using namespace pd;
    return { choice ("kit", "Drum Kit", "Drum-Kit", factory::kitNamesEn(), factory::kitNamesDe(), 0) };
}

void DrumKit::registerLateParams (ParamRegistry& reg)
{
    for (auto d : lateParamDescriptors())
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
    for (const auto& d : lateParamDescriptors())
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
    if (i == kitParamIndex) { kit.store (std::clamp ((int) std::lround (v), 0, factory::numKits - 1), std::memory_order_relaxed); return; }
    const int p = (i - 2) / numPadParams;
    if (p < 0 || p >= numPads) return;
    auto& s = pads[(size_t) p];
    switch ((i - 2) % numPadParams)
    {
        case pSource: s.source = (PadSource) std::clamp ((int) std::lround (v), 0, (int) PadSource::numSources - 1); break;
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
    target->trigger (p, s, velocity, padSampleRt (p), ++counter);
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
            v.render (l, r, numSamples, v.padIndex() >= 0 ? padSampleRt (v.padIndex()) : nullptr);
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

const SampleData* DrumKit::padSampleRt (int p) const noexcept
{
    // 0.4: the selected factory kit, unless the user loaded a sample onto the pad
    const int k0 = kit.load (std::memory_order_relaxed);
    if (k0 > 0 && ! userSet[(size_t) p].load (std::memory_order_acquire))
        if (const auto* k = factory::kitSample (k0, p)) return k;
    return samples[(size_t) p].get();
}

void DrumKit::publish (int p)
{
    const auto& u = userSamples[(size_t) p];
    userSet[(size_t) p].store (u != nullptr, std::memory_order_release);
    samples[(size_t) p].set (u != nullptr ? u : factorySamples[(size_t) p]);
}

void DrumKit::setPadSample (int p, SamplePtr s)
{
    if (p < 0 || p >= numPads) return;
    userSamples[(size_t) p] = std::move (s);
    publish (p);
}

SamplePtr DrumKit::padSample (int p) const { return p >= 0 && p < numPads ? userSamples[(size_t) p] : nullptr; }

void DrumKit::setFactorySample (int p, SamplePtr s)
{
    if (p < 0 || p >= numPads) return;
    factorySamples[(size_t) p] = std::move (s);
    publish (p);
}

SamplePtr DrumKit::factorySample (int p) const { return p >= 0 && p < numPads ? factorySamples[(size_t) p] : nullptr; }

SamplePtr DrumKit::effectiveSample (int p) const
{
    if (p < 0 || p >= numPads) return nullptr;
    const int k0 = kit.load (std::memory_order_relaxed);
    if (k0 > 0 && userSamples[(size_t) p] == nullptr)
        if (auto k = factory::kitSampleOwner (k0, p)) return k;
    return samples[(size_t) p].owner();
}
} // namespace ohpe::drum
