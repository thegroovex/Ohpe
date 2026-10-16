// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../engines/SamplerEngine.h"

#include <algorithm>
#include <string>
#include <vector>

/**
    0.4 (package F): the factory sample bank, shared by every instance in the process.

    * Instruments: multisampled recordings (CC0, see THIRD-PARTY.md) that the Sampler engine
      plays when no user sample is loaded and "pN.sampler.inst" selects one, and that the Grain
      engine granulates when "pN.grain.inst" selects one.
    * Drum kits: alternative recordings for the 24 drum pads, selected with "drum.kit"
      (kit 0 is the original factory kit, which the plug-in still hands to each DrumKit with
      setFactorySample()).

    The core has no files: the plug-in decodes the embedded recordings once at start-up and
    hands them in (message thread). Entries are written once and never replaced or freed, so
    the audio thread reads them through plain pointers without any lifetime handling. Without
    a plug-in (core tests) the bank is empty: the Sampler plays its built-in tone, the Grain
    engine its built-in texture and the pads the original kit / synth models.
*/
namespace ohpe::factory
{
/** Instrument list (append-only: the index is stored in presets). 0 = built-in sound. */
constexpr int numInstruments = 15;
/** The instruments of the 0.4 "inst" choice (its list is released); 0.8 adds the keyboard instruments
    11 .. 14 (grand piano, upright piano, tine piano, reed piano), chosen with the "inst2" choice. */
constexpr int numClassicInstruments = 11;
/** Names of the instruments [from, to) (the "inst" choice: 0 .. numClassicInstruments). */
std::vector<std::string> instrumentNamesEn (int from, int to);
std::vector<std::string> instrumentNamesDe (int from, int to);
/** 0.8: the "inst2" choice: "Off" + instruments numClassicInstruments .. numInstruments - 1. */
std::vector<std::string> keyboardChoiceEn();
std::vector<std::string> keyboardChoiceDe();
/** 0.8: the instrument a part plays from its "inst" and "inst2" values (inst2 > 0 wins). */
inline int effectiveInstrument (int inst, int inst2) noexcept
{
    return inst2 > 0 ? std::min (numInstruments - 1, numClassicInstruments - 1 + inst2) : inst;
}
const char* instrumentId (int index) noexcept;      ///< "kalimba" ... (resource folder name)
const char* instrumentNameEn (int index) noexcept;
const char* instrumentNameDe (int index) noexcept;
std::vector<std::string> instrumentNamesEn();
std::vector<std::string> instrumentNamesDe();

/** Drum kits (append-only). 0 = the original factory kit. */
constexpr int numKits = 6;
const char* kitId (int index) noexcept;             ///< "studio", "tight" ...
const char* kitNameEn (int index) noexcept;
const char* kitNameDe (int index) noexcept;
std::vector<std::string> kitNamesEn();
std::vector<std::string> kitNamesDe();

/** Message thread: installs an instrument (first call wins; later calls are ignored). */
void setInstrument (int index, std::vector<sampler::ZoneSpec> zones);
/** Audio thread: the instrument's playback map, or nullptr when it is not loaded. */
const sampler::SampleMap* instrumentMap (int index) noexcept;
/** Audio thread: the zone used as Grain source, its root key in `root`; nullptr if not loaded. */
const SampleData* grainSource (int index, int& root) noexcept;

/** Message thread: installs one pad recording of a kit 1..numKits-1 (first call wins). */
void setKitSample (int kit, int pad, SamplePtr sample);
/** Audio thread: the kit's recording for a pad (nullptr = none). */
const SampleData* kitSample (int kit, int pad) noexcept;
/** Message thread: the owner pointer of a kit recording (nullptr = none). */
SamplePtr kitSampleOwner (int kit, int pad);

/** Number of installed instruments / kit recordings (tests, diagnostics). */
int numLoadedInstruments() noexcept;
int numLoadedKitSamples() noexcept;
} // namespace ohpe::factory
