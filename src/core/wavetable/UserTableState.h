// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "UserTable.h"
#include "../update/PackFormat.h"

#include <functional>
#include <string>
#include <vector>

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

    Tables of an installed content pack are stored as a reference only:
        "wavetable":{"name":"Glass Choir","content":"atmo/glass-choir","frames":64}
    ("<pack>/<table>", see update::ContentRef). They are never embedded and never looked up by file name: when the
    pack is not installed the reference counts as "content pack missing" (RestoreResult::packMissing, the pack ids in
    missingPacks) - the part plays the built-in material and the UI says which pack to install.
*/
namespace ohpe::usertable
{
constexpr int embedMaxFrames = 64;

enum class Target { Wavetable = 0, Vocal };

UserTablePtr tableOf (Core& core, int part, Target target);
void setTable (Core& core, int part, Target target, UserTablePtr table);

/** Finds a table file by path or (path empty / not found) by file name in the library; nullptr = not found. */
using Resolver = std::function<UserTablePtr (const std::string& path, const std::string& fileName)>;
/** Finds pack content: the table, or nullptr with `state` PackMissing / ItemMissing. */
using ContentResolver = std::function<UserTablePtr (const update::ContentRef& ref, update::RefState& state)>;

struct RestoreResult
{
    int loaded = 0;   ///< found by path / file name
    int embedded = 0; ///< restored from the embedded copy
    int missing = 0;  ///< not found, built-in material plays (includes packMissing)
    int packMissing = 0;                  ///< pack references whose pack is not installed
    std::vector<std::string> missingPacks; ///< their pack ids (each once)

    void add (const RestoreResult& r);
};

/** The part's references (empty string when the part has no user material). */
std::string partToJson (Core& core, int part);
/** Clears the part's user material, then loads the references of `json` (empty = just clear). */
RestoreResult partFromJson (Core& core, int part, const std::string& json, const Resolver& resolve, const ContentResolver& content = {});

std::string saveAssignments (Core& core);
RestoreResult restoreAssignments (Core& core, const std::string& json, const Resolver& resolve, const ContentResolver& content = {});
} // namespace ohpe::usertable
