// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "UpdateManifest.h"

#include <string>
#include <vector>

namespace ohpe::update
{
/**
    p27: content updates - factory preset / sample packs installed without reinstalling Ohpe.

    A pack is a zip file listed in the release manifest ("content"). It is unpacked into its own folder
    <user data>/groovex/Ohpe/Content/<id>/; the library shows its presets next to the factory bank (read-only).
    Installing a new version first moves the old folder to Content/.backup/<id>/ (one step of rollback). User presets,
    user wavetables, bounces and settings live in other folders and are never touched: every path inside a pack is
    checked to stay inside the pack's folder.

    The registry Content/installed.json records id -> version (and the previous version kept as backup).
*/
struct InstalledPack
{
    std::string id, version;
    std::string previousVersion; ///< kept in Content/.backup/<id>/, empty if none
};

struct ContentRegistry
{
    std::vector<InstalledPack> packs;

    const InstalledPack* find (const std::string& id) const;
    /** Records `version` of pack `id`; the version installed before becomes the backup. */
    void installed (const std::string& id, const std::string& version);
    /** The backup became current again. False if there is none. */
    bool rolledBack (const std::string& id);
    void removed (const std::string& id);

    std::string toJson() const;
    static ContentRegistry fromJson (const std::string& json);
};

/** Pack ids: 1 - 64 characters of a-z, 0-9, '-', '_', '.', not starting with '.'. */
bool isSafePackId (const std::string& id);
/** A relative path inside a pack: no absolute path, drive letter, "..", empty or hidden system names. Backslashes
    count as separators. */
bool isSafeEntryPath (const std::string& path);

enum class PackState { NotInstalled = 0, Installed, UpdateAvailable, NeedsNewerApp };
PackState packState (const ContentPack& offered, const ContentRegistry& reg, const Version& app);
} // namespace ohpe::update
