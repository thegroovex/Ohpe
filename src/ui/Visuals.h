// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "dsp/StereoAnalysis.h"
#include "model/StereoFifo.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

namespace ohpe::ui
{
/**
    0.7: the moving visuals of the display, fed from the core's scope tap (lock-free FIFO).

      * stereo scope: goniometer (Lissajous figure rotated by 45 degrees: mid up, side across),
        correlation meter and M / S level bars;
      * spectrum terrain: a perspective waterfall of the spectrum - new rows rise at the front
        and recede to the horizon. It comes to rest (flat) when the signal is silent.

    UI thread only. All buffers are allocated in the constructor; update() reads the FIFO,
    analyses and keeps the smoothed state that paint() draws. About 30 updates per second.
*/
class DisplayVisuals
{
public:
    DisplayVisuals();

    /** Reads what the tap delivered since the last call and advances the animation. */
    void update (ScopeTap& tap, double sampleRate, float seconds);
    /** Forgets the history (source changed). */
    void clear();

    /** True while sound arrives (and shortly after): the terrain moves. */
    bool isLive() const noexcept { return liveTime > 0.0f; }
    /** True when every terrain row has come to rest. */
    bool isResting() const noexcept { return resting; }

    struct Labels
    {
        juce::String correlation, mid, side, width; ///< translated
    };

    /** Goniometer + correlation + M / S bars in `area`. */
    void paintScope (juce::Graphics& g, juce::Rectangle<float> area, const Labels& labels) const;
    /** The spectrum terrain in `area`; `parallax` (-1..1) shifts the near rows more than the far
        ones, `opacity` dims the whole terrain (backdrop behind the character). */
    void paintTerrain (juce::Graphics& g, juce::Rectangle<float> area, juce::Point<float> parallax, float opacity) const;

    // for tests
    float correlation() const noexcept { return corr; }
    float midLevelDb() const noexcept { return midDb; }
    float sideLevelDb() const noexcept { return sideDb; }
    size_t framesSeen() const noexcept { return seen; }

    static constexpr int numBands = 48;
    static constexpr int numRows = 26;
    static constexpr int historyFrames = 2048;

private:
    std::vector<float> readL, readR, histL, histR, mono;
    int histPos = 0;
    size_t seen = 0;
    dsp::SpectrumBands spectrum { 11, numBands }; // 2048 points: ~23 Hz bins, clean low bands
    std::array<std::array<float, numBands>, numRows> rows {};
    int head = 0;       ///< index of the newest (front) row
    float rowClock = 0.0f, liveTime = 0.0f;
    bool resting = true;
    float corr = 0.0f, midDb = -100.0f, sideDb = -100.0f, scale = 4.0f;
};
} // namespace ohpe::ui
