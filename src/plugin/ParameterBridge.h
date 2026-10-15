// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "model/ParamRegistry.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace ohpe
{
/** Creates one JUCE host parameter per registry entry, in registry order (append-only IDs). */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const ParamRegistry& reg);

/** Value text with unit for a registry parameter ("1.2 kHz", "-6.0 dB", "120 ms" ...). */
juce::String formatValue (const ParamDescriptor& d, float plain);
} // namespace ohpe
