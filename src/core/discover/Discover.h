// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ohpe::discover
{
/**
    p21: the Discover strip of the pro view - a part's sound as a list of genes (parameter ID + normalised value) that
    can be varied (six siblings), crossed (two sounds -> one), rolled per group (dice, gentle .. wild, locks) and
    stepped back through (the last 20 states).

    The breeding itself sits behind the small `Breeder` interface. The reusable breeding core of issue #13 plugs in
    there when it lands (breeder() returns it); until then `basicBreeder()` is used - a plain, deterministic
    implementation that only knows normalised values and discrete steps.

    Message thread only (allocates).
*/
struct Gene
{
    std::string id;    ///< full parameter ID ("p2.fm.index")
    float value = 0.0f; ///< normalised 0..1
    int steps = 0;     ///< 0 = continuous, else the number of discrete values (choices, integers, toggles)
};

struct Genome
{
    std::vector<Gene> genes;
    bool empty() const noexcept { return genes.empty(); }
    size_t size() const noexcept { return genes.size(); }
    /** Index of a gene by ID (-1 = not in this genome). */
    int indexOf (const std::string& id) const noexcept;
    /** Mean absolute difference of the normalised values (genomes over the same IDs); 0 = identical. */
    float distance (const Genome& other) const noexcept;
};

/** Which parts of a part's parameters take part in breeding: its sound, not its routing or performance setup. */
bool isBreedable (const std::string& partLocalId) noexcept;

/** The breedable genes of `ids` read from the registry (unknown IDs are skipped). */
Genome capture (const ParamRegistry& reg, const std::vector<std::string>& ids);
/** Writes a genome into the registry (tests / headless; the editor writes through its host parameters). */
void apply (ParamRegistry& reg, const Genome& g);

/** The interface the breeding core implements (see the note above). */
class Breeder
{
public:
    virtual ~Breeder() = default;
    /** A sibling of `parent`: genes with mask[i] == false (locked) stay; strength 0 = gentle .. 1 = wild. */
    virtual Genome vary (const Genome& parent, float strength, uint32_t seed, const std::vector<bool>& mask) const = 0;
    /** A child of two genomes over the same IDs (genes of `a` that `b` does not have stay as in `a`). */
    virtual Genome cross (const Genome& a, const Genome& b, uint32_t seed) const = 0;
};

const Breeder& basicBreeder();
/** The breeder the UI uses (the basic one until the shared breeding core is merged). */
const Breeder& breeder();

/** `count` siblings of `parent` (seeds seed, seed + 1, ...); strength 0..1. */
std::vector<Genome> siblings (const Genome& parent, int count, float strength, uint32_t seed, const std::vector<bool>& mask = {});

/** The last `capacity` states (oldest first) and the one shown; pushing after stepping back drops the newer ones. */
class Timeline
{
public:
    explicit Timeline (size_t capacity = 20) : cap (capacity) {}
    void push (Genome g);
    size_t size() const noexcept { return states.size(); }
    size_t capacity() const noexcept { return cap; }
    int current() const noexcept { return cursor; }
    const Genome* at (int i) const noexcept { return i >= 0 && i < (int) states.size() ? &states[(size_t) i] : nullptr; }
    /** Moves the cursor (clamped) and returns that state (nullptr when empty). */
    const Genome* select (int i) noexcept;
    void clear() noexcept { states.clear(); cursor = -1; }

private:
    size_t cap;
    std::vector<Genome> states;
    int cursor = -1;
};
} // namespace ohpe::discover
