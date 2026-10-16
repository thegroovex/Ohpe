// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "discover/Discover.h"
#include "plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace ohpe::ui
{
/**
    p21: DISCOVER - the strip at the top of the pro view (part modes):

      * four character macros BRIGHT / HARD / ALIVE / WIDE ("pN.char.*", bipolar knobs: drag up / down, Shift = fine,
        double-click = 0, right-click = MIDI learn; a violet arc shows what the matrix adds);
      * VARY: six siblings of the current sound as tiles with a small waveform sketch - click = audition (the sound is
        applied and a short note plays), hold or double-click = keep (the sound stays, a state is recorded), drag one
        tile onto another = cross the two (the child replaces the target tile and is auditioned);
      * the dice: rolls every unlocked group at once with the strength set below it (gentle .. wild);
      * the timeline: the last 20 states as dots - click one to go back to it.

    A sound being auditioned and not kept returns when VARY is pressed again, or when the strip is reset.
*/
class DiscoverStrip : public juce::Component,
                      public juce::TooltipClient
{
public:
    explicit DiscoverStrip (OhpeProcessor& p);

    /** The part and the parameters that make up its sound (the genome; unknown / unbreedable IDs are skipped). */
    void setPart (int part, const std::vector<std::string>& soundIds);
    int part() const noexcept { return partIndex; }
    /** True for genome IDs the user locked (their cards' lock icons): they never change. */
    std::function<bool (const std::string&)> isLocked;
    /** After every change of the sound by the strip (audition, keep, dice, timeline). */
    std::function<void()> onSoundChanged;
    /** The lock icon was clicked: unlock every group. */
    std::function<void()> onUnlockAll;

    void tick (float seconds);

    // ---- actions (mouse, tests, screenshots)
    void vary();
    void audition (int tile);
    void keep (int tile);
    void cross (int fromTile, int toTile);
    void roll (const std::vector<std::string>& onlyIds = {}); ///< dice: every unlocked gene (or only these IDs)
    void restore (int state);
    void recordState();          ///< pushes the current sound onto the timeline
    void revertAudition();       ///< back to the sound before the audition (when not kept)
    void setStrength (float s) { strength = juce::jlimit (0.0f, 1.0f, s); repaint(); }
    float getStrength() const noexcept { return strength; }

    int numVariants() const noexcept { return (int) variants.size(); }
    const discover::Genome* variant (int i) const noexcept { return i >= 0 && i < (int) variants.size() ? &variants[(size_t) i] : nullptr; }
    int auditioned() const noexcept { return auditionIndex; }
    int keptIndex() const noexcept { return keptTile; }
    const discover::Timeline& timeline() const noexcept { return history; }
    discover::Genome currentSound() const { return soundOf (ids); }
    /** The genome over any parameter IDs (breedable ones of this part only), read from the host parameters. */
    discover::Genome soundOf (const std::vector<std::string>& over) const;

    // layout (tests)
    juce::Rectangle<float> knobArea (int macro) const;
    juce::Rectangle<float> varyArea() const;
    juce::Rectangle<float> tileArea (int tile) const;
    juce::Rectangle<float> diceArea() const;
    juce::Rectangle<float> lockArea() const;
    juce::Rectangle<float> strengthArea() const;
    juce::Rectangle<float> dotArea (int state) const;

    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override { lastMouse = e.position; }

private:
    float unit() const noexcept { return (float) getHeight() / 10.0f; }
    std::string macroId (int m) const;
    void apply (const discover::Genome& g);
    std::vector<bool> maskFor (const discover::Genome& g, const std::vector<std::string>& onlyIds = {}) const;
    void previewNote();
    std::vector<float> sketch (const discover::Genome& g) const;
    int tileAt (juce::Point<float> p) const;

    OhpeProcessor& proc;
    int partIndex = 0;
    std::vector<std::string> ids;
    std::vector<discover::Genome> variants;
    std::vector<std::vector<float>> sketches;
    discover::Genome base;   ///< the sound the audition returns to
    discover::Genome varyFrom; ///< the sound the tiles are siblings of (their pictures show how they differ from it)
    int auditionIndex = -1, keptTile = -1;
    discover::Timeline history { 20 };
    float strength = 0.4f;
    uint32_t seed = 1;

    // mouse
    int dragMacro = -1, pressTile = -1, dropTile = -1;
    bool strengthDrag = false;
    double pressTime = 0.0;
    float dragStart = 0.0f;
    juce::Point<float> anchor, lastMouse;
    float time = 0.0f;
    std::array<float, 4> shownMacro { -9.0f, -9.0f, -9.0f, -9.0f };
    std::array<float, 4> shownMod {};
    int noteOnAt = -1;
    float noteTimer = 0.0f;
    bool holdKept = false;
};
} // namespace ohpe::ui
