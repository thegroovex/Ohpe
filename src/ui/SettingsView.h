// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/Updater.h"
#include "plugin/UserSettingsStore.h"
#include "Tooltips.h"

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
class SettingsView : public juce::Component,
                     public juce::TooltipClient,
                     public tips::RegionProvider,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    enum Page { General = 0, Display, Midi, MidiLearn, Controller, Updates, Help, About, numPages }; ///< p53: + Controller

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
    std::function<void()> onShowTour;        ///< p64: the first-start tour (General and Help pages)
    std::function<bool()> getAnimatedVisuals;
    std::function<void (bool)> setAnimatedVisuals;
    void setStandalone (bool s) { standalone = s; }
    /** p46: the Grid-Controller (Profil 1) the MIDI page sets up and previews (standalone; nullptr = none). */
    void setGridController (GridController* c) { gridCtl = c; }
    /** p46: the virtual controller view (display lines, pads, message log). p53: always on the Controller page; this
        opens that page. */
    void showControllerPreview (bool on) { if (on) showPage (Controller); repaint(); }
    bool controllerPreviewShown() const noexcept { return current == Controller; }
    /** p53: picks a controller port as the port menus do (switches automatic off); empty = none (tests). */
    void chooseControllerPort (bool input, const juce::String& name);
    /** p53: where the virtual pad grid is drawn (tests: mouse presses). */
    juce::Rectangle<float> virtualPadBounds() const noexcept { return padGrid; }

    // help pages (EN / DE)
    static int numHelpTopics();
    static juce::String helpTitle (int topic);
    static juce::String helpText (int topic);
    void setHelpTopic (int t);
    int helpTopic() const noexcept { return topic; }
    // p82: the help search (a field above the topics of the Help page; Ctrl / Cmd + F)
    /** The help topics as search entries (titles and texts EN / DE, target = topic index). */
    static std::vector<tips::Entry> helpSearchEntries();
    std::function<std::vector<tips::Entry>()> getSearchIndex;     ///< everything the search finds (the editor)
    std::function<bool (const tips::Entry&)> onSearchResult;      ///< jumps to a result (the editor)
    void setSearchText (const juce::String& text);
    juce::String searchText() const { return searchBox.getText(); }
    const std::vector<tips::Result>& searchResults() const noexcept { return results; }
    /** Opens result `index` as a click on it does (a topic shows here, the rest via onSearchResult). */
    bool clickSearchResult (int index);
    void focusSearch();
    juce::TextEditor& searchField() noexcept { return searchBox; }

    juce::String getTooltip() override;               ///< p82: the painted control under the mouse
    std::vector<tips::Region> tipRegions() override;  ///< p82: every chip, tab and field of the shown page

    /** The line on the About page and the help pages (and nowhere else). */
    static juce::String aboutLine();
    /** p43: the app icon (128 px image from resources/icon/png). */
    static juce::Image appIcon();

    /** Changes a user setting as the view's chips do (tests). */
    void changeSettings (const std::function<void (settings::UserSettings&)>& fn);
    /** Clicks the chip whose text is `text` (upper case as drawn) on the shown page; false if there is none (tests). */
    bool clickChip (const juce::String& text);
    juce::StringArray chipTexts();
    /** p53: every clickable area of the shown page (tests: nothing is cut off by the panel edge). */
    std::vector<juce::Rectangle<float>> clickableAreas();
    /** MIDI learn list: the mapping row texts as shown (tests). */
    juce::StringArray learnRows() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override; ///< p53: a virtual pad let go
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& k) override;

private:
    struct Hit
    {
        juce::Rectangle<float> area;
        juce::String text;
        std::function<void()> action;
        juce::String tip; ///< p82
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
        // p53: the Controller page follows the device (pads, display) ten times a second; the MIDI page twice
        if (! isShowing()) return;
        if (current == Controller || (current == Midi && ++slowTicks % 5 == 0)) repaint();
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
    void paintController (juce::Graphics& g, juce::Rectangle<float> a);        ///< p46: the controller section (p53: its page)
    void portMenu (bool input, juce::Rectangle<float> near);                   ///< p53: the port pop-up menu
    void dropdown (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, std::function<void()> action, bool enabled);
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
    juce::Rectangle<float> padGrid, lcdArea; ///< p53: the virtual pads (8 x 8 cells), the virtual display
    int pressedPad = -1, slowTicks = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    // p82: tooltips of the painted chips (rowTip sets the one of the next chips) and the help search
    juce::String tipNow;
    void rowTip (const char* en, const char* de) { tipNow = tips::t (en, de); }
    juce::TextEditor searchBox;
    std::vector<tips::Entry> searchIndex;
    std::vector<tips::Result> results;
    void runSearch();
    juce::Rectangle<float> searchBoxArea() const;
    void paintSearchResults (juce::Graphics& g, juce::Rectangle<float> list, juce::Rectangle<float> text);

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
