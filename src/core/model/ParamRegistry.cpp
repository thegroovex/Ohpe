// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ParamRegistry.h"

#include <algorithm>
#include <stdexcept>

namespace ohpe
{
int ParamRegistry::add (ParamDescriptor d)
{
    if (frozen)
        throw std::logic_error ("ParamRegistry: add() after freeze()");
    if (d.id.empty() || byId.count (d.id) != 0)
        throw std::invalid_argument ("ParamRegistry: empty or duplicate parameter id '" + d.id + "'");
    if (d.kind == ParamKind::Choice && d.choicesDe.size() != d.choices.size())
        d.choicesDe = d.choices;
    d.defaultValue = d.clamp (d.defaultValue);
    const int index = (int) descriptors.size();
    byId.emplace (d.id, index);
    descriptors.push_back (std::move (d));
    return index;
}

void ParamRegistry::add (const std::vector<ParamDescriptor>& list)
{
    for (const auto& d : list)
        add (d);
}

void ParamRegistry::freeze()
{
    if (frozen)
        return;
    values.reset (new std::atomic<float>[descriptors.size()]);
    dirtyWords = (int) ((descriptors.size() + 63) / 64);
    dirty.reset (new std::atomic<uint64_t>[(size_t) std::max (1, dirtyWords)]);
    for (int w = 0; w < dirtyWords; ++w) dirty[(size_t) w].store (0);
    frozen = true;
    resetToDefaults();
}

int ParamRegistry::indexOf (const std::string& id) const noexcept
{
    const auto it = byId.find (id);
    return it == byId.end() ? invalidIndex : it->second;
}

void ParamRegistry::set (int index, float plain) noexcept
{
    if (index < 0 || index >= size())
        return;
    values[(size_t) index].store (descriptors[(size_t) index].clamp (plain), std::memory_order_relaxed);
    dirty[(size_t) index >> 6].fetch_or (uint64_t (1) << (index & 63), std::memory_order_release); // p35: after the value
    counter.fetch_add (1, std::memory_order_relaxed);
}

float ParamRegistry::get (const std::string& id) const noexcept
{
    const int i = indexOf (id);
    return i == invalidIndex ? 0.0f : get (i);
}

bool ParamRegistry::set (const std::string& id, float plain) noexcept
{
    const int i = indexOf (id);
    if (i == invalidIndex)
        return false;
    set (i, plain);
    return true;
}

void ParamRegistry::resetToDefaults() noexcept
{
    for (size_t i = 0; i < descriptors.size(); ++i)
        values[i].store (descriptors[i].defaultValue, std::memory_order_relaxed);
    for (int w = 0; w < dirtyWords; ++w) dirty[(size_t) w].store (~uint64_t (0), std::memory_order_release); // p35
    counter.fetch_add (1, std::memory_order_relaxed);
}
} // namespace ohpe
