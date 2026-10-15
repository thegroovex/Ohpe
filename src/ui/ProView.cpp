// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ProView.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

namespace ohpe::ui
{
using i18n::tr;

// ---------------------------------------------------------------- ParamBar
ParamBar::ParamBar (OhpeProcessor& p, juce::String paramId, juce::Colour c) : proc (p), id (std::move (paramId)), accent (c)
{
    param = proc.parameter (id);
    index = proc.core().params().indexOf (id.toStdString());
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

juce::Rectangle<float> ParamBar::barBounds() const
{
    auto b = getLocalBounds().toFloat();
    return b.withTop (b.getBottom() - b.getHeight() * 0.26f).withTrimmedBottom (b.getHeight() * 0.08f);
}

void ParamBar::refresh()
{
    const float v = param != nullptr ? param->getValue() : 0.0f;
    if (std::abs (v - shown) > 1.0e-6f) repaint();
}

void ParamBar::paint (juce::Graphics& g)
{
    if (param == nullptr || index < 0) return;
    const auto& d = proc.core().params().descriptor (index);
    shown = param->getValue();
    auto b = getLocalBounds().toFloat();
    const auto text = b.withHeight (b.getHeight() * 0.62f);
    const auto value = formatValue (d, d.fromNormalised (shown));
    g.setFont (monoFont (text.getHeight() * 0.68f));
    const float valueW = juce::jmin (b.getWidth() * 0.45f, juce::GlyphArrangement::getStringWidth (monoFont (text.getHeight() * 0.68f), value) + 6.0f);
    g.setColour (accent);
    g.drawText (value, text, juce::Justification::centredRight, true);
    g.setColour (colour::displayText);
    g.setFont (labelFont (text.getHeight() * 0.7f));
    g.drawText (i18n::pick (d.nameEn, d.nameDe), text.withTrimmedRight (valueW + 4.0f), juce::Justification::centredLeft, true);
    auto bar = barBounds();
    g.setColour (colour::displayGrid.brighter (0.25f));
    g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
    g.setColour (accent);
    if (d.minValue < 0.0f && d.maxValue > 0.0f && d.kind == ParamKind::Continuous)
    {
        const float zero = d.toNormalised (0.0f);
        const float a = juce::jmin (zero, shown), e = juce::jmax (zero, shown);
        g.fillRoundedRectangle (bar.withX (bar.getX() + bar.getWidth() * a).withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * (e - a))), bar.getHeight() * 0.5f);
    }
    else
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * shown)), bar.getHeight() * 0.5f);
    if (proc.core().midiMap().isMapped (id.toStdString()))
    {
        g.setColour (accent);
        g.fillEllipse (b.getX() - 6.0f, text.getCentreY() - 2.0f, 4.0f, 4.0f);
    }
    if (proc.learningParamId() == id)
    {
        g.setColour (accent);
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.2f);
    }
}

void ParamBar::mouseDown (const juce::MouseEvent& e)
{
    if (param == nullptr) return;
    if (e.mods.isPopupMenu()) { showControlMenu (proc, id, this); return; }
    const bool fine = isFineDrag (e.mods);
    float start = param->getValue();
    const auto bar = barBounds();
    if (! fine && e.position.y >= bar.getY() - bar.getHeight() * 1.5f)
        start = juce::jlimit (0.0f, 1.0f, (e.position.x - bar.getX()) / juce::jmax (1.0f, bar.getWidth()));
    param->beginChangeGesture();
    dragging = true;
    drag.begin (start, e.position, fine);
    param->setValueNotifyingHost (start);
    repaint();
}

void ParamBar::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || param == nullptr) return;
    param->setValueNotifyingHost (drag.update (e.position, juce::jmax (40.0f, barBounds().getWidth()), isFineDrag (e.mods)));
    repaint();
}

void ParamBar::mouseUp (const juce::MouseEvent&)
{
    if (dragging && param != nullptr) param->endChangeGesture();
    dragging = false;
}

void ParamBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (param != nullptr && ! e.mods.isPopupMenu()) setParameterNormalised (proc, id, param->getDefaultValue());
    repaint();
}

// ---------------------------------------------------------------- ProView
ProView::ProView (OhpeProcessor& p) : proc (p)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

ProView::~ProView() { viewport.setViewedComponent (nullptr, false); }

float ProView::unit() const { return (float) getWidth() / 114.0f; } // 1140 design px wide -> 10 px

juce::Rectangle<float> ProView::tabArea (int m) const
{
    const float u = unit();
    const float w = u * 10.4f;
    return { u * 2.0f + w * (float) m, u * 1.4f, w - u * 0.6f, u * 3.2f };
}

juce::Rectangle<float> ProView::closeArea() const
{
    const float u = unit();
    return { (float) getWidth() - u * 12.0f, u * 1.4f, u * 10.0f, u * 3.2f };
}

void ProView::setContent (Mode m, const std::vector<ParamGroup>& g, const juce::String& sub)
{
    mode = m;
    subtitle = sub;
    bool same = g.size() == groups.size();
    for (size_t i = 0; same && i < g.size(); ++i) same = g[i].ids == groups[i].ids && g[i].title == groups[i].title && g[i].number == groups[i].number;
    groups = g;
    if (! same)
    {
        bars.clear();
        for (size_t gi = 0; gi < groups.size(); ++gi)
            for (const auto& id : groups[gi].ids)
            {
                if (proc.parameter (juce::String (id)) == nullptr) continue;
                auto b = std::make_unique<ParamBar> (proc, juce::String (id), colour::encoder ((int) gi));
                content.addAndMakeVisible (*b);
                bars.push_back (std::move (b));
            }
        viewport.setViewPosition (0, 0);
    }
    layoutContent();
    repaint();
}

void ProView::refresh()
{
    for (auto& b : bars) b->refresh();
}

void ProView::resized()
{
    const float u = unit();
    viewport.setBounds (getLocalBounds().withTrimmedTop ((int) (u * 8.6f)).reduced ((int) (u * 2.0f), (int) (u * 0.6f)));
    layoutContent();
}

void ProView::layoutContent()
{
    const float u = unit();
    const int width = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
    columns = juce::jmax (1, (int) ((float) width / (u * 27.0f)));
    const int colW = width / columns;
    const int rowH = (int) (u * 3.4f), titleH = (int) (u * 3.0f), gap = (int) (u * 1.6f);
    std::vector<int> colY ((size_t) columns, 0);
    groupBoxes.clear();
    size_t barIndex = 0;
    for (const auto& grp : groups)
    {
        int n = 0;
        for (const auto& id : grp.ids)
            if (proc.parameter (juce::String (id)) != nullptr) ++n;
        // the shortest column takes the next group
        const auto col = (size_t) std::distance (colY.begin(), std::min_element (colY.begin(), colY.end()));
        const int x = (int) col * colW;
        const int h = titleH + n * rowH;
        groupBoxes.push_back ({ x + (int) (u * 0.8f), colY[col], colW - (int) (u * 1.6f), h + (int) (u * 0.8f) });
        for (int k = 0; k < n && barIndex < bars.size(); ++k, ++barIndex)
            bars[barIndex]->setBounds (x + (int) (u * 1.8f), colY[col] + titleH + k * rowH, colW - (int) (u * 3.6f), rowH - (int) (u * 0.4f));
        colY[col] += h + gap;
    }
    content.setSize (width, juce::jmax (viewport.getHeight(), *std::max_element (colY.begin(), colY.end())));
    content.repaint();
}

void ProView::Content::paint (juce::Graphics& g)
{
    const float u = owner.unit();
    for (size_t i = 0; i < owner.groupBoxes.size() && i < owner.groups.size(); ++i)
    {
        const auto box = owner.groupBoxes[i].toFloat();
        g.setColour (colour::displayGrid);
        g.fillRoundedRectangle (box, u * 0.8f);
        const auto& grp = owner.groups[i];
        juce::String title = i18n::upper (tr (grp.title));
        if (grp.number > 0) title << " " << grp.number;
        g.setColour (colour::encoder ((int) i));
        g.fillRect (box.getX() + u, box.getY() + u * 1.0f, u * 0.5f, u * 1.4f);
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.5f));
        g.drawText (title, box.withHeight (u * 3.0f).withTrimmedLeft (u * 2.0f).withTrimmedRight (u), juce::Justification::centredLeft, true);
    }
}

void ProView::paint (juce::Graphics& g)
{
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    g.setColour (colour::bodyShade);
    g.fillRoundedRectangle (b, u * 1.4f);
    g.setColour (colour::display);
    g.fillRoundedRectangle (b.reduced (u * 0.5f), u * 1.1f);
    for (int m = 0; m < numModes; ++m)
    {
        const auto r = tabArea (m);
        const bool sel = (int) mode == m;
        g.setColour (sel ? colour::displayText : colour::displayGrid.brighter (0.3f));
        if (sel) g.fillRoundedRectangle (r, u * 0.6f);
        else g.drawRoundedRectangle (r, u * 0.6f, 1.0f);
        g.setColour (sel ? colour::display : colour::displayDim);
        g.setFont (labelFont (u * 1.5f));
        g.drawText (i18n::upper (tr (modeInfo ((Mode) m).nameEn)), r, juce::Justification::centred, true);
    }
    const auto c = closeArea();
    g.setColour (colour::signalRed);
    g.fillRoundedRectangle (c, u * 0.6f);
    g.setColour (colour::displayText);
    g.setFont (labelFont (u * 1.5f));
    g.drawText (i18n::upper (tr ("Close")), c, juce::Justification::centred, true);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.4f));
    g.drawText (i18n::upper (tr ("Pro view")) + (subtitle.isNotEmpty() ? "  /  " + subtitle : juce::String()),
                juce::Rectangle<float> (u * 2.0f, u * 5.2f, b.getWidth() - u * 4.0f, u * 2.6f), juce::Justification::centredLeft, true);
}

void ProView::mouseDown (const juce::MouseEvent& e)
{
    if (closeArea().contains (e.position)) { if (onClose) onClose(); return; }
    for (int m = 0; m < numModes; ++m)
        if (tabArea (m).contains (e.position)) { if (onModeSelected) onModeSelected ((Mode) m); return; }
}
} // namespace ohpe::ui
