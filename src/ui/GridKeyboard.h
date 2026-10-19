// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace ohpe::ui
{
/**
    p24: the isomorphic on-screen keyboard (the GRID key between OCTAVE + and MIDI swaps it in for the piano keys).

    Four rows of 24 matte cells; every chord shape has the same form everywhere on the grid. The layout
    ("harmony.kb.layout") sets the steps: Fourths (+1 right, +5 up), Thirds (+1 / +4), Whole Tones (+2 / +5) or In Key
    (scale steps of the remembered key, a fourth per row: only key notes). The octave keys shift it like the piano.

    Cells show the harmony brain: key notes light, other notes graphite, chord tones carry a violet bar, the key's
    root is marked, a pressed cell sinks with a red bar (as the piano keys).

    It plays the edit part, MPE-style: every touch (mouse or finger) gets its own member channel (2 .. 16) when the
    part has MPE on - sliding sideways bends that note (in the layout's semitones per cell, scaled to the part's
    MPE bend range), up / down sends slide (CC 74), touch / pen pressure sends channel pressure. On a part without
    MPE the notes play on its channel (through scale / chord / follow), expression is left out.
*/
class GridKeyboard : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int rows = 4;
    static constexpr int cols = 24;
    static constexpr int maxTouches = 10;

    explicit GridKeyboard (OhpeProcessor& p);
    ~GridKeyboard() override;

    void setPart (int partIndex);
    int part() const noexcept { return partIndex; }
    void setBaseOctave (int octave); ///< lowest C = 12 * (octave + 1) (the piano's octave)
    int baseOctave() const noexcept { return octave; }
    /** Editor timer: repaints when the brain, the layout or the octave changed. */
    void refresh();

    int noteAt (int col, int row) const;    ///< row 0 = bottom
    juce::Rectangle<float> cellArea (int col, int row) const;
    int lowestNote() const;
    int highestNote() const;
    bool isPressed (int note) const noexcept;
    /** p53: notes held on the controller's pads - shown pressed here too (the pads and the screen in sync). */
    std::function<bool (int)> alsoPressed;
    /** Releases every touch (hidden, part change). */
    void releaseAll();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void visibilityChanged() override;

private:
    struct Touch
    {
        int source = -1, note = -1, channel = 1, col = 0, row = 0;
        juce::Point<float> start;
        int lastBend = 8192, lastSlide = 64, lastPressure = -1;
    };
    int layout() const;
    bool partMpe() const;
    int bendRange() const;
    void send (const MidiEvent& e);
    void express (Touch& t, const juce::MouseEvent& e);
    int cellAt (juce::Point<float> p, int& col, int& row) const;

    OhpeProcessor& proc;
    int partIndex = 0, octave = 3, nextChannel = 2;
    std::array<Touch, maxTouches> touches {};
    int lastSignature = 0;
    uint64_t lastHeld = 0; ///< p53

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridKeyboard)
};
} // namespace ohpe::ui
