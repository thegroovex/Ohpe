// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TempoControl.h"

#include "plugin/Localisation.h"

#include <cmath>

namespace ohpe::ui
{
using i18n::tr;

void setParamFromScreen (OhpeProcessor& proc, const char* id, float plain)
{
    if (auto* p = proc.parameter (juce::String (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
    proc.core().params().set (id, plain);
}

void drawMetronomeSign (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour ink, float swing)
{
    const float s = juce::jmin (a.getWidth(), a.getHeight());
    auto r = a.withSizeKeepingCentre (s * 0.8f, s);
    const float stroke = juce::jmax (1.0f, s * 0.085f);
    juce::Path body; // a pyramid: narrow top, wide foot
    body.startNewSubPath (r.getX() + r.getWidth() * 0.36f, r.getY() + s * 0.06f);
    body.lineTo (r.getRight() - r.getWidth() * 0.36f, r.getY() + s * 0.06f);
    body.lineTo (r.getRight() - r.getWidth() * 0.06f, r.getBottom() - s * 0.06f);
    body.lineTo (r.getX() + r.getWidth() * 0.06f, r.getBottom() - s * 0.06f);
    body.closeSubPath();
    g.setColour (ink);
    g.strokePath (body, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // the pendulum, pivoting low in the body
    const juce::Point<float> pivot (r.getCentreX(), r.getBottom() - s * 0.24f);
    const float angle = juce::jlimit (-1.0f, 1.0f, swing) * 0.55f;
    const juce::Point<float> tip (pivot.x + std::sin (angle) * s * 0.62f, pivot.y - std::cos (angle) * s * 0.62f);
    g.drawLine ({ pivot, tip }, stroke);
    g.fillEllipse (juce::Rectangle<float> (stroke * 2.4f, stroke * 2.4f).withCentre (pivot + (tip - pivot) * 0.62f));
}

namespace
{
void paintGlass (juce::Graphics& g, juce::Rectangle<float> bounds, float u)
{
    const float r = radius::inset * u;
    g.setColour (colour::sheen.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds.translated (0, 1.0f), r);
    auto glass = bounds.withTrimmedBottom (1.0f);
    g.setColour (colour::display);
    g.fillRoundedRectangle (glass, r);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (glass.reduced (0.5f), r, 1.0f);
}
} // namespace

// ---------------------------------------------------------------- tempo readout
TempoReadout::TempoReadout (OhpeProcessor& p, Style s) : proc (p), style (s)
{
    setRepaintsOnMouseActivity (true);
    entry.setJustification (juce::Justification::centred);
    entry.setInputRestrictions (6, "0123456789.,");
    entry.setColour (juce::TextEditor::backgroundColourId, colour::display);
    entry.setColour (juce::TextEditor::textColourId, colour::displayText);
    entry.setColour (juce::TextEditor::outlineColourId, colour::sunYellow);
    entry.setColour (juce::TextEditor::focusedOutlineColourId, colour::sunYellow);
    entry.setColour (juce::TextEditor::highlightColourId, colour::sunYellow.withAlpha (0.4f));
    entry.setColour (juce::CaretComponent::caretColourId, colour::sunYellow);
    entry.onReturnKey = [this] { commitText (entry.getText()); hideTextEntry(); };
    entry.onEscapeKey = [this] { hideTextEntry(); };
    entry.onFocusLost = [this] { hideTextEntry(); };
    addChildComponent (entry);
}

juce::String TempoReadout::getTooltip()
{
    return tr ("Tempo: drag up / down (Shift: fine), double-click to type, mouse wheel; right-click: MIDI learn");
}

juce::String TempoReadout::shownText() const
{
    return juce::String (proc.currentTempo(), 1);
}

void TempoReadout::refresh()
{
    const float bpm = proc.currentTempo();
    const bool locked = proc.tempoFollowsHost();
    const bool running = proc.core().transportPlaying();
    if (std::abs (bpm - shownBpm) > 0.04f || locked != shownLocked || running != shownRunning)
    {
        shownBpm = bpm;
        shownLocked = locked;
        shownRunning = running;
        repaint();
    }
}

void TempoReadout::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const bool locked = proc.tempoFollowsHost();
    const juce::String tag = proc.hostTempoActive() ? tr ("Host") : tr ("Clock");
    const juce::String value = juce::String (proc.currentTempo(), 1);
    const bool hover = isMouseOverOrDragging() && ! locked;
    if (style == Style::Header)
    {
        // as before: dim mono text on the header, a play sign while the sequencer runs; hover / drag: yellow (edit)
        const float h = b.getHeight();
        juce::String text = value + " BPM";
        if (proc.core().transportPlaying()) text = juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 ")) + text;
        if (locked) text = i18n::upper (tag) + "  " + text;
        g.setFont (monoFont (h * textSize));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
        if (hover || dragging)
        {
            g.setColour (colour::sunYellow.withAlpha (0.16f));
            g.fillRoundedRectangle (b.withLeft (b.getRight() - w - h * 0.5f).reduced (0.0f, h * 0.12f), h * 0.2f);
        }
        g.setColour (dragging ? colour::sunYellow : (hover ? colour::displayText : colour::displayDim));
        g.drawText (text, b.withTrimmedRight (h * 0.25f), juce::Justification::centredRight, false);
        return;
    }
    // panel: a glass like the tape readout - small legend over the value
    const float u = uiScale;
    paintGlass (g, b, u);
    auto in = b.withTrimmedBottom (1.0f).reduced (6.0f * u, 4.0f * u);
    auto top = in.removeFromTop (in.getHeight() * 0.42f);
    g.setColour (locked ? colour::ultraviolet.brighter (0.3f) : (dragging || hover ? colour::sunYellow : colour::displayDim));
    drawFittedLegend (g, locked ? i18n::upper (tag) : juce::String ("BPM"), top, juce::jmin (top.getHeight() * 0.9f, 10.0f * u),
                      juce::Justification::centredLeft);
    g.setColour (dragging ? colour::sunYellow : colour::displayText);
    g.setFont (monoFont (juce::jmin (in.getHeight() * 0.85f, 14.0f * u)));
    g.drawFittedText (value, in.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
}

void TempoReadout::resized()
{
    entry.setBounds (getLocalBounds().reduced (1));
    entry.setFont (monoFont (juce::jmax (9.0f, getHeight() * 0.55f)));
}

void TempoReadout::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showControlMenu (proc, Core::Ids::tempo, this);
        return;
    }
    if (proc.tempoFollowsHost()) return;
    dragStartBpm = proc.currentTempo();
    dragging = true;
    repaint();
}

void TempoReadout::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || proc.tempoFollowsHost()) return;
    const float dy = (float) -e.getDistanceFromDragStartY();
    const float step = isFineDrag (e.mods) ? 0.025f : 0.25f; // BPM per pixel
    float v = dragStartBpm + dy * step;
    v = isFineDrag (e.mods) ? std::round (v * 10.0f) / 10.0f : std::round (v);
    if (std::abs (v - proc.currentTempo()) > 0.01f) proc.setTempoFromUi (v);
    repaint();
}

void TempoReadout::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

void TempoReadout::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) showTextEntry();
}

void TempoReadout::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (proc.tempoFollowsHost() || w.deltaY == 0.0f) return;
    const float step = isFineDrag (e.mods) ? 0.1f : 1.0f;
    proc.setTempoFromUi (proc.currentTempo() + (w.deltaY > 0.0f ? step : -step));
    repaint();
}

void TempoReadout::showTextEntry()
{
    if (proc.tempoFollowsHost()) return;
    entry.setText (juce::String (proc.currentTempo(), 1), false);
    entry.setVisible (true);
    entry.selectAll();
    entry.grabKeyboardFocus();
}

void TempoReadout::hideTextEntry()
{
    if (entry.isVisible()) entry.setVisible (false);
    repaint();
}

bool TempoReadout::commitText (const juce::String& text)
{
    const auto t = text.trim().replaceCharacter (',', '.');
    if (t.isEmpty() || ! t.containsOnly ("0123456789.")) return false;
    const float v = t.getFloatValue();
    if (! (v >= 20.0f && v <= 300.0f) || proc.tempoFollowsHost()) return false;
    proc.setTempoFromUi (v);
    repaint();
    return true;
}

// ---------------------------------------------------------------- click / count-in strip
ClickStrip::ClickStrip (OhpeProcessor& p) : proc (p) {}

juce::Rectangle<float> ClickStrip::fieldArea (int f) const
{
    const auto b = getLocalBounds().toFloat().withTrimmedBottom (1.0f);
    const float w = b.getWidth() / (float) numFields;
    return b.withX (b.getX() + w * (float) f).withWidth (w);
}

int ClickStrip::fieldAt (juce::Point<float> p) const
{
    for (int f = 0; f < numFields; ++f)
        if (fieldArea (f).contains (p)) return f;
    return -1;
}

std::vector<tips::Region> ClickStrip::tipRegions()
{
    return { { fieldArea (ClickField), tr ("Metronome: click on / off while the sequencer or the tape runs; right-click: level, accent, plug-in") },
             { fieldArea (CountInField), tr ("Count-in for recording: off, 1 bar, 2 bars - REC starts after it, exactly on the bar") } };
}

juce::String ClickStrip::getTooltip()
{
    const int f = fieldAt (getMouseXYRelative().toFloat());
    if (f == ClickField) return tr ("Metronome: click on / off while the sequencer or the tape runs; right-click: level, accent, plug-in");
    if (f == CountInField) return tr ("Count-in for recording: off, 1 bar, 2 bars - REC starts after it, exactly on the bar");
    return {};
}

void ClickStrip::refresh()
{
    const auto n = proc.core().clickCount();
    if (n != clicksSeen)
    {
        clicksSeen = n;
        flash = 1.0f;
        flashDown = proc.core().lastClickWasDownbeat();
    }
    else if (flash > 0.0f)
        flash = juce::jmax (0.0f, flash - 0.25f);
    const auto& reg = proc.core().params();
    const int state = (reg.get (Metronome::Ids::on) >= 0.5f ? 1 : 0) + 2 * (int) std::lround (reg.get (Metronome::Ids::countIn))
                    + 8 * proc.core().countInBeatsLeft();
    if (flash > 0.0f || state != shownState)
    {
        shownState = state;
        repaint();
    }
}

void ClickStrip::paint (juce::Graphics& g)
{
    const float u = uiScale;
    paintGlass (g, getLocalBounds().toFloat(), u);
    const auto& reg = proc.core().params();
    const bool on = reg.get (Metronome::Ids::on) >= 0.5f;
    const int bars = (int) std::lround (reg.get (Metronome::Ids::countIn));
    const int left = proc.core().countInBeatsLeft();
    // a hairline between the two fields
    g.setColour (colour::displayGrid.brighter (0.4f));
    g.fillRect (juce::Rectangle<float> (fieldArea (1).getX() - 0.5f, 6.0f * u, 1.0f, (float) getHeight() - 12.0f * u));
    for (int f = 0; f < numFields; ++f)
    {
        auto a = fieldArea (f).reduced (4.0f * u, 3.0f * u);
        if (hover == f)
        {
            g.setColour (colour::displayGrid.brighter (0.25f));
            g.fillRoundedRectangle (a.expanded (1.0f * u, 0.0f), 3.0f * u);
        }
        if (f == ClickField)
        {
            // off: dim; on: paper white; on every click: green (the downbeat brighter)
            auto ink = on ? colour::displayText : colour::displayDim;
            if (flash > 0.0f && (on || left > 0)) ink = ink.interpolatedWith (flashDown ? colour::playGreenLit.brighter (0.2f) : colour::playGreenLit, flash);
            drawMetronomeSign (g, a.reduced (a.getHeight() * 0.06f), ink, on ? (flashDown ? -0.8f : 0.8f) * (0.4f + 0.6f * flash) : 0.0f);
            if (on) // a light dot: the click is on
            {
                g.setColour (colour::playGreenLit);
                g.fillEllipse (juce::Rectangle<float> (3.0f * u, 3.0f * u).withCentre ({ a.getRight() - 1.5f * u, a.getY() + 1.5f * u }));
            }
        }
        else
        {
            auto top = a.removeFromTop (a.getHeight() * 0.42f);
            const bool counting = left > 0;
            g.setColour (counting ? colour::signalRed : (bars > 0 ? colour::displayText : colour::displayDim));
            drawFittedLegend (g, tr ("COUNT"), top, juce::jmin (top.getHeight() * 0.9f, 9.0f * u), juce::Justification::centred);
            juce::String value = bars == 0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) : juce::String (bars);
            if (counting) value = juce::String (((left - 1) % Metronome::beatsPerBar) + 1);
            g.setColour (counting ? colour::signalRed : (bars > 0 ? colour::displayText : colour::displayDim));
            g.setFont (monoFont (juce::jmin (a.getHeight() * 0.9f, 14.0f * u)));
            g.drawText (value, a, juce::Justification::centred, false);
        }
    }
}

void ClickStrip::clickField (int f)
{
    const auto& reg = proc.core().params();
    if (f == ClickField) setParamFromScreen (proc, Metronome::Ids::on, reg.get (Metronome::Ids::on) >= 0.5f ? 0.0f : 1.0f);
    else if (f == CountInField) setParamFromScreen (proc, Metronome::Ids::countIn, (float) (((int) std::lround (reg.get (Metronome::Ids::countIn)) + 1) % 3));
    shownState = -1;
    repaint();
}

void ClickStrip::mouseDown (const juce::MouseEvent& e)
{
    const int f = fieldAt (e.position);
    if (f < 0) return;
    if (e.mods.isPopupMenu())
    {
        if (f == ClickField) showMetronomeMenu();
        else showControlMenu (proc, Metronome::Ids::countIn, this);
        return;
    }
    clickField (f);
}

void ClickStrip::showMetronomeMenu()
{
    const auto& reg = proc.core().params();
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Metronome"));
    m.addItem (1, tr ("Click on"), true, reg.get (Metronome::Ids::on) >= 0.5f);
    juce::PopupMenu level;
    const float lv = reg.get (Metronome::Ids::level);
    for (int i = 1; i <= 4; ++i)
        level.addItem (10 + i, juce::String (i * 25) + " %", true, std::abs (lv - 0.25f * (float) i) < 0.05f);
    m.addSubMenu (tr ("Click Level"), level);
    m.addItem (2, tr ("Accent on beat 1"), true, reg.get (Metronome::Ids::accent) >= 0.5f);
    m.addItem (3, tr ("Click in the plug-in too"), true, reg.get (Metronome::Ids::plugin) >= 0.5f);
    m.addSeparator();
    m.addItem (4, tr ("MIDI Learn"));
    juce::Component::SafePointer<ClickStrip> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        const auto& rg = p.core().params();
        auto flip = [&] (const char* id) { setParamFromScreen (p, id, rg.get (id) >= 0.5f ? 0.0f : 1.0f); };
        if (r == 1) flip (Metronome::Ids::on);
        else if (r == 2) flip (Metronome::Ids::accent);
        else if (r == 3) flip (Metronome::Ids::plugin);
        else if (r == 4) p.armMidiLearn (Metronome::Ids::on);
        else if (r > 10 && r <= 14) setParamFromScreen (p, Metronome::Ids::level, 0.25f * (float) (r - 10));
        safe->shownState = -1;
        safe->repaint();
    });
}

// ---------------------------------------------------------------- TAP
TapKey::TapKey (OhpeProcessor& p) : proc (p) {}

juce::String TapKey::getTooltip() { return tr ("Tap tempo: click in time (at least twice); right-click: MIDI learn"); }

void TapKey::refresh()
{
    if (const int n = proc.tapCount(); n != tapsSeen) { tapsSeen = n; lit = 1.0f; repaint(); }
    else if (lit > 0.0f) { lit = juce::jmax (0.0f, lit - 0.2f); repaint(); }
}

void TapKey::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f, getHeight() * 0.14f);
    const bool locked = proc.tempoFollowsHost();
    const auto edge = locked ? colour::displayGrid.brighter (0.3f) : colour::displayGrid.brighter (isMouseOver() ? 0.7f : 0.4f);
    if (lit > 0.0f)
    {
        g.setColour (colour::sunYellow.withAlpha (0.25f + 0.75f * lit));
        g.fillRoundedRectangle (b, b.getHeight() * 0.3f);
    }
    g.setColour (edge);
    g.drawRoundedRectangle (b, b.getHeight() * 0.3f, 1.0f);
    g.setColour (lit > 0.5f ? colour::display : (locked ? colour::displayDim.withAlpha (0.6f) : colour::displayText));
    drawFittedLegend (g, "TAP", b, b.getHeight() * 0.62f, juce::Justification::centred);
}

void TapKey::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showControlMenu (proc, Metronome::Ids::tap, this); return; }
    if (proc.tempoFollowsHost()) return;
    proc.tapTempo();
    refresh();
}
} // namespace ohpe::ui
