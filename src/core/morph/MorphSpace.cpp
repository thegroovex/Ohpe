// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MorphSpace.h"
#include "model/JsonParse.h" // p63: depth-limited parse
#include "../model/FormatVersions.h"

#include "../engines/Part.h"
#include "../fx/InsertSlot.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>

namespace ohpe
{
using nlohmann::json;

namespace
{
enum ValueMode : uint8_t { ModeConst = 0, ModeInterp, ModePick };
enum Owner : int8_t { OwnAlways = 0, OwnEngine, OwnFx };
constexpr int numRackSlots = FxRack::numPartSlots;
constexpr float sleepSeconds = 0.1f;
constexpr int moveBlock = 64; ///< samples per morph update while the position glides or the lanes cross-fade (1.3 ms)

bool startsWith (const std::string& s, const char* prefix)
{
    const size_t n = std::char_traits<char>::length (prefix);
    return s.size() >= n && s.compare (0, n, prefix) == 0;
}

std::vector<std::string> split (const std::string& s)
{
    std::vector<std::string> out;
    size_t a = 0;
    while (true)
    {
        const size_t b = s.find ('.', a);
        out.push_back (s.substr (a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}
} // namespace

// ------------------------------------------------------------------------------------------------ private types
struct MorphSpace::PartInfo
{
    std::string prefix;
    std::vector<int> soundIdx, otherIdx;      ///< registry indices: corner parameters / the rest of "pN.*"
    std::vector<std::string> local;           ///< local ID of each sound parameter
    std::vector<uint8_t> discrete, continuous;
    std::vector<int8_t> ownerKind;
    std::vector<int16_t> ownerA, ownerB;      ///< engine type / (rack slot, effect type)
    std::vector<int8_t> modSlot;              ///< routing 0..15 a sound parameter belongs to, -1 = none
    int engineK = -1, layerK = -1;            ///< positions in the sound list
    std::array<int, numRackSlots> slotTypeK {};
    std::array<int, mod::numSlots> modDstIdx {};
    int onIdx = -1, xIdx = -1, yIdx = -1, glideIdx = -1, xsrcIdx = -1, ysrcIdx = -1;
};

struct MorphSpace::Plan
{
    bool any = false;                         ///< at least one corner is assigned
    int numLanes = 1;
    std::array<int, numCorners> lane {};
    std::array<bool, numCorners> live {};     ///< empty corner: the part's own sound
    std::array<std::vector<float>, numCorners> plain, norm;
    std::array<std::vector<uint8_t>, maxLanes> mode;
    std::array<std::vector<float>, maxLanes> constVal;
};

struct MorphSpace::Lane
{
    std::unique_ptr<Part> part;
    ParamSnapshot snap;
    int monitor = -1;
};

// ------------------------------------------------------------------------------------------------ static
void MorphSpace::registerParams (ParamRegistry& reg, int parts)
{
    using namespace pd;
    for (int p = 1; p <= parts; ++p)
    {
        const auto n = std::to_string (p);
        const auto pp = "p" + n + ".morph.";
        const std::string P = "Part " + n + " ";
        auto add = [&] (ParamDescriptor d) { d.group = "part" + n + "/morph"; reg.add (std::move (d)); };
        const std::vector<std::string> ctrlEn { "Off", "Pressure", "Slide", "Mod Wheel" };
        const std::vector<std::string> ctrlDe { "Aus", "Druck", "Slide", "Modulationsrad" };
        add (toggle (pp + "on", P + "Morph", P + "Morph", false));
        add (continuous (pp + "x", P + "Morph X", P + "Morph X", 0.0f, 1.0f, 0.5f));
        add (continuous (pp + "y", P + "Morph Y", P + "Morph Y", 0.0f, 1.0f, 0.5f));
        add (continuous (pp + "glide", P + "Morph Glide", P + "Morph-Gleiten", 0.0f, 2.0f, 0.05f, ParamUnit::Seconds, 0.4f));
        add (choice (pp + "xsrc", P + "Morph X Control", P + "Morph X Steuerung", ctrlEn, ctrlDe, CtrlOff));
        add (choice (pp + "ysrc", P + "Morph Y Control", P + "Morph Y Steuerung", ctrlEn, ctrlDe, CtrlOff));
    }
}

std::array<float, MorphSpace::numCorners> MorphSpace::weights (float x, float y) noexcept
{
    x = std::clamp (x, 0.0f, 1.0f);
    y = std::clamp (y, 0.0f, 1.0f);
    return { (1.0f - x) * y, x * y, (1.0f - x) * (1.0f - y), x * (1.0f - y) };
}

bool MorphSpace::isExcluded (const std::string& id) noexcept
{
    return startsWith (id, "morph.") || startsWith (id, "motion.") || id == "midich" || id == "quant" || id == "chord" || id == "mpe" || id == "mpe.bend"
        || id == "mpe.press" || id == "mpe.timbre" || id == "mpe.hd" || id == "mpe.onset" || id == "mpe.round"; // p50: discrete MPE setup
}

const char* MorphSpace::cornerName (int corner) noexcept
{
    static const char* n[numCorners] = { "A", "B", "C", "D" };
    return corner >= 0 && corner < numCorners ? n[corner] : "?";
}

MorphSpace::MorphSpace() = default;

MorphSpace::~MorphSpace()
{
    for (auto& m : mains)
        if (m != nullptr)
            for (int k = 0; k < Part::maxFollowers; ++k) m->setFollower (k, nullptr);
}

// ------------------------------------------------------------------------------------------------ bind / prepare
void MorphSpace::bind (const ParamRegistry& r, ParamSnapshot& mainSnapshot, const std::array<Part*, numParts>& parts)
{
    reg = &r;
    snap = &mainSnapshot;
    mains = parts;
    info.clear();
    for (int p = 0; p < numParts; ++p)
    {
        auto pi = std::make_unique<PartInfo>();
        pi->prefix = "p" + std::to_string (p + 1) + ".";
        pi->slotTypeK.fill (-1);
        pi->modDstIdx.fill (-1);
        std::vector<std::string> fxIds ((size_t) InsertSlot::numTypes);
        for (int t = InsertSlot::Off + 1; t < InsertSlot::numTypes; ++t)
            if (const auto inf = InsertSlot::typeInfo (t); inf.id != nullptr) fxIds[(size_t) t] = inf.id;
        std::vector<std::string> slotPrefix;
        for (int s = 0; s < numRackSlots; ++s) slotPrefix.push_back ((s == 0 ? std::string ("fx") : "fx" + std::to_string (s + 1)));
        for (int i = 0; i < r.size(); ++i)
        {
            const auto& d = r.descriptor (i);
            if (d.id.compare (0, pi->prefix.size(), pi->prefix) != 0) continue;
            const auto loc = d.id.substr (pi->prefix.size());
            if (isExcluded (loc)) { pi->otherIdx.push_back (i); continue; }
            const int k = (int) pi->soundIdx.size();
            pi->soundIdx.push_back (i);
            pi->local.push_back (loc);
            pi->discrete.push_back (d.isDiscrete() ? 1 : 0);
            pi->continuous.push_back (d.isDiscrete() ? 0 : 1);
            int8_t kind = OwnAlways;
            int16_t a = -1, b = -1;
            const auto seg = split (loc);
            for (int t = 0; t < numEngineTypes; ++t)
                if (seg.size() >= 2 && seg[0] == engineTypeInfo ((EngineType) t).id) { kind = OwnEngine; a = (int16_t) t; }
            if (kind == OwnAlways && seg.size() >= 3)
                for (int s = 0; s < numRackSlots; ++s)
                    if (seg[0] == slotPrefix[(size_t) s])
                        for (int t = InsertSlot::Off + 1; t < InsertSlot::numTypes; ++t)
                            if (! fxIds[(size_t) t].empty() && seg[1] == fxIds[(size_t) t]) { kind = OwnFx; a = (int16_t) s; b = (int16_t) t; }
            pi->ownerKind.push_back (kind);
            pi->ownerA.push_back (a);
            pi->ownerB.push_back (b);
            int8_t ms = -1;
            if (seg.size() == 3 && seg[0] == "mod" && seg[1].size() >= 2 && seg[1][0] == 's')
            {
                const int slot = std::atoi (seg[1].c_str() + 1) - 1;
                if (slot >= 0 && slot < mod::numSlots)
                {
                    ms = (int8_t) slot;
                    if (seg[2] == "dst") pi->modDstIdx[(size_t) slot] = i;
                }
            }
            pi->modSlot.push_back (ms);
            if (loc == "engine") pi->engineK = k;
            if (loc == "layer.engine") pi->layerK = k;
            for (int s = 0; s < numRackSlots; ++s)
                if (loc == slotPrefix[(size_t) s] + ".type") pi->slotTypeK[(size_t) s] = k;
        }
        pi->onIdx = r.indexOf (pi->prefix + "morph.on");
        pi->xIdx = r.indexOf (pi->prefix + "morph.x");
        pi->yIdx = r.indexOf (pi->prefix + "morph.y");
        pi->glideIdx = r.indexOf (pi->prefix + "morph.glide");
        pi->xsrcIdx = r.indexOf (pi->prefix + "morph.xsrc");
        pi->ysrcIdx = r.indexOf (pi->prefix + "morph.ysrc");
        info.push_back (std::move (pi));
    }
}

void MorphSpace::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = std::max (1, maxBlockSize);
    tmpL.assign ((size_t) maxBlock, 0.0f);
    tmpR.assign ((size_t) maxBlock, 0.0f);
    for (auto& store : laneStore)
        for (auto& l : store)
            if (l != nullptr) { l->part->prepare (sr, maxBlock); l->monitor = -1; }
    reset();
}

void MorphSpace::reset() noexcept
{
    for (int p = 0; p < numParts; ++p)
    {
        auto& a = audio[(size_t) p];
        for (int l = 1; l < maxLanes; ++l)
            if (a.awake[(size_t) l]) sleepLane (p, l);
        a.gain.fill (0.0f);
        a.awake.fill (false);
        a.silent.fill (0);
        a.init = false;
        a.wasOn = false;
        a.lanesNeedPull = true;
        a.appliedX = a.appliedY = -1.0f;
    }
}

// ------------------------------------------------------------------------------------------------ corners
std::vector<float> MorphSpace::homeValues (int part) const
{
    const auto& pi = *info[(size_t) part];
    std::vector<float> v (pi.soundIdx.size());
    for (size_t k = 0; k < v.size(); ++k) v[k] = reg->get (pi.soundIdx[k]);
    return v;
}

void MorphSpace::captureCorner (int part, int corner, const std::string& name)
{
    if (part < 0 || part >= numParts || corner < 0 || corner >= numCorners) return;
    auto& c = corners[(size_t) part][(size_t) corner];
    c.set = true;
    c.name = name;
    c.plain = homeValues (part);
    rebuild (part);
}

void MorphSpace::setCorner (int part, int corner, const std::string& name, const std::map<std::string, float>& params)
{
    if (part < 0 || part >= numParts || corner < 0 || corner >= numCorners) return;
    const auto& pi = *info[(size_t) part];
    auto& c = corners[(size_t) part][(size_t) corner];
    c.set = true;
    c.name = name;
    c.plain.assign (pi.soundIdx.size(), 0.0f);
    for (size_t k = 0; k < pi.soundIdx.size(); ++k)
    {
        const auto& d = reg->descriptor (pi.soundIdx[k]);
        const auto it = params.find (pi.local[k]);
        c.plain[k] = it != params.end() ? d.clamp (it->second) : d.defaultValue;
    }
    rebuild (part);
}

void MorphSpace::clearCorner (int part, int corner)
{
    if (part < 0 || part >= numParts || corner < 0 || corner >= numCorners) return;
    corners[(size_t) part][(size_t) corner] = {};
    rebuild (part);
}

void MorphSpace::clearPart (int part)
{
    if (part < 0 || part >= numParts) return;
    for (auto& c : corners[(size_t) part]) c = {};
    rebuild (part);
}

void MorphSpace::clearAll()
{
    for (int p = 0; p < numParts; ++p) clearPart (p);
}

bool MorphSpace::cornerSet (int part, int corner) const
{
    return part >= 0 && part < numParts && corner >= 0 && corner < numCorners && corners[(size_t) part][(size_t) corner].set;
}

bool MorphSpace::anyCorner (int part) const
{
    for (int c = 0; c < numCorners; ++c)
        if (cornerSet (part, c)) return true;
    return false;
}

std::string MorphSpace::cornerLabel (int part, int corner) const
{
    return cornerSet (part, corner) ? corners[(size_t) part][(size_t) corner].name : std::string();
}

std::map<std::string, float> MorphSpace::cornerParams (int part, int corner) const
{
    std::map<std::string, float> m;
    if (! cornerSet (part, corner)) return m;
    const auto& pi = *info[(size_t) part];
    const auto& c = corners[(size_t) part][(size_t) corner];
    for (size_t k = 0; k < pi.soundIdx.size(); ++k) m[pi.local[k]] = c.plain[k];
    return m;
}

std::string MorphSpace::partToJson (int part) const
{
    json j;
    json list = json::array();
    if (part >= 0 && part < numParts)
    {
        const auto& pi = *info[(size_t) part];
        for (int c = 0; c < numCorners; ++c)
        {
            const auto& cd = corners[(size_t) part][(size_t) c];
            if (! cd.set) { list.push_back (nullptr); continue; }
            json params = json::object();
            for (size_t k = 0; k < pi.soundIdx.size(); ++k)
                if (cd.plain[k] != reg->descriptor (pi.soundIdx[k]).defaultValue) params[pi.local[k]] = cd.plain[k];
            list.push_back ({ { "name", cd.name }, { "params", params } });
        }
    }
    j["corners"] = list;
    return j.dump();
}

bool MorphSpace::partFromJson (int part, const std::string& text)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    if (part < 0 || part >= numParts) return false;
    const auto j = jsonutil::parse (text);
    for (auto& c : corners[(size_t) part]) c = {};
    bool ok = false;
    if (! j.is_discarded() && j.is_object() && j.contains ("corners") && j["corners"].is_array())
    {
        ok = true;
        const auto& list = j["corners"];
        const auto& pi = *info[(size_t) part];
        for (int c = 0; c < numCorners && c < (int) list.size(); ++c)
        {
            const auto& e = list[(size_t) c];
            if (! e.is_object()) continue;
            auto& cd = corners[(size_t) part][(size_t) c];
            cd.set = true;
            cd.name = e.value ("name", std::string());
            std::map<std::string, float> params;
            if (e.contains ("params") && e["params"].is_object())
                for (const auto& [id, v] : e["params"].items())
                    if (v.is_number()) params[id] = v.get<float>();
            cd.plain.assign (pi.soundIdx.size(), 0.0f);
            for (size_t k = 0; k < pi.soundIdx.size(); ++k)
            {
                const auto& d = reg->descriptor (pi.soundIdx[k]);
                const auto it = params.find (pi.local[k]);
                cd.plain[k] = it != params.end() ? d.clamp (it->second) : d.defaultValue;
            }
        }
    }
    rebuild (part);
    return ok;
}
catch (const std::exception&)
{
    return false;
}

std::string MorphSpace::toJson() const
{
    json j;
    j["version"] = formats::morph;
    json parts = json::array();
    for (int p = 0; p < numParts; ++p) parts.push_back (json::parse (partToJson (p)));
    j["parts"] = parts;
    return j.dump();
}

bool MorphSpace::fromJson (const std::string& text)
try // p63: a damaged document (wrong types) never throws out of the loader
{
    const auto j = jsonutil::parse (text);
    if (j.is_discarded() || ! j.is_object() || ! j.contains ("parts") || ! j["parts"].is_array()) { clearAll(); return false; }
    const auto& parts = j["parts"];
    for (int p = 0; p < numParts; ++p)
    {
        if (p < (int) parts.size() && parts[(size_t) p].is_object()) partFromJson (p, parts[(size_t) p].dump());
        else clearPart (p);
    }
    return true;
}
catch (const std::exception&)
{
    clearAll();
    return false;
}

// ------------------------------------------------------------------------------------------------ plans
uint64_t MorphSpace::homeHash (int part) const
{
    const auto& pi = *info[(size_t) part];
    uint64_t h = 1469598103934665603ull;
    for (size_t k = 0; k < pi.soundIdx.size(); ++k)
    {
        if (! pi.discrete[k]) continue;
        const auto v = (uint64_t) (int64_t) std::lround (reg->get (pi.soundIdx[k]));
        h = (h ^ (v + k * 0x9e3779b97f4a7c15ull)) * 1099511628211ull;
    }
    return h;
}

void MorphSpace::rebuild (int part)
{
    if (reg == nullptr) return;
    const auto& pi = *info[(size_t) part];
    const size_t n = pi.soundIdx.size();
    auto plan = std::make_unique<Plan>();
    const auto home = homeValues (part);
    for (int c = 0; c < numCorners; ++c)
    {
        const auto& cd = corners[(size_t) part][(size_t) c];
        plan->any = plan->any || cd.set;
        plan->live[(size_t) c] = ! cd.set;
        plan->plain[(size_t) c] = cd.set ? cd.plain : home;
        auto& nv = plan->norm[(size_t) c];
        nv.resize (n);
        for (size_t k = 0; k < n; ++k) nv[k] = reg->descriptor (pi.soundIdx[k]).toNormalised (plan->plain[(size_t) c][k]);
    }

    // lanes: corners whose relevant discrete parameters agree
    auto relevant = [&] (const std::vector<float>& v, size_t k) {
        switch (pi.ownerKind[k])
        {
            case OwnEngine:
            {
                const int eng = pi.engineK >= 0 ? (int) std::lround (v[(size_t) pi.engineK]) : -1;
                const int lay = pi.layerK >= 0 ? (int) std::lround (v[(size_t) pi.layerK]) - 1 : -1;
                return pi.ownerA[k] == eng || pi.ownerA[k] == lay;
            }
            case OwnFx:
            {
                const int tk = pi.slotTypeK[(size_t) pi.ownerA[k]];
                return tk >= 0 && (int) std::lround (v[(size_t) tk]) == pi.ownerB[k];
            }
            default: return true;
        }
    };
    auto sameLane = [&] (int c1, int c2) {
        const auto& v1 = plan->plain[(size_t) c1];
        const auto& v2 = plan->plain[(size_t) c2];
        for (size_t k = 0; k < n; ++k)
            if (pi.discrete[k] && v1[k] != v2[k] && (relevant (v1, k) || relevant (v2, k))) return false;
        return true;
    };
    // lane 0 (the part itself) holds the part's own sound when an empty corner stands for it, else corner A
    std::array<int, numCorners> order { 0, 1, 2, 3 };
    for (int c = 0; c < numCorners; ++c)
        if (plan->live[(size_t) c]) { std::swap (order[0], order[(size_t) c]); break; }
    std::array<int, maxLanes> laneFirst {};
    plan->lane.fill (-1);
    int lanes = 0;
    for (int c : order)
    {
        int l = 0;
        while (l < lanes && ! sameLane (laneFirst[(size_t) l], c)) ++l;
        if (l == lanes) laneFirst[(size_t) lanes++] = c;
        plan->lane[(size_t) c] = l;
    }
    plan->numLanes = std::max (1, lanes);
    for (int l = 0; l < plan->numLanes; ++l)
    {
        auto& md = plan->mode[(size_t) l];
        auto& cv = plan->constVal[(size_t) l];
        md.assign (n, ModeConst);
        cv.assign (n, 0.0f);
        bool hasLive = false;
        int first = -1;
        for (int c = 0; c < numCorners; ++c)
            if (plan->lane[(size_t) c] == l)
            {
                hasLive = hasLive || plan->live[(size_t) c];
                if (first < 0) first = c;
            }
        for (size_t k = 0; k < n; ++k)
        {
            bool same = ! hasLive;
            for (int c = 0; c < numCorners && same; ++c)
                if (plan->lane[(size_t) c] == l && plan->plain[(size_t) c][k] != plan->plain[(size_t) first][k]) same = false;
            if (same) { md[k] = ModeConst; cv[k] = plan->plain[(size_t) first][k]; }
            else md[k] = pi.continuous[k] ? ModeInterp : ModePick;
        }
    }

    // lanes beyond the part itself: extra parts (created once, kept), effect types prepared for every corner
    auto& store = laneStore[(size_t) part];
    while ((int) store.size() < plan->numLanes - 1)
    {
        auto lane = std::make_unique<Lane>();
        lane->part = std::make_unique<Part> (part + 1);
        lane->part->bind (*reg);
        lane->snap.prepare (*reg);
        lane->snap.pull (*reg);
        lane->part->setSnapshot (&lane->snap);
        lane->part->prepare (sr, maxBlock);
        audio[(size_t) part].lanes[store.size()].store (lane.get(), std::memory_order_release);
        store.push_back (std::move (lane));
    }
    for (int c = 0; c < numCorners; ++c)
        for (int s = 0; s < numRackSlots; ++s)
            if (const int tk = pi.slotTypeK[(size_t) s]; tk >= 0)
            {
                const int t = (int) std::lround (plan->plain[(size_t) c][(size_t) tk]);
                if (mains[(size_t) part] != nullptr) mains[(size_t) part]->fxRack().slot (s).prepareType (t);
                for (auto& l : store) l->part->fxRack().slot (s).prepareType (t);
            }

    builtHash[(size_t) part] = homeHash (part);
    // publish; the old plan is freed by a later update() once the audio thread has let go of it
    auto* raw = plan.get();
    audio[(size_t) part].plan.store (raw, std::memory_order_seq_cst);
    if (published[(size_t) part] != nullptr) retired.push_back (std::move (published[(size_t) part]));
    published[(size_t) part] = std::move (plan);
}

std::vector<Part*> MorphSpace::laneParts (int part) const
{
    std::vector<Part*> out;
    if (part < 0 || part >= numParts) return out;
    for (const auto& l : laneStore[(size_t) part])
        if (l != nullptr && l->part != nullptr) out.push_back (l->part.get());
    return out;
}

bool MorphSpace::update()
{
    // free retired plans the audio thread no longer uses (it re-reads the published pointer every block)
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [this] (const std::unique_ptr<Plan>& p) {
                                       for (const auto& a : audio)
                                           if (a.inUse.load (std::memory_order_seq_cst) == p.get()) return false;
                                       return true;
                                   }),
                   retired.end());
    bool any = false;
    for (int p = 0; p < numParts; ++p)
    {
        if (! anyCorner (p)) continue;
        bool live = false;
        for (int c = 0; c < numCorners; ++c) live = live || ! cornerSet (p, c);
        if (live && homeHash (p) != builtHash[(size_t) p]) { rebuild (p); any = true; }
    }
    return any;
}

int MorphSpace::numLanes (int part) const
{
    if (part < 0 || part >= numParts) return 1;
    const auto& pl = published[(size_t) part];
    return pl != nullptr ? pl->numLanes : 1;
}

int MorphSpace::laneOfCorner (int part, int corner) const
{
    if (part < 0 || part >= numParts || corner < 0 || corner >= numCorners) return 0;
    const auto& pl = published[(size_t) part];
    return pl != nullptr ? pl->lane[(size_t) corner] : 0;
}

// ------------------------------------------------------------------------------------------------ audio thread
MorphSpace::Plan* MorphSpace::acquire (Audio& a) noexcept
{
    Plan* p = a.plan.load (std::memory_order_seq_cst);
    for (int tries = 0; tries < 4; ++tries)
    {
        a.inUse.store (p, std::memory_order_seq_cst);
        Plan* again = a.plan.load (std::memory_order_seq_cst);
        if (again == p) break;
        p = again;
    }
    return p;
}

bool MorphSpace::isOn (int part, const Plan* p) const noexcept
{
    return p != nullptr && p->any && info[(size_t) part]->onIdx >= 0 && snap->getBool (info[(size_t) part]->onIdx);
}

float MorphSpace::controlValue (int part, int control) const noexcept
{
    const auto& m = mains[(size_t) part]->modMatrix();
    switch (control)
    {
        case CtrlPressure: return std::max (m.sourceValue (mod::SrcChannelPressure), m.sourceValue (mod::SrcNotePressure));
        case CtrlSlide: return 0.5f * m.sourceValue (mod::SrcSlide);
        case CtrlModWheel: return m.sourceValue (mod::SrcModWheel);
        default: return 0.0f;
    }
}

void MorphSpace::applyValues (int part, const Plan& plan, const std::array<float, numCorners>& w) noexcept
{
    auto& a = audio[(size_t) part];
    const auto& pi = *info[(size_t) part];
    const auto& r = *reg;
    const auto& main = *snap;
    // routings of the part's own sound that move the pad itself stay as they are (they are not part of a corner)
    std::array<bool, mod::numSlots> xyRoute {};
    for (int s = 0; s < mod::numSlots; ++s)
        if (const int di = pi.modDstIdx[(size_t) s]; di >= 0)
        {
            const int d = (int) std::lround (main.base (r, di));
            xyRoute[(size_t) s] = d == mod::DstMorphX || d == mod::DstMorphY;
        }
    for (int l = 0; l < plan.numLanes; ++l)
    {
        ParamSnapshot* target = snap;
        if (l > 0)
        {
            Lane* ln = a.lanes[(size_t) l - 1].load (std::memory_order_acquire);
            if (ln == nullptr) continue;
            target = &ln->snap;
        }
        float W = 0.0f;
        for (int c = 0; c < numCorners; ++c)
            if (plan.lane[(size_t) c] == l) W += w[(size_t) c];
        auto& nw = a.nw[(size_t) l];
        if (W > 1.0e-9f)
            for (int c = 0; c < numCorners; ++c) nw[(size_t) c] = plan.lane[(size_t) c] == l ? w[(size_t) c] / W : 0.0f;
        else
        {
            float sum = 0.0f;
            for (int c = 0; c < numCorners; ++c) sum += plan.lane[(size_t) c] == l ? nw[(size_t) c] : 0.0f;
            if (! (sum > 0.5f)) // never set: the lane's corners equally
            {
                int cnt = 0;
                for (int c = 0; c < numCorners; ++c) cnt += plan.lane[(size_t) c] == l ? 1 : 0;
                for (int c = 0; c < numCorners; ++c) nw[(size_t) c] = plan.lane[(size_t) c] == l ? 1.0f / (float) std::max (1, cnt) : 0.0f;
            }
        }
        int dom = -1;
        for (int c = 0; c < numCorners; ++c)
            if (plan.lane[(size_t) c] == l && (dom < 0 || nw[(size_t) c] > nw[(size_t) dom])) dom = c;
        if (dom < 0) continue;
        const bool exact = nw[(size_t) dom] >= 1.0f - 1.0e-6f;
        const auto& md = plan.mode[(size_t) l];
        const auto& cv = plan.constVal[(size_t) l];
        const size_t n = pi.soundIdx.size();
        for (size_t k = 0; k < n; ++k)
        {
            const int idx = pi.soundIdx[k];
            float v;
            if (main.isLocked (idx) || (pi.modSlot[k] >= 0 && xyRoute[(size_t) pi.modSlot[k]])) v = main.base (r, idx);
            else if (md[k] == ModeConst) v = cv[k];
            else if (exact || md[k] == ModePick) v = plan.live[(size_t) dom] ? main.base (r, idx) : plan.plain[(size_t) dom][k];
            else
            {
                const auto& d = r.descriptor (idx);
                float acc = 0.0f;
                for (int c = 0; c < numCorners; ++c)
                    if (const float wc = nw[(size_t) c]; wc > 0.0f)
                        acc += wc * (plan.live[(size_t) c] ? d.toNormalised (main.base (r, idx)) : plan.norm[(size_t) c][k]);
                v = d.fromNormalised (acc);
            }
            target->setModulated (idx, v);
        }
        // the rest of the part's parameters at their own values (the matrix may have moved them in the last block)
        for (const int idx : pi.otherIdx) target->setModulated (idx, main.base (r, idx));
    }
}

void MorphSpace::afterPull() noexcept
{
    if (reg == nullptr) return;
    for (int p = 0; p < numParts; ++p)
    {
        auto& a = audio[(size_t) p];
        Plan* plan = acquire (a);
        if (plan != a.current) { a.current = plan; a.appliedX = a.appliedY = -1.0f; }
        if (! isOn (p, plan)) continue;
        if (! a.init)
        {
            const auto& pi = *info[(size_t) p];
            a.x = std::clamp (snap->base (*reg, pi.xIdx), 0.0f, 1.0f);
            a.y = std::clamp (snap->base (*reg, pi.yIdx), 0.0f, 1.0f);
        }
        applyValues (p, *plan, weights (a.x, a.y));
        a.appliedX = a.x;
        a.appliedY = a.y;
        a.lanesNeedPull = true;
    }
}

void MorphSpace::wakeLane (int part, int l) noexcept
{
    auto& a = audio[(size_t) part];
    Lane* ln = a.lanes[(size_t) l - 1].load (std::memory_order_acquire);
    if (ln == nullptr) return;
    Part& main = *mains[(size_t) part];
    Part& lp = *ln->part;
    lp.reset();
    lp.pullParams (ln->snap);
    if (ln->monitor != monitorMode) { ln->monitor = monitorMode; lp.setMonitorMode (monitorMode); }
    // the keys being played: sustain pedal, bend, held notes
    if (main.sustainPedal()) lp.handleMidi (MidiEvent::controller (1, 64, 127, 0));
    if (const float b = main.pitchBend(); b != 0.0f)
    {
        const int v = std::clamp ((int) std::lround (8192.0f + b * 8192.0f), 0, 16383);
        MidiEvent e;
        e.status = 0xe0;
        e.data1 = (uint8_t) (v & 0x7f);
        e.data2 = (uint8_t) ((v >> 7) & 0x7f);
        lp.handleMidi (e);
    }
    for (int note = 0; note < 128; ++note)
        if (const float vel = main.heldVelocity (note); vel > 0.0f) lp.noteOn (note, vel);
    main.setFollower (l - 1, &lp);
    a.awake[(size_t) l] = true;
    a.silent[(size_t) l] = 0;
}

void MorphSpace::sleepLane (int part, int l) noexcept
{
    auto& a = audio[(size_t) part];
    if (mains[(size_t) part] != nullptr) mains[(size_t) part]->setFollower (l - 1, nullptr);
    if (Lane* ln = a.lanes[(size_t) l - 1].load (std::memory_order_acquire)) ln->part->allNotesOff (true);
    a.awake[(size_t) l] = false;
    a.silent[(size_t) l] = 0;
    a.gain[(size_t) l] = 0.0f;
}

void MorphSpace::render (int part, float* l, float* r, int n, double bpm, double ppq, bool playing) noexcept
{
    auto& a = audio[(size_t) part];
    Part& main = *mains[(size_t) part];
    const auto& pi = *info[(size_t) part];
    Plan* plan = a.current;
    const bool on = isOn (part, plan);
    bool anyLane = false;
    for (int k = 1; k < maxLanes; ++k) anyLane = anyLane || a.awake[(size_t) k];
    if (! on && ! anyLane)
    {
        if (a.wasOn) { a.wasOn = false; a.init = false; }
        a.gain.fill (0.0f);
        a.gain[0] = 1.0f; // the part alone, at full level
        a.uiOn.store (false, std::memory_order_relaxed);
        a.uiAwake.store (1, std::memory_order_relaxed);
        main.render (l, r, n);
        return;
    }
    const int lanes = on ? plan->numLanes : 1;
    if (on && ! a.wasOn)
    {
        a.wasOn = true;
        a.init = false;
    }
    const float srf = (float) sr;
    int pos = 0;
    while (pos < n)
    {
        // 1. position: pad + matrix + control, glided
        float tx = a.x, ty = a.y;
        if (on)
        {
            tx = snap->base (*reg, pi.xIdx) + main.modulationOf (pi.xIdx);
            ty = snap->base (*reg, pi.yIdx) + main.modulationOf (pi.yIdx);
            if (pi.xsrcIdx >= 0) tx += controlValue (part, snap->getInt (pi.xsrcIdx));
            if (pi.ysrcIdx >= 0) ty += controlValue (part, snap->getInt (pi.ysrcIdx));
            tx = std::clamp (tx, 0.0f, 1.0f);
            ty = std::clamp (ty, 0.0f, 1.0f);
            if (! a.init) { a.x = tx; a.y = ty; a.init = true; }
        }
        std::array<float, maxLanes> gT {};
        bool moving = on && (tx != a.x || ty != a.y);
        int len = n - pos;
        if (moving) len = std::min (len, moveBlock);
        if (moving)
        {
            const float tau = pi.glideIdx >= 0 ? std::max (0.0f, (*snap)[pi.glideIdx]) : 0.05f;
            const float k = tau <= 1.0e-4f ? 1.0f : 1.0f - std::exp (-(float) len / (tau * srf));
            a.x += (tx - a.x) * k;
            a.y += (ty - a.y) * k;
            if (std::abs (tx - a.x) < 1.0e-5f) a.x = tx;
            if (std::abs (ty - a.y) < 1.0e-5f) a.y = ty;
        }
        const auto w = weights (a.x, a.y);
        if (on)
        {
            for (int c = 0; c < numCorners; ++c) gT[(size_t) plan->lane[(size_t) c]] += w[(size_t) c];
            if (lanes == 1) gT[0] = 1.0f;
            else
                for (int k = 0; k < lanes; ++k) gT[(size_t) k] = std::sqrt (std::max (0.0f, gT[(size_t) k]));
        }
        else gT[0] = 1.0f; // morph switched off: the part alone, the lanes fade out

        // 2. lanes wake / sleep
        for (int k = 1; k < maxLanes; ++k)
        {
            if (gT[(size_t) k] > 0.0f && ! a.awake[(size_t) k] && k < lanes) { wakeLane (part, k); a.gain[(size_t) k] = 0.0f; }
            if (a.awake[(size_t) k] && gT[(size_t) k] <= 0.0f && a.gain[(size_t) k] <= 0.0f)
            {
                a.silent[(size_t) k] += len;
                if ((float) a.silent[(size_t) k] > sleepSeconds * srf || k >= lanes) sleepLane (part, k);
            }
            else a.silent[(size_t) k] = 0;
        }
        bool ramping = false;
        for (int k = 0; k < maxLanes; ++k) ramping = ramping || gT[(size_t) k] != a.gain[(size_t) k];
        if (ramping && ! moving) len = std::min (len, moveBlock);
        if (lanes == 1 && ! anyLane && a.gain[0] != 1.0f) { a.gain[0] = 1.0f; gT[0] = 1.0f; } // one lane: no fade

        // 3. values for the new position
        if (on && (a.x != a.appliedX || a.y != a.appliedY))
        {
            applyValues (part, *plan, w);
            a.appliedX = a.x;
            a.appliedY = a.y;
            main.pullParams (*snap);
            a.lanesNeedPull = true;
        }
        if (a.lanesNeedPull)
        {
            for (int k = 1; k < maxLanes; ++k)
                if (a.awake[(size_t) k])
                    if (Lane* ln = a.lanes[(size_t) k - 1].load (std::memory_order_acquire)) ln->part->pullParams (ln->snap);
            a.lanesNeedPull = false;
        }
        main.modMatrix().setSourceValue (mod::SrcMorphX, a.x);
        main.modMatrix().setSourceValue (mod::SrcMorphY, a.y);

        // 4. render: the part (lane 0) and the awake lanes, equal-power gains ramped over the block
        main.render (l + pos, r + pos, len);
        const bool mix = lanes > 1 || anyLane || a.gain[0] != 1.0f || gT[0] != 1.0f;
        if (mix)
        {
            const float g0 = a.gain[0], step = (gT[0] - g0) / (float) len;
            for (int i = 0; i < len; ++i)
            {
                const float g = g0 + step * (float) (i + 1);
                l[pos + i] *= g;
                r[pos + i] *= g;
            }
        }
        a.gain[0] = gT[0];
        int awakeCount = 1;
        for (int k = 1; k < maxLanes; ++k)
        {
            if (! a.awake[(size_t) k]) continue;
            Lane* ln = a.lanes[(size_t) k - 1].load (std::memory_order_acquire);
            if (ln == nullptr || len > (int) tmpL.size()) continue;
            ++awakeCount;
            Part& lp = *ln->part;
            if (ln->monitor != monitorMode) { ln->monitor = monitorMode; lp.setMonitorMode (monitorMode); }
            lp.setTransport (bpm, ppq + (double) pos * bpm / 60.0 / sr, playing);
            lp.modMatrix().setSourceValue (mod::SrcMorphX, a.x);
            lp.modMatrix().setSourceValue (mod::SrcMorphY, a.y);
            lp.render (tmpL.data(), tmpR.data(), len);
            const float g0 = a.gain[(size_t) k], step = (gT[(size_t) k] - g0) / (float) len;
            for (int i = 0; i < len; ++i)
            {
                const float g = g0 + step * (float) (i + 1);
                l[pos + i] += tmpL[(size_t) i] * g;
                r[pos + i] += tmpR[(size_t) i] * g;
            }
            a.gain[(size_t) k] = gT[(size_t) k];
        }
        anyLane = awakeCount > 1;
        pos += len;
    }
    a.uiX.store (a.x, std::memory_order_relaxed);
    a.uiY.store (a.y, std::memory_order_relaxed);
    a.uiOn.store (on, std::memory_order_relaxed);
    int awake = 1;
    for (int k = 1; k < maxLanes; ++k) awake += a.awake[(size_t) k] ? 1 : 0;
    a.uiAwake.store (awake, std::memory_order_relaxed);
}

float MorphSpace::laneValue (int part, int corner, int registryIndex) const noexcept
{
    if (part < 0 || part >= numParts || corner < 0 || corner >= numCorners) return 0.0f;
    const auto& a = audio[(size_t) part];
    const Plan* p = a.current;
    const int l = p != nullptr ? p->lane[(size_t) corner] : 0;
    if (l <= 0) return (*snap)[registryIndex];
    const Lane* ln = a.lanes[(size_t) l - 1].load (std::memory_order_acquire);
    return ln != nullptr ? ln->snap[registryIndex] : 0.0f;
}
} // namespace ohpe
