// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p22: the motion recorder on screen - the LFO mode's Motion view: the edit part's four lanes as rows (the recorded
// movement in the encoder accent colours, sixteenth grid over the loop length, the playhead green while playing,
// red while recording; a lane being touched lights red).

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace ohpe::ui::motionview
{
/** Encoder page of the view: play, record, length, depth. */
std::vector<std::string> motionParams (int part);
std::vector<std::string> motionTitles();

/** Row of lane `lane` (0..3) inside the display's visual area. */
juce::Rectangle<float> laneArea (juce::Rectangle<float> visualArea, int lane);
/** Lane whose row contains p, or -1. */
int laneAt (juce::Rectangle<float> visualArea, juce::Point<float> p);
/** Name of a lane: its parameter (compact, translated), or "Empty" (translated). */
juce::String laneTitle (Core& core, int part, int lane);

/** Paints the lanes of `part` into the visual area. `time` drives the touch glow. */
void paint (juce::Graphics& g, juce::Rectangle<float> visualArea, Core& core, int part, float time);
/** One line under the rows: what the recorder does right now. Translated. */
juce::String statusLine (Core& core, int part);
} // namespace ohpe::ui::motionview
