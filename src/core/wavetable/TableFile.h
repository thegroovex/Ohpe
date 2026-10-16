// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "UserTable.h"

#include <cstdint>
#include <string>
#include <vector>

/**
    Wavetable files (issue #16), no JUCE.

    Ohpe's own format ".ohpewt" (little endian):

        "OHPEWT"            6 bytes magic
        uint16 version      1
        uint32 frameSize    2048
        uint32 numFrames    0 .. 256 wavetable frames
        uint32 numFormant   0 .. 64 formant frames
        uint32 nameBytes    then the UTF-8 name
        float32[numFrames * frameSize]      level 0 of every frame (band-limited, normalised)
        float32[numFormant * 15]            per frame: 5 frequencies (Hz), 5 levels (dB), 5 bandwidths (Hz)

    Single-cycle wavetable sets (WAV / AIFF / FLAC): frames of `frameSize` samples back to back.
    The frame size comes from a "clm " chunk ("<!>2048 ..."), evenly spaced cue points, or is 2048;
    other sizes are resampled to 2048.
*/
namespace ohpe::usertable
{
constexpr const char* fileExtension = ".ohpewt";

std::vector<uint8_t> encode (const UserTable& table);
/** nullptr (and `error`, English) when the bytes are not a valid table file. */
UserTablePtr decode (const uint8_t* data, size_t size, std::string* error = nullptr);
inline UserTablePtr decode (const std::vector<uint8_t>& bytes, std::string* error = nullptr) { return decode (bytes.data(), bytes.size(), error); }
bool isTableFile (const uint8_t* data, size_t size) noexcept;

/** Frame size declared by a RIFF / WAVE file: "clm " chunk, else evenly spaced "cue " points; 0 = none. */
int wavFrameSizeHint (const uint8_t* data, size_t size);

/** True when a sound of `numSamples` reads as a single-cycle set (a hint, or a multiple of 2048 up to 256 frames). */
bool looksLikeWavetableSet (int64_t numSamples, int frameSizeHint) noexcept;

/** Frames of a single-cycle set (frame size `frameSize`, 0 = 2048) resampled to 2048; at most 256 frames (evenly picked). */
std::vector<float> framesFromSet (const float* samples, int64_t numSamples, int frameSize);

std::string base64Encode (const std::vector<uint8_t>& bytes);
std::vector<uint8_t> base64Decode (const std::string& text);
} // namespace ohpe::usertable
