// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "SampleData.h"

#include <functional>
#include <string>

namespace ohpe
{
class Core;

/**
    Which sample files are loaded where (sampler zones, grain source, drum pads), saved as the
    "samples" state section:

        {"format":"ohpe-samples","version":1,
         "parts":{"p1":{"sampler":{"zones":[{"path":"...","lo":0,"hi":127,"vlo":1,"vhi":127,"root":-1,
                                              "ls":-1,"le":-1}]},
                        "grain":{"path":"..."}}},
         "drum":[{"pad":1,"path":"..."}]}

    Only file paths are stored, never audio. Built-in sounds are not listed. Message thread.
*/
namespace sample
{
using Decoder = std::function<SamplePtr (const std::string& path)>;

std::string saveAssignments (Core& core);

struct RestoreResult
{
    int loaded = 0;
    int missing = 0; ///< files that could not be decoded (moved / deleted)
};

/** Clears every sample assignment, then loads the ones in `json` through `decode`. */
RestoreResult restoreAssignments (Core& core, const std::string& json, const Decoder& decode);
} // namespace sample
} // namespace ohpe
