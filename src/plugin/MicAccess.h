// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <functional>

namespace ohpe::mic
{
/**
    p86: the operating system's permission to use the microphone / audio inputs. On macOS an app without it gets
    silence from every input device (no error), so the Voice view asks here before it says "no signal".
    Elsewhere the answer is always granted. Message thread.
*/
enum class Access
{
    granted,       ///< allowed (or the system has no such permission)
    notDetermined, ///< never asked: the system asks the first time an input opens (or on request())
    denied,        ///< refused in the system settings
    restricted     ///< blocked by the device's management
};

Access access();
/** True when the user has to change the permission in the system settings (denied / restricted). */
inline bool blocked (Access a) { return a == Access::denied || a == Access::restricted; }
/** Asks the system for the permission if it was never asked; calls back on the message thread (true = granted). */
void request (std::function<void (bool granted)> done = {});
/** Opens the microphone page of the system's privacy settings (macOS, Windows). False where there is none. */
bool openPrivacySettings();

/** Tests / documentation screenshots: answer `a` instead of the system (`reset` = ask the system again). */
void setAccessForTesting (Access a);
void resetAccessForTesting();
} // namespace ohpe::mic
