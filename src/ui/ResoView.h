// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p19: the resonator body on screen - the "Body" view of the Effects mode (pages of the part's resonator stage) and
// the small body visual on the dark display: a vibrating string pair with its sympathetic strings, a plate with its
// moving mode pattern, or a bore with its standing wave and open / closed ends; the excitation point as a dot.

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace ohpe::ui::resoview
{
/** Body view pages of the Effects mode for one part (four per page) and their titles. */
std::vector<std::string> resoParams (int part);
std::vector<std::string> resoTitles();

/** What the visual shows (taken from a ResonatorEffect: the part stage or a rack slot). */
struct BodyState
{
    int model = 0;          ///< ResonatorEffect::ModelType
    float level = 0.0f;     ///< 0..1 output level (vibration amplitude)
    float pitchHz = 130.8f; ///< body pitch (animation speed of the standing wave, label)
    float position = 0.3f;  ///< excitation point 0..1
    float sympathetic = 0.0f;
    float size = 0.5f;
    bool on = true;
};
BodyState stateOf (const ResonatorEffect& e, const Core& core, const std::string& paramPrefix, bool on);

/** Paints the body into `area` on the display. `time` in seconds drives the motion, `ink` is the accent. */
void paintBody (juce::Graphics& g, juce::Rectangle<float> area, const BodyState& s, float time, juce::Colour ink);

/** The status line under the rows: model, tuning, pitch / or how to switch it on. */
juce::String statusLine (const Core& core, int part);
} // namespace ohpe::ui::resoview
