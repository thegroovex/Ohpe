// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ohpe::ui
{
struct ParamGroup;
enum class Mode;

/**
    p21: pro view 2.0 - the graphic tiles on top of the group cards ("see -> grab -> hear"). Each tile draws what its
    parameters do (an envelope curve, a filter response, an LFO wave, the FM operator graph, the wavetable frames, a
    waveform with its grain cloud, the 3D stage ...) and is edited directly in the picture. Colours: yellow = what you
    edit, violet = modulation (the range a routing moves a parameter, the live offset), green = playing.

    Every tile is also a drop target for modulation sources: dropping a source chip (LFO, envelope ...) onto a tile
    creates a matrix routing to the parameter under the drop point (or the tile's main parameter).

    Animation: the editor's 30 Hz timer calls tick(); a tile repaints only when something it shows has changed
    (values, modulation, playhead) - nothing is drawn while the sound and the controls rest.
*/
namespace pro
{
/** One sentence on what a parameter does (EN / DE by the current language). */
juce::String explanation (const std::string& paramId);
/** "Name  value - explanation" for hover texts. */
juce::String hoverText (OhpeProcessor& proc, const std::string& paramId);
/** Matrix target index that moves `paramId` of `part` (-1 = none). */
int destForParam (OhpeProcessor& proc, int part, const std::string& paramId);
/** Range a part's matrix routings move a parameter (normalised; lo <= 0 <= hi) - the violet shadow. */
juce::Range<float> modRange (OhpeProcessor& proc, int part, const std::string& paramId);
/** Creates a routing source -> paramId in the first free slot of the part's matrix (amount 35 %). Returns the slot
    (0-based) or -1 when no slot is free / the parameter is no target. */
int addRouting (OhpeProcessor& proc, int part, int source, const std::string& paramId, float amount = 0.35f);
/** Drag description of a modulation source chip ("modsrc:<index>"). */
juce::String sourceDragDescription (int source);
int sourceFromDragDescription (const juce::var& description); ///< -1 when it is no source chip

class Tile : public juce::Component,
             public juce::DragAndDropTarget,
             public juce::TooltipClient
{
public:
    Tile (OhpeProcessor& p, int part, std::vector<std::string> ids);
    ~Tile() override = default;

    /** Advances the animation; repaints when what the tile shows changed. Returns true when it repainted. */
    bool tick (float seconds);
    const std::vector<std::string>& paramIds() const noexcept { return ids; }
    /** Called after an edit gesture in the picture ended (the pro view records an undo state). */
    std::function<void()> onEdited;

    juce::String getTooltip() override;
    bool isInterestedInDragSource (const SourceDetails& d) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override;
    void mouseMove (const juce::MouseEvent& e) override { lastMouse = e.position; }
    void paint (juce::Graphics& g) override;

    /** The parameter the picture edits at `p` (hover text, drop target); default: the first one. */
    virtual std::string paramAt (juce::Point<float> p) const;
    /** True while the tile moves by itself (playhead, LFO, grains) - it then repaints every tick. */
    virtual bool animating() const { return false; }

protected:
    virtual void paintTile (juce::Graphics& g, juce::Rectangle<float> area) = 0;
    /** What the picture shows, folded into one number (values, modulation ...); a change repaints. */
    virtual uint64_t stateHash() const;

    float norm (const std::string& id) const;        ///< current normalised value (0 when missing)
    float normAt (size_t i) const { return i < ids.size() ? norm (ids[i]) : 0.0f; }
    float mod (const std::string& id) const;         ///< live matrix offset (normalised)
    juce::Range<float> range (const std::string& id) const { return modRange (proc, part, id); }
    bool has (const std::string& id) const;
    juce::String choiceText (const std::string& id) const; ///< English choice label ("Sine")
    juce::String valueText (const std::string& id) const;
    void begin (const std::string& id);
    void set (const std::string& id, float norm);
    void end (const std::string& id);
    void endAll();                                    ///< ends every open gesture, then onEdited
    void nudge (const std::string& id, float delta);  ///< one gesture, relative (wheel, steps)
    void reset (const std::string& id);
    void stepChoice (const std::string& id, int delta);
    /** Shared drawing: violet range bar along a line (a..b in pixels) for a modulated parameter. */
    void drawModBar (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, const std::string& id) const;
    static uint64_t mix (uint64_t h, float v) noexcept;
    float unit() const noexcept { return juce::jmax (1.0f, (float) getHeight() / 18.0f); }

    OhpeProcessor& proc;
    int part;
    std::vector<std::string> ids;
    float time = 0.0f;
    juce::Point<float> lastMouse;
    bool dropHover = false;
    std::vector<std::string> open; ///< parameters with an open gesture

private:
    uint64_t lastHash = 0, rangeHash = 0;
    int ticks = 0;
    bool first = true;
};

/** Builds the tile for a pro view group (nullptr = the group has no picture). */
std::unique_ptr<Tile> makeTile (OhpeProcessor& proc, Mode mode, int part, const ParamGroup& group);

/** p21: a source chip for the modulation drag (LFO 1, LFO 2, Mod Env ...) - drag it onto a tile. */
class SourceChip : public juce::Component, public juce::TooltipClient
{
public:
    SourceChip (OhpeProcessor& p, int source);
    void setPart (int partIndex) { part = partIndex; repaint(); }
    int source() const noexcept { return src; }
    juce::String getTooltip() override;
    void paint (juce::Graphics& g) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    OhpeProcessor& proc;
    int src, part = 0;
};
} // namespace pro
} // namespace ohpe::ui
