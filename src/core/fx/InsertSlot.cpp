// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InsertSlot.h"

#include "Delay.h"
#include "InsertEffects.h"
#include "Reverb.h"

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
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    add (choice (prefix + "type", "FX Slot", "FX-Slot",
                 { "Off", "Chorus", "Phaser", "Bitcrush", "Filter Drive", "Delay", "Reverb" },
                 { "Aus", "Chorus", "Phaser", "Bitcrusher", "Filter-Drive", "Delay", "Hall" }, Off));
    for (int t = Off + 1; t < numTypes; ++t)
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
        for (const auto& d : e->getParamDescriptors()) paramIndex[(size_t) t].push_back (reg.indexOf (p + d.id));
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
    const int t = typeIndex >= 0 ? std::clamp (s.getInt (typeIndex), 0, numTypes - 1) : Off;
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
