// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/ParamRegistry.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ohpe
{
class Core;
}

/**
    p18: preset breeding ("Preset-Zucht") - the reusable, engine-agnostic core.

    A sound is a *genome*: the plain values of every parameter of one part, in the order of a *schema* that is built
    from the parameter registry alone (descriptors: range, skew, kind). Nothing here knows an engine's DSP; the schema
    only reads the parameter IDs to sort each one into a *gene group* and to find out which engine / effect / routing
    owns it, so new engines and effects take part without code changes.

    * Groups: Engine (engine choice, layer, the engines' own sound parameters), Filter (cutoff / resonance / filter
      type / tone / brightness / drive / key tracking of the engines), Envelope (amp / mod / third envelope, pitch
      envelope, glide, velocity), FX (the six rack slots, space), Mod (LFOs, matrix routings, macros, MPE amounts,
      life), Level (volume, part gain, layer level) and Fixed (tuning, voices, MIDI routing, morph block: inherited from
      parent A unchanged).
    * Crossover: per group the child takes parent A, parent B, or a blend (one blend position per group, in normalised
      units so skewed ranges blend evenly). Discrete parameters are never interpolated: choices and switches come from
      one parent, whole numbers are rounded. Ownership keeps sounds coherent: parameters of an engine come from the
      parent that plays that engine (main or layer), effect parameters from the parent whose slot holds that effect, a
      matrix routing (source, via, target, curve, amount) travels as one unit unless both parents route the same.
      Two parents with different engines: the child plays the engine of one of them; on a blend it may keep the other
      engine as its layer (a hybrid), at a moderate layer level.
    * Mutation (0 .. 1): Gaussian steps on continuous parameters (normalised, safe range 2 .. 98 %), rare steps on
      discrete ones. Never mutated: engine choice, sample choice, routing sources / targets, levels; resonance,
      feedback, drive, gain-like parameters cannot rise above the louder parent by more than a small margin.
      Mutation 0 leaves the parent blend untouched (every value between the parents' values).
    * Levels: the child's volume / gain / layer level are blended from its parents; Audition::matchLevel then sets the
      part gain so the child plays as loud as its parents did (rendered offline), so a brood never jumps in level.

    Deterministic: every random choice comes from Rng (SplitMix64 + Box-Muller, no std distributions), so a seed gives
    the same brood on every platform.
*/
namespace ohpe::breed
{
enum Group : int { Engine = 0, Filter, Envelope, Fx, Mod, Level, Fixed, numGroups };
/** Groups the user can steer (Level and Fixed follow rules). */
constexpr int numChoiceGroups = 5;
const char* groupName (int group) noexcept;   ///< "Engine", "Filter" ... (English, UI translates)

/** Deterministic random numbers (identical on every platform). */
class Rng
{
public:
    explicit Rng (uint64_t seed = 1) noexcept : state (seed) {}
    uint64_t next() noexcept;
    float uniform() noexcept;                  ///< [0, 1)
    float gauss() noexcept;                    ///< standard normal
    int below (int n) noexcept;                ///< [0, n)
    static uint64_t mix (uint64_t a, uint64_t b) noexcept; ///< hash of two numbers (child seeds)

private:
    uint64_t state;
    bool haveSpare = false;
    float spare = 0.0f;
};

struct Gene
{
    std::string id;           ///< local ID ("analog.cutoff", "fx3.delay.mix")
    int group = Fixed;
    ParamKind kind = ParamKind::Continuous;
    float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f, skew = 1.0f;
    bool mutable_ = true;     ///< mutation may change it
    bool risky = false;       ///< resonance / feedback / drive / gain-like: capped near the parents' maximum
    /** p32: false = a fixed gene that takes parent A's value without drawing random numbers, so a gene appended to
        the part keeps the broods of every seed exactly as they were (crossover and mutation share one sequence). */
    bool drawsRandom = true;
    // ownership: the parameter only matters when its owner is active
    enum OwnerKind : int8_t { OwnNone = 0, OwnEngine, OwnFx, OwnRoute } owner = OwnNone;
    int ownerA = -1, ownerB = -1; ///< engine type / (rack slot, effect type) / (routing slot, -)
    bool discrete() const noexcept { return kind != ParamKind::Continuous; }
    float toNorm (float plain) const noexcept;
    float fromNorm (float norm) const noexcept;
    float clamp (float plain) const noexcept;
};

/** The genes of one part (the same for every part: built from part 1's registry entries). */
class Schema
{
public:
    /** Builds the schema of part `partIndex` (0-based) from the registry. */
    static Schema forPart (const ParamRegistry& reg, int partIndex = 0);

    int size() const noexcept { return (int) genes.size(); }
    const Gene& gene (int i) const noexcept { return genes[(size_t) i]; }
    int indexOf (const std::string& localId) const noexcept;
    int engineGene() const noexcept { return engineK; }
    int layerGene() const noexcept { return layerK; }
    int gainGene() const noexcept { return gainK; }
    int slotTypeGene (int slot) const noexcept { return slot >= 0 && slot < (int) slotTypeK.size() ? slotTypeK[(size_t) slot] : -1; }
    int numSlots() const noexcept { return (int) slotTypeK.size(); }
    bool isSlotType (int i) const noexcept { return i >= 0 && i < (int) slotFlag.size() && slotFlag[(size_t) i] != 0; }
    int numRoutes() const noexcept { return (int) routeGenes.size(); }
    /** Genes of matrix routing `r` and which of them are the discrete "wiring" (source, via, target, curve). */
    const std::vector<int>& routeMembers (int r) const noexcept { return routeGenes[(size_t) r]; }

private:
    std::vector<Gene> genes;
    std::map<std::string, int> byId;
    int engineK = -1, layerK = -1, gainK = -1;
    std::vector<int> slotTypeK;
    std::vector<uint8_t> slotFlag;
    std::vector<std::vector<int>> routeGenes;
};

/** Plain values in schema order. */
using Genome = std::vector<float>;

/** A genome from local parameters (a part preset's "params"): missing ones take their defaults. */
Genome fromParams (const Schema& s, const std::map<std::string, float>& localParams);
/** The genome of part `partIndex` in the registry. */
Genome fromRegistry (const Schema& s, const ParamRegistry& reg, int partIndex);
/** Local parameters of a genome (every gene; `sparse` = only values that differ from the default). */
std::map<std::string, float> toParams (const Schema& s, const Genome& g, bool sparse = false);
/** True when every value is finite and inside its range (discrete ones whole numbers). */
bool valid (const Schema& s, const Genome& g);

/** Engine type (EngineType) a genome plays; its layer engine (-1 = none). */
int engineOf (const Schema& s, const Genome& g) noexcept;
int layerOf (const Schema& s, const Genome& g) noexcept;

/** How a group is inherited. Auto = random per child (A, B or a blend). */
enum class Choice : int { Auto = 0, A, B, Blend };

struct Options
{
    std::array<Choice, numChoiceGroups> choice {}; ///< per steerable group (Engine .. Mod)
    float hybridChance = 0.5f;  ///< parents with different engines, Engine group blended: chance of a layered hybrid
    float hybridLevel = 0.45f;  ///< layer level of a hybrid (plain, 0..1)
};

/** What a child got from whom (UI: the parent chips of a tile). */
struct Inheritance
{
    std::array<float, numGroups> fromB {}; ///< 0 = parent A, 1 = parent B, between = blend position
    bool hybrid = false;                   ///< plays the other parent's engine as its layer
};

/** Crossover of two genomes (no mutation). */
Genome crossover (const Schema& s, const Genome& a, const Genome& b, const Options& o, Rng& rng, Inheritance* inh = nullptr);
/** Mutates `g` in place by `amount` (0..1). `a` / `b` are its parents (safe caps for risky genes). */
void mutate (const Schema& s, Genome& g, const Genome& a, const Genome& b, float amount, Rng& rng);

struct Child
{
    Genome genome;
    Inheritance inheritance;
    uint64_t seed = 0;
    std::string name;
    bool kept = false;     ///< heart: survives into the next generation as a parent
    float gainDb = 0.0f;   ///< level match applied (0 = none yet)
    bool matched = false;
};

/** `count` children of a and b: child i = crossover + mutation with seed mix (seed, i). */
std::vector<Child> makeBrood (const Schema& s, const Genome& a, const Genome& b, int count, float amount, uint64_t seed,
                          const Options& o = {});

/**
    A breeding session: the parents, the generations bred so far (history with undo / redo) and the kept children.
    Engine-agnostic and JUCE-free; the UI (and later the Discover strip) drive it.
*/
class Session
{
public:
    static constexpr int maxGenerations = 24;
    static constexpr int defaultBrood = 6;

    struct Parent
    {
        std::string name;
        Genome genome;
        bool set = false;
    };
    struct Generation
    {
        int number = 1;
        Parent a, b;
        float amount = 0.3f;
        uint64_t seed = 0;
        std::vector<Child> children;
    };

    explicit Session (Schema schema);
    const Schema& schema() const noexcept { return sch; }

    void setParent (int which, const std::string& name, Genome g); ///< which 0 = A, 1 = B
    const Parent& parent (int which) const noexcept { return which == 0 ? pa : pb; }
    bool ready() const noexcept { return pa.set && pb.set; }

    float amount = 0.3f;     ///< mutation of the next brood
    int broodSize = defaultBrood;
    Options options;

    /** New generation from the two parents (a brood of broodSize children). False when a parent is missing. */
    bool breedParents (uint64_t seed);
    /** Next generation from the kept children of the current one: two or more kept = crosses among them, one kept =
        that child with itself (mutation only, at least 10 %), none kept = the current parents again. */
    bool breedAgain (uint64_t seed);

    bool empty() const noexcept { return history.empty(); }
    int numGenerations() const noexcept { return (int) history.size(); }
    int currentIndex() const noexcept { return cur; }
    const Generation* current() const noexcept { return cur >= 0 ? &history[(size_t) cur] : nullptr; }
    Generation* current() noexcept { return cur >= 0 ? &history[(size_t) cur] : nullptr; }
    bool canUndo() const noexcept { return cur > 0; }
    bool canRedo() const noexcept { return cur + 1 < (int) history.size(); }
    void undo() noexcept { if (canUndo()) --cur; }
    void redo() noexcept { if (canRedo()) ++cur; }

    void setKept (int child, bool keep);
    /** Makes a child of the current generation parent A or B. */
    void promote (int child, int which);
    void clear();

    std::string toJson() const;
    bool fromJson (const std::string& json);

private:
    void push (Generation g);
    static std::string childName (const std::string& a, const std::string& b, int gen, int index);
    Schema sch;
    Parent pa, pb;
    std::vector<Generation> history;
    int cur = -1;
};

/**
    Offline audition: renders a genome on a private Core (part 1, master limiter off) for thumbnails, brightness and
    level matching. Message / worker thread only; never the audio thread.
*/
class Audition
{
public:
    static constexpr int thumbPoints = 48;
    struct Sketch
    {
        std::array<float, thumbPoints> wave {}; ///< peak per slice, 0..1 (full scale = 1)
        float rmsDb = -200.0f;   ///< active RMS (50 ms windows within 30 dB of the loudest)
        float peakDb = -200.0f;
        float brightness = 0.0f; ///< 0 (dark, ~80 Hz) .. 1 (bright, ~8 kHz): spectral centroid estimate
        int nonFinite = 0;
    };

    explicit Audition (double sampleRate = 32000.0);
    ~Audition();
    Audition (const Audition&) = delete;
    Audition& operator= (const Audition&) = delete;

    const Schema& schema() const noexcept { return sch; }
    /** Plays C4 + G4 (velocity 100) for `seconds`, then 0.25 s of tail. */
    Sketch render (const Genome& g, float seconds = 0.7f);
    /**
        Sets the part gain of `g` so it plays at `targetDb` (active RMS): at most `maxBoostDb` louder than its blended
        gain, any amount quieter, peak kept below -1 dBFS. Returns the sketch of the result.
    */
    Sketch matchLevel (Genome& g, float targetDb, float maxBoostDb = 6.0f);
    Core& core() noexcept { return *engineCore; }
    /** Optional cancel flag (owned by the caller, e.g. a worker thread): when set, render / matchLevel stop within one
        block and return what they have (the sketch is then incomplete and should be discarded). */
    void setAbortFlag (const std::atomic<bool>* flag) noexcept { abortFlag = flag; }
    bool aborted() const noexcept { return abortFlag != nullptr && abortFlag->load (std::memory_order_relaxed); }

private:
    std::unique_ptr<Core> engineCore;
    Schema sch;
    const std::atomic<bool>* abortFlag = nullptr;
};
} // namespace ohpe::breed
