// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FxRack.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ohpe
{
std::string FxRack::partSlotPrefix (const std::string& partPrefix, int slot)
{
    return slot <= 0 ? partPrefix + "fx." : partPrefix + "fx" + std::to_string (slot + 1) + ".";
}

std::string FxRack::masterSlotPrefix (int slot) { return "mfx" + std::to_string (slot + 1) + "."; }

std::vector<std::string> FxRack::partPrefixes (const std::string& partPrefix)
{
    std::vector<std::string> v;
    for (int s = 0; s < numPartSlots; ++s) v.push_back (partSlotPrefix (partPrefix, s));
    return v;
}

std::vector<std::string> FxRack::masterPrefixes()
{
    std::vector<std::string> v;
    for (int s = 0; s < numMasterSlots; ++s) v.push_back (masterSlotPrefix (s));
    return v;
}

std::vector<std::string> FxRack::allSlotPrefixes (int numParts)
{
    std::vector<std::string> v;
    for (int p = 1; p <= numParts; ++p)
        for (const auto& s : partPrefixes ("p" + std::to_string (p) + ".")) v.push_back (s);
    for (const auto& s : masterPrefixes()) v.push_back (s);
    return v;
}

std::string FxRack::groupOf (const std::string& slotPrefix)
{
    if (slotPrefix.rfind ("mfx", 0) == 0) return "master/fx";
    const auto dot = slotPrefix.find ('.');
    return "part" + slotPrefix.substr (1, dot - 1) + "/fx";
}

void FxRack::registerExtraSlots (ParamRegistry& reg, const std::string& partPrefix, const std::string& group)
{
    for (int s = 1; s < numPartSlots; ++s)
    {
        const auto pre = partSlotPrefix (partPrefix, s);
        InsertSlot::registerParams (reg, pre, group);
    }
}

namespace
{
void addControls (ParamRegistry& reg, const std::string& pre, const std::string& group, const std::string& en, const std::string& de)
{
    using namespace pd;
    auto m = continuous (pre + "mix", en + " Mix", de + " Mischung", 0.0f, 1.0f, 1.0f, ParamUnit::Percent);
    m.group = group;
    reg.add (std::move (m));
    auto b = toggle (pre + "bypass", en + " Bypass", de + " Bypass", false);
    b.group = group;
    reg.add (std::move (b));
}
} // namespace

void FxRack::registerSlotControls (ParamRegistry& reg, const std::string& partPrefix, const std::string& group)
{
    const auto n = partPrefix.substr (1, partPrefix.size() - 2);
    for (int s = 0; s < numPartSlots; ++s)
        addControls (reg, partSlotPrefix (partPrefix, s), group, "Part " + n + " FX " + std::to_string (s + 1),
                     "Part " + n + " FX " + std::to_string (s + 1));
}

void FxRack::registerMaster (ParamRegistry& reg)
{
    for (int s = 0; s < numMasterSlots; ++s) InsertSlot::registerParams (reg, masterSlotPrefix (s), "master/fx");
    for (int s = 0; s < numMasterSlots; ++s)
        addControls (reg, masterSlotPrefix (s), "master/fx", "Master FX " + std::to_string (s + 1), "Master-FX " + std::to_string (s + 1));
}

void FxRack::registerTypeBlocks (ParamRegistry& reg, int numParts, int fromType, int toType)
{
    for (const auto& pre : allSlotPrefixes (numParts)) InsertSlot::registerBlocks (reg, pre, groupOf (pre), fromType, toType);
}

FxRack::FxRack (int slots) : slotCount (std::clamp (slots, 1, maxSlots))
{
    mixIndex.fill (-1);
    bypassIndex.fill (-1);
    mix.fill (1.0f);
    mixSmooth.fill (1.0f);
    for (auto& p : peaks) p.store (0.0f);
}

void FxRack::bindSlots (const ParamRegistry& reg)
{
    for (int s = 0; s < slotCount; ++s)
    {
        slots[(size_t) s].bind (reg, prefixes[(size_t) s]);
        mixIndex[(size_t) s] = reg.indexOf (prefixes[(size_t) s] + "mix");
        bypassIndex[(size_t) s] = reg.indexOf (prefixes[(size_t) s] + "bypass");
    }
}

void FxRack::bindPart (const ParamRegistry& reg, const std::string& partPrefix)
{
    for (int s = 0; s < slotCount; ++s)
    {
        prefixes[(size_t) s] = partSlotPrefix (partPrefix, s);
        slots[(size_t) s].setLazy (s > 0); // slot 1: every type ready at once (as the 0.2 slot)
    }
    bindSlots (reg);
}

void FxRack::bindMaster (const ParamRegistry& reg)
{
    for (int s = 0; s < slotCount; ++s)
    {
        prefixes[(size_t) s] = masterSlotPrefix (s);
        slots[(size_t) s].setLazy (true);
    }
    bindSlots (reg);
}

void FxRack::prepare (double sampleRate, int maxBlockSize)
{
    for (int s = 0; s < slotCount; ++s) slots[(size_t) s].prepare (sampleRate, maxBlockSize);
    dryL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    dryR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    smoothCoeff = 1.0f - std::exp (-1.0f / (0.01f * (float) sampleRate)); // ~10 ms mix / bypass glide
    for (int s = 0; s < slotCount; ++s) mixSmooth[(size_t) s] = bypass[(size_t) s] ? 0.0f : mix[(size_t) s];
}

void FxRack::reset() noexcept
{
    for (int s = 0; s < slotCount; ++s) slots[(size_t) s].reset();
}

bool FxRack::prepareSelected (const ParamRegistry& reg)
{
    bool any = false;
    for (int s = 0; s < slotCount; ++s) any = slots[(size_t) s].prepareSelected (reg) || any;
    return any;
}

bool FxRack::isWaiting() const noexcept
{
    for (int s = 0; s < slotCount; ++s)
        if (slots[(size_t) s].isWaiting()) return true;
    return false;
}

void FxRack::pullParams (const ParamSnapshot& snap) noexcept
{
    activeSlots = 0;
    for (int s = 0; s < slotCount; ++s)
    {
        auto& sl = slots[(size_t) s];
        sl.pullParams (snap);
        mix[(size_t) s] = mixIndex[(size_t) s] >= 0 ? dsp::clamp01 (snap[mixIndex[(size_t) s]]) : 1.0f;
        bypass[(size_t) s] = bypassIndex[(size_t) s] >= 0 && snap.getBool (bypassIndex[(size_t) s]);
        if (sl.currentType() != InsertSlot::Off) ++activeSlots;
    }
}

void FxRack::process (float* l, float* r, int n) noexcept
{
    n = std::min (n, (int) dryL.size());
    for (int s = 0; s < slotCount; ++s)
    {
        auto& sl = slots[(size_t) s];
        const float target = bypass[(size_t) s] ? 0.0f : mix[(size_t) s];
        float& m = mixSmooth[(size_t) s];
        if (sl.currentType() == InsertSlot::Off) { m = target; continue; }
        if (target == 0.0f && m == 0.0f) { peaks[(size_t) s].store (0.0f, std::memory_order_relaxed); continue; } // bypassed
        if (target == 1.0f && m == 1.0f)
        {
            sl.process (l, r, n); // exact: the effect's own output
        }
        else
        {
            std::memcpy (dryL.data(), l, sizeof (float) * (size_t) n);
            std::memcpy (dryR.data(), r, sizeof (float) * (size_t) n);
            sl.process (l, r, n);
            for (int i = 0; i < n; ++i)
            {
                m += (target - m) * smoothCoeff;
                if (std::abs (target - m) < 1.0e-5f) m = target;
                l[i] = dryL[(size_t) i] + (l[i] - dryL[(size_t) i]) * m;
                r[i] = dryR[(size_t) i] + (r[i] - dryR[(size_t) i]) * m;
            }
        }
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i])));
        peaks[(size_t) s].store (pk, std::memory_order_relaxed);
    }
}

bool FxRack::moveSlot (const ParamRegistry& reg, const std::vector<std::string>& pre, int from, int to,
                       const std::function<void (int, float)>& setParam)
{
    const int n = (int) pre.size();
    if (from < 0 || to < 0 || from >= n || to >= n || from == to) return false;
    // every parameter of a slot: the IDs below its prefix, in registry order (same suffixes for every slot)
    auto collect = [&] (const std::string& p) {
        std::vector<std::pair<std::string, float>> v;
        for (int i = 0; i < reg.size(); ++i)
        {
            const auto& id = reg.descriptor (i).id;
            if (id.compare (0, p.size(), p) == 0) v.emplace_back (id.substr (p.size()), reg.get (i));
        }
        return v;
    };
    std::vector<std::vector<std::pair<std::string, float>>> values;
    for (const auto& p : pre) values.push_back (collect (p));
    auto order = std::vector<int> ((size_t) n);
    for (int i = 0; i < n; ++i) order[(size_t) i] = i;
    const int moved = order[(size_t) from];
    order.erase (order.begin() + from);
    order.insert (order.begin() + to, moved);
    for (int i = 0; i < n; ++i)
    {
        if (order[(size_t) i] == i) continue;
        for (const auto& [suffix, v] : values[(size_t) order[(size_t) i]])
            if (const int idx = reg.indexOf (pre[(size_t) i] + suffix); idx >= 0 && reg.get (idx) != v) setParam (idx, v);
    }
    return true;
}
} // namespace ohpe
