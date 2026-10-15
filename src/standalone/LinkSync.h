// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"

#include <atomic>
#include <memory>

namespace ohpe
{
/**
    Standalone tempo sync over the network with the Link library (third-party, built when
    the CMake option OHPE_WITH_LINK is on). While "sync.src" is Link the core's transport takes
    tempo and beat phase from the Link session: steps of every sequencer land on the session's
    beat grid. Changing Ohpe's tempo changes the session tempo. Start / stop stays local
    ("seq.run").

    Message thread: setEnabled(), numPeers(), sessionTempo(). Audio thread: fill().
*/
class LinkSync final : public OhpeProcessor::ExternalTransport
{
public:
    LinkSync();
    ~LinkSync() override;

    /** True when Ohpe was built with Link. */
    static bool available() noexcept;

    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept;
    int numPeers() const;
    double sessionTempo() const;
    /** Output latency of the audio device; the beat is computed for the moment the block is heard. */
    void setOutputLatency (double seconds) noexcept { latency.store (seconds); }

    bool fill (Transport& tr, int numSamples, double sampleRate, double tempoParam) noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<double> latency { 0.0 };
    double lastTempoParam = -1.0;
};
} // namespace ohpe
