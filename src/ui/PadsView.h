// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p25: the performance pads on screen - the "Pads" view of the Effects mode: four pads on the dark display (stutter,
// tape stop, filter sweep, reverb freeze), each with a small picture of what it does. Press and hold a pad to throw
// it, shift-click latches it; a waiting pad (quantised to the grid) blinks violet, an engaged one lights green, a
// ringing freeze tail stays dim green. Right-click: MIDI learn of the pad.

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace ohpe::ui::padsview
{
/** Encoder pages of the view (four per page) and their titles. */
std::vector<std::string> padsParams();
std::vector<std::string> padsTitles();

/** Pad 0..3 inside the display's visual area (2 x 2). */
juce::Rectangle<float> padArea (juce::Rectangle<float> visualArea, int pad);
/** Pad under p, or -1. */
int padAt (juce::Rectangle<float> visualArea, juce::Point<float> p);
/** Name of a pad ("Stutter", "Tape Stop", "Sweep", "Freeze"; untranslated key). */
const char* padName (int pad) noexcept;

/** Paints the four pads. `time` drives the blinking of a waiting pad and the pictures. */
void paint (juce::Graphics& g, juce::Rectangle<float> visualArea, Core& core, float time);
/** One line under the rows: what the pads do right now. Translated. */
juce::String statusLine (Core& core);
} // namespace ohpe::ui::padsview
