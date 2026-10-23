// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "KeyControl.h"

#include "plugin/Localisation.h"
#include "TempoControl.h"

#include <cmath>

namespace ohpe::ui
{
using i18n::tr;

namespace
{
bool german() { return i18n::current() == i18n::Language::German; }
const char* const keyParams[] = { Core::Ids::keyRoot, Core::Ids::keyScale, Core::Ids::keyLock, Core::Ids::keyLockMode };
} // namespace

void drawLockSign (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, bool closed)
{
    const float s = juce::jmin (area.getWidth(), area.getHeight());
    auto r = area.withSizeKeepingCentre (s * 0.72f, s);
    const float stroke = juce::jmax (1.0f, s * 0.1f);
    auto body = r.withTop (r.getY() + s * 0.44f).withTrimmedBottom (s * 0.06f);
    g.setColour (ink);
    if (closed) g.fillRoundedRectangle (body, s * 0.08f);
    else g.drawRoundedRectangle (body.reduced (stroke * 0.5f), s * 0.08f, stroke);
    juce::Path shackle; // an arc over the body; open: lifted to the right
    const float w = body.getWidth() * 0.62f, x0 = body.getCentreX() - w * 0.5f;
    const float lift = closed ? 0.0f : s * 0.12f;
    shackle.startNewSubPath (x0, body.getY() - lift + (closed ? 0.0f : s * 0.02f));
    shackle.lineTo (x0, r.getY() + s * 0.28f - lift);
    shackle.quadraticTo (body.getCentreX(), r.getY() - lift - s * 0.02f, x0 + w, r.getY() + s * 0.28f - lift);
    shackle.lineTo (x0 + w, body.getY() - (closed ? 0.0f : lift + s * 0.08f));
    g.strokePath (shackle, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (closed)
    {
        g.setColour (colour::display);
        g.fillEllipse (juce::Rectangle<float> (stroke * 1.3f, stroke * 1.3f).withCentre (body.getCentre()));
    }
}

// ---------------------------------------------------------------- key readout
KeyReadout::KeyReadout (OhpeProcessor& p) : proc (p) { setRepaintsOnMouseActivity (true); }

std::unique_ptr<juce::AccessibilityHandler> KeyReadout::createAccessibilityHandler()
{
    return a11y::textHandler (*this, [] { return tr ("Key"); }, [this] { return shownText(); });
}

juce::String KeyReadout::shortScaleName (int scale)
{
    // at most five letters: the header keeps room for the title (the menu and the tooltip give the full names)
    static const char* en[] = { "CHROM", "MAJ", "MIN", "DOR", "PHRYG", "LYD", "MIXO", "LOCR", "HMIN", "MMIN", "PENT", "MPENT", "BLUES", "WHOLE" };
    static const char* de[] = { "CHROM", "DUR", "MOLL", "DOR", "PHRYG", "LYD", "MIXO", "LOKR", "HMOLL", "MMOLL", "DPENT", "MPENT", "BLUES", "GANZT" };
    scale = juce::jlimit (0, scales::numScales - 1, scale);
    return german() ? de[scale] : en[scale];
}

juce::String KeyReadout::shownText() const
{
    const auto k = proc.keyScale();
    juce::String t = juce::String (keys::pitchName (k.root, k.root, k.scale, german())).toUpperCase() + " " + shortScaleName (k.scale);
    if (k.fromHost) t = i18n::upper (tr ("Host")) + "  " + t;
    return t;
}

juce::String KeyReadout::headerText() const
{
    // p75: what the header paints in full form - key and scale; a host key says so by its violet colour (as the
    // compact form), the tooltip names the host, so "HOST" does not take the title's room
    const auto k = proc.keyScale();
    return juce::String (keys::pitchName (k.root, k.root, k.scale, german())).toUpperCase() + " " + shortScaleName (k.scale);
}

juce::String KeyReadout::compactText() const
{
    // root + a short lower-case suffix (major: none, minor: "m"); a host key keeps its violet colour instead of HOST
    static const char* en[] = { "ch", "", "m", "do", "ph", "ly", "mx", "lo", "hm", "mm", "p", "mp", "bl", "wt" };
    static const char* de[] = { "ch", "", "m", "do", "ph", "ly", "mx", "lo", "hm", "mm", "p", "mp", "bl", "gt" };
    const auto k = proc.keyScale();
    const int sc = juce::jlimit (0, scales::numScales - 1, k.scale);
    return juce::String (keys::pitchName (k.root, k.root, k.scale, german())).toUpperCase() + (german() ? de[sc] : en[sc]);
}

float KeyReadout::neededWidth (float h, bool compactForm) const
{
    const auto f = monoFont (h * textSizeFraction (compactForm));
    // the lock and some air; at least room to grab the text left of the lock ("C" alone)
    return juce::jmax (h * 1.7f, juce::GlyphArrangement::getStringWidth (f, compactForm ? compactText() : headerText()) + h * 0.84f);
}

float KeyReadout::preferredWidth (float h, bool host)
{
    const auto f = monoFont (h * 0.5f);
    float widest = 0.0f;
    for (int s = 0; s < scales::numScales; ++s)
        widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (f, "C# " + shortScaleName (s)));
    if (host) widest += juce::GlyphArrangement::getStringWidth (f, i18n::upper (tr ("Host")) + "  ");
    return widest + h * 1.3f; // the lock and some air
}

juce::Rectangle<float> KeyReadout::lockArea() const
{
    const auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();
    return { b.getRight() - h * 0.68f, b.getY() + h * 0.23f, h * 0.54f, h * 0.54f }; // p75: a little smaller
}

void KeyReadout::refresh()
{
    const auto k = proc.keyScale();
    if (k != shown)
    {
        shown = k;
        repaint();
    }
}

juce::String KeyReadout::getTooltip()
{
    if (lockArea().expanded (2.0f).contains (getMouseXYRelative().toFloat()))
        return tr ("Scale Lock: every note plays in the key (nearest note, or the white keys play the key); right-click: MIDI learn");
    if (proc.keyScale().fromHost) return tr ("Key from the host (read-only); click: Scale Lock");
    return tr ("Key: drag up / down (Shift: scale), mouse wheel; click: key, scale, Scale Lock, MIDI learn");
}

void KeyReadout::paint (juce::Graphics& g)
{
    const auto k = proc.keyScale();
    auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();
    const bool hover = isMouseOverOrDragging();
    const auto lock = lockArea();
    auto textArea = b.withRight (lock.getX() - h * 0.1f);
    const auto text = paintedText();
    g.setFont (monoFont (h * textSizeFraction (compact)));
    const float w = juce::jmin (textArea.getWidth(), juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text));
    if (hover || dragging) // as the tempo: a soft yellow field behind while it is edited
    {
        g.setColour (colour::sunYellow.withAlpha (0.16f));
        g.fillRoundedRectangle (juce::Rectangle<float> (textArea.getRight() - w - h * 0.25f, b.getY() + h * 0.12f, w + h * 0.25f + (b.getRight() - textArea.getRight()), h * 0.76f), h * 0.2f);
    }
    g.setColour (k.fromHost ? colour::ultraviolet.brighter (0.3f) : dragging ? colour::sunYellow : (hover ? colour::displayText : colour::displayDim));
    g.drawText (text, textArea, juce::Justification::centredRight, true);
    // the lock: violet and closed while Scale Lock is on; White Keys adds two small keys under it
    const auto ink = k.lock ? colour::ultraviolet.brighter (0.35f) : colour::displayDim;
    drawLockSign (g, k.lock && k.lockMode == keys::LockWhiteKeys ? lock.withTrimmedBottom (lock.getHeight() * 0.22f) : lock, ink, k.lock);
    if (k.lock && k.lockMode == keys::LockWhiteKeys)
    {
        auto keysRow = lock.withTop (lock.getBottom() - lock.getHeight() * 0.18f).reduced (lock.getWidth() * 0.12f, 0.0f);
        const float kw = keysRow.getWidth() / 3.0f;
        g.setColour (colour::displayText);
        for (int i = 0; i < 3; ++i)
            g.fillRoundedRectangle (keysRow.withX (keysRow.getX() + kw * (float) i).withWidth (kw * 0.8f), 1.0f);
    }
}

void KeyReadout::mouseDown (const juce::MouseEvent& e)
{
    onLock = lockArea().expanded (2.0f).contains (e.position);
    moved = false;
    if (e.mods.isPopupMenu())
    {
        if (onLock) showControlMenu (proc, Core::Ids::keyLock, this);
        else showMenu();
        return;
    }
    const auto k = proc.keyScale();
    dragStartRoot = k.root;
    dragStartScale = k.scale;
    dragging = ! onLock && ! k.fromHost;
    repaint();
}

void KeyReadout::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 3) moved = true;
    if (! dragging || e.mods.isPopupMenu()) return;
    const int dy = -e.getDistanceFromDragStartY();
    if (isFineDrag (e.mods)) // the scale
    {
        const int s = juce::jlimit (0, scales::numScales - 1, dragStartScale + dy / 16);
        if (s != proc.keyScale().scale) proc.setKeyFromUi (dragStartRoot, s);
    }
    else
    {
        const int r = ((dragStartRoot + dy / 12) % 12 + 12) % 12;
        if (r != proc.keyScale().root) proc.setKeyFromUi (r, proc.keyScale().scale);
    }
    repaint();
}

void KeyReadout::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDrag = moved;
    dragging = false;
    repaint();
    if (e.mods.isPopupMenu() || wasDrag) return;
    if (onLock)
    {
        const auto k = proc.keyScale();
        proc.setScaleLockFromUi (! k.lock, k.lockMode);
        repaint();
        return;
    }
    showMenu();
}

void KeyReadout::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto k = proc.keyScale();
    if (k.fromHost || w.deltaY == 0.0f) return;
    const int d = w.deltaY > 0.0f ? 1 : -1;
    if (isFineDrag (e.mods)) proc.setKeyFromUi (k.root, juce::jlimit (0, scales::numScales - 1, k.scale + d));
    else proc.setKeyFromUi (k.root + d, k.scale);
    repaint();
}

void KeyReadout::showMenu()
{
    const auto k = proc.keyScale();
    const bool de = german();
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Key") + (k.fromHost ? " (" + tr ("Host") + ")" : juce::String()));
    juce::PopupMenu roots, scalesMenu, learn;
    for (int r = 0; r < 12; ++r)
        roots.addItem (RootBase + r, keys::pitchName (r, r, k.scale, de), ! k.fromHost, r == k.root);
    const auto en = scales::namesEn(), dn = scales::namesDe();
    for (int s = 0; s < scales::numScales; ++s)
        scalesMenu.addItem (ScaleBase + s, i18n::pick (en[(size_t) s], dn[(size_t) s]), ! k.fromHost, s == k.scale);
    m.addSubMenu (tr ("Key") + ": " + juce::String (keys::pitchName (k.root, k.root, k.scale, de)), roots, ! k.fromHost);
    m.addSubMenu (tr ("Scale") + ": " + i18n::pick (en[(size_t) k.scale], dn[(size_t) k.scale]), scalesMenu, ! k.fromHost);
    m.addSeparator();
    m.addItem (LockToggle, tr ("Scale Lock"), true, k.lock);
    m.addItem (LockModeBase + keys::LockNearest, tr ("Snap to the nearest note of the key"), true, k.lockMode == keys::LockNearest);
    m.addItem (LockModeBase + keys::LockWhiteKeys, tr ("White keys play the key (black keys are silent)"), true, k.lockMode == keys::LockWhiteKeys);
    m.addSeparator();
    const auto& reg = proc.core().params();
    for (int i = 0; i < 4; ++i)
    {
        const int idx = reg.indexOf (keyParams[i]);
        if (idx < 0) continue;
        const auto& d = reg.descriptor (idx);
        const auto mapping = proc.describeMidiMapping (keyParams[i]);
        learn.addItem (LearnBase + i, i18n::pick (d.nameEn, d.nameDe) + (mapping.isNotEmpty() ? "  (" + mapping + ")" : juce::String()));
    }
    m.addSubMenu (tr ("MIDI Learn"), learn);
    juce::Component::SafePointer<KeyReadout> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
        if (safe != nullptr && r > 0) safe->applyMenuResult (r);
    });
}

void KeyReadout::applyMenuResult (int id)
{
    const auto k = proc.keyScale();
    if (id >= RootBase && id < RootBase + 12) proc.setKeyFromUi (id - RootBase, k.scale);
    else if (id >= ScaleBase && id < ScaleBase + scales::numScales) proc.setKeyFromUi (k.root, id - ScaleBase);
    else if (id == LockToggle) proc.setScaleLockFromUi (! k.lock, k.lockMode);
    else if (id >= LockModeBase && id < LockModeBase + keys::numLockModes) proc.setScaleLockFromUi (true, id - LockModeBase);
    else if (id >= LearnBase && id < LearnBase + 4) proc.armMidiLearn (keyParams[id - LearnBase]);
    refresh();
    repaint();
}

// ---------------------------------------------------------------- voice strip
VoiceStripLayout layoutVoiceStrip (juce::Rectangle<float> area, bool softKeys, const juce::String& legend)
{
    VoiceStripLayout l;
    const float h = area.getHeight();
    l.legendFont = h * (softKeys ? 0.22f : 0.16f);
    l.noteFont = h * (softKeys ? 0.5f : 0.38f);
    l.chordFont = h * (softKeys ? 0.3f : 0.22f);
    // the note's room: the widest name a note can have (sharps / flats, German H / B, octaves -1 .. 9)
    float noteW = 0.0f;
    for (const char* n : { "C#-1", "Db-1", "Eb-1", "B#-2", "G#9", "Ab9", "Gb9", "H#-2", "Cb9" })
        noteW = juce::jmax (noteW, juce::GlyphArrangement::getStringWidth (brandFont (l.noteFont), n));
    const float legendW = juce::GlyphArrangement::getStringWidth (labelFont (l.legendFont), legend);
    const float rw = juce::jmax (noteW, legendW) + h * 0.08f;
    auto right = area.withLeft (area.getRight() - rw);
    l.legend = right.withHeight (h * (softKeys ? 0.3f : 0.26f)).withY (area.getY() + h * 0.04f);
    l.note = right.withTop (l.legend.getBottom()).withBottom (area.getBottom() - h * 0.04f);
    // the dots fill what is left (a gap before the note's room), the chord under them
    const float avail = juce::jmax (10.0f, right.getX() - area.getX() - h * 0.3f);
    l.dot = juce::jmin (h * (softKeys ? 0.26f : 0.18f), avail / (16.0f * 1.7f));
    l.step = l.dot * 1.7f;
    const float cy = area.getY() + h * (softKeys ? 0.34f : 0.32f);
    l.dots = juce::Rectangle<float> (area.getX() + l.dot * 0.5f, cy - l.dot * 0.5f, l.step * 15.0f + l.dot, l.dot);
    l.chord = juce::Rectangle<float> (l.dots.getX(), l.dots.getBottom() + h * 0.06f, avail - l.dot * 0.5f, l.chordFont * 1.2f);
    if (l.chord.getBottom() > area.getBottom()) l.chord = l.chord.withBottom (area.getBottom());
    return l;
}

// ---------------------------------------------------------------- p70: Key Gravity on the header
GravityControl::GravityControl (OhpeProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (true);
    a11y::makeFocusable (*this); // p83
    refresh();
}

std::unique_ptr<juce::AccessibilityHandler> GravityControl::createAccessibilityHandler()
{
    auto v = a11y::paramAccess (proc, [this] { return paramId(); }, [] { return tr ("Key Gravity"); });
    v.enabled = [this] { return available(); };
    return a11y::valueHandler (*this, std::move (v));
}

bool GravityControl::keyPressed (const juce::KeyPress& key)
{
    if (! available()) return false;
    if (key.getKeyCode() == juce::KeyPress::returnKey && ! key.getModifiers().isAnyModifierKeyDown())
    {
        if (onOpen) onOpen();
        return true;
    }
    const bool used = a11y::handleValueKey (key, a11y::paramAccess (proc, [this] { return paramId(); }));
    if (used) refresh();
    return used;
}

void GravityControl::setPart (int part)
{
    part = juce::jlimit (0, Core::numParts - 1, part);
    if (part == editPart) return;
    editPart = part;
    refresh();
    repaint();
}

juce::String GravityControl::paramId() const { return "p" + juce::String (editPart + 1) + ".grav.amt"; }

bool GravityControl::available() const { return proc.core().params().indexOf (paramId().toStdString()) >= 0; }

float GravityControl::amount() const
{
    return available() ? juce::jlimit (0.0f, 1.0f, proc.core().params().get (paramId().toStdString())) : 0.0f;
}

void GravityControl::refresh()
{
    const float a = amount();
    if (std::abs (a - shown) > 1.0e-4f)
    {
        shown = a;
        repaint();
    }
}

void GravityControl::setAmount (float a)
{
    if (! available()) return;
    setParamFromScreen (proc, paramId().toRawUTF8(), juce::jlimit (0.0f, 1.0f, a));
    refresh();
}

juce::String GravityControl::getTooltip()
{
    return tr ("Key Gravity \u2013 pulls the sound into key") + " (" + juce::String (juce::roundToInt (shown * 100.0f)) + " %)";
}

void GravityControl::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();
    const bool hover = isMouseOverOrDragging();
    if (hover || dragging) // as the key and the tempo: a soft yellow field behind while it is edited
    {
        g.setColour (colour::sunYellow.withAlpha (0.16f));
        g.fillRoundedRectangle (b.reduced (h * 0.04f, h * 0.12f), h * 0.2f);
    }
    const float d = juce::jmin (b.getWidth(), h) * 0.64f;
    const auto ring = b.withSizeKeepingCentre (d, d);
    const float stroke = juce::jmax (1.5f, d * 0.16f);
    const auto c = ring.getCentre();
    const float r = (d - stroke) * 0.5f;
    // the track: a thin dim circle
    g.setColour (colour::displayDim.withAlpha (hover ? 0.9f : 0.6f));
    g.drawEllipse (ring.reduced (stroke * 0.5f), juce::jmax (1.0f, stroke * 0.45f));
    // the amount: an arc from 12 o'clock, clockwise; empty at 0 %
    const auto accent = colour::encoder (0);
    if (shown > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * juce::jmin (1.0f, shown), true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // the centre: a small dot - the pitch the ring pulls to (lit with the amount)
    g.setColour (shown > 0.001f ? accent : colour::displayDim);
    g.fillEllipse (juce::Rectangle<float> (d * 0.22f, d * 0.22f).withCentre (c));
}

void GravityControl::mouseDown (const juce::MouseEvent& e)
{
    moved = false;
    if (e.mods.isPopupMenu())
    {
        if (available()) showControlMenu (proc, paramId(), this);
        return;
    }
    dragStart = amount();
    dragging = true;
    repaint();
}

void GravityControl::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || e.mods.isPopupMenu()) return;
    const float dy = (float) -e.getDistanceFromDragStartY();
    if (! moved && std::abs (dy) < 3.0f) return;
    moved = true;
    setAmount (dragStart + dy * (e.mods.isShiftDown() ? 0.001f : 0.0066f));
}

void GravityControl::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDrag = moved;
    dragging = false;
    repaint();
    if (e.mods.isPopupMenu() || wasDrag) return;
    if (onOpen) onOpen();
}

void GravityControl::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float dir = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (dir == 0.0f) return;
    const float step = e.mods.isShiftDown() ? 0.01f : 0.05f;
    // whole steps (a 5 % grid), so the wheel lands on round values
    const float now = amount();
    const float next = dir > 0.0f ? std::floor (now / step + 1.0e-3f) * step + step : std::ceil (now / step - 1.0e-3f) * step - step;
    setAmount (next);
}

} // namespace ohpe::ui
