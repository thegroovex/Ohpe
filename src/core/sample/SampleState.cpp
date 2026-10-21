// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleState.h"
#include "../model/FormatVersions.h"

#include "../OhpeCore.h"
#include "../engines/GrainEngine.h"
#include "../engines/SamplerEngine.h"

#include <nlohmann/json.hpp>

namespace ohpe::sample
{
namespace
{
SamplerEngine* samplerOf (Core& core, int part)
{
    return dynamic_cast<SamplerEngine*> (core.part (part).engine (EngineType::Sampler));
}

GrainEngine* grainOf (Core& core, int part)
{
    return dynamic_cast<GrainEngine*> (core.part (part).engine (EngineType::Grain));
}
} // namespace

std::string saveAssignments (Core& core)
{
    using nlohmann::json;
    json j;
    j["format"] = "ohpe-samples";
    j["version"] = formats::samples;
    json parts = json::object();
    for (int p = 0; p < Core::numParts; ++p)
    {
        json part = json::object();
        if (auto* s = samplerOf (core, p))
        {
            json zones = json::array();
            for (const auto& z : s->zones())
                if (z.sample != nullptr && ! z.sample->path().empty())
                    zones.push_back ({ { "path", z.sample->path() }, { "lo", z.keyLo }, { "hi", z.keyHi }, { "vlo", z.velLo },
                                       { "vhi", z.velHi }, { "root", z.rootKey }, { "ls", z.loopStart }, { "le", z.loopEnd } });
            if (! zones.empty()) part["sampler"] = { { "zones", zones } };
        }
        if (auto* g = grainOf (core, p))
            if (auto smp = g->sample(); smp != nullptr && ! smp->path().empty())
                part["grain"] = { { "path", smp->path() } };
        if (! part.empty()) parts["p" + std::to_string (p + 1)] = part;
    }
    j["parts"] = parts;
    json drum = json::array();
    for (int pad = 0; pad < drum::DrumKit::numPads; ++pad)
        if (auto smp = core.drumKit().padSample (pad); smp != nullptr && ! smp->path().empty())
            drum.push_back ({ { "pad", pad + 1 }, { "path", smp->path() } });
    j["drum"] = drum;
    return j.dump();
}

RestoreResult restoreAssignments (Core& core, const std::string& text, const Decoder& decode)
{
    using nlohmann::json;
    RestoreResult result;

    // start from a clean slate: built-in sounds everywhere
    for (int p = 0; p < Core::numParts; ++p)
    {
        if (auto* s = samplerOf (core, p)) s->clearZones();
        if (auto* g = grainOf (core, p)) g->setSample (nullptr);
    }
    for (int pad = 0; pad < drum::DrumKit::numPads; ++pad) core.drumKit().setPadSample (pad, nullptr);

    if (text.empty() || ! decode) return result;
    const json j = json::parse (text, nullptr, false);
    if (! j.is_object() || j.value ("format", std::string()) != "ohpe-samples") return result;

    auto load = [&] (const json& node) -> SamplePtr {
        if (! node.is_object() || ! node.contains ("path") || ! node["path"].is_string()) return nullptr;
        auto s = decode (node["path"].get<std::string>());
        if (s != nullptr) ++result.loaded;
        else ++result.missing;
        return s;
    };

    if (j.contains ("parts") && j["parts"].is_object())
        for (int p = 0; p < Core::numParts; ++p)
        {
            const auto key = "p" + std::to_string (p + 1);
            if (! j["parts"].contains (key)) continue;
            const auto& part = j["parts"][key];
            if (auto* s = samplerOf (core, p); s != nullptr && part.contains ("sampler") && part["sampler"].contains ("zones"))
            {
                std::vector<sampler::ZoneSpec> zones;
                for (const auto& z : part["sampler"]["zones"])
                {
                    sampler::ZoneSpec spec;
                    spec.sample = load (z);
                    if (spec.sample == nullptr) continue;
                    spec.keyLo = z.value ("lo", 0);
                    spec.keyHi = z.value ("hi", 127);
                    spec.velLo = z.value ("vlo", 1);
                    spec.velHi = z.value ("vhi", 127);
                    spec.rootKey = z.value ("root", -1);
                    spec.loopStart = z.value ("ls", (int64_t) -1);
                    spec.loopEnd = z.value ("le", (int64_t) -1);
                    zones.push_back (std::move (spec));
                }
                s->setZones (std::move (zones));
            }
            if (auto* g = grainOf (core, p); g != nullptr && part.contains ("grain"))
                g->setSample (load (part["grain"]));
        }

    if (j.contains ("drum") && j["drum"].is_array())
        for (const auto& d : j["drum"])
        {
            const int pad = d.value ("pad", 0) - 1;
            if (pad < 0 || pad >= drum::DrumKit::numPads) continue;
            core.drumKit().setPadSample (pad, load (d));
        }
    return result;
}
} // namespace ohpe::sample
