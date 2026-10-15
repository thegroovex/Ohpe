// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "ISynthEngine.h"

#include <memory>

namespace ohpe
{
/**
    Engine types in their permanent order. The order is the value of the "pN.engine" choice
    parameter, so it is append-only like parameter IDs: never reorder, only add at the end.
*/
enum class EngineType
{
    FM = 0,
    Wavetable,
    Analog,
    Pluck,
    Grain,
    Vocal,
    Cluster,
    Sampler,
    numTypes
};

constexpr int numEngineTypes = (int) EngineType::numTypes;

struct EngineTypeInfo
{
    const char* id;        ///< "fm", "wavetable", ... (parameter ID segment)
    const char* nameEn;
    const char* nameDe;
    bool implemented;      ///< false = slot reserved, plays silence (phase 2)
};

const EngineTypeInfo& engineTypeInfo (EngineType type) noexcept;

/** Creates an engine instance, or nullptr for a type that is not implemented yet. */
std::unique_ptr<ISynthEngine> createEngine (EngineType type);
} // namespace ohpe
