// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ProView.h"

#include "Palette.h"
#include "mod/ModMatrix.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

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

juce::String ParamBar::getTooltip() { return pro::hoverText (proc, id.toStdString()); }

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
    // p21: what the matrix adds around the value (violet: something else moves it)
    const int barPart = id.startsWithChar ('p') && id.length() > 2 && id[2] == '.' ? id.substring (1, 2).getIntValue() - 1 : -1;
    if (const auto mr = barPart >= 0 && barPart < Core::numParts ? pro::modRange (proc, barPart, id.toStdString()) : juce::Range<float>();
        ! mr.isEmpty())
    {
        const float lo = juce::jlimit (0.0f, 1.0f, shown + mr.getStart()), hi = juce::jlimit (0.0f, 1.0f, shown + mr.getEnd());
        const auto band = bar.withX (bar.getX() + bar.getWidth() * lo).withWidth (juce::jmax (2.0f, bar.getWidth() * (hi - lo)));
        g.setColour (colour::ultraviolet.withAlpha (0.55f));
        g.fillRoundedRectangle (band, bar.getHeight() * 0.5f);
        g.setColour (colour::ultraviolet);
        g.fillRect (bar.getX() + bar.getWidth() * shown - 1.0f, bar.getY() - 1.0f, 2.0f, bar.getHeight() + 2.0f);
    }
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
    if (dragging && param != nullptr)
    {
        param->endChangeGesture();
        if (onEdited) onEdited();
    }
    dragging = false;
}

void ParamBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (param != nullptr && ! e.mods.isPopupMenu()) setParameterNormalised (proc, id, param->getDefaultValue());
    repaint();
}

// ---------------------------------------------------------------- ProView
ProView::ProView (OhpeProcessor& p) : proc (p), strip (p)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    addChildComponent (strip);
    strip.isLocked = [this] (const std::string& id) {
        for (const auto& c : cards)
            if (c.partSound && locked.count (cardKey (c)) > 0 && std::find (c.group.ids.begin(), c.group.ids.end(), id) != c.group.ids.end()) return true;
        return false;
    };
    strip.onUnlockAll = [this] { locked.clear(); content.repaint(); };
    for (int s : { (int) mod::SrcLfo1, (int) mod::SrcLfo2, (int) mod::SrcLfo3, (int) mod::SrcModEnv, (int) mod::SrcEnv3, (int) mod::SrcModWheel })
    {
        auto c = std::make_unique<pro::SourceChip> (proc, s);
        addChildComponent (*c);
        chips.push_back (std::move (c));
    }
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

juce::Rectangle<float> ProView::simpleToggleArea() const
{
    const float u = unit();
    return { (float) getWidth() - u * 19.0f, u * 5.3f, u * 17.0f, u * 2.4f };
}

bool ProView::partMode() const noexcept { return mode == Mode::Synth || mode == Mode::Envelope || mode == Mode::Lfo || mode == Mode::Effects; }

std::string ProView::cardKey (const Card& c) const { return c.group.title + "#" + std::to_string (c.group.number); }

bool ProView::anyTile() const noexcept
{
    for (const auto& c : cards)
        if (c.tile != nullptr) return true;
    return false;
}

int ProView::numTiles() const noexcept
{
    int n = 0;
    for (const auto& c : cards)
        if (c.tile != nullptr) ++n;
    return n;
}

pro::Tile* ProView::tile (int card) { return card >= 0 && card < (int) cards.size() ? cards[(size_t) card].tile.get() : nullptr; }

int ProView::cardIndex (const std::string& title, int number) const
{
    for (size_t i = 0; i < cards.size(); ++i)
        if (cards[i].group.title == title && cards[i].group.number == number) return (int) i;
    return -1;
}

juce::Rectangle<int> ProView::cardBounds (int card) const { return card >= 0 && card < (int) cards.size() ? cards[(size_t) card].box : juce::Rectangle<int>(); }

std::vector<std::string> ProView::soundIds() const
{
    PageContext ctx;
    ctx.part = part;
    std::vector<std::string> v;
    const std::string prefix = "p" + std::to_string (part + 1) + ".";
    for (const auto& g : proGroups (Mode::Synth, proc.core(), ctx))
        for (const auto& id : g.ids)
            if (id.rfind (prefix, 0) == 0 && discover::isBreedable (id.substr (prefix.size()))
                && std::find (v.begin(), v.end(), id) == v.end())
                v.push_back (id);
    return v;
}

void ProView::setContent (Mode m, const std::vector<ParamGroup>& g, const juce::String& sub, int partIndex)
{
    const int newPart = juce::jlimit (0, Core::numParts - 1, partIndex);
    const auto newEngine = selectedEngine (proc.core(), newPart);
    const bool modeChanged = m != mode || newPart != part || newEngine != engine;
    mode = m;
    part = newPart;
    engine = newEngine;
    subtitle = sub;
    bool same = ! modeChanged && g.size() == groups.size();
    for (size_t i = 0; same && i < g.size(); ++i) same = g[i].ids == groups[i].ids && g[i].title == groups[i].title && g[i].number == groups[i].number;
    groups = g;
    if (! same)
    {
        bars.clear();
        cards.clear();
        for (size_t gi = 0; gi < groups.size(); ++gi)
        {
            Card c;
            c.group = groups[gi];
            c.firstBar = bars.size();
            for (const auto& id : groups[gi].ids)
            {
                if (proc.parameter (juce::String (id)) == nullptr) continue;
                auto b = std::make_unique<ParamBar> (proc, juce::String (id), colour::sunYellow);
                b->onEdited = [this] { recordState(); };
                content.addAndMakeVisible (*b);
                bars.push_back (std::move (b));
            }
            c.numBars = bars.size() - c.firstBar;
            c.tile = pro::makeTile (proc, mode, part, c.group);
            if (c.tile != nullptr)
            {
                c.tile->onEdited = [this] { recordState(); };
                content.addAndMakeVisible (*c.tile);
            }
            if (mode == Mode::Synth || mode == Mode::Envelope || mode == Mode::Lfo)
            {
                const std::string prefix = "p" + std::to_string (part + 1) + ".";
                for (const auto& id : c.group.ids)
                    if (id.rfind (prefix, 0) == 0 && discover::isBreedable (id.substr (prefix.size()))) { c.partSound = true; break; }
            }
            cards.push_back (std::move (c));
        }
        viewport.setViewPosition (0, 0);
    }
    discoverShown = partMode();
    strip.setPart (part, soundIds());
    strip.setVisible (discoverShown);
    for (auto& c : chips)
    {
        c->setPart (part);
        c->setVisible (discoverShown);
    }
    resized();
    repaint();
}

void ProView::recordState()
{
    if (discoverShown) strip.recordState();
}

void ProView::setSimple (bool s, bool notify)
{
    if (s == simple) return;
    simple = s;
    layoutContent();
    repaint();
    if (notify && onSettingsChanged) onSettingsChanged();
}

void ProView::setCollapsed (int card, bool c)
{
    if (card < 0 || card >= (int) cards.size()) return;
    const auto k = cardKey (cards[(size_t) card]);
    if (c) collapsed.insert (k);
    else collapsed.erase (k);
    layoutContent();
}

bool ProView::isCollapsed (int card) const
{
    return card >= 0 && card < (int) cards.size() && collapsed.count (cardKey (cards[(size_t) card])) > 0;
}

void ProView::setLocked (int card, bool l)
{
    if (card < 0 || card >= (int) cards.size()) return;
    const auto k = cardKey (cards[(size_t) card]);
    if (l) locked.insert (k);
    else locked.erase (k);
    content.repaint();
    strip.repaint();
}

bool ProView::isLocked (int card) const
{
    return card >= 0 && card < (int) cards.size() && locked.count (cardKey (cards[(size_t) card])) > 0;
}

void ProView::rollCard (int card)
{
    if (card < 0 || card >= (int) cards.size() || ! cards[(size_t) card].partSound || isLocked (card)) return;
    strip.roll (cards[(size_t) card].group.ids);
}

void ProView::refresh() { tick (1.0f / 30.0f); }

void ProView::tick (float seconds)
{
    // the matrix of this part changed (a route added / removed / its amount): the violet ranges on the bars move
    {
        const auto& reg = proc.core().params();
        const std::string pre = "p" + std::to_string (part + 1) + ".mod.s";
        uint64_t sig = 1469598103934665603ull;
        for (int k = 1; k <= mod::numSlots; ++k)
            for (const char* f : { "src", "dst", "amt" })
            {
                const float v = reg.get (pre + std::to_string (k) + "." + f);
                uint32_t bits = 0;
                std::memcpy (&bits, &v, sizeof bits);
                sig = (sig ^ bits) * 1099511628211ull;
            }
        if (sig != routingSig)
        {
            routingSig = sig;
            for (auto& b : bars) b->repaint();
            content.repaint();
        }
    }
    for (auto& b : bars)
        if (b->isVisible()) b->refresh();
    for (auto& c : cards)
        if (c.tile != nullptr && c.tile->isVisible()) c.tile->tick (seconds);
    if (discoverShown)
    {
        strip.tick (seconds);
        for (auto& c : chips) c->repaint();
    }
}

void ProView::resized()
{
    const float u = unit();
    const float stripH = u * 9.6f;
    int top = (int) (u * 8.6f);
    if (discoverShown)
    {
        strip.setBounds (juce::Rectangle<float> (u * 2.0f, u * 8.2f, (float) getWidth() - u * 4.0f, stripH).toNearestInt());
        top = (int) (u * 8.2f + stripH + u * 0.8f);
        // the source chips on the DISCOVER line, left of the toggle
        float x = simpleToggleArea().getX() - u * 1.0f;
        for (auto it = chips.rbegin(); it != chips.rend(); ++it)
        {
            const float w = u * 7.2f;
            x -= w;
            (*it)->setBounds (juce::Rectangle<float> (x, u * 5.5f, w - u * 0.6f, u * 2.0f).toNearestInt());
        }
    }
    viewport.setBounds (getLocalBounds().withTrimmedTop (top).reduced ((int) (u * 2.0f), (int) (u * 0.6f)));
    layoutContent();
}

juce::Rectangle<float> ProView::chevronArea (const Card& c) const
{
    const float u = unit();
    const auto b = c.box.toFloat();
    return { b.getRight() - u * 3.0f, b.getY() + u * 0.5f, u * 2.4f, u * 2.0f };
}

juce::Rectangle<float> ProView::lockArea (const Card& c) const
{
    const float u = unit();
    return chevronArea (c).translated (-u * 2.6f, 0.0f);
}

juce::Rectangle<float> ProView::diceArea (const Card& c) const
{
    const float u = unit();
    return lockArea (c).translated (-u * 2.6f, 0.0f);
}

void ProView::layoutContent()
{
    const float u = unit();
    const int width = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
    columns = juce::jlimit (1, 4, (int) ((float) width / juce::jmax (u * 27.0f, 250.0f))); // p21: 1 - 4 columns by width
    const int colW = width / columns;
    const int rowH = (int) (u * 3.4f), titleH = (int) (u * 3.0f), gap = (int) (u * 1.6f);
    const bool simpleNow = simple && anyTile(); // a mode without pictures shows its bars in Simple too
    std::vector<int> colY ((size_t) columns, 0);
    for (auto& c : cards)
    {
        const bool showBars = ! simpleNow && collapsed.count (cardKey (c)) == 0;
        c.visible = ! simpleNow || c.tile != nullptr;
        if (! c.visible)
        {
            for (size_t k = 0; k < c.numBars; ++k) bars[c.firstBar + k]->setVisible (false);
            if (c.tile != nullptr) c.tile->setVisible (false);
            c.box = {};
            continue;
        }
        const auto col = (size_t) std::distance (colY.begin(), std::min_element (colY.begin(), colY.end()));
        const int x = (int) col * colW;
        int tileH = 0;
        if (c.tile != nullptr)
        {
            const auto& t = c.group.title;
            float f = 11.0f;
            if (t == "Engine") f = 16.0f;
            else if (t == "LFO" || t == "LFO 2" || t == "LFO 3") f = 9.0f;
            else if (t == "Pitch & Level") f = 10.0f;
            else if (t == "Sub") f = 8.0f; // p31
            else if (t == "3D Space") f = 13.0f;
            else if (mode == Mode::Effects) f = 7.0f;
            tileH = (int) (u * f);
        }
        const int n = showBars ? (int) c.numBars : 0;
        const int h = titleH + (tileH > 0 ? tileH + (int) (u * 0.6f) : 0) + n * rowH;
        c.box = { x + (int) (u * 0.8f), colY[col], colW - (int) (u * 1.6f), h + (int) (u * 0.8f) };
        if (c.tile != nullptr)
        {
            c.tile->setVisible (true);
            c.tile->setBounds (x + (int) (u * 1.6f), colY[col] + titleH, colW - (int) (u * 3.2f), tileH);
        }
        const int barTop = colY[col] + titleH + (tileH > 0 ? tileH + (int) (u * 0.6f) : 0);
        for (size_t k = 0; k < c.numBars; ++k)
        {
            auto& b = *bars[c.firstBar + k];
            b.setVisible (showBars);
            if (showBars) b.setBounds (x + (int) (u * 1.8f), barTop + (int) k * rowH, colW - (int) (u * 3.6f), rowH - (int) (u * 0.4f));
        }
        colY[col] += h + gap;
    }
    content.setSize (width, juce::jmax (viewport.getHeight(), *std::max_element (colY.begin(), colY.end())));
    content.repaint();
}

void ProView::Content::paint (juce::Graphics& g)
{
    const float u = owner.unit();
    for (const auto& c : owner.cards)
    {
        if (! c.visible || c.box.isEmpty()) continue;
        const auto box = c.box.toFloat();
        g.setColour (colour::displayGrid);
        g.fillRoundedRectangle (box, u * 0.8f);
        juce::String title = i18n::upper (tr (c.group.title));
        if (c.group.title == "Engine" && owner.mode == Mode::Synth)
            title = i18n::upper (i18n::pick (engineTypeInfo (owner.engine).nameEn, engineTypeInfo (owner.engine).nameDe)) + " " + i18n::upper (tr ("Engine"));
        if (c.group.number > 0) title << " " << c.group.number;
        // the marker: yellow = edit, violet = something modulates this group
        bool modulated = false;
        if (c.tile != nullptr)
            for (const auto& id : c.tile->paramIds())
                if (! id.empty() && ! pro::modRange (owner.proc, owner.part, id).isEmpty()) { modulated = true; break; }
        g.setColour (modulated ? colour::ultraviolet : colour::sunYellow);
        g.fillRoundedRectangle (box.getX() + u, box.getY() + u * 0.8f, u * 0.5f, u * 1.4f, u * 0.2f);
        g.setColour (colour::displayText.withAlpha (0.85f));
        g.setFont (labelFont (u * 1.5f));
        g.drawText (title, box.withHeight (u * 3.0f).withTrimmedLeft (u * 2.0f).withTrimmedRight (u * 9.0f), juce::Justification::centredLeft, true);
        // chevron: collapse the bars
        if (c.numBars > 0 && ! (owner.simple && owner.anyTile()))
        {
            const auto ch = owner.chevronArea (c);
            const bool col = owner.collapsed.count (owner.cardKey (c)) > 0;
            juce::Path p;
            const auto cc = ch.getCentre();
            const float s = u * 0.55f;
            if (col) { p.startNewSubPath (cc.x - s * 0.6f, cc.y - s); p.lineTo (cc.x + s * 0.6f, cc.y); p.lineTo (cc.x - s * 0.6f, cc.y + s); }
            else { p.startNewSubPath (cc.x - s, cc.y - s * 0.5f); p.lineTo (cc.x, cc.y + s * 0.5f); p.lineTo (cc.x + s, cc.y - s * 0.5f); }
            g.setColour (colour::displayDim);
            g.strokePath (p, juce::PathStrokeType (u * 0.25f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }
        if (c.partSound)
        {
            const bool lk = owner.locked.count (owner.cardKey (c)) > 0;
            // dice
            const auto d = owner.diceArea (c).reduced (u * 0.25f);
            g.setColour (lk ? colour::displayGrid.brighter (0.5f) : colour::displayDim.brighter (0.3f));
            g.drawRoundedRectangle (d, u * 0.35f, u * 0.15f);
            const float ps = d.getWidth() * 0.22f, pr = u * 0.18f;
            for (auto off : { juce::Point<float> (-ps, -ps), juce::Point<float> (ps, ps), juce::Point<float> (0, 0) })
                g.fillEllipse (juce::Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (d.getCentre() + off));
            // lock
            const auto l = owner.lockArea (c).reduced (u * 0.45f, u * 0.25f);
            g.setColour (lk ? colour::sunYellow : colour::displayDim);
            const auto body = l.withTrimmedTop (l.getHeight() * 0.45f);
            g.fillRoundedRectangle (body, u * 0.15f);
            juce::Path sh;
            sh.addCentredArc (body.getCentreX(), body.getY(), body.getWidth() * 0.3f, l.getHeight() * 0.32f, 0.0f, -juce::MathConstants<float>::halfPi,
                              juce::MathConstants<float>::halfPi, true);
            g.strokePath (sh, juce::PathStrokeType (u * 0.18f));
        }
    }
}

void ProView::Content::mouseDown (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < owner.cards.size(); ++i)
    {
        const auto& c = owner.cards[i];
        if (! c.visible) continue;
        if (c.numBars > 0 && owner.chevronArea (c).contains (e.position)) { owner.setCollapsed ((int) i, ! owner.isCollapsed ((int) i)); return; }
        if (c.partSound && owner.lockArea (c).contains (e.position)) { owner.setLocked ((int) i, ! owner.isLocked ((int) i)); return; }
        if (c.partSound && owner.diceArea (c).contains (e.position)) { owner.rollCard ((int) i); return; }
    }
}

void ProView::paint (juce::Graphics& g)
{
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colour::bezel, 0, b.getY(), colour::bezelLow, 0, b.getBottom(), false)); // p14: dark frame
    const float du = (float) getWidth() / 1152.0f; // p14: design units (the overlay is 1152 wide)
    g.fillRoundedRectangle (b, radius::panel * du);
    g.setColour (colour::display);
    g.fillRoundedRectangle (b.reduced (u * 0.5f), radius::inset * du);
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
    // second line: DISCOVER (part modes) or PRO VIEW, the subtitle, the source chips, SIMPLE | ALL
    const auto line = juce::Rectangle<float> (u * 2.0f, u * 5.2f, b.getWidth() - u * 4.0f, u * 2.6f);
    float textRight = simpleToggleArea().getX() - u * 1.0f;
    if (discoverShown && ! chips.empty()) textRight = (float) chips.front()->getX() - u * 1.0f;
    juce::String head = discoverShown ? i18n::upper (tr ("Discover")) : i18n::upper (tr ("Pro view"));
    g.setColour (discoverShown ? colour::displayText : colour::displayDim);
    g.setFont (labelFont (u * 1.5f));
    const float headW = juce::GlyphArrangement::getStringWidth (labelFont (u * 1.5f), head) + u * 1.5f;
    g.drawText (head, line.withWidth (headW), juce::Justification::centredLeft, false);
    if (subtitle.isNotEmpty())
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.25f));
        g.drawText ("/  " + subtitle, juce::Rectangle<float> (line.getX() + headW, line.getY(), juce::jmax (0.0f, textRight - line.getX() - headW), line.getHeight()),
                    juce::Justification::centredLeft, true);
    }
    // SIMPLE | ALL
    const auto t = simpleToggleArea();
    const auto left = t.withWidth (t.getWidth() * 0.5f), right = t.withTrimmedLeft (t.getWidth() * 0.5f);
    g.setColour (colour::displayGrid.brighter (0.3f));
    g.drawRoundedRectangle (t, t.getHeight() * 0.5f, 1.0f);
    g.setColour (colour::displayText);
    g.fillRoundedRectangle ((simple ? left : right).reduced (u * 0.2f), t.getHeight() * 0.45f);
    g.setFont (labelFont (u * 1.2f));
    g.setColour (simple ? colour::display : colour::displayDim);
    g.drawText (i18n::upper (tr ("Simple")), left, juce::Justification::centred, false);
    g.setColour (simple ? colour::displayDim : colour::display);
    g.drawText (i18n::upper (i18n::pick ("All", "Alles")), right, juce::Justification::centred, false);
}

void ProView::mouseDown (const juce::MouseEvent& e)
{
    if (closeArea().contains (e.position)) { if (onClose) onClose(); return; }
    if (simpleToggleArea().contains (e.position))
    {
        setSimple (e.position.x < simpleToggleArea().getCentreX());
        return;
    }
    for (int m = 0; m < numModes; ++m)
        if (tabArea (m).contains (e.position)) { if (onModeSelected) onModeSelected ((Mode) m); return; }
}
} // namespace ohpe::ui
