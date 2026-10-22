// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "breed/TasteProfile.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>
#include <string>

namespace ohpe
{
class OhpeProcessor;

/**
    p69: the learning breed's local taste profile (breed::TasteProfile) and the signals that feed it - message thread only.

    tick() runs from the processor's timer: it follows the sound in the edit part (the library entry loaded last, or
    a bred child the breeding view auditions), counts the seconds it is played with notes and, when the next sound
    replaces it, records "played" (3 s or more) or "skipped" (notes played, replaced within 8 s, under 3 s of play).
    A library entry loaded again counts as "reloaded"; saving and favouriting come from the preset manager, keeping a
    child (heart) from the breeding view. The profile is written to <user data>/groovex/Ohpe/Taste/taste.json at most
    every few seconds and when the plug-in closes; nothing leaves the computer.
*/
class TasteStore
{
public:
    explicit TasteStore (OhpeProcessor& p);
    ~TasteStore();

    /** Tests / screenshot tool: false = the profile lives in memory only (no file read or written) unless a test
        folder is set. */
    static inline std::atomic<bool> filesEnabled { true };

    juce::File file() const;
    void setFolderForTests (const juce::File& f);

    /** Timer (20 Hz): follows the edit part's sound and its play time; writes the profile when changed. */
    void tick (double nowSeconds);
    /** A sound became the edit part's sound (key: library uid or "breed:<seed>"; `genome` empty = read it later). */
    void soundChanged (const std::string& key, const breed::Genome& genome = {});
    /** A signal for a sound (genome empty = the edit part's sound now when `key` is the current sound). */
    void signal (const std::string& key, breed::Signal s, const breed::Genome& genome = {});

    bool learning() const noexcept { return profile.learning; }
    void setLearning (bool on);
    float curiosity() const noexcept { return profile.curiosity; }
    void setCuriosity (float c);
    void reset();
    void save();
    void load();

    const breed::TasteProfile& taste() const noexcept { return profile; }
    const breed::Schema& schema();
    /** The bias for the next brood (nullptr when learning is off or nothing is learned yet). */
    std::shared_ptr<const breed::TasteBias> bias();
    const std::string& currentKey() const noexcept { return key; }

private:
    breed::Genome editGenome();
    void commit (double now);
    OhpeProcessor& proc;
    breed::TasteProfile profile;
    std::unique_ptr<breed::Schema> sch;
    juce::File testFolder;
    std::string key, lastUid;
    breed::Genome genome;
    double loadedAt = 0.0, lastTick = -1.0, played = 0.0, lastSnapshot = 0.0, lastSave = 0.0;
    bool notesSeen = false, pendingReload = false, dirty = false, uidKnown = false;
};
} // namespace ohpe
