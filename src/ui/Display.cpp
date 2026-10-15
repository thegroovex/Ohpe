// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Display.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <cmath>

namespace ohpe::ui
{
using i18n::tr;

namespace
{
const juce::String& dot()
{
    static const juce::String s = juce::String::fromUTF8 ("  \xc2\xb7  ");
    return s;
}

juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String paramText (OhpeProcessor& proc, const juce::String& id, float norm)
{
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (id.toStdString());
    if (idx < 0) return {};
    const auto& d = reg.descriptor (idx);
    return formatValue (d, d.fromNormalised (norm));
}

/** Euclidean rhythm: hit at step i of `steps` with `pulses` hits, rotated by `rot`. */
bool euclidHit (int i, int steps, int pulses, int rot)
{
    if (steps <= 0 || pulses <= 0) return false;
    if (pulses >= steps) return true;
    const int k = ((i - rot) % steps + steps) % steps;
    return ((k * pulses) % steps) < pulses;
}
} // namespace

Display::Display (OhpeProcessor& p) : proc (p) { setOpaque (false); }

void Display::setMode (Mode m, int pg, int numPagesInMode, const std::vector<juce::String>& encoderIds, const juce::String& title)
{
    if (m != mode) overviewAge = 1.0e9f;
    mode = m;
    page = pg;
    pages = numPagesInMode;
    ids = encoderIds;
    pageTitle = title;
    repaint();
}

void Display::flash (const juce::String& message)
{
    flashText = message;
    flashTime = 2.5f;
    repaint();
}

void Display::tick (float seconds)
{
    time += seconds;
    const float target = juce::jmin (1.0f, (float) proc.core().activeVoices() / 3.0f);
    energy += (target - energy) * 0.2f;
    if (flashTime > 0.0f) flashTime -= seconds;
    overviewAge += seconds;
    if (mode == Mode::Tape && overviewAge > 0.5f) updateTapeOverview();
    repaint();
}

void Display::updateTapeOverview()
{
    overviewAge = 0.0f;
    auto& tape = proc.core().tape();
    const int64_t end = juce::jmax ((int64_t) 1, tape.contentEndFrames());
    overviewFrames = tape.contentEndFrames();
    std::array<float, 128> l {}, r {};
    for (int t = 0; t < TapeMachine::numTracks; ++t)
    {
        const int64_t trackEnd = tape.contentEndFrames (t);
        for (int i = 0; i < overviewPoints; ++i)
        {
            const int64_t start = end * i / overviewPoints;
            float peak = 0.0f;
            if (start < trackEnd)
            {
                tape.readCommitted (t, start, (int) l.size(), l.data(), r.data());
                for (size_t k = 0; k < l.size(); ++k) peak = juce::jmax (peak, std::abs (l[k]), std::abs (r[k]));
            }
            overview[(size_t) t][(size_t) i] = juce::jmin (1.0f, peak);
        }
    }
}

// ---------------------------------------------------------------- layout
juce::Rectangle<float> Display::headerArea() const
{
    auto b = getLocalBounds().toFloat().reduced (getWidth() * 0.02f);
    return b.withHeight (b.getHeight() * 0.12f);
}

juce::Rectangle<float> Display::stripArea() const
{
    auto b = getLocalBounds().toFloat().reduced (getWidth() * 0.02f);
    float f = 0.27f;
    if (mode == Mode::Tape) f = 0.36f;
    else if (mode == Mode::Drum) f = 0.36f;
    else if (hasSoftKeys() && mode != Mode::Sequencer) f = 0.30f;
    else if (mode == Mode::Sequencer && hasSoftKeys()) f = 0.33f;
    return b.withTop (b.getBottom() - b.getHeight() * f);
}

juce::Rectangle<float> Display::midArea() const
{
    const float u = (float) getWidth() / 100.0f;
    auto glass = getLocalBounds().toFloat().reduced (u * 0.6f);
    auto inner = glass.reduced (u * 1.4f);
    return inner.withTop (headerArea().getBottom() + u).withBottom (stripArea().getY() - u);
}

juce::Rectangle<float> Display::seqTab (int view) const
{
    auto mid = midArea();
    const float w = mid.getWidth() / (float) numSeqViews;
    return { mid.getX() + w * (float) view, mid.getY(), w, mid.getHeight() * 0.14f };
}

juce::Rectangle<float> Display::rowsArea() const
{
    const float u = (float) getWidth() / 100.0f;
    auto mid = midArea();
    if (mode == Mode::Sequencer) mid = mid.withTrimmedTop (mid.getHeight() * 0.17f);
    return mid.withTrimmedLeft (mid.getWidth() * 0.42f + u * 2.0f);
}

juce::Rectangle<float> Display::captionArea() const
{
    auto r = rowsArea();
    return r.withHeight (r.getHeight() * 0.13f);
}

juce::Rectangle<float> Display::rowArea (int i) const
{
    auto r = rowsArea();
    r = r.withTrimmedTop (r.getHeight() * 0.13f);
    const float rowH = r.getHeight() / 4.0f;
    return { r.getX(), r.getY() + rowH * (float) i, r.getWidth(), rowH };
}

juce::Rectangle<float> Display::barArea (int i) const
{
    const auto full = rowArea (i);
    const float rowH = full.getHeight();
    const auto row = full.reduced (0, rowH * 0.12f);
    const auto textArea = row.withTrimmedLeft (row.getHeight() * 0.7f);
    return textArea.withTop (textArea.getY() + rowH * 0.72f).withHeight (rowH * 0.12f);
}

juce::Rectangle<float> Display::softKeyRow() const
{
    auto s = stripArea();
    return s.withTop (s.getBottom() - s.getHeight() * (mode == Mode::Tape ? 0.2f : (mode == Mode::Drum ? 0.22f : 0.3f)));
}

juce::Rectangle<float> Display::softKeyArea (int index) const
{
    if (index < 0 || index >= (int) softKeys.size()) return {};
    auto row = softKeyRow();
    const int n = juce::jmax (4, (int) softKeys.size());
    // the tape page keeps the left part of the row free for the time readout
    if (mode == Mode::Tape) row = row.withTrimmedLeft (row.getWidth() * 0.24f);
    const float w = row.getWidth() / (float) n;
    // keys sit at the right end of the row
    const float x0 = row.getRight() - w * (float) softKeys.size();
    return juce::Rectangle<float> (x0 + w * (float) index, row.getY(), w, row.getHeight()).reduced (w * 0.04f, row.getHeight() * 0.1f);
}

juce::Rectangle<float> Display::stepCell (int i) const
{
    auto s = stripArea();
    s = s.withTrimmedLeft (s.getWidth() * 0.12f);
    const float w = s.getWidth() / 16.0f;
    const float h = hasSoftKeys() ? 0.50f : 0.62f;
    // leaves room below for the play marker and the hint line / soft keys
    return { s.getX() + w * (float) i, s.getY() + s.getHeight() * 0.08f, w, s.getHeight() * h };
}

juce::Rectangle<float> Display::stepPageTab (int i) const
{
    auto s = stripArea();
    const float w = s.getWidth() * 0.12f / 2.0f;
    const float h = hasSoftKeys() ? 0.25f : 0.30f;
    return { s.getX() + w * (float) (i % 2), s.getY() + s.getHeight() * (0.08f + (h + 0.02f) * (float) (i / 2)), w, s.getHeight() * h };
}

juce::Rectangle<float> Display::channelCell (int i) const
{
    auto s = stripArea();
    const float w = s.getWidth() / 8.0f;
    return { s.getX() + w * (float) i, s.getY(), w, s.getHeight() };
}

juce::Rectangle<float> Display::trackLane (int t) const
{
    auto s = stripArea();
    s = s.withTrimmedBottom (s.getHeight() * 0.22f).withTrimmedTop (s.getHeight() * 0.03f);
    const float h = s.getHeight() / (float) TapeMachine::numTracks;
    return { s.getX(), s.getY() + h * (float) t, s.getWidth(), h };
}

juce::Rectangle<float> Display::padCell (int p) const
{
    auto s = stripArea();
    s = s.withTrimmedBottom (s.getHeight() * 0.24f).withTrimmedTop (s.getHeight() * 0.03f);
    const float w = s.getWidth() / 12.0f, h = s.getHeight() / 2.0f;
    return juce::Rectangle<float> (s.getX() + w * (float) (p % 12), s.getY() + h * (float) (p / 12), w, h).reduced (w * 0.06f, h * 0.06f);
}

juce::Rectangle<float> Display::pageArrow (bool right) const
{
    auto h = headerArea();
    const float w = h.getHeight() * 1.1f;
    return right ? h.withLeft (h.getRight() - w) : h.withLeft (h.getRight() - w * 3.2f).withWidth (w);
}

// ---------------------------------------------------------------- row values
float Display::rowValue (int row) const
{
    if (row < 0 || row >= 4) return 0.0f;
    if (auto& v = virtuals[(size_t) row]; v != nullptr) return v->get ? v->get() : 0.0f;
    if (row < (int) ids.size())
        if (auto* p = proc.parameter (ids[(size_t) row])) return p->getValue();
    return 0.0f;
}

void Display::setRowValue (int row, float norm)
{
    if (row < 0 || row >= 4) return;
    if (auto& v = virtuals[(size_t) row]; v != nullptr)
    {
        if (v->set) v->set (norm);
    }
    else if (row < (int) ids.size())
        if (auto* p = proc.parameter (ids[(size_t) row])) p->setValueNotifyingHost (norm);
    repaint();
}

void Display::resetRow (int row)
{
    if (row < 0 || row >= 4) return;
    if (auto& v = virtuals[(size_t) row]; v != nullptr)
    {
        if (v->reset) v->reset();
    }
    else if (row < (int) ids.size())
        if (auto* p = proc.parameter (ids[(size_t) row])) setParameterNormalised (proc, ids[(size_t) row], p->getDefaultValue());
    repaint();
}

int Display::rowAt (juce::Point<float> p) const
{
    for (int i = 0; i < 4; ++i)
    {
        const bool used = virtuals[(size_t) i] != nullptr || (i < (int) ids.size() && proc.parameter (ids[(size_t) i]) != nullptr);
        if (used && rowArea (i).contains (p)) return i;
    }
    return -1;
}

// ---------------------------------------------------------------- mouse
void Display::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    dragRow = -1;
    if (pages > 1 && pageArrow (false).contains (p)) { if (onPageChange) onPageChange (-1); return; }
    if (pages > 1 && pageArrow (true).contains (p)) { if (onPageChange) onPageChange (1); return; }
    for (int i = 0; i < (int) softKeys.size(); ++i)
        if (softKeyArea (i).contains (p))
        {
            if (softKeys[(size_t) i].enabled && onSoftKey) onSoftKey (softKeys[(size_t) i].id);
            return;
        }
    if (lockStep >= 0 && captionArea().contains (p)) { if (onLockBadgeClicked) onLockBadgeClicked(); return; }

    // parameter rows: drag, click to jump, right-click menu
    if (const int row = rowAt (p); row >= 0)
    {
        const juce::String id = virtuals[(size_t) row] != nullptr ? virtuals[(size_t) row]->paramId
                                                                    : (row < (int) ids.size() ? ids[(size_t) row] : juce::String());
        if (e.mods.isPopupMenu())
        {
            if (id.isNotEmpty()) showControlMenu (proc, id, this);
            return;
        }
        dragRow = row;
        const bool fine = isFineDrag (e.mods);
        float start = rowValue (row);
        const auto bar = barArea (row);
        if (! fine && p.x >= bar.getX() && p.x <= bar.getRight() && p.y >= bar.getY() - rowArea (row).getHeight() * 0.25f)
            start = juce::jlimit (0.0f, 1.0f, (p.x - bar.getX()) / juce::jmax (1.0f, bar.getWidth())); // jump to the click
        if (virtuals[(size_t) row] == nullptr)
            if (auto* prm = proc.parameter (id)) prm->beginChangeGesture();
        drag.begin (start, p, fine);
        setRowValue (row, start);
        return;
    }

    if (mode == Mode::Sequencer)
    {
        for (int v = 0; v < numSeqViews; ++v)
            if (seqTab (v).contains (p)) { if (onSeqViewClicked) onSeqViewClicked ((SeqView) v); return; }
        if (ctx.seqView == SeqView::Step)
        {
            for (int i = 0; i < 16; ++i)
                if (stepCell (i).contains (p))
                {
                    const int step = stepPage * 16 + i;
                    if (e.mods.isShiftDown() || e.mods.isPopupMenu()) { if (onStepSelected) onStepSelected (step); }
                    else if (onStepClicked) onStepClicked (step);
                    return;
                }
            for (int i = 0; i < 4; ++i)
                if (stepPageTab (i).contains (p)) { if (onStepPageClicked) onStepPageClicked (i); return; }
        }
    }
    if (mode == Mode::Mixer)
        for (int i = 0; i < 8; ++i)
            if (channelCell (i).contains (p)) { if (onChannelClicked) onChannelClicked (i); return; }
    if (mode == Mode::Tape)
        for (int t = 0; t < TapeMachine::numTracks; ++t)
            if (trackLane (t).contains (p)) { if (onTrackClicked) onTrackClicked (t); return; }
    if (mode == Mode::Drum)
        for (int pad = 0; pad < 24; ++pad)
            if (padCell (pad).contains (p)) { if (onPadClicked) onPadClicked (pad); return; }
}

void Display::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0) return;
    const float len = juce::jmax (40.0f, barArea (dragRow).getWidth());
    setRowValue (dragRow, drag.update (e.position, len, isFineDrag (e.mods)));
}

void Display::mouseUp (const juce::MouseEvent&)
{
    if (dragRow >= 0 && virtuals[(size_t) dragRow] == nullptr && dragRow < (int) ids.size())
        if (auto* prm = proc.parameter (ids[(size_t) dragRow])) prm->endChangeGesture();
    dragRow = -1;
}

void Display::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (const int row = rowAt (e.position); row >= 0 && ! e.mods.isPopupMenu()) resetRow (row);
}

// ---------------------------------------------------------------- painting
void Display::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float u = b.getWidth() / 100.0f; // display unit

    // glass: bezel, deep fill, faint dot grid
    g.setColour (colour::bodyShade);
    g.fillRoundedRectangle (b, u * 1.6f);
    const auto glass = b.reduced (u * 0.6f);
    g.setColour (colour::display);
    g.fillRoundedRectangle (glass, u * 1.2f);
    g.setColour (colour::displayGrid);
    for (float x = glass.getX() + u * 2; x < glass.getRight() - u; x += u * 2.0f)
        for (float y = glass.getY() + u * 2; y < glass.getBottom() - u; y += u * 2.0f)
            g.fillRect (x, y, u * 0.15f, u * 0.15f);

    auto& core = proc.core();
    const auto& info = modeInfo (mode);
    const auto type = selectedEngine (core, ctx.part);
    const auto& eng = engineTypeInfo (type);

    // header
    auto h = headerArea();
    g.setFont (brandFont (h.getHeight() * 0.62f));
    g.setColour (colour::displayText);
    juce::String title = i18n::upper (tr (info.nameEn));
    const bool partMode = mode == Mode::Synth || mode == Mode::Envelope || mode == Mode::Lfo;
    if (mode == Mode::Synth) title << "  /  " << i18n::upper (i18n::pick (eng.nameEn, eng.nameDe));
    if (mode == Mode::Mixer) title << "  /  " << i18n::upper (tr ("Channel")) << " " << (ctx.mixerChannel + 1);
    if (mode == Mode::Tape) title << "  /  " << i18n::upper (tr ("Track")) << " " << (ctx.tapeTrack + 1);
    if (mode == Mode::Drum) title << "  /  " << i18n::upper (tr ("Pad")) << " " << (ctx.drumPad + 1);
    const float titleW = juce::jmin (h.getWidth() * 0.62f, juce::GlyphArrangement::getStringWidth (brandFont (h.getHeight() * 0.62f), title) + 4.0f);
    g.drawText (title, h.withWidth (titleW), juce::Justification::centredLeft, true);
    if (partMode || mode == Mode::Effects)
    {
        // part badge: which of the four parts the pages edit
        auto badge = juce::Rectangle<float> (h.getHeight() * 1.5f, h.getHeight() * 0.62f)
                         .withPosition (h.getX() + titleW + h.getHeight() * 0.4f, h.getCentreY() - h.getHeight() * 0.31f);
        g.setColour (colour::sunYellow);
        g.fillRoundedRectangle (badge, badge.getHeight() * 0.3f);
        g.setColour (colour::display);
        g.setFont (brandFont (badge.getHeight() * 0.72f));
        g.drawText ("P" + juce::String (ctx.part + 1), badge, juce::Justification::centred);
    }
    g.setFont (monoFont (h.getHeight() * 0.5f));
    g.setColour (colour::displayDim);
    const auto bpm = core.params().get (Core::Ids::tempo);
    const bool running = core.params().get (Core::Ids::seqRun) >= 0.5f;
    g.drawText ((running ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 ")) : juce::String()) + juce::String (bpm, 1) + " BPM",
                h.withTrimmedRight (h.getHeight() * 4.0f), juce::Justification::centredRight);
    if (pages > 1)
    {
        g.setColour (colour::displayText);
        g.drawText (juce::String (page + 1) + "/" + juce::String (pages), pageArrow (false).withX (pageArrow (false).getRight()).withWidth (pageArrow (true).getX() - pageArrow (false).getRight()),
                    juce::Justification::centred);
        for (bool right : { false, true })
        {
            auto a = pageArrow (right).reduced (h.getHeight() * 0.28f);
            juce::Path tri;
            if (right) tri.addTriangle (a.getX(), a.getY(), a.getX(), a.getBottom(), a.getRight(), a.getCentreY());
            else tri.addTriangle (a.getRight(), a.getY(), a.getRight(), a.getBottom(), a.getX(), a.getCentreY());
            g.fillPath (tri);
        }
    }

    // sequencer views: tabs
    auto mid = midArea();
    if (mode == Mode::Sequencer)
    {
        for (int v = 0; v < numSeqViews; ++v)
        {
            auto tab = seqTab (v).reduced (u * 0.4f, 0);
            const bool sel = (int) ctx.seqView == v;
            g.setColour (sel ? colour::displayText : colour::displayGrid.brighter (0.25f));
            if (sel) g.fillRoundedRectangle (tab, tab.getHeight() * 0.25f);
            else g.drawRoundedRectangle (tab, tab.getHeight() * 0.25f, 1.0f);
            g.setColour (sel ? colour::display : colour::displayDim);
            g.setFont (labelFont (tab.getHeight() * 0.6f));
            g.drawText (i18n::upper (tr (seqViewName ((SeqView) v))), tab, juce::Justification::centred, true);
        }
        mid = mid.withTrimmedTop (mid.getHeight() * 0.17f);
    }

    // middle: pictogram character + encoder rows
    auto pictoArea = mid.withWidth (mid.getWidth() * 0.42f);
    PictoState ps;
    ps.large = true;
    ps.time = time;
    ps.energy = energy;
    for (int i = 0; i < 4; ++i) ps.macro[i] = rowValue (i);
    const Picto picto = mode == Mode::Synth ? (Picto) (int) type : info.picto;
    const bool dim = (mode == Mode::Synth && ! eng.implemented) || ! info.available;
    drawPictogram (g, pictoArea.reduced (u * 1.5f), picto, ps, dim ? colour::displayDim : colour::displayText);
    if (dim)
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 2.2f));
        g.drawText (tr ("Coming in a later version"), pictoArea.withTop (pictoArea.getBottom() - u * 4), juce::Justification::centred);
    }
    paintRows (g);
    paintStrip (g, stripArea());
}

void Display::paintRows (juce::Graphics& g)
{
    const auto& reg = proc.core().params();
    // caption: page title, or the lock badge while a step is selected
    auto cap = captionArea();
    if (lockStep >= 0)
    {
        g.setColour (colour::signalRed);
        auto pill = cap.withWidth (juce::jmin (cap.getWidth(), cap.getHeight() * 9.5f));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.3f);
        g.setColour (colour::displayText);
        g.setFont (labelFont (pill.getHeight() * 0.72f));
        g.drawText (i18n::upper (tr ("Lock")) + "  " + i18n::upper (tr ("Step")) + " " + juce::String (lockStep + 1) + utf8 ("  \xc3\x97"),
                    pill.reduced (pill.getHeight() * 0.3f, 0), juce::Justification::centredLeft, true);
    }
    else if (pageTitle.isNotEmpty())
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (cap.getHeight() * 0.75f));
        g.drawText (i18n::upper (pageTitle), cap, juce::Justification::centredLeft, true);
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto full = rowArea (i);
        const float rowH = full.getHeight();
        auto row = full.reduced (0, rowH * 0.12f);
        const auto c = colour::encoder (i);
        const auto dot = row.withWidth (row.getHeight() * 0.42f).withSizeKeepingCentre (row.getHeight() * 0.42f, row.getHeight() * 0.42f);
        const auto& v = virtuals[(size_t) i];
        const juce::String id = v != nullptr ? v->paramId : (i < (int) ids.size() ? ids[(size_t) i] : juce::String());
        const int idx = reg.indexOf (id.toStdString());
        const bool used = v != nullptr || idx >= 0;
        g.setColour (used ? c : colour::displayGrid);
        if (v != nullptr && v->locked) g.fillRoundedRectangle (dot, dot.getHeight() * 0.2f); // square dot = locked value
        else g.fillEllipse (dot);
        if (! used) continue;
        const float norm = rowValue (i);
        juce::String name, value;
        if (v != nullptr)
        {
            name = v->name;
            value = v->text ? v->text (norm) : juce::String();
        }
        else
        {
            const auto& d = reg.descriptor (idx);
            name = i18n::pick (d.nameEn, d.nameDe);
            value = paramText (proc, id, norm);
        }
        auto textArea = row.withTrimmedLeft (row.getHeight() * 0.7f);
        g.setFont (monoFont (row.getHeight() * 0.4f));
        const float valueW = juce::jmin (textArea.getWidth() * 0.45f, juce::GlyphArrangement::getStringWidth (monoFont (row.getHeight() * 0.4f), value) + 6.0f);
        g.setColour (colour::displayText);
        g.setFont (labelFont (row.getHeight() * 0.42f));
        g.drawText (name, textArea.withHeight (row.getHeight() * 0.6f).withTrimmedRight (valueW + 4.0f), juce::Justification::centredLeft, true);
        g.setFont (monoFont (row.getHeight() * 0.4f));
        g.setColour (c);
        g.drawText (value, textArea.withHeight (row.getHeight() * 0.6f), juce::Justification::centredRight, true);
        auto bar = barArea (i);
        g.setColour (colour::displayGrid.brighter (0.2f));
        g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
        g.setColour (c);
        const bool bipolar = idx >= 0 && reg.descriptor (idx).minValue < 0.0f && reg.descriptor (idx).maxValue > 0.0f
                             && reg.descriptor (idx).kind == ParamKind::Continuous;
        if (bipolar)
        {
            const float zero = reg.descriptor (idx).toNormalised (0.0f);
            const float a = juce::jmin (zero, norm), b2 = juce::jmax (zero, norm);
            g.fillRoundedRectangle (bar.withX (bar.getX() + bar.getWidth() * a).withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * (b2 - a))), bar.getHeight() * 0.5f);
        }
        else
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * norm)), bar.getHeight() * 0.5f);
        // handle: shows that the bar can be dragged
        const float hx = bar.getX() + bar.getWidth() * norm;
        g.setColour (dragRow == i ? colour::displayText : c.brighter (0.4f));
        g.fillEllipse (juce::Rectangle<float> (bar.getHeight() * 2.2f, bar.getHeight() * 2.2f).withCentre ({ hx, bar.getCentreY() }));
        if (idx >= 0 && proc.learningParamId() == id && std::fmod (time, 0.6f) < 0.3f)
        {
            g.setColour (c);
            g.drawRoundedRectangle (row.expanded (2.0f), 3.0f, 1.5f);
        }
    }
}

void Display::paintHint (juce::Graphics& g, juce::Rectangle<float> s, const juce::String& text)
{
    const bool flashing = flashTime > 0.0f && flashText.isNotEmpty();
    g.setColour (flashing ? colour::sunYellow : colour::displayDim);
    g.setFont (labelFont (juce::jmin (s.getHeight() * 0.8f, (float) getHeight() * 0.04f)));
    g.drawText (flashing ? flashText : text, s, juce::Justification::centredLeft, true);
}

void Display::paintSoftKeys (juce::Graphics& g)
{
    for (int i = 0; i < (int) softKeys.size(); ++i)
    {
        const auto& k = softKeys[(size_t) i];
        auto r = softKeyArea (i);
        g.setColour (k.active ? colour::signalRed : (k.enabled ? colour::displayText : colour::displayGrid.brighter (0.3f)));
        if (k.active) g.fillRoundedRectangle (r, r.getHeight() * 0.25f);
        else g.drawRoundedRectangle (r, r.getHeight() * 0.25f, 1.0f);
        g.setColour (k.active ? colour::displayText : (k.enabled ? colour::displayText : colour::displayDim));
        g.setFont (labelFont (juce::jmin (r.getHeight() * 0.55f, r.getWidth() * 0.16f)));
        g.drawText (i18n::upper (k.label), r.reduced (2.0f, 0), juce::Justification::centred, true);
    }
}

void Display::paintStepStrip (juce::Graphics& g, juce::Rectangle<float> s)
{
    auto& core = proc.core();
    auto& seq = core.stepSequencer();
    const int pos = seq.playPosition();
    const int length = (int) core.params().get ("seq.step.length");
    for (int t = 0; t < 4; ++t)
    {
        auto tab = stepPageTab (t).reduced (2.0f);
        g.setColour (t == stepPage ? colour::displayText : colour::displayDim);
        g.drawRoundedRectangle (tab, 2.0f, 1.0f);
        g.setFont (monoFont (tab.getHeight() * 0.6f));
        g.drawText (juce::String (t + 1), tab, juce::Justification::centred);
    }
    for (int i = 0; i < 16; ++i)
    {
        const int step = stepPage * 16 + i;
        auto cell = stepCell (i).reduced (stepCell (i).getWidth() * 0.12f, 0);
        const auto st = seq.getStep (step);
        const bool inLength = step < length;
        const juce::Colour c = (i / 4) % 2 == 0 ? colour::signalRed : colour::sunYellow;
        if (st.on)
        {
            g.setColour (inLength ? c : c.withAlpha (0.3f));
            g.fillRoundedRectangle (cell, 3.0f);
        }
        else
        {
            g.setColour (inLength ? colour::displayDim : colour::displayGrid);
            g.drawRoundedRectangle (cell, 3.0f, 1.0f);
        }
        // lock indicators: one small bar per used lock slot along the top of the step
        int locks = 0;
        for (const auto& l : st.locks) locks += l.paramIndex >= 0 ? 1 : 0;
        if (locks > 0)
        {
            const float w = cell.getWidth() / 5.0f;
            for (int k = 0; k < locks; ++k)
            {
                g.setColour (st.on ? colour::display : colour::ultraviolet);
                g.fillRect (juce::Rectangle<float> (cell.getX() + w * (0.5f + (float) k), cell.getY() + 2.0f, w * 0.7f, juce::jmax (2.0f, cell.getHeight() * 0.1f)));
            }
        }
        if (step == lockStep)
        {
            g.setColour (colour::displayText);
            g.drawRoundedRectangle (cell.expanded (2.5f), 4.0f, 2.0f);
        }
        if (step == pos)
        {
            g.setColour (colour::displayText);
            g.fillRect (cell.withTop (cell.getBottom() + 3.0f).withHeight (3.0f));
        }
    }
    if (! hasSoftKeys())
        paintHint (g, s.withTop (s.getBottom() - s.getHeight() * 0.16f).withTrimmedLeft (s.getWidth() * 0.12f),
                   tr ("Tap a step to switch it on or off") + dot() + tr ("Shift-tap: edit and lock parameters"));
    else
    {
        paintSoftKeys (g);
        paintHint (g, softKeyRow().withWidth (softKeyArea (0).getX() - softKeyRow().getX() - 4.0f), status);
    }
}

void Display::paintSeqViewStrip (juce::Graphics& g, juce::Rectangle<float> s)
{
    const auto& reg = proc.core().params();
    auto get = [&] (const std::string& id) { return (int) std::lround (reg.get (id)); };
    auto choiceText = [&] (const std::string& id) {
        const int idx = reg.indexOf (id);
        if (idx < 0) return juce::String();
        return formatValue (reg.descriptor (idx), reg.get (idx));
    };
    g.setFont (monoFont (s.getHeight() * 0.16f));
    if (ctx.seqView == SeqView::Euclid || ctx.seqView == SeqView::Orbit)
    {
        const bool euclid = ctx.seqView == SeqView::Euclid;
        const bool on = get (euclid ? "seq.euclid.on" : "seq.orbit.on") != 0;
        const float rowH = s.getHeight() / 4.0f;
        for (int r = 0; r < 4; ++r)
        {
            const auto rp = euclid ? "seq.euclid.r" + std::to_string (r + 1) + "." : "seq.orbit.l" + std::to_string (r + 1) + ".";
            const int steps = juce::jmax (1, get (rp + (euclid ? "steps" : "len")));
            const int pulses = euclid ? get (rp + "pulses") : steps;
            const int rot = euclid ? get (rp + "rot") : 0;
            auto row = juce::Rectangle<float> (s.getX(), s.getY() + rowH * (float) r, s.getWidth(), rowH);
            g.setColour (colour::displayDim);
            g.drawText ((euclid ? "R" : "L") + juce::String (r + 1), row.withWidth (s.getWidth() * 0.06f), juce::Justification::centredLeft);
            auto dots = row.withTrimmedLeft (s.getWidth() * 0.07f);
            const float w = dots.getWidth() / 32.0f;
            const float d = juce::jmin (w * 0.7f, rowH * 0.6f);
            for (int i = 0; i < steps; ++i)
            {
                auto cell = juce::Rectangle<float> (d, d).withCentre ({ dots.getX() + w * ((float) i + 0.5f), row.getCentreY() });
                const bool hit = euclidHit (i, steps, pulses, rot);
                g.setColour (hit ? colour::encoder (r).withAlpha (on ? 1.0f : 0.45f) : colour::displayGrid.brighter (0.25f));
                if (hit) g.fillEllipse (cell); else g.drawEllipse (cell, 1.0f);
            }
        }
        return;
    }
    juce::String line;
    if (ctx.seqView == SeqView::Arp)
        line = i18n::upper (tr ("Arp")) + " " + (get ("seq.arp.on") != 0 ? tr ("On") : tr ("Off")) + dot() + choiceText ("seq.arp.mode") + dot()
               + juce::String (get ("seq.arp.oct")) + " " + tr ("Oct") + dot() + choiceText ("seq.arp.rate") + utf8 ("  \xe2\x86\x92  ")
               + tr ("Part") + " " + juce::String (get ("seq.arp.part"));
    else
        line = tr ("Sync") + ": " + choiceText (Core::Ids::syncSource) + dot() + juce::String (proc.core().transportBpm(), 1) + " BPM"
               + (proc.core().transportPlaying() ? utf8 ("  \xe2\x96\xb6") : juce::String());
    g.setColour (colour::displayText);
    g.setFont (labelFont (s.getHeight() * 0.22f));
    g.drawText (line, s.withTrimmedBottom (s.getHeight() * 0.3f), juce::Justification::centredLeft, true);
}

void Display::paintTapeStrip (juce::Graphics& g, juce::Rectangle<float> s)
{
    juce::ignoreUnused (s);
    auto& core = proc.core();
    auto& tape = core.tape();
    const auto& reg = core.params();
    const double sr = juce::jmax (1.0, tape.sampleRate());
    const int64_t pos = tape.positionFrames();
    const int64_t shownEnd = juce::jmax ((int64_t) (sr * 8.0), juce::jmax (overviewFrames, pos + (int64_t) sr));
    const float loopIn = (float) (reg.get ("tape.loopin") * sr / (double) shownEnd);
    const float loopOut = (float) (reg.get ("tape.loopout") * sr / (double) shownEnd);
    const bool loopOn = reg.get ("tape.loop") >= 0.5f;
    for (int t = 0; t < TapeMachine::numTracks; ++t)
    {
        auto lane = trackLane (t).reduced (0, 1.0f);
        const bool sel = t == ctx.tapeTrack;
        const bool armed = reg.get ("tape.tr" + std::to_string (t + 1) + ".arm") >= 0.5f;
        auto label = lane.withWidth (lane.getHeight() * 2.6f);
        g.setColour (sel ? colour::displayText : colour::displayDim);
        g.setFont (monoFont (lane.getHeight() * 0.8f));
        g.drawText (juce::String (t + 1), label.withWidth (label.getWidth() * 0.5f), juce::Justification::centredLeft);
        auto armDot = juce::Rectangle<float> (lane.getHeight() * 0.6f, lane.getHeight() * 0.6f).withCentre ({ label.getX() + label.getWidth() * 0.72f, lane.getCentreY() });
        g.setColour (armed ? colour::signalRed : colour::displayGrid.brighter (0.3f));
        g.fillEllipse (armDot);
        auto body = lane.withTrimmedLeft (label.getWidth());
        g.setColour (colour::displayGrid.brighter (sel ? 0.35f : 0.15f));
        g.fillRect (body);
        // content overview
        if (overviewFrames > 0)
        {
            const float scale = (float) ((double) overviewFrames / (double) shownEnd);
            const float w = body.getWidth() * scale / (float) overviewPoints;
            g.setColour (colour::encoder (t).withAlpha (sel ? 1.0f : 0.7f));
            for (int i = 0; i < overviewPoints; ++i)
            {
                const float a = overview[(size_t) t][(size_t) i];
                if (a <= 0.001f) continue;
                const float hh = juce::jmax (1.0f, body.getHeight() * juce::jmin (1.0f, 0.15f + a));
                g.fillRect (body.getX() + w * (float) i, body.getCentreY() - hh * 0.5f, juce::jmax (1.0f, w * 0.85f), hh);
            }
        }
        if (sel)
        {
            g.setColour (colour::displayText);
            g.drawRect (lane, 1.0f);
        }
    }
    auto all = trackLane (0).getUnion (trackLane (TapeMachine::numTracks - 1));
    all = all.withTrimmedLeft (trackLane (0).getHeight() * 2.6f);
    if (loopOut > loopIn)
    {
        g.setColour (colour::sunYellow.withAlpha (loopOn ? 0.22f : 0.1f));
        g.fillRect (all.withX (all.getX() + all.getWidth() * juce::jlimit (0.0f, 1.0f, loopIn))
                        .withWidth (all.getWidth() * (juce::jlimit (0.0f, 1.0f, loopOut) - juce::jlimit (0.0f, 1.0f, loopIn))));
    }
    const float px = all.getX() + all.getWidth() * (float) ((double) pos / (double) shownEnd);
    g.setColour (reg.get ("tape.rec") >= 0.5f ? colour::signalRed : colour::displayText);
    g.fillRect (px - 1.0f, all.getY(), 2.0f, all.getHeight());
    paintSoftKeys (g);
    const auto seconds = (double) pos / sr;
    juce::String clock = juce::String ((int) seconds / 60) + ":" + juce::String ((int) seconds % 60).paddedLeft ('0', 2) + "." + juce::String ((int) (seconds * 10.0) % 10);
    if (status.isNotEmpty()) clock << "  " << status;
    paintHint (g, softKeyRow().withWidth (juce::jmax (10.0f, softKeyArea (0).getX() - softKeyRow().getX() - 4.0f)), clock);
}

void Display::paintDrumStrip (juce::Graphics& g, juce::Rectangle<float> s)
{
    auto& kit = proc.core().drumKit();
    const auto& reg = proc.core().params();
    juce::ignoreUnused (s);
    for (int p = 0; p < 24; ++p)
    {
        auto cell = padCell (p);
        const bool sel = p == ctx.drumPad;
        const auto pfx = "drum.pad" + std::to_string (p + 1) + ".";
        juce::String name;
        if (auto smp = kit.effectiveSample (p); smp != nullptr && reg.get (pfx + "src") < 0.5f)
        {
            name = juce::String::fromUTF8 (smp->name().c_str());
            while (name.isNotEmpty() && (juce::CharacterFunctions::isDigit (name[0]) || name[0] == '-' || name[0] == ' ')) name = name.substring (1);
            name = name.upToFirstOccurrenceOf (".", false, false);
        }
        else
        {
            const int idx = reg.indexOf (pfx + "type");
            if (idx >= 0) name = formatValue (reg.descriptor (idx), reg.get (idx));
        }
        g.setColour (sel ? colour::encoder (p % 4) : colour::displayGrid.brighter (0.2f));
        g.fillRoundedRectangle (cell, cell.getHeight() * 0.12f);
        g.setColour (sel ? colour::display : colour::displayText);
        g.setFont (monoFont (cell.getHeight() * 0.3f));
        g.drawText (juce::String (p + 1), cell.reduced (cell.getWidth() * 0.08f, cell.getHeight() * 0.06f), juce::Justification::topLeft);
        g.setColour (sel ? colour::display : colour::displayText.withAlpha (0.72f)); // readable on the grey pad
        g.setFont (labelFont (juce::jmin (cell.getHeight() * 0.26f, cell.getWidth() * 0.2f)));
        g.drawFittedText (i18n::upper (name), cell.reduced (cell.getWidth() * 0.06f, cell.getHeight() * 0.06f).withTrimmedTop (cell.getHeight() * 0.38f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);
    }
    paintSoftKeys (g);
    paintHint (g, softKeyRow().withWidth (juce::jmax (10.0f, softKeyArea (0).getX() - softKeyRow().getX() - 4.0f)),
               status.isNotEmpty() ? status : tr ("Tap a pad to play and edit it"));
}

void Display::paintStrip (juce::Graphics& g, juce::Rectangle<float> s)
{
    auto& core = proc.core();
    g.setColour (colour::displayGrid);
    g.drawLine (s.getX(), s.getY() - 2.0f, s.getRight(), s.getY() - 2.0f, 1.0f);
    if (mode == Mode::Sequencer)
    {
        if (ctx.seqView == SeqView::Step) paintStepStrip (g, s);
        else paintSeqViewStrip (g, s);
        return;
    }
    if (mode == Mode::Tape) { paintTapeStrip (g, s); return; }
    if (mode == Mode::Drum) { paintDrumStrip (g, s); return; }
    if (mode == Mode::Mixer)
    {
        for (int i = 0; i < 8; ++i)
        {
            auto cell = channelCell (i).reduced (channelCell (i).getWidth() * 0.1f, 2.0f);
            const bool sel = i == ctx.mixerChannel;
            g.setColour (sel ? colour::displayText : colour::displayDim);
            g.setFont (monoFont (cell.getHeight() * 0.2f));
            g.drawText (juce::String (i + 1), cell.withHeight (cell.getHeight() * 0.25f), juce::Justification::centredLeft);
            const auto outIdx = (int) core.params().get ("mix.ch" + std::to_string (i + 1) + ".out");
            g.drawText (outIdx == 0 ? i18n::upper (tr ("Main")) : "OUT" + juce::String (outIdx),
                        cell.withHeight (cell.getHeight() * 0.25f), juce::Justification::centredRight);
            auto meter = cell.withTop (cell.getY() + cell.getHeight() * 0.32f);
            g.setColour (colour::displayGrid.brighter (0.15f));
            g.fillRoundedRectangle (meter, 2.0f);
            const float level = juce::jlimit (0.0f, 1.0f, core.mixer().meter (i) * 1.5f);
            g.setColour (sel ? colour::ultraviolet : colour::ultraviolet.withAlpha (0.6f));
            g.fillRoundedRectangle (meter.withTop (meter.getBottom() - meter.getHeight() * level), 2.0f);
            if (sel)
            {
                g.setColour (colour::displayText);
                g.drawRoundedRectangle (cell.expanded (2.0f), 3.0f, 1.0f);
            }
        }
        return;
    }
    // default: 16 voice dots + last note (+ soft keys / status, e.g. the loaded sample)
    auto top = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
    const int voices = core.activeVoices();
    const float d = top.getHeight() * (hasSoftKeys() ? 0.3f : 0.22f);
    for (int i = 0; i < 16; ++i)
    {
        auto dot = juce::Rectangle<float> (d, d).withCentre ({ top.getX() + d + (float) i * d * 1.7f, top.getCentreY() });
        g.setColour (i < voices ? colour::rose : colour::displayGrid.brighter (0.2f));
        g.fillEllipse (dot);
    }
    g.setColour (colour::displayDim);
    g.setFont (labelFont (top.getHeight() * (hasSoftKeys() ? 0.28f : 0.2f)));
    g.drawText (i18n::upper (tr ("Voices")), top.withTrimmedLeft (d * 29.0f), juce::Justification::centredLeft);
    const int note = core.lastNote();
    if (note >= 0)
    {
        g.setColour (colour::displayText);
        g.setFont (brandFont (top.getHeight() * 0.5f));
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4), top, juce::Justification::centredRight);
    }
    if (hasSoftKeys())
    {
        paintSoftKeys (g);
        paintHint (g, softKeyRow().withWidth (juce::jmax (10.0f, softKeyArea (0).getX() - softKeyRow().getX() - 4.0f)), status);
    }
    else if (flashTime > 0.0f)
        paintHint (g, s.withTop (s.getBottom() - s.getHeight() * 0.2f), {});
}
} // namespace ohpe::ui
