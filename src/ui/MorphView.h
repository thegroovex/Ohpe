// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p16: the morph space on screen - the XY pad of the LFO mode's Morph view: four corner chips (A top left, B top
// right, C bottom left, D bottom right) in the encoder accent colours, the edges between them (solid = the two corners
// morph their parameters, dotted = they cross-fade as parallel sounds), the glided position with its weights.

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace ohpe::ui::morphview
{
/** The square pad inside the display's visual area. */
juce::Rectangle<float> padArea (juce::Rectangle<float> visualArea);
/** The chip of corner c (0..3) on the pad. */
juce::Rectangle<float> cornerChip (juce::Rectangle<float> pad, int corner);
/** Corner whose chip contains p, or -1. */
int cornerAt (juce::Rectangle<float> pad, juce::Point<float> p);
/** Pad position (x 0..1 left to right, y 0..1 bottom to top) of a point, clamped. */
juce::Point<float> positionAt (juce::Rectangle<float> pad, juce::Point<float> p);
/** Screen point of a pad position. */
juce::Point<float> pointOf (juce::Rectangle<float> pad, float x, float y);

/** Name shown on a corner chip: its given name, or "Own sound" (translated) for an empty corner. */
juce::String cornerTitle (Core& core, int part, int corner);
/** Encoder pages of the view: on / X / Y / glide, then the X / Y control. */
std::vector<std::string> morphParams (int part);
std::vector<std::string> morphTitles();

/** Paints the pad of `part` into the visual area. `time` drives the soft glow of the position. */
void paint (juce::Graphics& g, juce::Rectangle<float> visualArea, Core& core, int part, float time);
/** One line under the rows: what the pad does right now (lanes, hint). Translated. */
juce::String statusLine (Core& core, int part);
} // namespace ohpe::ui::morphview
