// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p86: microphone permission - Windows / Linux (no permission the app can read: always granted).
#include "plugin/MicAccessCommon.h"

#include <juce_events/juce_events.h>

namespace ohpe::mic
{
void setAccessForTesting (Access a) { detail::forced().store ((int) a); }
void resetAccessForTesting() { detail::forced().store (-1); }

Access access()
{
    Access a;
    return detail::overridden (a) ? a : Access::granted;
}

void request (std::function<void (bool)> done)
{
    const bool ok = access() == Access::granted;
    if (done) juce::MessageManager::callAsync ([done, ok] { done (ok); });
}

bool openPrivacySettings()
{
   #if JUCE_WINDOWS
    return juce::URL ("ms-settings:privacy-microphone").launchInDefaultBrowser();
   #else
    return false;
   #endif
}
} // namespace ohpe::mic
