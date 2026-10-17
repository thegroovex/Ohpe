// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PackFormat.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

using nlohmann::json;

namespace ohpe::update
{
namespace
{
std::string lower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
    return s;
}

std::string extensionOf (const std::string& path)
{
    const auto slash = path.find_last_of ('/');
    const auto dot = path.find_last_of ('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    return lower (path.substr (dot));
}

std::string normalised (std::string p)
{
    std::replace (p.begin(), p.end(), '\\', '/');
    return p;
}

std::string text (const json& j, const char* key)
{
    if (! j.is_object() || ! j.contains (key)) return {};
    const auto& v = j[key];
    return v.is_string() ? v.get<std::string>() : std::string();
}

void bilingual (const json& j, const char* key, std::string& en, std::string& de)
{
    if (! j.is_object() || ! j.contains (key)) return;
    const auto& v = j[key];
    if (v.is_string()) { en = v.get<std::string>(); return; }
    en = text (v, "en");
    de = text (v, "de");
}
} // namespace

PackKind kindFromName (const std::string& name) noexcept
{
    if (name == "presets") return PackKind::Presets;
    if (name == "wavetables") return PackKind::Wavetables;
    return PackKind::Unknown;
}

const char* kindName (PackKind k) noexcept
{
    switch (k)
    {
        case PackKind::Presets: return "presets";
        case PackKind::Wavetables: return "wavetables";
        case PackKind::Unknown: break;
    }
    return "";
}

const char* kindFolder (PackKind k) noexcept { return kindName (k); }

std::vector<std::string> kindExtensions (PackKind k)
{
    switch (k)
    {
        case PackKind::Presets: return { ".ohpepreset" };
        case PackKind::Wavetables: return { ".wav" };
        case PackKind::Unknown: break;
    }
    return {};
}

bool isPackJsonId (const std::string& id)
{
    if (id.empty() || id.size() > 64 || id[0] == '-') return false;
    return std::all_of (id.begin(), id.end(), [] (char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

bool parsePackInfo (const std::string& jsonText, PackInfo& out, std::string* error)
{
    auto fail = [error] (const char* e) {
        if (error != nullptr) *error = e;
        return false;
    };
    const json j = json::parse (jsonText, nullptr, false);
    if (j.is_discarded() || ! j.is_object()) return fail ("pack.json is not valid JSON");
    if (j.contains ("format") && j["format"].is_number_integer() && j["format"].get<int>() > packFormat)
        return fail ("pack.json has a newer format");
    PackInfo p;
    p.id = text (j, "id");
    p.version = text (j, "version");
    p.kind = j.contains ("kind") ? text (j, "kind") : std::string ("presets");
    bilingual (j, "name", p.nameEn, p.nameDe);
    bilingual (j, "description", p.descriptionEn, p.descriptionDe);
    p.minApp = text (j, "minApp");
    if (! isPackJsonId (p.id)) return fail ("pack.json: the id is missing or not valid (a-z, 0-9, -)");
    if (! Version::of (p.version).valid()) return fail ("pack.json: the version is missing or not valid");
    if (kindFromName (p.kind) == PackKind::Unknown) return fail ("pack.json: unknown kind");
    if (! p.minApp.empty() && ! Version::of (p.minApp).valid()) return fail ("pack.json: minApp is not a version");
    if (j.contains ("files"))
    {
        if (! j["files"].is_object()) return fail ("pack.json: files is not a list of checksums");
        for (const auto& [path, sum] : j["files"].items())
        {
            if (! isSafeEntryPath (path) || path == packInfoFile) return fail ("pack.json: files lists a path that is not valid");
            if (! sum.is_string()) return fail ("pack.json: a checksum is not valid");
            auto hex = sum.get<std::string>();
            std::transform (hex.begin(), hex.end(), hex.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
            if (hex.size() != 64 || hex.find_first_not_of ("0123456789abcdef") != std::string::npos) return fail ("pack.json: a checksum is not valid");
            p.files[path] = hex;
        }
    }
    if (j.contains ("license"))
    {
        const auto& l = j["license"];
        if (l.is_string()) p.license.type = l.get<std::string>();
        else if (l.is_object())
        {
            p.license.type = text (l, "type");
            p.license.productId = text (l, "productId");
        }
        else return fail ("pack.json: license is not valid");
        if (p.license.type.empty()) p.license.type = "free";
        if (p.license.type != "free" && p.license.type != "paid") return fail ("pack.json: license type must be free or paid");
        if (p.license.productId.size() > 128) return fail ("pack.json: license productId is too long");
    }
    if (j.contains ("signature"))
    {
        const auto& sg = j["signature"];
        if (sg.is_string()) { p.signature.algorithm = "ed25519"; p.signature.value = sg.get<std::string>(); }
        else if (sg.is_object())
        {
            p.signature.algorithm = text (sg, "alg");
            p.signature.keyId = text (sg, "keyId");
            p.signature.value = text (sg, "value");
        }
        else return fail ("pack.json: signature is not valid");
        if (p.signature.value.size() > 1024 || p.signature.keyId.size() > 128) return fail ("pack.json: signature is not valid");
    }
    if (p.nameEn.empty()) p.nameEn = p.id;
    if (p.nameDe.empty()) p.nameDe = p.nameEn;
    if (p.descriptionDe.empty()) p.descriptionDe = p.descriptionEn;
    out = std::move (p);
    return true;
}

std::string packInfoToJson (const PackInfo& p)
{
    json j = { { "format", packFormat },
               { "id", p.id },
               { "version", p.version },
               { "kind", p.kind },
               { "name", { { "en", p.nameEn }, { "de", p.nameDe } } },
               { "description", { { "en", p.descriptionEn }, { "de", p.descriptionDe } } },
               { "minApp", p.minApp } };
    if (! p.files.empty())
    {
        json f = json::object();
        for (const auto& [path, sum] : p.files) f[path] = sum;
        j["files"] = f;
    }
    if (p.license.paid() || ! p.license.productId.empty()) j["license"] = { { "type", p.license.type }, { "productId", p.license.productId } };
    if (p.signature.present()) j["signature"] = { { "alg", p.signature.algorithm }, { "keyId", p.signature.keyId }, { "value", p.signature.value } };
    return j.dump (2);
}

std::string canonicalPackJson (const std::string& text)
{
    json j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object()) return {};
    j.erase ("signature");
    return j.dump(); // nlohmann objects keep their keys sorted; dump() writes no white space
}

namespace
{
class OpenGate final : public PackGate
{
public:
    PackAccess access (const PackInfo&, const std::string&) const override { return PackAccess::Allowed; }
};
} // namespace

const PackGate& openGate()
{
    static const OpenGate gate;
    return gate;
}

bool parsePackFileName (const std::string& fileName, std::string& id, std::string& version)
{
    static const std::regex re ("^Ohpe-content-([a-z0-9_.\\-]+)-([0-9]+\\.[0-9]+\\.[0-9]+(?:-[0-9A-Za-z.\\-]+)?)\\.zip$", std::regex::icase);
    std::smatch m;
    if (! std::regex_match (fileName, m, re)) return false;
    if (! isSafePackId (m[1].str()) || ! Version::of (m[2].str()).valid()) return false;
    id = m[1].str();
    version = m[2].str();
    return true;
}

KindLimits limitsFor (PackKind k) noexcept
{
    constexpr uint64_t mb = 1024ull * 1024ull;
    switch (k)
    {
        case PackKind::Presets: return { 64 * mb, 2 * mb, 5000 };
        case PackKind::Wavetables: return { 512 * mb, 16 * mb, 2000 };
        case PackKind::Unknown: break;
    }
    return { 0, 0, 0 };
}

PackPlan planPack (const std::vector<ZipItem>& items, const std::string& packJson, const PackInfo& expected, uint64_t usedBytes,
                   bool requireChecksums)
{
    PackPlan plan;
    auto fail = [&plan] (const char* e, std::string detail = {}) {
        plan.ok = false;
        plan.error = e;
        plan.detail = std::move (detail);
        plan.files.clear();
        return plan;
    };
    if (items.empty()) return fail ("The content pack is damaged");
    for (const auto& it : items)
    {
        auto p = normalised (it.path);
        if (it.directory && ! p.empty() && p.back() == '/') p.pop_back();
        if (! isSafeEntryPath (p)) return fail ("The content pack is damaged", it.path);
    }

    // what the pack is
    if (! packJson.empty())
    {
        std::string err;
        if (! parsePackInfo (packJson, plan.info, &err)) return fail ("The pack description (pack.json) is not valid", err);
        plan.hasPackInfo = true;
        if (! expected.id.empty() && expected.id != plan.info.id) return fail ("The content pack does not match its description", plan.info.id);
        if (! expected.version.empty() && Version::of (expected.version) != Version::of (plan.info.version))
            return fail ("The content pack does not match its description", plan.info.version);
        // kind only when a description was given (manifest entry): a default PackInfo says "presets" - a local
        // .ohpepack of any kind ("Install pack ..." / drop) must not be held to that
        if (! expected.id.empty() && ! expected.kind.empty() && expected.kind != plan.info.kind)
            return fail ("The content pack does not match its description", plan.info.kind);
    }
    else if (requireChecksums)
        return fail ("The content pack has no description (pack.json)");
    else
    {
        plan.info = expected;
        if (plan.info.kind.empty()) plan.info.kind = "presets";
        if (plan.info.nameEn.empty()) plan.info.nameEn = plan.info.id;
        if (plan.info.nameDe.empty()) plan.info.nameDe = plan.info.nameEn;
        if (! isSafePackId (plan.info.id) || ! Version::of (plan.info.version).valid())
            return fail ("The content pack has no description (pack.json)");
    }
    plan.kind = kindFromName (plan.info.kind);
    if (plan.kind == PackKind::Unknown) return fail ("This kind of content pack needs a newer Ohpe", plan.info.kind);
    if (requireChecksums && plan.info.files.empty()) return fail ("The content pack has no checksums");
    const bool checksums = ! plan.info.files.empty();

    const std::string folder = std::string (kindFolder (plan.kind)) + "/";
    const auto exts = kindExtensions (plan.kind);
    const auto limits = limitsFor (plan.kind);
    std::set<std::string> targets;
    int contentFiles = 0;
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto& it = items[(size_t) i];
        if (it.directory) continue;
        const auto path = normalised (it.path);
        const auto ext = extensionOf (path);
        const bool atRoot = path.find ('/') == std::string::npos;
        const bool isContent = std::find (exts.begin(), exts.end(), ext) != exts.end();
        std::string target;
        if (path == packInfoFile)
        {
            if (it.size > maxNoteBytes) return fail ("The content pack is too big", path);
            target = path;
        }
        else if (isContent && path.rfind (folder, 0) == 0 && path.size() > folder.size())
            target = path;
        else if (isContent && atRoot)
        {
            target = folder + path;
            plan.flat = true;
        }
        else if (atRoot && (ext == ".txt" || ext == ".md") && it.size <= maxNoteBytes)
            target = path;
        else
        {
            ++plan.skipped;
            continue;
        }
        if (! targets.insert (lower (target)).second) return fail ("The content pack is damaged", path); // twice (flat + folder, or case)
        if (isContent)
        {
            if (it.size > limits.maxFileBytes) return fail ("A file of the content pack is too big", path);
            ++contentFiles;
        }
        std::string sum;
        if (checksums && path != packInfoFile)
        {
            const auto listed = plan.info.files.find (path);
            if (listed == plan.info.files.end()) return fail ("A file of the content pack is not in its checksum list", path);
            sum = listed->second;
        }
        plan.totalBytes += it.size;
        plan.files.push_back ({ i, target, it.size, sum });
    }
    if (checksums)
        for (const auto& [path, sum] : plan.info.files)
        {
            const bool there = std::any_of (items.begin(), items.end(), [&path = path] (const ZipItem& it) {
                return ! it.directory && normalised (it.path) == path;
            });
            if (! there) return fail ("The content pack is not complete", path);
        }
    if (contentFiles == 0) return fail ("The content pack holds no content", plan.info.kind);
    if (contentFiles > limits.maxFiles) return fail ("The content pack has too many files", std::to_string (contentFiles));
    if (plan.totalBytes > limits.maxPackBytes) return fail ("The content pack is too big", std::to_string (plan.totalBytes / (1024 * 1024)) + " MB");
    if (usedBytes + plan.totalBytes > contentQuotaBytes)
        return fail ("Not enough room for content packs - remove a pack first", std::to_string ((usedBytes + plan.totalBytes) / (1024 * 1024)) + " MB");
    plan.ok = true;
    return plan;
}

std::string contentUidPath (const std::string& packId, const std::string& pathInPack)
{
    auto p = normalised (pathInPack);
    for (auto k : { PackKind::Presets, PackKind::Wavetables })
    {
        const std::string f = std::string (kindFolder (k)) + "/";
        if (p.rfind (f, 0) == 0)
        {
            p = p.substr (f.size());
            break;
        }
    }
    return packId + "/" + p;
}

bool ContentRef::valid() const
{
    if (! isSafePackId (pack) || item.empty() || item.size() > 256 || item.back() == '/') return false;
    return isSafeEntryPath (item) && item.find ('\\') == std::string::npos;
}

ContentRef ContentRef::parse (const std::string& t)
{
    const auto slash = t.find ('/');
    if (slash == std::string::npos) return {};
    ContentRef r { t.substr (0, slash), t.substr (slash + 1) };
    return r.valid() ? r : ContentRef {};
}

const char* refStateName (RefState s) noexcept
{
    switch (s)
    {
        case RefState::Found: return "found";
        case RefState::PackMissing: return "pack-missing";
        case RefState::ItemMissing: return "item-missing";
        case RefState::Invalid: break;
    }
    return "invalid";
}

const ContentIndex::Pack* ContentIndex::find (const std::string& id) const
{
    for (const auto& p : packs)
        if (p.id == id) return &p;
    return nullptr;
}

RefState ContentIndex::resolve (const ContentRef& ref, PackKind kind) const
{
    if (! ref.valid()) return RefState::Invalid;
    const auto* p = find (ref.pack);
    if (p == nullptr) return RefState::PackMissing;
    for (const auto& [k, item] : p->items)
        if (k == kind && item == ref.item) return RefState::Found;
    return RefState::ItemMissing;
}

} // namespace ohpe::update
