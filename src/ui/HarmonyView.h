// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p24: the harmony brain on screen - the LFO mode's Harmony view: the remembered key and chord, the pitch-class
// memory as twelve bars (key notes bright, chord tones violet, held notes green, the key's root marked yellow) and
// the chord history as chips (newest first).

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace ohpe::ui::harmonyview
{
/** Encoder page of the view: the edit part's follow mode, memory, hold, grid layout. */
std::vector<std::string> harmonyParams (int part);
std::vector<std::string> harmonyTitles();

/** Name of a pitch class in the current language (German: H for B, sharps as "is"). */
juce::String noteName (int pitchClass);
/** Chord symbol ("Am7", German "H7"), "–" for none. */
juce::String chordName (const harmony::Chord& c);
/** Key ("E Minor" / "E Moll"). */
juce::String keyName (int root, int scale);

/** Paints the brain into the display's visual area. */
void paint (juce::Graphics& g, juce::Rectangle<float> visualArea, Core& core, int part, float time);
/** One line under the rows: what the brain does for the edit part. Translated. */
juce::String statusLine (Core& core, int part);
} // namespace ohpe::ui::harmonyview
