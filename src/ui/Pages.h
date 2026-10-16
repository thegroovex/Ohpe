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

/** 0.3: the sequencer mode has one view per sequencer, chosen with tabs on the display. */
enum class SeqView { Step = 0, Euclid, Arp, Orbit, Sync, numViews };
constexpr int numSeqViews = (int) SeqView::numViews;
const char* seqViewName (SeqView v) noexcept; ///< English, translated with i18n::tr

struct ModeInfo
{
    const char* nameEn; ///< translated with i18n::tr
    Picto picto;
    bool available;     ///< false = later version (key dimmed)
};

const ModeInfo& modeInfo (Mode m) noexcept;

/** What the pages of a mode refer to (0.3). */
struct PageContext
{
    int part = 0;         ///< edited part, 0..3
    int mixerChannel = 0; ///< 0..7
    SeqView seqView = SeqView::Step;
    int tapeTrack = 0;    ///< 0..7
    int drumPad = 0;      ///< 0..23
    int lfoView = 0;      ///< 0.7: LFO mode view (modfx::LfoView: LFO, Mod)
    int fxView = 0;       ///< 0.7: Effects mode view (modfx::FxView: Sends, Rack, Master)
    int modRouting = 0;   ///< 0.7: selected routing of the matrix, 0..15
    int fxSlot = 0;       ///< 0.7: selected slot of the shown FX rack
};

/** Parameter IDs a mode puts on the encoders, four per page. Empty strings leave an encoder free. */
std::vector<std::string> modeParams (Mode m, Core& core, const PageContext& ctx);
/** 0.1 signature: part 1, mixer channel `mixerChannel`. */
std::vector<std::string> modeParams (Mode m, Core& core, int mixerChannel);

/** English title of each page of modeParams() (same count as pages), translated with i18n::tr. */
std::vector<std::string> pageTitles (Mode m, Core& core, const PageContext& ctx);

/**
    Groups of parameters for the pro view: every parameter of the mode at once (for the tape all
    eight tracks, for the drum kit all 24 pads). Titles are English (i18n::tr) and may carry a
    number suffix ("Track 3" = tr ("Track") + " 3").
*/
struct ParamGroup
{
    std::string title;
    int number = 0; ///< appended to the translated title when > 0
    std::vector<std::string> ids;
};
std::vector<ParamGroup> proGroups (Mode m, Core& core, const PageContext& ctx);

/** The engine selected for a part: the parameter value (follows a click at once), or the engine
    the part plays when the selected one is not available. */
EngineType selectedEngine (Core& core, int part = 0);

inline int numPages (const std::vector<std::string>& ids) { return juce::jmax (1, ((int) ids.size() + 3) / 4); }
} // namespace ohpe::ui
