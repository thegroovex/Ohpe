// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Part.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ohpe
{
Part::Part (int partNumber) : number (partNumber), idPrefix ("p" + std::to_string (partNumber) + ".")
{
    for (int t = 0; t < numEngineTypes; ++t)
        engines[(size_t) t] = createEngine ((EngineType) t);
}

void Part::registerCommonParams (ParamRegistry& reg) const
{
    using namespace pd;
    std::vector<std::string> en, de;
    for (int t = 0; t < numEngineTypes; ++t)
    {
        en.emplace_back (engineTypeInfo ((EngineType) t).nameEn);
        de.emplace_back (engineTypeInfo ((EngineType) t).nameDe);
    }
    const auto group = "part" + std::to_string (number);
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    add (choice (id ("engine"), "Engine", "Engine", en, de, (int) EngineType::Analog));
    add (continuous (id ("vol"), "Volume", "Lautstärke", 0.0f, 1.0f, 0.8f));
    add (integer (id ("voices"), "Voices", "Stimmen", 1, ISynthEngine::maxVoices, 8));
    add (integer (id ("tune"), "Tune", "Stimmung", -24, 24, 0, ParamUnit::Semitones));
    add (continuous (id ("fine"), "Fine Tune", "Feinstimmung", -100.0f, 100.0f, 0.0f, ParamUnit::Cents));
    add (continuous (id ("glide"), "Glide", "Glide", 0.0f, 2.0f, 0.0f, ParamUnit::Seconds, 0.4f));
    add (integer (id ("bend"), "Bend Range", "Pitchbend-Bereich", 0, 24, 2, ParamUnit::Semitones));
    add (continuous (id ("velsens"), "Velocity", "Anschlag", 0.0f, 1.0f, 0.7f));
    add (continuous (id ("amp.a"), "Amp Attack", "Amp Attack", 0.001f, 10.0f, 0.005f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("amp.d"), "Amp Decay", "Amp Decay", 0.001f, 10.0f, 0.3f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("amp.s"), "Amp Sustain", "Amp Sustain", 0.0f, 1.0f, 0.8f));
    add (continuous (id ("amp.r"), "Amp Release", "Amp Release", 0.001f, 10.0f, 0.3f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("mod.a"), "Mod Attack", "Mod Attack", 0.001f, 10.0f, 0.005f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("mod.d"), "Mod Decay", "Mod Decay", 0.001f, 10.0f, 0.4f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("mod.s"), "Mod Sustain", "Mod Sustain", 0.0f, 1.0f, 0.3f));
    add (continuous (id ("mod.r"), "Mod Release", "Mod Release", 0.001f, 10.0f, 0.4f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("mod.amt"), "Mod Env Amount", "Mod-Hüllkurve Menge", -1.0f, 1.0f, 0.3f));
    add (continuous (id ("lfo.rate"), "LFO Rate", "LFO Tempo", 0.01f, 40.0f, 2.0f, ParamUnit::Hertz, 0.3f));
    add (continuous (id ("lfo.depth"), "LFO Depth", "LFO Tiefe", 0.0f, 1.0f, 0.0f));
    add (choice (id ("lfo.shape"), "LFO Shape", "LFO Form", { "Sine", "Triangle", "Saw", "Square", "S&H" },
                 { "Sinus", "Dreieck", "Sägezahn", "Rechteck", "S&H" }, 0));
    add (choice (id ("lfo.dest"), "LFO Target", "LFO Ziel", { "Pitch", "Timbre", "Amp", "Pan" },
                 { "Tonhöhe", "Klangfarbe", "Pegel", "Panorama" }, 0));
}

void Part::registerLateCommonParams (ParamRegistry& reg) const
{
    using namespace pd;
    auto d = continuous (id ("gain"), "Part " + std::to_string (number) + " Gain", "Part " + std::to_string (number) + " Gain",
                         -24.0f, 24.0f, 0.0f, ParamUnit::Decibel);
    d.group = "part" + std::to_string (number);
    reg.add (std::move (d));
}

void Part::registerEngineParams (ParamRegistry& reg, EngineType type) const
{
    const auto& eng = engines[(size_t) type];
    if (eng == nullptr)
        return;
    const std::string enginePrefix = idPrefix + engineTypeInfo (type).id + ".";
    const auto group = "part" + std::to_string (number) + "/" + engineTypeInfo (type).id;
    const auto list = eng->getParamDescriptors();
    const int late = std::min ((int) list.size(), eng->firstLateParam());
    for (int i = 0; i < late; ++i)
    {
        auto d = list[(size_t) i];
        d.id = enginePrefix + d.id;
        d.group = group;
        reg.add (std::move (d));
    }
}

void Part::registerLateEngineParams (ParamRegistry& reg, EngineType type) const
{
    const auto& eng = engines[(size_t) type];
    if (eng == nullptr)
        return;
    const std::string enginePrefix = idPrefix + engineTypeInfo (type).id + ".";
    const auto group = "part" + std::to_string (number) + "/" + engineTypeInfo (type).id;
    const auto list = eng->getParamDescriptors();
    for (int i = std::min ((int) list.size(), eng->firstLateParam()); i < (int) list.size(); ++i)
    {
        auto d = list[(size_t) i];
        d.id = enginePrefix + d.id;
        d.group = group;
        reg.add (std::move (d));
    }
}

void Part::bind (const ParamRegistry& reg)
{
    static const char* names[numCommon] = { "engine", "vol", "voices", "tune", "fine", "glide", "bend", "velsens",
                                            "amp.a", "amp.d", "amp.s", "amp.r", "mod.a", "mod.d", "mod.s", "mod.r",
                                            "mod.amt", "lfo.rate", "lfo.depth", "lfo.shape", "lfo.dest" };
    for (int i = 0; i < numCommon; ++i)
        commonIndex[(size_t) i] = reg.indexOf (id (names[i]));
    gainIndex = reg.indexOf (id ("gain"));

    for (int t = 0; t < numEngineTypes; ++t)
    {
        auto& slot = slots[(size_t) t];
        slot.paramIndex.clear();
        slot.lastValue.clear();
        if (engines[(size_t) t] == nullptr)
            continue;
        const std::string enginePrefix = idPrefix + engineTypeInfo ((EngineType) t).id + ".";
        for (const auto& d : engines[(size_t) t]->getParamDescriptors())
        {
            slot.paramIndex.push_back (reg.indexOf (enginePrefix + d.id));
            slot.lastValue.push_back (NAN);
        }
    }
}

void Part::prepare (double sampleRate, int maxBlockSize)
{
    for (auto& e : engines)
        if (e != nullptr) e->prepare (sampleRate, maxBlockSize);
    for (auto& s : slots)
        for (auto& v : s.lastValue) v = NAN;
    dcCoeff = 1.0f - (float) (2.0 * 3.14159265358979 * 5.0 / sampleRate);
    reset();
}

void Part::reset() noexcept
{
    for (auto& e : engines)
        if (e != nullptr) e->reset();
    sustain = false;
    sustained.fill (false);
    bend = 0.0f;
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
}

void Part::pullParams (const ParamSnapshot& s) noexcept
{
    auto v = [&] (Common c) { return s[commonIndex[(size_t) c]]; };
    const auto newType = (EngineType) std::clamp (s.getInt (commonIndex[cEngine]), 0, numEngineTypes - 1);
    if (newType != current)
    {
        if (auto* old = engines[(size_t) current].get()) old->allNotesOff (false); // let it ring out
        current = newType;
    }
    volume = v (cVolume);
    gainFactor = gainIndex >= 0 ? std::pow (10.0f, s[gainIndex] / 20.0f) : 1.0f;
    polyphony = s.getInt (commonIndex[cVoices]);
    tune = v (cTune) + v (cFine) * 0.01f;
    bendRange = v (cBendRange);
    common.glideSeconds = v (cGlide);
    common.velocitySens = v (cVelSens);
    common.amp = { v (cAmpA), v (cAmpD), v (cAmpS), v (cAmpR) };
    common.mod = { v (cModA), v (cModD), v (cModS), v (cModR) };
    common.modEnvAmount = v (cModAmount);
    common.lfoRate = v (cLfoRate);
    common.lfoDepth = v (cLfoDepth);
    common.lfoShape = s.getInt (commonIndex[cLfoShape]);
    common.lfoDest = (LfoDest) std::clamp (s.getInt (commonIndex[cLfoDest]), 0, (int) LfoDest::numDests - 1);
    common.pitchOffset = tune + bend * bendRange;

    for (int t = 0; t < numEngineTypes; ++t)
    {
        auto* e = engines[(size_t) t].get();
        if (e == nullptr) continue;
        auto& slot = slots[(size_t) t];
        for (size_t i = 0; i < slot.paramIndex.size(); ++i)
        {
            const float val = s[slot.paramIndex[i]];
            if (! (val == slot.lastValue[i]))
            {
                slot.lastValue[i] = val;
                e->setParam ((int) i, val);
            }
        }
        e->setCommon (common);
        e->setPolyphony (polyphony);
    }
}

void Part::noteOn (int note, float velocity) noexcept
{
    if (note < 0 || note > 127) return;
    sustained[(size_t) note] = false;
    if (auto* e = engines[(size_t) current].get()) e->noteOn (note, velocity);
}

void Part::noteOff (int note) noexcept
{
    if (note < 0 || note > 127) return;
    if (sustain) { sustained[(size_t) note] = true; return; }
    for (auto& e : engines)
        if (e != nullptr) e->noteOff (note);
}

void Part::allNotesOff (bool hard) noexcept
{
    sustained.fill (false);
    for (auto& e : engines)
        if (e != nullptr) e->allNotesOff (hard);
}

void Part::noteExpression (int note, const NoteExpression& x) noexcept
{
    for (auto& e : engines)
        if (e != nullptr) e->noteExpression (note, x);
}

void Part::handleMidi (const MidiEvent& m) noexcept
{
    if (m.isNoteOn()) noteOn (m.data1, (float) m.data2 / 127.0f);
    else if (m.isNoteOff()) noteOff (m.data1);
    else if (m.isPitchBend())
    {
        bend = ((float) m.pitchBendValue() - 8192.0f) / 8192.0f;
        common.pitchOffset = tune + bend * bendRange;
        for (auto& e : engines)
            if (e != nullptr) e->setCommon (common);
    }
    else if (m.isController())
    {
        if (m.data1 == 64)
        {
            sustain = m.data2 >= 64;
            if (! sustain)
                for (int n = 0; n < 128; ++n)
                    if (sustained[(size_t) n]) { sustained[(size_t) n] = false; noteOff (n); }
        }
        else if (m.data1 == 120) allNotesOff (true);
        else if (m.data1 == 123) allNotesOff (false);
    }
}

void Part::render (float* l, float* r, int numSamples) noexcept
{
    std::memset (l, 0, sizeof (float) * (size_t) numSamples);
    std::memset (r, 0, sizeof (float) * (size_t) numSamples);
    for (auto& e : engines)
        if (e != nullptr && e->activeVoiceCount() > 0)
            e->render (l, r, numSamples);
    // 0.4: a gentle DC blocker (one pole at ~5 Hz): pulse shapes and asymmetric waves must not
    // push a DC offset into the mix
    const float g = volume * gainFactor;
    float* ch[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        float x1 = dcX[c], y1 = dcY[c];
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = ch[c][i];
            y1 = x - x1 + dcCoeff * y1;
            x1 = x;
            ch[c][i] = y1 * g;
        }
        dcX[c] = x1;
        dcY[c] = (std::abs (y1) < 1.0e-20f) ? 0.0f : y1; // no denormals in silence
    }
}

int Part::activeVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& e : engines)
        if (e != nullptr) n += e->activeVoiceCount();
    return n;
}
} // namespace ohpe
