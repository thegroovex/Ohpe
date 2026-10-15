// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "SampleData.h"

#include <cstdint>
#include <vector>

namespace ohpe::sample
{
struct SliceSettings
{
    float sensitivity = 0.5f;   ///< 0 = only the strongest hits, 1 = every small attack
    float minGapMs = 40.0f;     ///< no two slices closer than this
    int maxSlices = 64;         ///< slice-to-key covers at most this many keys
};

/**
    Transient (onset) detection for auto-slicing. Returns the start frames of the slices in
    ascending order; the first slice always starts at frame 0. Message thread (allocates).

    Method: an instant-attack peak follower is compared with a slow average of the rectified
    signal; an onset is a sample where the peak exceeds the average by a ratio (from the
    sensitivity) and an absolute floor relative to the sample's peak. Each onset is moved back
    to the last near-silent sample before it, so a slice starts just before its attack.
*/
std::vector<int64_t> detectTransients (const IAudioRegion& audio, const SliceSettings& settings);
} // namespace ohpe::sample
