// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Widgets.h"
#include "ModFxViews.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <algorithm>

namespace ohpe::ui
{
using i18n::tr;

void showControlMenu (OhpeProcessor& proc, const juce::String& paramId, juce::Component* target)
{
    if (paramId.isEmpty()) return;
    const auto mapping = proc.describeMidiMapping (paramId);
    const bool learning = proc.learningParamId() == paramId;
    juce::PopupMenu m;
    const int idx = proc.core().params().indexOf (paramId.toStdString());
    if (idx >= 0)
        m.addSectionHeader (i18n::pick (proc.core().params().descriptor (idx).nameEn, proc.core().params().descriptor (idx).nameDe));
    if (learning) m.addItem (3, tr ("Cancel MIDI Learn"));
    else m.addItem (1, tr ("MIDI Learn"));
    m.addItem (2, tr ("Clear MIDI mapping") + (mapping.isNotEmpty() ? " (" + mapping + ")" : juce::String()), mapping.isNotEmpty());
    m.addSeparator();
    m.addItem (4, tr ("Reset to default"));
    juce::Component::SafePointer<juce::Component> safe (target);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [&proc, paramId, safe] (int r) {
        if (r == 1) proc.armMidiLearn (paramId);
        else if (r == 2) proc.clearMidiMapping (paramId);
        else if (r == 3) proc.cancelMidiLearn();
        else if (r == 4)
            if (auto* p = proc.parameter (paramId)) p->setValueNotifyingHost (p->getDefaultValue());
        if (safe != nullptr) safe->repaint();
    });
}

// ---------------------------------------------------------------- 0.3: direct manipulation
float dragToNormalised (float startNorm, float deltaPixels, float lengthPixels, bool fine) noexcept
{
    if (! (lengthPixels > 1.0f)) lengthPixels = 1.0f;
    const float v = startNorm + deltaPixels / lengthPixels * (fine ? fineFactor : 1.0f);
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void BarDrag::begin (float norm, juce::Point<float> pos, bool fineMode) noexcept
{
    startNorm = lastNorm = norm;
    anchor = pos;
    fine = fineMode;
}

float BarDrag::update (juce::Point<float> pos, float lengthPixels, bool fineMode) noexcept
{
    if (fineMode != fine) begin (lastNorm, pos, fineMode); // switching fine control never jumps
    const float delta = (pos.x - anchor.x) - (pos.y - anchor.y);
    lastNorm = dragToNormalised (startNorm, delta, lengthPixels, fine);
    return lastNorm;
}

void setParameterNormalised (OhpeProcessor& proc, const juce::String& paramId, float norm)
{
    if (auto* p = proc.parameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        p->endChangeGesture();
    }
}

// ---------------------------------------------------------------- Encoder
Encoder::Encoder (OhpeProcessor& p, int i) : proc (p), index (i)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setColour (juce::Slider::rotarySliderFillColourId, colour::encoder (index));
    knob.setPopupMenuEnabled (false);
    knob.setMouseDragSensitivity (220);
    knob.setVelocityBasedMode (false);
    knob.setVelocityModeParameters (1.0, 1, 0.0, false); // Cmd / Ctrl mean fine control, not velocity mode
    knob.onValueChange = [this] { repaint(); };
    addAndMakeVisible (knob);
}

Encoder::~Encoder() { attachment.reset(); }

void Encoder::bindVirtual (VirtualPtr v)
{
    if (v == nullptr)
    {
        if (virt != nullptr)
        {
            virt.reset();
            knob.onValueChange = [this] { repaint(); };
            const auto id = paramId;
            paramId = {};
            bind (id);
        }
        return;
    }
    attachment.reset();
    virt = std::move (v);
    paramId = virt->paramId;
    knob.setEnabled (true);
    knob.setNormalisableRange ({ 0.0, 1.0 });
    knob.setValue (virt->get ? virt->get() : 0.0f, juce::dontSendNotification);
    knob.onValueChange = [this] {
        if (virt != nullptr && virt->set) virt->set ((float) knob.getValue());
        repaint();
    };
    repaint();
}

void Encoder::refreshVirtual()
{
    if (virt == nullptr || ! virt->get || knob.isMouseButtonDown()) return;
    const float v = virt->get();
    if (std::abs ((float) knob.getValue() - v) > 1.0e-5f)
    {
        knob.setValue (v, juce::dontSendNotification);
        repaint();
    }
}

void Encoder::bind (const juce::String& id)
{
    if (virt != nullptr)
    {
        virt.reset();
        knob.onValueChange = [this] { repaint(); };
        paramId = {};
    }
    if (id == paramId && (attachment != nullptr || id.isEmpty())) return;
    attachment.reset();
    paramId = id;
    if (auto* p = proc.parameter (id))
    {
        attachment = std::make_unique<juce::SliderParameterAttachment> (*p, knob);
        knob.setEnabled (true);
    }
    else
    {
        knob.setEnabled (false);
        knob.setValue (0.0, juce::dontSendNotification);
    }
    repaint();
}

float Encoder::normalisedValue() const
{
    if (virt != nullptr) return virt->get ? virt->get() : 0.0f;
    if (auto* p = proc.parameter (paramId)) return p->getValue();
    return 0.0f;
}

void Encoder::resized()
{
    const float s = (float) getWidth();
    knob.setBounds (getLocalBounds().withHeight ((int) (s * 0.78f)).reduced ((int) (s * 0.06f)));
}

void Encoder::setModulation (bool any, float lo, float hi, float live)
{
    auto& props = knob.getProperties();
    const bool had = props.getWithDefault ("modAny", false);
    if (! any && ! had) return;
    const float q = 1.0f / 512.0f; // repaint only on visible changes
    auto changed = [&] (const char* k, float v) { return std::abs ((float) props.getWithDefault (k, -1.0f) - v) > q; };
    if (any == had && ! changed ("modLo", lo) && ! changed ("modHi", hi) && ! changed ("modLive", live)) return;
    props.set ("modAny", any);
    props.set ("modLo", lo);
    props.set ("modHi", hi);
    props.set ("modLive", live);
    knob.repaint();
}

void Encoder::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const auto text = getLocalBounds().toFloat().withTop (w * 0.80f);
    const int idx = proc.core().params().indexOf (paramId.toStdString());
    const bool learning = paramId.isNotEmpty() && proc.learningParamId() == paramId;
    juce::String name = "-", value;
    if (virt != nullptr)
    {
        name = virt->name;
        value = virt->text ? virt->text (normalisedValue()) : juce::String();
    }
    else if (idx >= 0)
    {
        const auto& d = proc.core().params().descriptor (idx);
        name = modfx::compactName (d.id, d.nameEn, d.nameDe); // 0.7: short names in the Mod / Rack views
        if (auto* p = proc.parameter (paramId))
        {
            const float plain = p->convertFrom0to1 (p->getValue());
            value = modfx::specialValueText (proc.core(), d.id, plain);
            if (value.isEmpty()) value = formatValue (d, plain);
        }
    }
    // legend printed on the body: small caps, engraved grey
    g.setColour (learning ? colour::encoder (index) : colour::engraving);
    g.setFont (labelFont (w * 0.092f));
    g.drawText (learning ? tr ("MIDI Learn") : i18n::upper (name), text.withHeight (w * 0.13f), juce::Justification::centred, true);
    g.setColour (colour::ink);
    g.setFont (monoFont (w * 0.098f));
    g.drawText (value, text.withTrimmedTop (w * 0.13f).withHeight (w * 0.13f), juce::Justification::centred, true);
    if (virt != nullptr && virt->locked)
    {
        // lock marker: a small filled square in the encoder's colour (the value is a step lock)
        g.setColour (colour::encoder (index));
        g.fillRoundedRectangle (w * 0.05f, w * 0.03f, w * 0.08f, w * 0.08f, w * 0.015f);
    }
    if (idx >= 0 && proc.core().midiMap().isMapped (paramId.toStdString()))
    {
        g.setColour (colour::encoder (index));
        g.fillEllipse (w * 0.88f, w * 0.04f, w * 0.06f, w * 0.06f); // small dot: this parameter is MIDI-mapped
    }
}

void Encoder::Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showControlMenu (owner.proc, owner.paramId, this); return; }
    // 0.3: Shift / Cmd / Ctrl at the start of a drag = fine control
    setMouseDragSensitivity (isFineDrag (e.mods) ? (int) (220.0f / fineFactor) : 220);
    juce::Slider::mouseDown (e);
}

void Encoder::Knob::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    setMouseDragSensitivity (220);
}

void Encoder::Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (owner.virt != nullptr)
    {
        if (owner.virt->reset) owner.virt->reset();
        owner.refreshVirtual();
        owner.repaint();
        return;
    }
    if (auto* p = owner.proc.parameter (owner.paramId)) p->setValueNotifyingHost (p->getDefaultValue());
}

// ---------------------------------------------------------------- PictoKey
PictoKey::PictoKey (Picto p, juce::String legendEnglish)
    : juce::Button (legendEnglish), picto (p), legend (std::move (legendEnglish)), accent (colour::signalRed)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    // p11: keys never take the keyboard focus - Space / R are the transport shortcuts in the
    // standalone app, not "click the last key"
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

void PictoKey::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onSecondaryClick) onSecondaryClick();
        return;
    }
    juce::Button::mouseDown (e);
}

void PictoKey::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    juce::Button::mouseUp (e);
}

void drawFittedLegend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height, juce::Justification j)
{
    auto f = labelFont (height);
    const float w = juce::GlyphArrangement::getStringWidth (f, text);
    if (w > area.getWidth() && w > 0.0f) f = labelFont (juce::jmax (height * 0.7f, height * area.getWidth() / w));
    g.setFont (f);
    g.drawText (text, area, j, true);
}

void drawBaselineLegend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height, juce::Justification j, bool optical)
{
    auto f = labelFont (height);
    const float baseline = area.getY() + (area.getHeight() + f.getAscent() - f.getDescent()) * 0.5f;
    const float w = juce::GlyphArrangement::getStringWidth (f, text);
    if (w > area.getWidth() && w > 0.0f) f = f.withHorizontalScale (juce::jmax (0.7f, area.getWidth() / w));
    const bool left = j.testFlags (juce::Justification::left), right = j.testFlags (juce::Justification::right);
    if (optical && (left || right) && text.isNotEmpty())
    {
        // p47: place by the ink - lay the line out at 0, measure its outline, move it so the ink meets the edge
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, text, 0.0f, juce::roundToInt (baseline));
        juce::Path ink;
        ga.createPath (ink);
        const auto inkBox = ink.getBounds();
        ga.moveRangeOfGlyphs (0, -1, left ? area.getX() - inkBox.getX() : area.getRight() - inkBox.getRight(), 0.0f);
        ga.draw (g);
        return;
    }
    g.setFont (f);
    const int x = left ? (int) area.getX() : right ? (int) area.getRight() : (int) area.getCentreX();
    g.drawSingleLineText (text, x, juce::roundToInt (baseline), j.getOnlyHorizontalFlags());
}

namespace
{
/** Soft two-layer shadow under a rounded shape (matte objects on the housing). */
void softShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius, float lift)
{
    g.setColour (juce::Colours::black.withAlpha (0.05f));
    g.fillRoundedRectangle (r.expanded (lift * 0.6f).translated (0, lift * 1.4f), radius + lift * 0.6f);
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillRoundedRectangle (r.translated (0, lift * 0.7f), radius);
}

/** The four accent colours as one ring of four arcs (the header's round keys). */
void fourColourRing (juce::Graphics& g, juce::Point<float> c, float radius, float width)
{
    const float gap = 0.10f;
    for (int i = 0; i < 4; ++i)
    {
        const float a0 = juce::MathConstants<float>::pi * (-0.75f + 0.5f * (float) i) + gap, a1 = a0 + juce::MathConstants<float>::halfPi - 2.0f * gap;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
        g.setColour (colour::encoder (i));
        g.strokePath (arc, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
} // namespace

void PictoKey::setLed (juce::Colour c, float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (! segmented && level == ledLevel && (level == 0.0f || c == ledColour)) return;
    segmented = false;
    ledLevel = level;
    ledColour = c;
    repaint();
}

void PictoKey::setBarSegments (const std::array<juce::Colour, 4>& colours, const std::array<float, 4>& levels)
{
    std::array<float, 4> l {};
    for (size_t i = 0; i < 4; ++i) l[i] = juce::jlimit (0.0f, 1.0f, levels[i]);
    bool same = segmented && l == segLevel;
    for (size_t i = 0; i < 4 && same; ++i) same = l[i] == 0.0f || colours[i] == segColour[i];
    if (same) return;
    segmented = true;
    segColour = colours;
    segLevel = l;
    ledLevel = *std::max_element (l.begin(), l.end());
    repaint();
}

juce::Rectangle<float> PictoKey::capArea() const
{
    const auto b = getLocalBounds().toFloat();
    const float legendH = showLegend ? juce::jmin (18.0f * uiScale, b.getHeight() * 0.4f) : 0.0f;
    // joined keys: no gap on the joined side - the caps meet in a fine seam
    return b.withTrimmedBottom (legendH + (showLegend ? 2.0f : 1.0f)).withTrimmedTop (1.0f)
            .withTrimmedLeft (joinLeft ? 0.0f : 2.0f).withTrimmedRight (joinRight ? 0.0f : 2.0f);
}

juce::Rectangle<float> PictoKey::barArea() const
{
    if (! hasLed || style != Style::Cap) return {};
    const auto cap = capArea();
    const float h = juce::jmax (2.0f, 3.0f * uiScale);
    const float w = segmented ? juce::jmin (cap.getWidth() * 0.56f, 40.0f * uiScale)
                              : juce::jmin (cap.getWidth() * 0.34f, 26.0f * uiScale);
    return juce::Rectangle<float> (w, h).withCentre ({ cap.getCentreX(), cap.getBottom() - juce::jmax (5.0f, cap.getHeight() * 0.11f) });
}

namespace
{
/** A key cap's outline: rounded, square on the sides joined to a neighbour. */
juce::Path capPath (juce::Rectangle<float> r, float radius, bool joinLeft, bool joinRight)
{
    juce::Path p;
    p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius,
                           ! joinLeft, ! joinRight, ! joinLeft, ! joinRight);
    return p;
}

/** p14 rev 3: one window of a light bar: unlit a faint dark slot sunk into the cap, lit a flat colour
    with a soft glow on the cap around it (no specular glare). */
void drawBarWindow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float level, float s)
{
    const float rr = r.getHeight() * 0.5f;
    g.setColour (colour::sheen.withAlpha (0.6f)); // the lower lip of the slot
    g.fillRoundedRectangle (r.translated (0, 0.7f), rr);
    g.setColour (colour::capEdge.darker (0.05f));
    g.fillRoundedRectangle (r, rr);
    if (level <= 0.0f) return;
    for (int i = 3; i >= 1; --i) // the glow on the cap
    {
        g.setColour (colour.withAlpha (0.08f * level));
        g.fillRoundedRectangle (r.expanded (1.2f * s * (float) i, 1.0f * s * (float) i), rr + 1.2f * s * (float) i);
    }
    g.setColour (colour.withMultipliedAlpha (0.4f + 0.6f * level));
    g.fillRoundedRectangle (r, rr);
}
} // namespace

void PictoKey::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const bool pressed = down || (active && style == Style::Cap);
    // p14: header keys carry their legend above (12 units, the header's legend baseline), 4 units of air below it
    const float legendH = legendOnTop ? 16.0f * uiScale : ! showLegend ? 0.0f : juce::jmin ((style == Style::Cap ? 18.0f : 16.0f) * uiScale, b.getHeight() * 0.4f);
    const auto legendArea = legendOnTop ? b.withHeight (12.0f * uiScale) : b.withTop (b.getBottom() - legendH);
    const auto body = legendOnTop ? b.withTrimmedTop (legendH) : b.withTrimmedBottom (legendH);
    auto ink = colour::capInk;
    PictoState ps;
    const bool transportSign = picto == Picto::Play || picto == Picto::Stop || picto == Picto::Record
                            || picto == Picto::ToStart || picto == Picto::Loop || picto == Picto::OctaveDown || picto == Picto::OctaveUp;

    if (style == Style::Round)
    {
        const float d = juce::jmin (body.getWidth(), body.getHeight()) - 2.0f;
        auto ringBox = juce::Rectangle<float> (d, d).withCentre (body.getCentre());
        if (flushRight) ringBox = ringBox.withX (body.getRight() - d); // p47: the ring's outer edge on the area's right edge
        const float ringW = juce::jmax (1.5f, d * 0.07f);
        fourColourRing (g, ringBox.getCentre(), d * 0.5f - ringW * 0.5f, ringW);
        auto cap = ringBox.reduced (ringW * 1.9f);
        g.setColour (juce::Colours::black.withAlpha (0.14f));
        g.fillEllipse (cap.translated (0, down ? 0.5f : d * 0.035f));
        if (down) cap = cap.translated (0, d * 0.015f);
        g.setGradientFill (juce::ColourGradient (colour::capTop, cap.getX(), cap.getY(), colour::capBottom, cap.getX(), cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (colour::capEdge);
        g.drawEllipse (cap.reduced (0.5f), 1.0f);
        if (over && ! down) { g.setColour (colour::sheen.withAlpha (0.25f)); g.fillEllipse (cap); }
        drawPictogram (g, cap.reduced (cap.getWidth() * 0.06f), picto, ps, available ? ink : ink.withAlpha (0.3f));
    }
    else if (style == Style::Pill)
    {
        auto cap = legendOnTop ? body.withSizeKeepingCentre (body.getWidth() - 2.0f, 32.0f * uiScale) // the strips' height
                               : body.reduced (1.0f, b.getHeight() * 0.04f);
        const float r = cap.getHeight() * 0.5f;
        softShadow (g, cap, r, down ? 0.6f : 2.0f);
        if (down) cap = cap.translated (0, 1.0f);
        g.setGradientFill (juce::ColourGradient (colour::pillTop, cap.getX(), cap.getY(), colour::pill, cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, r);
        g.setColour (colour::sheen.withAlpha (over && ! down ? 0.10f : 0.05f));
        g.drawRoundedRectangle (cap.reduced (0.5f), r, 1.0f);
        const auto icon = cap.withSizeKeepingCentre (cap.getHeight(), cap.getHeight()).reduced (cap.getHeight() * 0.1f);
        drawPictogram (g, icon, picto, ps, available ? (active ? accent : colour::keyLight.withAlpha (0.9f)) : colour::keyLight.withAlpha (0.25f));
        if (active)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (juce::Rectangle<float> (cap.getHeight() * 0.5f, 2.0f).withCentre ({ cap.getCentreX(), cap.getBottom() - cap.getHeight() * 0.12f }), 1.0f);
        }
    }
    else
    {
        auto cap = capArea();
        const float r = radius::inset * uiScale; // p14: the radius system (Palette.h)
        { // soft two-layer shadow under the cap (the joined sides square)
            const float lift = pressed ? 0.8f : 2.4f;
            g.setColour (juce::Colours::black.withAlpha (0.05f));
            g.fillPath (capPath (cap.expanded (lift * 0.6f).translated (0, lift * 1.4f), r + lift * 0.6f, joinLeft, joinRight));
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            g.fillPath (capPath (cap.translated (0, lift * 0.7f), r, joinLeft, joinRight));
        }
        if (pressed) cap = cap.translated (0, 1.0f);
        const auto outline = capPath (cap, r, joinLeft, joinRight);
        const auto top = pressed ? colour::capBottom : colour::capTop;
        const auto bottom = pressed ? colour::capBottom.darker (0.03f) : colour::capBottom;
        g.setGradientFill (juce::ColourGradient (top, cap.getX(), cap.getY(), bottom, cap.getX(), cap.getBottom(), false));
        g.fillPath (outline);
        g.setColour (colour::capEdge);
        g.strokePath (capPath (cap.reduced (0.5f), r, joinLeft, joinRight), juce::PathStrokeType (1.0f));
        if (! pressed) // a fine lit upper edge: the soft depth of a matte key
        {
            g.setColour (colour::sheen.withAlpha (0.75f));
            g.fillRect (cap.withTrimmedLeft (joinLeft ? 1.0f : r).withTrimmedRight (joinRight ? 1.0f : r).withHeight (1.0f).translated (0, 1.0f));
        }
        if (over && ! down) { g.setColour (colour::sheen.withAlpha (0.22f)); g.fillPath (outline); }
        if (hasPictoInk && available) ink = pictoInk;
        else if (! available) ink = ink.withAlpha (0.28f);
        // rev 3: the light bar sits at the cap's lower edge, under the pictogram (as the lit bar of before)
        const auto bar = barArea().translated (0, pressed ? 1.0f : 0.0f);
        if (transportSign)
        {
            const float side = juce::jmin (cap.getHeight(), cap.getWidth());
            const auto sign = cap.withSizeKeepingCentre (side, side).reduced (side * 0.18f);
            if (hasLed && (picto == Picto::Play || picto == Picto::Record) && ledLevel > 0.0f && available)
            {   // rev 3: the PLAY triangle and the REC circle glow with their bar - steady when on (REC armed),
                // breathing with the bar's level while PLAY runs / REC records (p14)
                const auto c = sign.getCentre();
                const float d = sign.getHeight() * 0.62f;
                for (int i = 4; i >= 1; --i)
                {
                    g.setColour (ledColour.withAlpha (0.07f * ledLevel));
                    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c).expanded (d * 0.13f * (float) i));
                }
                if (picto == Picto::Play)
                    ink = ledColour.darker (0.25f * (1.0f - ledLevel)).interpolatedWith (ledColour.brighter (0.2f), 0.5f * ledLevel);
                else
                    ink = ink.interpolatedWith (ledColour.brighter (0.25f), 0.5f * ledLevel);
            }
            drawPictogram (g, sign, picto, ps, ink);
        }
        else if (cap.getWidth() < cap.getHeight()) // rev 3: a narrow key (MIDI in the octave row): a square sign area
            drawPictogram (g, cap.withSizeKeepingCentre (cap.getWidth(), cap.getWidth()).reduced (cap.getWidth() * 0.12f), picto, ps, ink);
        else
            drawPictogram (g, cap.reduced (cap.getWidth() * 0.2f, cap.getHeight() * 0.12f), picto, ps, ink);
        if (hasLed)
        {
            if (segmented)
            {
                const float gap = juce::jmax (1.5f, 2.0f * uiScale), w = (bar.getWidth() - 3.0f * gap) / 4.0f;
                for (int i = 0; i < 4; ++i)
                    drawBarWindow (g, bar.withWidth (w).translated ((w + gap) * (float) i, 0), segColour[(size_t) i], segLevel[(size_t) i], uiScale);
            }
            else
                drawBarWindow (g, bar, ledColour, ledLevel, uiScale);
        }
        else if (active) // the lit bar of the active key, in its function colour
        {
            g.setColour (accent);
            g.fillRoundedRectangle (juce::Rectangle<float> (cap.getWidth() * 0.3f, juce::jmax (2.0f, cap.getHeight() * 0.05f))
                                        .withCentre ({ cap.getCentreX(), cap.getBottom() - cap.getHeight() * 0.11f }), 1.5f);
        }
    }
    if (! showLegend && ! legendOnTop) return;
    g.setColour (available ? colour::engraving : colour::engraving.withAlpha (0.45f));
    if (legendOnTop) drawBaselineLegend (g, i18n::upper (tr (legend)), legendArea, 11.0f * uiScale,
                                         flushRight ? juce::Justification::centredRight : juce::Justification::centred, flushRight);
    else drawFittedLegend (g, i18n::upper (tr (legend)), legendArea, 10.5f * uiScale);
}

// ---------------------------------------------------------------- Keyboard
Keyboard::Keyboard (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setScrollButtonsVisible (false);
    setOctaveForMiddleC (4);
    setBlackNoteLengthProportion (0.62f);
    setMidiChannel (1);
    setVelocity (0.8f, true);
    refreshColours();
    setBaseOctave (3);
}

void Keyboard::refreshColours()
{
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colour::capEdge.darker (0.08f)); // p14: the gaps between the keys
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, colour::keyDark);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    repaint();
}

void Keyboard::setBaseOctave (int octave)
{
    baseOctave = juce::jlimit (0, 7, octave);
    const int low = 12 * (baseOctave + 1);
    setAvailableRange (low, juce::jmin (127, low + 12 * numOctaves));
    resized();
}

void Keyboard::resized()
{
    setKeyWidth ((float) getWidth() / (float) (7 * numOctaves + 1)); // three octaves + top C = 22 white keys
    setLowestVisibleKey (12 * (baseOctave + 1));
    juce::MidiKeyboardComponent::resized();
}

void Keyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    // matte keys with a narrow gap, a soft shadow under the front edge; pressed keys sink a little
    auto key = area.reduced (juce::jmax (1.0f, area.getWidth() * 0.035f), 0).withTrimmedBottom (area.getHeight() * 0.03f);
    const float r = key.getWidth() * 0.09f;
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRoundedRectangle (key.translated (0, isDown ? 1.0f : 2.5f), r);
    if (isDown) key = key.withTrimmedTop (1.0f).translated (0, 1.0f);
    const auto top = isDown ? colour::keyLightPressed : (isOver ? colour::keyLight.brighter (0.03f) : colour::keyLight);
    g.setGradientFill (juce::ColourGradient (top, 0, key.getY(), top.darker (0.04f), 0, key.getBottom(), false));
    g.fillRoundedRectangle (key, r);
    g.setColour (colour::capEdge.withAlpha (0.7f));
    g.drawRoundedRectangle (key.reduced (0.5f), r, 1.0f);
    if (isDown)
    {
        g.setColour (colour::signalRed);
        g.fillRoundedRectangle (key.withTop (key.getBottom() - juce::jmax (2.0f, key.getHeight() * 0.03f)).reduced (key.getWidth() * 0.28f, 0)
                                    .translated (0, -key.getHeight() * 0.03f), 1.0f);
    }
    if (note % 12 == 0)
    {
        g.setColour (colour::keyInk); // p32: readable on the natural keys of both skins
        g.setFont (labelFont (juce::jmin (key.getWidth() * 0.34f, 13.0f * (float) getHeight() / 136.0f)));
        g.drawText ("C" + juce::String (note / 12 - 1), key.withTop (key.getBottom() - key.getWidth() * 0.9f).withTrimmedLeft (key.getWidth() * 0.16f),
                    juce::Justification::centredLeft);
    }
}

void Keyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto key = area.reduced (area.getWidth() * 0.08f, 0);
    const float r = key.getWidth() * 0.14f;
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (key.translated (0, isDown ? 1.0f : 3.0f), r);
    const auto base = isDown ? colour::keyDark.brighter (0.22f) : (isOver ? colour::keyDarkTop : colour::keyDark);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), 0, key.getY(), base, 0, key.getBottom(), false));
    g.fillRoundedRectangle (key, r);
    // the lit front face of a matte black key
    g.setColour (colour::sheen.withAlpha (0.07f));
    g.fillRoundedRectangle (key.withTop (key.getBottom() - key.getHeight() * 0.1f).reduced (key.getWidth() * 0.1f, 0), r * 0.6f);
    if (isDown)
    {
        g.setColour (colour::signalRed);
        g.fillRoundedRectangle (key.withTop (key.getBottom() - juce::jmax (2.0f, key.getHeight() * 0.04f)).reduced (key.getWidth() * 0.25f, 0)
                                    .translated (0, -key.getHeight() * 0.14f), 1.0f);
    }
}

// ---------------------------------------------------------------- p14: performance wheels
Wheel::Wheel (OhpeProcessor& p, juce::String paramId, juce::String legendEnglish, bool springBack)
    : proc (p), id (std::move (paramId)), legend (std::move (legendEnglish)), spring (springBack)
{
    setName (legend);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setWantsKeyboardFocus (false);
}

float Wheel::value() const
{
    if (auto* q = proc.parameter (id)) return q->getValue();
    return spring ? 0.5f : 0.0f;
}

void Wheel::set (float norm)
{
    if (auto* q = proc.parameter (id)) q->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    refresh();
}

void Wheel::refresh()
{
    // p28: follow whichever moved last - the parameter (mouse, host automation, MIDI learn) or the controller's wheel
    const auto& core = proc.core();
    const bool bend = spring;
    const uint32_t seq = bend ? core.midiWheelBendCount() : core.midiWheelModCount();
    const float param = value();
    float next = shown < 0.0f ? param : shown;
    if (! midiInit) { midiInit = true; midiSeen = seq; next = param; }
    else if (seq != midiSeen && ! dragging)
    {
        midiSeen = seq;
        next = bend ? 0.5f + 0.5f * core.midiWheelBend() : core.midiWheelMod();
    }
    else if (std::abs (param - lastParam) > 1.0e-6f) next = param;
    if (seq != midiSeen) midiSeen = seq; // while dragging: the mouse wins
    lastParam = param;
    next = juce::jlimit (0.0f, 1.0f, next);
    if (std::abs (next - shown) > 1.0e-4f) { shown = next; repaint(); }
}

juce::Rectangle<float> Wheel::slotArea() const
{
    auto b = getLocalBounds().toFloat();
    return b.withTrimmedBottom (b.getHeight() * 0.19f);
}

void Wheel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto slot = slotArea();
    const float r = slot.getWidth() * 0.22f;
    // the recess in the housing
    g.setGradientFill (juce::ColourGradient (colour::bezelLow, 0, slot.getY(), colour::bezel, 0, slot.getBottom(), false));
    g.fillRoundedRectangle (slot, r);
    g.setColour (colour::sheen.withAlpha (0.6f));
    g.drawRoundedRectangle (slot.expanded (0.5f), r + 0.5f, 1.0f);
    // the wheel: a matte graphite cylinder, lit in the middle, ribs that roll with the value
    auto wheel = slot.reduced (slot.getWidth() * 0.16f, slot.getWidth() * 0.12f);
    juce::ColourGradient cyl (colour::keyDark.darker (0.5f), 0, wheel.getY(), colour::keyDark.darker (0.5f), 0, wheel.getBottom(), false);
    cyl.addColour (0.5, colour::keyDarkTop.brighter (0.25f));
    g.setGradientFill (cyl);
    g.fillRoundedRectangle (wheel, wheel.getWidth() * 0.18f);
    const float v = shownValue(); // p28: also a controller's wheel
    const float travel = (spring ? v - 0.5f : v) * 2.6f;
    const int ribs = 14;
    for (int i = 0; i < ribs; ++i)
    {
        const float ph = std::fmod ((float) i / (float) ribs + travel * 0.25f + 10.0f, 1.0f); // 0..1 around the visible half
        const float a = (ph - 0.5f) * juce::MathConstants<float>::pi;                         // -pi/2..pi/2
        const float y = wheel.getCentreY() + std::sin (a) * wheel.getHeight() * 0.47f;
        const float lit = std::cos (a);
        g.setColour (juce::Colours::black.withAlpha (0.25f + 0.35f * lit));
        g.fillRect (juce::Rectangle<float> (wheel.getX() + wheel.getWidth() * 0.12f, y - 0.5f, wheel.getWidth() * 0.76f, juce::jmax (1.0f, lit * 1.4f)));
    }
    // the centre mark of the pitch wheel / the position line of the mod wheel
    const float markY = wheel.getCentreY() - (spring ? (v - 0.5f) : (v - 0.5f)) * wheel.getHeight() * 0.8f;
    g.setColour (spring ? colour::displayText : colour::sunYellow);
    g.fillRoundedRectangle (juce::Rectangle<float> (wheel.getWidth() * 0.86f, juce::jmax (1.5f, wheel.getHeight() * 0.02f))
                                .withCentre ({ wheel.getCentreX(), markY }), 1.0f);
    const bool learning = proc.learningParamId() == id;
    g.setColour (learning ? colour::signalRed : colour::engraving);
    drawFittedLegend (g, learning ? tr ("MIDI Learn") : i18n::upper (tr (legend)), b.withTop (slot.getBottom() + b.getHeight() * 0.03f),
                      juce::jmin (12.0f * b.getWidth() / 40.0f, b.getHeight() * 0.11f));
    if (proc.core().midiMap().isMapped (id.toStdString()))
    {
        g.setColour (colour::signalRed);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ slot.getRight() - 1.0f, slot.getY() + 1.0f }));
    }
}

void Wheel::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showControlMenu (proc, id, this); return; }
    stopTimer();
    dragging = true;
    if (auto* q = proc.parameter (id)) q->beginChangeGesture();
    drag.begin (value(), e.position, isFineDrag (e.mods));
}

void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    // up = more; the slot's height covers the whole range
    set (drag.update ({ drag.anchor.x, e.position.y }, slotArea().getHeight(), isFineDrag (e.mods)));
}

void Wheel::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || ! dragging) return;
    dragging = false;
    if (spring) startTimerHz (60); // springs back to the centre
    else if (auto* q = proc.parameter (id)) q->endChangeGesture();
}

void Wheel::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* q = proc.parameter (id)) set (q->getDefaultValue());
}

void Wheel::timerCallback()
{
    const float v = value();
    const float next = std::abs (v - 0.5f) < 0.004f ? 0.5f : 0.5f + (v - 0.5f) * 0.55f; // ~70 ms back to the centre
    set (next);
    if (next == 0.5f)
    {
        stopTimer();
        if (auto* q = proc.parameter (id)) q->endChangeGesture();
    }
}

// ---------------------------------------------------------------- p14: slide switch (glide mode)
SlideSwitch::SlideSwitch (OhpeProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
}

void SlideSwitch::bind (const juce::String& paramId)
{
    id = paramId;
    shown = -1;
    refresh();
}

int SlideSwitch::position() const
{
    if (auto* q = proc.parameter (id)) return juce::jlimit (0, 2, (int) std::lround (q->convertFrom0to1 (q->getValue())));
    return 0;
}

void SlideSwitch::refresh()
{
    if (const int p = position(); p != shown) { shown = p; repaint(); }
}

// p14 rev 3: design units (128 x 72): the caption PORTAMENTO on the text column (x 64, as GLIDE's), the three
// positions under it; the slot centred in the 56-unit control column (the GLIDE knob's width) beside them
juce::Rectangle<float> SlideSwitch::slotArea() const
{
    const float u = uiScale;
    return { 19.0f * u, 16.0f * u, 18.0f * u, juce::jmax (8.0f, (float) getHeight() - 16.0f * u) };
}

juce::Rectangle<float> SlideSwitch::labelArea (int i) const
{
    const auto s = slotArea();
    const float h = s.getHeight() / 3.0f, x = captionCol::textX * uiScale;
    return { x, s.getY() + h * (float) i, (float) getWidth() - x, h };
}

juce::Rectangle<float> SlideSwitch::captionArea() const
{
    const float x = captionCol::textX * uiScale;
    return { x, 2.0f * uiScale, (float) getWidth() - x, captionCol::boxH * uiScale };
}

void SlideSwitch::paint (juce::Graphics& g)
{
    const auto s = slotArea();
    const float r = s.getWidth() * 0.5f;
    g.setGradientFill (juce::ColourGradient (colour::bezelLow, 0, s.getY(), colour::bezel, 0, s.getBottom(), false));
    g.fillRoundedRectangle (s, r);
    g.setColour (colour::sheen.withAlpha (0.6f));
    g.drawRoundedRectangle (s.expanded (0.5f), r + 0.5f, 1.0f);
    const int pos = position();
    const float h = s.getHeight() / 3.0f;
    auto knob = juce::Rectangle<float> (s.getWidth() - 4.0f, h - 2.0f).withCentre ({ s.getCentreX(), s.getY() + h * ((float) pos + 0.5f) });
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (knob.translated (0, 1.0f), knob.getWidth() * 0.3f);
    g.setGradientFill (juce::ColourGradient (colour::capTop, 0, knob.getY(), colour::capBottom, 0, knob.getBottom(), false));
    g.fillRoundedRectangle (knob, knob.getWidth() * 0.3f);
    g.setColour (colour::capEdge);
    g.drawLine (knob.getX() + 3.0f, knob.getCentreY(), knob.getRight() - 3.0f, knob.getCentreY(), 1.0f);
    const int idx = proc.core().params().indexOf (id.toStdString());
    for (int i = 0; i < 3; ++i)
    {
        juce::String label;
        if (idx >= 0)
        {
            const auto& d = proc.core().params().descriptor (idx);
            if (i < (int) d.choices.size()) label = i18n::pick (d.choices[(size_t) i], i < (int) d.choicesDe.size() ? d.choicesDe[(size_t) i] : d.choices[(size_t) i]);
        }
        g.setColour (i == pos ? colour::ink : colour::engraving.withAlpha (0.8f));
        drawFittedLegend (g, i18n::upper (label), labelArea (i), juce::jmin (labelArea (i).getHeight() * 0.62f, 10.0f * uiScale),
                          juce::Justification::centredLeft);
    }
    const bool learning = id.isNotEmpty() && proc.learningParamId() == id;
    if (caption.isNotEmpty()) // rev 3: the caption in GLIDE's typography, on GLIDE's text column
    {
        g.setColour (learning ? colour::signalRed : colour::engraving);
        drawBaselineLegend (g, learning ? tr ("MIDI Learn") : i18n::upper (tr (caption)), captionArea(), captionCol::height * uiScale, juce::Justification::left);
    }
    if (learning)
    {
        g.setColour (colour::signalRed);
        g.drawRoundedRectangle (s.expanded (1.5f), r + 1.5f, 1.2f);
    }
}

void SlideSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showControlMenu (proc, id, this); return; }
    int target = -1;
    for (int i = 0; i < 3; ++i)
        if (labelArea (i).withLeft (0).contains (e.position)) target = i;
    if (target < 0) target = (position() + 1) % 3;
    if (auto* q = proc.parameter (id))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (q->convertTo0to1 ((float) target));
        q->endChangeGesture();
    }
    refresh();
}

// ---------------------------------------------------------------- p14: mini knob (glide)
MiniKnob::MiniKnob (OhpeProcessor& p, juce::String legendEnglish) : proc (p), legend (std::move (legendEnglish))
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setColour (juce::Slider::rotarySliderFillColourId, colour::capInk);
    knob.setPopupMenuEnabled (false);
    knob.setMouseDragSensitivity (180);
    knob.setVelocityModeParameters (1.0, 1, 0.0, false);
    knob.onValueChange = [this] { repaint(); };
    knob.setWantsKeyboardFocus (false);
    addAndMakeVisible (knob);
}

MiniKnob::~MiniKnob() { attachment.reset(); }

void MiniKnob::bind (const juce::String& paramId)
{
    if (paramId == id && attachment != nullptr) return;
    attachment.reset();
    id = paramId;
    if (auto* q = proc.parameter (id)) attachment = std::make_unique<juce::SliderParameterAttachment> (*q, knob);
    repaint();
}

void MiniKnob::resized()
{
    const int d = juce::jmin (getHeight(), getWidth() / 2, juce::roundToInt ((captionCol::textX - 8.0f) * uiScale));
    knob.setBounds (0, (getHeight() - d) / 2, d, d);
}

juce::Rectangle<float> MiniKnob::captionArea() const
{
    // rev 3: the caption box ends 2 units above the knob's centre line, the value sits under it
    const float x = captionCol::textX * uiScale;
    return { x, (float) getHeight() * 0.5f - (captionCol::boxH + 2.0f) * uiScale, (float) getWidth() - x, captionCol::boxH * uiScale };
}

void MiniKnob::paint (juce::Graphics& g)
{
    const auto cap = captionArea();
    const bool learning = id.isNotEmpty() && proc.learningParamId() == id;
    g.setColour (learning ? colour::signalRed : colour::engraving);
    drawBaselineLegend (g, learning ? tr ("MIDI Learn") : i18n::upper (tr (legend)), cap, captionCol::height * uiScale, juce::Justification::left);
    juce::String value;
    const int idx = proc.core().params().indexOf (id.toStdString());
    if (idx >= 0)
        if (auto* q = proc.parameter (id)) value = formatValue (proc.core().params().descriptor (idx), q->convertFrom0to1 (q->getValue()));
    g.setColour (colour::ink);
    g.setFont (monoFont (11.0f * uiScale));
    g.drawText (value, cap.withY ((float) getHeight() * 0.5f + 2.0f * uiScale).withHeight (14.0f * uiScale), juce::Justification::centredLeft, true);
    if (idx >= 0 && proc.core().midiMap().isMapped (id.toStdString()))
    {
        g.setColour (colour::signalRed);
        g.fillEllipse ((float) knob.getRight() - 4.0f, (float) knob.getY(), 4.0f, 4.0f);
    }
}

void MiniKnob::Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showControlMenu (owner.proc, owner.id, this); return; }
    setMouseDragSensitivity (isFineDrag (e.mods) ? (int) (180.0f / fineFactor) : 180);
    juce::Slider::mouseDown (e);
}

void MiniKnob::Knob::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    setMouseDragSensitivity (180);
}

void MiniKnob::Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* q = owner.proc.parameter (owner.id)) q->setValueNotifyingHost (q->getDefaultValue());
}

// ---------------------------------------------------------------- p15: LIFE light bar
LifeBar::LifeBar (OhpeProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setName ("LifeBar");
}

void LifeBar::bind (const juce::String& paramId)
{
    if (paramId == id && param != nullptr) return;
    id = paramId;
    param = proc.parameter (id);
    shown = -1.0f;
    repaint();
}

float LifeBar::normalisedValue() const { return param != nullptr ? param->getValue() : 0.0f; }

juce::Rectangle<float> LifeBar::slotArea() const
{
    const auto b = getLocalBounds().toFloat();
    const float h = 10.0f * uiScale;
    return { b.getX(), b.getBottom() - h - 2.0f * uiScale, b.getWidth(), h };
}

void LifeBar::tick (float seconds, bool sounding, float modDelta)
{
    const float v = normalisedValue();
    const bool nowAlive = sounding && v > 0.0f;
    const bool learning = id.isNotEmpty() && proc.learningParamId() == id;
    time += seconds;
    if (nowAlive || nowAlive != alive || std::abs (v - shown) > 1.0e-6f || std::abs (modDelta - mod) > 1.0e-4f || learning != wasLearning)
        repaint();
    alive = nowAlive;
    mod = modDelta;
    wasLearning = learning;
}

void LifeBar::paint (juce::Graphics& g)
{
    const float u = uiScale;
    const auto b = getLocalBounds().toFloat();
    shown = normalisedValue();
    const bool learning = id.isNotEmpty() && proc.learningParamId() == id;
    // the legend and the value on one line, above the slot
    const auto line = b.withHeight (12.0f * u);
    juce::String value;
    const int idx = proc.core().params().indexOf (id.toStdString());
    if (idx >= 0 && param != nullptr) value = formatValue (proc.core().params().descriptor (idx), param->convertFrom0to1 (shown));
    g.setColour (colour::ink);
    g.setFont (monoFont (11.0f * u));
    g.drawText (value, line, juce::Justification::centredRight, false);
    g.setColour (learning ? colour::signalRed : colour::engraving);
    drawBaselineLegend (g, learning ? i18n::upper (i18n::tr ("MIDI Learn")) : i18n::upper (i18n::tr ("Life")), line.withTrimmedRight (60.0f * u),
                        11.0f * u, juce::Justification::left);
    if (idx >= 0 && proc.core().midiMap().isMapped (id.toStdString()))
    {
        g.setColour (colour::signalRed);
        g.fillEllipse (line.getRight() - 64.0f * u, line.getCentreY() - 2.0f * u, 4.0f * u, 4.0f * u);
    }

    // the slot: dark glass sunk into the panel (a light lower lip), the light bar inside it
    const auto slot = slotArea();
    const float r = slot.getHeight() * 0.5f;
    g.setColour (colour::sheen.withAlpha (0.7f));
    g.fillRoundedRectangle (slot.translated (0, 1.0f), r);
    g.setColour (colour::display);
    g.fillRoundedRectangle (slot, r);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (slot.reduced (0.5f), r, 1.0f);
    const auto inner = slot.reduced (2.0f * u);
    const float ir = inner.getHeight() * 0.5f;
    // unlit: a faint track
    g.setColour (colour::rose.withAlpha (0.10f));
    g.fillRoundedRectangle (inner, ir);
    if (shown > 0.0f)
    {
        const auto lit = inner.withWidth (juce::jmax (inner.getHeight(), inner.getWidth() * shown));
        const auto base = colour::rose;
        if (alive)
        {
            // the shimmer: a soft brighter crest wanders along the lit bar, faster with more life
            const float speed = 0.12f + 0.35f * shown;
            const float pos = std::fmod (time * speed, 1.0f);
            juce::ColourGradient grad (base.withMultipliedBrightness (0.78f), lit.getX(), 0, base.withMultipliedBrightness (0.78f), lit.getRight(), 0, false);
            grad.addColour (juce::jlimit (0.0, 1.0, (double) pos - 0.18), base.withMultipliedBrightness (0.78f));
            grad.addColour ((double) pos, base.brighter (0.35f));
            grad.addColour (juce::jlimit (0.0, 1.0, (double) pos + 0.18), base.withMultipliedBrightness (0.78f));
            g.setGradientFill (grad);
        }
        else
            g.setColour (base.withMultipliedBrightness (0.9f));
        g.fillRoundedRectangle (lit, ir);
        g.setColour (base.withAlpha (0.25f)); // a soft glow around the lit glass
        g.drawRoundedRectangle (lit.expanded (0.5f), ir + 0.5f, 1.0f);
    }
    if (std::abs (mod) > 1.0e-4f) // the modulation matrix moves it: a fine light tick at the live value
    {
        const float live = juce::jlimit (0.0f, 1.0f, shown + mod);
        const float x = inner.getX() + inner.getWidth() * live;
        g.setColour (colour::displayText);
        g.fillRect (juce::Rectangle<float> (1.5f * u, inner.getHeight() + 2.0f * u).withCentre ({ x, inner.getCentreY() }));
    }
    if (learning)
    {
        g.setColour (colour::signalRed);
        g.drawRoundedRectangle (slot.expanded (1.5f * u), r + 1.5f * u, 1.2f);
    }
}

void LifeBar::mouseDown (const juce::MouseEvent& e)
{
    if (param == nullptr) return;
    if (e.mods.isPopupMenu()) { showControlMenu (proc, id, this); return; }
    const bool fine = isFineDrag (e.mods);
    float start = param->getValue();
    const auto slot = slotArea();
    if (! fine && e.position.y >= slot.getY() - slot.getHeight()) // a click on the slot jumps there
        start = juce::jlimit (0.0f, 1.0f, (e.position.x - slot.getX()) / juce::jmax (1.0f, slot.getWidth()));
    param->beginChangeGesture();
    dragging = true;
    drag.begin (start, e.position, fine);
    param->setValueNotifyingHost (start);
    repaint();
}

void LifeBar::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || param == nullptr) return;
    param->setValueNotifyingHost (drag.update (e.position, juce::jmax (40.0f, slotArea().getWidth()), isFineDrag (e.mods)));
    repaint();
}

void LifeBar::mouseUp (const juce::MouseEvent&)
{
    if (dragging && param != nullptr) param->endChangeGesture();
    dragging = false;
}

void LifeBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (param != nullptr && ! e.mods.isPopupMenu()) setParameterNormalised (proc, id, param->getDefaultValue());
    repaint();
}

// ---------------------------------------------------------------- p14 rev 3: readout glass
void Readout::setContent (const juce::String& b, juce::Colour c, const juce::String& m, const juce::String& a)
{
    if (b == badge && c == badgeColour && m == main && a == aside) return;
    badge = b;
    badgeColour = c;
    main = m;
    aside = a;
    repaint();
}

void Readout::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float u = uiScale, r = radius::inset * u;
    // a glass sunk into the panel: a light lower lip, the dark glass, a fine shadow under the upper edge
    g.setColour (colour::sheen.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds.translated (0, 1.0f), r);
    auto glass = bounds.withTrimmedBottom (1.0f);
    g.setColour (colour::display);
    g.fillRoundedRectangle (glass, r);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (glass.reduced (0.5f), r, 1.0f);
    auto in = glass.reduced (8.0f * u, 4.0f * u);
    if (hint.isNotEmpty()) // p47: a short hint in place of the readout
    {
        g.setColour (colour::displayText);
        drawFittedLegend (g, hint, in, juce::jmin (in.getHeight() * 0.62f, 11.0f * u), juce::Justification::centredLeft);
        return;
    }
    const bool oneRow = glass.getWidth() > glass.getHeight() * 3.0f;
    if (oneRow)
    {
        const float h = in.getHeight();
        g.setColour (badgeColour);
        auto badgeArea = in.removeFromLeft (juce::jmin (in.getWidth() * 0.3f, 44.0f * u));
        drawFittedLegend (g, badge, badgeArea, juce::jmin (h * 0.62f, 11.0f * u), juce::Justification::centredLeft);
        g.setColour (colour::displayDim);
        g.setFont (monoFont (juce::jmin (h * 0.6f, 11.0f * u)));
        const float asideW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), aside) + 2.0f;
        g.drawText (aside, in.removeFromRight (asideW), juce::Justification::centredRight, false);
        g.setColour (colour::displayText);
        g.setFont (monoFont (juce::jmin (h * 0.8f, 15.0f * u)));
        g.drawText (main, in, juce::Justification::centredLeft, true);
    }
    else
    {
        auto top = in.removeFromTop (in.getHeight() * 0.42f);
        g.setColour (colour::displayDim);
        g.setFont (monoFont (juce::jmin (top.getHeight() * 0.85f, 10.0f * u)));
        g.drawText (aside, top, juce::Justification::centredRight, false);
        g.setColour (badgeColour);
        drawFittedLegend (g, badge, top.withTrimmedRight (top.getWidth() * 0.45f), juce::jmin (top.getHeight() * 0.9f, 10.0f * u), juce::Justification::centredLeft);
        g.setColour (colour::displayText);
        g.setFont (monoFont (juce::jmin (in.getHeight() * 0.85f, 15.0f * u)));
        g.drawText (main, in, juce::Justification::centredLeft, true);
    }
}
} // namespace ohpe::ui
