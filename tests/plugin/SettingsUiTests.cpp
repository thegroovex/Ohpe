// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p32: the settings menu - gear key and overlay rules, language / skin / tooltips / help overlay / animations / window
// size from the per-user settings file, contrast of both skins, the MIDI page (existing parameters), the MIDI learn
// list (edit, clear, clear all), the About line, the extra page hook. No new host parameters.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettingsStore.h"
#include "ui/Palette.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
/** An editor whose user settings live in a temporary folder (reset to the defaults afterwards). */
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
        e->userSettingsStore().set ({});
        e->applyUserSettings();
        i18n::setCurrent (i18n::Language::English);
        ui::applySkin (ui::Skin::Light);
    }
    ui::SettingsView& view() { return e->settingsComponent(); }
};

float hostPlain (OhpeProcessor& p, const char* id)
{
    auto* q = p.parameter (id);
    return q != nullptr ? q->convertFrom0to1 (q->getValue()) : -1.0f;
}

double contrast (juce::Colour a, juce::Colour b) { return settings::contrastRatio (a.getARGB(), b.getARGB()); }

void checkContrast (const char* skin)
{
    using namespace ui;
    INFO (skin);
    CHECK (contrast (colour::engraving, colour::body) >= 4.5);
    CHECK (contrast (colour::engraving, colour::panel) >= 4.5);
    CHECK (contrast (colour::ink, colour::body) >= 7.0);
    CHECK (contrast (colour::capInk, colour::capTop) >= 4.5);
    CHECK (contrast (colour::capInk, colour::capBottom) >= 4.5);
    CHECK (contrast (colour::displayText, colour::display) >= 7.0);
    CHECK (contrast (colour::displayText, colour::displayGrid) >= 4.5);
    CHECK (contrast (colour::displayText, colour::pill) >= 4.5);
    CHECK (contrast (colour::displayText, colour::keyDark) >= 4.5); // popup menus
    CHECK (contrast (colour::displayDim, colour::display) >= 3.0);
    CHECK (contrast (colour::displayDim, colour::displayGrid) >= 3.0);
    CHECK (contrast (colour::display, colour::sunYellow) >= 4.5); // text on a lit chip
    CHECK (contrast (colour::sunYellow, colour::display) >= 4.5);
    CHECK (contrast (colour::playGreenLit, colour::display) >= 4.5);
    // octave labels on the natural keys (top and bottom of the key's gradient, pressed or not)
    for (auto key : { colour::keyLight, colour::keyLightPressed })
    {
        CHECK (contrast (colour::keyInk, key) >= 4.5);
        CHECK (contrast (colour::keyInk, key.darker (0.04f)) >= 4.5);
    }
}
} // namespace

TEST_CASE ("settings: gear key opens the view, overlays exclude each other, Esc closes")
{
    Rig r;
    auto& e = *r.e;
    CHECK_FALSE (e.settingsVisible());
    REQUIRE (e.settingsKeyComponent().onClick);
    e.settingsKeyComponent().onClick();
    CHECK (e.settingsVisible());
    CHECK (e.overlayOpen());
    CHECK_FALSE (e.displayComponent().isVisible()); // the plate's controls are hidden under the overlay
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.showLibrary (true);
    CHECK_FALSE (e.settingsVisible());
    e.showSettings (true, ui::SettingsView::MidiLearn);
    CHECK_FALSE (e.libraryVisible());
    CHECK (r.view().page() == ui::SettingsView::MidiLearn);
    CHECK (e.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_FALSE (e.settingsVisible());
    CHECK_FALSE (e.overlayOpen());
    // the language pills left the main panel; the gear sits in the header
    CHECK (OhpeEditor::designArea ("settingsKey").getRight() == doctest::Approx (1160.0f));
    // every tab paints in both languages
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        e.showSettings (true);
        for (int t = 0; t < r.view().numTabs(); ++t)
        {
            r.view().showPage (t);
            CHECK (r.view().tabName (t).isNotEmpty());
            CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
        }
        e.showSettings (false);
    }
}

TEST_CASE ("settings: language is a user setting, stored in settings.json")
{
    Rig r;
    auto& e = *r.e;
    r.view().changeSettings ([] (settings::UserSettings& s) { s.language = settings::Language::German; });
    CHECK (i18n::current() == i18n::Language::German);
    CHECK (e.userSettingsStore().file().loadFileAsString().contains ("\"de\""));
    // another store reading the same file sees it (per user, not per project)
    UserSettingsStore other;
    other.setFolderForTests (e.userSettingsStore().file().getParentDirectory().getParentDirectory());
    CHECK (other.get().language == settings::Language::German);
    // a new session (processor state) does not change it
    juce::MemoryBlock block;
    r.p.getStateInformation (block);
    r.p.setStateInformation (block.getData(), (int) block.getSize());
    CHECK (i18n::current() == i18n::Language::German);
    r.view().changeSettings ([] (settings::UserSettings& s) { s.language = settings::Language::English; });
    CHECK (i18n::current() == i18n::Language::English);
    // chips: the language chips of the General page
    e.showSettings (true, ui::SettingsView::General);
    CHECK (r.view().clickChip ("DEUTSCH"));
    CHECK (i18n::current() == i18n::Language::German);
    CHECK (r.view().clickChip ("ENGLISH"));
    CHECK (i18n::current() == i18n::Language::English);
    CHECK (r.view().clickChip ("SYSTEM"));
    CHECK (e.userSettingsStore().get().language == settings::Language::System);
    i18n::setCurrent (i18n::Language::English);
}

TEST_CASE ("settings: dark skin covers the UI and keeps the contrast")
{
    Rig r;
    auto& e = *r.e;
    checkContrast ("light");
    const auto lightBody = ui::colour::body;
    r.view().changeSettings ([] (settings::UserSettings& s) { s.skin = settings::Skin::Dark; });
    CHECK (ui::currentSkin() == ui::Skin::Dark);
    CHECK (ui::colour::body != lightBody);
    CHECK (ui::colour::body.getPerceivedBrightness() < 0.25f);
    checkContrast ("dark");
    // low glare: no pure white text or highlight in the dark skin
    for (auto c : { ui::colour::displayText, ui::colour::ink, ui::colour::capInk, ui::colour::keyLight, ui::colour::sheen })
        CHECK (juce::jmax (c.getRed(), c.getGreen(), c.getBlue()) < 0xf0);
    // the accents are dimmer than in the light skin
    CHECK (ui::colour::sunYellow.getPerceivedBrightness() < juce::Colour (0xfff6c20f).getPerceivedBrightness());
    // every view renders in the dark skin
    auto img = e.createComponentSnapshot (e.getLocalBounds());
    CHECK (img.isValid());
    CHECK (img.getPixelAt (600, 30).getPerceivedBrightness() < 0.35f); // the housing is dark
    e.showProView (true);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.showProView (false);
    e.showLibrary (true);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.showLibrary (false);
    e.showSettings (true, ui::SettingsView::Display);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    CHECK (r.view().clickChip ("LIGHT"));
    CHECK (ui::currentSkin() == ui::Skin::Light);
    CHECK (e.userSettingsStore().get().skin == settings::Skin::Light);
    CHECK (r.view().clickChip ("AUTO"));
    CHECK (e.userSettingsStore().get().skin == settings::Skin::Auto);
    CHECK (ui::currentSkin() == (UserSettingsStore::systemDark() ? ui::Skin::Dark : ui::Skin::Light));
}

TEST_CASE ("settings: tooltips, help overlay, animations, brightness, window size")
{
    Rig r;
    auto& e = *r.e;
    CHECK (e.tooltipsEnabled());
    r.view().changeSettings ([] (settings::UserSettings& s) { s.tooltips = false; });
    CHECK_FALSE (e.tooltipsEnabled());
    r.view().changeSettings ([] (settings::UserSettings& s) { s.tooltips = true; });
    CHECK (e.tooltipsEnabled());

    CHECK_FALSE (e.helpOverlayComponent().isVisible());
    r.view().changeSettings ([] (settings::UserSettings& s) { s.helpOverlay = true; });
    CHECK (e.helpOverlayComponent().isVisible());
    CHECK (e.helpOverlayComponent().itemList().size() >= 8);
    {
        bool self = true, children = true;
        e.helpOverlayComponent().getInterceptsMouseClicks (self, children);
        CHECK_FALSE (self); // clicks go through to the controls
        CHECK_FALSE (children);
    }
    // every tag shows its name and line in full (no "SETTIN…" on the narrow gear key) and stays inside the window
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        const auto bounds = e.helpOverlayComponent().getLocalBounds().toFloat();
        const float s = bounds.getWidth() / 1200.0f;
        for (const auto& it : e.helpOverlayComponent().itemList())
        {
            INFO (it.title.toStdString());
            const auto tag = ui::HelpOverlay::tagBounds (it, bounds);
            CHECK (tag.getWidth() >= ui::HelpOverlay::tagWidthNeeded (it, s) - 0.01f);
            CHECK (bounds.contains (tag));
        }
    }
    e.setLanguage (i18n::Language::English);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.showSettings (true);
    CHECK_FALSE (e.helpOverlayComponent().isVisible()); // not over an overlay
    e.showSettings (false);
    CHECK (e.helpOverlayComponent().isVisible());
    CHECK (e.keyPressed (juce::KeyPress (juce::KeyPress::F1Key)));
    CHECK_FALSE (e.helpOverlayComponent().isVisible());
    CHECK_FALSE (e.userSettingsStore().get().helpOverlay);

    CHECK (e.animationRate() == doctest::Approx (1.0f));
    r.view().changeSettings ([] (settings::UserSettings& s) { s.animations = settings::Animations::Off; });
    CHECK (e.animationRate() == 0.0f);
    r.view().changeSettings ([] (settings::UserSettings& s) { s.animations = settings::Animations::Reduced; });
    CHECK (e.animationRate() > 0.0f);
    CHECK (e.animationRate() < 1.0f);

    r.view().changeSettings ([] (settings::UserSettings& s) { s.brightness = 0.55f; });
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());

    r.view().changeSettings ([] (settings::UserSettings& s) { s.windowScale = 1.25f; });
    CHECK (e.getWidth() == 1500);
    r.view().changeSettings ([] (settings::UserSettings& s) { s.windowScale = 1.0f; });
    CHECK (e.getWidth() == 1200);
}

TEST_CASE ("settings: MIDI page edits existing parameters, MIDI learn list edits and clears")
{
    Rig r;
    auto& e = *r.e;
    const int numParams = r.p.core().params().size();
    e.showSettings (true, ui::SettingsView::Midi);
    CHECK (r.view().clickChip ("MIDI CLOCK"));
    CHECK (hostPlain (r.p, Core::Ids::syncSource) == doctest::Approx (1.0f));
    CHECK (r.view().clickChip ("INTERNAL"));
    CHECK (hostPlain (r.p, Core::Ids::syncSource) == doctest::Approx (0.0f));
    CHECK (r.p.core().params().size() == numParams); // nothing new: the page shows existing parameters

    MidiMapping a;
    a.paramId = "tempo";
    a.number = 20;
    MidiMapping b;
    b.paramId = "p1.vol";
    b.number = 21;
    b.channel = 2;
    r.p.core().midiMap().add (a);
    r.p.core().midiMap().add (b);
    r.p.core().midiMapChanged();
    e.showSettings (true, ui::SettingsView::MidiLearn);
    auto rows = r.view().learnRows();
    REQUIRE (rows.size() == 2);
    CHECK (rows[0].contains ("CC 20"));
    CHECK (rows[1].contains ("Ch 2"));
    auto edited = r.p.midiMappings()[0];
    edited.mode = MidiMapMode::Toggle;
    edited.channel = 5;
    CHECK (r.p.setMidiMapping (0, edited));
    rows = r.view().learnRows();
    CHECK (rows[0].contains ("Toggle"));
    CHECK (rows[0].contains ("Ch 5"));
    CHECK (r.view().clickChip ("CLEAR")); // the last row's CLEAR
    CHECK (r.p.midiMappings().size() == 1);
    CHECK (r.p.core().midiMap().isMapped ("tempo"));
    CHECK (r.view().clickChip ("CLEAR ALL"));
    CHECK (r.p.midiMappings().empty());
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
}

TEST_CASE ("settings: About page, the line appears only there and in the help pages")
{
    Rig r;
    auto& e = *r.e;
    CHECK (ui::SettingsView::aboutLine() == juce::String::fromUTF8 ("Beat tactics \xe2\x80\x94 designed by groovex"));
    REQUIRE (e.versionBadgeComponent().onClick);
    e.versionBadgeComponent().onClick(); // the wordmark / version legend: About (no update offered)
    CHECK (e.settingsVisible());
    CHECK (r.view().page() == ui::SettingsView::About);
    CHECK (r.view().chipTexts().contains ("CHECK NOW"));
    e.versionBadgeComponent().onClick();
    CHECK_FALSE (e.settingsVisible());
    // nowhere else: no UI text carries it
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        i18n::setCurrent (lang);
        for (const auto& k : i18n::allKeys())
        {
            CHECK_FALSE (k.containsIgnoreCase ("beat tactics"));
            CHECK_FALSE (i18n::tr (k).containsIgnoreCase ("beat tactics"));
        }
        CHECK (ui::SettingsView::numHelpTopics() >= 6);
        for (int t = 0; t < ui::SettingsView::numHelpTopics(); ++t)
        {
            CHECK (ui::SettingsView::helpTitle (t).isNotEmpty());
            CHECK (ui::SettingsView::helpText (t).length() > 80);
        }
    }
    i18n::setCurrent (i18n::Language::German);
    CHECK (ui::SettingsView::helpTitle (0) == juce::String::fromUTF8 ("Erste Schritte"));
    CHECK (i18n::tr ("Settings") == "Einstellungen");
    CHECK (i18n::tr ("Skin") == "Erscheinung");
    i18n::setCurrent (i18n::Language::English);
}

TEST_CASE ("settings: extra page hook")
{
    Rig r;
    auto& e = *r.e;
    juce::Component page;
    const int before = r.view().numTabs();
    r.view().addExtraPage ("Expression", "Ausdruck", &page);
    CHECK (r.view().numTabs() == before + 1);
    e.showSettings (true, before);
    CHECK (r.view().page() == before);
    CHECK (page.isVisible());
    CHECK (r.view().tabName (before) == "Expression");
    r.view().showPage (ui::SettingsView::General);
    CHECK_FALSE (page.isVisible());
    e.showSettings (false);
    r.view().removeChildComponent (&page);
}
