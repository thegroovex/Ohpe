// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UserTableState.h"

#include "../OhpeCore.h"
#include "../engines/VocalEngine.h"
#include "../engines/WavetableEngine.h"
#include "TableFile.h"

#include <nlohmann/json.hpp>

namespace ohpe::usertable
{
namespace
{
using nlohmann::json;

WavetableEngine* wavetableOf (Core& core, int part)
{
    if (part < 0 || part >= Core::numParts) return nullptr;
    return dynamic_cast<WavetableEngine*> (core.part (part).engine (EngineType::Wavetable));
}

VocalEngine* vocalOf (Core& core, int part)
{
    if (part < 0 || part >= Core::numParts) return nullptr;
    return dynamic_cast<VocalEngine*> (core.part (part).engine (EngineType::Vocal));
}

std::string fileNameOf (const std::string& path)
{
    const auto slash = path.find_last_of ("/\\");
    return slash == std::string::npos ? path : path.substr (slash + 1);
}

json refOf (const UserTable& t)
{
    json r = { { "name", t.name() }, { "path", t.path }, { "file", fileNameOf (t.path) }, { "frames", t.numFrames() },
               { "formants", (int) t.formants().size() } };
    if (t.numFrames() <= embedMaxFrames) r["data"] = base64Encode (encode (t));
    return r;
}

UserTablePtr resolveRef (const json& r, const Resolver& resolve, RestoreResult& result)
{
    if (! r.is_object()) return nullptr;
    const std::string path = r.value ("path", std::string());
    std::string file = r.value ("file", std::string());
    if (file.empty() && ! path.empty()) file = fileNameOf (path);
    if (resolve && (! path.empty() || ! file.empty()))
        if (auto t = resolve (path, file))
        {
            ++result.loaded;
            return t;
        }
    if (r.contains ("data") && r["data"].is_string())
        if (auto t = decode (base64Decode (r["data"].get<std::string>())))
        {
            auto copy = std::const_pointer_cast<UserTable> (t);
            copy->path = path; // the next save refers to the same file again
            ++result.embedded;
            return t;
        }
    ++result.missing;
    return nullptr;
}
} // namespace

UserTablePtr tableOf (Core& core, int part, Target target)
{
    if (target == Target::Wavetable)
    {
        auto* e = wavetableOf (core, part);
        return e != nullptr ? e->userTable() : nullptr;
    }
    auto* e = vocalOf (core, part);
    return e != nullptr ? e->userTable() : nullptr;
}

void setTable (Core& core, int part, Target target, UserTablePtr table)
{
    if (target == Target::Wavetable)
    {
        if (auto* e = wavetableOf (core, part)) e->setUserTable (std::move (table));
    }
    else if (auto* e = vocalOf (core, part))
        e->setUserTable (std::move (table));
}

std::string partToJson (Core& core, int part)
{
    const auto wt = tableOf (core, part, Target::Wavetable);
    const auto vo = tableOf (core, part, Target::Vocal);
    if (wt == nullptr && vo == nullptr) return {};
    json j = json::object();
    if (wt != nullptr) j["wavetable"] = refOf (*wt);
    if (vo != nullptr) j["vocal"] = vo == wt ? json { { "sameAs", "wavetable" } } : refOf (*vo);
    return j.dump();
}

RestoreResult partFromJson (Core& core, int part, const std::string& text, const Resolver& resolve)
{
    RestoreResult result;
    setTable (core, part, Target::Wavetable, nullptr);
    setTable (core, part, Target::Vocal, nullptr);
    if (text.empty()) return result;
    const json j = json::parse (text, nullptr, false);
    if (! j.is_object()) return result;
    UserTablePtr wt;
    if (j.contains ("wavetable"))
    {
        wt = resolveRef (j["wavetable"], resolve, result);
        setTable (core, part, Target::Wavetable, wt);
    }
    if (j.contains ("vocal"))
    {
        const auto& v = j["vocal"];
        if (v.is_object() && v.value ("sameAs", std::string()) == "wavetable") setTable (core, part, Target::Vocal, wt);
        else setTable (core, part, Target::Vocal, resolveRef (v, resolve, result));
    }
    return result;
}

std::string saveAssignments (Core& core)
{
    json j = { { "format", "ohpe-usertables" }, { "version", 1 } };
    json parts = json::object();
    for (int p = 0; p < Core::numParts; ++p)
    {
        const auto t = partToJson (core, p);
        if (! t.empty()) parts["p" + std::to_string (p + 1)] = json::parse (t);
    }
    j["parts"] = parts;
    return j.dump();
}

RestoreResult restoreAssignments (Core& core, const std::string& text, const Resolver& resolve)
{
    RestoreResult total;
    const json j = text.empty() ? json() : json::parse (text, nullptr, false);
    const bool valid = j.is_object() && j.value ("format", std::string()) == "ohpe-usertables" && j.contains ("parts") && j["parts"].is_object();
    for (int p = 0; p < Core::numParts; ++p)
    {
        const auto key = "p" + std::to_string (p + 1);
        const auto r = partFromJson (core, p, valid && j["parts"].contains (key) ? j["parts"][key].dump() : std::string(), resolve);
        total.loaded += r.loaded;
        total.embedded += r.embedded;
        total.missing += r.missing;
    }
    return total;
}
} // namespace ohpe::usertable
