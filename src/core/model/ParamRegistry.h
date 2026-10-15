// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "ParamDescriptor.h"

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ohpe
{
/**
    The central parameter registry: one table of every automatable / MIDI-mappable value.

    - Built once (message thread) from descriptors, then frozen. The order of registration is
      the host parameter order and is append-only.
    - Values are stored as plain (unnormalised) floats in lock-free atomics, so any thread may
      read or write them. The audio thread never allocates or locks here.
    - Host automation (plug-in wrapper), MIDI mapping, presets and the UI all address values
      through this table by string ID (resolved once to an index).
*/
class ParamRegistry
{
public:
    static constexpr int invalidIndex = -1;

    ParamRegistry() = default;
    ParamRegistry (const ParamRegistry&) = delete;
    ParamRegistry& operator= (const ParamRegistry&) = delete;

    /** Adds a parameter (only before freeze()). Returns its index. Duplicate IDs throw. */
    int add (ParamDescriptor descriptor);
    void add (const std::vector<ParamDescriptor>& descriptors);

    /** Allocates the value storage and sets every value to its default. No add() afterwards. */
    void freeze();
    bool isFrozen() const noexcept { return frozen; }

    int size() const noexcept { return (int) descriptors.size(); }
    int indexOf (const std::string& id) const noexcept;
    const ParamDescriptor& descriptor (int index) const { return descriptors[(size_t) index]; }
    const std::vector<ParamDescriptor>& all() const noexcept { return descriptors; }

    // ---- values (real-time safe) ----
    float get (int index) const noexcept { return values[(size_t) index].load (std::memory_order_relaxed); }
    float getNormalised (int index) const noexcept { return descriptors[(size_t) index].toNormalised (get (index)); }
    void set (int index, float plain) noexcept;
    void setNormalised (int index, float norm) noexcept { set (index, descriptors[(size_t) index].fromNormalised (norm)); }

    float get (const std::string& id) const noexcept;
    bool set (const std::string& id, float plain) noexcept;

    void resetToDefaults() noexcept;

    /** Incremented on every set(); lets UIs cheaply notice changes. */
    uint32_t changeCounter() const noexcept { return counter.load (std::memory_order_relaxed); }

private:
    std::vector<ParamDescriptor> descriptors;
    std::unordered_map<std::string, int> byId;
    std::unique_ptr<std::atomic<float>[]> values;
    std::atomic<uint32_t> counter { 0 };
    bool frozen = false;
};
} // namespace ohpe
