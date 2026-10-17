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
        motion.assign ((size_t) reg.size(), NAN);
    }

    void pull (const ParamRegistry& reg) noexcept
    {
        const int n = (int) values.size();
        for (int i = 0; i < n; ++i)
        {
            const float l = locks[(size_t) i];
            values[(size_t) i] = ! std::isnan (l) ? l : ! std::isnan (motion[(size_t) i]) ? motion[(size_t) i] : reg.get (i);
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
    /** 0.7: the modulation matrix writes the modulated value of a parameter for the rest of the block (the next
        pull() restores the parameter's own value; locks are kept). Audio thread. */
    void setModulated (int index, float plain) noexcept
    {
        if (index >= 0 && index < (int) values.size()) values[(size_t) index] = plain;
    }
    /** p16: the value a parameter has before modulation and morphing: its sequencer lock, else (p22) its motion lane,
        else the registry value. */
    float base (const ParamRegistry& reg, int index) const noexcept
    {
        if (index < 0 || index >= (int) values.size()) return 0.0f;
        const float l = locks[(size_t) index];
        if (! std::isnan (l)) return l;
        const float m = motion[(size_t) index];
        return std::isnan (m) ? reg.get (index) : m;
    }
    /** p22: the motion recorder lays a recorded value over a parameter (below locks, above the registry value) until
        clearMotion(); kept by every pull(). Takes effect at once unless the parameter is locked. Audio thread. */
    void setMotion (int index, float plain) noexcept
    {
        if (index < 0 || index >= (int) motion.size()) return;
        motion[(size_t) index] = plain;
        if (std::isnan (locks[(size_t) index])) values[(size_t) index] = plain;
    }
    /** p22: removes the overlay; `own` is the parameter's registry value (taken at once unless locked). */
    void clearMotion (int index, float own) noexcept
    {
        if (index < 0 || index >= (int) motion.size()) return;
        motion[(size_t) index] = NAN;
        if (std::isnan (locks[(size_t) index])) values[(size_t) index] = own;
    }
    bool hasMotion (int index) const noexcept { return index >= 0 && index < (int) motion.size() && ! std::isnan (motion[(size_t) index]); }
    bool isLocked (int index) const noexcept { return index >= 0 && index < (int) locks.size() && ! std::isnan (locks[(size_t) index]); }
    int size() const noexcept { return (int) values.size(); }

private:
    std::vector<float> values, locks, motion;
};
} // namespace ohpe
