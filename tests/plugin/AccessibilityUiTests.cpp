// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p83: accessibility and UI scaling. Every control Tab reaches has a screen reader title (EN / DE) and the right
// role; value controls step with the arrow / Page keys, keys press with Enter / Space; Tab follows the plate's areas
// and the keyboard keeps the focus for notes by default. The UI sizes 75 .. 200 % are per computer (user settings),
// the window keeps its aspect ratio, and the plate renders the same at every size and pixel density (vector).

#include <doctest/doctest.h>

#include "settings/UserSettings.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettingsStore.h"
#include "ui/Accessibility.h"
#include "ui/PluginEditor.h"

#include <typeinfo>

using namespace ohpe;

namespace
{
struct Rig
{
    juce::TemporaryFile tmp;
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;

    Rig()
    {
        tmp.getFile().createDirectory();
        p.prepareToPlay (48000.0, 256);
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        e->userSettingsStore().setFolderForTests (tmp.getFile()); // the real settings are never touched
        e->userSettingsStore().set ({});
        e->applyUserSettings();
        e->setLanguage (i18n::Language::English);
    }
    ~Rig()
    {
        e->setLanguage (i18n::Language::English);
        ed.reset();
        tmp.getFile().deleteRecursively();
    }
};

juce::String titleOf (juce::Component& c)
{
    auto h = c.createAccessibilityHandler();
    return h != nullptr ? h->getTitle() : juce::String();
}

juce::Component* findByTitle (OhpeEditor& e, const juce::String& title)
{
    for (auto* c : e.focusOrder())
        if (titleOf (*c) == title) return c;
    return nullptr;
}

double meanDifference (const juce::Image& a, const juce::Image& b)
{
    REQUIRE (a.getWidth() == b.getWidth());
    REQUIRE (a.getHeight() == b.getHeight());
    const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly), db (b, juce::Image::BitmapData::readOnly);
    double sum = 0.0;
    for (int y = 0; y < a.getHeight(); y += 2)
        for (int x = 0; x < a.getWidth(); x += 2)
        {
            const auto ca = da.getPixelColour (x, y), cb = db.getPixelColour (x, y);
            sum += std::abs ((int) ca.getRed() - (int) cb.getRed()) + std::abs ((int) ca.getGreen() - (int) cb.getGreen())
                   + std::abs ((int) ca.getBlue() - (int) cb.getBlue());
        }
    return sum / (3.0 * (double) ((a.getWidth() + 1) / 2) * (double) ((a.getHeight() + 1) / 2));
}
} // namespace

TEST_CASE ("p83 accessibility: every control Tab reaches has a title and a role in English and German")
{
    Rig r;
    auto& e = *r.e;
    const auto areas = e.focusAreas();
    CHECK (areas.size() >= 6); // header, engines, display, encoders, modes, transport, keyboard row
    const auto order = e.focusOrder();
    CHECK (order.size() >= 40);
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        juce::StringArray titles;
        for (auto* c : order)
        {
            INFO ((lang == i18n::Language::German ? "DE " : "EN ") << typeid (*c).name() << " " << c->getName().toStdString());
            auto h = c->createAccessibilityHandler();
            REQUIRE (h != nullptr);
            CHECK (h->getTitle().isNotEmpty());
            CHECK (h->getRole() != juce::AccessibilityRole::ignored);
            CHECK (c->getWantsKeyboardFocus());
            const bool clickGrabs = c->getMouseClickGrabsKeyboardFocus() && c != &e.keyboardComponent() && c != &e.gridKeyboard();
            CHECK_FALSE (clickGrabs);
            if (auto* v = h->getValueInterface()) CHECK (v->getCurrentValueAsString().isNotEmpty());
            titles.add (h->getTitle());
        }
        INFO (titles.joinIntoString (" | ").toStdString());
        const bool de = lang == i18n::Language::German;
        for (const char* en : { "Octave down", "Octave up", "Tempo", "Life", "Settings", "Part", "Preset", "Keyboard", "Display", "Play" })
            CHECK (titles.contains (i18n::tr (en)));
        if (de)
        {
            CHECK (titles.contains (juce::String::fromUTF8 ("Oktave hoch")));
            CHECK (titles.contains ("Tastatur"));
            CHECK (titles.contains ("Einstellungen"));
        }
        // an encoder speaks the bound parameter's full name in the current language
        auto& enc = e.encoder (2);
        const int idx = r.p.core().params().indexOf (enc.boundId().toStdString());
        REQUIRE (idx >= 0);
        const auto& d = r.p.core().params().descriptor (idx);
        CHECK (titleOf (enc) == i18n::pick (d.nameEn, d.nameDe));
    }
    e.setLanguage (i18n::Language::English);
    // read-only elements carry titles too
    CHECK (titleOf (e.displayComponent().headerKey()) == "Key");
    CHECK (e.displayComponent().createAccessibilityHandler()->getValueInterface()->getCurrentValueAsString().isNotEmpty());
}

TEST_CASE ("p83 keyboard: arrows / Page / Delete change values, Enter / Space press keys, Tab order, notes keep their keys")
{
    Rig r;
    auto& e = *r.e;
    auto& enc = e.encoder (2);
    REQUIRE (enc.boundId().isNotEmpty());
    auto* param = r.p.parameter (enc.boundId());
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (0.5f);
    const float v0 = enc.normalisedValue();
    CHECK (enc.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    const float v1 = enc.normalisedValue();
    CHECK (v1 > v0);
    CHECK (enc.keyPressed (juce::KeyPress (juce::KeyPress::pageUpKey)));
    const float v2 = enc.normalisedValue();
    CHECK (v2 - v1 > (v1 - v0) * 3.0f); // Page moves further than an arrow
    CHECK (enc.keyPressed (juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK (enc.normalisedValue() < v2);
    CHECK (enc.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (enc.normalisedValue() == doctest::Approx (param->getDefaultValue()).epsilon (0.002));
    CHECK_FALSE (enc.keyPressed (juce::KeyPress ('a'))); // letters go on to the editor (shortcuts)
    // a stepped value moves one position per arrow
    CHECK (ui::a11y::keyStep (4, false, false) == doctest::Approx (1.0f / 3.0f));
    CHECK (ui::a11y::keyStep (0, false, true) == doctest::Approx (0.1f));
    CHECK (ui::a11y::keyStep (0, true, false) == doctest::Approx (0.001f));

    // Space on the focused SETTINGS key opens the settings; while an overlay is open, Tab stays out of the plate
    auto* settingsKey = findByTitle (e, "Settings");
    REQUIRE (settingsKey != nullptr);
    CHECK (settingsKey->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (e.settingsVisible());
    CHECK (e.focusAreas().empty());
    e.showSettings (false);
    CHECK_FALSE (e.focusAreas().empty());
    // Enter on PRO opens the pro view
    auto* pro = findByTitle (e, "Pro");
    REQUIRE (pro != nullptr);
    CHECK (pro->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    CHECK (e.focusAreas().empty()); // the pro view is an overlay
    e.showProView (false);

    // the part selector: arrows and 1 .. 4
    auto* part = findByTitle (e, "Part");
    REQUIRE (part != nullptr);
    e.selectPart (0);
    CHECK (part->keyPressed (juce::KeyPress ('3')));
    CHECK (e.currentPart() == 2);
    CHECK (part->keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
    CHECK (e.currentPart() == 1);
    e.selectPart (0);

    // the traverser: the keyboard by default (notes), Tab walks the order and wraps
    auto t = e.createKeyboardFocusTraverser();
    CHECK (t->getDefaultComponent (&e) == &e.keyboardComponent());
    const auto order = e.focusOrder();
    REQUIRE (order.size() > 2);
    CHECK (t->getNextComponent (order[0]) == order[1]);
    CHECK (t->getPreviousComponent (order[0]) == order.back());
    CHECK (t->getNextComponent (order.back()) == order.front());
    // every area starts where F6 lands, and the keyboard is in the last one
    const auto areas = e.focusAreas();
    CHECK (std::find (areas.back().begin(), areas.back().end(), &e.keyboardComponent()) != areas.back().end());

    // the navigation keys are no note keys of the computer keyboard (they never play notes)
    auto& kb = e.keyboardComponent();
    for (int code : { juce::KeyPress::tabKey, juce::KeyPress::F6Key, juce::KeyPress::upKey, juce::KeyPress::downKey, juce::KeyPress::leftKey,
                      juce::KeyPress::rightKey, juce::KeyPress::pageUpKey, juce::KeyPress::pageDownKey, juce::KeyPress::returnKey,
                      juce::KeyPress::spaceKey, juce::KeyPress::escapeKey, juce::KeyPress::deleteKey })
        CHECK_FALSE (kb.keyPressed (juce::KeyPress (code)));
    CHECK (kb.keyPressed (juce::KeyPress ('a'))); // still plays notes while it has the focus
}

TEST_CASE ("p83 UI size: 75 .. 200 % per computer, fixed aspect ratio, the same picture at every size and pixel density")
{
    Rig r;
    auto& e = *r.e;
    CHECK (std::size (settings::windowScales) == 6);
    CHECK (settings::windowScales[0] == doctest::Approx (0.75f));
    CHECK (settings::windowScales[5] == doctest::Approx (2.0f));
    auto* c = e.getConstrainer();
    REQUIRE (c != nullptr);
    CHECK (c->getFixedAspectRatio() == doctest::Approx (1.5));
    CHECK (c->getMinimumWidth() <= 900);
    CHECK (c->getMaximumWidth() >= 2400);
    CHECK (e.isResizable());

    for (float s : settings::windowScales)
    {
        INFO (s);
        e.setUiSize (s);
        CHECK (e.getWidth() == juce::roundToInt (1200.0f * s));
        CHECK (e.getHeight() == juce::roundToInt (800.0f * s));
        CHECK (e.userSettingsStore().get().windowScale == doctest::Approx (s)); // stored per computer
        e.advanceAnimation (0.02f);
        const auto img = e.createComponentSnapshot (e.getLocalBounds());
        CHECK (img.getWidth() == e.getWidth());
        // the display scales with the plate
        CHECK ((float) e.displayComponent().getWidth() / (float) e.getWidth() > 0.4f);
    }
    // HiDPI: 100 % on a 2x screen paints the same picture as 200 % on a 1x screen (vector drawing, background at the
    // physical pixel scale)
    e.setUiSize (1.0f);
    e.advanceAnimation (0.02f);
    const auto hidpi = e.createComponentSnapshot (e.getLocalBounds(), true, 2.0f);
    e.setUiSize (2.0f);
    e.advanceAnimation (0.0f);
    const auto big = e.createComponentSnapshot (e.getLocalBounds(), true, 1.0f);
    CHECK (hidpi.getWidth() == 2400);
    const double diff = meanDifference (hidpi, big);
    INFO ("mean difference " << diff);
    CHECK (diff < 8.0);
    // and 75 % is the same plate, only smaller (compared at 75 %)
    e.setUiSize (0.75f);
    e.advanceAnimation (0.0f);
    const auto small = e.createComponentSnapshot (e.getLocalBounds());
    const auto bigDown = big.rescaled (small.getWidth(), small.getHeight(), juce::Graphics::highResamplingQuality);
    const double diffSmall = meanDifference (small, bigDown);
    INFO ("mean difference 75 % " << diffSmall);
    CHECK (diffSmall < 14.0);
    e.setUiSize (1.0f);
}
