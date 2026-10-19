// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p55: the graphical FX slot editor of the Effects mode (views Rack and Master). One slot at a time: its effect as a
// picture you edit directly (EQ band points on the response curve, the compressor's threshold and ratio on its transfer
// curve, rate and depth on a modulation wave, time and feedback as echo taps ...) next to every parameter of the slot as
// a labelled bar. Every edit is a normal host parameter change (gesture, automation, MIDI learn, undo of the host).
// The encoders stay live below the editor: a click on a point puts its parameters on them (the "focus" page).

#include "ProTiles.h"
#include "ProView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <string>
#include <vector>

namespace ohpe::ui
{
namespace fxgraph
{
/** How one coordinate of a point maps to a parameter. */
struct Axis
{
    enum Kind { None = 0, Norm, Lin, Log };
    Kind kind = None;
    float lo = 0.0f, hi = 0.0f; ///< value range (Lin / Log); lo >= hi = the parameter's own range
    float f0 = 0.0f, f1 = 1.0f; ///< where lo..hi sits in the region (fraction; 0 = left / bottom); f0 > f1 inverts
    float fixed = 0.5f;         ///< the fraction when kind == None
};

/** Points whose position follows from more than one parameter. */
enum class Special { None, CompThreshold, CompRatio, MultibandBand };

/** One point of the picture you can grab. */
struct Handle
{
    std::string x, y, alt; ///< full parameter IDs ("" = none); alt = wheel / Alt-drag (Q, knee, shape ...)
    std::string aux;       ///< Special: the other parameter (compressor ratio: the threshold)
    Axis ax, ay;
    std::string nameEn, nameDe;
    Special special = Special::None;
    int band = 0;   ///< MultibandBand: 0 low, 1 mid, 2 high
    int region = 0; ///< the part of the picture it lives in (Layout::regions)
};

/** The picture of a slot: its regions (fractions of the plot) and points. */
struct Layout
{
    std::vector<juce::Rectangle<float>> regions;
    std::vector<Handle> handles;
};

/** The block prefix of a slot's effect ("p1.fx3.eq."; empty for an empty slot). */
std::string effectBlock (const Core& core, const std::string& slotPrefix);
/** The picture of the slot's current type (no points for empty slots, Space 3D, Resonator). */
Layout layoutFor (OhpeProcessor& proc, const std::string& slotPrefix);
/** Fraction of a value on an axis / value of a fraction (tests). */
float fractionOf (OhpeProcessor& proc, const Axis& a, const std::string& id);
float normalisedFor (OhpeProcessor& proc, const Axis& a, const std::string& id, float fraction);
} // namespace fxgraph

/** The picture of one slot, edited in place. */
class FxGraphTile final : public pro::Tile
{
public:
    FxGraphTile (OhpeProcessor& p, int part, int fxView, int slot, std::string slotPrefix);

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    std::string paramAt (juce::Point<float> p) const override;
    bool animating() const override;

    const fxgraph::Layout& layout() const noexcept { return lay; }
    int numHandles() const noexcept { return (int) lay.handles.size(); }
    int selectedHandle() const noexcept { return selected; }
    void selectHandle (int h, bool notify = true);
    /** Re-reads the points (a rate point becomes a sync point when the effect follows the tempo). */
    void refreshLayout();
    bool isDragging() const noexcept { return dragging >= 0; }
    /** Centre of a point in tile coordinates (tests, screenshots). */
    juce::Point<float> handlePosition (int h) const;
    juce::Rectangle<float> plotArea() const;
    juce::Rectangle<float> regionArea (int r) const;
    /** Moves a point to a position in tile coordinates as a drag would (tests). */
    void dragHandleTo (int h, juce::Point<float> p);
    /** The parameters a point edits (x, y, alt; the encoders' focus page). */
    std::vector<std::string> handleParams (int h) const;
    juce::String handleName (int h) const;

    std::function<void (int handle)> onHandleSelected;

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override;
    uint64_t stateHash() const override;

private:
    juce::Point<float> fractionsOf (const fxgraph::Handle& h) const;
    void apply (const fxgraph::Handle& h, juce::Point<float> f);
    int hit (juce::Point<float> p) const;
    float u() const noexcept;
    float plain (const std::string& id) const;
    InsertSlot* slotObject() const;
    void paintBackground (juce::Graphics& g, juce::Rectangle<float> plot);
    juce::String handleValues (int h) const;

    int fxView = 1, slotIndex = 0;
    std::string prefix, block, type;
    fxgraph::Layout lay;
    int selected = -1, hover = -1, dragging = -1;
    juce::Point<float> dragStart, fracStart;
    float altStart = 0.0f;
};

/** The editor panel: header with the slots, the picture, every parameter of the slot. */
class FxEditor : public juce::Component
{
public:
    explicit FxEditor (OhpeProcessor& p);

    /** Shows a slot (rebuilds when the view, part, slot or the slot's type changed). */
    void show (int fxView, int part, int slot);
    /** The parameters on the four encoders now (their colours mark the bars). */
    void setEncoderIds (const std::vector<juce::String>& ids);
    void refresh();            ///< bars + type change (30 Hz)
    void tick (float seconds); ///< the picture's animation (also headless)

    std::function<void()> onClose;
    std::function<void (int slot)> onSlotSelected;
    /** A point was selected: its parameters go to the encoders (empty = focus off). */
    std::function<void (const std::vector<std::string>& ids, const juce::String& title)> onFocus;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    FxGraphTile* graph() noexcept { return tile.get(); }
    int numBars() const noexcept { return (int) bars.size(); }
    ParamBar& bar (int i) { return *bars[(size_t) i]; }
    int shownSlot() const noexcept { return slot; }
    int shownView() const noexcept { return fxView; }
    juce::Rectangle<float> slotTabArea (int i) const;
    juce::Rectangle<float> closeArea() const;
    int numSlots() const noexcept { return (int) prefixes.size(); }

private:
    void rebuild();
    float unit() const;
    juce::Rectangle<float> bodyArea() const;
    juce::Rectangle<float> hintArea() const;

    OhpeProcessor& proc;
    int fxView = -1, part = -1, slot = -1, type = -1;
    std::vector<std::string> prefixes;
    std::unique_ptr<FxGraphTile> tile;
    std::vector<std::unique_ptr<ParamBar>> bars;
    std::vector<juce::String> encoderIds;
};
} // namespace ohpe::ui
