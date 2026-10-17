// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "TapeMachine.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ohpe::tapefile
{
/**
    p38: the whole tape recorder as one file (".ohpetape"): all eight tracks plus the tape length and the loop
    range, to be loaded again later (in another project, another session).

    Layout (little-endian, as written by the platform): 8 bytes magic "OHPETPF1", uint32 header length, the header
    as UTF-8 JSON {"format":"ohpe-tape","version":1,"sampleRate":..,"lengthSeconds":..,"loopIn":..,"loopOut":..,
    "loop":bool,"editTrack":1..8}, then the tape audio exactly as TapeMachine::saveAudio() writes it (float32
    blocks that hold audio). A tape recorded at another sample rate is resampled on load (linear), so it keeps
    its pitch and its timing; the loop points are in seconds.
*/
inline constexpr const char* extension = ".ohpetape";

struct Settings
{
    double sampleRate = 0.0;      ///< of the stored audio (filled by save from the machine)
    double lengthSeconds = TapeMachine::defaultLengthSeconds;
    double loopIn = 0.0, loopOut = 0.0;
    bool loop = false;
    int editTrack = 1;            ///< 1..8
};

/** Message thread. The tape's committed content (an unfinished recording pass is committed first when not recording). */
std::vector<uint8_t> save (const TapeMachine& tape, Settings settings);

/** Reads the header only. Returns false for anything that is not a tape file of a known version. */
bool readSettings (const void* data, size_t size, Settings& out);

/**
    Message thread. Sets the tape length, then loads the content as one undo step (undo brings the previous tape
    back). Refused (false, nothing changed) while recording or when the data is not a valid tape file. On success
    `out` holds the file's settings (loop range, edit track) for the caller to apply to the parameters.
*/
bool load (TapeMachine& tape, const void* data, size_t size, Settings& out);
} // namespace ohpe::tapefile
