// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Preset.h"

#include "../OhpeCore.h"

#include <cmath>
#include <nlohmann/json.hpp>

namespace ohpe::preset
{
using nlohmann::json;

namespace
{
std::string partPrefix (int partIndex) { return "p" + std::to_string (partIndex + 1) + "."; }

bool startsWith (const std::string& s, const char* prefix)
{
    const size_t n = std::char_traits<char>::length (prefix);
    return s.size() >= n && s.compare (0, n, prefix) == 0;
}
} // namespace

const std::vector<Category>& categories()
{
    // append-only: these keys are written into preset files
    static const std::vector<Category> list = {
        { "Bass", "Bass" },     { "Keys", "Tasten" }, { "Pads", "Flächen" }, { "Leads", "Leads" },        { "Plucks", "Zupfer" },
        { "Drums", "Drums" },   { "FX", "FX" },       { "Arp", "Arp" },       { "Textures", "Texturen" }, { "Vocal", "Vokal" },
        { "Boom", "Boom" }, // 0.6: deep tuned kicks and sub basses
    };
    return list;
}

bool isCategory (const std::string& tag) noexcept
{
    for (const auto& c : categories())
        if (tag == c.key) return true;
    return false;
}

bool isExcludedFromProject (const std::string& id) noexcept
{
    return startsWith (id, "lib.") || startsWith (id, "ui.") || startsWith (id, "tape.edit.") || startsWith (id, "sync.");
}

std::string engineIdOfPart (const ParamRegistry& reg, int partIndex)
{
    const int idx = reg.indexOf (partPrefix (partIndex) + "engine");
    if (idx < 0) return {};
    const int t = (int) std::lround (reg.get (idx));
    if (t < 0 || t >= numEngineTypes) return {};
    return engineTypeInfo ((EngineType) t).id;
}

Preset capturePart (const ParamRegistry& reg, int partIndex, Meta meta)
{
    Preset p;
    meta.kind = Kind::Part;
    meta.version = formatVersion;
    meta.engine = engineIdOfPart (reg, partIndex);
    p.meta = std::move (meta);
    const auto prefix = partPrefix (partIndex);
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& id = reg.descriptor (i).id;
        if (id.compare (0, prefix.size(), prefix) == 0)
            p.params[id.substr (prefix.size())] = reg.get (i);
    }
    return p;
}

Preset captureProject (Core& core, Meta meta, const std::map<std::string, std::string>& extraSections)
{
    Preset p;
    const auto& reg = core.params();
    meta.kind = Kind::Project;
    meta.version = formatVersion;
    meta.engine = engineIdOfPart (reg, 0);
    p.meta = std::move (meta);
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& id = reg.descriptor (i).id;
        if (! isExcludedFromProject (id)) p.params[id] = reg.get (i);
    }
    p.sections["sequencer"] = core.stepSequencer().toJson();
    p.sections["orbit"] = core.orbitSequencer().toJson();
    for (const auto& [name, text] : extraSections)
        if (! text.empty()) p.sections[name] = text;
    return p;
}

std::string toJson (const Preset& p)
{
    json j;
    j["format"] = "ohpe-preset";
    j["version"] = formatVersion;
    j["kind"] = p.meta.kind == Kind::Project ? "project" : "part";
    j["name"] = p.meta.name;
    j["author"] = p.meta.author;
    j["tags"] = p.meta.tags;
    j["engine"] = p.meta.engine;
    j["comment"] = p.meta.comment;
    json params = json::object();
    for (const auto& [id, v] : p.params) params[id] = v;
    j["params"] = params;
    if (! p.sections.empty())
    {
        json s = json::object();
        for (const auto& [name, text] : p.sections)
        {
            auto parsed = json::parse (text, nullptr, false);
            s[name] = parsed.is_discarded() ? json (text) : parsed;
        }
        j["sections"] = s;
    }
    return j.dump (2);
}

bool fromJson (const std::string& text, Preset& out, std::string* error)
{
    auto fail = [&] (const char* e) { if (error != nullptr) *error = e; return false; };
    const auto j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object()) return fail ("not JSON");
    if (j.value ("format", "") != "ohpe-preset") return fail ("not an Ohpe preset");
    Preset p;
    p.meta.version = j.value ("version", 1);
    const std::string kind = j.value ("kind", "part");
    p.meta.kind = kind == "project" ? Kind::Project : Kind::Part;
    p.meta.name = j.value ("name", "");
    p.meta.author = j.value ("author", "");
    p.meta.comment = j.value ("comment", "");
    p.meta.engine = j.value ("engine", "");
    if (j.contains ("tags") && j["tags"].is_array())
        for (const auto& t : j["tags"])
            if (t.is_string() && ! t.get<std::string>().empty()) p.meta.tags.push_back (t.get<std::string>());
    if (j.contains ("params") && j["params"].is_object())
        for (const auto& [id, v] : j["params"].items())
            if (v.is_number()) p.params[id] = v.get<float>();
    if (j.contains ("sections") && j["sections"].is_object())
        for (const auto& [name, v] : j["sections"].items())
            p.sections[name] = v.is_string() ? v.get<std::string>() : v.dump();
    if (p.meta.kind == Kind::Part && p.meta.engine.empty())
    {
        // version 1 has no engine field: derive it from the engine parameter
        const auto it = p.params.find ("engine");
        if (it != p.params.end())
        {
            const int t = (int) std::lround (it->second);
            if (t >= 0 && t < numEngineTypes) p.meta.engine = engineTypeInfo ((EngineType) t).id;
        }
    }
    out = std::move (p);
    return true;
}

ApplyResult applyPart (const Preset& p, ParamRegistry& reg, int partIndex)
{
    ApplyResult r;
    if (p.meta.kind != Kind::Part || partIndex < 0 || partIndex >= Core::numParts) return r;
    const auto prefix = partPrefix (partIndex);
    // parameters the preset does not list return to their defaults; the part's MIDI channel is
    // a setup value and stays
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        if (d.id.compare (0, prefix.size(), prefix) == 0 && d.id != prefix + "midich")
            reg.set (i, d.defaultValue);
    }
    for (const auto& [id, v] : p.params)
    {
        if (id == "midich") continue;
        if (reg.set (prefix + id, v)) ++r.applied;
        else ++r.unknown;
    }
    r.ok = true;
    return r;
}

ApplyResult applyProject (const Preset& p, Core& core)
{
    ApplyResult r;
    if (p.meta.kind != Kind::Project) return r;
    auto& reg = core.params();
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        if (! isExcludedFromProject (d.id)) reg.set (i, d.defaultValue);
    }
    for (const auto& [id, v] : p.params)
    {
        if (isExcludedFromProject (id)) continue;
        if (reg.set (id, v)) ++r.applied;
        else ++r.unknown;
    }
    if (auto it = p.sections.find ("sequencer"); it != p.sections.end()) core.stepSequencer().fromJson (it->second);
    else core.stepSequencer().clearPattern();
    if (auto it = p.sections.find ("orbit"); it != p.sections.end()) core.orbitSequencer().fromJson (it->second);
    else core.orbitSequencer().clearPattern();
    for (const auto& [name, text] : p.sections)
        if (name != "sequencer" && name != "orbit" && name != "midimap" && name != "ui" && name != "tape")
            core.setStateSection (name, text);
    r.ok = true;
    return r;
}

std::string safeFileName (const std::string& name)
{
    std::string s;
    for (unsigned char c : name)
    {
        if (c < 0x20 || std::string ("/\\:*?\"<>|").find ((char) c) != std::string::npos) s += '_';
        else s += (char) c;
    }
    while (! s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    while (! s.empty() && (s.front() == ' ' || s.front() == '.')) s.erase (s.begin());
    if (s.empty()) s = "Preset";
    if (s.size() > 80)
    {
        s.resize (80);
        while (! s.empty() && (((unsigned char) s.back()) & 0xc0) == 0x80) s.pop_back(); // no cut UTF-8 sequence
        if (! s.empty() && ((unsigned char) s.back()) >= 0xc0) s.pop_back();
    }
    return s;
}
} // namespace ohpe::preset
