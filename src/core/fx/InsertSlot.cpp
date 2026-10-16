// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InsertSlot.h"

#include "Delay.h"
#include "InsertEffects.h"
#include "MidSide.h"
#include "RackEffects.h"
#include "InsertEffects2.h"
#include "Reverb.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ohpe
{
std::unique_ptr<IEffect> InsertSlot::create (int t)
{
    switch (t)
    {
        case Chorus: return std::make_unique<ChorusEffect>();
        case Phaser: return std::make_unique<PhaserEffect>();
        case Bitcrush: return std::make_unique<BitcrushEffect>();
        case FilterDrive: return std::make_unique<FilterDriveEffect>();
        case Delay: return std::make_unique<DelayEffect>();
        case Reverb: return std::make_unique<ReverbEffect>();
        case Hyper: return std::make_unique<HyperEffect>();
        case Distortion: return std::make_unique<DistortionEffect>();
        case Flanger: return std::make_unique<FlangerRackEffect>();
        case Compressor: return std::make_unique<CompressorEffect>();
        case Multiband: return std::make_unique<MultibandEffect>();
        case Eq: return std::make_unique<EqEffect>();
        case Filter: return std::make_unique<FilterRackEffect>();
        case Echo: return std::make_unique<EchoEffect>();
        case Hall: return std::make_unique<HallEffect>();
        case MidSide: return std::make_unique<MidSideEffect>();
        case Comb: return std::make_unique<CombEffect>();
        case Formant: return std::make_unique<FormantEffect>();
        case Wobble: return std::make_unique<WobbleEffect>();
        case Tremolo: return std::make_unique<TremoloEffect>();
        case Width: return std::make_unique<WidthEffect>();
        default: return nullptr;
    }
}

InsertSlot::InsertSlot()
{
    for (int t = 0; t < numTypes; ++t) effects[(size_t) t] = create (t);
    mixIndex.fill (-1);
}

// English names: the UI derives a type's parameter block from them only for the 0.2 types; every type's block ID is
// its EffectInfo::id. Append-only.
std::vector<std::string> InsertSlot::typeNamesEn()
{
    return { "Off", "Chorus", "Phaser", "Bitcrush", "Filter Drive", "Delay", "Reverb",
             "Hyper", "Distortion", "Flanger", "Compressor", "Multiband", "EQ", "Filter", "Echo", "Hall", "Mid/Side",
             "Comb", "Formant", "Wobble", "Tremolo", "Width" }; // 0.8
}
std::vector<std::string> InsertSlot::typeNamesDe()
{
    return { "Aus", "Chorus", "Phaser", "Bitcrusher", "Filter-Drive", "Delay", "Hall",
             "Hyper", "Verzerrung", "Flanger", "Kompressor", "Multiband", "EQ", "Filter", "Echo", "Raum", "Mitte/Seite",
             "Kammfilter", "Formant", "Wobble", "Tremolo", "Stereobreite" }; // 0.8
}

EffectInfo InsertSlot::typeInfo (int t)
{
    if (auto e = create (t)) return e->info();
    return { nullptr, "Off", "Aus", "" };
}

std::string InsertSlot::hostNamePrefix (const std::string& prefix, bool german)
{
    if (prefix == "p1.fx.") return {}; // the 0.2 slot keeps its names
    if (prefix.rfind ("mfx", 0) == 0) return (german ? "Master-FX " : "Master FX ") + prefix.substr (3, prefix.size() - 4) + " ";
    const auto dot = prefix.find ('.');
    std::string slot = prefix.substr (dot + 3, prefix.size() - dot - 4); // "fx3." -> "3", "fx." -> ""
    if (slot.empty()) slot = "1";
    return "P" + prefix.substr (1, dot - 1) + " FX" + slot + " ";
}

void InsertSlot::registerParams (ParamRegistry& reg, const std::string& prefix, const std::string& group)
{
    using namespace pd;
    auto t = choice (prefix + "type", hostNamePrefix (prefix, false) + "FX Slot", hostNamePrefix (prefix, true) + "FX-Slot", typeNamesEn(),
                     typeNamesDe(), Off);
    t.group = group;
    reg.add (std::move (t));
    registerBlocks (reg, prefix, group, Off + 1, numLegacyTypes);
}

void InsertSlot::registerBlocks (ParamRegistry& reg, const std::string& prefix, const std::string& group, int fromType, int toType)
{
    using namespace pd;
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    for (int t = std::max (fromType, Off + 1); t < std::min (toType, (int) numTypes); ++t)
    {
        auto e = create (t);
        const std::string p = prefix + e->info().id + ".";
        auto descs = e->getParamDescriptors();
        if (t == Chorus) descs.resize ((size_t) ChorusEffect::Voices); // 0.8 ensemble params: registered later (registerBlockTail)
        for (auto d : descs)
        {
            d.id = p + d.id;
            d.nameEn = hostNamePrefix (prefix, false) + d.nameEn;
            d.nameDe = hostNamePrefix (prefix, true) + d.nameDe;
            add (std::move (d));
        }
        if (isWetOnly (t))
            add (continuous (p + "mix", hostNamePrefix (prefix, false) + e->info().nameEn + " Mix",
                             hostNamePrefix (prefix, true) + e->info().nameDe + "-Mischung", 0.0f, 1.0f, 0.35f));
    }
}

void InsertSlot::registerBlockTail (ParamRegistry& reg, const std::string& prefix, const std::string& group, int type, int firstParam)
{
    auto e = create (type);
    if (e == nullptr) return;
    const std::string p = prefix + e->info().id + ".";
    const auto descs = e->getParamDescriptors();
    for (size_t i = (size_t) std::max (0, firstParam); i < descs.size(); ++i)
    {
        auto d = descs[i];
        d.id = p + d.id;
        d.nameEn = hostNamePrefix (prefix, false) + d.nameEn;
        d.nameDe = hostNamePrefix (prefix, true) + d.nameDe;
        d.group = group;
        reg.add (std::move (d));
    }
}

void InsertSlot::bind (const ParamRegistry& reg, const std::string& prefix)
{
    typeIndex = reg.indexOf (prefix + "type");
    for (int t = Off + 1; t < numTypes; ++t)
    {
        auto& e = effects[(size_t) t];
        const std::string p = prefix + e->info().id + ".";
        paramIndex[(size_t) t].clear();
        for (const auto& d : e->getParamDescriptors()) paramIndex[(size_t) t].push_back (reg.indexOf (p + d.id)); // -1: not registered
        lastValue[(size_t) t].assign (paramIndex[(size_t) t].size(), NAN);
        mixIndex[(size_t) t] = isWetOnly (t) ? reg.indexOf (p + "mix") : -1;
    }
}

void InsertSlot::prepare (double sampleRate, int maxBlockSize)
{
    const bool rateChanged = sampleRate != preparedRate || maxBlockSize != preparedBlock;
    preparedRate = sampleRate;
    preparedBlock = maxBlockSize;
    for (int t = Off + 1; t < numTypes; ++t)
    {
        auto& r = ready[(size_t) t];
        // lazy slots: keep (and re-prepare) the types prepared before, nothing else
        if (lazy && ! r.load (std::memory_order_relaxed)) continue;
        if (lazy && ! rateChanged) { effects[(size_t) t]->reset(); continue; }
        r.store (false, std::memory_order_release);
        effects[(size_t) t]->prepare (sampleRate, maxBlockSize);
        r.store (true, std::memory_order_release);
    }
    dryL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    dryR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    for (auto& v : lastValue) std::fill (v.begin(), v.end(), NAN);
}

bool InsertSlot::prepareSelected (const ParamRegistry& reg)
{
    if (typeIndex < 0 || preparedRate <= 0.0) return false;
    const int t = std::clamp ((int) std::lround (reg.get (typeIndex)), 0, numTypes - 1);
    if (t == Off || ready[(size_t) t].load (std::memory_order_acquire)) return false;
    if (paramIndex[(size_t) t].empty() || paramIndex[(size_t) t].front() < 0) return false; // type not registered here
    effects[(size_t) t]->prepare (preparedRate, preparedBlock);
    std::fill (lastValue[(size_t) t].begin(), lastValue[(size_t) t].end(), NAN);
    ready[(size_t) t].store (true, std::memory_order_release);
    return true;
}

void InsertSlot::reset() noexcept
{
    for (int t = Off + 1; t < numTypes; ++t)
        if (ready[(size_t) t].load (std::memory_order_acquire)) effects[(size_t) t]->reset();
}

void InsertSlot::pullParams (const ParamSnapshot& s) noexcept
{
    int t = typeIndex >= 0 ? std::clamp (s.getInt (typeIndex), 0, numTypes - 1) : Off;
    if (t != Off && (paramIndex[(size_t) t].empty() || paramIndex[(size_t) t].front() < 0)) t = Off; // type not registered here
    const bool isReady = t == Off || ready[(size_t) t].load (std::memory_order_acquire);
    waiting.store (! isReady, std::memory_order_relaxed);
    if (! isReady) t = Off; // lazy slot: dry until the message thread prepared the type
    if (t != type)
    {
        type = t;
        if (auto* e = effect (type)) e->reset();
    }
    if (type == Off) return;
    auto& idx = paramIndex[(size_t) type];
    auto& last = lastValue[(size_t) type];
    auto* e = effects[(size_t) type].get();
    for (size_t i = 0; i < idx.size(); ++i)
    {
        if (idx[i] < 0) continue; // parameter not registered for this slot: keep the effect's default
        const float v = s[idx[i]];
        if (! (v == last[i])) { last[i] = v; e->setParam ((int) i, v); }
    }
    if (mixIndex[(size_t) type] >= 0) wetMix = dsp::clamp01 (s[mixIndex[(size_t) type]]);
}

void InsertSlot::setTempo (double bpm) noexcept
{
    if (auto* e = effect (type)) e->setTempo (bpm);
}

void InsertSlot::noteOn() noexcept
{
    if (auto* e = effect (type)) e->noteOn();
}

void InsertSlot::process (float* l, float* r, int n) noexcept
{
    if (type == Off) return;
    auto* e = effects[(size_t) type].get();
    if (! isWetOnly (type))
    {
        e->process (l, r, n);
        return;
    }
    n = std::min (n, (int) dryL.size());
    std::memcpy (dryL.data(), l, sizeof (float) * (size_t) n);
    std::memcpy (dryR.data(), r, sizeof (float) * (size_t) n);
    e->process (l, r, n);
    for (int i = 0; i < n; ++i)
    {
        l[i] = dryL[(size_t) i] * (1.0f - wetMix) + l[i] * wetMix;
        r[i] = dryR[(size_t) i] * (1.0f - wetMix) + r[i] * wetMix;
    }
}
} // namespace ohpe
