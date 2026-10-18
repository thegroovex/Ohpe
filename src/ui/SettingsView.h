// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/Updater.h"
#include "plugin/UserSettingsStore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ohpe::ui
{
class GridController;

/**
    p32: the settings view - an overlay on the plate like the library and the update view, opened with the gear key in
    the header (where the language pills were), the standalone's window menu, or a click on the wordmark (About page).

    Tabs (left): General (language, tooltips, help overlay), Display (skin light / dark / auto, display brightness,
    animations, animated 3D visuals, window size), MIDI (inputs of the standalone app, channel / MPE / bend range of
    the four parts, clock and sync), MIDI learn (every mapping: edit channel / mode / range, learn again, clear, clear
    all, import / export map files), Updates (the updater's settings and status), Help (help pages EN / DE), About.

    The user settings (language, help, skin, display) are per user (UserSettingsStore, settings.json) and never host
    parameters. The MIDI page shows existing parameters (pN.midich, pN.mpe, pN.bend, pN.mpe.bend, sync.*) - changing
    them here is the same as anywhere else (host notified, automatable and MIDI-mappable as before).

    Hook: addExtraPage() adds a tab whose content is a component of another feature (shown in the content area).
*/
class SettingsView : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    enum Page { General = 0, Display, Midi, MidiLearn, Updates, Help, About, numPages };

    SettingsView (OhpeProcessor& p, Updater& u, UserSettingsStore& s);
    ~SettingsView() override;

    void open (int page = -1); ///< -1: the page shown last
    void close();
    void showPage (int page);
    int page() const noexcept { return current; }
    int numTabs() const noexcept { return numPages + (int) extras.size(); }
    juce::String tabName (int tab) const;

    /** Another feature's page (a tab after About; `content` is owned by the caller and shown in the content area). */
    void addExtraPage (const juce::String& nameEn, const juce::String& nameDe, juce::Component* content);

    std::function<void()> onClosed;
    std::function<void()> onSettingsChanged; ///< a user setting changed (the store is already saved)
    std::function<void()> onOpenUpdates;     ///< the update view (Updates / About page)
    std::function<void()> onInstallPack;     ///< content packs: "Install pack ..." (Updates page)
    std::function<void()> onManagePacks;     ///< content packs: the pack manager page
    std::function<bool()> getAnimatedVisuals;
    std::function<void (bool)> setAnimatedVisuals;
    void setStandalone (bool s) { standalone = s; }
    /** p46: the Grid-Controller (Profil 1) the MIDI page sets up and previews (standalone; nullptr = none). */
    void setGridController (GridController* c) { gridCtl = c; }
    /** p46: the virtual controller view (display lines, pads, message log) instead of the parts table. */
    void showControllerPreview (bool on) { controllerPreview = on; repaint(); }
    bool controllerPreviewShown() const noexcept { return controllerPreview; }

    // help pages (EN / DE)
    static int numHelpTopics();
    static juce::String helpTitle (int topic);
    static juce::String helpText (int topic);
    void setHelpTopic (int t);
    int helpTopic() const noexcept { return topic; }
    /** The line on the About page and the help pages (and nowhere else). */
    static juce::String aboutLine();
    /** p43: the app icon (128 px image from resources/icon/png). */
    static juce::Image appIcon();

    /** Changes a user setting as the view's chips do (tests). */
    void changeSettings (const std::function<void (settings::UserSettings&)>& fn);
    /** Clicks the chip whose text is `text` (upper case as drawn) on the shown page; false if there is none (tests). */
    bool clickChip (const juce::String& text);
    juce::StringArray chipTexts();
    /** MIDI learn list: the mapping row texts as shown (tests). */
    juce::StringArray learnRows() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& k) override;

private:
    struct Hit
    {
        juce::Rectangle<float> area;
        juce::String text;
        std::function<void()> action;
    };
    struct Extra
    {
        juce::String nameEn, nameDe;
        juce::Component* content = nullptr;
    };
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    /** p50: the MIDI page shows what a fingerboard controller sends (HD data, smoothing): refreshed twice a second. */
    void timerCallback() override
    {
        if (current == Midi && isShowing()) repaint();
    }
    void paintFingerboard (juce::Graphics& g, juce::Rectangle<float> a, float xParts); ///< p50
    float unit() const;
    juce::Rectangle<float> contentArea() const;
    void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent, std::function<void()> action,
               bool enabled = true);
    void legend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r);
    void label (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, bool dim = false);
    void stepper (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& value, std::function<void()> down, std::function<void()> up);

    void paintGeneral (juce::Graphics& g, juce::Rectangle<float> a);
    void paintDisplay (juce::Graphics& g, juce::Rectangle<float> a);
    void paintMidi (juce::Graphics& g, juce::Rectangle<float> a);
    void paintController (juce::Graphics& g, juce::Rectangle<float> a);        ///< p46: the controller section
    void paintControllerPreview (juce::Graphics& g, juce::Rectangle<float> a); ///< p46: the virtual controller
    void paintLearn (juce::Graphics& g, juce::Rectangle<float> a);
    void paintUpdates (juce::Graphics& g, juce::Rectangle<float> a);
    void paintHelp (juce::Graphics& g, juce::Rectangle<float> a);
    void paintAbout (juce::Graphics& g, juce::Rectangle<float> a);
    void paintOffscreen(); ///< builds the hit list of the shown page (tests)
    void paintText (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& text, float& scrollPos, float& maxScrollPos);

    float plain (const juce::String& id) const;
    void setPlain (const juce::String& id, float v);
    void step (const juce::String& id, int delta);
    void editMapping (int index, juce::Rectangle<float> near);
    void mapFile (bool save);
    juce::String mappingRow (const MidiMapping& m) const;

    OhpeProcessor& proc;
    Updater& updater;
    UserSettingsStore& store;
    std::vector<Hit> hits;
    std::vector<Extra> extras;
    int current = General, topic = 0;
    float helpScroll = 0.0f, helpMax = 0.0f, learnScroll = 0.0f, learnMax = 0.0f;
    bool standalone = false;
    GridController* gridCtl = nullptr; ///< p46
    bool controllerPreview = false;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsView)
};

/**
    p32: the help overlay - names and one-line explanations over the plate's areas (Settings > General > Help overlay,
    or F1). It lets every click through to the controls below.
*/
class HelpOverlay : public juce::Component
{
public:
    struct Item
    {
        juce::Rectangle<float> design; ///< design units (1200 x 800)
        juce::String title, text;
    };
    HelpOverlay() { setInterceptsMouseClicks (false, false); }
    void setItems (std::vector<Item> i) { items = std::move (i); repaint(); }
    const std::vector<Item>& itemList() const noexcept { return items; }
    void paint (juce::Graphics& g) override;

    /** The width a tag needs so its title (upper case) and text show in full, in pixels at scale s. */
    static float tagWidthNeeded (const Item& it, float s);
    /** Where an item's tag goes (pixels, in an overlay of the given size): inside the area's top left when it fits there,
        otherwise outside, below (or above) the area, right-aligned to it and kept inside the overlay - never cut off. */
    static juce::Rectangle<float> tagBounds (const Item& it, juce::Rectangle<float> overlay);

private:
    std::vector<Item> items;
};
} // namespace ohpe::ui
