// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GridKeyboard.h"

#include "HarmonyView.h"
#include "Palette.h"
#include "plugin/Localisation.h"
#include "seq/ScaleChord.h"

#include <cmath>

namespace ohpe::ui
{
GridKeyboard::GridKeyboard (OhpeProcessor& p) : proc (p)
{
    setOpaque (false);
}

GridKeyboard::~GridKeyboard() { releaseAll(); }

void GridKeyboard::setPart (int p)
{
    p = juce::jlimit (0, Core::numParts - 1, p);
    if (p == partIndex) return;
    releaseAll();
    partIndex = p;
    repaint();
}

void GridKeyboard::setBaseOctave (int o)
{
    o = juce::jlimit (0, 7, o);
    if (o == octave) return;
    releaseAll();
    octave = o;
    repaint();
}

int GridKeyboard::layout() const
{
    return juce::jlimit (0, (int) harmony::numLayouts - 1, (int) std::lround (proc.core().params().get ("harmony.kb.layout")));
}

bool GridKeyboard::partMpe() const { return proc.core().params().get ("p" + std::to_string (partIndex + 1) + ".mpe") >= 0.5f; }

int GridKeyboard::bendRange() const
{
    return juce::jmax (1, (int) std::lround (proc.core().params().get ("p" + std::to_string (partIndex + 1) + ".mpe.bend")));
}

int GridKeyboard::noteAt (int col, int row) const
{
    const auto& h = proc.core().harmony();
    return harmony::gridNote (layout(), col, row, 12 * (octave + 1), h.keyRoot(), h.keyScale());
}

int GridKeyboard::lowestNote() const { return noteAt (0, 0); }
int GridKeyboard::highestNote() const { return noteAt (cols - 1, rows - 1); }

juce::Rectangle<float> GridKeyboard::cellArea (int col, int row) const
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth() / (float) cols, h = b.getHeight() / (float) rows;
    return { b.getX() + w * (float) col, b.getBottom() - h * (float) (row + 1), w, h };
}

int GridKeyboard::cellAt (juce::Point<float> p, int& col, int& row) const
{
    const auto b = getLocalBounds().toFloat();
    if (! b.contains (p)) return -1;
    col = juce::jlimit (0, cols - 1, (int) ((p.x - b.getX()) / b.getWidth() * (float) cols));
    row = juce::jlimit (0, rows - 1, (int) ((b.getBottom() - p.y) / b.getHeight() * (float) rows));
    return noteAt (col, row);
}

bool GridKeyboard::isPressed (int note) const noexcept
{
    for (const auto& t : touches)
        if (t.source >= 0 && t.note == note) return true;
    return false;
}

void GridKeyboard::send (const MidiEvent& e) { proc.core().queueGridEvent (partIndex, e); }

void GridKeyboard::releaseAll()
{
    for (auto& t : touches)
        if (t.source >= 0)
        {
            send (MidiEvent::noteOff (t.channel, t.note, 0));
            t = Touch {};
        }
}

void GridKeyboard::visibilityChanged()
{
    if (! isVisible()) releaseAll();
}

void GridKeyboard::refresh()
{
    const auto& h = proc.core().harmony();
    const auto c = h.chord();
    const int sig = ((((h.keyRoot() * 16 + h.keyScale()) * 200 + (c.root + 1) * 16 + c.type) * 8 + layout()) * 8 + octave) * 2 + (partMpe() ? 1 : 0);
    if (sig != lastSignature)
    {
        lastSignature = sig;
        repaint();
    }
}

void GridKeyboard::mouseDown (const juce::MouseEvent& e)
{
    int col = 0, row = 0;
    const int note = cellAt (e.position, col, row);
    if (note < 0) return;
    Touch* t = nullptr;
    for (auto& x : touches)
        if (x.source == e.source.getIndex()) { t = &x; break; }
    if (t == nullptr)
        for (auto& x : touches)
            if (x.source < 0) { t = &x; break; }
    if (t == nullptr) return;
    if (t->source >= 0) send (MidiEvent::noteOff (t->channel, t->note, 0));
    const bool mpe = partMpe();
    int ch = 1;
    if (mpe) // a free member channel, round robin over 2 .. 16
    {
        for (int k = 0; k < 15; ++k)
        {
            const int cand = 2 + (nextChannel - 2 + k) % 15;
            bool busy = false;
            for (const auto& x : touches) busy |= x.source >= 0 && x.channel == cand;
            if (! busy) { ch = cand; break; }
        }
        nextChannel = 2 + (ch - 2 + 1) % 15;
    }
    *t = Touch {};
    t->source = e.source.getIndex();
    t->note = note;
    t->channel = ch;
    t->col = col;
    t->row = row;
    t->start = e.position;
    // velocity from where the cell is struck: lower = softer, the upper edge = full
    const auto cell = cellArea (col, row);
    const float y = juce::jlimit (0.0f, 1.0f, (cell.getBottom() - e.position.y) / juce::jmax (1.0f, cell.getHeight()));
    const int vel = juce::jlimit (1, 127, (int) std::lround (56.0f + 71.0f * y));
    if (mpe)
    {
        MidiEvent bend;
        bend.status = (uint8_t) (0xe0 | ((ch - 1) & 0x0f));
        bend.data1 = 0;
        bend.data2 = 64;
        send (bend);
        send (MidiEvent::controller (ch, 74, 64, 0));
    }
    send (MidiEvent::noteOn (ch, note, vel, 0));
    if (mpe) express (*t, e);
    repaint (cellArea (col, row).getSmallestIntegerContainer().expanded (2));
}

void GridKeyboard::express (Touch& t, const juce::MouseEvent& e)
{
    const auto cell = cellArea (t.col, t.row);
    const auto steps = harmony::layoutSteps (layout());
    const float semisPerCell = layout() == harmony::LayoutInKey ? 12.0f / 7.0f : (float) steps.col;
    const float semis = (e.position.x - t.start.x) / juce::jmax (1.0f, cell.getWidth()) * semisPerCell;
    const int bend = juce::jlimit (0, 16383, (int) std::lround (8192.0f + semis / (float) bendRange() * 8192.0f));
    if (bend != t.lastBend)
    {
        t.lastBend = bend;
        MidiEvent b;
        b.status = (uint8_t) (0xe0 | ((t.channel - 1) & 0x0f));
        b.data1 = (uint8_t) (bend & 0x7f);
        b.data2 = (uint8_t) ((bend >> 7) & 0x7f);
        send (b);
    }
    const int slide = juce::jlimit (0, 127, (int) std::lround (64.0f + (t.start.y - e.position.y) / juce::jmax (1.0f, cell.getHeight() * 1.5f) * 63.0f));
    if (slide != t.lastSlide)
    {
        t.lastSlide = slide;
        send (MidiEvent::controller (t.channel, 74, slide, 0));
    }
    if (e.isPressureValid())
    {
        const int pr = juce::jlimit (0, 127, (int) std::lround (e.pressure * 127.0f));
        if (pr != t.lastPressure)
        {
            t.lastPressure = pr;
            MidiEvent p;
            p.status = (uint8_t) (0xd0 | ((t.channel - 1) & 0x0f));
            p.data1 = (uint8_t) pr;
            p.data2 = 0;
            send (p);
        }
    }
}

void GridKeyboard::mouseDrag (const juce::MouseEvent& e)
{
    if (! partMpe()) return; // without MPE a slide does nothing (the note keeps its cell)
    for (auto& t : touches)
        if (t.source == e.source.getIndex()) express (t, e);
}

void GridKeyboard::mouseUp (const juce::MouseEvent& e)
{
    for (auto& t : touches)
        if (t.source == e.source.getIndex())
        {
            send (MidiEvent::noteOff (t.channel, t.note, 0));
            const auto cell = cellArea (t.col, t.row);
            t = Touch {};
            repaint (cell.getSmallestIntegerContainer().expanded (2));
        }
}

void GridKeyboard::paint (juce::Graphics& g)
{
    const auto& h = proc.core().harmony();
    const int kr = h.keyRoot(), ks = h.keyScale();
    const auto chord = h.chord();
    uint16_t chordPcs = 0;
    if (chord.root >= 0)
        for (int i = 0; i < 12; ++i)
            if (((harmony::chordShape (chord.type) >> i) & 1u) != 0) chordPcs = (uint16_t) (chordPcs | (1u << ((chord.root + i) % 12)));
    const float s = (float) getHeight() / 136.0f; // editor scale (the grid is 136 design units high)

    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
        {
            const int note = noteAt (col, row);
            const int pc = note % 12;
            const bool inKey = scales::inScale (note, kr, ks), isRoot = pc == kr, isChord = ((chordPcs >> pc) & 1u) != 0;
            const bool down = isPressed (note);
            auto cell = cellArea (col, row).reduced (juce::jmax (1.0f, 1.6f * s));
            const float r = juce::jmin (5.0f * s, cell.getHeight() * 0.18f);
            g.setColour (juce::Colours::black.withAlpha (inKey ? 0.12f : 0.22f));
            g.fillRoundedRectangle (cell.translated (0, down ? 0.8f * s : 2.0f * s), r);
            if (down) cell = cell.translated (0, 0.8f * s);
            if (inKey)
            {
                const auto top = down ? colour::keyLightPressed : (isRoot ? colour::keyLight : colour::capTop);
                g.setGradientFill (juce::ColourGradient (top, 0, cell.getY(), top.darker (0.05f), 0, cell.getBottom(), false));
                g.fillRoundedRectangle (cell, r);
                g.setColour (colour::capEdge.withAlpha (0.7f));
                g.drawRoundedRectangle (cell.reduced (0.5f), r, 1.0f);
            }
            else
            {
                const auto base = down ? colour::keyDark.brighter (0.22f) : colour::keyDark;
                g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), 0, cell.getY(), base, 0, cell.getBottom(), false));
                g.fillRoundedRectangle (cell, r);
            }
            // light bars at the lower edge: violet = chord tone, red = pressed
            auto bar = cell.withTop (cell.getBottom() - juce::jmax (2.0f, cell.getHeight() * 0.08f)).reduced (cell.getWidth() * 0.26f, 0.0f)
                           .translated (0, -cell.getHeight() * 0.1f);
            if (down) { g.setColour (colour::signalRed); g.fillRoundedRectangle (bar, 1.0f); }
            else if (isChord) { g.setColour (colour::ultraviolet); g.fillRoundedRectangle (bar, 1.0f); }
            // the note's name; the key's root bold with its octave
            g.setFont (isRoot ? brandFont (cell.getHeight() * 0.3f) : labelFont (cell.getHeight() * 0.26f));
            g.setColour (inKey ? (isRoot ? colour::ink : colour::engraving) : colour::displayText.withAlpha (0.55f));
            const auto label = isRoot ? harmonyview::noteName (pc) + juce::String (note / 12 - 1) : harmonyview::noteName (pc);
            g.drawText (label, cell.withTrimmedBottom (cell.getHeight() * 0.28f), juce::Justification::centred, false);
        }
}
} // namespace ohpe::ui
