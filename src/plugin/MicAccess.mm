// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p86: microphone permission - macOS (AVCaptureDevice authorisation for audio). The app is ad-hoc signed: a new
// build has a new signature, and the system may keep an earlier "denied" or ask again - so the state is read live.
#include "plugin/MicAccessCommon.h"

#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>

#import <AVFoundation/AVFoundation.h>

namespace ohpe::mic
{
void setAccessForTesting (Access a) { detail::forced().store ((int) a); }
void resetAccessForTesting() { detail::forced().store (-1); }

Access access()
{
    Access a;
    if (detail::overridden (a)) return a;
    if (@available (macOS 10.14, *))
    {
        switch ([AVCaptureDevice authorizationStatusForMediaType: AVMediaTypeAudio])
        {
            case AVAuthorizationStatusAuthorized: return Access::granted;
            case AVAuthorizationStatusNotDetermined: return Access::notDetermined;
            case AVAuthorizationStatusDenied: return Access::denied;
            case AVAuthorizationStatusRestricted: return Access::restricted;
        }
    }
    return Access::granted;
}

void request (std::function<void (bool)> done)
{
    const auto a = access();
    if (a != Access::notDetermined)
    {
        if (done) juce::MessageManager::callAsync ([done, a] { done (a == Access::granted); });
        return;
    }
    // a host without a microphone usage text would be ended by the system on the request: the host asks itself
    // when it opens its input then
    const bool canAsk = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"NSMicrophoneUsageDescription"] != nil;
    if (! canAsk)
    {
        if (done) juce::MessageManager::callAsync ([done] { done (false); });
        return;
    }
    if (@available (macOS 10.14, *))
    {
        auto cb = std::make_shared<std::function<void (bool)>> (std::move (done));
        [AVCaptureDevice requestAccessForMediaType: AVMediaTypeAudio
                                 completionHandler: ^(BOOL granted) {
                                     const bool ok = granted == YES;
                                     juce::MessageManager::callAsync ([cb, ok] { if (*cb) (*cb) (ok); });
                                 }];
    }
}

bool openPrivacySettings()
{
    return juce::URL ("x-apple.systempreferences:com.apple.preference.security?Privacy_Microphone").launchInDefaultBrowser();
}
} // namespace ohpe::mic
