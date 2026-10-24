// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LibraryBrowser.h"

#include "SpaceView.h"

#include "Palette.h"
#include "plugin/Localisation.h"

#include <algorithm>

namespace ohpe::ui
{
using i18n::tr;

namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

const juce::String& starFull()
{
    static const juce::String s = juce::String::fromUTF8 ("\xe2\x98\x85");
    return s;
}
const juce::String& starEmpty()
{
    static const juce::String s = juce::String::fromUTF8 ("\xe2\x98\x86");
    return s;
}

juce::String categoryLabel (const preset::Category& c)
{
    return i18n::current() == i18n::Language::German ? str (c.nameDe) : juce::String (c.key);
}

juce::String tagLabel (const std::string& tag)
{
    for (const auto& c : preset::categories())
        if (tag == c.key) return categoryLabel (c);
    for (const auto& c : preset::quickTags())
        if (tag == c.key) return categoryLabel (c);
    return str (tag);
}

juce::String engineLabel (const std::string& id)
{
    for (int t = 0; t < numEngineTypes; ++t)
        if (id == engineTypeInfo ((EngineType) t).id) return i18n::pick (engineTypeInfo ((EngineType) t).nameEn, engineTypeInfo ((EngineType) t).nameDe);
    return str (id);
}

void styleEditor (juce::TextEditor& e)
{
    e.setColour (juce::TextEditor::backgroundColourId, colour::displayGrid);
    e.setColour (juce::TextEditor::textColourId, colour::displayText);
    e.setColour (juce::TextEditor::outlineColourId, colour::displayGrid.brighter (0.3f));
    e.setColour (juce::TextEditor::focusedOutlineColourId, colour::sunYellow);
    e.setColour (juce::TextEditor::highlightColourId, colour::ultraviolet.withAlpha (0.5f));
    e.setColour (juce::CaretComponent::caretColourId, colour::sunYellow);
    e.setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
}
} // namespace

LibraryBrowser::LibraryBrowser (OhpeProcessor& p) : proc (p), presets (p.presets())
{
    setWantsKeyboardFocus (true);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, colour::displayDim);
    list.setMultipleSelectionEnabled (false);
    addAndMakeVisible (list);
    for (auto* e : { &search, &saveName, &saveAuthor, &saveFreeTags })
    {
        styleEditor (*e);
        addChildComponent (*e);
    }
    search.setVisible (true);
    languageChanged();
    search.onTextChange = [this] {
        presets.filter.text = search.getText().toStdString();
        rebuild();
    };
    search.onEscapeKey = [this] { close (false); };
    presets.addChangeListener (this);
    setVisible (false);
}

LibraryBrowser::~LibraryBrowser() { presets.removeChangeListener (this); }

void LibraryBrowser::languageChanged()
{
    // p82: the fields' tooltips
    search.setTooltip (tips::t ("Search the presets: name, category, tag, engine, author (English or German)", "Presets suchen: Name, Kategorie, Tag, Engine, Autor (Deutsch oder Englisch)"));
    saveName.setTooltip (tips::t ("Name of the new preset", "Name des neuen Presets"));
    saveAuthor.setTooltip (tips::t ("Who made it (remembered for the next preset)", "Wer es gemacht hat (für das nächste Preset gemerkt)"));
    saveFreeTags.setTooltip (tips::t ("More tags of your own, comma separated", "Weitere eigene Tags, durch Komma getrennt"));
    search.setTextToShowWhenEmpty (tr ("Search: name, category, MPE, engine ..."), colour::displayDim);
    saveName.setTextToShowWhenEmpty (tr ("Name"), colour::displayDim);
    saveAuthor.setTextToShowWhenEmpty (tr ("Author"), colour::displayDim);
    saveFreeTags.setTextToShowWhenEmpty (tr ("More tags, comma separated"), colour::displayDim);
    repaint();
}

float LibraryBrowser::unit() const { return (float) getWidth() / 114.0f; }

juce::Rectangle<float> LibraryBrowser::topArea() const
{
    const float u = unit();
    return { u * 2.0f, u * 1.4f, (float) getWidth() - u * 4.0f, u * 7.0f };
}

juce::Rectangle<float> LibraryBrowser::leftArea() const
{
    const float u = unit();
    return { u * 2.0f, u * 9.6f, u * 22.5f, (float) getHeight() - u * 11.0f };
}

juce::Rectangle<float> LibraryBrowser::rightArea() const
{
    const float u = unit();
    return { u * 80.0f, u * 9.6f, (float) getWidth() - u * 82.0f, (float) getHeight() - u * 11.0f };
}

void LibraryBrowser::open()
{
    presets.rescan();
    presets.beginPreview();
    saving = false;
    status.clear();
    showSavePanel (false);
    setVisible (true);
    rebuild();
    // show the preset that is loaded
    for (int r = 0; r < (int) rows.size(); ++r)
        if (rows[(size_t) r] == presets.current())
        {
            const juce::ScopedValueSetter<bool> svs (ignoreSelection, true);
            list.selectRow (r);
        }
    toFront (true);
    grabKeyboardFocus();
}

juce::String LibraryBrowser::tagText (const std::string& tag) { return tagLabel (tag); }
juce::String LibraryBrowser::engineText (const std::string& id) { return engineLabel (id); }

void LibraryBrowser::followController (int entry)
{
    if (! isVisible()) return;
    rebuild();
    const juce::ScopedValueSetter<bool> svs (ignoreSelection, true);
    const int r = rowOfEntry (entry);
    if (r >= 0)
    {
        list.selectRow (r);
        list.scrollToEnsureRowIsOnscreen (r);
    }
    else list.deselectAllRows();
}

void LibraryBrowser::close (bool keep)
{
    if (! isVisible()) return;
    if (keep) presets.endPreview();
    else presets.revertPreview();
    setVisible (false);
    if (onClosed) onClosed();
}

void LibraryBrowser::rebuild()
{
    rows = presets.view();
    list.updateContent();
    list.repaint();
    repaint();
}

void LibraryBrowser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! isVisible()) return;
    const int keep = selectedEntry();
    rows = presets.view();
    list.updateContent();
    const juce::ScopedValueSetter<bool> svs (ignoreSelection, true);
    for (int r = 0; r < (int) rows.size(); ++r)
        if (rows[(size_t) r] == (presets.current() >= 0 ? presets.current() : keep)) list.selectRow (r);
    repaint();
}

int LibraryBrowser::selectedEntry() const
{
    const int r = list.getSelectedRow();
    return r >= 0 && r < (int) rows.size() ? rows[(size_t) r] : -1;
}

void LibraryBrowser::selectRow (int row)
{
    list.selectRow (row);
}

void LibraryBrowser::setSearchText (const juce::String& text)
{
    search.setText (text, false);
    presets.filter.text = text.toStdString();
    rebuild();
}

void LibraryBrowser::toggleTag (const juce::String& tag)
{
    auto& t = presets.filter.tags;
    const auto s = tag.toStdString();
    if (auto it = std::find (t.begin(), t.end(), s); it != t.end()) t.erase (it);
    else t.push_back (s);
    rebuild();
}

void LibraryBrowser::toggleQuickTag (const juce::String& tag)
{
    auto& t = presets.filter.required;
    const auto s = tag.toStdString();
    if (auto it = std::find (t.begin(), t.end(), s); it != t.end()) t.erase (it);
    else t.push_back (s);
    rebuild();
}

int LibraryBrowser::rowOfEntry (int index) const noexcept
{
    for (int r = 0; r < (int) rows.size(); ++r)
        if (rows[(size_t) r] == index) return r;
    return -1;
}

void LibraryBrowser::showSavePanel (bool show)
{
    saving = show;
    if (show)
    {
        const int sel = presets.current();
        const auto* loaded = sel >= 0 && sel < (int) presets.entries().size() ? &presets.entries()[(size_t) sel] : nullptr;
        if (saveName.isEmpty())
        {
            saveName.setText (loaded != nullptr ? str (loaded->meta.name) : juce::String(), false);
            // the edited sound keeps the tags of the preset it came from (category filters still find it)
            if (loaded != nullptr && saveTags.isEmpty() && saveFreeTags.isEmpty())
            {
                juce::StringArray more;
                for (const auto& t : loaded->meta.tags)
                    (preset::isCategory (t) ? saveTags : more).addIfNotAlreadyThere (str (t));
                saveFreeTags.setText (more.joinIntoString (", "), false);
                saveProject = loaded->meta.kind == preset::Kind::Project;
            }
        }
        if (saveAuthor.isEmpty()) saveAuthor.setText (presets.author(), false);
    }
    for (auto* e : { &saveName, &saveAuthor, &saveFreeTags }) e->setVisible (show);
    resized();
    repaint();
}

void LibraryBrowser::setSaveFields (const juce::String& name, const juce::String& author, const juce::StringArray& tags, bool project)
{
    saveName.setText (name, false);
    saveAuthor.setText (author, false);
    saveTags = tags;
    saveFreeTags.clear();
    saveProject = project;
    repaint();
}

void LibraryBrowser::reveal (int index)
{
    if (index < 0 || index >= (int) presets.entries().size()) return;
    rows = presets.view();
    if (rowOfEntry (index) < 0)
    {
        // a filter hides it (category, engine, search text, favourites, bank, kind): show the user bank
        const auto sort = presets.filter.sort;
        presets.filter = {};
        presets.filter.sort = sort;
        presets.filter.bank = presets.entries()[(size_t) index].factory ? preset::Bank::All : preset::Bank::User;
        search.setText ({}, false);
        rows = presets.view();
    }
    list.updateContent();
    const juce::ScopedValueSetter<bool> svs (ignoreSelection, true); // it is loaded already
    if (const int r = rowOfEntry (index); r >= 0)
    {
        list.selectRow (r);
        list.scrollToEnsureRowIsOnscreen (r);
    }
    list.repaint();
    repaint();
}

bool LibraryBrowser::commitSave (SaveConflict conflict)
{
    preset::Meta meta;
    meta.name = saveName.getText().trim().toStdString();
    meta.author = saveAuthor.getText().trim().toStdString();
    for (const auto& t : saveTags) meta.tags.push_back (t.toStdString());
    for (auto t : juce::StringArray::fromTokens (saveFreeTags.getText(), ",", ""))
        if (t.trim().isNotEmpty() && ! saveTags.contains (t.trim())) meta.tags.push_back (t.trim().toStdString());
    if (meta.name.empty()) meta.name = "Preset";

    const bool project = saveProject;
    if (conflict == SaveConflict::Ask && presets.userFileFor (meta.name, project).exists())
    {
        juce::Component::SafePointer<LibraryBrowser> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("Ohpe")
                                          .withMessage (tr ("A user preset with this name exists already.") + "\n" + str (meta.name))
                                          .withButton (tr ("Replace"))
                                          .withButton (tr ("Keep both"))
                                          .withButton (tr ("Cancel")),
                                      [safe] (int result) {
                                          if (safe == nullptr || ! safe->saving) return;
                                          if (result == 1) safe->commitSave (SaveConflict::Replace);
                                          else if (result == 2) safe->commitSave (SaveConflict::KeepBoth);
                                      });
        return false;
    }

    const auto file = presets.save (meta, project, -1, conflict == SaveConflict::Replace);
    if (file == juce::File())
    {
        status = tr ("Could not save the preset in") + " " + presets.userFolder().getFullPathName();
        statusError = true;
        repaint();
        return false;
    }
    presets.endPreview();
    presets.beginPreview(); // the saved sound is the new "before"
    const int index = presets.indexOfFile (file);
    status = tr ("Saved") + ": " + (index >= 0 ? str (presets.entries()[(size_t) index].meta.name) : file.getFileNameWithoutExtension());
    statusError = false;
    saveName.clear();
    saveFreeTags.clear();
    saveTags.clear();
    showSavePanel (false);
    reveal (index);
    return true;
}

void LibraryBrowser::resized()
{
    const float u = unit();
    list.setBounds (juce::Rectangle<float> (u * 26.0f, u * 10.6f, u * 52.0f, (float) getHeight() - u * 12.0f).toNearestInt());
    list.setRowHeight (juce::jmax (12, (int) (u * 3.4f)));
    search.setBounds (juce::Rectangle<float> (u * 16.0f, u * 1.4f, u * 34.0f, u * 3.2f).toNearestInt());
    search.setFont (labelFont (u * 1.7f));
    auto r = rightArea();
    saveName.setBounds (juce::Rectangle<float> (r.getX(), r.getY() + u * 3.0f, r.getWidth(), u * 3.0f).toNearestInt());
    saveAuthor.setBounds (juce::Rectangle<float> (r.getX(), r.getY() + u * 7.0f, r.getWidth(), u * 3.0f).toNearestInt());
    saveFreeTags.setBounds (juce::Rectangle<float> (r.getX(), r.getY() + u * 29.0f, r.getWidth(), u * 3.0f).toNearestInt());
    for (auto* e : { &saveName, &saveAuthor, &saveFreeTags }) e->setFont (labelFont (u * 1.6f));
}

void LibraryBrowser::chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent,
                           std::function<void()> action)
{
    const float u = unit();
    g.setColour (on ? accent : colour::displayGrid.brighter (0.15f));
    g.fillRoundedRectangle (r, u * 0.6f);
    g.setColour (on ? colour::display : colour::displayText);
    g.setFont (labelFont (juce::jmin (r.getHeight() * 0.52f, u * 1.6f)));
    g.drawText (text, r.reduced (u * 0.4f, 0), juce::Justification::centred, true);
    hits.push_back ({ r, std::move (action), tipNow });
}

juce::String LibraryBrowser::getTooltip()
{
    const auto p = getMouseXYRelative().toFloat();
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (p)) return it->tip;
    return {};
}

std::vector<tips::Region> LibraryBrowser::tipRegions()
{
    {
        juce::Image img (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
        juce::Graphics g (img);
        paint (g); // the hit list of what is shown now
    }
    std::vector<tips::Region> r;
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) r.push_back ({ it->area, it->tip });
    return r;
}

juce::String LibraryBrowser::getTooltipForRow (int row)
{
    if (row < 0 || row >= (int) rows.size()) return {};
    return tips::t ("Click: hear the preset - double-click: keep it and close", "Klick: das Preset anhören - Doppelklick: übernehmen und schließen");
}

void LibraryBrowser::paint (juce::Graphics& g)
{
    hits.clear();
    tipNow = {};
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colour::bezel, 0, b.getY(), colour::bezelLow, 0, b.getBottom(), false)); // p14: dark frame
    const float du = (float) getWidth() / 1152.0f; // p14: design units (the overlay is 1152 wide)
    g.fillRoundedRectangle (b, radius::panel * du);
    g.setColour (colour::display);
    g.fillRoundedRectangle (b.reduced (u * 0.5f), radius::inset * du);

    // top row: title, search (child), current preset, keep / cancel
    g.setColour (colour::displayText);
    g.setFont (brandFont (u * 2.4f));
    g.drawText (i18n::upper (tr ("Library")), juce::Rectangle<float> (u * 2.0f, u * 1.4f, u * 13.5f, u * 3.2f), juce::Justification::centredLeft, true);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.5f));
    g.drawText (tr ("Part") + " " + juce::String (presets.editPart() + 1) + "  /  " + presets.currentName(),
                juce::Rectangle<float> (u * 51.0f, u * 1.4f, u * 37.0f, u * 3.2f), juce::Justification::centredLeft, true);
    rowTip ("Keep: close the library with the preset loaded", "Übernehmen: die Bibliothek mit dem geladenen Preset schließen");
    chip (g, { b.getWidth() - u * 24.0f, u * 1.4f, u * 10.5f, u * 3.2f }, i18n::upper (tr ("Keep")), true, colour::sunYellow, [this] { close (true); });
    rowTip ("Cancel: back to the sound from before (Esc)", "Abbrechen: zurück zum Sound von vorher (Esc)");
    chip (g, { b.getWidth() - u * 12.5f, u * 1.4f, u * 10.5f, u * 3.2f }, i18n::upper (tr ("Cancel")), false, colour::signalRed, [this] { close (false); });

    // second row: bank, kind, favourites, sort
    auto& f = presets.filter;
    float x = u * 2.0f;
    const float y2 = u * 5.6f, h2 = u * 2.8f;
    auto seg = [&] (const juce::String& text, float w, bool on, std::function<void()> act) {
        chip (g, { x, y2, w, h2 }, text, on, colour::displayText, std::move (act));
        x += w + u * 0.5f;
    };
    rowTip ("Bank filter: all, factory or your own presets", "Bank-Filter: alle, Werks- oder eigene Presets");
    seg (tr ("All"), u * 6.0f, f.bank == preset::Bank::All, [this] { presets.filter.bank = preset::Bank::All; rebuild(); });
    seg (tr ("Factory"), u * 8.0f, f.bank == preset::Bank::Factory, [this] { presets.filter.bank = preset::Bank::Factory; rebuild(); });
    seg (tr ("User"), u * 7.5f, f.bank == preset::Bank::User, [this] { presets.filter.bank = preset::Bank::User; rebuild(); });
    x += u * 1.5f;
    rowTip ("Kind filter: part sounds or whole projects (click again: both)", "Art-Filter: Part-Sounds oder ganze Projekte (nochmal: beide)");
    seg (tr ("Parts"), u * 7.0f, f.kind == 0, [this] { presets.filter.kind = presets.filter.kind == 0 ? -1 : 0; rebuild(); });
    seg (tr ("Projects"), u * 8.5f, f.kind == 1, [this] { presets.filter.kind = presets.filter.kind == 1 ? -1 : 1; rebuild(); });
    x += u * 1.5f;
    rowTip ("Only your favourites", "Nur deine Favoriten");
    seg (starFull() + " " + tr ("Favourites"), u * 12.0f, f.favouritesOnly, [this] { presets.filter.favouritesOnly = ! presets.filter.favouritesOnly; rebuild(); });
    x += u * 1.5f;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.4f));
    // the label is as wide as its text (SORTIEREN is much longer than SORT)
    const auto sortLabel = i18n::upper (tr ("Sort"));
    const float sortW = juce::jmax (u * 5.0f, juce::GlyphArrangement::getStringWidth (labelFont (u * 1.4f), sortLabel) + u * 1.0f);
    g.drawText (sortLabel, juce::Rectangle<float> (x, y2, sortW, h2), juce::Justification::centredLeft, false);
    x += sortW;
    const char* sorts[] = { "Name", "Rating", "Engine", "Newest" };
rowTip ("Sort the list: name, rating, engine or newest", "Liste sortieren: Name, Bewertung, Engine oder neueste");
    for (int s = 0; s < 4; ++s)
        seg (tr (sorts[s]), u * 8.0f, (int) f.sort == s, [this, s] { presets.filter.sort = (preset::Sort) s; rebuild(); });
    // content packs: filter by pack, install, manage (menu)
    {
        const float pw = juce::jmin (u * 12.0f, b.getWidth() - u * 2.0f - x);
        if (pw > u * 6.0f)
            rowTip ("Content packs: filter by pack, install or manage packs", "Inhaltspakete: nach Paket filtern, Pakete installieren oder verwalten");
        if (pw > u * 6.0f)
            chip (g, { b.getWidth() - u * 2.0f - pw, y2, pw, h2 }, packChipText(), ! f.pack.empty(), colour::ultraviolet, [this] { showPackMenu(); });
    }

    // left: categories + engines
    auto left = leftArea();
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.4f));
    g.drawText (i18n::upper (tr ("Categories")), left.withHeight (u * 2.2f), juce::Justification::centredLeft, true);
    const auto& cats = preset::categories();
    // 0.6: the chips share the column with the engine chips; they shrink when the categories need more rows
    // p54: the quick chips (MPE, Fingerboard, Morph: "all of") follow the categories in the same grid
    const auto quick = preset::usedQuickTags (presets.entries());
    const size_t numChips = cats.size() + quick.size();
    const float catRows = (float) ((numChips + 1) / 2), engineRows = (float) ((numEngineTypes + 2) / 2);
    const float cw = (left.getWidth() - u * 0.6f) / 2.0f,
                ch = juce::jmin (u * 2.8f, (left.getHeight() - u * 5.8f) / (catRows + engineRows) - u * 0.5f);
    for (size_t i = 0; i < cats.size(); ++i)
    {
        const auto key = std::string (cats[i].key);
        const bool on = std::find (f.tags.begin(), f.tags.end(), key) != f.tags.end();
rowTip ("Category filter - several add up (click again to remove)", "Kategorie-Filter - mehrere ergänzen sich (nochmal klicken entfernt)");
        chip (g, { left.getX() + (cw + u * 0.6f) * (float) (i % 2), left.getY() + u * 2.4f + (ch + u * 0.5f) * (float) (i / 2), cw, ch },
              categoryLabel (cats[i]), on, colour::encoder ((int) i), [this, key] { toggleTag (str (key)); });
    }
    for (size_t q = 0; q < quick.size(); ++q)
    {
        const auto key = quick[q];
        const size_t i = cats.size() + q;
        const bool on = std::find (f.required.begin(), f.required.end(), key) != f.required.end();
rowTip ("Quick filter: only presets that have all chosen tags", "Schnellfilter: nur Presets, die alle gewählten Tags haben");
        chip (g, { left.getX() + (cw + u * 0.6f) * (float) (i % 2), left.getY() + u * 2.4f + (ch + u * 0.5f) * (float) (i / 2), cw, ch },
              tagLabel (key), on, colour::ultraviolet, [this, key] { toggleQuickTag (str (key)); });
    }
    const float ey = left.getY() + u * 2.4f + (ch + u * 0.5f) * catRows + u * 1.0f;
    g.setColour (colour::displayDim);
    g.drawText (i18n::upper (tr ("Engine")), juce::Rectangle<float> (left.getX(), ey, left.getWidth(), u * 2.2f), juce::Justification::centredLeft, true);
    for (int t = -1; t < numEngineTypes; ++t)
    {
        const int i = t + 1;
        const std::string id = t < 0 ? std::string() : engineTypeInfo ((EngineType) t).id;
        const juce::String label = t < 0 ? tr ("All") : engineLabel (id);
rowTip ("Engine filter: presets of one sound engine", "Engine-Filter: Presets einer Klangerzeugung");
        chip (g, { left.getX() + (cw + u * 0.6f) * (float) (i % 2), ey + u * 2.4f + (ch + u * 0.5f) * (float) (i / 2), cw, ch }, label,
              f.engine == id, colour::sunYellow, [this, id] { presets.filter.engine = id; rebuild(); });
    }

    // list header
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    juce::String countText = juce::String ((int) rows.size()) + " " + tr ("presets");
    for (const auto& p : presets.contentPacks())
        if (p.id == f.pack) countText << "  /  " << packName (p) << " " << str (p.version);
    g.drawText (countText, juce::Rectangle<float> (u * 26.0f, u * 8.9f, u * 30.0f, u * 1.5f), juce::Justification::centredLeft, true);

    if (status.isNotEmpty())
    {
        g.setColour (statusError ? colour::signalRed : colour::sunYellow);
        g.drawText (status, juce::Rectangle<float> (u * 44.0f, u * 8.9f, u * 34.0f, u * 1.5f), juce::Justification::centredRight, true);
    }

    // right: details or save panel
    auto r = rightArea();
    g.setColour (colour::displayGrid);
    g.fillRoundedRectangle (r.expanded (u * 0.6f), u * 0.8f);
    if (saving)
    {
        g.setColour (colour::displayText);
        g.setFont (brandFont (u * 1.9f));
        g.drawText (i18n::upper (tr ("Save preset")), r.withHeight (u * 2.6f), juce::Justification::centredLeft, true);
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.3f));
        g.drawText (i18n::upper (tr ("Tags")), juce::Rectangle<float> (r.getX(), r.getY() + u * 10.6f, r.getWidth(), u * 2.0f), juce::Justification::centredLeft, true);
        // three columns, so every category tag fits above the part / project switch
        const float tw = (r.getWidth() - u * 1.0f) / 3.0f, th = u * 2.6f;
        for (size_t i = 0; i < cats.size(); ++i)
        {
            const auto key = juce::String (cats[i].key);
rowTip ("Tag the new preset with this category", "Das neue Preset mit dieser Kategorie markieren");
            chip (g, { r.getX() + (tw + u * 0.5f) * (float) (i % 3), r.getY() + u * 12.8f + (th + u * 0.4f) * (float) (i / 3), tw, th },
                  categoryLabel (cats[i]), saveTags.contains (key), colour::encoder ((int) i), [this, key] {
                      if (saveTags.contains (key)) saveTags.removeString (key);
                      else saveTags.add (key);
                      repaint();
                  });
        }
        const float ky = r.getY() + u * 33.0f;
        rowTip ("Save the edit part only, or the whole project (all parts, mixer, effects)", "Nur den Edit-Part speichern, oder das ganze Projekt (alle Parts, Mixer, Effekte)");
        chip (g, { r.getX(), ky, r.getWidth() * 0.49f, u * 2.8f }, tr ("This part"), ! saveProject, colour::displayText, [this] { saveProject = false; repaint(); });
        chip (g, { r.getX() + r.getWidth() * 0.51f, ky, r.getWidth() * 0.49f, u * 2.8f }, tr ("Whole project"), saveProject, colour::displayText,
              [this] { saveProject = true; repaint(); });
        const float by = r.getBottom() - u * 3.4f;
        rowTip ("Save the preset", "Das Preset speichern");
        chip (g, { r.getX(), by, r.getWidth() * 0.49f, u * 3.2f }, i18n::upper (tr ("Save")), true, colour::sunYellow, [this] { commitSave(); });
        rowTip ("Back to the list without saving", "Zurück zur Liste, ohne zu speichern");
        chip (g, { r.getX() + r.getWidth() * 0.51f, by, r.getWidth() * 0.49f, u * 3.2f }, i18n::upper (tr ("Back")), false, colour::displayText,
              [this] { showSavePanel (false); });
        return;
    }
    const int sel = selectedEntry() >= 0 ? selectedEntry() : presets.current();
    if (sel >= 0 && sel < (int) presets.entries().size())
    {
        const auto& e = presets.entries()[(size_t) sel];
        g.setColour (colour::displayText);
        g.setFont (brandFont (u * 2.2f));
        g.drawFittedText (str (e.meta.name), r.withHeight (u * 3.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setFont (labelFont (u * 1.45f));
        g.setColour (colour::displayDim);
        juce::String bank = e.factory ? tr ("Factory") : tr ("User");
        for (const auto& p : presets.contentPacks())
            if (p.id == e.pack) bank = tr ("Pack") + " " + packName (p);
        juce::String info = bank + "  /  " + (e.meta.kind == preset::Kind::Project ? tr ("Project") : tr ("Part"));
        if (! e.meta.engine.empty()) info << "  /  " << engineLabel (e.meta.engine);
        g.drawText (info, juce::Rectangle<float> (r.getX(), r.getY() + u * 3.2f, r.getWidth(), u * 2.0f), juce::Justification::centredLeft, true);
        if (! e.meta.author.empty())
            g.drawText (tr ("by") + " " + str (e.meta.author), juce::Rectangle<float> (r.getX(), r.getY() + u * 5.2f, r.getWidth(), u * 2.0f),
                        juce::Justification::centredLeft, true);
        float tx = r.getX(), ty = r.getY() + u * 7.8f;
        g.setFont (labelFont (u * 1.3f));
        for (const auto& t : e.meta.tags)
        {
            const auto label = tagLabel (t);
            const float w = juce::GlyphArrangement::getStringWidth (labelFont (u * 1.3f), label) + u * 1.6f;
            if (tx + w > r.getRight()) { tx = r.getX(); ty += u * 2.6f; }
            g.setColour (colour::displayGrid.brighter (0.35f));
            g.fillRoundedRectangle (tx, ty, w, u * 2.1f, u * 0.5f);
            g.setColour (colour::displayText);
            g.drawText (label, juce::Rectangle<float> (tx, ty, w, u * 2.1f), juce::Justification::centred, true);
            tx += w + u * 0.5f;
        }
        if (! e.meta.comment.empty())
        {
            g.setColour (colour::displayDim);
            g.setFont (labelFont (u * 1.3f));
            g.drawFittedText (str (e.meta.comment), juce::Rectangle<float> (r.getX(), ty + u * 3.0f, r.getWidth(), u * 8.0f).toNearestInt(),
                              juce::Justification::topLeft, 4, 0.9f);
        }
        // favourite + rating
        const float ry = r.getY() + u * 26.0f;
        rowTip ("Favourite on / off", "Favorit ein / aus");
        chip (g, { r.getX(), ry, u * 4.0f, u * 3.0f }, e.favourite ? starFull() : starEmpty(), e.favourite, colour::rose,
              [this, sel, fav = e.favourite] { presets.setFavourite (sel, ! fav); });
rowTip ("Rating: one to five stars (click the same star again: none)", "Bewertung: ein bis fünf Sterne (denselben Stern nochmal: keine)");
        for (int s = 1; s <= 5; ++s)
            chip (g, { r.getX() + u * 5.0f + (float) (s - 1) * u * 3.4f, ry, u * 3.0f, u * 3.0f }, s <= e.rating ? starFull() : starEmpty(),
                  s <= e.rating, colour::sunYellow, [this, sel, s, rating = e.rating] { presets.setRating (sel, rating == s ? 0 : s); });
        if (e.meta.kind == preset::Kind::Part && onBreed) // p18
        {
            const juce::Rectangle<float> br { r.getX() + u * 22.6f, ry, r.getWidth() - u * 22.6f, u * 3.0f };
            rowTip ("Breed the current sound with this preset", "Den aktuellen Sound mit diesem Preset kreuzen");
            chip (g, br, i18n::upper (tr ("Breed")), false, colour::ultraviolet, [this, sel] { if (onBreed) onBreed (sel); });
        }
        if (! e.factory)
            rowTip ("Delete this user preset (asks first)", "Dieses User-Preset löschen (fragt vorher)");
        if (! e.factory)
            chip (g, { r.getX(), r.getBottom() - u * 7.2f, r.getWidth(), u * 3.0f }, tr ("Delete"), false, colour::signalRed, [this, sel] {
                juce::Component::SafePointer<LibraryBrowser> safe (this);
                juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                  .withTitle ("Ohpe").withMessage (tr ("Delete this preset?")).withButton (tr ("Delete")).withButton (tr ("Cancel")),
                                              [safe, sel] (int result) {
                                                  if (safe != nullptr && result == 1) safe->presets.remove (sel);
                                              });
            });
    }
    else
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.5f));
        g.drawFittedText (tr ("Select a preset to hear it. Keep closes the library with it, Cancel goes back."),
                          r.withHeight (u * 10.0f).toNearestInt(), juce::Justification::topLeft, 4, 0.9f);
    }
rowTip ("Save the edit part or the project as a user preset", "Den Edit-Part oder das Projekt als User-Preset speichern");
    chip (g, { r.getX(), r.getBottom() - u * 3.4f, r.getWidth(), u * 3.2f }, i18n::upper (tr ("Save preset")) + " ...", false, colour::sunYellow,
          [this] { showSavePanel (true); });
}

void LibraryBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto& e = presets.entries()[(size_t) rows[(size_t) row]];
    const float u = unit();
    auto b = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0, 1.0f);
    if (selected)
    {
        g.setColour (colour::displayText);
        g.fillRoundedRectangle (b, u * 0.5f);
    }
    else if (row % 2 == 0)
    {
        g.setColour (colour::displayGrid.withAlpha (0.6f));
        g.fillRoundedRectangle (b, u * 0.5f);
    }
    const auto ink = selected ? colour::display : colour::displayText;
    const auto dim = selected ? colour::display.withAlpha (0.65f) : colour::displayDim;
    g.setFont (labelFont (u * 1.6f));
    g.setColour (e.favourite ? colour::rose : dim);
    g.drawText (e.favourite ? starFull() : starEmpty(), b.withWidth (u * 3.0f), juce::Justification::centred);
    g.setColour (ink);
    // 1.0: binaural 3D presets carry the headphones pictogram after their name
    const bool is3d = std::find (e.meta.tags.begin(), e.meta.tags.end(), std::string ("3D")) != e.meta.tags.end();
    g.drawText (str (e.meta.name), b.withTrimmedLeft (u * 3.0f).withWidth (is3d ? u * 15.6f : u * 18.0f), juce::Justification::centredLeft, true);
    if (is3d)
        space3d::drawHeadphones (g, b.withX (b.getX() + u * 18.8f).withWidth (u * 2.2f).reduced (0, b.getHeight() * 0.12f),
                                 selected ? colour::ultraviolet.darker (0.2f) : colour::ultraviolet.brighter (0.35f), selected ? 0.0f : 0.5f);
    g.setFont (labelFont (u * 1.3f));
    g.setColour (dim);
    g.drawText (engineLabel (e.meta.engine), b.withTrimmedLeft (u * 21.5f).withWidth (u * 8.0f), juce::Justification::centredLeft, true);
    juce::StringArray tags;
    for (const auto& t : e.meta.tags) tags.add (tagLabel (t));
    g.drawText (tags.joinIntoString (", "), b.withTrimmedLeft (u * 30.0f).withWidth (u * 11.0f), juce::Justification::centredLeft, true);
    juce::String stars;
    for (int s = 1; s <= 5; ++s) stars << (s <= e.rating ? starFull() : starEmpty());
    g.setColour (e.rating > 0 ? (selected ? colour::display : colour::sunYellow) : dim);
    g.drawText (stars, b.withTrimmedLeft (u * 41.5f).withWidth (u * 7.5f), juce::Justification::centredLeft, true);
    g.setColour (dim);
    g.drawText (! e.pack.empty() ? "P" : e.factory ? "F" : "U", b.withTrimmedRight (u * 0.8f), juce::Justification::centredRight, true);
}

juce::String LibraryBrowser::packName (const update::ContentIndex::Pack& p)
{
    return str (i18n::current() == i18n::Language::German && ! p.nameDe.empty() ? p.nameDe : p.nameEn);
}

juce::String LibraryBrowser::packChipText() const
{
    for (const auto& p : presets.contentPacks())
        if (p.id == presets.filter.pack) return i18n::upper (packName (p)) + juce::String::fromUTF8 (" \xe2\x96\xbe");
    return i18n::upper (tr ("Packs")) + juce::String::fromUTF8 (" \xe2\x96\xbe");
}

juce::StringArray LibraryBrowser::packMenuItems() const
{
    juce::StringArray items;
    items.add (tr ("All presets"));
    for (const auto& p : presets.contentPacks()) items.add (packName (p) + "  " + str (p.version));
    items.add (tr ("Install pack") + juce::String::fromUTF8 (" \xe2\x80\xa6"));
    items.add (tr ("Manage packs") + juce::String::fromUTF8 (" \xe2\x80\xa6"));
    return items;
}

void LibraryBrowser::setPackFilter (const juce::String& packId)
{
    presets.filter.pack = packId.toStdString();
    rebuild();
}

void LibraryBrowser::showPackMenu()
{
    juce::PopupMenu m;
    const auto& packs = presets.contentPacks();
    m.addItem (1, tr ("All presets"), true, presets.filter.pack.empty());
    if (! packs.empty()) m.addSeparator();
    for (size_t i = 0; i < packs.size(); ++i)
        m.addItem (10 + (int) i, packName (packs[i]) + "  " + str (packs[i].version), true, packs[i].id == presets.filter.pack);
    if (packs.empty()) m.addItem (2, tr ("No content packs installed"), false, false);
    m.addSeparator();
    m.addItem (3, tr ("Install pack") + juce::String::fromUTF8 (" \xe2\x80\xa6"), onInstallPack != nullptr);
    m.addItem (4, tr ("Manage packs") + juce::String::fromUTF8 (" \xe2\x80\xa6"), onManagePacks != nullptr);
    std::vector<std::string> ids;
    for (const auto& p : packs) ids.push_back (p.id);
    juce::Component::SafePointer<LibraryBrowser> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, ids] (int r) {
        if (safe == nullptr || r == 0) return;
        if (r == 1) safe->setPackFilter ({});
        else if (r == 3 && safe->onInstallPack) safe->onInstallPack();
        else if (r == 4 && safe->onManagePacks) safe->onManagePacks();
        else if (r >= 10 && r - 10 < (int) ids.size()) safe->setPackFilter (juce::String::fromUTF8 (ids[(size_t) (r - 10)].c_str()));
    });
}

void LibraryBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const float u = unit();
    const int idx = rows[(size_t) row];
    if (e.position.x < u * 3.0f) presets.setFavourite (idx, ! presets.entries()[(size_t) idx].favourite);
    else if (e.position.x >= u * 41.5f && e.position.x < u * 49.0f)
    {
        const int s = juce::jlimit (1, 5, 1 + (int) ((e.position.x - u * 41.5f) / (u * 1.5f)));
        presets.setRating (idx, presets.entries()[(size_t) idx].rating == s ? 0 : s);
    }
}

void LibraryBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) rows.size()) close (true);
}

void LibraryBrowser::selectedRowsChanged (int lastRowSelected)
{
    if (ignoreSelection || lastRowSelected < 0 || lastRowSelected >= (int) rows.size()) return;
    presets.load (rows[(size_t) lastRowSelected]); // preview: the sound changes at once
    repaint();
}

void LibraryBrowser::mouseDown (const juce::MouseEvent& e)
{
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            auto action = it->action; // the action may repaint and rebuild the hit list
            if (action) action();
            return;
        }
}

bool LibraryBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close (false); return true; }
    if (k == juce::KeyPress::returnKey && ! saving) { close (true); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey)
    {
        const int n = (int) rows.size();
        if (n == 0) return true;
        const int cur = list.getSelectedRow();
        list.selectRow (juce::jlimit (0, n - 1, cur + (k == juce::KeyPress::downKey ? 1 : -1)));
        return true;
    }
    return false;
}
} // namespace ohpe::ui
