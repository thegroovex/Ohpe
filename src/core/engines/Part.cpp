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
    for (auto& u : uiTargetIndex) u.store (-1);
    for (auto& u : uiTargetDelta) u.store (0.0f);
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

void Part::registerLate05Params (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    auto d = toggle (id ("polyglide"), "Part " + n + " Poly Glide", "Part " + n + " Poly-Glide", false);
    d.group = "part" + n;
    reg.add (std::move (d));
}

void Part::registerLate06Params (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const auto group = "part" + n;
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    add (choice (id ("glide.mode"), "Part " + n + " Glide Mode", "Part " + n + " Glide-Modus", { "Auto", "Legato", "Always" },
                 { "Auto", "Legato", "Immer" }, GlideAuto));
    add (choice (id ("glide.rate"), "Part " + n + " Glide Timing", "Part " + n + " Glide-Timing", { "Constant Time", "Constant Rate" },
                 { "Feste Zeit", "Feste Rate" }, 0));
    add (continuous (id ("pitchenv.amt"), "Part " + n + " Pitch Env Amount", "Part " + n + " Tonhöhen-Hüllkurve Menge", -24.0f, 24.0f,
                     0.0f, ParamUnit::Semitones));
    add (continuous (id ("pitchenv.time"), "Part " + n + " Pitch Env Time", "Part " + n + " Tonhöhen-Hüllkurve Zeit", 0.005f, 4.0f,
                     0.25f, ParamUnit::Seconds, 0.35f));
    add (continuous (id ("pitchenv.curve"), "Part " + n + " Pitch Env Curve", "Part " + n + " Tonhöhen-Hüllkurve Kurve", 0.0f, 1.0f, 0.5f));
}

void Part::registerLate07Rack (ParamRegistry& reg) const
{
    const auto group = "part" + std::to_string (number) + "/fx";
    if (number > 1) InsertSlot::registerParams (reg, FxRack::partSlotPrefix (idPrefix, 0), group); // slot 1 of parts 2-4
    FxRack::registerExtraSlots (reg, idPrefix, group);
    FxRack::registerSlotControls (reg, idPrefix, group);
}

void Part::registerLate07Mod (ParamRegistry& reg) const { mod::PartMatrix::registerParams (reg, idPrefix, number); }

void Part::registerEngineParams (ParamRegistry& reg, EngineType type) const
{
    const auto& eng = engines[(size_t) type];
    if (eng == nullptr)
        return;
    const std::string enginePrefix = idPrefix + engineTypeInfo (type).id + ".";
    const auto group = "part" + std::to_string (number) + "/" + engineTypeInfo (type).id;
    const auto list = eng->getParamDescriptors();
    const int late = std::min ((int) list.size(), std::min (eng->firstLateParam(), eng->firstLate08Param()));
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
    const int end = std::min ((int) list.size(), eng->firstLate08Param());
    for (int i = std::min ((int) list.size(), eng->firstLateParam()); i < end; ++i)
    {
        auto d = list[(size_t) i];
        d.id = enginePrefix + d.id;
        d.group = group;
        reg.add (std::move (d));
    }
}

void Part::registerLate08EngineParams (ParamRegistry& reg, EngineType type) const
{
    const auto& eng = engines[(size_t) type];
    if (eng == nullptr)
        return;
    const std::string enginePrefix = idPrefix + engineTypeInfo (type).id + ".";
    const auto group = "part" + std::to_string (number) + "/" + engineTypeInfo (type).id;
    const auto list = eng->getParamDescriptors();
    for (int i = std::min ((int) list.size(), eng->firstLate08Param()); i < (int) list.size(); ++i)
    {
        auto d = list[(size_t) i];
        d.id = enginePrefix + d.id;
        d.group = group;
        reg.add (std::move (d));
    }
}

void Part::registerLate08MpeParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const auto group = "part" + n + "/midi";
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    const std::string P = "Part " + n + " ";
    add (continuous (id ("mpe.pressamt"), P + "Pressure Amount", P + "Druck-Menge", -1.0f, 1.0f, 1.0f));
    add (continuous (id ("mpe.timbreamt"), P + "Timbre Amount", P + "Timbre-Menge", -1.0f, 1.0f, 1.0f));
    add (continuous (id ("mpe.smooth"), P + "Expression Smoothing", P + "Ausdrucks-Glättung", 0.0f, 0.5f, 0.0f, ParamUnit::Seconds, 0.5f));
    add (continuous (id ("mpe.slide"), P + "Per-Note Slide", P + "Pro-Noten-Slide", 0.0f, 1.0f, 0.0f, ParamUnit::Seconds, 0.5f));
}

void Part::registerLate08MonoParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const std::string P = "Part " + n + " ";
    auto d = continuous (id ("monobass"), P + "Bass Mono Below", P + "Bass mono unter", 20.0f, 400.0f, 20.0f, ParamUnit::Hertz, 0.5f);
    d.group = "part" + n;
    reg.add (std::move (d));
}

void Part::applyBassMono (float* l, float* r, int numSamples) noexcept
{
    if (monoBassHz <= 20.5f) return;
    for (int i = 0; i < numSamples; ++i)
    {
        const float m = 0.5f * (l[i] + r[i]);
        float s = 0.5f * (l[i] - r[i]);
        s = monoLp2.process (monoLp1.process (s, dsp::Svf::HighPass), dsp::Svf::HighPass); // 4th-order high-pass of the side
        l[i] = m + s;
        r[i] = m - s;
    }
}

void Part::registerLate15LifeParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const std::string P = "Part " + n + " ";
    auto d = continuous (id ("life"), P + "Life", P + "Leben", 0.0f, 1.0f, 0.0f, ParamUnit::Percent);
    d.group = "part" + n;
    reg.add (std::move (d));
}

void Part::registerLate20UserTableParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const std::string P = "Part " + n + " ";
    auto add = [&] (ParamDescriptor d) { d.group = "part" + n; reg.add (std::move (d)); };
    add (toggle (id ("usertable.wt"), P + "User Wavetable", P + "Eigene Wavetable", false));
    add (toggle (id ("usertable.vocal"), P + "User Formants", P + "Eigene Formanten", false));
}

void Part::registerLate19ResoParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const auto group = "part" + n + "/reso";
    const std::string P = "Part " + n + " ";
    auto on = toggle (id ("reso.on"), P + "Resonator", P + "Resonanzkörper", false);
    on.group = group;
    reg.add (std::move (on));
    for (auto d : ResonatorEffect().getParamDescriptors())
    {
        d.id = id ("reso." + d.id);
        d.nameEn = P + d.nameEn;
        d.nameDe = P + d.nameDe;
        d.group = group;
        reg.add (std::move (d));
    }
}

void Part::registerLate21CharacterParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const std::string P = "Part " + n + " ";
    for (int m = 0; m < character::numMacros; ++m)
    {
        auto d = continuous (id (std::string ("char.") + character::key (m)), P + character::nameEn (m), P + character::nameDe (m), -1.0f, 1.0f,
                             0.0f, ParamUnit::Percent);
        d.group = "part" + n;
        reg.add (std::move (d));
    }
}

void Part::registerLate32GlidePressParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const std::string P = "Part " + n + " ";
    auto d = continuous (id ("glide.press"), P + "Glide Pressure", P + "Glide-Druck", -1.0f, 1.0f, 0.0f, ParamUnit::Percent);
    d.group = "part" + n;
    reg.add (std::move (d));
}

void Part::applyReso (float* l, float* r, int n) noexcept
{
    if (! resoOn && resoFade <= 0.0f) return;
    if (resoOn && resoFade >= 1.0f)
    {
        reso.process (l, r, n);
        return;
    }
    n = std::min (n, (int) resoDryL.size());
    std::memcpy (resoDryL.data(), l, sizeof (float) * (size_t) n);
    std::memcpy (resoDryR.data(), r, sizeof (float) * (size_t) n);
    reso.process (l, r, n);
    const float step = resoOn ? spaceFadeStep : -spaceFadeStep; // 20 ms, as the 3D stage
    for (int i = 0; i < n; ++i)
    {
        resoFade = std::clamp (resoFade + step, 0.0f, 1.0f);
        const float w = resoFade * resoFade * (3.0f - 2.0f * resoFade);
        l[i] = resoDryL[(size_t) i] + (l[i] - resoDryL[(size_t) i]) * w;
        r[i] = resoDryR[(size_t) i] + (r[i] - resoDryR[(size_t) i]) * w;
    }
    if (resoFade <= 0.0f)
    {
        reso.reset(); // the next switch-on starts clean
        resoRunningFlag.store (false, std::memory_order_relaxed);
    }
}

void Part::updateHeldKeys() noexcept
{
    int k = 0;
    for (int nn = 0; nn < 128 && k < KeyState::maxHeld; ++nn)
        if (held[(size_t) nn] > 0.0f || sustained[(size_t) nn]) keys.held[(size_t) k++] = nn;
    keys.numHeld = k;
}

void Part::registerLate10SpaceParams (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const auto group = "part" + n + "/space";
    const std::string P = "Part " + n + " 3D ";
    auto on = toggle (id ("space.on"), P + "Space", P + "Raum", false); // display: "3D Space" / "3D Raum"
    on.group = group;
    reg.add (std::move (on));
    for (auto d : SpaceEffect().getParamDescriptors())
    {
        d.id = id ("space." + d.id);
        d.nameEn = P + d.nameEn;
        d.nameDe = P + d.nameDe;
        d.group = group;
        reg.add (std::move (d));
    }
}

void Part::applySpace (float* l, float* r, int n) noexcept
{
    if (! spaceOn && spaceFade <= 0.0f) return;
    if (spaceOn && spaceFade >= 1.0f)
    {
        space.process (l, r, n);
        return;
    }
    n = std::min (n, (int) spaceDryL.size());
    std::memcpy (spaceDryL.data(), l, sizeof (float) * (size_t) n);
    std::memcpy (spaceDryR.data(), r, sizeof (float) * (size_t) n);
    space.process (l, r, n);
    const float step = spaceOn ? spaceFadeStep : -spaceFadeStep;
    for (int i = 0; i < n; ++i)
    {
        spaceFade = std::clamp (spaceFade + step, 0.0f, 1.0f);
        const float w = spaceFade * spaceFade * (3.0f - 2.0f * spaceFade); // smooth equal-gain fade (coherent signals)
        l[i] = spaceDryL[(size_t) i] + (l[i] - spaceDryL[(size_t) i]) * w;
        r[i] = spaceDryR[(size_t) i] + (r[i] - spaceDryR[(size_t) i]) * w;
    }
    if (spaceFade <= 0.0f)
    {
        space.reset(); // the next switch-on starts clean (no stale delay lines)
        spaceRunningFlag.store (false, std::memory_order_relaxed);
    }
}

void Part::registerLate08Params (ParamRegistry& reg) const
{
    using namespace pd;
    const auto n = std::to_string (number);
    const auto group = "part" + n + "/layer";
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    std::vector<std::string> en { "Off" }, de { "Aus" };
    for (int t = 0; t < numEngineTypes; ++t)
    {
        en.emplace_back (engineTypeInfo ((EngineType) t).nameEn);
        de.emplace_back (engineTypeInfo ((EngineType) t).nameDe);
    }
    const std::string P = "Part " + n + " ";
    add (choice (id ("layer.engine"), P + "Layer Engine", P + "Layer-Engine", en, de, 0));
    add (continuous (id ("layer.level"), P + "Layer Level", P + "Layer-Pegel", 0.0f, 1.0f, 0.7f));
    add (integer (id ("layer.tune"), P + "Layer Tune", P + "Layer-Stimmung", -24, 24, 0, ParamUnit::Semitones));
    add (continuous (id ("layer.fine"), P + "Layer Fine", P + "Layer-Feinstimmung", -50.0f, 50.0f, 0.0f, ParamUnit::Cents));
    add (choice (id ("layer.xmode"), P + "Cross-Mod Mode", P + "Kreuzmod.-Art", { "Ring", "AM", "Fold" }, { "Ring", "AM", "Falten" }, XRing));
    add (continuous (id ("layer.xmod"), P + "Cross-Mod", P + "Kreuzmodulation", 0.0f, 1.0f, 0.0f));
    add (continuous (id ("layer.width"), P + "Layer Width", P + "Layer-Breite", 0.0f, 1.0f, 0.5f));
    add (toggle (id ("layer.ownenv"), P + "Layer Own Envelope", P + "Layer eigene Hüllkurve", false));
    add (continuous (id ("layer.a"), P + "Layer Attack", P + "Layer Attack", 0.001f, 10.0f, 0.005f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("layer.d"), P + "Layer Decay", P + "Layer Decay", 0.001f, 10.0f, 0.3f, ParamUnit::Seconds, 0.3f));
    add (continuous (id ("layer.s"), P + "Layer Sustain", P + "Layer Sustain", 0.0f, 1.0f, 0.8f));
    add (continuous (id ("layer.r"), P + "Layer Release", P + "Layer Release", 0.001f, 10.0f, 0.3f, ParamUnit::Seconds, 0.3f));
}

void Part::bind (const ParamRegistry& reg)
{
    static const char* late08[numLate08] = { "layer.engine", "layer.level", "layer.tune", "layer.fine", "layer.xmode", "layer.xmod",
                                             "layer.width", "layer.ownenv", "layer.a", "layer.d", "layer.s", "layer.r" };
    for (int i = 0; i < numLate08; ++i) late08Index[(size_t) i] = reg.indexOf (id (late08[i]));
    exprSmoothIndex = reg.indexOf (id ("mpe.smooth"));
    exprSlideIndex = reg.indexOf (id ("mpe.slide"));
    monoBassIndex = reg.indexOf (id ("monobass"));
    lifeIndex = reg.indexOf (id ("life")); // p15
    glidePressIndex = reg.indexOf (id ("glide.press")); // p32
    userWtIndex = reg.indexOf (id ("usertable.wt"));       // p20
    userVocalIndex = reg.indexOf (id ("usertable.vocal")); // p20
    for (int m = 0; m < character::numMacros; ++m) charIndex[(size_t) m] = reg.indexOf (id (std::string ("char.") + character::key (m))); // p21
    spaceOnIndex = reg.indexOf (id ("space.on")); // 1.0
    resoOnIndex = reg.indexOf (id ("reso.on"));   // p19
    {
        const auto ds = reso.getParamDescriptors();
        for (size_t i = 0; i < ds.size() && i < resoIndex.size(); ++i) resoIndex[i] = reg.indexOf (id ("reso." + ds[i].id));
    }
    {
        const auto ds = space.getParamDescriptors();
        for (size_t i = 0; i < ds.size() && i < spaceIndex.size(); ++i) spaceIndex[i] = reg.indexOf (id ("space." + ds[i].id));
    }
    static const char* names[numCommon] = { "engine", "vol", "voices", "tune", "fine", "glide", "bend", "velsens",
                                            "amp.a", "amp.d", "amp.s", "amp.r", "mod.a", "mod.d", "mod.s", "mod.r",
                                            "mod.amt", "lfo.rate", "lfo.depth", "lfo.shape", "lfo.dest" };
    for (int i = 0; i < numCommon; ++i)
        commonIndex[(size_t) i] = reg.indexOf (id (names[i]));
    registry = &reg;
    for (int d = 0; d < mod::numDests; ++d) // matrix targets that name a part parameter (inert while it does not exist)
    {
        const auto& info = mod::destTable()[(size_t) d];
        partDestIndex[(size_t) d] = info.kind == mod::DestKind::PartParam && info.paramSuffix != nullptr ? reg.indexOf (id (info.paramSuffix)) : -1;
    }
    rack.bindPart (reg, idPrefix);
    matrix.bind (reg, idPrefix);
    gainIndex = reg.indexOf (id ("gain"));
    polyGlideIndex = reg.indexOf (id ("polyglide"));
    static const char* late06[numLate06] = { "glide.mode", "glide.rate", "pitchenv.amt", "pitchenv.time", "pitchenv.curve" };
    for (int i = 0; i < numLate06; ++i) late06Index[(size_t) i] = reg.indexOf (id (late06[i]));

    for (int t = 0; t < numEngineTypes; ++t)
    {
        auto& slot = slots[(size_t) t];
        slot.paramIndex.clear();
        slot.lastValue.clear();
        slot.charWeights.clear();
        if (engines[(size_t) t] == nullptr)
            continue;
        const std::string enginePrefix = idPrefix + engineTypeInfo ((EngineType) t).id + ".";
        for (const auto& d : engines[(size_t) t]->getParamDescriptors())
        {
            slot.paramIndex.push_back (reg.indexOf (enginePrefix + d.id));
            slot.lastValue.push_back (NAN);
            slot.charWeights.push_back (d.kind == ParamKind::Continuous ? character::engineWeights (d.id) : character::Weights {}); // p21
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
    rack.prepare (sampleRate, maxBlockSize);
    matrix.prepare (sampleRate);
    lastGain = -1.0f;
    layerL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    layerR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    xEnvCoeff = 1.0f - (float) std::exp (-1.0 / (0.004 * sampleRate));
    partSr = (float) sampleRate;
    monoLp1.prepare (partSr);
    monoLp2.prepare (partSr);
    monoLp1.set (monoBassHz, 0.299f); // Q 0.707 each
    monoLp2.set (monoBassHz, 0.299f);
    space.setSeed ((uint32_t) number * 0x51ED27u + 7u); // 1.0: every part its own random orbit
    space.prepare (sampleRate, maxBlockSize);
    spaceDryL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    spaceDryR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    spaceFadeStep = 1.0f / (0.02f * (float) sampleRate);
    spaceLast.fill (NAN);
    reso.setKeys (&keys); // p19: the body and the rack's resonators follow this part's keyboard
    rack.setKeys (&keys);
    reso.prepare (sampleRate, maxBlockSize);
    resoDryL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    resoDryR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    resoLast.fill (NAN);
    reset();
}

void Part::reset() noexcept
{
    for (auto& e : engines)
        if (e != nullptr) e->reset();
    sustain = false;
    sustained.fill (false);
    held.fill (0.0f); // p16
    bend = 0.0f;
    portaSwitch = true;
    ccGlideSeconds = -1.0f;
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
    rack.reset();
    matrix.reset();
    xEnv = 0.0f;
    exprBlend = 0.0f;
    monoLp1.reset();
    monoLp2.reset();
    space.reset();
    spaceFade = spaceOn ? 1.0f : 0.0f;
    keys = KeyState {}; // p19
    keyNoteBend = 0.0f;
    reso.reset();
    resoFade = resoOn ? 1.0f : 0.0f;
}

void Part::pullParams (const ParamSnapshot& s) noexcept
{
    applyParams (s);
    captureModBases (s);
}

void Part::applyParams (const ParamSnapshot& s) noexcept
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
    if (const float g = v (cGlide); g != lastGlideParam) { lastGlideParam = g; ccGlideSeconds = -1.0f; }
    if (glidePressIndex >= 0) // p32
    {
        const float a = s[glidePressIndex];
        glidePressAmt = std::isfinite (a) ? std::clamp (a, -1.0f, 1.0f) : 0.0f;
    }
    updateGlide();
    auto late = [&] (Late06 k, float def) { return late06Index[(size_t) k] >= 0 ? s[late06Index[(size_t) k]] : def; };
    common.glideMode = std::clamp ((int) std::lround (late (lGlideMode, 0.0f)), 0, 2);
    common.glideConstantRate = std::lround (late (lGlideRate, 0.0f)) == 1;
    common.pitchEnvAmount = late (lPitchEnvAmt, 0.0f);
    common.pitchEnvTime = late (lPitchEnvTime, 0.25f);
    common.pitchEnvCurve = late (lPitchEnvCurve, 0.5f);
    common.polyGlide = polyGlideIndex >= 0 && s.getBool (polyGlideIndex);
    common.velocitySens = v (cVelSens);
    // p21: the character macros (0 = neutral: every line below leaves the value untouched)
    charActive = false;
    for (int m = 0; m < character::numMacros; ++m)
    {
        const int ci = charIndex[(size_t) m];
        charAmount[(size_t) m] = ci >= 0 ? std::clamp (s[ci], -1.0f, 1.0f) : 0.0f;
        charActive = charActive || charAmount[(size_t) m] != 0.0f;
    }
    common.amp = { v (cAmpA), v (cAmpD), v (cAmpS), v (cAmpR) };
    if (charActive && registry != nullptr && charAmount[character::Hard] != 0.0f)
    {
        const auto& d = registry->descriptor (commonIndex[cAmpA]);
        common.amp.attack = d.fromNormalised (character::moved (d.toNormalised (common.amp.attack), character::ampAttackHard * charAmount[character::Hard]));
    }
    common.mod = { v (cModA), v (cModD), v (cModS), v (cModR) };
    common.modEnvAmount = v (cModAmount);
    common.lfoRate = v (cLfoRate);
    common.lfoDepth = v (cLfoDepth);
    common.lfoShape = s.getInt (commonIndex[cLfoShape]);
    common.lfoDest = (LfoDest) std::clamp (s.getInt (commonIndex[cLfoDest]), 0, (int) LfoDest::numDests - 1);
    common.pitchOffset = tune + bend * bendRange;
    common.modProgram = &matrix.voiceProgram();
    matrix.pull (s, common.amp, common.mod, common.lfoRate, common.lfoShape);
    rack.pullParams (s);
    common.exprSmooth = exprSmoothIndex >= 0 ? s[exprSmoothIndex] : 0.0f;
    common.exprSlide = exprSlideIndex >= 0 ? s[exprSlideIndex] : 0.0f;
    common.life = lifeIndex >= 0 ? dsp::clamp01 (s[lifeIndex]) : 0.0f; // p15 (the matrix may move it: "life" target)
    if (charActive) common.life = dsp::clamp01 (common.life + character::lifeAlive * charAmount[character::Alive]); // p21 ALIVE
    common.lifeSeed = 0x51ED27u * (uint32_t) number;
    // p20: user wavetable / user formants
    if (auto* e = engines[(size_t) EngineType::Wavetable].get()) e->setUserSource (userWtIndex >= 0 && s.getBool (userWtIndex));
    if (auto* e = engines[(size_t) EngineType::Vocal].get()) e->setUserSource (userVocalIndex >= 0 && s.getBool (userVocalIndex));
    if (monoBassIndex >= 0 && s[monoBassIndex] != monoBassHz) // 0.8 bass mono
    {
        monoBassHz = std::clamp (s[monoBassIndex], 20.0f, 400.0f);
        monoLp1.set (monoBassHz, 0.299f);
        monoLp2.set (monoBassHz, 0.299f);
    }

    // 1.0 spatialiser stage
    if (spaceOnIndex >= 0)
    {
        const bool on = s.getBool (spaceOnIndex);
        if (on && ! spaceOn) spaceRunningFlag.store (true, std::memory_order_relaxed);
        spaceOn = on;
        for (size_t i = 0; i < spaceIndex.size(); ++i)
            if (spaceIndex[i] >= 0)
                if (const float val = s[spaceIndex[i]]; ! (val == spaceLast[i])) { spaceLast[i] = val; space.setParam ((int) i, val); }
    }

    // p19 resonator body stage
    if (resoOnIndex >= 0)
    {
        const bool on = s.getBool (resoOnIndex);
        if (on && ! resoOn) resoRunningFlag.store (true, std::memory_order_relaxed);
        resoOn = on;
        for (size_t i = 0; i < resoIndex.size(); ++i)
            if (resoIndex[i] >= 0)
                if (const float val = s[resoIndex[i]]; ! (val == resoLast[i])) { resoLast[i] = val; reso.setParam ((int) i, val); }
    }
    keys.bendSemitones = tune + bend * bendRange + keyNoteBend; // the part's tuning moves the body too

    // 0.8 layer
    {
        auto ly = [&] (Late08 k, float def) { return late08Index[(size_t) k] >= 0 ? s[late08Index[(size_t) k]] : def; };
        int lt = (int) std::lround (ly (lyEngine, 0.0f)) - 1;
        if (lt < 0 || lt >= numEngineTypes || lt == (int) current || engines[(size_t) lt] == nullptr) lt = -1;
        if (lt != layerType)
        {
            if (auto* old = layerEngine()) old->allNotesOff (false); // let it ring out (as a plain engine)
            layerType = lt;
        }
        layerLevel = dsp::clamp01 (ly (lyLevel, 0.7f));
        layerTune = ly (lyTune, 0.0f) + 0.01f * ly (lyFine, 0.0f);
        layerXMode = std::clamp ((int) std::lround (ly (lyXMode, 0.0f)), 0, numXMods - 1);
        layerXMod = dsp::clamp01 (ly (lyXMod, 0.0f));
        layerWidth = dsp::clamp01 (ly (lyWidth, 0.5f) + (charActive ? character::layerWidthWide * charAmount[character::Wide] : 0.0f)); // p21 WIDE
        layerOwnEnv = ly (lyOwnEnv, 0.0f) >= 0.5f;
        layerAmp = { ly (lyA, 0.005f), ly (lyD, 0.3f), ly (lyS, 0.8f), ly (lyR, 0.3f) };
    }

    for (int t = 0; t < numEngineTypes; ++t)
    {
        auto* e = engines[(size_t) t].get();
        if (e == nullptr) continue;
        auto& slot = slots[(size_t) t];
        for (size_t i = 0; i < slot.paramIndex.size(); ++i)
        {
            float val = s[slot.paramIndex[i]];
            if (charActive && registry != nullptr && i < slot.charWeights.size() && slot.charWeights[i].any()) // p21
            {
                const auto& d = registry->descriptor (slot.paramIndex[i]);
                val = d.fromNormalised (character::moved (d.toNormalised (val), character::offset (slot.charWeights[i], charAmount)));
            }
            if (! (val == slot.lastValue[i]))
            {
                slot.lastValue[i] = val;
                e->setParam ((int) i, val);
            }
        }
        e->setPolyphony (polyphony);
    }
    pushCommon();
}

void Part::pushCommon() noexcept
{
    for (auto& e : engines)
        if (e != nullptr) e->setCommon (common);
    if (auto* le = layerEngine())
    {
        VoiceCommon c = common;
        c.pitchOffset += layerTune;
        c.lifeSeed ^= 0x5BD1E995u; // p15: the layer's voices move on their own
        if (layerOwnEnv) c.amp = layerAmp;
        le->setCommon (c);
    }
}

void Part::noteOn (int note, float velocity) noexcept
{
    if (note < 0 || note > 127) return;
    held[(size_t) note] = std::max (velocity, 1.0e-3f); // p16
    for (auto* f : followers)
        if (f != nullptr) f->noteOn (note, velocity);
    sustained[(size_t) note] = false;
    keys.lastNote = note; // p19: the resonator body follows the newest note
    keys.velocity = velocity;
    keys.pressure = 0.0f;
    keys.slide = 0.0f;
    keyNoteBend = 0.0f;
    keys.bendSemitones = tune + bend * bendRange;
    ++keys.noteCounter;
    updateHeldKeys();
    matrix.noteOn (note, velocity); // first: the voice reads the new note's seed
    if (glidePressAmt != 0.0f) // p32: the new note's own pressure starts at 0 (the channel pressure stays)
    {
        const float before = common.glideSeconds;
        updateGlide();
        if (common.glideSeconds != before) pushCommon();
    }
    rack.noteOn();                  // 0.7: retriggered rack effects
    if (auto* e = engines[(size_t) current].get()) e->noteOn (note, velocity);
    if (auto* le = layerEngine()) le->noteOn (note, velocity); // 0.8 layer
}

void Part::noteOff (int note) noexcept
{
    if (note < 0 || note > 127) return;
    held[(size_t) note] = 0.0f; // p16
    for (auto* f : followers)
        if (f != nullptr) f->noteOff (note);
    if (sustain) { sustained[(size_t) note] = true; updateHeldKeys(); return; }
    updateHeldKeys(); // p19
    matrix.noteOff (note);
    for (auto& e : engines)
        if (e != nullptr) e->noteOff (note);
}

void Part::allNotesOff (bool hard) noexcept
{
    sustained.fill (false);
    held.fill (0.0f); // p16
    keys.numHeld = 0;  // p19
    for (auto* f : followers)
        if (f != nullptr) f->allNotesOff (hard);
    matrix.allNotesOff();
    for (auto& e : engines)
        if (e != nullptr) e->allNotesOff (hard);
}

void Part::noteExpression (int note, const NoteExpression& x) noexcept
{
    for (auto* f : followers) // p16
        if (f != nullptr) f->noteExpression (note, x);
    matrix.setNoteExpression (note, x.pressure, x.slide, x.noteBend);
    if (note < 0 || note == keys.lastNote) // p19: the body follows the newest note's expression
    {
        keys.pressure = std::isfinite (x.pressure) ? std::clamp (x.pressure, 0.0f, 1.0f) : 0.0f;
        keys.slide = std::isfinite (x.slide) ? std::clamp (x.slide, -1.0f, 1.0f) : 0.0f;
        keyNoteBend = std::isfinite (x.pitch) ? std::clamp (x.pitch, -96.0f, 96.0f) : 0.0f;
        keys.bendSemitones = tune + bend * bendRange + keyNoteBend;
    }
    exprBlend = std::clamp (x.blend, -1.0f, 1.0f); // 0.8: part-wide (the mix of two engines is not per note)
    if (glidePressAmt != 0.0f) // p32: the pressure moves the glide time of new and ongoing glides at once
    {
        const float before = common.glideSeconds;
        updateGlide();
        if (common.glideSeconds != before) pushCommon();
    }
    for (auto& e : engines)
        if (e != nullptr) e->noteExpression (note, x);
}

void Part::handleMidi (const MidiEvent& m) noexcept
{
    if (! m.isNoteOn() && ! m.isNoteOff())
        for (auto* f : followers) // p16: controllers, bend ... (notes reach them through noteOn / noteOff)
            if (f != nullptr) f->handleMidi (m);
    if (m.isNoteOn()) noteOn (m.data1, (float) m.data2 / 127.0f);
    else if (m.isNoteOff()) noteOff (m.data1);
    else if (m.isPitchBend())
    {
        bend = ((float) m.pitchBendValue() - 8192.0f) / 8192.0f;
        matrix.setPitchBend (bend);
        common.pitchOffset = tune + bend * bendRange;
        keys.bendSemitones = tune + bend * bendRange + keyNoteBend; // p19
        pushCommon();
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
        else if (m.data1 == 65 || m.data1 == 5) // 0.6: portamento on / off, portamento time
        {
            if (m.data1 == 65) portaSwitch = m.data2 >= 64;
            else
            {
                const float x = (float) m.data2 / 127.0f;
                ccGlideSeconds = 2.0f * x * x; // same range as the "glide" parameter, finer at short times
            }
            updateGlide();
            pushCommon();
        }
        else if (m.data1 == 1) matrix.setModWheel ((float) m.data2 / 127.0f); // 0.7: matrix source
        else if (m.data1 == 120) allNotesOff (true);
        else if (m.data1 == 123) allNotesOff (false);
    }
}

void Part::renderEngines (float* l, float* r, int numSamples) noexcept
{
    std::memset (l, 0, sizeof (float) * (size_t) numSamples);
    std::memset (r, 0, sizeof (float) * (size_t) numSamples);
    auto* le = layerEngine();
    for (auto& e : engines)
        if (e != nullptr && e.get() != le && e->activeVoiceCount() > 0)
            e->render (l, r, numSamples);
    if (le != nullptr && le->activeVoiceCount() > 0 && numSamples <= (int) layerL.size()) // 0.8 layer
    {
        float* a = layerL.data();
        float* b = layerR.data();
        std::memset (a, 0, sizeof (float) * (size_t) numSamples);
        std::memset (b, 0, sizeof (float) * (size_t) numSamples);
        le->render (a, b, numSamples);
        const float side = 2.0f * layerWidth, x = layerXMod;
        // 0.8: the "layer blend" expression (MPE / aftertouch) moves the balance between main engine and layer
        const float layerScale = std::clamp (1.0f + exprBlend, 0.0f, 2.0f), mainScale = std::clamp (1.0f - 0.5f * exprBlend, 0.5f, 1.5f);
        if (exprBlend != 0.0f)
            for (int i = 0; i < numSamples; ++i) { l[i] *= mainScale; r[i] *= mainScale; }
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = 0.5f * (a[i] + b[i]), sd = 0.5f * (a[i] - b[i]) * side;
            const float la = (m + sd) * layerLevel * layerScale, lb = (m - sd) * layerLevel * layerScale;
            if (x <= 0.0f) { l[i] += la; r[i] += lb; continue; }
            const float lm = 0.5f * (la + lb);
            switch (layerXMode)
            {
                case XAm: // the layer's envelope shapes the main engine (chopped / gated / talking)
                {
                    xEnv = dsp::flush (xEnv + (std::abs (lm) - xEnv) * xEnvCoeff);
                    const float g = (1.0f - x) + x * std::min (2.5f, 4.0f * xEnv);
                    l[i] = l[i] * g + la;
                    r[i] = r[i] * g + lb;
                    break;
                }
                case XFold: // the sum is folded: harmonics move with both engines
                {
                    const float k = 1.0f + 5.0f * x;
                    const float sl = l[i] + la, sr2 = r[i] + lb;
                    l[i] = (1.0f - x) * sl + x * 0.6f * std::sin (k * sl * 1.5707963f);
                    r[i] = (1.0f - x) * sr2 + x * 0.6f * std::sin (k * sr2 * 1.5707963f);
                    break;
                }
                default: // ring: the main engine multiplied by the layer
                {
                    const float ring = dsp::softClip (4.0f * lm) * 1.4f;
                    l[i] = l[i] * (1.0f - x) + l[i] * ring * x + la * (1.0f - 0.5f * x);
                    r[i] = r[i] * (1.0f - x) + r[i] * ring * x + lb * (1.0f - 0.5f * x);
                    break;
                }
            }
        }
    }
}

void Part::finishBlock (float* l, float* r, int numSamples) noexcept
{
    applyReso (l, r, numSamples); // p19: the body right after the engines (before volume and FX rack)
    // 0.4: a gentle DC blocker (one pole at ~5 Hz): pulse shapes and asymmetric waves must not
    // push a DC offset into the mix. 0.7: the volume glides over the block when it changed (automation, matrix).
    const float g = volume * gainFactor;
    const float g0 = lastGain < 0.0f ? g : lastGain;
    lastGain = g;
    const float step = (g - g0) / (float) std::max (1, numSamples);
    float* ch[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        float x1 = dcX[c], y1 = dcY[c];
        if (g0 == g)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = ch[c][i];
                y1 = x - x1 + dcCoeff * y1;
                x1 = x;
                ch[c][i] = y1 * g;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = ch[c][i];
                y1 = x - x1 + dcCoeff * y1;
                x1 = x;
                ch[c][i] = y1 * (g0 + step * (float) (i + 1));
            }
        }
        dcX[c] = x1;
        dcY[c] = (std::abs (y1) < 1.0e-20f) ? 0.0f : y1; // no denormals in silence
    }
    rack.process (l, r, numSamples); // 0.7: the FX rack (slot 1 = the 0.2 FX slot)
    applyBassMono (l, r, numSamples); // 0.8: after the rack, so no effect widens the sub again
    applySpace (l, r, numSamples);    // 1.0: the 3D stage last (it keeps the low band mono itself)
}

void Part::render (float* l, float* r, int numSamples) noexcept
{
    if (! matrix.hasParamRoutes() || liveSnapshot == nullptr || numModTargets == 0)
    {
        matrix.advance (numSamples);
        matrix.tickParamRoutes (numSamples); // display values
        renderEngines (l, r, numSamples);
        finishBlock (l, r, numSamples);
    }
    else
    {
        // routings to parameters: modulated values every control block (32 samples), glided by the matrix
        for (int pos = 0; pos < numSamples; pos += controlBlock)
        {
            const int len = std::min (controlBlock, numSamples - pos);
            matrix.advance (len);
            matrix.tickParamRoutes (len);
            writeModulation();
            applyParams (*liveSnapshot);
            renderEngines (l + pos, r + pos, len);
            finishBlock (l + pos, r + pos, len);
        }
    }
    matrix.publishUi();
}

int Part::destParamIndex (int dest) const noexcept
{
    using namespace mod;
    if (dest <= DstOff || dest >= numDests) return -1;
    const auto& d = destTable()[(size_t) dest];
    switch (d.kind)
    {
        case DestKind::EngineParam:
        {
            const auto& slot = slots[(size_t) current];
            return d.arg < (int) slot.paramIndex.size() ? slot.paramIndex[(size_t) d.arg] : -1;
        }
        case DestKind::PartParam: return partDestIndex[(size_t) dest];
        case DestKind::FxMix: return d.arg < rack.numSlots() ? rack.mixParamIndex (d.arg) : -1;
        case DestKind::FxParam:
        {
            const int k = d.arg / numFxDestParams, j = d.arg % numFxDestParams;
            if (k >= rack.numSlots()) return -1;
            const auto& sl = rack.slot (k);
            return sl.modParamIndexOf (sl.currentType(), j);
        }
        default: return -1; // off, per voice
    }
}

bool Part::destAvailable (int dest) const noexcept
{
    if (dest <= mod::DstOff || dest >= mod::numDests) return dest == mod::DstOff;
    const auto& d = mod::destTable()[(size_t) dest];
    return d.kind != mod::DestKind::PartParam || partDestIndex[(size_t) dest] >= 0;
}

void Part::captureModBases (const ParamSnapshot& s) noexcept
{
    numModTargets = 0;
    slotTarget.fill (-1);
    if (! matrix.hasParamRoutes() || registry == nullptr) { for (auto& u : uiTargetIndex) u.store (-1, std::memory_order_relaxed); return; }
    const auto& routes = matrix.routes();
    for (int k = 0; k < mod::numSlots; ++k)
    {
        if (! matrix.isParamRoute (k)) continue;
        const int idx = destParamIndex (routes[(size_t) k].dst);
        if (idx < 0 || idx >= s.size()) continue;
        int t = 0;
        while (t < numModTargets && modTargets[(size_t) t].index != idx) ++t;
        if (t == numModTargets)
        {
            modTargets[(size_t) t] = { idx, registry->descriptor (idx).toNormalised (s[idx]) };
            ++numModTargets;
        }
        slotTarget[(size_t) k] = t;
    }
    for (int t = 0; t < mod::numSlots; ++t)
        uiTargetIndex[(size_t) t].store (t < numModTargets ? modTargets[(size_t) t].index : -1, std::memory_order_relaxed);
}

void Part::writeModulation() noexcept
{
    std::array<float, mod::numSlots> delta {};
    for (int k = 0; k < mod::numSlots; ++k)
        if (const int t = slotTarget[(size_t) k]; t >= 0) delta[(size_t) t] += matrix.routeValue (k);
    for (int t = 0; t < numModTargets; ++t)
    {
        const auto& m = modTargets[(size_t) t];
        const auto& d = registry->descriptor (m.index);
        const float norm = dsp::clamp01 (m.baseNorm + delta[(size_t) t]);
        liveSnapshot->setModulated (m.index, d.fromNormalised (norm));
        uiTargetDelta[(size_t) t].store (norm - m.baseNorm, std::memory_order_relaxed);
    }
}

float Part::modulationOf (int registryIndex) const noexcept
{
    if (registryIndex < 0) return 0.0f;
    for (int t = 0; t < mod::numSlots; ++t)
        if (uiTargetIndex[(size_t) t].load (std::memory_order_relaxed) == registryIndex)
            return uiTargetDelta[(size_t) t].load (std::memory_order_relaxed);
    return 0.0f;
}

void Part::updateGlide() noexcept
{
    float t = ccGlideSeconds >= 0.0f ? ccGlideSeconds : std::max (0.0f, lastGlideParam);
    if (glidePressAmt != 0.0f && t > 0.0f) // p32: aftertouch scales the glide time (0 = exactly the old time)
    {
        float p = std::max (matrix.sourceValue (mod::SrcChannelPressure), matrix.sourceValue (mod::SrcNotePressure));
        p = std::isfinite (p) ? std::clamp (p, 0.0f, 1.0f) : 0.0f;
        const float k = glidePressAmt > 0.0f ? 1.0f + 3.0f * glidePressAmt * p : std::max (0.0f, 1.0f + glidePressAmt * p);
        t *= k;
    }
    common.glideSeconds = portaSwitch ? t : 0.0f;
}

int Part::activeVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& e : engines)
        if (e != nullptr) n += e->activeVoiceCount();
    return n;
}
} // namespace ohpe
