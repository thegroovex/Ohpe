// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"
#include "Pictograms.h"

#include <array>
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

// ---------------------------------------------------------------- p14: key LEDs
/** What one key LED shows. Colours (see docs/DESIGN.md): Focus = sun yellow, Active = ultraviolet,
    Record = signal red, Off = the dark unlit lens. */
enum class Led { Off = 0, Focus, Active, Record };

/** The state the column LEDs are computed from (read from the core by ledInputs()). */
struct LedInputs
{
    std::array<int, Core::numParts> partEngine {}; ///< EngineType of every part
    int editPart = 0;                              ///< 0..3
    Mode page = Mode::Synth;                       ///< the mode shown on the display
    bool sequencerRunning = false;                 ///< seq.run
    bool drumSeqRunning = false;                   ///< seq.run and a running sequencer plays the drum kit
    bool tapeRecording = false, tapePlaying = false;
    bool effectsActive = false;                    ///< any rack slot of any part or the master: a type set, not bypassed
    bool modulationActive = false;                 ///< any part: LFO 1 with depth > 0 or a matrix routing (source, target, amount != 0)
};

struct KeyLeds
{
    std::array<Led, numEngineTypes> engine {};        ///< Focus = the edit part's engine, Active = another part's
    std::array<unsigned, numEngineTypes> engineParts {}; ///< bit p set = part p + 1 plays this engine
    /** p14 rev 3: the engine key's light bar has one segment per part: Focus = the edit part plays this
        engine, Active = another part plays it, Off = the part plays another engine. */
    std::array<std::array<Led, Core::numParts>, numEngineTypes> engineSegments {};
    std::array<Led, numModes> mode {};                ///< Record > Active (status) > Focus (shown page) > Off
};

KeyLeds keyLeds (const LedInputs& in) noexcept;
LedInputs ledInputs (Core& core, int editPart, Mode page, bool tapeRecording, bool tapePlaying);

inline int numPages (const std::vector<std::string>& ids) { return juce::jmax (1, ((int) ids.size() + 3) / 4); }
} // namespace ohpe::ui
