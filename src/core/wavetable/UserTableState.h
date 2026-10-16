// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "UserTable.h"

#include <functional>
#include <string>

namespace ohpe
{
class Core;
}

/**
    Which user wavetable / formant material each part uses (p20), saved as the "usertables" state
    section and as the "usertables" block of part presets:

        {"format":"ohpe-usertables","version":1,
         "parts":{"p1":{"wavetable":{"name":"Choir","path":"/.../Choir.ohpewt","file":"Choir.ohpewt","frames":64,
                                     "data":"<base64 of the .ohpewt file>"},
                        "vocal":{"sameAs":"wavetable"}}}}

    A table is found again by its path, then by its file name in the user wavetable library, then
    from the embedded copy ("data", written for tables up to `embedMaxFrames` frames). When none
    works the part plays the built-in material and the table counts as missing. Message thread.
*/
namespace ohpe::usertable
{
constexpr int embedMaxFrames = 64;

enum class Target { Wavetable = 0, Vocal };

UserTablePtr tableOf (Core& core, int part, Target target);
void setTable (Core& core, int part, Target target, UserTablePtr table);

/** Finds a table file by path or (path empty / not found) by file name in the library; nullptr = not found. */
using Resolver = std::function<UserTablePtr (const std::string& path, const std::string& fileName)>;

struct RestoreResult
{
    int loaded = 0;   ///< found by path / file name
    int embedded = 0; ///< restored from the embedded copy
    int missing = 0;  ///< not found, built-in material plays
};

/** The part's references (empty string when the part has no user material). */
std::string partToJson (Core& core, int part);
/** Clears the part's user material, then loads the references of `json` (empty = just clear). */
RestoreResult partFromJson (Core& core, int part, const std::string& json, const Resolver& resolve);

std::string saveAssignments (Core& core);
RestoreResult restoreAssignments (Core& core, const std::string& json, const Resolver& resolve);
} // namespace ohpe::usertable
