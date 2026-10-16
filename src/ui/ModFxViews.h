// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// 0.7: display views of the modulation matrix (LFO mode, view "Mod") and the FX racks (Effects mode, views
// "Rack" and "Master"): the routing list, the chain of slots, the visual feedback of the selected effect, and the
// modulation ranges the encoders and display rows show as a ring.

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace ohpe::ui::modfx
{
/** Views of the LFO mode (PageContext::lfoView) and of the Effects mode (PageContext::fxView). */
enum LfoView { LfoViewLfo = 0, LfoViewMod, numLfoViews };
enum FxView { FxViewSends = 0, FxViewRack, FxViewMaster, numFxViews };
const char* lfoViewName (int v) noexcept; ///< English, translated with i18n::tr
const char* fxViewName (int v) noexcept;

// ---- pages ------------------------------------------------------------------------------------------------------
/** Mod view: the selected routing (source, via, target, amount), its curve and the sequencer lane, macros, LFO 2,
    LFO 3, envelope 3 - four per page. */
std::vector<std::string> modParams (const Core& core, int part, int routing);
std::vector<std::string> modTitles (const Core& core, int part, int routing);
/** Rack views: the selected slot's type / mix / bypass, then the parameters of its effect. */
std::vector<std::string> rackParams (const Core& core, const std::vector<std::string>& slotPrefixes, int slot);
std::vector<std::string> rackTitles (const Core& core, const std::vector<std::string>& slotPrefixes, int slot);
/** Slot prefixes of the rack a view shows (part rack of `part`, or the master rack). */
std::vector<std::string> rackPrefixes (int fxView, int part);
/** Parameter IDs of a slot's selected effect ("p1.fx3.delay.time" ...). */
std::vector<std::string> slotEffectParams (const Core& core, const std::string& slotPrefix);

// ---- display drawing ----------------------------------------------------------------------------------------------
/** Routing list: 16 lines in two columns - source -> target, amount bar, live value. */
void paintModList (juce::Graphics& g, juce::Rectangle<float> area, Core& core, int part, int selected);
/** Index of the routing line at `p` (-1 = none). */
int modListHit (juce::Rectangle<float> area, juce::Point<float> p);
/** Trace of the selected routing's value (history -1..+1, oldest first) with its source / target names. */
void paintModTrace (juce::Graphics& g, juce::Rectangle<float> area, Core& core, int part, int routing, const std::vector<float>& history);

/** The chain of slots as blocks (type, mix, bypass, level), the selected one outlined. */
void paintRackChain (juce::Graphics& g, juce::Rectangle<float> area, Core& core, const std::vector<std::string>& prefixes, FxRack& rack,
                     int selected);
int rackChainHit (juce::Rectangle<float> area, int numSlots, juce::Point<float> p);
/** Visual feedback of the selected slot: transfer curve, EQ curve, compressor / multiband gain reduction ... */
void paintSlotVisual (juce::Graphics& g, juce::Rectangle<float> area, Core& core, const std::string& slotPrefix, InsertSlot& slot);

// ---- modulation rings ----------------------------------------------------------------------------------------------
/** What the matrix does to one parameter: the static range of its routings (normalised, around the value) and the live
    modulated position. */
struct Range
{
    bool any = false;
    float lo = 0.0f, hi = 0.0f; ///< normalised, value + negative / positive reach
    float live = 0.0f;          ///< normalised position now (value + current modulation)
    bool liveValid = false;
};
Range modulationRange (Core& core, const std::string& paramId);
} // namespace ohpe::ui::modfx
