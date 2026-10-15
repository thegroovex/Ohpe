// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "OhpeCore.h"

namespace ohpe::factorykit
{
/**
    Loads the factory drum kit - real drum recordings (CC0, see THIRD-PARTY.md) embedded in the
    plug-in as FLAC - into the drum pads as their factory samples. Resource "NN-name.flac" goes to
    pad NN. Pads without a recording keep their synth model. Message thread; returns the number
    of pads that got a recording.
*/
int load (Core& core);

/** Number of embedded recordings. */
int numRecordings() noexcept;

/**
    0.4 (package F): decodes the embedded factory sample bank (resources/samples: instruments for
    the Sampler / Grain engines, drum kits Tight, Room, Lo-Fi, Hybrid - CC0 recordings, see
    THIRD-PARTY.md) into the process-wide ohpe::factory bank. Only the first call per process
    decodes; later calls return at once. Message thread; returns the number of files installed
    by this call.
*/
int loadBank();

/** Number of embedded bank files (FLAC). */
int numBankFiles() noexcept;
} // namespace ohpe::factorykit
