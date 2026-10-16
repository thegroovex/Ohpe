// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p20: the user wavetable on screen - on the Synth page of the Wavetable engine a stack of the table's frames (the
// frame at the current position lit), on the Vocal page the five formant tracks; while an import runs a progress bar.

#include "plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace ohpe::ui::usertableview
{
/** True when the display shows the user material of `part` instead of the character (user table on, or an import running). */
bool wants (OhpeProcessor& proc, int part, EngineType engine);
/** Paints the frame stack / formant tracks into `area`. `position` 0..1 (negative = the part's position / vowel parameter). */
void paint (juce::Graphics& g, juce::Rectangle<float> area, OhpeProcessor& proc, int part, EngineType engine, float position, float time);
/** One line for the soft key row: table name and size, the import progress, or the hint. Translated. */
juce::String statusLine (OhpeProcessor& proc, int part, EngineType engine);
} // namespace ohpe::ui::usertableview
