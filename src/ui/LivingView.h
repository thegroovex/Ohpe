// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p26: the living display - the sound as a 3D object in the display's visual area. It breathes with the amp envelope,
// bulges with the LFOs, ripples with the mod envelope and the modulation matrix, stretches with the stereo width and
// splits when the channels drift out of phase (living::Shape in the core). Light on the CPU: the display repaints
// only the object's own rectangle, and only while it moves.

#include "OhpeCore.h"
#include "Visuals.h"
#include "visual/LivingShape.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace ohpe::ui::living
{
/** Samples what the object listens to: the part's envelopes, LFOs (weighted by how much they are routed) and
    matrix depth; the stereo picture from the display's scope visuals (the part's or the master's signal). */
ohpe::living::Inputs gather (Core& core, int part, const DisplayVisuals& visuals);

/** Where the object (with its floor shadow and glow) can be inside `area` right now - the region to repaint. */
juce::Rectangle<float> objectBounds (juce::Rectangle<float> area, const ohpe::living::Shape& shape);
/** The three readouts under the object (envelope, modulation, width). */
juce::Rectangle<float> readoutArea (juce::Rectangle<float> area);

/** Paints the object into `area`: caption (view + source), the mesh in the part's colour, readouts. Translated. */
void paint (juce::Graphics& g, juce::Rectangle<float> area, const ohpe::living::Shape& shape, juce::Colour accent,
            const juce::String& source, juce::Point<float> parallax);
} // namespace ohpe::ui::living
