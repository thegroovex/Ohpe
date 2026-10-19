// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "DiscoverStrip.h"
#include "Pages.h"
#include "ProTiles.h"
#include "Widgets.h"

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace ohpe::ui
{
/**
    One parameter as a horizontal bar (pro view): name, value and a bar you drag (right / up =
    more; Shift, Cmd or Ctrl = fine), click on the bar to jump, double-click resets, right-click
    opens MIDI learn. p21: the hover text names the value and says in one sentence what it does.
*/
class ParamBar : public juce::Component,
                 public juce::TooltipClient
{
public:
    ParamBar (OhpeProcessor& p, juce::String paramId, juce::Colour accent);

    const juce::String& paramId() const noexcept { return id; }
    void refresh();  ///< repaints if the value changed

    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    juce::Rectangle<float> barBounds() const;
    std::function<void()> onEdited; ///< p21: after a gesture (undo state)
    /** p55: a shorter name than the parameter's own (the FX editor drops the slot and effect prefix); empty = own name. */
    void setLabel (const juce::String& text) { label = text; repaint(); }

private:
    OhpeProcessor& proc;
    juce::String id;
    juce::String label; ///< p55
    juce::Colour accent;
    juce::RangedAudioParameter* param = nullptr;
    int index = -1;
    float shown = -1.0f;
    bool dragging = false;
    BarDrag drag;
};

/**
    The pro view. 0.3: every parameter of the current mode at once, grouped, in as many columns as fit.
    p21 (pro view 2.0, "see -> grab -> hear"):

      * every group card that has a picture shows it first (ProTiles: envelope curves, filter response, LFO wave,
        FM operator graph, wavetable frames, waveform + grains, tuning ring + meter, FX chain, 3D head) - edited in
        the picture; the bars stay below as the precise row (collapsible per card: the chevron);
      * DISCOVER at the top (part modes): the character macros, VARY, the dice, the timeline (DiscoverStrip), and
        the modulation sources as chips - drag one onto a picture to route it in the matrix;
      * every card of the part's sound has a dice (roll only this group) and a lock (VARY / dice leave it alone);
      * SIMPLE | ALL: Simple shows the pictures and Discover only, All adds every bar;
      * responsive: one to four columns; 30 Hz animation, a tile repaints only when what it shows changed.
*/
class ProView : public juce::Component,
                public juce::DragAndDropContainer
{
public:
    explicit ProView (OhpeProcessor& p);
    ~ProView() override;

    void setContent (Mode mode, const std::vector<ParamGroup>& groups, const juce::String& subtitle, int part = 0);
    void refresh();              ///< bars + one animation step (30 Hz)
    void tick (float seconds);   ///< advances the tiles' animation (also headless)

    std::function<void (Mode)> onModeSelected;
    std::function<void()> onClose;
    std::function<void()> onSettingsChanged; ///< p21: Simple / All changed (the editor saves it)

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    int numBars() const noexcept { return (int) bars.size(); }
    ParamBar& bar (int i) { return *bars[(size_t) i]; }
    Mode currentMode() const noexcept { return mode; }
    int numColumns() const noexcept { return columns; }

    // ---- p21
    void setSimple (bool simple, bool notify = true);
    bool isSimple() const noexcept { return simple; }
    bool showsDiscover() const noexcept { return discoverShown; }
    DiscoverStrip& discoverStrip() noexcept { return strip; }
    int numTiles() const noexcept;
    pro::Tile* tile (int card);                 ///< nullptr when the card has no picture
    int numCards() const noexcept { return (int) cards.size(); }
    const ParamGroup& cardGroup (int card) const { return cards[(size_t) card].group; }
    int cardIndex (const std::string& title, int number = 0) const; ///< -1 = not shown
    void setCollapsed (int card, bool collapsed);
    bool isCollapsed (int card) const;
    void setLocked (int card, bool locked);
    bool isLocked (int card) const;
    void rollCard (int card);                   ///< the card's dice
    juce::Rectangle<float> simpleToggleArea() const;
    juce::Rectangle<int> cardBounds (int card) const; ///< in content coordinates
    juce::Viewport& scroller() noexcept { return viewport; }
    /** The parameters that make up the part's sound (the Discover genome): every breedable ID of the Synth groups. */
    std::vector<std::string> soundIds() const;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (ProView& o) : owner (o) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        ProView& owner;
    };

    struct Card
    {
        ParamGroup group;
        std::unique_ptr<pro::Tile> tile;
        size_t firstBar = 0, numBars = 0;
        juce::Rectangle<int> box;
        bool partSound = false; ///< dice + lock
        bool visible = true;
    };

    juce::Rectangle<float> tabArea (int m) const;
    juce::Rectangle<float> closeArea() const;
    juce::Rectangle<float> chevronArea (const Card& c) const;
    juce::Rectangle<float> diceArea (const Card& c) const;
    juce::Rectangle<float> lockArea (const Card& c) const;
    float unit() const;
    void layoutContent();
    bool partMode() const noexcept;
    std::string cardKey (const Card& c) const;
    bool anyTile() const noexcept;
    void recordState();

    OhpeProcessor& proc;
    Mode mode = Mode::Synth;
    int part = 0;
    uint64_t routingSig = 0; ///< the part's matrix slots last seen (repaint the bars when it changes)
    EngineType engine = EngineType::Analog;
    juce::String subtitle;
    std::vector<ParamGroup> groups;
    std::vector<Card> cards;
    std::vector<std::unique_ptr<ParamBar>> bars;
    juce::Viewport viewport;
    Content content { *this };
    DiscoverStrip strip;
    std::vector<std::unique_ptr<pro::SourceChip>> chips;
    std::set<std::string> collapsed, locked;
    int columns = 1;
    bool simple = false, discoverShown = false;
};
} // namespace ohpe::ui
