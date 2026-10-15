// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "ParamRegistry.h"

#include <cmath>
#include <vector>

namespace ohpe
{
/**
    Per-block copy of all parameter values as seen by the DSP (audio thread only).

    Filled at the start of each block from the registry, then sequencer parameter locks are laid
    on top (they win until cleared). Modules read their values from here, never from the
    registry directly, so a block always sees one consistent set of values.
*/
class ParamSnapshot
{
public:
    void prepare (const ParamRegistry& reg)
    {
        values.assign ((size_t) reg.size(), 0.0f);
        locks.assign ((size_t) reg.size(), NAN);
    }

    void pull (const ParamRegistry& reg) noexcept
    {
        const int n = (int) values.size();
        for (int i = 0; i < n; ++i)
        {
            const float l = locks[(size_t) i];
            values[(size_t) i] = std::isnan (l) ? reg.get (i) : l;
        }
    }

    float operator[] (int index) const noexcept { return index >= 0 && index < (int) values.size() ? values[(size_t) index] : 0.0f; }
    bool getBool (int index) const noexcept { return (*this)[index] >= 0.5f; }
    int getInt (int index) const noexcept { return (int) std::lround ((*this)[index]); }

    void setLock (int index, float plain) noexcept
    {
        if (index >= 0 && index < (int) locks.size()) { locks[(size_t) index] = plain; values[(size_t) index] = plain; }
    }
    void clearLocks() noexcept
    {
        for (auto& l : locks) l = NAN;
    }
    bool isLocked (int index) const noexcept { return index >= 0 && index < (int) locks.size() && ! std::isnan (locks[(size_t) index]); }
    int size() const noexcept { return (int) values.size(); }

private:
    std::vector<float> values, locks;
};
} // namespace ohpe
