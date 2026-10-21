// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "StateSerializer.h"
#include "model/JsonParse.h" // p63: depth-limited parse

#include <nlohmann/json.hpp>

namespace ohpe::state
{
using nlohmann::json;

std::string saveState (const ParamRegistry& reg, const Sections& sections)
{
    json j;
    j["format"] = "ohpe-state";
    j["version"] = currentVersion;
    j["app"] = "Ohpe";
    json p = json::object();
    for (int i = 0; i < reg.size(); ++i)
        p[reg.descriptor (i).id] = reg.get (i);
    j["params"] = p;
    json s = json::object();
    for (const auto& [name, text] : sections)
    {
        auto parsed = jsonutil::parse (text);
        s[name] = parsed.is_discarded() ? json (text) : parsed;
    }
    j["sections"] = s;
    return j.dump();
}

LoadResult loadState (const std::string& text, ParamRegistry& reg, bool resetFirst)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    LoadResult r;
    const auto j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-state")
        return r;
    if (resetFirst) reg.resetToDefaults();
    if (j.contains ("params") && j["params"].is_object())
        for (const auto& [id, v] : j["params"].items())
        {
            if (! v.is_number()) continue;
            if (reg.set (id, v.get<float>())) ++r.applied;
            else ++r.unknown;
        }
    if (j.contains ("sections") && j["sections"].is_object())
        for (const auto& [name, v] : j["sections"].items())
            r.sections[name] = v.is_string() ? v.get<std::string>() : v.dump();
    r.ok = true;
    return r;
}
catch (const std::exception&)
{
    return {};
}

std::string savePreset (const ParamRegistry& reg, const std::string& prefix, const std::string& name)
{
    json j;
    j["format"] = "ohpe-preset";
    j["version"] = currentVersion;
    j["name"] = name;
    j["prefix"] = prefix;
    json p = json::object();
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& id = reg.descriptor (i).id;
        if (id.compare (0, prefix.size(), prefix) == 0)
            p[id.substr (prefix.size())] = reg.get (i);
    }
    j["params"] = p;
    return j.dump (2);
}

LoadResult loadPreset (const std::string& text, ParamRegistry& reg, const std::string& targetPrefix)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    LoadResult r;
    const auto j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-preset")
        return r;
    // parameters of the target prefix that the preset does not contain go back to their defaults
    for (int i = 0; i < reg.size(); ++i)
        if (reg.descriptor (i).id.compare (0, targetPrefix.size(), targetPrefix) == 0)
            reg.set (i, reg.descriptor (i).defaultValue);
    if (j.contains ("params") && j["params"].is_object())
        for (const auto& [id, v] : j["params"].items())
        {
            if (! v.is_number()) continue;
            if (reg.set (targetPrefix + id, v.get<float>())) ++r.applied;
            else ++r.unknown;
        }
    r.ok = true;
    return r;
}
catch (const std::exception&)
{
    return {};
}
} // namespace ohpe::state
