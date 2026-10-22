// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p68: Voice Control on screen - the LFO mode's Voice view: the vowel map (A E I O U, the live vowel as a dot), the input
// meter with the gate line and five small bars (level, brightness, noisiness, pitch, onset). Encoder pages: on, gain,
// gate, smoothing / pitch tracking, monitor. Soft keys: On, "Vowel -> Filter + Morph" (quick map for the edit part).

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace ohpe::ui::voiceview
{
std::vector<std::string> voiceParams();
std::vector<std::string> voiceTitles();

/** Paints the view into the display's visual area. */
void paint (juce::Graphics& g, juce::Rectangle<float> visualArea, const Core& core);
/** One line under the view: what Voice Control hears right now. Translated. */
juce::String statusLine (const Core& core);
/** Changes that matter for repainting / the soft keys (cheap, message thread). */
int signature (const Core& core);
} // namespace ohpe::ui::voiceview
