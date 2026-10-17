// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ohpe::usertable
{
/** Samples per frame (the same as the built-in tables, wavetable::Bank::size). */
constexpr int frameSize = 2048;
/** Mip levels per frame: level k holds harmonics 1 .. maxHarmonics >> k (as wavetable::Bank). */
constexpr int numLevels = 10;
constexpr int maxHarmonics = 512;
constexpr int stride = frameSize + 1; ///< one guard sample for interpolation
constexpr int minFrames = 1, maxFrames = 256;
/** Formant frames per table (the Vocal engine's formant tracks). */
constexpr int numFormants = 5;
constexpr int maxFormantFrames = 64;

/** One frame of formant material: five formants, frequency (Hz), level (dB, 0 = loudest) and bandwidth (Hz). */
struct FormantFrame
{
    std::array<float, numFormants> freq {};
    std::array<float, numFormants> gainDb {};
    std::array<float, numFormants> bandwidth {};
};

/**
    A user wavetable: up to 256 single-cycle frames of 2048 samples (each band-limited, zero-DC,
    normalised to peak 1, with one mip level per octave like the built-in bank) and / or up to
    64 frames of formant material for the Vocal engine. Immutable once created; built on the
    message or a worker thread, published to the engines with RtShared.
*/
class UserTable
{
public:
    /**
        Builds a table. `frames` holds numFrames * frameSize samples (any level, any DC; every
        frame is cleaned: DC removed, harmonics above 512 removed, normalised). Frames that are
        silent copy their neighbour. Either part may be empty, not both.
    */
    static std::shared_ptr<const UserTable> create (std::string name, const std::vector<float>& frames,
                                                    std::vector<FormantFrame> formants = {});

    const std::string& name() const noexcept { return tableName; }
    int numFrames() const noexcept { return frames; }
    bool hasWave() const noexcept { return frames > 0; }
    bool hasFormants() const noexcept { return ! formantFrames.empty(); }

    /** Band-limited frame `f`, mip level `level` (frameSize + 1 samples, the last = the first). */
    const float* frame (int f, int level) const noexcept
    {
        return mips.data() + ((size_t) (f * numLevels + level)) * (size_t) stride;
    }
    /** Level 0 of every frame back to back (numFrames * frameSize): what a table file stores. */
    std::vector<float> levelZero() const;
    const std::vector<FormantFrame>& formants() const noexcept { return formantFrames; }

    /** Formants at a continuous position 0 .. 1 through the formant frames (geometric frequency blend). */
    FormantFrame formantsAt (float position) const noexcept;

    /** File the table was loaded from / saved to (empty = not saved yet). Set by the owner before publishing. */
    std::string path;
    /** Content pack reference "<pack>/<table>" when the table comes from an installed pack (read-only; presets store
        this reference instead of a path or an embedded copy). Empty for user tables. */
    std::string contentRef;

private:
    UserTable() = default;
    std::string tableName;
    int frames = 0;
    std::vector<float> mips;
    std::vector<FormantFrame> formantFrames;
};

using UserTablePtr = std::shared_ptr<const UserTable>;

/** Makes a valid, sorted formant frame (frequencies rising, 50 Hz .. 8 kHz, bandwidths 30 .. 600 Hz, gains -60 .. 0 dB). */
FormantFrame sanitise (FormantFrame f) noexcept;
} // namespace ohpe::usertable
