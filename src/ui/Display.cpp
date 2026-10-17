// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Display.h"
#include "UserTableView.h"

#include "ModFxViews.h"
#include "ResoView.h"
#include "MorphView.h"
#include "MotionView.h"
#include "PadsView.h"
#include "HarmonyView.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <algorithm>
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

Display::Display (OhpeProcessor& p) : proc (p)
{
    setOpaque (false);
    addAndMakeVisible (tempoHeader); // p39
    addAndMakeVisible (tapHeader);
    addAndMakeVisible (keyHeader); // p45
}

Display::~Display() { proc.core().scope().setSource (ScopeTap::None); }

// ---------------------------------------------------------------- 0.7: visuals
void Display::setVisual (Visual v)
{
    userVisual = (Visual) juce::jlimit (0, (int) Visual::numVisuals - 1, (int) v);
    repaint();
}

void Display::setAnimatedVisuals (bool on)
{
    animated = on;
    if (! on) parallax = parallaxTarget = {};
    repaint();
}

int Display::forcedScopeSource() const
{
    // a Mid/Side effect on the encoders (any FX rack slot "pN.fx.", "pN.fx3.", "mfx2." ...) -> its scope
    const auto& reg = proc.core().params();
    for (const auto& id : ids)
    {
        int source = ScopeTap::None;
        juce::String prefix;
        if (id.startsWith ("mfx"))
        {
            prefix = id.upToFirstOccurrenceOf (".", true, false);
            source = ScopeTap::Master;
        }
        else if (id.length() > 5 && id[0] == 'p' && id.substring (2, 5) == ".fx")
        {
            const int part = (int) id[1] - (int) '1';
            if (part < 0 || part >= Core::numParts) continue;
            prefix = id.upToFirstOccurrenceOf (".", true, false) + id.fromFirstOccurrenceOf (".", false, false).upToFirstOccurrenceOf (".", true, false);
            source = part;
        }
        else
            continue;
        const int typeIdx = reg.indexOf ((prefix + "type").toStdString());
        if (typeIdx >= 0 && (int) std::lround (reg.get (typeIdx)) == InsertSlot::MidSide) return source;
    }
    return ScopeTap::None;
}

Display::Visual Display::shownVisual() const
{
    if (mode == Mode::Tape) return Visual::Character; // p11: the tape page always shows its transport
    if (forcedScopeSource() != ScopeTap::None) return Visual::Scope;
    if (userVisual == Visual::Terrain && ! animated) return Visual::Character;
    if (showsSpaceView()) return Visual::Space; // 1.0
    return userVisual;
}

int Display::scopeSource() const
{
    if (const int forced = forcedScopeSource(); forced != ScopeTap::None) return forced;
    switch (shownVisual())
    {
        case Visual::Scope:
        case Visual::Terrain:
        case Visual::Space: return ScopeTap::Master; // 1.0: the 3D view shows the master correlation
        case Visual::Living: return mode == Mode::Mixer ? (int) ScopeTap::Master : juce::jlimit (0, Core::numParts - 1, ctx.part); // p26
        case Visual::Character:
        case Visual::numVisuals: break;
    }
    // the character stands on a terrain of its own sound (synth: the edit part, mixer: the master)
    if (animated && mode == Mode::Synth) return ctx.part;
    if (animated && mode == Mode::Mixer) return ScopeTap::Master;
    return ScopeTap::None;
}

juce::Rectangle<float> Display::characterArea() const
{
    auto mid = midArea();
    if (mode == Mode::Sequencer) mid = mid.withTrimmedTop (mid.getHeight() * 0.17f);
    return mid.withWidth (mid.getWidth() * 0.42f);
}

// ---------------------------------------------------------------- p11: tape transport
juce::Rectangle<float> Display::transportButton (int b) const
{
    if (mode != Mode::Tape || b < 0 || b >= numTransportButtons) return {};
    const auto a = characterArea();
    const auto row = a.withTrimmedTop (a.getHeight() * 0.53f).withHeight (a.getHeight() * 0.27f);
    const float gap = row.getWidth() * 0.03f;
    const float w = (row.getWidth() - gap * (float) (numTransportButtons - 1)) / (float) numTransportButtons;
    return { row.getX() + (w + gap) * (float) b, row.getY(), w, row.getHeight() };
}

juce::Rectangle<float> Display::trackArmArea (int t) const
{
    if (mode != Mode::Tape || t < 0 || t >= TapeMachine::numTracks) return {};
    const auto lane = trackLane (t);
    const auto label = lane.withWidth (lane.getHeight() * 2.6f);
    return label.withTrimmedLeft (label.getWidth() * 0.45f);
}

juce::String Display::tapeHint() const
{
    auto& tape = proc.core().tape();
    if (proc.tapeRecording()) return tr ("Recording - REC again punches out, STOP ends");
    if (proc.tapeRecordReady() && ! proc.tapeRolling()) return tr ("Record is on - press PLAY to start");
    if (proc.tapeRolling()) return tr ("Playing - STOP stops, twice goes to the start");
    if (tape.contentEndFrames() <= 0) return tr ("Pick a track, press REC");
    return tr ("PLAY plays the tape - REC records on the armed track");
}

namespace
{
juce::String clockText (double seconds)
{
    seconds = juce::jmax (0.0, seconds);
    const int tenths = (int) (seconds * 10.0);
    return juce::String (tenths / 600) + ":" + juce::String ((tenths / 10) % 60).paddedLeft ('0', 2) + "." + juce::String (tenths % 10);
}

const char* transportTriggerId (int b)
{
    switch (b)
    {
        case Display::TxToStart: return Core::Ids::tapeRtz;
        case Display::TxStop: return Core::Ids::tapeTxStop;
        case Display::TxPlay: return Core::Ids::tapeTxTapePlay;
        case Display::TxRecord: return Core::Ids::tapeTxRec;
        default: return Core::Ids::tapeTxLoop;
    }
}
} // namespace

juce::String Display::getTooltip()
{
    const auto p = lastMouse;
    auto key = [this] (const char* k) { return shortcuts ? "  (" + juce::String (k) + ")" : juce::String(); };
    if (spaceBadgeArea().contains (p)) // 1.0: the 3D badge in the header
        return space3d::badgeTooltip (space3d::partState (proc.core(), ctx.part));
    if (mode == Mode::Tape)
    {
        for (int b = 0; b < numTransportButtons; ++b)
            if (transportButton (b).contains (p))
                switch (b)
                {
                    case TxToStart: return tr ("To the start of the tape") + key ("Enter");
                    case TxStop: return tr ("Stop - press again to go to the start") + key ("Space");
                    case TxPlay: return tr ("Play / stop the tape") + key ("Space");
                    case TxRecord: return tr ("Record on the armed track - arms the selected track when none is armed") + key ("R");
                    default: return tr ("Loop between loop in and out") + key ("L");
                }
        for (int t = 0; t < TapeMachine::numTracks; ++t)
        {
            if (trackArmArea (t).contains (p)) return tr ("Arm the track for recording");
            if (trackLane (t).contains (p)) return tr ("Select the track");
        }
    }
    return {};
}

void Display::paintTapeTransport (juce::Graphics& g, juce::Rectangle<float> a)
{
    auto& core = proc.core();
    auto& tape = core.tape();
    const auto& reg = core.params();
    const double sr = juce::jmax (1.0, tape.sampleRate());
    const double secs = (double) tape.positionFrames() / sr;
    const bool counting = core.countInActive(); // p39: the tape waits for the count-in
    const bool rolling = proc.tapeRolling() && ! counting, ready = proc.tapeRecordReady(), recording = proc.tapeRecording() && ! counting;
    const float pulse = breath (time); // p14: recording breathes (no hard blink)
    const int mask = proc.armedTrackMask();

    // statusRow line: state badge (left), armed tracks (right)
    auto statusRow = a.withHeight (a.getHeight() * 0.17f);
    {
        juce::String text;
        juce::Colour c = colour::displayDim;
        bool filled = false;
        Picto sign = Picto::Stop;
        if (recording) { text = i18n::upper (tr ("Rec")); c = colour::signalRed; filled = true; sign = Picto::Record; }
        else if (ready) { text = i18n::upper (tr ("Rec ready")); c = colour::signalRed; sign = Picto::Record; }
        else if (rolling) { text = i18n::upper (tr ("Play")); c = colour::playGreenLit; sign = Picto::Play; }
        else text = i18n::upper (tr ("Stop"));
        const auto f = labelFont (statusRow.getHeight() * 0.62f);
        const float h = statusRow.getHeight() * 0.84f;
        const float w = juce::GlyphArrangement::getStringWidth (f, text) + h * 1.9f;
        auto badge = statusRow.withWidth (juce::jmin (statusRow.getWidth() * 0.55f, w)).withSizeKeepingCentre (juce::jmin (statusRow.getWidth() * 0.55f, w), h);
        badge.setX (statusRow.getX());
        g.setColour (filled ? c.withAlpha (0.35f + 0.65f * pulse) : c);
        if (filled) g.fillRoundedRectangle (badge, badge.getHeight() * 0.3f);
        else g.drawRoundedRectangle (badge, badge.getHeight() * 0.3f, 1.2f);
        const auto ink = filled ? colour::displayText : c;
        // the sign is drawn, not a glyph: the fonts need not carry it
        drawPictogram (g, badge.withWidth (h).translated (h * 0.2f, 0.0f).reduced (h * 0.08f), sign, PictoState {}, ink);
        g.setColour (ink);
        g.setFont (f);
        g.drawText (text, badge.withTrimmedLeft (h * 1.1f).withTrimmedRight (h * 0.3f), juce::Justification::centred, true);

        juce::String armed;
        for (int t = 0; t < TapeMachine::numTracks; ++t)
            if ((mask >> t) & 1) armed << (armed.isEmpty() ? "" : ", ") << (t + 1);
        g.setColour (mask != 0 ? colour::signalRed : colour::displayDim);
        g.setFont (labelFont (statusRow.getHeight() * 0.55f));
        g.drawText (mask != 0 ? i18n::upper (tr ("Armed")) + " " + armed : i18n::upper (tr ("No track armed")),
                    statusRow.withTrimmedLeft (badge.getWidth() + statusRow.getHeight() * 0.4f), juce::Justification::centredRight, true);
    }

    // clock row: reels (turning with the tape), time, bar.beat
    auto clockRow = a.withTrimmedTop (a.getHeight() * 0.2f).withHeight (a.getHeight() * 0.3f);
    {
        auto reels = clockRow.removeFromLeft (clockRow.getHeight() * 1.9f);
        PictoState ps;
        ps.large = true;
        ps.time = (float) (secs * 1.5); // the reels follow the tape: still when stopped, backwards in reverse
        drawPictogram (g, reels, Picto::Tape, ps, recording ? colour::signalRed : colour::displayText);
        const auto bpm = juce::jmax (20.0, (double) reg.get (Core::Ids::tempo));
        const double beats = secs * bpm / 60.0;
        const juce::String bar = i18n::upper (tr ("Bar")) + " " + juce::String ((int) (beats / 4.0) + 1) + "." + juce::String ((int) std::fmod (beats, 4.0) + 1);
        auto barArea = clockRow.removeFromRight (clockRow.getWidth() * 0.34f);
        g.setColour (colour::displayDim);
        g.setFont (monoFont (clockRow.getHeight() * 0.36f));
        g.drawText (bar, barArea, juce::Justification::centredRight, true);
        g.setColour (recording ? colour::signalRed : colour::displayText);
        g.setFont (monoFont (clockRow.getHeight() * 0.72f));
        g.drawText (clockText (secs), clockRow.withTrimmedLeft (clockRow.getHeight() * 0.25f), juce::Justification::centredLeft, true);
    }

    // transport buttons
    const bool loopOn = reg.get ("tape.loop") >= 0.5f;
    for (int b = 0; b < numTransportButtons; ++b)
    {
        const auto r = transportButton (b);
        const bool isRec = b == TxRecord;
        const bool active = b == TxPlay ? rolling : b == TxRecord ? (recording || ready) : b == TxLoop ? loopOn : false;
        auto accent = isRec ? colour::signalRed : b == TxPlay ? colour::playGreenLit : colour::displayText;
        if ((isRec && recording) || (b == TxPlay && rolling)) accent = accent.withAlpha (0.35f + 0.65f * pulse);
        const bool hover = isMouseOver() && r.contains (lastMouse);
        if (active)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (r, r.getHeight() * 0.22f);
        }
        else
        {
            g.setColour (colour::displayGrid.brighter (hover ? 0.6f : 0.35f));
            g.drawRoundedRectangle (r, r.getHeight() * 0.22f, 1.2f);
        }
        static const Picto pictos[] = { Picto::ToStart, Picto::Stop, Picto::Play, Picto::Record, Picto::Loop };
        const auto ink = active ? (isRec ? colour::displayText : colour::display) : (isRec ? colour::signalRed : colour::displayText);
        drawPictogram (g, r.withSizeKeepingCentre (r.getHeight(), r.getHeight()).reduced (r.getHeight() * 0.12f), pictos[b], PictoState {}, ink);
    }

    // hint line
    auto hint = a.withTop (transportButton (0).getBottom() + a.getHeight() * 0.05f);
    g.setColour (proc.armedTrackMask() == 0 && ! rolling ? colour::sunYellow.withAlpha (0.9f) : colour::displayDim);
    g.setFont (labelFont (juce::jmin (hint.getHeight() * 0.7f, (float) getHeight() * 0.04f)));
    g.drawFittedText (tapeHint(), hint.toNearestInt(), juce::Justification::centredLeft, 2, 0.85f);
}

void Display::paintTapeHeaderStatus (juce::Graphics& g, juce::Rectangle<float> area, float height)
{
    // p11: the tape's state on every page: "(rec sign) REC 0:12.3" (breathing, p14) or "(play sign) TAPE 0:12.3" (green)
    if (! proc.tapeRolling() && ! proc.tapeRecordReady()) return;
    auto& tape = proc.core().tape();
    const double secs = (double) tape.positionFrames() / juce::jmax (1.0, tape.sampleRate());
    const bool rec = proc.tapeRecording() || proc.tapeRecordReady();
    const float pulse = breath (time);
    juce::String text = rec ? i18n::upper (tr ("Rec")) : i18n::upper (tr ("Tape"));
    const auto f = monoFont (height * 0.42f);
    const float signW = height * 0.55f;
    juce::String full = text + " " + clockText (secs);
    float w = juce::GlyphArrangement::getStringWidth (f, full) + height * 0.6f + signW;
    if (w > area.getWidth()) { full = text; w = juce::GlyphArrangement::getStringWidth (f, full) + height * 0.6f + signW; }
    if (w > area.getWidth()) return;
    auto pill = juce::Rectangle<float> (w, height * 0.6f).withPosition (area.getRight() - w, area.getCentreY() - height * 0.3f);
    const auto c = rec ? colour::signalRed : colour::playGreenLit;
    const bool recording = rec && proc.tapeRecording();
    g.setColour (recording ? c.withAlpha (0.35f + 0.65f * pulse) : c);
    if (recording) g.fillRoundedRectangle (pill, pill.getHeight() * 0.3f);
    else g.drawRoundedRectangle (pill, pill.getHeight() * 0.3f, 1.2f);
    const auto ink = recording ? colour::displayText : c;
    drawPictogram (g, pill.withWidth (pill.getHeight()).translated (height * 0.12f, 0.0f).reduced (pill.getHeight() * 0.1f),
                   rec ? Picto::Record : Picto::Play, PictoState {}, ink);
    g.setColour (ink);
    g.setFont (f);
    g.drawText (full, pill.withTrimmedLeft (signW + height * 0.1f), juce::Justification::centred, false);
}

void Display::paintStereoScope (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float u = (float) getWidth() / 100.0f;
    DisplayVisuals::Labels labels;
    labels.correlation = tr ("Correlation");
    visuals.paintScope (g, area.reduced (u * 0.6f, u * 0.4f), labels);
    // which signal: the part after its FX rack, or the master
    const int s = scopeSource();
    const int src = s >= 0 && s < Core::numParts ? s : (int) ScopeTap::Master;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.7f));
    g.drawText (src == ScopeTap::Master ? i18n::upper (tr ("Master")) : "P" + juce::String (src + 1),
                area.reduced (u * 0.6f, u * 0.2f).withHeight (u * 2.4f), juce::Justification::topLeft, false);
}

void Display::showVisualMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Display view"));
    const bool forced = forcedScopeSource() != ScopeTap::None;
    m.addItem (1, tr ("Character"), ! forced, userVisual == Visual::Character);
    m.addItem (2, tr ("Stereo scope"), ! forced, userVisual == Visual::Scope);
    m.addItem (3, tr ("Spectrum terrain"), ! forced && animated, userVisual == Visual::Terrain);
    m.addItem (4, tr ("3D space"), ! forced, userVisual == Visual::Space); // 1.0
    m.addItem (5, tr ("Living object"), ! forced, userVisual == Visual::Living); // p26
    m.addSeparator();
    m.addItem (10, tr ("Animated 3D visuals"), true, animated);
    juce::Component::SafePointer<Display> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        if (r == 10) safe->setAnimatedVisuals (! safe->animated);
        else safe->setVisual ((Visual) (r - 1));
        if (safe->onVisualSettingsChanged) safe->onVisualSettingsChanged();
    });
}

void Display::mouseMove (const juce::MouseEvent& e)
{
    lastMouse = e.position; // p11: tooltips
    if (! animated || getWidth() <= 0) return;
    parallaxTarget = { juce::jlimit (-1.0f, 1.0f, (e.position.x / (float) getWidth()) * 2.0f - 1.0f),
                       juce::jlimit (-1.0f, 1.0f, (e.position.y / (float) getHeight()) * 2.0f - 1.0f) };
}

void Display::mouseExit (const juce::MouseEvent&) { parallaxTarget = {}; }

// ---------------------------------------------------------------- 0.7: depth layers
void Display::resized()
{
    renderLayers();
    placeTempo();
}

void Display::placeTempo()
{
    // p39: the tempo where it always was (right of the header, before the page arrows), the TAP key left of it
    const auto h = headerArea();
    const auto bpmArea = h.withTrimmedRight (h.getHeight() * 4.0f);
    // as wide as its longest text (the play sign, three digits, the HOST / CLOCK tag when it follows), TAP right before it
    const juce::String widest = juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 ")) + "300.0 BPM" + (proc.tempoFollowsHost() ? "  CLOCK" : "");
    const float w = juce::GlyphArrangement::getStringWidth (monoFont (h.getHeight() * 0.5f), widest) + h.getHeight() * 0.45f;
    tempoHeader.setBounds (bpmArea.withLeft (bpmArea.getRight() - w).toNearestInt());
    tapHeader.setBounds (juce::Rectangle<float> (h.getHeight() * 1.3f, h.getHeight()).withPosition (bpmArea.getRight() - w - h.getHeight() * 1.4f, h.getY()).toNearestInt());
    tapHeader.setVisible (! proc.tempoFollowsHost());
    // p45: the key left of TAP (left of the tempo when TAP is hidden); compact ("F#m") when the full form would reach
    // the badges after the (shortest) title - it never overlaps the 3D badge
    const float hh = h.getHeight();
    const float left = (float) (tapHeader.isVisible() ? tapHeader.getX() : tempoHeader.getX()) - hh * 0.1f;
    const auto tf = brandFont (hh * 0.62f);
    const float titleFull = juce::GlyphArrangement::getStringWidth (tf, headerTitle (false)) + 4.0f;
    const float titleMin = mode == Mode::Synth ? juce::GlyphArrangement::getStringWidth (tf, headerTitle (true)) + 4.0f : juce::jmin (titleFull, hh * 4.0f);
    const float leftNeed = h.getX() + juce::jmin (titleFull, titleMin) + headerBadgesWidth() + hh * 0.2f;
    const bool full = left - keyHeader.neededWidth (hh, false) >= leftNeed;
    keyHeader.setCompact (! full);
    const float kw = keyHeader.neededWidth (hh, ! full);
    keyHeader.setBounds (juce::Rectangle<float> (kw, hh).withPosition (left - kw, h.getY()).toNearestInt());
    keyHost = proc.keyScale().fromHost;
    keyLaidOut = keyHeader.shownText() + "|" + headerTitle (false);
}

juce::String Display::headerTitle (bool engineOnly)
{
    auto& core = proc.core();
    const auto& eng = engineTypeInfo (selectedEngine (core, ctx.part));
    if (engineOnly && mode == Mode::Synth) return i18n::upper (i18n::pick (eng.nameEn, eng.nameDe));
    juce::String title = i18n::upper (tr (modeInfo (mode).nameEn));
    if (mode == Mode::Synth) title << "  /  " << i18n::upper (i18n::pick (eng.nameEn, eng.nameDe));
    if (mode == Mode::Mixer) title << "  /  " << i18n::upper (tr ("Channel")) << " " << (ctx.mixerChannel + 1);
    if (mode == Mode::Tape) title << "  /  " << i18n::upper (tr ("Track")) << " " << (ctx.tapeTrack + 1);
    if (mode == Mode::Drum) title << "  /  " << i18n::upper (tr ("Pad")) << " " << (ctx.drumPad + 1);
    return title;
}

float Display::headerBadgesWidth() const
{
    const bool partBadge = mode == Mode::Synth || mode == Mode::Envelope || mode == Mode::Lfo || mode == Mode::Effects;
    return headerArea().getHeight() * (0.4f + (partBadge ? 1.9f : 0.0f) + 1.75f);
}

void Display::paintOverChildren (juce::Graphics& g)
{
    // p39: the count-in - a big countdown over the middle of the display (beats left in the bar, the bar of how many)
    const int left = proc.core().countInBeatsLeft();
    if (left <= 0) return;
    const int bars = juce::jmax (1, (int) std::lround (proc.core().params().get (Metronome::Ids::countIn)));
    const int bar = juce::jlimit (1, bars, bars - (left - 1) / Metronome::beatsPerBar);
    const int beat = ((left - 1) % Metronome::beatsPerBar) + 1;
    const auto mid = midArea();
    const float u = (float) getWidth() / 100.0f;
    auto box = mid.withSizeKeepingCentre (juce::jmin (mid.getWidth() * 0.42f, u * 30.0f), juce::jmin (mid.getHeight() * 0.86f, u * 22.0f));
    g.setColour (colour::display.withAlpha (0.92f));
    g.fillRoundedRectangle (box, u * 1.2f);
    g.setColour (colour::signalRed);
    g.drawRoundedRectangle (box.reduced (0.5f), u * 1.2f, juce::jmax (1.0f, u * 0.25f));
    auto in = box.reduced (u * 1.2f);
    g.setFont (labelFont (u * 2.0f));
    g.drawText (i18n::upper (tr ("Count-in")), in.removeFromTop (u * 3.0f), juce::Justification::centred, false);
    auto foot = in.removeFromBottom (u * 3.0f);
    g.setColour (colour::displayDim);
    g.drawText (i18n::upper (tr ("Bar")) + " " + juce::String (bar) + " / " + juce::String (bars), foot, juce::Justification::centred, false);
    g.setColour (colour::displayText);
    g.setFont (monoFont (in.getHeight() * 0.95f));
    g.drawText (juce::String (beat), in, juce::Justification::centred, false);
    // four dots: the beats of this bar still to come
    const float d = u * 1.1f;
    for (int i = 0; i < Metronome::beatsPerBar; ++i)
    {
        auto dot = juce::Rectangle<float> (d, d).withCentre ({ box.getCentreX() + ((float) i - 1.5f) * d * 2.2f, foot.getY() - d * 0.3f });
        g.setColour (i < Metronome::beatsPerBar - beat + 1 ? colour::signalRed : colour::displayGrid.brighter (0.4f));
        g.fillEllipse (dot);
    }
}

void Display::renderLayers()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    const auto b = getLocalBounds().toFloat();
    const float u = b.getWidth() / 100.0f;
    const auto glass = b.reduced (u * 0.6f);
    juce::Path glassPath;
    const float du = b.getWidth() / 688.0f; // p14: design units (the display is 688 wide)
    glassPath.addRoundedRectangle (glass, radius::inset * du);

    // base: an inset bezel (shaded at the top, lit at the lower lip) and deep glass with a soft
    // light falling in from above, darker towards the corners
    baseLayer = juce::Image (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (baseLayer);
        g.setGradientFill (juce::ColourGradient (colour::bezelLow, 0, b.getY(), colour::bezel, 0, b.getBottom(), false)); // p14: dark frame
        g.fillRoundedRectangle (b, (radius::inset + 1.5f) * du);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawRoundedRectangle (b.reduced (0.5f), (radius::inset + 1.5f) * du, 1.0f);
        juce::ColourGradient fill (juce::Colour (0xff15171c), 0, glass.getY(), juce::Colour (0xff08090b), 0, glass.getBottom(), false);
        fill.addColour (0.55, colour::display);
        g.setGradientFill (fill);
        g.fillPath (glassPath);
        g.saveState();
        g.reduceClipRegion (glassPath);
        g.setGradientFill (juce::ColourGradient (colour::sheen.withAlpha (0.035f), glass.getCentreX(), glass.getY() + glass.getHeight() * 0.3f,
                                                 juce::Colours::transparentWhite, glass.getCentreX(), glass.getY() + glass.getHeight() * 1.1f, true));
        g.fillRect (glass);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, glass.getCentreX(), glass.getCentreY(),
                                                 juce::Colours::black.withAlpha (0.4f), glass.getX(), glass.getY(), true));
        g.fillRect (glass);
        g.restoreState();
    }

    // dot grid: its own layer behind the content, shifted by the parallax
    gridLayer = juce::Image (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (gridLayer);
        g.setColour (colour::displayGrid.brighter (0.08f));
        for (float x = glass.getX() + u; x < glass.getRight() - u * 0.5f; x += u * 2.0f)
            for (float y = glass.getY() + u; y < glass.getBottom() - u * 0.5f; y += u * 2.0f)
                g.fillRect (x, y, u * 0.16f, u * 0.16f);
    }

    // glass on top: inner shadow under the upper bezel, a lit lower lip and a faint diagonal sheen
    glassLayer = juce::Image (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (glassLayer);
        g.reduceClipRegion (glassPath);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.62f), 0, glass.getY(), juce::Colours::transparentBlack, 0,
                                                 glass.getY() + u * 2.6f, false));
        g.fillRect (glass.withHeight (u * 2.6f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.38f), glass.getX(), 0, juce::Colours::transparentBlack,
                                                 glass.getX() + u * 1.4f, 0, false));
        g.fillRect (glass.withWidth (u * 1.4f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.3f), glass.getRight(), 0, juce::Colours::transparentBlack,
                                                 glass.getRight() - u * 1.2f, 0, false));
        g.fillRect (glass.withLeft (glass.getRight() - u * 1.2f));
        g.setColour (colour::sheen.withAlpha (0.07f));
        g.fillRect (glass.withTop (glass.getBottom() - 1.2f).reduced (u * 1.2f, 0));
        juce::Path sheen;
        sheen.startNewSubPath (glass.getX(), glass.getY());
        sheen.lineTo (glass.getX() + glass.getWidth() * 0.58f, glass.getY());
        sheen.lineTo (glass.getX() + glass.getWidth() * 0.30f, glass.getBottom());
        sheen.lineTo (glass.getX(), glass.getBottom());
        sheen.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour::sheen.withAlpha (0.045f), glass.getX(), glass.getY(), juce::Colours::transparentWhite,
                                                 glass.getX() + glass.getWidth() * 0.42f, glass.getY() + glass.getHeight() * 0.5f, false));
        g.fillPath (sheen);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.strokePath (glassPath, juce::PathStrokeType (1.0f));
    }
}

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
    // p39: tempo readout, TAP, the count-in countdown
    if (tapHeader.isVisible() == proc.tempoFollowsHost()) placeTempo();
    tempoHeader.refresh();
    tapHeader.refresh();
    if (proc.keyScale().fromHost != keyHost || keyHeader.shownText() + "|" + headerTitle (false) != keyLaidOut) placeTempo(); // p45
    keyHeader.refresh();
    updateChord();
    if (const int ci = proc.core().countInBeatsLeft(); ci != shownCountIn)
    {
        shownCountIn = ci;
        repaint (midArea().toNearestInt().expanded (2));
    }
    // 0.7: scope tap source for the current view, visuals, parallax
    if (const int src = scopeSource(); src != lastSource)
    {
        proc.core().scope().setSource (src);
        visuals.clear();
        lastSource = src;
    }
    if (lastSource != ScopeTap::None) visuals.update (proc.core().scope(), proc.core().sampleRate(), seconds);
    spaceScene.update (proc.core(), seconds); // 1.0: trails of the 3D sources
    if (animated) parallax += (parallaxTarget - parallax) * juce::jmin (1.0f, seconds * 5.0f);
    const float target = juce::jmin (1.0f, (float) proc.core().activeVoices() / 3.0f);
    energy += (target - energy) * 0.2f;
    if (flashTime > 0.0f) flashTime -= seconds;
    overviewAge += seconds;
    if (mode == Mode::Tape && overviewAge > 0.5f) updateTapeOverview();
    if (showsModView()) // 0.7: history of the selected routing's value (about 3 s)
    {
        const auto& m = proc.core().part (juce::jlimit (0, Core::numParts - 1, ctx.part)).modMatrix();
        modHistory.push_back (m.uiRouteValue (juce::jlimit (0, mod::numSlots - 1, ctx.modRouting)));
        if (modHistory.size() > 96) modHistory.erase (modHistory.begin());
    }
    livingDirty = {};
    if (livingOnScreen()) // p26: the object repaints only its own region, and only while it moves
    {
        const auto area = characterArea();
        livingObj.update (living::gather (proc.core(), lastSource == ScopeTap::Master ? ctx.part : lastSource, visuals), seconds, animated);
        const bool settled = parallax.getDistanceFrom (parallaxTarget) < 0.002f || ! animated;
        juce::RectangleList<int> rest (getLocalBounds());
        rest.subtract (area.toNearestInt());
        if (! settled) rest = juce::RectangleList<int> (getLocalBounds()); // parallax moves the grid layer
        for (const auto& r : rest) repaint (r);
        if (livingObj.consumeChanged() || ! settled)
        {
            const auto now = living::objectBounds (area, livingObj);
            livingDirty = now.getUnion (livingLast.isEmpty() ? now : livingLast).getUnion (living::readoutArea (area))
                              .getUnion (area.withTrimmedTop (area.getHeight() * 0.7f).withHeight (area.getHeight() * 0.14f)); // the rest hint
            livingLast = now;
            repaint (livingDirty.getSmallestIntegerContainer());
        }
        return;
    }
    livingObj.reset();
    livingLast = {};
    repaint();
}

bool Display::livingOnScreen() const
{
    return shownVisual() == Visual::Living && mode != Mode::Tape && ! showsModView() && ! showsRackView() && ! showsSpaceView()
        && ! showsBodyView() && ! showsMorphView() && ! showsMotionView() && ! showsPadsView() && ! showsHarmonyView(); // p25, p24
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

// ---------------------------------------------------------------- 1.0: 3D badge
juce::Rectangle<float> Display::spaceBadgeArea() const
{
    const auto h = headerArea();
    // after the title and the part badge (placed while painting the header)
    if (! badgeRect.isEmpty()) return badgeRect;
    return juce::Rectangle<float> (h.getHeight() * 1.75f, h.getHeight() * 0.66f).withPosition (h.getCentreX(), h.getCentreY() - h.getHeight() * 0.33f);
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
    if (mode == Mode::Tape) f = softKeyRows() > 1 ? 0.40f : 0.36f; // p38: two key rows take a little more room
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
    const float w = mid.getWidth() / (float) juce::jmax (1, numViewTabs());
    return { mid.getX() + w * (float) view, mid.getY(), w, mid.getHeight() * 0.14f };
}

int Display::numViewTabs() const noexcept
{
    if (mode == Mode::Sequencer) return numSeqViews;
    if (mode == Mode::Lfo) return modfx::numLfoViews;   // 0.7: LFO | Mod
    if (mode == Mode::Effects) return modfx::numFxViews; // 0.7: Sends | Rack | Master
    return 0;
}

juce::String Display::viewTabName (int v) const
{
    if (mode == Mode::Lfo) return tr (modfx::lfoViewName (v));
    if (mode == Mode::Effects) return tr (modfx::fxViewName (v));
    return tr (seqViewName ((SeqView) v));
}

int Display::selectedViewTab() const noexcept
{
    if (mode == Mode::Lfo) return ctx.lfoView;
    if (mode == Mode::Effects) return ctx.fxView;
    return (int) ctx.seqView;
}

bool Display::showsModView() const noexcept { return mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMod; }
bool Display::showsRackView() const noexcept { return mode == Mode::Effects && modfx::isRackView (ctx.fxView); }
bool Display::showsSpaceView() const noexcept { return mode == Mode::Effects && ctx.fxView == modfx::FxViewSpace; }
bool Display::showsBodyView() const noexcept { return mode == Mode::Effects && ctx.fxView == modfx::FxViewBody; }
bool Display::showsMorphView() const noexcept { return mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMorph; }
bool Display::showsMotionView() const noexcept { return mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewMotion; }
bool Display::showsPadsView() const noexcept { return mode == Mode::Effects && ctx.fxView == modfx::FxViewPads; }
bool Display::showsHarmonyView() const noexcept { return mode == Mode::Lfo && ctx.lfoView == modfx::LfoViewHarmony; }

juce::Rectangle<float> Display::morphPadArea() const { return morphview::padArea (visualArea()); }
juce::Rectangle<float> Display::morphCornerArea (int corner) const { return morphview::cornerChip (morphPadArea(), corner); }

void Display::setMorphPosition (juce::Point<float> p)
{
    const auto pos = morphview::positionAt (morphPadArea(), p);
    const auto pre = "p" + juce::String (ctx.part + 1) + ".morph.";
    if (auto* x = proc.parameter (pre + "x")) x->setValueNotifyingHost (pos.x);
    if (auto* y = proc.parameter (pre + "y")) y->setValueNotifyingHost (pos.y);
    repaint();
}

juce::Rectangle<float> Display::visualArea() const
{
    const float u = (float) getWidth() / 100.0f;
    auto mid = midArea();
    if (numViewTabs() > 0) mid = mid.withTrimmedTop (mid.getHeight() * 0.17f);
    return mid.withWidth (mid.getWidth() * 0.42f).reduced (u * 0.3f, 0.0f);
}

FxRack& Display::shownRack() const
{
    auto& core = proc.core();
    return ctx.fxView == modfx::FxViewMaster ? core.masterFxRack() : core.fxRack (juce::jlimit (0, Core::numParts - 1, ctx.part));
}

juce::Rectangle<float> Display::rowsArea() const
{
    const float u = (float) getWidth() / 100.0f;
    auto mid = midArea();
    if (numViewTabs() > 0) mid = mid.withTrimmedTop (mid.getHeight() * 0.17f);
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
    // p38: the tape page can have two rows of keys (each as high as the single row: 0.2 of a 0.36 strip)
    const float tapeRow = softKeyRows() > 1 ? 2.0f * 0.2f * 0.36f / 0.40f : 0.2f;
    return s.withTop (s.getBottom() - s.getHeight() * (mode == Mode::Tape ? tapeRow : (mode == Mode::Drum ? 0.22f : 0.3f)));
}

int Display::softKeyRows() const noexcept
{
    return mode == Mode::Tape && softKeys.size() > 8 ? 2 : 1; // p38: the tape page's twelve keys sit in two rows of six
}

juce::Rectangle<float> Display::softKeyArea (int index) const
{
    if (index < 0 || index >= (int) softKeys.size()) return {};
    auto row = softKeyRow();
    const int rows = softKeyRows();
    const int perRow = ((int) softKeys.size() + rows - 1) / rows;
    const int r = index / perRow, col = index % perRow;
    const int inRow = juce::jmin (perRow, (int) softKeys.size() - r * perRow);
    const int n = juce::jmax (4, perRow);
    // the tape page keeps the left part of the row free for the time readout
    if (mode == Mode::Tape) row = row.withTrimmedLeft (row.getWidth() * 0.24f);
    const float h = row.getHeight() / (float) rows;
    row = row.withY (row.getY() + h * (float) r).withHeight (h);
    const float w = row.getWidth() / (float) n;
    // keys sit at the right end of the row
    const float x0 = row.getRight() - w * (float) inRow;
    return juce::Rectangle<float> (x0 + w * (float) col, row.getY(), w, row.getHeight()).reduced (w * 0.04f, row.getHeight() * 0.1f);
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
    s = s.withBottom (softKeyRow().getY() - s.getHeight() * 0.02f).withTrimmedTop (s.getHeight() * 0.03f); // p38: above the key rows
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
    lastMouse = p; // p11: tooltips
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
    if (mode == Mode::Tape) // p11: transport buttons (right-click = MIDI learn), arm dots
    {
        for (int b = 0; b < numTransportButtons; ++b)
            if (transportButton (b).contains (p))
            {
                if (e.mods.isPopupMenu()) showControlMenu (proc, transportTriggerId (b), this);
                else if (onTransportClicked) onTransportClicked (b);
                return;
            }
        for (int t = 0; t < TapeMachine::numTracks; ++t)
            if (trackArmArea (t).contains (p))
            {
                if (e.mods.isPopupMenu()) showControlMenu (proc, "tape.tr" + juce::String (t + 1) + ".arm", this);
                else if (onTrackArmClicked) onTrackArmClicked (t);
                return;
            }
    }

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

    if (spaceBadgeArea().contains (p)) // 1.0: the 3D badge opens the Space view of the edit part
    {
        if (onSpaceBadgeClicked) onSpaceBadgeClicked();
        return;
    }
    if (mode == Mode::Lfo || mode == Mode::Effects) // 0.7: view tabs, routing list, FX chain
    {
        for (int v = 0; v < numViewTabs(); ++v)
            if (seqTab (v).contains (p)) { if (onViewTabClicked) onViewTabClicked (v); return; }
        if (showsModView())
            if (const int r = modfx::modListHit (visualArea(), p); r >= 0) { if (onModRoutingClicked) onModRoutingClicked (r); return; }
        if (showsMorphView()) // p16: corner chips assign sounds, the pad moves the position (gesture per drag)
        {
            if (const int c = morphview::cornerAt (morphPadArea(), p); c >= 0) { if (onMorphCornerClicked) onMorphCornerClicked (c); return; }
            if (morphPadArea().contains (p))
            {
                const auto pre = "p" + juce::String (ctx.part + 1) + ".morph.";
                if (e.mods.isPopupMenu()) { showControlMenu (proc, pre + "x", this); return; }
                for (const char* k : { "x", "y" })
                    if (auto* prm = proc.parameter (pre + k)) prm->beginChangeGesture();
                morphDrag = true;
                setMorphPosition (p);
                return;
            }
        }
        if (showsPadsView()) // p25: press and hold a pad (shift-click latches, right-click: MIDI learn)
            if (const int k = padsview::padAt (visualArea(), p); k >= 0)
            {
                const juce::String id (PerformancePads::padId (k));
                if (e.mods.isPopupMenu()) { showControlMenu (proc, id, this); return; }
                if (auto* prm = proc.parameter (id))
                {
                    const bool on = prm->getValue() > 0.5f;
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (e.mods.isShiftDown() && on ? 0.0f : 1.0f);
                    if (e.mods.isShiftDown()) prm->endChangeGesture();
                    else heldPad = k;
                }
                repaint();
                return;
            }
        if (showsRackView())
            if (const int s = modfx::rackChainHit (visualArea(), shownRack().numSlots(), p); s >= 0)
            {
                dragSlot = s;
                if (onFxSlotClicked) onFxSlotClicked (s);
                return;
            }
    }

    // 0.7: the visual area - click cycles character / scope / terrain, right-click opens the view menu
    if (! showsModView() && ! showsRackView() && ! showsSpaceView() && ! showsBodyView() && ! showsMorphView() && ! showsMotionView() && ! showsHarmonyView() && ! showsPadsView() && mode != Mode::Tape && characterArea().contains (p)
        && ! (mode == Mode::Sequencer && seqTab (0).getBottom() > p.y))
    {
        if (e.mods.isPopupMenu()) { showVisualMenu(); return; }
        if (forcedScopeSource() == ScopeTap::None)
        {
            int v = ((int) userVisual + 1) % (int) Visual::numVisuals;
            if (v == (int) Visual::Terrain && ! animated) v = (int) Visual::Character;
            setVisual ((Visual) v);
            if (onVisualSettingsChanged) onVisualSettingsChanged();
            return;
        }
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
    if (morphDrag) { setMorphPosition (e.position); return; } // p16
    if (dragRow < 0) return;
    const float len = juce::jmax (40.0f, barArea (dragRow).getWidth());
    setRowValue (dragRow, drag.update (e.position, len, isFineDrag (e.mods)));
}

void Display::mouseUp (const juce::MouseEvent& e)
{
    if (dragSlot >= 0 && showsRackView()) // 0.7: drop a slot on another one = move it there
    {
        const int to = modfx::rackChainHit (visualArea(), shownRack().numSlots(), e.position);
        if (to >= 0 && to != dragSlot && onFxSlotMoved) onFxSlotMoved (dragSlot, to);
    }
    dragSlot = -1;
    if (heldPad >= 0) // p25: a held pad lets go
    {
        if (auto* prm = proc.parameter (PerformancePads::padId (heldPad)))
        {
            prm->setValueNotifyingHost (0.0f);
            prm->endChangeGesture();
        }
        heldPad = -1;
        repaint();
    }
    if (morphDrag) // p16
    {
        morphDrag = false;
        const auto pre = "p" + juce::String (ctx.part + 1) + ".morph.";
        for (const char* k : { "x", "y" })
            if (auto* prm = proc.parameter (pre + k)) prm->endChangeGesture();
    }
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

    // glass (0.7: with depth): inset bezel and deep glass, the dot grid as a layer further back
    // (parallax), the content, then the glass surface itself (inner shadow, sheen) on top
    if (baseLayer.getWidth() != getWidth() || baseLayer.getHeight() != getHeight()) renderLayers();
    const auto glass = b.reduced (u * 0.6f);
    g.drawImageAt (baseLayer, 0, 0);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (glass.reduced (1.0f), u * 1.2f);
        g.reduceClipRegion (clip);
        g.drawImageTransformed (gridLayer, juce::AffineTransform::translation (-parallax.x * u * 0.6f, -parallax.y * u * 0.45f));
    }

    auto& core = proc.core();
    const auto& info = modeInfo (mode);
    const auto type = selectedEngine (core, ctx.part);
    const auto& eng = engineTypeInfo (type);

    // header
    auto h = headerArea();
    g.setFont (brandFont (h.getHeight() * 0.62f));
    g.setColour (colour::displayText);
    juce::String title = headerTitle (false);
    const bool partMode = mode == Mode::Synth || mode == Mode::Envelope || mode == Mode::Lfo;
    // 1.0: a long title (engine names, German) shortens with an ellipsis before it pushes the part badge and the 3D badge
    // into the tempo on the right: badges = 0.4 + 1.9 (part) + 2.3 (3D) header heights, tempo + page arrows = 7.8
    const float badgesW = h.getHeight() * ((partMode || mode == Mode::Effects ? 2.3f : 0.4f) + 1.75f + 0.2f);
    const float titleMax = juce::jmax (h.getHeight() * 1.0f, juce::jmin (h.getWidth() * 0.62f, h.getWidth() - juce::jmax (h.getHeight() * 7.8f, h.getRight() - (float) keyHeader.getX() + h.getHeight() * 0.3f) - badgesW));
    // p45: a crowded Synth header keeps the engine name ("VOCAL") rather than cutting "SYNTH / VO..."
    if (mode == Mode::Synth && juce::GlyphArrangement::getStringWidth (brandFont (h.getHeight() * 0.62f), title) + 4.0f > titleMax) title = headerTitle (true);
    const float titleW = juce::jmin (titleMax, juce::GlyphArrangement::getStringWidth (brandFont (h.getHeight() * 0.62f), title) + 4.0f);
    g.drawText (title, h.withWidth (titleW), juce::Justification::centredLeft, true);
    float badgeX = h.getX() + titleW + h.getHeight() * 0.4f; // 1.0: where the 3D badge goes
    if (partMode || mode == Mode::Effects)
    {
        badgeX += h.getHeight() * 1.9f;
        // part badge: which of the four parts the pages edit
        auto badge = juce::Rectangle<float> (h.getHeight() * 1.5f, h.getHeight() * 0.62f)
                         .withPosition (h.getX() + titleW + h.getHeight() * 0.4f, h.getCentreY() - h.getHeight() * 0.31f);
        g.setColour (colour::sunYellow);
        g.fillRoundedRectangle (badge, badge.getHeight() * 0.3f);
        g.setColour (colour::display);
        g.setFont (brandFont (badge.getHeight() * 0.72f));
        g.drawText ("P" + juce::String (ctx.part + 1), badge, juce::Justification::centred);
    }
    // 1.0: the 3D badge - lights up while the edit part plays binaural 3D
    badgeRect = juce::Rectangle<float> (h.getHeight() * 1.75f, h.getHeight() * 0.66f).withPosition (badgeX, h.getCentreY() - h.getHeight() * 0.33f);
    space3d::drawBadge (g, spaceBadgeArea(), space3d::partState (core, ctx.part), time);
    g.setFont (monoFont (h.getHeight() * 0.5f));
    g.setColour (colour::displayDim);
    // p39: the tempo and TAP are child components (editable); the header keeps their room
    const auto bpmArea = h.withTrimmedRight (h.getHeight() * 4.0f);
    if (mode != Mode::Tape) // p11: the tape's state on every other page
    {
        const float bpmW = bpmArea.getRight() - (float) keyHeader.getX() - h.getHeight() * 0.3f; // p45: the key, TAP, the tempo
        auto room = bpmArea.withTrimmedRight (bpmW + h.getHeight() * 0.5f);
        room = room.withLeft (h.getX() + titleW + h.getHeight() * ((partMode || mode == Mode::Effects) ? 2.4f : 0.5f));
        if (room.getWidth() > 0.0f) paintTapeHeaderStatus (g, room, h.getHeight());
    }
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
    if (numViewTabs() > 0)
    {
        for (int v = 0; v < numViewTabs(); ++v)
        {
            auto tab = seqTab (v).reduced (u * 0.4f, 0);
            const bool sel = selectedViewTab() == v;
            g.setColour (sel ? colour::displayText : colour::displayGrid.brighter (0.25f));
            if (sel) g.fillRoundedRectangle (tab, tab.getHeight() * 0.25f);
            else g.drawRoundedRectangle (tab, tab.getHeight() * 0.25f, 1.0f);
            g.setColour (sel ? colour::display : colour::displayDim);
            g.setFont (labelFont (tab.getHeight() * 0.6f));
            g.drawText (i18n::upper (viewTabName (v)), tab, juce::Justification::centred, true);
        }
        mid = mid.withTrimmedTop (mid.getHeight() * 0.17f);
    }

    // 0.7: the matrix's routing list / the FX rack's chain instead of the pictogram
    if (showsSpaceView()) // 1.0: the head-centred 3D view of every part's source
    {
        spaceScene.paint (g, visualArea(), core, ctx.part, parallax, visuals.correlation());
        paintRows (g);
        paintStrip (g, stripArea());
        g.drawImageAt (glassLayer, 0, 0);
        return;
    }
    if (showsBodyView()) // p19: the edit part's resonator body
    {
        const int pt = juce::jlimit (0, Core::numParts - 1, ctx.part);
        const auto pre = "p" + std::to_string (pt + 1) + ".reso.";
        const bool on = core.params().get (pre + "on") >= 0.5f;
        const auto va = visualArea();
        resoview::paintBody (g, va, resoview::stateOf (core.part (pt).resoStage(), core, pre, on), time, colour::encoder (pt));
        g.setFont (labelFont (va.getHeight() * 0.05f)); // caption: the body model, as the 3D view names its monitoring
        g.setColour (colour::displayDim);
        const int mi = core.params().indexOf (pre + "model");
        g.drawText (i18n::upper (tr ("Resonator")) + (mi >= 0 ? "  " + i18n::upper (formatValue (core.params().descriptor (mi), core.params().get (mi))) : juce::String()),
                    va.withHeight (va.getHeight() * 0.08f), juce::Justification::topLeft, true);
        paintRows (g);
        paintStrip (g, stripArea());
        return;
    }
    if (showsPadsView()) // p25: the performance pads
    {
        padsview::paint (g, visualArea(), core, time);
        paintRows (g);
        paintStrip (g, stripArea());
        return;
    }
    if (showsHarmonyView()) // p24: the harmony brain
    {
        harmonyview::paint (g, visualArea(), core, juce::jlimit (0, Core::numParts - 1, ctx.part), time);
        paintRows (g);
        paintStrip (g, stripArea());
        return;
    }
    if (showsMotionView()) // p22: the motion lanes of the edit part
    {
        motionview::paint (g, visualArea(), core, juce::jlimit (0, Core::numParts - 1, ctx.part), time);
        paintRows (g);
        paintStrip (g, stripArea());
        return;
    }
    if (showsMorphView()) // p16: the morph pad of the edit part
    {
        morphview::paint (g, visualArea(), core, juce::jlimit (0, Core::numParts - 1, ctx.part), time);
        paintRows (g);
        paintStrip (g, stripArea());
        return;
    }
    if (showsModView() || showsRackView())
    {
        if (showsModView()) modfx::paintModList (g, visualArea(), core, ctx.part, ctx.modRouting);
        else modfx::paintRackChain (g, visualArea(), core, modfx::rackPrefixes (ctx.fxView, ctx.part), shownRack(), ctx.fxSlot);
        paintRows (g);
        paintStrip (g, stripArea());
        return;
    }

    // middle: pictogram character (0.7: or stereo scope / spectrum terrain) + encoder rows
    auto pictoArea = mid.withWidth (mid.getWidth() * 0.42f);
    PictoState ps;
    ps.large = true;
    ps.time = time;
    ps.energy = energy;
    for (int i = 0; i < 4; ++i) ps.macro[i] = rowValue (i);
    const Picto picto = mode == Mode::Synth ? (Picto) (int) type : info.picto;
    const bool dim = (mode == Mode::Synth && ! eng.implemented) || ! info.available;
    const auto shown = shownVisual();
    if (showsExpression()) // p33: velocity / aftertouch responses instead of the character
    {
        paintExpression (g, pictoArea.reduced (u * 0.8f, u * 0.6f));
        paintRows (g);
        paintStrip (g, stripArea());
        g.drawImageAt (glassLayer, 0, 0);
        return;
    }
    if (shown == Visual::Scope)
    {
        paintStereoScope (g, pictoArea);
    }
    else if (shown == Visual::Space) // 1.0
    {
        spaceScene.paint (g, pictoArea.reduced (u * 0.4f, u * 0.2f), core, ctx.part, parallax, visuals.correlation());
    }
    else if (shown == Visual::Living) // p26: drawn only where the tick asked for it (the clip)
    {
        const auto area = characterArea();
        if (g.clipRegionIntersects (area.getSmallestIntegerContainer()))
            living::paint (g, area, livingObj, colour::encoder (juce::jlimit (0, Core::numParts - 1, ctx.part)),
                           lastSource == ScopeTap::Master ? i18n::upper (tr ("Master")) : "P" + juce::String (juce::jlimit (0, Core::numParts - 1, ctx.part) + 1),
                           animated ? parallax : juce::Point<float>());
    }
    else if (shown == Visual::Terrain)
    {
        visuals.paintTerrain (g, pictoArea.reduced (u * 0.4f, u * 0.6f), parallax, 1.0f);
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.7f));
        g.drawText (i18n::upper (tr ("Master")), pictoArea.reduced (u * 0.6f, u * 0.2f).withHeight (u * 2.4f), juce::Justification::topLeft, false);
    }
    else
    {
        // the character stands on a slowly moving terrain of its sound (a layer further back)
        if (animated && (mode == Mode::Synth || mode == Mode::Mixer))
            visuals.paintTerrain (g, pictoArea.withTrimmedTop (pictoArea.getHeight() * 0.42f).reduced (u * 0.4f, 0), parallax * 0.6f, 0.42f);
        if (mode == Mode::Tape) // p11: the transport panel takes the character's place
        {
            paintTapeTransport (g, characterArea());
            paintRows (g);
            paintStrip (g, stripArea());
            g.drawImageAt (glassLayer, 0, 0);
            return;
        }
        const auto charArea = pictoArea.reduced (u * 1.5f);
        if (mode == Mode::Synth && usertableview::wants (proc, ctx.part, type)) // p20: the user wavetable / formants
            usertableview::paint (g, pictoArea.reduced (u * 0.6f), proc, ctx.part, type, -1.0f, time);
        else
        {
            drawPictogram (g, charArea.translated (u * 0.35f, u * 0.6f), picto, ps, juce::Colours::black.withAlpha (0.55f)); // cast shadow
            drawPictogram (g, charArea, picto, ps, dim ? colour::displayDim : colour::displayText);
        }
    }
    if (dim && shown == Visual::Character)
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 2.2f));
        g.drawText (tr ("Coming in a later version"), pictoArea.withTop (pictoArea.getBottom() - u * 4), juce::Justification::centred);
    }
    paintRows (g);
    paintStrip (g, stripArea());
    g.drawImageAt (glassLayer, 0, 0);
}

bool Display::showsExpression() const
{
    if (mode != Mode::Envelope) return false;
    for (const auto& id : ids)
        if (id.contains (".expr.")) return true;
    return false;
}

void Display::paintExpression (juce::Graphics& g, juce::Rectangle<float> area) const
{
    auto& core = proc.core();
    const auto& reg = core.params();
    const int pt = juce::jlimit (0, Core::numParts - 1, ctx.part);
    const std::string pre = "p" + std::to_string (pt + 1) + ".expr.";
    const float u = (float) getWidth() / 100.0f;
    // the slots this page shows (velocity 1 / 2, aftertouch 1 / 2), in encoder order
    std::vector<std::pair<int, int>> shownSlots;
    for (const auto& id : ids)
    {
        if (! id.contains (".expr.")) continue;
        const std::pair<int, int> sk { id.contains (".expr.vel.") ? 0 : 1, id.endsWithChar ('2') ? 1 : 0 };
        if (std::find (shownSlots.begin(), shownSlots.end(), sk) == shownSlots.end()) shownSlots.push_back (sk);
    }
    const auto plot = area.withTrimmedTop (u * 2.4f).withTrimmedBottom (u * 2.4f);
    g.setColour (colour::displayGrid.brighter (0.2f));
    g.drawRoundedRectangle (plot, u * 0.6f, 1.0f);
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    g.setFont (labelFont (u * 1.6f));
    const auto& m = core.part (pt).modMatrix();
    int line = 0;
    for (const auto& [in, k] : shownSlots)
    {
        const std::string sp = pre + (in == 0 ? "vel." : "at.");
        const std::string ks = std::to_string (k + 1);
        const int di = reg.indexOf (sp + "dst" + ks), ai = reg.indexOf (sp + "amt" + ks), ci = reg.indexOf (sp + "curve" + ks);
        if (di < 0 || ai < 0 || ci < 0) continue;
        const float amt = reg.get (ai);
        const int curve = (int) std::lround (reg.get (ci));
        const bool on = std::lround (reg.get (di)) != 0 && amt != 0.0f;
        const auto c = (on ? colour::encoder (in * 2 + k) : colour::displayDim);
        juce::Path p;
        for (int i = 0; i <= 48; ++i)
        {
            const float x = (float) i / 48.0f;
            const float y = mod::applyCurve (curve, x) * amt;
            const juce::Point<float> q (plot.getX() + plot.getWidth() * x, plot.getCentreY() - y * plot.getHeight() * 0.46f);
            if (i == 0) p.startNewSubPath (q);
            else p.lineTo (q);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (on ? u * 0.32f : u * 0.18f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // the live input: a dot on the curve
        const float x = juce::jlimit (0.0f, 1.0f, m.uiSourceValue (in == 0 ? mod::SrcVelocity : mod::SrcNotePressure));
        if (on)
            g.fillEllipse (juce::Rectangle<float> (u * 1.0f, u * 1.0f)
                               .withCentre ({ plot.getX() + plot.getWidth() * x, plot.getCentreY() - mod::applyCurve (curve, x) * amt * plot.getHeight() * 0.46f }));
        const auto label = i18n::upper (tr (in == 0 ? "Velocity" : "Aftertouch")) + " " + juce::String (k + 1) + "  "
                         + i18n::upper (formatValue (reg.descriptor (di), reg.get (di)));
        const auto row = area.withHeight (u * 2.2f).translated (0.0f, line < 2 ? 0.0f : area.getHeight() - u * 2.2f);
        g.drawText (label, row, (line % 2) == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight, true);
        ++line;
    }
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
            name = modfx::compactName (d.id, d.nameEn, d.nameDe); // 0.7: "Source", not "Part 1 Mod 3 Source"
            value = modfx::specialValueText (proc.core(), d.id, d.fromNormalised (norm));
            if (value.isEmpty()) value = paramText (proc, id, norm);
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
        // 0.7: modulation - the reach of the matrix routings above the bar, the live position as a tick
        if (const auto mr = modfx::modulationRange (proc.core(), id.toStdString()); mr.any)
        {
            auto band = bar.translated (0.0f, -bar.getHeight() * 1.6f).withHeight (bar.getHeight() * 0.7f);
            g.setColour (colour::displayText.withAlpha (0.35f));
            g.fillRoundedRectangle (band.withX (band.getX() + band.getWidth() * mr.lo).withWidth (juce::jmax (2.0f, band.getWidth() * (mr.hi - mr.lo))),
                                    band.getHeight() * 0.5f);
            g.setColour (colour::displayText);
            const float lx = band.getX() + band.getWidth() * mr.live;
            g.fillRect (lx - 1.0f, band.getY() - 1.0f, 2.0f, band.getHeight() + 2.0f);
        }
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
        g.drawFittedText (i18n::upper (k.label), r.reduced (2.0f, 0).toNearestInt(), juce::Justification::centred, 1, 0.75f); // p38: long labels narrow a little
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
    // p11: the clock moved to the transport panel; here the loop range (lift / cut work on it)
    juce::String info = i18n::upper (tr ("Loop")) + " " + clockText (reg.get ("tape.loopin")) + utf8 ("\xe2\x80\x93") + clockText (reg.get ("tape.loopout"));
    if (status.isNotEmpty()) info = status;
    paintHint (g, softKeyRow().withWidth (juce::jmax (10.0f, softKeyArea (0).getX() - softKeyRow().getX() - 4.0f)), info);
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
    if (showsModView()) // 0.7: trace of the selected routing
    {
        modfx::paintModTrace (g, hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s, core, ctx.part, ctx.modRouting, modHistory);
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
    if (showsPadsView()) // p25: what the pads do right now
    {
        const auto area = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
        auto line = area.withHeight (area.getHeight() * 0.4f).reduced (area.getHeight() * 0.05f, 0).translated (0, area.getHeight() * 0.08f);
        g.setFont (labelFont (line.getHeight() * 0.55f));
        g.setColour (colour::displayText);
        g.drawText (i18n::upper (tr ("Pads")) + "   " + padsview::statusLine (core), line, juce::Justification::centredLeft, true);
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
    if (showsHarmonyView()) // p24: what the brain does right now
    {
        const auto area = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
        auto line = area.withHeight (area.getHeight() * 0.4f).reduced (area.getHeight() * 0.05f, 0).translated (0, area.getHeight() * 0.08f);
        g.setFont (labelFont (line.getHeight() * 0.55f));
        g.setColour (colour::displayText);
        g.drawText (i18n::upper (tr ("Harmony")) + "   " + harmonyview::statusLine (core, juce::jlimit (0, Core::numParts - 1, ctx.part)), line,
                    juce::Justification::centredLeft, true);
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
    if (showsMotionView()) // p22: what the recorder does right now
    {
        const auto area = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
        auto line = area.withHeight (area.getHeight() * 0.4f).reduced (area.getHeight() * 0.05f, 0).translated (0, area.getHeight() * 0.08f);
        g.setFont (labelFont (line.getHeight() * 0.55f));
        g.setColour (colour::displayText);
        g.drawText (i18n::upper (tr ("Motion")) + "   " + motionview::statusLine (core, juce::jlimit (0, Core::numParts - 1, ctx.part)), line,
                    juce::Justification::centredLeft, true);
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
    if (showsMorphView()) // p16: what the pad does right now
    {
        const auto area = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
        auto line = area.withHeight (area.getHeight() * 0.4f).reduced (area.getHeight() * 0.05f, 0).translated (0, area.getHeight() * 0.08f);
        g.setFont (labelFont (line.getHeight() * 0.55f));
        g.setColour (colour::displayText);
        g.drawText (i18n::upper (tr ("Morph")) + "   " + morphview::statusLine (core, juce::jlimit (0, Core::numParts - 1, ctx.part)), line,
                    juce::Justification::centredLeft, true);
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
    if (showsBodyView()) // p19: model, tuning, pitch of the body
    {
        const auto area = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
        auto line1 = area.withHeight (area.getHeight() * 0.4f).reduced (area.getHeight() * 0.05f, 0).translated (0, area.getHeight() * 0.08f);
        const int pt = juce::jlimit (0, Core::numParts - 1, ctx.part);
        const bool on = core.params().get ("p" + std::to_string (pt + 1) + ".reso.on") >= 0.5f;
        g.setFont (labelFont (line1.getHeight() * 0.55f));
        g.setColour (on ? colour::displayText : colour::sunYellow);
        g.drawText (i18n::upper (tr ("Resonator")) + "   " + resoview::statusLine (core, pt), line1, juce::Justification::centredLeft, true);
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
    if (showsSpaceView()) // 1.0: motion summary, monitoring, mono check
    {
        const auto area = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
        const auto& reg = core.params();
        const auto p = "p" + std::to_string (ctx.part + 1) + ".space.";
        auto text = [&] (const std::string& id) {
            const int i = reg.indexOf (id);
            return i >= 0 ? formatValue (reg.descriptor (i), reg.get (i)) : juce::String();
        };
        const bool on = reg.get (p + "on") >= 0.5f;
        auto line1 = area.withHeight (area.getHeight() * 0.4f).reduced (area.getHeight() * 0.05f, 0).translated (0, area.getHeight() * 0.08f);
        auto line2 = line1.translated (0, area.getHeight() * 0.42f);
        g.setFont (labelFont (line1.getHeight() * 0.55f));
        g.setColour (on ? colour::displayText : colour::displayDim);
        g.drawText (i18n::upper (tr ("Motion")) + "   " + text (p + "path") + "  /  " + text (p + "pathlen") + "  /  x" + text (p + "pathrate"),
                    line1, juce::Justification::centredLeft, true);
        g.setColour (colour::displayDim);
        g.drawText (i18n::upper (tr ("3D Monitoring")) + "   " + text (Core::Ids::spaceOut)
                        + (reg.get (Core::Ids::spaceMonoCheck) >= 0.5f ? "   /   " + i18n::upper (tr ("Mono Check")) : juce::String()),
                    line2, juce::Justification::centredLeft, true);
        if (! on)
        {
            g.setColour (colour::sunYellow);
            g.drawText (tr ("Switch Space on (encoder 1) to place and move this part"), line1, juce::Justification::centredRight, true);
        }
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
    if (showsRackView()) // 0.7: what the selected effect does
    {
        auto& rack = shownRack();
        const int slot = juce::jlimit (0, rack.numSlots() - 1, ctx.fxSlot);
        const auto area = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
        if ((int) std::lround (core.params().get (rack.slotPrefix (slot) + "type")) == InsertSlot::MidSide)
        {
            // Mid/Side: the stereo picture it makes (scope of the part after its rack / of the master)
            auto plot = area;
            auto caption = plot.removeFromLeft (plot.getWidth() * 0.22f);
            g.setFont (labelFont (caption.getHeight() * 0.2f));
            g.setColour (colour::displayDim);
            const auto info = InsertSlot::typeInfo (InsertSlot::MidSide);
            g.drawFittedText (i18n::upper (juce::String (i18n::pick (info.nameEn, info.nameDe))), caption.toNearestInt(),
                              juce::Justification::centredLeft, 2, 0.8f);
            paintStereoScope (g, plot.reduced (plot.getWidth() * 0.01f, plot.getHeight() * 0.04f));
        }
        else
            modfx::paintSlotVisual (g, area, core, rack.slotPrefix (slot), rack.slot (slot));
        if (hasSoftKeys()) paintSoftKeys (g);
        return;
    }
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
    // default: 16 voice dots, the VOICES legend over the last note (p45: never overlapping), the chord that sounds
    // under the dots (+ soft keys / status, e.g. the loaded sample)
    const int voices = core.activeVoices();
    const auto l = voiceStripLayout();
    for (int i = 0; i < 16; ++i)
    {
        auto dot = juce::Rectangle<float> (l.dot, l.dot).withPosition (l.dots.getX() + (float) i * l.step, l.dots.getY());
        g.setColour (i < voices ? colour::rose : colour::displayGrid.brighter (0.2f));
        g.fillEllipse (dot);
    }
    g.setColour (colour::displayDim);
    g.setFont (labelFont (l.legendFont));
    g.drawText (i18n::upper (tr ("Voices")), l.legend, juce::Justification::centredRight, false);
    const int note = core.lastNote();
    if (note >= 0)
    {
        const auto k = proc.keyScale();
        g.setColour (colour::displayText);
        g.setFont (brandFont (l.noteFont));
        g.drawText (juce::String (keys::noteName (note, k.root, k.scale, i18n::current() == i18n::Language::German)), l.note,
                    juce::Justification::centredRight, false);
    }
    if (chordText.isNotEmpty()) // the chord: bright while it sounds, dim after (the last one stays)
    {
        g.setFont (brandFont (l.chordFont));
        g.setColour (chordSounding ? colour::ultraviolet.brighter (0.45f) : colour::displayDim);
        const float cw = juce::jmin (l.chord.getWidth(), juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), chordText) + 2.0f);
        g.drawText (chordText, l.chord.withWidth (cw), juce::Justification::centredLeft, true);
        if (chordDetail.isNotEmpty() && l.chord.getWidth() - cw > l.chordFont * 3.0f)
        {
            g.setFont (labelFont (l.chordFont * 0.62f));
            g.setColour (colour::displayDim);
            g.drawText (chordDetail, l.chord.withTrimmedLeft (cw + l.chordFont * 0.5f), juce::Justification::centredLeft, true);
        }
    }
    if (hasSoftKeys())
    {
        paintSoftKeys (g);
        paintHint (g, softKeyRow().withWidth (juce::jmax (10.0f, softKeyArea (0).getX() - softKeyRow().getX() - 4.0f)), status);
    }
    else if (flashTime > 0.0f)
        paintHint (g, s.withTop (s.getBottom() - s.getHeight() * 0.2f), {});
}

// ---------------------------------------------------------------- p45: voice strip, chord
VoiceStripLayout Display::voiceStripLayout() const
{
    const auto s = stripArea();
    const auto top = hasSoftKeys() ? s.withTrimmedBottom (softKeyRow().getHeight()) : s;
    return layoutVoiceStrip (top, hasSoftKeys(), i18n::upper (tr ("Voices")));
}

void Display::updateChord()
{
    // the notes that sound in all four parts (with the notes the harmony makes); the last chord stays, dimmed
    const auto notes = proc.core().soundingNotes();
    const auto c = chords::detect (notes);
    const bool de = i18n::current() == i18n::Language::German;
    if (c.valid())
    {
        const auto k = proc.keyScale();
        chordText = juce::String (chords::symbol (c, k.root, k.scale, de));
        chordDetail = i18n::upper (juce::String::fromUTF8 (chords::inversionName (c, de).c_str()));
        chordSounding = true;
    }
    else
        chordSounding = false;
}
} // namespace ohpe::ui
