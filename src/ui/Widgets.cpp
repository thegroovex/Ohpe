// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Widgets.h"

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
        name = i18n::pick (d.nameEn, d.nameDe);
        if (auto* p = proc.parameter (paramId)) value = formatValue (d, p->convertFrom0to1 (p->getValue()));
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

void PictoKey::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const float legendH = b.getHeight() * 0.22f;
    auto cap = b.withTrimmedBottom (legendH).reduced (b.getWidth() * 0.04f);
    const float r = cap.getWidth() * 0.08f;
    // soft drop shadow on the aluminium
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (cap.translated (0, down ? 0.5f : cap.getHeight() * 0.04f), r);
    if (down) cap = cap.translated (0, cap.getHeight() * 0.02f);
    g.setGradientFill (juce::ColourGradient (colour::keyDarkTop, cap.getX(), cap.getY(), colour::keyDark, cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, r);
    if (over && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (cap, r);
    }
    PictoState s;
    const auto ink = available ? (active ? accent : colour::keyLight.withAlpha (0.85f)) : colour::keyLight.withAlpha (0.25f);
    drawPictogram (g, cap.reduced (cap.getWidth() * 0.2f), picto, s, ink);
    if (active)
    {
        g.setColour (accent);
        g.fillEllipse (cap.getRight() - cap.getWidth() * 0.16f, cap.getY() + cap.getWidth() * 0.08f, cap.getWidth() * 0.08f, cap.getWidth() * 0.08f);
    }
    g.setColour (available ? colour::engraving : colour::engraving.withAlpha (0.45f));
    g.setFont (labelFont (juce::jmin (legendH * 0.62f, b.getWidth() * 0.16f)));
    g.drawText (i18n::upper (tr (legend)), b.withTop (b.getBottom() - legendH), juce::Justification::centred, true);
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
