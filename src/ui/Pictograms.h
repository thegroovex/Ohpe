// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ohpe::ui
{
/**
    Pictogram "characters". Every engine and mode has its own little animated vector figure
    (see docs/DESIGN.md). The same drawing is used as a small static icon on the keys and as
    the large animated character on the display.
*/
enum class Picto
{
    // engines (same order as ohpe::EngineType)
    FM = 0, Wavetable, Analog, Pluck, Grain, Vocal, Cluster, Sampler,
    // modes
    Synth, Envelope, Lfo, Mixer, Effects, Sequencer, Tape, Drum,
    // functions
    Play, OctaveDown, OctaveUp, Midi,
    // 0.3
    Library, Pro, Load, Export,
    // p11: tape transport
    Stop, Record, ToStart, Loop,
    // p14: header round keys
    Random, Save,
    // p24: the isomorphic grid keyboard
    Grid,
    // p32: the settings key (header)
    Settings
};

struct PictoState
{
    float macro[4] { 0.5f, 0.5f, 0.5f, 0.5f }; ///< normalised values of the four encoders
    float energy = 0.0f;                        ///< 0..1, note activity (voices sounding)
    float time = 0.0f;                          ///< seconds, drives the animation
    bool large = false;                         ///< display character (true) or key icon (false)
};

void drawPictogram (juce::Graphics& g, juce::Rectangle<float> area, Picto id, const PictoState& state, juce::Colour ink);
} // namespace ohpe::ui
