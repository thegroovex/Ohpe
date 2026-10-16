// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InsertSlot.h"

#include "Delay.h"
#include "InsertEffects.h"
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
        case Shaper: return std::make_unique<ShaperEffect>();
        case Squash: return std::make_unique<SquashEffect>();
        case Comb: return std::make_unique<CombEffect>();
        case Formant: return std::make_unique<FormantEffect>();
        case Flanger: return std::make_unique<FlangerEffect>();
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

void InsertSlot::registerParams (ParamRegistry& reg, const std::string& prefix, const std::string& group)
{
    using namespace pd;
    // English names: the UI derives the parameter block from them (lower case, no spaces = effect ID).
    // Append-only: 0.8 appended Shaper ... Width (their blocks: registerLateParams).
    std::vector<std::string> en { "Off", "Chorus", "Phaser", "Bitcrush", "Filter Drive", "Delay", "Reverb",
                                  "Shaper", "Squash", "Comb", "Formant", "Flanger", "Wobble", "Tremolo", "Width" };
    std::vector<std::string> de { "Aus", "Chorus", "Phaser", "Bitcrusher", "Filter-Drive", "Delay", "Hall",
                                  "Waveshaper", "Squash", "Kammfilter", "Formant", "Flanger", "Wobble", "Tremolo", "Stereobreite" };
    auto t = choice (prefix + "type", "FX Slot", "FX-Slot", en, de, Off);
    t.group = group;
    reg.add (std::move (t));
    registerBlocks (reg, prefix, group, Off + 1, numLegacyTypes);
}

void InsertSlot::registerLateParams (ParamRegistry& reg, const std::string& prefix, const std::string& group)
{
    registerBlocks (reg, prefix, group, numLegacyTypes, numTypes);
}

void InsertSlot::registerBlocks (ParamRegistry& reg, const std::string& prefix, const std::string& group, int from, int to)
{
    using namespace pd;
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    for (int t = from; t < to; ++t)
    {
        auto e = create (t);
        const std::string p = prefix + e->info().id + ".";
        for (auto d : e->getParamDescriptors())
        {
            d.id = p + d.id;
            add (std::move (d));
        }
        if (isWetOnly (t))
            add (continuous (p + "mix", std::string (e->info().nameEn) + " Mix", std::string (e->info().nameDe) + "-Mischung", 0.0f, 1.0f, 0.35f));
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
        for (const auto& d : e->getParamDescriptors()) paramIndex[(size_t) t].push_back (reg.indexOf (p + d.id)); // -1 when not exposed
        lastValue[(size_t) t].assign (paramIndex[(size_t) t].size(), NAN);
        mixIndex[(size_t) t] = isWetOnly (t) ? reg.indexOf (p + "mix") : -1;
    }
}

void InsertSlot::prepare (double sampleRate, int maxBlockSize)
{
    for (auto& e : effects)
        if (e != nullptr) e->prepare (sampleRate, maxBlockSize);
    dryL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    dryR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    for (auto& v : lastValue) std::fill (v.begin(), v.end(), NAN);
}

void InsertSlot::reset() noexcept
{
    for (auto& e : effects)
        if (e != nullptr) e->reset();
}

void InsertSlot::pullParams (const ParamSnapshot& s) noexcept
{
    int t = typeIndex >= 0 ? std::clamp (s.getInt (typeIndex), 0, numTypes - 1) : Off;
    if (t != Off && ! paramIndex[(size_t) t].empty() && paramIndex[(size_t) t].front() < 0) t = Off; // type not registered here
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
        if (const float v = s[idx[i]]; ! (v == last[i])) { last[i] = v; e->setParam ((int) i, v); }
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
