// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SoundGenome.h"

#include "../OhpeCore.h"
#include "../engines/EngineFactory.h"
#include "../fx/InsertSlot.h"
#include "../preset/Preset.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <nlohmann/json.hpp>

namespace ohpe::breed
{
using nlohmann::json;

namespace
{
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

bool contains (const std::string& s, const char* part) { return s.find (part) != std::string::npos; }

bool endsWith (const std::string& s, const char* suffix)
{
    const size_t n = std::char_traits<char>::length (suffix);
    return s.size() >= n && s.compare (s.size() - n, n, suffix) == 0;
}

bool isFilterLeaf (const std::string& leaf)
{
    for (const char* f : { "cutoff", "reso", "ftype", "keytrack", "tone", "bright", "damping", "drive" })
        if (leaf == f) return true;
    return false;
}

bool isRiskyLeaf (const std::string& leaf)
{
    for (const char* f : { "reso", "feedback", "drive", "output", "makeup", "noise", "crackle", "xmod", "index", "fm", "envfm" })
        if (leaf == f) return true;
    return contains (leaf, "gain") || contains (leaf, "level");
}

constexpr float twoPi = 6.283185307179586f;
} // namespace

const char* groupName (int group) noexcept
{
    static const char* n[numGroups] = { "Engine", "Filter", "Envelope", "FX", "Mod", "Level", "Fixed" };
    return group >= 0 && group < numGroups ? n[group] : "?";
}

// ------------------------------------------------------------------------------------------------ Rng
uint64_t Rng::next() noexcept
{
    uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

float Rng::uniform() noexcept { return (float) (next() >> 40) * (1.0f / 16777216.0f); }

float Rng::gauss() noexcept
{
    if (haveSpare) { haveSpare = false; return spare; }
    float u = 0.0f;
    while (u <= 1.0e-7f) u = uniform();
    const float v = uniform();
    const float r = std::sqrt (-2.0f * std::log (u));
    spare = r * std::sin (twoPi * v);
    haveSpare = true;
    return r * std::cos (twoPi * v);
}

int Rng::below (int n) noexcept { return n <= 1 ? 0 : (int) (next() % (uint64_t) n); }

uint64_t Rng::mix (uint64_t a, uint64_t b) noexcept
{
    Rng r (a ^ (b * 0xD6E8FEB86659FD93ull) ^ 0x5851F42D4C957F2Dull);
    r.next();
    return r.next();
}

// ------------------------------------------------------------------------------------------------ Gene
float Gene::clamp (float v) const noexcept
{
    if (std::isnan (v)) v = defaultValue;
    if (v < minValue) v = minValue;
    if (v > maxValue) v = maxValue;
    if (kind != ParamKind::Continuous) v = std::round (v);
    return v;
}

float Gene::toNorm (float plain) const noexcept
{
    const float range = maxValue - minValue;
    if (range <= 0.0f) return 0.0f;
    float p = (clamp (plain) - minValue) / range;
    if (skew != 1.0f && p > 0.0f) p = std::pow (p, skew);
    return p;
}

float Gene::fromNorm (float norm) const noexcept
{
    if (! (norm >= 0.0f)) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    if (skew != 1.0f && norm > 0.0f) norm = std::exp (std::log (norm) / skew);
    return clamp (minValue + (maxValue - minValue) * norm);
}

// ------------------------------------------------------------------------------------------------ Schema
Schema Schema::forPart (const ParamRegistry& reg, int partIndex)
{
    Schema s;
    const std::string prefix = "p" + std::to_string (partIndex + 1) + ".";
    constexpr int slots = FxRack::numPartSlots;
    s.slotTypeK.assign ((size_t) slots, -1);
    std::vector<std::string> slotPrefix;
    for (int k = 0; k < slots; ++k) slotPrefix.push_back (k == 0 ? std::string ("fx") : "fx" + std::to_string (k + 1));
    std::vector<std::string> fxIds ((size_t) InsertSlot::numTypes);
    for (int t = InsertSlot::Off + 1; t < InsertSlot::numTypes; ++t)
        if (const auto inf = InsertSlot::typeInfo (t); inf.id != nullptr) fxIds[(size_t) t] = inf.id;

    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        if (d.id.compare (0, prefix.size(), prefix) != 0) continue;
        Gene g;
        g.id = d.id.substr (prefix.size());
        g.kind = d.kind;
        g.minValue = d.minValue;
        g.maxValue = d.maxValue;
        g.defaultValue = d.defaultValue;
        g.skew = d.skew;
        const auto seg = split (g.id);
        const auto& head = seg[0];
        const auto& leaf = seg.back();
        const int k = (int) s.genes.size();

        int engineOwner = -1;
        for (int t = 0; t < numEngineTypes; ++t)
            if (seg.size() >= 2 && head == engineTypeInfo ((EngineType) t).id) engineOwner = t;
        int slot = -1;
        for (int q = 0; q < slots; ++q)
            if (head == slotPrefix[(size_t) q]) slot = q;

        if (g.id == "tune" || g.id == "fine" || g.id == "voices" || g.id == "bend" || g.id == "midich" || g.id == "quant"
            || g.id == "chord" || g.id == "mpe" || g.id == "mpe.bend" || g.id == "mpe.press" || g.id == "mpe.timbre"
            || g.id == "seqlane" || head == "morph" || head == "motion" || head == "usertable") // usertable: follows its table, never bred
        {
            g.group = Fixed;
            g.mutable_ = false;
        }
        else if (g.id == "vol" || g.id == "gain" || g.id == "layer.level")
        {
            g.group = Level;
            g.mutable_ = false;
        }
        else if (engineOwner >= 0)
        {
            g.owner = Gene::OwnEngine;
            g.ownerA = engineOwner;
            g.group = isFilterLeaf (leaf) ? Filter : Engine;
            if (contains (leaf, "inst") || leaf == "root") g.mutable_ = false; // the sample choice stays
        }
        else if (g.id == "engine" || head == "layer" || g.id == "monobass")
        {
            g.group = Engine;
            if (g.id == "engine" || g.id == "layer.engine") g.mutable_ = false;
        }
        else if (head == "amp" || head == "env3" || head == "pitchenv" || head == "glide" || g.id == "polyglide" || g.id == "velsens"
                 || g.id == "mod.a" || g.id == "mod.d" || g.id == "mod.r" || g.id == "mod.amt")
        {
            g.group = Envelope;
        }
        else if (slot >= 0)
        {
            g.group = Fx;
            if (seg.size() == 2 && leaf == "type") s.slotTypeK[(size_t) slot] = k; // own crossover / mutation rule
            if (seg.size() == 2 && leaf == "bypass") g.mutable_ = false;
            if (seg.size() >= 3)
                for (int t = InsertSlot::Off + 1; t < InsertSlot::numTypes; ++t)
                    if (! fxIds[(size_t) t].empty() && seg[1] == fxIds[(size_t) t])
                    {
                        g.owner = Gene::OwnFx;
                        g.ownerA = slot;
                        g.ownerB = t;
                    }
        }
        else if (head == "space")
        {
            g.group = Fx;
            if (g.id == "space.on") g.mutable_ = false;
        }
        else if (head == "reso") // p19: the part's resonator body stage - bred like an effect, switched on by a parent only
        {
            g.group = Fx;
            if (g.id == "reso.on") g.mutable_ = false;
        }
        else if (seg.size() == 3 && head == "mod" && seg[1].size() >= 2 && seg[1][0] == 's')
        {
            g.group = Mod;
            const int r = std::atoi (seg[1].c_str() + 1) - 1;
            if (r >= 0)
            {
                g.owner = Gene::OwnRoute;
                g.ownerA = r;
                if ((int) s.routeGenes.size() <= r) s.routeGenes.resize ((size_t) r + 1);
                s.routeGenes[(size_t) r].push_back (k);
                if (leaf != "amt") g.mutable_ = false; // the wiring stays; amounts mutate
            }
        }
        else
        {
            g.group = Mod; // LFOs, macros, MPE amounts, life, anything new
        }
        g.risky = g.kind == ParamKind::Continuous && isRiskyLeaf (leaf);
        s.slotFlag.push_back (0);
        if (g.id == "engine") s.engineK = k;
        if (g.id == "layer.engine") s.layerK = k;
        if (g.id == "gain") s.gainK = k;
        s.byId[g.id] = k;
        s.genes.push_back (std::move (g));
    }
    for (int k : s.slotTypeK)
        if (k >= 0) s.slotFlag[(size_t) k] = 1;
    return s;
}

int Schema::indexOf (const std::string& localId) const noexcept
{
    const auto it = byId.find (localId);
    return it != byId.end() ? it->second : -1;
}

// ------------------------------------------------------------------------------------------------ genomes
Genome fromParams (const Schema& s, const std::map<std::string, float>& localParams)
{
    Genome g ((size_t) s.size());
    for (int i = 0; i < s.size(); ++i)
    {
        const auto& gene = s.gene (i);
        const auto it = localParams.find (gene.id);
        g[(size_t) i] = gene.clamp (it != localParams.end() ? it->second : gene.defaultValue);
    }
    return g;
}

Genome fromRegistry (const Schema& s, const ParamRegistry& reg, int partIndex)
{
    Genome g ((size_t) s.size());
    const std::string prefix = "p" + std::to_string (partIndex + 1) + ".";
    for (int i = 0; i < s.size(); ++i)
    {
        const auto& gene = s.gene (i);
        const int idx = reg.indexOf (prefix + gene.id);
        g[(size_t) i] = gene.clamp (idx >= 0 ? reg.get (idx) : gene.defaultValue);
    }
    return g;
}

std::map<std::string, float> toParams (const Schema& s, const Genome& g, bool sparse)
{
    std::map<std::string, float> out;
    for (int i = 0; i < s.size() && i < (int) g.size(); ++i)
    {
        const auto& gene = s.gene (i);
        const float v = gene.clamp (g[(size_t) i]);
        if (! sparse || v != gene.defaultValue) out[gene.id] = v;
    }
    return out;
}

bool valid (const Schema& s, const Genome& g)
{
    if ((int) g.size() != s.size()) return false;
    for (int i = 0; i < s.size(); ++i)
    {
        const auto& gene = s.gene (i);
        const float v = g[(size_t) i];
        if (! std::isfinite (v) || v < gene.minValue || v > gene.maxValue) return false;
        if (gene.discrete() && v != std::round (v)) return false;
    }
    return true;
}

int engineOf (const Schema& s, const Genome& g) noexcept
{
    return s.engineGene() >= 0 ? std::clamp ((int) std::lround (g[(size_t) s.engineGene()]), 0, numEngineTypes - 1) : 0;
}

int layerOf (const Schema& s, const Genome& g) noexcept
{
    if (s.layerGene() < 0) return -1;
    const int l = (int) std::lround (g[(size_t) s.layerGene()]) - 1;
    return l >= 0 && l < numEngineTypes && l != engineOf (s, g) ? l : -1;
}

// ------------------------------------------------------------------------------------------------ crossover
namespace
{
float pickT (Choice c, Rng& rng)
{
    if (c == Choice::Auto)
    {
        const float r = rng.uniform();
        c = r < 0.35f ? Choice::A : (r < 0.7f ? Choice::B : Choice::Blend);
    }
    const float u = rng.uniform(); // drawn always: the sequence does not depend on the choice
    switch (c)
    {
        case Choice::A: return 0.0f;
        case Choice::B: return 1.0f;
        default: return 0.25f + 0.5f * u;
    }
}

/** Value between a and b at t (continuous: normalised blend, integer: rounded, choice / switch: one parent). */
float inherit (const Gene& gene, float a, float b, float t, Rng& rng)
{
    const float u = rng.uniform(); // always drawn (deterministic sequence)
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    if (a == b) return a;
    switch (gene.kind)
    {
        case ParamKind::Continuous:
        {
            const float v = gene.fromNorm (gene.toNorm (a) + (gene.toNorm (b) - gene.toNorm (a)) * t);
            return std::clamp (v, std::min (a, b), std::max (a, b)); // rounding of the skew mapping stays inside
        }
        case ParamKind::Integer: return gene.clamp (std::round (a + (b - a) * t));
        default: return u < t ? b : a;
    }
}
} // namespace

Genome crossover (const Schema& s, const Genome& a, const Genome& b, const Options& o, Rng& rng, Inheritance* inh)
{
    const int n = s.size();
    Genome c ((size_t) n);
    std::array<float, numGroups> t {};
    for (int g = 0; g < numChoiceGroups; ++g) t[(size_t) g] = pickT (o.choice[(size_t) g], rng);
    t[Fixed] = 0.0f;

    // engine: one parent's engine; a blend of two engines may keep the other one as the layer (hybrid)
    const int ea = engineOf (s, a), eb = engineOf (s, b);
    const float tEngine = t[Engine];
    const float u1 = rng.uniform(), u2 = rng.uniform();
    const bool engineFromB = tEngine >= 1.0f || (tEngine > 0.0f && u1 < tEngine);
    const Genome& ep = engineFromB ? b : a;
    const int childEngine = engineFromB ? eb : ea;
    const bool hybrid = ea != eb && tEngine > 0.0f && tEngine < 1.0f && layerOf (s, ep) < 0 && s.layerGene() >= 0 && u2 < o.hybridChance;
    int childLayer = layerOf (s, ep);
    if (hybrid) childLayer = engineFromB ? ea : eb;
    const float tEp = engineFromB ? 1.0f : 0.0f;
    // levels: a blend leaning to the engine parent (the part gain is level-matched afterwards)
    t[Level] = std::clamp (tEngine, 0.25f, 0.75f);

    // rack slot types (before their parameters)
    std::vector<int> slotType ((size_t) s.numSlots(), 0);
    for (int q = 0; q < s.numSlots(); ++q)
    {
        const int k = s.slotTypeGene (q);
        if (k < 0) continue;
        c[(size_t) k] = inherit (s.gene (k), a[(size_t) k], b[(size_t) k], t[Fx], rng);
        slotType[(size_t) q] = (int) std::lround (c[(size_t) k]);
    }

    // matrix routings travel as units unless both parents wire them alike
    std::vector<float> routeT ((size_t) s.numRoutes(), 0.0f);
    for (int r = 0; r < s.numRoutes(); ++r)
    {
        bool same = true;
        for (int k : s.routeMembers (r))
            if (! endsWith (s.gene (k).id, "amt") && a[(size_t) k] != b[(size_t) k]) same = false;
        const float u = rng.uniform();
        const float tm = t[Mod];
        routeT[(size_t) r] = same ? tm : (tm >= 1.0f || (tm > 0.0f && u < tm) ? 1.0f : 0.0f);
    }

    const int la = layerOf (s, a), lb = layerOf (s, b);
    for (int i = 0; i < n; ++i)
    {
        const auto& gene = s.gene (i);
        if (i == s.engineGene()) { c[(size_t) i] = (float) childEngine; continue; }
        if (s.isSlotType (i)) continue; // chosen above
        float ti = t[(size_t) gene.group];
        switch (gene.owner)
        {
            case Gene::OwnEngine:
            {
                const bool active = gene.ownerA == childEngine || gene.ownerA == childLayer;
                const bool ra = gene.ownerA == ea || gene.ownerA == la;
                const bool rb = gene.ownerA == eb || gene.ownerA == lb;
                if (! active) ti = tEp;            // not played: the engine parent's (silent) values
                else if (ra && ! rb) ti = 0.0f;
                else if (rb && ! ra) ti = 1.0f;
                else if (! ra && ! rb) ti = tEp;
                break;
            }
            case Gene::OwnFx:
            {
                const int q = gene.ownerA, k = s.slotTypeGene (q);
                const bool ra = k >= 0 && (int) std::lround (a[(size_t) k]) == gene.ownerB;
                const bool rb = k >= 0 && (int) std::lround (b[(size_t) k]) == gene.ownerB;
                const bool active = slotType[(size_t) q] == gene.ownerB;
                if (! active || (! ra && ! rb)) ti = 0.0f;
                else if (ra && ! rb) ti = 0.0f;
                else if (rb && ! ra) ti = 1.0f;
                break;
            }
            case Gene::OwnRoute: ti = routeT[(size_t) gene.ownerA]; break;
            default:
                if (gene.group == Engine) ti = tEp; // layer setup, mono bass: with the engine
                break;
        }
        c[(size_t) i] = inherit (gene, a[(size_t) i], b[(size_t) i], ti, rng);
    }

    if (hybrid)
    {
        c[(size_t) s.layerGene()] = (float) (childLayer + 1);
        if (const int k = s.indexOf ("layer.level"); k >= 0) c[(size_t) k] = s.gene (k).clamp (o.hybridLevel);
        if (const int k = s.indexOf ("layer.xmod"); k >= 0) c[(size_t) k] = s.gene (k).clamp (0.0f);
        if (const int k = s.indexOf ("layer.tune"); k >= 0) c[(size_t) k] = s.gene (k).clamp (0.0f);
        if (const int k = s.indexOf ("layer.fine"); k >= 0) c[(size_t) k] = s.gene (k).clamp (0.0f);
        if (const int k = s.indexOf ("layer.ownenv"); k >= 0) c[(size_t) k] = s.gene (k).clamp (0.0f);
    }

    if (inh != nullptr)
    {
        inh->fromB = t;
        inh->fromB[Engine] = engineFromB ? (hybrid ? 0.75f : 1.0f) : (hybrid ? 0.25f : 0.0f);
        inh->fromB[Fixed] = 0.0f;
        inh->hybrid = hybrid;
    }
    return c;
}

// ------------------------------------------------------------------------------------------------ mutation
void mutate (const Schema& s, Genome& g, const Genome& a, const Genome& b, float amount, Rng& rng)
{
    if (! (amount > 0.0f)) return;
    amount = std::min (amount, 1.0f);
    const float touch = 0.3f + 0.7f * amount;
    const float sigma = 0.22f * amount;
    const float pDiscrete = 0.12f * amount;
    const float pSlot = amount > 0.5f ? (amount - 0.5f) * 0.1f : 0.0f;
    for (int i = 0; i < s.size(); ++i)
    {
        const auto& gene = s.gene (i);
        const float u = rng.uniform(), z = rng.gauss(), w = rng.uniform();
        float& v = g[(size_t) i];
        if (s.isSlotType (i))
        {
            if (u < pSlot) v = gene.clamp ((float) rng.below ((int) gene.maxValue + 1));
            continue;
        }
        if (! gene.mutable_ || gene.group == Level || gene.group == Fixed) continue;
        if (! gene.discrete())
        {
            if (u >= touch) continue;
            const float n0 = gene.toNorm (v);
            float lo = 0.02f, hi = 0.98f;
            if (gene.risky) hi = std::min (hi, std::max (gene.toNorm (a[(size_t) i]), gene.toNorm (b[(size_t) i])) + 0.05f);
            lo = std::min (lo, n0);
            hi = std::max (hi, n0);
            v = gene.fromNorm (std::clamp (n0 + z * sigma, lo, hi));
            continue;
        }
        if (u >= pDiscrete) continue;
        const int lo = (int) gene.minValue, hi = (int) gene.maxValue, cur = (int) std::lround (v);
        if (hi <= lo) continue;
        if (gene.kind == ParamKind::Integer)
        {
            const int step = (1 + (int) (w * 2.0f)) * (z < 0.0f ? -1 : 1);
            v = (float) std::clamp (cur + step, lo, hi);
        }
        else
        {
            int nv = lo + (int) (w * (float) (hi - lo)); // another option
            if (nv >= cur) ++nv;
            v = (float) std::clamp (nv, lo, hi);
        }
    }
}

std::vector<Child> makeBrood (const Schema& s, const Genome& a, const Genome& b, int count, float amount, uint64_t seed, const Options& o)
{
    std::vector<Child> out;
    for (int i = 0; i < count; ++i)
    {
        Child c;
        c.seed = Rng::mix (seed, (uint64_t) i + 1);
        Rng rng (c.seed);
        c.genome = crossover (s, a, b, o, rng, &c.inheritance);
        mutate (s, c.genome, a, b, amount, rng);
        out.push_back (std::move (c));
    }
    return out;
}

// ------------------------------------------------------------------------------------------------ Session
Session::Session (Schema schema) : sch (std::move (schema)) {}

void Session::setParent (int which, const std::string& name, Genome g)
{
    auto& p = which == 0 ? pa : pb;
    p.name = name;
    p.genome = std::move (g);
    p.set = (int) p.genome.size() == sch.size();
}

std::string Session::childName (const std::string& a, const std::string& b, int gen, int index)
{
    auto first = [] (const std::string& s) {
        const auto p = s.find (' ');
        return p == std::string::npos ? s : s.substr (0, p);
    };
    const auto fa = first (a), fb = first (b);
    std::string base = fa.empty() ? "Child" : fa;
    if (! fb.empty() && fb != fa) base += " " + fb;
    return base + " " + std::to_string (gen) + "." + std::to_string (index + 1);
}

void Session::push (Generation g)
{
    if (cur + 1 < (int) history.size()) history.erase (history.begin() + cur + 1, history.end()); // a new branch drops the redo
    history.push_back (std::move (g));
    if ((int) history.size() > maxGenerations) history.erase (history.begin());
    cur = (int) history.size() - 1;
}

bool Session::breedParents (uint64_t seed)
{
    if (! ready()) return false;
    Generation g;
    g.number = current() != nullptr ? current()->number + 1 : 1;
    g.a = pa;
    g.b = pb;
    g.amount = amount;
    g.seed = seed;
    g.children = makeBrood (sch, pa.genome, pb.genome, std::clamp (broodSize, 1, 16), amount, seed, options);
    for (int i = 0; i < (int) g.children.size(); ++i) g.children[(size_t) i].name = childName (pa.name, pb.name, g.number, i);
    push (std::move (g));
    return true;
}

bool Session::breedAgain (uint64_t seed)
{
    const auto* now = current();
    if (now == nullptr) return breedParents (seed);
    std::vector<const Child*> kept;
    for (const auto& c : now->children)
        if (c.kept) kept.push_back (&c);
    if (kept.empty())
    {
        pa = now->a;
        pb = now->b;
        return breedParents (seed);
    }
    Generation g;
    g.number = now->number + 1;
    g.amount = amount;
    g.seed = seed;
    const int count = std::clamp (broodSize, 1, 16);
    const int m = (int) kept.size();
    for (int i = 0; i < count; ++i)
    {
        const Child* x = kept[(size_t) (i % m)];
        const Child* y = m == 1 ? x : kept[(size_t) ((i % m + 1 + (i / m) % (m - 1)) % m)]; // every pair in turn
        Child c;
        c.seed = Rng::mix (seed, (uint64_t) i + 1);
        Rng rng (c.seed);
        c.genome = crossover (sch, x->genome, y->genome, options, rng, &c.inheritance);
        mutate (sch, c.genome, x->genome, y->genome, m == 1 ? std::max (amount, 0.1f) : amount, rng);
        c.name = childName (x->name, y->name, g.number, i);
        g.children.push_back (std::move (c));
    }
    g.a = { kept[0]->name, kept[0]->genome, true };
    g.b = { m > 1 ? kept[1]->name : kept[0]->name, m > 1 ? kept[1]->genome : kept[0]->genome, true };
    pa = g.a;
    pb = g.b;
    push (std::move (g));
    return true;
}

void Session::setKept (int child, bool keep)
{
    if (auto* g = current(); g != nullptr && child >= 0 && child < (int) g->children.size()) g->children[(size_t) child].kept = keep;
}

void Session::promote (int child, int which)
{
    if (const auto* g = current(); g != nullptr && child >= 0 && child < (int) g->children.size())
    {
        const auto c = g->children[(size_t) child]; // copy: setParent must not read what it overwrites
        setParent (which, c.name, c.genome);
    }
}

void Session::clear()
{
    history.clear();
    cur = -1;
}

namespace
{
json genomeJson (const Schema& s, const Genome& g)
{
    json o = json::object();
    for (const auto& [k, v] : toParams (s, g, true)) o[k] = v;
    return o;
}

Genome genomeFrom (const Schema& s, const json& j)
{
    std::map<std::string, float> m;
    if (j.is_object())
        for (auto it = j.begin(); it != j.end(); ++it)
            if (it.value().is_number()) m[it.key()] = it.value().get<float>();
    return fromParams (s, m);
}

json parentJson (const Schema& s, const Session::Parent& p)
{
    if (! p.set) return nullptr;
    return json { { "name", p.name }, { "params", genomeJson (s, p.genome) } };
}

Session::Parent parentFrom (const Schema& s, const json& j)
{
    Session::Parent p;
    if (! j.is_object()) return p;
    p.name = j.value ("name", std::string());
    p.genome = genomeFrom (s, j.value ("params", json::object()));
    p.set = true;
    return p;
}
} // namespace

std::string Session::toJson() const
{
    json j;
    j["version"] = 1;
    j["amount"] = amount;
    j["brood"] = broodSize;
    j["a"] = parentJson (sch, pa);
    j["b"] = parentJson (sch, pb);
    j["current"] = cur;
    json choices = json::array();
    for (auto c : options.choice) choices.push_back ((int) c);
    j["choice"] = std::move (choices);
    json gens = json::array();
    for (const auto& g : history)
    {
        json jg;
        jg["number"] = g.number;
        jg["amount"] = g.amount;
        jg["seed"] = std::to_string (g.seed); // 64 bits as text
        jg["a"] = parentJson (sch, g.a);
        jg["b"] = parentJson (sch, g.b);
        json kids = json::array();
        for (const auto& c : g.children)
        {
            json jc;
            jc["name"] = c.name;
            jc["seed"] = std::to_string (c.seed);
            jc["kept"] = c.kept;
            jc["gainDb"] = c.gainDb;
            jc["matched"] = c.matched;
            jc["hybrid"] = c.inheritance.hybrid;
            jc["fromB"] = c.inheritance.fromB;
            jc["params"] = genomeJson (sch, c.genome);
            kids.push_back (std::move (jc));
        }
        jg["children"] = std::move (kids);
        gens.push_back (std::move (jg));
    }
    j["generations"] = std::move (gens);
    return j.dump();
}

bool Session::fromJson (const std::string& text)
{
    const auto j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object()) return false;
    amount = std::clamp (j.value ("amount", 0.3f), 0.0f, 1.0f);
    broodSize = std::clamp (j.value ("brood", defaultBrood), 1, 16);
    pa = parentFrom (sch, j.value ("a", json()));
    pb = parentFrom (sch, j.value ("b", json()));
    options = {};
    if (j.contains ("choice") && j["choice"].is_array())
        for (size_t k = 0; k < options.choice.size() && k < j["choice"].size(); ++k)
            if (j["choice"][k].is_number_integer()) options.choice[k] = (Choice) std::clamp (j["choice"][k].get<int>(), 0, 3);
    history.clear();
    const auto gens = j.value ("generations", json::array());
    for (const auto& jg : gens)
    {
        Generation g;
        g.number = jg.value ("number", 1);
        g.amount = jg.value ("amount", 0.3f);
        g.seed = std::strtoull (jg.value ("seed", std::string ("0")).c_str(), nullptr, 10);
        g.a = parentFrom (sch, jg.value ("a", json()));
        g.b = parentFrom (sch, jg.value ("b", json()));
        for (const auto& jc : jg.value ("children", json::array()))
        {
            Child c;
            c.name = jc.value ("name", std::string());
            c.seed = std::strtoull (jc.value ("seed", std::string ("0")).c_str(), nullptr, 10);
            c.kept = jc.value ("kept", false);
            c.gainDb = jc.value ("gainDb", 0.0f);
            c.matched = jc.value ("matched", false);
            c.inheritance.hybrid = jc.value ("hybrid", false);
            if (jc.contains ("fromB") && jc["fromB"].is_array())
                for (size_t k = 0; k < c.inheritance.fromB.size() && k < jc["fromB"].size(); ++k)
                    if (jc["fromB"][k].is_number()) c.inheritance.fromB[k] = jc["fromB"][k].get<float>();
            c.genome = genomeFrom (sch, jc.value ("params", json::object()));
            g.children.push_back (std::move (c));
        }
        history.push_back (std::move (g));
        if ((int) history.size() >= maxGenerations) break;
    }
    cur = history.empty() ? -1 : std::clamp (j.value ("current", (int) history.size() - 1), 0, (int) history.size() - 1);
    return true;
}

// ------------------------------------------------------------------------------------------------ Audition
Audition::Audition (double sampleRate) : engineCore (std::make_unique<Core>())
{
    engineCore->prepare (sampleRate, 256);
    sch = Schema::forPart (engineCore->params(), 0);
}

Audition::~Audition() = default;

Audition::Sketch Audition::render (const Genome& g, float seconds)
{
    auto& core = *engineCore;
    preset::Preset p;
    p.meta.kind = preset::Kind::Part;
    p.params = toParams (sch, g);
    preset::PartOptions opt;
    opt.arp = false;
    preset::applyPart (p, core.params(), 0, opt);
    core.params().set ("mix.master.limit", 0.0f);
    core.prepareEffects();
    core.reset();

    const double sr = core.sampleRate();
    const int ch = std::clamp ((int) std::lround (core.params().get ("p1.midich")), 1, 16);
    const auto hold = (int64_t) (seconds * sr);
    const auto total = (int64_t) ((seconds + 0.25f) * sr);
    std::vector<float> l ((size_t) total, 0.0f), r ((size_t) total, 0.0f);
    constexpr int block = 256;
    for (int64_t pos = 0; pos < total; pos += block)
    {
        if (aborted()) break;
        const int nb = (int) std::min<int64_t> (block, total - pos);
        MidiEvent ev[2];
        int ne = 0;
        if (pos == 0)
        {
            ev[0] = MidiEvent::noteOn (ch, 60, 100);
            ev[1] = MidiEvent::noteOn (ch, 67, 100);
            ne = 2;
        }
        else if (hold >= pos && hold < pos + nb)
        {
            ev[0] = MidiEvent::noteOff (ch, 60, (int) (hold - pos));
            ev[1] = MidiEvent::noteOff (ch, 67, (int) (hold - pos));
            ne = 2;
        }
        OutputBuffers out;
        out.left[0] = l.data() + pos;
        out.right[0] = r.data() + pos;
        Transport tr;
        core.process (out, nb, ne > 0 ? ev : nullptr, ne, tr);
    }
    core.reset();

    Sketch s;
    double peak = 0.0, sx = 0.0, sd = 0.0;
    float prev = 0.0f;
    const size_t n = l.size();
    for (size_t i = 0; i < n; ++i)
    {
        const float a = l[i], b = r[i];
        if (! std::isfinite (a) || ! std::isfinite (b)) { ++s.nonFinite; continue; }
        peak = std::max ({ peak, (double) std::abs (a), (double) std::abs (b) });
        const float m = 0.5f * (a + b);
        sx += (double) m * m;
        sd += (double) (m - prev) * (m - prev);
        prev = m;
        const size_t slice = std::min ((size_t) thumbPoints - 1, i * (size_t) thumbPoints / n);
        s.wave[slice] = std::max (s.wave[slice], std::min (1.0f, std::max (std::abs (a), std::abs (b))));
    }
    s.peakDb = peak > 1.0e-10 ? (float) (20.0 * std::log10 (peak)) : -200.0f;
    if (sx > 1.0e-12)
    {
        // spectral centroid estimate: for a sine of frequency f, rms (x[n] - x[n-1]) / rms (x) = 2 sin (pi f / sr)
        const double ratio = std::min (1.0, 0.5 * std::sqrt (sd / sx));
        const double f = std::asin (ratio) * sr / 3.14159265358979;
        s.brightness = (float) std::clamp (std::log (std::max (f, 1.0) / 80.0) / std::log (100.0), 0.0, 1.0);
    }
    const size_t win = (size_t) (0.05 * sr);
    std::vector<double> powers;
    for (size_t st = 0; st + win <= n; st += win)
    {
        double e = 0.0;
        for (size_t i = st; i < st + win; ++i)
            if (std::isfinite (l[i]) && std::isfinite (r[i])) e += (double) l[i] * l[i] + (double) r[i] * r[i];
        powers.push_back (e / (2.0 * (double) win));
    }
    if (! powers.empty())
    {
        const double loudest = *std::max_element (powers.begin(), powers.end());
        double e = 0.0;
        int count = 0;
        for (double pw : powers)
            if (pw > loudest * 1.0e-3) { e += pw; ++count; }
        if (count > 0 && e > 0.0) s.rmsDb = (float) (10.0 * std::log10 (std::max (1.0e-20, e / count)));
    }
    return s;
}

Audition::Sketch Audition::matchLevel (Genome& g, float targetDb, float maxBoostDb)
{
    const int k = sch.gainGene();
    auto s = render (g);
    if (k < 0 || s.nonFinite > 0 || s.rmsDb < -90.0f) return s;
    const auto& gene = sch.gene (k);
    const float base = g[(size_t) k];
    const float ceiling = std::min (gene.maxValue, base + std::max (0.0f, maxBoostDb));
    for (int pass = 0; pass < 4; ++pass)
    {
        if (aborted()) break;
        float delta = targetDb - s.rmsDb;
        delta = std::min (delta, -1.5f - s.peakDb); // the peak stays below -1.5 dBFS
        if (std::abs (delta) < 0.5f && s.peakDb < -1.0f) break;
        const float next = std::clamp (g[(size_t) k] + delta, gene.minValue, ceiling);
        if (next == g[(size_t) k]) break;
        g[(size_t) k] = next;
        s = render (g);
    }
    return s;
}
} // namespace ohpe::breed
