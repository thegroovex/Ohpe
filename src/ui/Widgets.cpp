// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Widgets.h"
#include "ModFxViews.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

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
    g.setFont (labelFont (w * 0.105f));
    g.drawText (learning ? tr ("MIDI Learn") : i18n::upper (name), text.withHeight (w * 0.13f), juce::Justification::centred, true);
    g.setColour (colour::ink);
    g.setFont (monoFont (w * 0.11f));
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

void PictoKey::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const bool pressed = down || (active && style == Style::Cap);
    const float legendH = juce::jmin (20.0f * b.getHeight() / 80.0f, b.getHeight() * 0.27f);
    const auto legendArea = b.withTop (b.getBottom() - legendH);
    auto ink = colour::capInk;
    PictoState ps;
    const bool transportSign = picto == Picto::Play || picto == Picto::Stop || picto == Picto::Record
                            || picto == Picto::ToStart || picto == Picto::Loop || picto == Picto::OctaveDown || picto == Picto::OctaveUp;

    if (style == Style::Round)
    {
        const float d = juce::jmin (b.getWidth(), b.getHeight() - legendH) - 2.0f;
        const auto ringBox = juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), (b.getHeight() - legendH) * 0.5f });
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
        if (over && ! down) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillEllipse (cap); }
        drawPictogram (g, cap.reduced (cap.getWidth() * 0.2f), picto, ps, available ? ink : ink.withAlpha (0.3f));
    }
    else if (style == Style::Pill)
    {
        auto cap = b.withTrimmedBottom (legendH).reduced (1.0f, b.getHeight() * 0.04f);
        const float r = cap.getHeight() * 0.5f;
        softShadow (g, cap, r, down ? 0.6f : 2.0f);
        if (down) cap = cap.translated (0, 1.0f);
        g.setGradientFill (juce::ColourGradient (colour::pillTop, cap.getX(), cap.getY(), colour::pill, cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, r);
        g.setColour (juce::Colours::white.withAlpha (over && ! down ? 0.10f : 0.05f));
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
        auto cap = b.withTrimmedBottom (legendH + 2.0f).reduced (2.0f, 1.0f);
        const float r = juce::jmin (cap.getWidth(), cap.getHeight()) * 0.12f;
        softShadow (g, cap, r, pressed ? 0.8f : 2.4f);
        if (pressed) cap = cap.translated (0, 1.0f);
        const auto top = pressed ? colour::capBottom : colour::capTop;
        const auto bottom = pressed ? colour::capBottom.darker (0.03f) : colour::capBottom;
        g.setGradientFill (juce::ColourGradient (top, cap.getX(), cap.getY(), bottom, cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, r);
        g.setColour (colour::capEdge);
        g.drawRoundedRectangle (cap.reduced (0.5f), r, 1.0f);
        if (! pressed) // a fine lit upper edge: the soft depth of a matte key
        {
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.fillRect (cap.reduced (r, 0).withHeight (1.0f).translated (0, 1.0f));
        }
        if (over && ! down) { g.setColour (juce::Colours::white.withAlpha (0.22f)); g.fillRoundedRectangle (cap, r); }
        if (hasPictoInk && available) ink = pictoInk;
        else if (! available) ink = ink.withAlpha (0.28f);
        if (transportSign)
            drawPictogram (g, cap.withSizeKeepingCentre (cap.getHeight(), cap.getHeight()).reduced (cap.getHeight() * 0.18f), picto, ps, ink);
        else
            drawPictogram (g, cap.reduced (cap.getWidth() * 0.22f, cap.getHeight() * 0.16f).translated (0, -cap.getHeight() * 0.04f), picto, ps, ink);
        if (active) // the lit bar of the active key, in its function colour
        {
            g.setColour (accent);
            g.fillRoundedRectangle (juce::Rectangle<float> (cap.getWidth() * 0.3f, juce::jmax (2.0f, cap.getHeight() * 0.05f))
                                        .withCentre ({ cap.getCentreX(), cap.getBottom() - cap.getHeight() * 0.11f }), 1.5f);
        }
    }
    g.setColour (available ? colour::engraving : colour::engraving.withAlpha (0.45f));
    drawFittedLegend (g, i18n::upper (tr (legend)), legendArea, juce::jmin (legendH * 0.56f, 11.0f * b.getHeight() / 80.0f));
}

// ---------------------------------------------------------------- Keyboard
Keyboard::Keyboard (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setScrollButtonsVisible (false);
    setOctaveForMiddleC (4);
    setBlackNoteLengthProportion (0.6f);
    setMidiChannel (1);
    setVelocity (0.8f, true);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colour::keyLight);
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, colour::keyDark);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setBaseOctave (3);
}

void Keyboard::setBaseOctave (int octave)
{
    baseOctave = juce::jlimit (0, 7, octave);
    const int low = 12 * (baseOctave + 1);
    setAvailableRange (low, juce::jmin (127, low + 24));
    resized();
}

void Keyboard::resized()
{
    setKeyWidth ((float) getWidth() / 15.0f); // 2 octaves + top C = 15 white keys
    setLowestVisibleKey (12 * (baseOctave + 1));
    juce::MidiKeyboardComponent::resized();
}

void Keyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    auto key = area.reduced (area.getWidth() * 0.05f, 0).withTrimmedBottom (area.getHeight() * 0.02f);
    const float r = key.getWidth() * 0.1f;
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillRoundedRectangle (key.translated (0, isDown ? 1.0f : 3.0f), r);
    g.setColour (isDown ? colour::keyLightPressed : (isOver ? colour::keyLight.brighter (0.05f) : colour::keyLight));
    g.fillRoundedRectangle (key, r);
    if (isDown)
    {
        g.setColour (colour::signalRed);
        g.fillRoundedRectangle (key.withTop (key.getBottom() - key.getHeight() * 0.05f).reduced (key.getWidth() * 0.25f, 0), r * 0.5f);
    }
    if (note % 12 == 0)
    {
        g.setColour (colour::engraving);
        g.setFont (labelFont (key.getWidth() * 0.28f));
        g.drawText ("C" + juce::String (note / 12 - 1), key.withTop (key.getBottom() - key.getWidth() * 0.6f), juce::Justification::centred);
    }
}

void Keyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto key = area.reduced (area.getWidth() * 0.06f, 0);
    const float r = key.getWidth() * 0.12f;
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (key.translated (0, isDown ? 1.0f : 3.0f), r);
    g.setColour (isDown ? colour::keyDark.brighter (0.25f) : (isOver ? colour::keyDarkTop : colour::keyDark));
    g.fillRoundedRectangle (key, r);
    if (isDown)
    {
        g.setColour (colour::signalRed);
        g.fillRoundedRectangle (key.withTop (key.getBottom() - key.getHeight() * 0.06f).reduced (key.getWidth() * 0.25f, 0), r * 0.5f);
    }
}
} // namespace ohpe::ui
