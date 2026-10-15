// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "Pictograms.h"

#include <string>
#include <vector>

namespace ohpe::ui
{
/** The mode keys. Each mode shows a list of parameters on the four encoders, four per page. */
enum class Mode { Synth = 0, Envelope, Lfo, Mixer, Effects, Sequencer, Tape, Drum, numModes };
constexpr int numModes = (int) Mode::numModes;

struct ModeInfo
{
    const char* nameEn; ///< translated with i18n::tr
    Picto picto;
    bool available;     ///< false = later version (key dimmed)
};

const ModeInfo& modeInfo (Mode m) noexcept;

/**
    Parameter IDs a mode puts on the encoders. Empty strings leave an encoder free.
    `mixerChannel` is the selected mixer channel (0..7).
*/
std::vector<std::string> modeParams (Mode m, Core& core, int mixerChannel);

/** The engine selected for part 1: the parameter value (follows a click at once), or the engine
    the part plays when the selected one is not available. */
EngineType selectedEngine (Core& core);

inline int numPages (const std::vector<std::string>& ids) { return juce::jmax (1, ((int) ids.size() + 3) / 4); }
} // namespace ohpe::ui
