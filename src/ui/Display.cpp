// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Display.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

namespace ohpe::ui
{
using i18n::tr;

Display::Display (OhpeProcessor& p) : proc (p) { setOpaque (false); }

void Display::setMode (Mode m, int pg, int numPagesInMode, const std::vector<juce::String>& encoderIds)
{
    mode = m;
    page = pg;
    pages = numPagesInMode;
    ids = encoderIds;
    repaint();
}

void Display::tick (float seconds)
{
    time += seconds;
    const float target = juce::jmin (1.0f, (float) proc.core().activeVoices() / 3.0f);
    energy += (target - energy) * 0.2f;
    repaint();
}

juce::Rectangle<float> Display::headerArea() const
{
    auto b = getLocalBounds().toFloat().reduced (getWidth() * 0.02f);
    return b.withHeight (b.getHeight() * 0.12f);
}

juce::Rectangle<float> Display::stripArea() const
{
    auto b = getLocalBounds().toFloat().reduced (getWidth() * 0.02f);
    return b.withTop (b.getBottom() - b.getHeight() * 0.27f);
}

juce::Rectangle<float> Display::stepCell (int i) const
{
    auto s = stripArea();
    s = s.withTrimmedLeft (s.getWidth() * 0.12f);
    const float w = s.getWidth() / 16.0f;
    // leaves room below for the play marker and the hint line
    return { s.getX() + w * (float) i, s.getY() + s.getHeight() * 0.08f, w, s.getHeight() * 0.62f };
}

juce::Rectangle<float> Display::stepPageTab (int i) const
{
    auto s = stripArea();
    const float w = s.getWidth() * 0.12f / 2.0f;
    return { s.getX() + w * (float) (i % 2), s.getY() + s.getHeight() * (0.08f + 0.32f * (float) (i / 2)), w, s.getHeight() * 0.30f };
}

juce::Rectangle<float> Display::channelCell (int i) const
{
    auto s = stripArea();
    const float w = s.getWidth() / 8.0f;
    return { s.getX() + w * (float) i, s.getY(), w, s.getHeight() };
}

juce::Rectangle<float> Display::pageArrow (bool right) const
{
    auto h = headerArea();
    const float w = h.getHeight() * 1.1f;
    return right ? h.withLeft (h.getRight() - w) : h.withLeft (h.getRight() - w * 3.2f).withWidth (w);
}

void Display::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (pageArrow (false).contains (p)) { if (onPageChange) onPageChange (-1); return; }
    if (pageArrow (true).contains (p)) { if (onPageChange) onPageChange (1); return; }
    if (mode == Mode::Sequencer)
    {
        for (int i = 0; i < 16; ++i)
            if (stepCell (i).contains (p)) { if (onStepClicked) onStepClicked (stepPage * 16 + i); return; }
        for (int i = 0; i < 4; ++i)
            if (stepPageTab (i).contains (p)) { if (onStepPageClicked) onStepPageClicked (i); return; }
    }
    if (mode == Mode::Mixer)
        for (int i = 0; i < 8; ++i)
            if (channelCell (i).contains (p)) { if (onChannelClicked) onChannelClicked (i); return; }
}

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
    const auto type = core.part (0).currentEngineType();
    const auto& eng = engineTypeInfo (type);

    // header
    auto h = headerArea();
    g.setFont (brandFont (h.getHeight() * 0.62f));
    g.setColour (colour::displayText);
    juce::String title = i18n::upper (tr (info.nameEn));
    if (mode == Mode::Synth) title << "  /  " << i18n::upper (i18n::pick (eng.nameEn, eng.nameDe));
    if (mode == Mode::Mixer) title << "  /  " << i18n::upper (tr ("Channel")) << " " << (mixerChannel + 1);
    g.drawText (title, h, juce::Justification::centredLeft);
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

    // middle: pictogram character + encoder rows
    auto inner = glass.reduced (u * 1.4f);
    auto mid = inner.withTop (h.getBottom() + u).withBottom (stripArea().getY() - u);
    auto pictoArea = mid.withWidth (mid.getWidth() * 0.42f);
    PictoState ps;
    ps.large = true;
    ps.time = time;
    ps.energy = energy;
    for (int i = 0; i < 4; ++i)
        if (i < (int) ids.size())
            if (auto* p = proc.parameter (ids[(size_t) i])) ps.macro[i] = p->getValue();
    const bool engineAvailable = eng.implemented;
    const Picto picto = mode == Mode::Synth ? (Picto) (int) type : info.picto;
    const bool dim = (mode == Mode::Synth && ! engineAvailable) || ! info.available;
    drawPictogram (g, pictoArea.reduced (u * 1.5f), picto, ps, dim ? colour::displayDim : colour::displayText);
    if (dim)
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 2.2f));
        g.drawText (tr ("Coming in a later version"), pictoArea.withTop (pictoArea.getBottom() - u * 4), juce::Justification::centred);
    }
    paintRows (g, mid.withTrimmedLeft (pictoArea.getWidth() + u * 2));
    paintStrip (g, stripArea());
}

void Display::paintRows (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float rowH = area.getHeight() / 4.0f;
    const auto& reg = proc.core().params();
    for (int i = 0; i < 4; ++i)
    {
        auto row = juce::Rectangle<float> (area.getX(), area.getY() + rowH * (float) i, area.getWidth(), rowH).reduced (0, rowH * 0.12f);
        const auto c = colour::encoder (i);
        const auto dot = row.withWidth (row.getHeight() * 0.42f).withSizeKeepingCentre (row.getHeight() * 0.42f, row.getHeight() * 0.42f);
        const juce::String id = i < (int) ids.size() ? ids[(size_t) i] : juce::String();
        const int idx = reg.indexOf (id.toStdString());
        g.setColour (idx >= 0 ? c : colour::displayGrid);
        g.fillEllipse (dot);
        if (idx < 0) continue;
        const auto& d = reg.descriptor (idx);
        auto* p = proc.parameter (id);
        const float norm = p != nullptr ? p->getValue() : 0.0f;
        auto textArea = row.withTrimmedLeft (row.getHeight() * 0.7f);
        g.setColour (colour::displayText);
        g.setFont (labelFont (row.getHeight() * 0.42f));
        g.drawText (i18n::pick (d.nameEn, d.nameDe), textArea.withHeight (row.getHeight() * 0.6f), juce::Justification::centredLeft, true);
        g.setFont (monoFont (row.getHeight() * 0.4f));
        g.setColour (c);
        g.drawText (p != nullptr ? formatValue (d, p->convertFrom0to1 (norm)) : juce::String(), textArea.withHeight (row.getHeight() * 0.6f),
                    juce::Justification::centredRight, true);
        auto bar = textArea.withTop (textArea.getY() + row.getHeight() * 0.72f).withHeight (row.getHeight() * 0.12f);
        g.setColour (colour::displayGrid.brighter (0.2f));
        g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
        g.setColour (c);
        if (d.minValue < 0.0f && d.maxValue > 0.0f && d.kind == ParamKind::Continuous)
        {
            const float zero = d.toNormalised (0.0f);
            const float a = juce::jmin (zero, norm), b2 = juce::jmax (zero, norm);
            g.fillRoundedRectangle (bar.withX (bar.getX() + bar.getWidth() * a).withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * (b2 - a))), bar.getHeight() * 0.5f);
        }
        else
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * norm)), bar.getHeight() * 0.5f);
        if (proc.learningParamId() == id && std::fmod (time, 0.6f) < 0.3f)
        {
            g.setColour (c);
            g.drawRoundedRectangle (row.expanded (2.0f), 3.0f, 1.5f);
        }
    }
}

void Display::paintStrip (juce::Graphics& g, juce::Rectangle<float> s)
{
    auto& core = proc.core();
    g.setColour (colour::displayGrid);
    g.drawLine (s.getX(), s.getY() - 2.0f, s.getRight(), s.getY() - 2.0f, 1.0f);
    if (mode == Mode::Sequencer)
    {
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
            if (step == pos)
            {
                g.setColour (colour::displayText);
                g.fillRect (cell.withTop (cell.getBottom() + 3.0f).withHeight (3.0f));
            }
        }
        g.setColour (colour::displayDim);
        g.setFont (labelFont (s.getHeight() * 0.13f));
        g.drawText (tr ("Tap a step to switch it on or off"), s.withTop (s.getBottom() - s.getHeight() * 0.16f).withTrimmedLeft (s.getWidth() * 0.12f),
                    juce::Justification::centredLeft);
        return;
    }
    if (mode == Mode::Mixer)
    {
        for (int i = 0; i < 8; ++i)
        {
            auto cell = channelCell (i).reduced (channelCell (i).getWidth() * 0.1f, 2.0f);
            const bool sel = i == mixerChannel;
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
    // default: 16 voice dots + last note
    const int voices = core.activeVoices();
    const float d = s.getHeight() * 0.22f;
    for (int i = 0; i < 16; ++i)
    {
        auto dot = juce::Rectangle<float> (d, d).withCentre ({ s.getX() + d + (float) i * d * 1.7f, s.getCentreY() });
        g.setColour (i < voices ? colour::rose : colour::displayGrid.brighter (0.2f));
        g.fillEllipse (dot);
    }
    g.setColour (colour::displayDim);
    g.setFont (labelFont (s.getHeight() * 0.2f));
    g.drawText (i18n::upper (tr ("Voices")), s.withTrimmedLeft (d * 29.0f), juce::Justification::centredLeft);
    const int note = core.lastNote();
    if (note >= 0)
    {
        g.setColour (colour::displayText);
        g.setFont (brandFont (s.getHeight() * 0.5f));
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4), s, juce::Justification::centredRight);
    }
}
} // namespace ohpe::ui
