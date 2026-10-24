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

/** p86: what the status line says about the input (the first that applies, in this order). */
enum class InputState
{
    off,        ///< Voice Control is off
    micBlocked, ///< the system refuses the microphone (macOS privacy settings): every input is silent
    micAsk,     ///< the system has not asked for the microphone yet
    noDevice,   ///< no input device / host input bus delivers channels
    noSignal,   ///< an input is open but completely silent (level, cable, muted device)
    signal      ///< the input carries a signal
};
InputState inputState (const Core& core);
/** p86: the extra soft key of the Voice view: 0 = none, 1 = open the privacy settings, 2 = ask for the microphone. */
int micKey (const Core& core);
} // namespace ohpe::ui::voiceview
