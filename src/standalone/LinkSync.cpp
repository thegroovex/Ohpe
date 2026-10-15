// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LinkSync.h"

#if OHPE_WITH_LINK
 #include <ableton/Link.hpp>
#endif

#include <chrono>

namespace ohpe
{
#if OHPE_WITH_LINK
struct LinkSync::Impl
{
    ableton::Link link { 120.0 };
};

LinkSync::LinkSync() : impl (std::make_unique<Impl>()) {}

LinkSync::~LinkSync()
{
    impl->link.enable (false);
}

bool LinkSync::available() noexcept { return true; }

void LinkSync::setEnabled (bool b)
{
    if (impl->link.isEnabled() != b) impl->link.enable (b);
}

bool LinkSync::isEnabled() const noexcept { return impl->link.isEnabled(); }
int LinkSync::numPeers() const { return (int) impl->link.numPeers(); }
double LinkSync::sessionTempo() const { return impl->link.captureAppSessionState().tempo(); }

bool LinkSync::fill (Transport& tr, int, double, double tempoParam) noexcept
{
    if (! impl->link.isEnabled()) return false;
    constexpr double quantum = 4.0; // one bar of 4/4
    auto state = impl->link.captureAudioSessionState();
    const auto t = impl->link.clock().micros() + std::chrono::microseconds ((long long) (latency.load() * 1.0e6));
    if (tempoParam != lastTempoParam) // Ohpe's tempo was changed: it becomes the session tempo
    {
        if (lastTempoParam >= 0.0)
        {
            state.setTempo (tempoParam, t);
            impl->link.commitAudioSessionState (state);
        }
        lastTempoParam = tempoParam;
    }
    tr.bpm = state.tempo();
    tr.ppqAtBlockStart = state.beatAtTime (t, quantum);
    tr.playing = true; // the core combines this with "seq.run"
    tr.hostDriven = true;
    return true;
}
#else
struct LinkSync::Impl
{
};

LinkSync::LinkSync() = default;
LinkSync::~LinkSync() = default;
bool LinkSync::available() noexcept { return false; }
void LinkSync::setEnabled (bool) {}
bool LinkSync::isEnabled() const noexcept { return false; }
int LinkSync::numPeers() const { return 0; }
double LinkSync::sessionTempo() const { return 0.0; }
bool LinkSync::fill (Transport&, int, double, double) noexcept { return false; }
#endif
} // namespace ohpe
