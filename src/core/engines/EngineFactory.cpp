// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EngineFactory.h"

#include "AnalogEngine.h"
#include "FmEngine.h"
#include "WavetableEngine.h"
#include "PluckEngine.h"
#include "ClusterEngine.h"
#include "GrainEngine.h"
#include "SamplerEngine.h"
#include "VocalEngine.h"

namespace ohpe
{
const EngineTypeInfo& engineTypeInfo (EngineType type) noexcept
{
    static const EngineTypeInfo table[numEngineTypes] = {
        { "fm", "FM", "FM", true },
        { "wavetable", "Wavetable", "Wavetable", true },
        { "analog", "Analog", "Analog", true },
        { "pluck", "Pluck", "Zupfen", true },
        { "grain", "Grain", "Grain", true },
        { "vocal", "Vocal", "Vokal", true },
        { "cluster", "Cluster", "Cluster", true },
        { "sampler", "Sampler", "Sampler", true },
    };
    const int i = (int) type;
    return table[i >= 0 && i < numEngineTypes ? i : 0];
}

std::unique_ptr<ISynthEngine> createEngine (EngineType type)
{
    switch (type)
    {
        case EngineType::FM: return std::make_unique<FmEngine>();
        case EngineType::Analog: return std::make_unique<AnalogEngine>();
        case EngineType::Wavetable: return std::make_unique<WavetableEngine>();
        case EngineType::Pluck: return std::make_unique<PluckEngine>();
        case EngineType::Cluster: return std::make_unique<ClusterEngine>();
        case EngineType::Grain: return std::make_unique<GrainEngine>();
        case EngineType::Vocal: return std::make_unique<VocalEngine>();
        case EngineType::Sampler: return std::make_unique<SamplerEngine>();
        default: return nullptr;
    }
}
} // namespace ohpe
