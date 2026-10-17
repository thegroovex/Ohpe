// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "UpdateManifest.h"

#include <cstdint>
#include <string>

namespace ohpe::update
{
/**
    p27: the user's update settings (per user, not per project: <user data>/groovex/Ohpe/Settings/update.json).
    Nothing is sent anywhere: the updater only reads the public release list. Times are Unix seconds.
*/
struct Settings
{
    bool autoCheck = false;          ///< opt-in: look for a new version at most once a day
    Channel channel = Channel::Stable;
    int64_t lastCheck = 0;           ///< last successful check
    std::string skipped;             ///< "skip this version": not offered again (a newer one is)
    std::string remindVersion;       ///< "remind me later" for this version ...
    int64_t remindAfter = 0;         ///< ... until then

    /** Defaults for a build of `current`: pre-1.0 and pre-release builds follow the preview channel. */
    static Settings defaults (const Version& current);
    std::string toJson() const;
    /** Reads the JSON; unknown or broken values keep the defaults given in `base`. */
    static Settings fromJson (const std::string& json, const Settings& base);
};

constexpr int64_t secondsPerDay = 24 * 60 * 60;
constexpr int64_t remindDelaySeconds = 3 * secondsPerDay;

/** The automatic check is due: switched on, never while the transport plays, at most once a day. */
bool autoCheckDue (const Settings& s, int64_t now, bool playing);
/** `version` may be offered (not skipped, no pending "remind me later" for it). */
bool shouldOffer (const Settings& s, const Version& version, int64_t now);
/** Remind me later / skip this version. */
void remindLater (Settings& s, const Version& version, int64_t now);
void skipVersion (Settings& s, const Version& version);
} // namespace ohpe::update
