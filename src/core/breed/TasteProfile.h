// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "SoundGenome.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
    p69: "learning breed" ("Zucht lernt mit") - a small, local taste profile that the breeding key learns from use.

    Signals per sound (a library preset by its uid, a bred child by "breed:<seed>"): the time it was played with notes,
    saved, loaded again, favourited / kept, and skipped quickly (loaded, a note or two, replaced within seconds:
    negative). Each signal changes the sound's fitness, and the sound's genome is folded into a per-engine profile:
    for every continuous sound gene a weighted mean of the liked values (positive fitness) and of the disliked ones
    (negative), in normalised units. The weights saturate (maxWeight), so the profile follows the user's current taste.

    The breeding view then:
      * pulls mutation steps towards the liked region of the child's engine (and away from the disliked one),
      * breeds twice as many candidates as it shows and keeps the best-scoring ones,
      * picks the die's random parent with weights from the score.
    "Curiosity" (0 .. 1) keeps exploring: at 1 nothing is biased (the brood is exactly the unbiased one), at 0 every
    slot and step follows the profile. Levels are never touched (the brood is still level-matched), risky genes keep
    their caps, discrete genes are never biased. Message / worker thread only; deterministic (no random numbers here).

    The profile is a local JSON file (<user data>/groovex/Ohpe/Taste/taste.json, {"format":"ohpe-taste","version":1}),
    never sent anywhere. A damaged file loads as an empty profile.
*/
namespace ohpe::breed
{
/** What the user did with a sound. */
enum class Signal : int { Played = 0, Saved, Reloaded, Favourite, Unfavourite, Kept, Skipped };

/** Fitness change of one signal (Played: `amount` = seconds played with notes). */
float fitnessDelta (Signal s, float amount = 0.0f) noexcept;

/** The per-engine pull the breeding uses (built from a profile; normalised units, one entry per schema gene). */
struct EngineBias
{
    std::vector<float> like, likeConf;     ///< liked mean (NaN = none) and confidence 0..1
    std::vector<float> dislike, dislikeConf;
    bool any = false;
};

struct TasteBias
{
    std::map<int, EngineBias> engines; ///< by EngineType
    float curiosity = 1.0f;            ///< 0 = follow the profile, 1 = no bias at all
    const EngineBias* forEngine (int engine) const noexcept;
    /** Score of a genome (higher = closer to the liked region, further from the disliked one; 0 = nothing known). */
    float score (const Schema& s, const Genome& g) const;
    /** How strongly mutation steps are pulled (0 .. 0.6). */
    float pull() const noexcept;
};

class TasteProfile
{
public:
    static constexpr float maxWeight = 24.0f;  ///< saturation of a gene's weight (older signals fade after it)
    static constexpr int maxSounds = 256;      ///< sound records kept (the least recently used go first)
    static constexpr float confidenceHalf = 3.0f; ///< weight at which a gene's confidence is 0.5

    struct Sound
    {
        float fitness = 0.0f;
        int loads = 0;
        int engine = -1;
        uint64_t used = 0; ///< recency counter
    };
    struct Stat
    {
        float w = 0.0f, mean = 0.5f; ///< weight, normalised mean
    };
    struct GeneStats
    {
        Stat like, dislike;
    };
    /** A learned trait ("why"). */
    struct Trait
    {
        int engine = -1;
        std::string gene;       ///< local ID ("analog.cutoff")
        float liked = 0.5f;     ///< liked mean, normalised
        float reference = 0.5f; ///< the gene's default, normalised
        float confidence = 0.0f;
    };

    bool learning = true;
    float curiosity = 0.35f;

    /** Records a signal for sound `key` (engine and genome of the sound as it was played). Returns the delta. */
    float record (const Schema& s, const std::string& key, const Genome& g, Signal sig, float amount = 0.0f);
    /** Signal without a genome (the sound's fitness changes, the gene profile does not). */
    float recordKeyOnly (const std::string& key, Signal sig, float amount = 0.0f);
    /** Folds `g` into its engine's profile with fitness `delta` (> 0 liked, < 0 disliked). */
    void learn (const Schema& s, const Genome& g, float delta);

    const Sound* sound (const std::string& key) const;
    float fitness (const std::string& key) const; ///< 0 if unknown
    int numSounds() const noexcept { return (int) sounds.size(); }
    bool empty() const noexcept { return sounds.empty() && engines.empty(); }
    const std::map<std::string, GeneStats>* engineStats (int engine) const;

    TasteBias bias (const Schema& s) const;
    /** The strongest learned traits, best first (confidence x distance from the default). */
    std::vector<Trait> traits (const Schema& s, int count = 5) const;

    void reset(); ///< forgets everything learned (the settings stay)

    std::string toJson() const;
    /** Reads a profile; a damaged or unknown document gives an empty profile (false). Never throws. */
    bool fromJson (const std::string& json);
    int fileVersion = 0; ///< version of the document read (0 = none)

private:
    Sound& touch (const std::string& key);
    void prune();
    std::map<std::string, Sound> sounds;
    std::map<int, std::map<std::string, GeneStats>> engines;
    uint64_t clock = 0;
};

/** True for genes the profile learns and biases (continuous, mutable, sound groups; not levels / fixed). */
bool learnable (const Gene& g) noexcept;

/** Picks an index from `scores` (taste scores of candidates): curiosity 1 = uniform, 0 = strongly towards the best.
    `u` is a uniform random number [0, 1) (the caller's Rng keeps it deterministic). */
int pickByTaste (const std::vector<float>& scores, float curiosity, float u);
} // namespace ohpe::breed
