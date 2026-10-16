// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Visuals.h"

#include "Palette.h"
#include "plugin/Localisation.h"

#include <cmath>

namespace ohpe::ui
{
namespace
{
constexpr float rowSeconds = 1.0f / 30.0f;
constexpr float rootHalf = 0.70710678f;

float toDb (float rms) { return rms > 1.0e-5f ? 20.0f * std::log10 (rms) : -100.0f; }
} // namespace

DisplayVisuals::DisplayVisuals()
{
    readL.assign (8192, 0.0f);
    readR.assign (8192, 0.0f);
    histL.assign (historyFrames, 0.0f);
    histR.assign (historyFrames, 0.0f);
    mono.assign (spectrum.fftSize(), 0.0f);
}

void DisplayVisuals::clear()
{
    std::fill (histL.begin(), histL.end(), 0.0f);
    std::fill (histR.begin(), histR.end(), 0.0f);
    histPos = 0;
    corr = 0.0f;
    midDb = sideDb = -100.0f;
    liveTime = 0.0f;
}

void DisplayVisuals::update (ScopeTap& tap, double sampleRate, float seconds)
{
    // 1. drain the FIFO into the history ring
    size_t total = 0, lastChunk = 0;
    for (int pass = 0; pass < 4; ++pass)
    {
        const size_t n = tap.read (readL.data(), readR.data(), readL.size());
        if (n == 0) break;
        for (size_t i = 0; i < n; ++i)
        {
            histL[(size_t) histPos] = readL[i];
            histR[(size_t) histPos] = readR[i];
            histPos = (histPos + 1) % historyFrames;
        }
        total += n;
        lastChunk = n;
    }
    seen += total;

    // 2. levels, correlation, auto scale of the scope
    if (total > 0)
    {
        const size_t n = std::min (lastChunk, readL.size());
        const auto st = dsp::analyseStereo (readL.data(), readR.data(), n);
        if (st.peak > 1.0e-4f)
        {
            liveTime = 0.6f;
            corr += (st.correlation - corr) * 0.3f;
        }
        const float m = toDb (st.midRms), s = toDb (st.sideRms);
        midDb = m > midDb ? m : midDb + (m - midDb) * 0.12f;
        sideDb = s > sideDb ? s : sideDb + (s - sideDb) * 0.12f;
        const float target = std::clamp (0.85f / std::max (st.peak, 0.004f), 0.9f, 60.0f);
        scale += (target - scale) * (target < scale ? 0.5f : 0.08f);
    }
    else
    {
        midDb = std::max (-100.0f, midDb - 1.5f);
        sideDb = std::max (-100.0f, sideDb - 1.5f);
    }
    if (liveTime <= 0.0f) corr *= 0.9f;
    liveTime = std::max (0.0f, liveTime - seconds);

    // 3. spectrum terrain: one row per 1/30 s while it moves; silence scrolls in flat rows
    if (resting && ! isLive()) return;
    rowClock += seconds;
    int pushes = 0;
    while (rowClock >= rowSeconds && pushes < 4)
    {
        rowClock -= rowSeconds;
        ++pushes;
        head = (head + 1) % numRows;
        auto& row = rows[(size_t) head];
        if (isLive())
        {
            const int n = (int) mono.size();
            for (int i = 0; i < n; ++i)
            {
                const int k = ((histPos - n + i) % historyFrames + historyFrames) % historyFrames;
                mono[(size_t) i] = 0.5f * (histL[(size_t) k] + histR[(size_t) k]);
            }
            const auto& v = spectrum.analyse (mono.data(), mono.size(), sampleRate > 0.0 ? sampleRate : 48000.0);
            for (int b = 0; b < numBands; ++b)
            {
                // light smoothing across bands and a gentle lift of the highs (music falls off)
                const float a = v[(size_t) std::max (0, b - 1)], c = v[(size_t) std::min (numBands - 1, b + 1)];
                const float x = 0.25f * a + 0.5f * v[(size_t) b] + 0.25f * c + 0.1f * (float) b / (float) numBands;
                row[(size_t) b] = std::clamp ((x - 0.18f) / 0.82f, 0.0f, 1.0f);
            }
        }
        else
            row.fill (0.0f);
    }
    if (pushes == 4) rowClock = 0.0f;
    bool flat = ! isLive();
    for (const auto& r : rows)
        for (float x : r) flat = flat && x < 1.0e-3f;
    resting = flat;
}

// ---------------------------------------------------------------- stereo scope
void DisplayVisuals::paintScope (juce::Graphics& g, juce::Rectangle<float> area, const Labels& labels) const
{
    // layout: square-ish area = scope above the correlation row, M / S bars upright on the right;
    //         wide area (a strip) = scope on the left, correlation and horizontal M / S bars beside it
    const float h = area.getHeight();
    const bool wide = area.getWidth() > area.getHeight() * 2.2f;
    juce::Rectangle<float> column, corrRow;
    float size;
    juce::Rectangle<float> sq;
    if (wide)
    {
        size = h * 0.92f;
        sq = juce::Rectangle<float> (size, size).withPosition (area.getX(), area.getCentreY() - size * 0.5f);
        auto rest = area.withTrimmedLeft (size + h * 0.25f);
        rest = rest.withWidth (std::min (rest.getWidth(), h * 4.5f));
        corrRow = rest.removeFromTop (rest.getHeight() * 0.42f);
        column = rest;
    }
    else
    {
        column = area.removeFromRight (area.getWidth() * 0.22f);
        corrRow = area.removeFromBottom (h * 0.2f);
        size = std::min (area.getWidth(), area.getHeight());
        sq = juce::Rectangle<float> (size, size).withCentre (area.getCentre());
        corrRow = corrRow.withTrimmedLeft ((area.getWidth() - size) * 0.5f).withWidth (size);
    }
    const float cx = sq.getCentreX(), cy = sq.getCentreY(), rad = size * 0.48f;

    juce::Path diamond;
    diamond.startNewSubPath (cx, cy - rad);
    diamond.lineTo (cx + rad, cy);
    diamond.lineTo (cx, cy + rad);
    diamond.lineTo (cx - rad, cy);
    diamond.closeSubPath();

    // glass well of the scope (sunk a little deeper than the display)
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (diamond, juce::AffineTransform::translation (0.0f, rad * 0.025f));
    g.setColour (colour::display.brighter (0.04f));
    g.fillPath (diamond);

    // grid: half-scale diamond, mid / side axes, L and R diagonals
    const auto grid = colour::displayGrid.brighter (0.5f);
    g.setColour (grid.withAlpha (0.55f));
    g.drawLine (cx, cy - rad, cx, cy + rad, 1.0f);
    g.drawLine (cx - rad, cy, cx + rad, cy, 1.0f);
    g.setColour (grid.withAlpha (0.35f));
    g.drawLine (cx - rad * 0.5f, cy - rad * 0.5f, cx + rad * 0.5f, cy + rad * 0.5f, 1.0f); // L axis
    g.drawLine (cx + rad * 0.5f, cy - rad * 0.5f, cx - rad * 0.5f, cy + rad * 0.5f, 1.0f); // R axis
    g.strokePath (diamond, juce::PathStrokeType (1.0f), juce::AffineTransform::scale (0.5f, 0.5f, cx, cy));
    g.setColour (grid);
    g.strokePath (diamond, juce::PathStrokeType (1.2f));

    g.setFont (monoFont (std::max (8.0f, rad * 0.13f)));
    g.setColour (colour::displayDim);
    const float t = rad * 0.16f;
    g.drawText ("M", juce::Rectangle<float> (t * 2, t).withCentre ({ cx, cy - rad - t * 0.55f }), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (t, t).withCentre ({ cx - rad * 0.5f - t * 0.5f, cy - rad * 0.5f - t * 0.5f }), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (t, t).withCentre ({ cx + rad * 0.5f + t * 0.5f, cy - rad * 0.5f - t * 0.5f }), juce::Justification::centred);
    g.drawText ("S", juce::Rectangle<float> (t, t).withCentre ({ cx + rad + t * 0.4f, cy - t * 0.6f }), juce::Justification::centred);

    // the Lissajous figure: mid up, side across (left channel upper left)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (diamond);
        const int count = 900;
        juce::Path trace;
        auto point = [&] (int age) {
            const int k = ((histPos - 1 - age) % historyFrames + historyFrames) % historyFrames;
            const float l = histL[(size_t) k], r = histR[(size_t) k];
            return juce::Point<float> (cx + (r - l) * rootHalf * scale * rad, cy - (l + r) * rootHalf * scale * rad);
        };
        if (isLive() || midDb > -90.0f || sideDb > -90.0f)
        {
            trace.startNewSubPath (point (count - 1));
            for (int a = count - 2; a >= 0; --a) trace.lineTo (point (a));
            g.setColour (colour::ultraviolet.withAlpha (0.22f));
            g.strokePath (trace, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colour::displayText.withAlpha (0.55f));
            g.strokePath (trace, juce::PathStrokeType (1.0f));
            g.setColour (colour::displayText);
            for (int a = 0; a < 140; a += 2)
            {
                const auto p = point (a);
                g.fillRect (p.x - 0.8f, p.y - 0.8f, 1.6f, 1.6f);
            }
        }
        else
        {
            g.setColour (colour::displayDim);
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ cx, cy }));
        }
    }

    // correlation meter: -1 .. +1, centre mark, red when out of phase
    {
        auto row = corrRow;
        auto text = row.removeFromTop (row.getHeight() * 0.5f);
        g.setFont (labelFont (std::max (8.0f, text.getHeight() * 0.72f)));
        g.setColour (colour::displayDim);
        g.drawText (i18n::upper (labels.correlation), text, juce::Justification::centredLeft, true);
        g.setFont (monoFont (std::max (8.0f, text.getHeight() * 0.72f)));
        g.setColour (corr < 0.0f ? colour::signalRed : colour::displayText);
        g.drawText ((corr >= 0.0f ? "+" : "") + juce::String (corr, 2), text, juce::Justification::centredRight, false);
        auto bar = row.withSizeKeepingCentre (row.getWidth(), std::max (3.0f, row.getHeight() * 0.32f));
        g.setColour (colour::displayGrid.brighter (0.25f));
        g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
        const float zero = bar.getCentreX();
        const float x = bar.getX() + bar.getWidth() * (std::clamp (corr, -1.0f, 1.0f) + 1.0f) * 0.5f;
        g.setColour (corr < 0.0f ? colour::signalRed : colour::displayText.withAlpha (0.85f));
        g.fillRoundedRectangle (juce::Rectangle<float> (std::min (zero, x), bar.getY(), std::max (bar.getHeight(), std::abs (x - zero)), bar.getHeight()),
                                bar.getHeight() * 0.5f);
        g.setColour (colour::displayDim);
        g.fillRect (juce::Rectangle<float> (1.0f, bar.getHeight() * 2.0f).withCentre ({ zero, bar.getCentreY() }));
    }

    // M / S level bars (the colours of the Mid and Side level encoders)
    const float levels[2] = { midDb, sideDb };
    const juce::Colour colours[2] = { colour::ultraviolet, colour::rose };
    const char* names[2] = { "M", "S" };
    auto dbText = [] (float db) { return db <= -90.0f ? juce::String::fromUTF8 ("\xe2\x80\x93") : juce::String ((int) std::lround (db)) + " dB"; };
    if (wide)
    {
        const float rowH = column.getHeight() / 2.0f;
        for (int i = 0; i < 2; ++i)
        {
            auto row = juce::Rectangle<float> (column.getX(), column.getY() + rowH * (float) i, column.getWidth(), rowH).reduced (0, rowH * 0.18f);
            auto letter = row.removeFromLeft (row.getHeight() * 1.1f);
            auto value = row.removeFromRight (row.getHeight() * 3.2f);
            g.setColour (colours[i]);
            g.setFont (brandFont (std::max (8.0f, letter.getHeight() * 0.8f)));
            g.drawText (names[i], letter, juce::Justification::centredLeft);
            auto track = row.withSizeKeepingCentre (row.getWidth(), std::max (3.0f, row.getHeight() * 0.42f));
            g.setColour (colour::displayGrid.brighter (0.25f));
            g.fillRoundedRectangle (track, track.getHeight() * 0.5f);
            const float norm = std::clamp ((levels[i] + 54.0f) / 54.0f, 0.0f, 1.0f);
            if (norm > 0.0f)
            {
                g.setColour (colours[i]);
                g.fillRoundedRectangle (track.withWidth (std::max (track.getHeight(), track.getWidth() * norm)), track.getHeight() * 0.5f);
            }
            g.setColour (colour::display.withAlpha (0.8f));
            for (int db = 12; db < 54; db += 12)
                g.fillRect (track.getX() + track.getWidth() * (1.0f - (float) db / 54.0f), track.getY(), 1.0f, track.getHeight());
            g.setColour (colour::displayDim);
            g.setFont (monoFont (std::max (7.0f, value.getHeight() * 0.62f)));
            g.drawText (dbText (levels[i]), value, juce::Justification::centredRight);
        }
        return;
    }
    {
        auto col = column.reduced (column.getWidth() * 0.1f, 0);
        auto values = col.removeFromBottom (h * 0.12f);
        auto letters = col.removeFromBottom (h * 0.11f);
        const float w = col.getWidth() / 2.0f;
        for (int i = 0; i < 2; ++i)
        {
            auto track = juce::Rectangle<float> (col.getX() + w * (float) i, col.getY() + h * 0.04f, w, col.getHeight() - h * 0.04f)
                             .reduced (w * 0.28f, 0);
            g.setColour (colour::displayGrid.brighter (0.25f));
            g.fillRoundedRectangle (track, track.getWidth() * 0.3f);
            const float norm = std::clamp ((levels[i] + 54.0f) / 54.0f, 0.0f, 1.0f);
            if (norm > 0.0f)
            {
                g.setColour (colours[i]);
                g.fillRoundedRectangle (track.withTop (track.getBottom() - track.getHeight() * norm), track.getWidth() * 0.3f);
            }
            // -12 / -24 / -36 dB ticks
            g.setColour (colour::display.withAlpha (0.8f));
            for (int db = 12; db < 54; db += 12)
                g.fillRect (track.getX(), track.getBottom() - track.getHeight() * (1.0f - (float) db / 54.0f), track.getWidth(), 1.0f);
            auto cell = juce::Rectangle<float> (col.getX() + w * (float) i, letters.getY(), w, letters.getHeight());
            g.setColour (colours[i]);
            g.setFont (brandFont (std::max (8.0f, letters.getHeight() * 0.8f)));
            g.drawText (names[i], cell, juce::Justification::centred);
            g.setColour (colour::displayDim);
            g.setFont (monoFont (std::max (7.0f, values.getHeight() * 0.62f)));
            g.drawText (levels[i] <= -90.0f ? juce::String::fromUTF8 ("\xe2\x80\x93") : juce::String ((int) std::lround (levels[i])),
                        juce::Rectangle<float> (cell.getX(), values.getY(), cell.getWidth(), values.getHeight()), juce::Justification::centred);
        }
    }
}

// ---------------------------------------------------------------- spectrum terrain
void DisplayVisuals::paintTerrain (juce::Graphics& g, juce::Rectangle<float> area, juce::Point<float> parallax, float opacity) const
{
    if (opacity <= 0.0f || area.isEmpty()) return;
    const float horizon = area.getY() + area.getHeight() * 0.06f;
    const float front = area.getBottom() - area.getHeight() * 0.04f;
    const float sMin = 1.0f / (1.0f + 2.4f);
    for (int k = numRows - 1; k >= 0; --k) // back to front: nearer rows hide the ones behind
    {
        const float z = (float) k / (float) (numRows - 1);
        const float s = (1.0f / (1.0f + 2.4f * z) - sMin) / (1.0f - sMin); // 1 = front, 0 = horizon
        const float base = horizon + (front - horizon) * s;
        const float halfW = area.getWidth() * 0.5f * (0.42f + 0.58f * s);
        const float cx = area.getCentreX() + parallax.x * area.getWidth() * 0.035f * s;
        const float amp = area.getHeight() * 0.5f * (0.25f + 0.75f * s);
        const float lift = parallax.y * area.getHeight() * 0.02f * s;
        const auto& row = rows[(size_t) ((head - k + numRows) % numRows)];

        juce::Path line, fill;
        for (int b = 0; b < numBands; ++b)
        {
            const float x = cx - halfW + 2.0f * halfW * (float) b / (float) (numBands - 1);
            const float y = base + lift - amp * row[(size_t) b];
            if (b == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, base + lift + 1.0f); fill.lineTo (x, y); }
            else { line.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (cx + halfW, base + lift + 1.0f);
        fill.closeSubPath();
        g.setColour (colour::display.withAlpha (std::min (1.0f, opacity * 1.4f)));
        g.fillPath (fill);
        const float a = opacity * (0.18f + 0.82f * s * s);
        if (k == 0)
        {
            g.setColour (colour::ultraviolet.withAlpha (0.45f * opacity));
            g.strokePath (line, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (colour::displayDim.interpolatedWith (colour::displayText, s).withAlpha (a));
        g.strokePath (line, juce::PathStrokeType (k == 0 ? 1.4f : 1.0f));
    }
}
} // namespace ohpe::ui
