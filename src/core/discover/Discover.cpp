// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Discover.h"

#include <algorithm>
#include <cmath>

namespace ohpe::discover
{
namespace
{
/** Small deterministic generator (xorshift32 + a mixing step for the seed). */
struct Rng
{
    explicit Rng (uint32_t seed) noexcept
    {
        uint32_t h = seed * 0x9E3779B1u + 0x7F4A7C15u;
        h ^= h >> 15;
        h *= 0x85EBCA77u;
        h ^= h >> 13;
        state = h != 0 ? h : 0x1234567u;
    }
    uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); } ///< 0..1
    float bipolar() noexcept { return 2.0f * uniform() - 1.0f; }
    /** Roughly normal (sum of three uniforms), standard deviation about 1. */
    float normal() noexcept { return (bipolar() + bipolar() + bipolar()) * 1.0f; }
    uint32_t state;
};

float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

float snap (float v, int steps) noexcept
{
    if (steps <= 1) return clamp01 (v);
    const float n = (float) (steps - 1);
    return clamp01 (std::round (clamp01 (v) * n) / n);
}

class BasicBreeder final : public Breeder
{
public:
    Genome vary (const Genome& parent, float strength, uint32_t seed, const std::vector<bool>& mask) const override
    {
        Genome g = parent;
        Rng rng (seed);
        const float s = clamp01 (strength);
        const float sigma = 0.03f + 0.22f * s;        // continuous genes: spread of the step
        const float moveP = 0.45f + 0.5f * s;         // share of genes that move at all
        const float jumpP = 0.04f + 0.30f * s;        // discrete genes: chance to change
        for (size_t i = 0; i < g.genes.size(); ++i)
        {
            auto& gene = g.genes[i];
            const bool free = mask.empty() || (i < mask.size() && mask[i]);
            const float r1 = rng.uniform(), r2 = rng.normal(), r3 = rng.uniform(); // drawn for every gene: stable streams
            if (! free) continue;
            if (gene.steps > 1)
            {
                if (r1 < jumpP)
                {
                    const int n = gene.steps;
                    const int cur = (int) std::lround (gene.value * (float) (n - 1));
                    int k = (int) (r3 * (float) n);
                    if (k >= n) k = n - 1;
                    if (k == cur) k = (k + 1) % n;
                    gene.value = n > 1 ? (float) k / (float) (n - 1) : 0.0f;
                }
            }
            else if (r1 < moveP)
                gene.value = clamp01 (gene.value + sigma * r2 * 0.6f);
        }
        return g;
    }

    Genome cross (const Genome& a, const Genome& b, uint32_t seed) const override
    {
        Genome g = a;
        Rng rng (seed ^ 0xC2B2AE3Du);
        for (auto& gene : g.genes)
        {
            const int j = b.indexOf (gene.id);
            const float r = rng.uniform(), t = rng.uniform();
            if (j < 0) continue;
            const float other = b.genes[(size_t) j].value;
            if (gene.steps > 1) gene.value = r < 0.5f ? gene.value : other;          // discrete: from one parent
            else gene.value = clamp01 (gene.value + (other - gene.value) * (0.25f + 0.5f * t)); // continuous: a blend
        }
        return g;
    }
};
} // namespace

int Genome::indexOf (const std::string& id) const noexcept
{
    for (size_t i = 0; i < genes.size(); ++i)
        if (genes[i].id == id) return (int) i;
    return -1;
}

float Genome::distance (const Genome& other) const noexcept
{
    if (genes.empty()) return 0.0f;
    float sum = 0.0f;
    int n = 0;
    for (const auto& g : genes)
        if (const int j = other.indexOf (g.id); j >= 0) { sum += std::abs (g.value - other.genes[(size_t) j].value); ++n; }
    return n > 0 ? sum / (float) n : 0.0f;
}

bool isBreedable (const std::string& id) noexcept
{
    // the part's routing, performance and housekeeping stay as they are
    static const char* exact[] = { "engine", "vol", "voices", "midich", "quant", "chord", "bend", "gain", "polyglide", "glide.mode",
                                   "glide.rate", "monobass", "velsens", "tune", "life", "layer.engine", "layer.ownenv", "seqlane" };
    static const char* prefix[] = { "mpe", "morph.", "macro", "char.", "fx", "space." };
    for (const char* e : exact)
        if (id == e) return false;
    for (const char* p : prefix)
        if (id.rfind (p, 0) == 0) return false;
    if (id.rfind ("mod.s", 0) == 0 && id.size() > 5 && id[5] >= '0' && id[5] <= '9') return false; // matrix routings
    // engine parameters that pick a sample or an instrument (loading, not sound design)
    if (id.rfind ("sampler.", 0) == 0 || id.rfind ("grain.", 0) == 0)
        for (const char* tail : { ".source", ".inst", ".inst2", ".root", ".mode", ".slicesens", ".slicebase", ".loop", ".reverse" })
        {
            const std::string t (tail);
            if (id.size() > t.size() && id.compare (id.size() - t.size(), t.size(), t) == 0) return false;
        }
    return true;
}

Genome capture (const ParamRegistry& reg, const std::vector<std::string>& ids)
{
    Genome g;
    for (const auto& id : ids)
    {
        const int idx = reg.indexOf (id);
        if (idx < 0) continue;
        const auto dot = id.find ('.');
        if (dot == std::string::npos || ! isBreedable (id.substr (dot + 1))) continue;
        const auto& d = reg.descriptor (idx);
        Gene gene;
        gene.id = id;
        gene.value = d.toNormalised (reg.get (idx));
        gene.steps = d.isDiscrete() ? std::max (2, (int) std::lround (d.maxValue - d.minValue) + 1) : 0;
        g.genes.push_back (std::move (gene));
    }
    return g;
}

void apply (ParamRegistry& reg, const Genome& g)
{
    for (const auto& gene : g.genes)
    {
        const int idx = reg.indexOf (gene.id);
        if (idx < 0) continue;
        reg.set (idx, reg.descriptor (idx).fromNormalised (snap (gene.value, gene.steps)));
    }
}

const Breeder& basicBreeder()
{
    static const BasicBreeder b;
    return b;
}

const Breeder& breeder() { return basicBreeder(); }

std::vector<Genome> siblings (const Genome& parent, int count, float strength, uint32_t seed, const std::vector<bool>& mask)
{
    std::vector<Genome> out;
    for (int i = 0; i < count; ++i)
    {
        auto g = breeder().vary (parent, strength, seed + (uint32_t) i * 7919u, mask);
        for (auto& gene : g.genes) gene.value = snap (gene.value, gene.steps);
        out.push_back (std::move (g));
    }
    return out;
}

void Timeline::push (Genome g)
{
    if (cursor >= 0 && cursor + 1 < (int) states.size()) states.resize ((size_t) cursor + 1); // drop the "future"
    if (! states.empty() && states.back().distance (g) == 0.0f && states.back().size() == g.size())
    {
        cursor = (int) states.size() - 1;
        return; // the same state twice is one dot
    }
    states.push_back (std::move (g));
    while (states.size() > cap) states.erase (states.begin());
    cursor = (int) states.size() - 1;
}

const Genome* Timeline::select (int i) noexcept
{
    if (states.empty()) return nullptr;
    cursor = std::clamp (i, 0, (int) states.size() - 1);
    return &states[(size_t) cursor];
}
} // namespace ohpe::discover
