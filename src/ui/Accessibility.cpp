// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Accessibility.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"
#include "plugin/PluginProcessor.h"
#include "settings/UserSettings.h"

namespace ohpe::ui::a11y
{
float keyStep (int steps, bool fine, bool page) noexcept
{
    if (steps > 1 && steps <= maxSteps)
    {
        const float one = 1.0f / (float) (steps - 1);
        return page ? juce::jmin (1.0f, one * (steps > 24 ? 10.0f : 3.0f)) : one;
    }
    return page ? 0.1f : (fine ? 0.001f : 0.01f);
}

bool handleValueKey (const juce::KeyPress& key, const ValueAccess& v)
{
    if (! v.get || ! v.set) return false;
    if (v.enabled && ! v.enabled()) return false;
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        if (! v.reset) return false;
        v.reset();
        return true;
    }
    int dir = 0;
    bool page = false;
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) dir = 1;
    else if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) dir = -1;
    else if (code == juce::KeyPress::pageUpKey) { dir = 1; page = true; }
    else if (code == juce::KeyPress::pageDownKey) { dir = -1; page = true; }
    if (dir == 0) return false;
    const int steps = v.steps ? v.steps() : 0;
    const float step = keyStep (steps, mods.isShiftDown(), page);
    float x = v.get() + (float) dir * step;
    if (steps > 1 && steps <= maxSteps) x = std::round (x * (float) (steps - 1)) / (float) (steps - 1); // on a position
    v.set (juce::jlimit (0.0f, 1.0f, x));
    return true;
}

bool handlePressKey (const juce::KeyPress& key, const PressAccess& p)
{
    if (p.enabled && ! p.enabled()) return false;
    const auto mods = key.getModifiers();
    if (key.getKeyCode() == juce::KeyPress::F10Key && mods.isShiftDown())
    {
        if (! p.secondary) return false;
        p.secondary();
        return true;
    }
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    if (key.getKeyCode() == juce::KeyPress::returnKey || key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (! p.press) return false;
        p.press();
        return true;
    }
    return false;
}

namespace
{
class ValueInterface final : public juce::AccessibilityValueInterface
{
public:
    explicit ValueInterface (const ValueAccess& a) : access (a) {}
    bool isReadOnly() const override { return access.enabled && ! access.enabled(); }
    double getCurrentValue() const override { return access.get ? (double) access.get() : 0.0; }
    juce::String getCurrentValueAsString() const override
    {
        if (access.text)
            if (auto t = access.text(); t.isNotEmpty()) return t;
        return juce::String (juce::roundToInt (getCurrentValue() * 100.0)) + " %";
    }
    void setValue (double v) override
    {
        if (access.set && ! isReadOnly()) access.set (juce::jlimit (0.0f, 1.0f, (float) v));
    }
    void setValueAsString (const juce::String& s) override
    {
        // a number in percent ("40", "40 %") - the plate's own texts (Hz, ms, names) are entered on the plate
        if (s.containsAnyOf ("0123456789")) setValue (s.retainCharacters ("0123456789.-").getDoubleValue() / 100.0);
    }
    AccessibleValueRange getRange() const override
    {
        const int steps = access.steps ? access.steps() : 0;
        return { { 0.0, 1.0 }, (double) keyStep (steps, false, false) };
    }

private:
    const ValueAccess& access;
};

class ValueHandler final : public juce::AccessibilityHandler
{
public:
    ValueHandler (juce::Component& c, ValueAccess a)
        : juce::AccessibilityHandler (c, juce::AccessibilityRole::slider, {}, Interfaces { std::make_unique<ValueInterface> (access) }),
          access (std::move (a))
    {
    }
    juce::String getTitle() const override { return access.title ? access.title() : getComponent().getTitle(); }
    juce::AccessibleState getCurrentState() const override
    {
        auto s = juce::AccessibilityHandler::getCurrentState();
        return s.withFocusable();
    }

private:
    ValueAccess access; // declared after the base: the interface keeps a reference, set before any call
};

juce::AccessibilityActions pressActions (const PressAccess& p)
{
    juce::AccessibilityActions a;
    if (p.press) a.addAction (juce::AccessibilityActionType::press, [f = p.press] { f(); });
    if (p.secondary) a.addAction (juce::AccessibilityActionType::showMenu, [f = p.secondary] { f(); });
    return a;
}

class PressHandler final : public juce::AccessibilityHandler
{
public:
    PressHandler (juce::Component& c, PressAccess a)
        : juce::AccessibilityHandler (c, juce::AccessibilityRole::button, pressActions (a)), access (std::move (a))
    {
    }
    juce::String getTitle() const override { return access.title ? access.title() : getComponent().getTitle(); }
    juce::String getDescription() const override
    {
        const auto v = access.value ? access.value() : juce::String();
        return v.isNotEmpty() ? v : getComponent().getDescription();
    }
    juce::AccessibleState getCurrentState() const override
    {
        auto s = juce::AccessibilityHandler::getCurrentState().withFocusable();
        const int c = access.checked ? access.checked() : -1;
        if (c >= 0) s = s.withCheckable();
        if (c > 0) s = s.withChecked();
        return s;
    }

private:
    PressAccess access;
};

class TextValue final : public juce::AccessibilityTextValueInterface
{
public:
    explicit TextValue (std::function<juce::String()> v) : value (std::move (v)) {}
    bool isReadOnly() const override { return true; }
    juce::String getCurrentValueAsString() const override { return value ? value() : juce::String(); }
    void setValueAsString (const juce::String&) override {}

private:
    std::function<juce::String()> value;
};

class TextHandler final : public juce::AccessibilityHandler
{
public:
    TextHandler (juce::Component& c, std::function<juce::String()> t, std::function<juce::String()> v)
        : juce::AccessibilityHandler (c, juce::AccessibilityRole::staticText, {}, Interfaces { std::make_unique<TextValue> (std::move (v)) }),
          title (std::move (t))
    {
    }
    juce::String getTitle() const override { return title ? title() : getComponent().getTitle(); }

private:
    std::function<juce::String()> title;
};
} // namespace

std::unique_ptr<juce::AccessibilityHandler> valueHandler (juce::Component& c, ValueAccess v)
{
    return std::make_unique<ValueHandler> (c, std::move (v));
}

std::unique_ptr<juce::AccessibilityHandler> pressHandler (juce::Component& c, PressAccess p)
{
    return std::make_unique<PressHandler> (c, std::move (p));
}

std::unique_ptr<juce::AccessibilityHandler> textHandler (juce::Component& c, std::function<juce::String()> title,
                                                         std::function<juce::String()> value)
{
    return std::make_unique<TextHandler> (c, std::move (title), std::move (value));
}

void makeFocusable (juce::Component& c)
{
    c.setWantsKeyboardFocus (true);
    c.setMouseClickGrabsKeyboardFocus (false);
}

juce::String paramTitle (OhpeProcessor& proc, const juce::String& paramId)
{
    if (paramId.isEmpty()) return {};
    const int idx = proc.core().params().indexOf (paramId.toStdString());
    if (idx < 0) return {};
    const auto& d = proc.core().params().descriptor (idx);
    return i18n::pick (d.nameEn, d.nameDe);
}

juce::String paramText (OhpeProcessor& proc, const juce::String& paramId)
{
    const int idx = proc.core().params().indexOf (paramId.toStdString());
    auto* p = proc.parameter (paramId);
    if (idx < 0 || p == nullptr) return {};
    return formatValue (proc.core().params().descriptor (idx), p->convertFrom0to1 (p->getValue()));
}

ValueAccess paramAccess (OhpeProcessor& proc, std::function<juce::String()> paramId, std::function<juce::String()> title)
{
    ValueAccess v;
    auto* pr = &proc;
    v.title = title ? std::move (title) : std::function<juce::String()> ([pr, paramId] { return paramTitle (*pr, paramId()); });
    v.get = [pr, paramId] {
        auto* p = pr->parameter (paramId());
        return p != nullptr ? p->getValue() : 0.0f;
    };
    v.set = [pr, paramId] (float x) {
        if (auto* p = pr->parameter (paramId()))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, x));
            p->endChangeGesture();
        }
    };
    v.text = [pr, paramId] { return paramText (*pr, paramId()); };
    v.steps = [pr, paramId] {
        auto* p = pr->parameter (paramId());
        if (p == nullptr) return 0;
        const int n = p->getNumSteps();
        return n > 1 && n <= maxSteps ? n : 0;
    };
    v.reset = [pr, paramId] {
        if (auto* p = pr->parameter (paramId()))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    };
    v.enabled = [pr, paramId] { return pr->parameter (paramId()) != nullptr; };
    return v;
}

juce::Colour ensureContrast (juce::Colour fg, juce::Colour bg, double ratio)
{
    auto ratioOf = [&] (juce::Colour c) { return settings::contrastRatio (c.getARGB() | 0xff000000u, bg.getARGB() | 0xff000000u); };
    if (ratioOf (fg) >= ratio) return fg;
    const bool towardsBlack = ratioOf (juce::Colours::black) >= ratioOf (juce::Colours::white);
    const auto end = towardsBlack ? juce::Colours::black : juce::Colours::white;
    for (int i = 1; i <= 20; ++i)
    {
        const auto c = fg.interpolatedWith (end, (float) i / 20.0f);
        if (ratioOf (c) >= ratio) return c.withAlpha (fg.getAlpha());
    }
    return end.withAlpha (fg.getAlpha());
}

// ---------------------------------------------------------------- the focus ring
FocusRing::FocusRing()
{
    setInterceptsMouseClicks (false, false);
    setWantsKeyboardFocus (false);
    setAccessible (false);
    setVisible (false); // shown only while it has a control to ring (layout checks see no extra component)
    juce::Desktop::getInstance().addFocusChangeListener (this);
}

void FocusRing::update()
{
    setVisible (target() != nullptr);
    repaint();
}

FocusRing::~FocusRing() { juce::Desktop::getInstance().removeFocusChangeListener (this); }

void FocusRing::setKeyboardNavigation (bool on)
{
    if (on != navigating)
    {
        navigating = on;
        update();
    }
}

juce::Component* FocusRing::target() const
{
    auto* parent = getParentComponent();
    if (forced != nullptr) return forced.getComponent();
    if (blocked && blocked()) return nullptr; // an overlay covers the plate
    auto* f = juce::Component::getCurrentlyFocusedComponent();
    if (! navigating || parent == nullptr || f == nullptr || f == parent || ! parent->isParentOf (f) || ! f->isShowing()) return nullptr;
    return f;
}

void FocusRing::paint (juce::Graphics& g)
{
    auto* t = target();
    if (t == nullptr) return;
    const float s = scale;
    const auto r = getLocalArea (t, t->getLocalBounds().toFloat()).expanded (3.0f * s);
    const float rad = radius::inset * s + 3.0f * s;
    g.setColour (colour::ink.withAlpha (0.9f)); // the dark line outside: >= 3:1 on the light housing
    g.drawRoundedRectangle (r.expanded (1.0f * s), rad + 1.0f * s, 1.4f * s);
    g.setColour (colour::sunYellow); // the edit light inside: >= 3:1 on the night housing and the display
    g.drawRoundedRectangle (r, rad, 2.0f * s);
}

void FocusRing::globalFocusChanged (juce::Component*) { update(); }
} // namespace ohpe::ui::a11y
