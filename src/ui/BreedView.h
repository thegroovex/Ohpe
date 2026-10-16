// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "breed/SoundGenome.h"
#include "plugin/PluginProcessor.h"

#include <functional>
#include <map>
#include <memory>

namespace ohpe::ui
{
/**
    p18: the breeding view ("Zucht") - an overlay on the plate like the library, opened by holding RANDOM (or its
    right click, the library's "Breed" button, MIDI "lib.breed").

    Top: generation with undo / redo, Keep / Cancel. Then the two parent chips (the current sound, a library preset,
    a child; the die picks a random library sound for B), the mutation amount, BREED (a new brood of six from the
    parents) and AGAIN (the next generation from the kept children). A row of group chips steers the crossover
    (Engine, Filter, Envelopes, FX, Mod: Auto / A / B / Mix). Six child tiles show a waveform thumbnail tinted by the
    child's brightness, what it inherited from which parent, its level; a click loads it into the edit part and plays
    a short note (preview), the heart keeps it for the next generation, SAVE stores it in the library as a favourite.

    Thumbnails and level matching run on a worker thread (breed::Audition, a private core), so the brood plays at its
    parents' loudness. The session (parents, generations, kept children) lives in the plug-in state (section "breed").
*/
class BreedView : public juce::Component, private juce::Timer
{
public:
    explicit BreedView (OhpeProcessor& p);
    ~BreedView() override;

    /** Opens the view (starts the preview: Cancel restores the sound from before). `libraryEntry` >= 0 makes that
        library preset parent B (parent A = the current sound) and breeds at once. */
    void open (int libraryEntry = -1);
    void close (bool keep);
    std::function<void()> onClosed;

    void paint (juce::Graphics& g) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;

    // ---- actions (keys, MIDI remote, tests, screenshot tool) --------------------------------------------------------
    /** Parent `which` (0 = A, 1 = B): -1 = the current sound (as loaded before the view opened), >= 0 a library entry. */
    void setParentFromLibrary (int which, int entry);
    void setParentCurrent (int which);
    void setParentFromChild (int which, int child);
    /** RANDOM in the view: a random part preset of the library becomes parent B. */
    void randomParent (int which = 1);
    void breedNew();   ///< BREED: a new brood from the two parents
    void breedAgain(); ///< AGAIN: the next generation from the kept children (or the parents when none is kept)
    void undo();
    void redo();
    /** Loads child `i` into the edit part (and plays the preview note when enabled). */
    void audition (int i, bool playNote = true);
    void auditionNext();
    void toggleKept (int i);
    /** Saves child `i` as a user part preset, marked as a favourite. Returns the library entry (-1 = failed). */
    int saveChild (int i);
    void setAmount (float a);
    void setGroupChoice (int group, breed::Choice c);
    /** Tests / screenshots: a fixed seed stream instead of the clock. */
    void setSeedBase (juce::int64 s) { seedBase = s; seedCounter = 0; }
    /** Waits until the worker has thumbnails / level matches for every child of the current generation. */
    bool waitForSketches (int timeoutMs);

    breed::Session& session() noexcept { return *ses; }
    int selectedChild() const noexcept { return selected; }
    bool hasSketch (int i) const;
    breed::Audition::Sketch sketch (int i) const;
    const juce::String& statusMessage() const noexcept { return status; }

private:
    class Worker;
    struct Hit
    {
        juce::Rectangle<float> area;
        std::function<void()> action;
        bool rightClick = false;
        std::function<void()> secondary;
    };
    void timerCallback() override;
    float unit() const;
    juce::Rectangle<float> tileArea (int i) const;
    juce::Rectangle<float> amountArea() const;
    void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent, std::function<void()> action,
               std::function<void()> secondary = {});
    void paintTile (juce::Graphics& g, int i, juce::Rectangle<float> r);
    void showParentMenu (int which);
    void showChildMenu (int i);
    void generationChanged();     ///< new / undone generation: queue thumbnails, store the session
    void store();                 ///< session -> state section "breed"
    uint64_t nextSeed();
    breed::Genome currentGenome() const;
    preset::Preset childPreset (int i) const;
    juce::String parentLabel (int which) const;

    OhpeProcessor& proc;
    std::unique_ptr<breed::Session> ses;
    std::unique_ptr<Worker> worker;
    std::map<uint64_t, breed::Audition::Sketch> sketches; ///< by child seed
    std::vector<Hit> hits;
    juce::String status;
    int selected = -1;
    bool playNotes = true;
    juce::int64 seedBase = 0;
    juce::int64 seedCounter = 0;
    breed::Genome before;          ///< the edit part's sound when the view opened ("current sound")
    juce::String beforeName;
    int noteChannel = 1, noteOn = -1;
    int noteTicks = 0;
    bool draggingAmount = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreedView)
};
} // namespace ohpe::ui
