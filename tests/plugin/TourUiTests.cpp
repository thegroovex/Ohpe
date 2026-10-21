// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p64: the first-start tour - shown once per kind of editor (settings.json "tour"), skippable, re-openable from the
// settings, four steps in English and German whose cards never cover what they point at and stay on the plate.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettingsStore.h"
#include "ui/FirstStartTour.h"
#include "ui/Palette.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
struct TourSwitch
{
    explicit TourSwitch (bool on) : before (OhpeEditor::showTourOnFirstOpen.load()) { OhpeEditor::showTourOnFirstOpen.store (on); }
    ~TourSwitch() { OhpeEditor::showTourOnFirstOpen.store (before); }
    bool before;
};

struct Rig
{
    juce::TemporaryFile tmp;
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;

    Rig()
    {
        tmp.getFile().createDirectory();
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        e->userSettingsStore().setFolderForTests (tmp.getFile());
        e->userSettingsStore().set ({});
        e->applyUserSettings();
        e->setLanguage (i18n::Language::English);
    }
    ~Rig()
    {
        e->showTour (false);
        e->userSettingsStore().set ({});
        e->applyUserSettings();
        i18n::setCurrent (i18n::Language::English);
        ui::applySkin (ui::Skin::Light);
    }
};
} // namespace

TEST_CASE ("p64 tour: shown once on the first open, DONE marks it seen (plug-in), not again")
{
    TourSwitch on (true);
    Rig r;
    auto& e = *r.e;
    auto& t = e.tourComponent();
    e.showTourIfFirstOpen();
    REQUIRE (e.tourVisible());
    CHECK (t.step() == 0);
    CHECK_FALSE (t.hasKey (ui::FirstStartTour::BackKey));
    for (int s = 1; s < ui::FirstStartTour::numSteps; ++s)
    {
        t.press (ui::FirstStartTour::NextKey);
        CHECK (t.step() == s);
        CHECK (e.tourVisible());
    }
    t.press (ui::FirstStartTour::BackKey);
    CHECK (t.step() == ui::FirstStartTour::numSteps - 2);
    t.press (ui::FirstStartTour::NextKey);
    CHECK (t.keyText (ui::FirstStartTour::NextKey) == "DONE");
    t.press (ui::FirstStartTour::NextKey);
    CHECK_FALSE (e.tourVisible());
    CHECK (e.userSettingsStore().get().tourSeenPlugin);       // this editor is a plug-in (no standalone wrapper)
    CHECK_FALSE (e.userSettingsStore().get().tourSeenStandalone);
    // the file has it
    const auto text = e.userSettingsStore().file().loadFileAsString();
    CHECK (text.contains ("\"tour\""));
    e.showTourIfFirstOpen();
    CHECK_FALSE (e.tourVisible());
}

TEST_CASE ("p64 tour: SKIP and Esc close it and mark it seen; switched off (tests, screenshots) it never shows by itself")
{
    {
        TourSwitch off (false);
        Rig r;
        r.e->showTourIfFirstOpen();
        CHECK_FALSE (r.e->tourVisible());
    }
    TourSwitch on (true);
    Rig r;
    auto& e = *r.e;
    e.showTourIfFirstOpen();
    REQUIRE (e.tourVisible());
    e.tourComponent().press (ui::FirstStartTour::SkipKey);
    CHECK_FALSE (e.tourVisible());
    CHECK (e.userSettingsStore().get().tourSeenPlugin);
    // re-opened from the settings: the settings close, the tour starts at step 1; Esc ends it
    e.showSettings (true, ui::SettingsView::General);
    REQUIRE (e.settingsComponent().onShowTour != nullptr);
    CHECK (e.settingsComponent().clickChip ("SHOW TOUR"));
    CHECK (e.tourVisible());
    CHECK_FALSE (e.settingsVisible());
    CHECK (e.tourComponent().step() == 0);
    CHECK (e.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (e.tourComponent().step() == 1);
    CHECK (e.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_FALSE (e.tourVisible());
    // and from the help pages
    e.showSettings (true, ui::SettingsView::Help);
    CHECK (e.settingsComponent().clickChip ("SHOW TOUR"));
    CHECK (e.tourVisible());
}

TEST_CASE ("p64 tour: texts in English and German, cards on the plate and clear of the highlighted areas")
{
    TourSwitch off (false);
    Rig r;
    auto& e = *r.e;
    const juce::Rectangle<float> plate { 8, 8, 1184, 784 };
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        for (int s = 0; s < ui::FirstStartTour::numSteps; ++s)
        {
            CAPTURE (s);
            const auto title = ui::FirstStartTour::title (s), text = ui::FirstStartTour::text (s);
            CHECK (title.isNotEmpty());
            CHECK (text.length() > 40);
            if (lang == i18n::Language::German)
            {
                i18n::setCurrent (i18n::Language::English);
                CHECK (ui::FirstStartTour::text (s) != text); // translated
                i18n::setCurrent (i18n::Language::German);
            }
            const auto card = ui::FirstStartTour::cardDesign (s);
            CHECK (plate.contains (card));
            const auto marks = ui::FirstStartTour::highlights (s);
            CHECK_FALSE (marks.empty());
            for (const auto& m : marks)
            {
                CHECK (plate.contains (m));
                CHECK_FALSE (card.intersects (m.expanded (4.0f)));
            }
        }
    }
    e.setLanguage (i18n::Language::English);
    // the keys lie inside the card at the editor's size; the plate waits while the tour is open
    e.showTour (true, 1);
    auto& t = e.tourComponent();
    for (int k = 0; k < ui::FirstStartTour::numKeys; ++k)
        CHECK (t.cardArea().contains (t.keyArea (k)));
    CHECK (t.keyArea (ui::FirstStartTour::SkipKey).getRight() < t.keyArea (ui::FirstStartTour::BackKey).getX());
    CHECK (t.getBounds() == e.getLocalBounds());
    // painting in both skins does not throw
    for (auto skin : { ui::Skin::Light, ui::Skin::Dark })
    {
        ui::applySkin (skin);
        auto img = e.createComponentSnapshot (e.getLocalBounds(), true, 0.5f);
        CHECK (img.isValid());
    }
    ui::applySkin (ui::Skin::Light);
}
