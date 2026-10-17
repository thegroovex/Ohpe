// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SettingsView.h"

#include "Palette.h"
#include "plugin/Localisation.h"

#include <cmath>
#include <iterator>

namespace ohpe::ui
{
using i18n::tr;

namespace
{
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

struct HelpTopic
{
    const char* titleEn;
    const char* titleDe;
    const char* textEn;
    const char* textDe;
};

// the help pages: short, practical, in both languages (UTF-8)
const HelpTopic helpTopics[] = {
    { "Getting started", "Erste Schritte",
      "Ohpe has four parts. Each part plays one sound engine; pick the part at the top, the engine on the left.\n\n"
      "The four coloured encoders under the display change the four values in the same colours on the display. "
      "Hold Shift for fine steps. The arrows on the display turn the pages.\n\n"
      "The keys on the right switch the mode: Synth, Envelope, LFO, Mixer, Effects, Sequencer, Tape and Drum.\n\n"
      "LIBRARY opens the presets, PRO shows every parameter of the mode, RANDOM rolls a new sound (hold it for breeding), "
      "SAVE keeps the edit part as a user preset.",
      "Ohpe hat vier Parts. Jeder Part spielt eine Klangerzeugung; den Part oben wählen, die Engine links.\n\n"
      "Die vier farbigen Encoder unter dem Display ändern die vier Werte in denselben Farben im Display. "
      "Mit Shift in feinen Schritten. Die Pfeile im Display blättern die Seiten.\n\n"
      "Die Tasten rechts wählen den Modus: Synth, Hüllkurve, LFO, Mixer, Effekte, Sequencer, Tape und Drum.\n\n"
      "BIBLIOTHEK öffnet die Presets, PRO zeigt alle Parameter des Modus, ZUFALL würfelt einen neuen Klang (gehalten: Zucht), "
      "SPEICHERN legt den Edit-Part als eigenes Preset ab." },
    { "Light bars", "Leuchtbalken",
      "Every key carries a light bar at its lower edge.\n\n"
      "Yellow: the page or engine you edit, or a switched-on function (loop, keyboard shifted).\n"
      "Violet: something runs, or another part uses it.\n"
      "Green: playing.\n"
      "Red: recording, or MIDI learn is waiting for a control.\n\n"
      "Running lights breathe softly instead of blinking. Settings > Display > Animations calms or stops them.",
      "Jede Taste hat einen Leuchtbalken an der Unterkante.\n\n"
      "Gelb: die Seite oder Engine, die du bearbeitest, oder eine eingeschaltete Funktion (Loop, Tastatur verschoben).\n"
      "Violett: etwas läuft, oder ein anderer Part nutzt es.\n"
      "Grün: spielt.\n"
      "Rot: Aufnahme, oder MIDI-Learn wartet auf einen Regler.\n\n"
      "Laufende Lichter atmen sanft statt zu blinken. Einstellungen > Anzeige > Animationen beruhigt oder stoppt sie." },
    { "Display and encoders", "Display und Encoder",
      "Drag the bars on the display like the encoders. Double-click a value to reset it.\n\n"
      "Right-click any encoder, key or wheel for MIDI learn: move a control on your MIDI device and it is mapped. "
      "Settings > MIDI learn lists every mapping.\n\n"
      "The display can show the stereo scope, the spectrum terrain, the 3D space or the living object - click the visual area to switch.",
      "Die Balken im Display lassen sich ziehen wie die Encoder. Doppelklick setzt einen Wert zurück.\n\n"
      "Rechtsklick auf Encoder, Taste oder Rad öffnet MIDI-Learn: einen Regler am MIDI-Gerät bewegen, schon ist er zugewiesen. "
      "Einstellungen > MIDI-Learn zeigt alle Zuweisungen.\n\n"
      "Das Display zeigt Stereo-Scope, Spektrum-Landschaft, 3D-Raum oder das lebende Objekt - Klick auf die Grafik wechselt." },
    { "Sequencer and tape", "Sequencer und Tape",
      "PLAY starts the sequencer (and the tape, when they are linked). REC records the armed tape track; without an armed track "
      "it arms the selected one.\n\n"
      "In the standalone app: Space = play / stop, R = record, Enter = to the start, L = loop.\n\n"
      "The Sequencer mode has step, Euclid and orbit views; steps can lock parameter values.",
      "START startet den Sequencer (und das Tape, wenn verbunden). REC nimmt auf die scharfe Tape-Spur auf; ohne scharfe Spur "
      "wird die gewählte scharf geschaltet.\n\n"
      "In der Standalone-App: Leertaste = Start / Stopp, R = Aufnahme, Enter = an den Anfang, L = Loop.\n\n"
      "Der Sequencer-Modus hat Step-, Euklid- und Orbit-Ansichten; Schritte können Parameterwerte festhalten." },
    { "MIDI, MPE and sync", "MIDI, MPE und Sync",
      "Each part listens on its own MIDI channel (Omni = every channel). Settings > MIDI sets the channel, MPE and the bend ranges of every part.\n\n"
      "With MPE on, each note gets its own pitch bend, pressure and timbre (lower zone: channel 1 is the master channel).\n\n"
      "Sync: internal tempo, MIDI clock from outside, or network sync (standalone). Clock out sends MIDI clock to other devices.",
      "Jeder Part hört auf seinem MIDI-Kanal (Omni = alle Kanäle). Einstellungen > MIDI setzt Kanal, MPE und Bend-Bereiche jedes Parts.\n\n"
      "Mit MPE bekommt jede Note eigenes Pitchbend, Druck und Klangfarbe (untere Zone: Kanal 1 ist der Master-Kanal).\n\n"
      "Sync: internes Tempo, MIDI-Clock von außen oder Netzwerk-Sync (Standalone). Clock senden schickt MIDI-Clock an andere Geräte." },
    { "Presets and library", "Presets und Bibliothek",
      "The library sorts the presets by category (Bass, Keys, Pads, Drums, Plucks, FX ...). Click a tag to filter, double-click a preset to load it into the edit part.\n\n"
      "Your own presets are kept in your user folder and never overwritten by updates.",
      "Die Bibliothek sortiert die Presets nach Kategorien (Bass, Keys, Pads, Drums, Plucks, FX ...). Ein Tag filtert, Doppelklick lädt ein Preset in den Edit-Part.\n\n"
      "Eigene Presets liegen in deinem Benutzerordner und werden von Updates nie überschrieben." },
    { "Settings and night skin", "Einstellungen und Nachtmodus",
      "Settings are saved for you on this computer - not in projects or presets - and apply to every Ohpe you open.\n\n"
      "Display > Skin: Light, Dark (low glare for night sessions) or Auto (follows your system). Brightness dims the display.\n\n"
      "General > Help overlay names every area of the instrument; F1 switches it on and off.",
      "Einstellungen werden für dich auf diesem Computer gespeichert - nicht in Projekten oder Presets - und gelten für jedes geöffnete Ohpe.\n\n"
      "Anzeige > Erscheinung: Hell, Dunkel (blendarm für Nachtsessions) oder Auto (folgt dem System). Helligkeit dimmt das Display.\n\n"
      "Allgemein > Hilfe-Overlay benennt jeden Bereich des Instruments; F1 schaltet es ein und aus." },
    { "Updates", "Updates",
      "Ohpe can look for new versions once a day (off until you switch it on) or when you press Check now. Nothing about you is sent.\n\n"
      "Downloads are verified before they open. Content packs add presets without reinstalling; the previous pack is kept for a rollback.",
      "Ohpe kann einmal am Tag nach neuen Versionen suchen (aus, bis du es einschaltest) oder wenn du Jetzt prüfen drückst. Nichts über dich wird gesendet.\n\n"
      "Downloads werden vor dem Öffnen geprüft. Inhaltspakete bringen Presets ohne Neuinstallation; das vorige Paket bleibt für ein Zurücksetzen." },
};

juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
} // namespace

// ---------------------------------------------------------------- help pages
int SettingsView::numHelpTopics() { return (int) std::size (helpTopics); }

juce::String SettingsView::helpTitle (int t)
{
    const auto& h = helpTopics[(size_t) juce::jlimit (0, numHelpTopics() - 1, t)];
    return u8 (i18n::current() == i18n::Language::German ? h.titleDe : h.titleEn);
}

juce::String SettingsView::helpText (int t)
{
    const auto& h = helpTopics[(size_t) juce::jlimit (0, numHelpTopics() - 1, t)];
    return u8 (i18n::current() == i18n::Language::German ? h.textDe : h.textEn);
}

juce::String SettingsView::aboutLine() { return u8 ("Beat tactics \xe2\x80\x94 designed by groovex"); }

// ---------------------------------------------------------------- view
SettingsView::SettingsView (OhpeProcessor& p, Updater& u, UserSettingsStore& s) : proc (p), updater (u), store (s)
{
    updater.addChangeListener (this);
    store.addChangeListener (this);
    proc.addChangeListener (this);
    setWantsKeyboardFocus (true);
}

SettingsView::~SettingsView()
{
    proc.removeChangeListener (this);
    store.removeChangeListener (this);
    updater.removeChangeListener (this);
}

void SettingsView::open (int pg)
{
    if (pg >= 0) showPage (pg);
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    resized();
    repaint();
}

void SettingsView::close()
{
    if (! isVisible()) return;
    setVisible (false);
    if (onClosed) onClosed();
}

void SettingsView::showPage (int pg)
{
    current = juce::jlimit (0, numTabs() - 1, pg);
    learnScroll = 0.0f;
    resized();
    repaint();
}

juce::String SettingsView::tabName (int tab) const
{
    switch (tab)
    {
        case General: return tr ("General");
        case Display: return tr ("Display");
        case Midi: return "MIDI";
        case MidiLearn: return tr ("MIDI Learn");
        case Updates: return tr ("Updates");
        case Help: return tr ("Help");
        case About: return tr ("About");
        default: break;
    }
    const int e = tab - numPages;
    if (e >= 0 && e < (int) extras.size())
        return i18n::current() == i18n::Language::German && extras[(size_t) e].nameDe.isNotEmpty() ? extras[(size_t) e].nameDe : extras[(size_t) e].nameEn;
    return {};
}

void SettingsView::addExtraPage (const juce::String& nameEn, const juce::String& nameDe, juce::Component* content)
{
    extras.push_back ({ nameEn, nameDe, content });
    if (content != nullptr) addChildComponent (content);
    resized();
}

void SettingsView::setHelpTopic (int t)
{
    topic = juce::jlimit (0, numHelpTopics() - 1, t);
    helpScroll = 0.0f;
    repaint();
}

void SettingsView::changeSettings (const std::function<void (settings::UserSettings&)>& fn)
{
    auto s = store.get();
    fn (s);
    if (s == store.get()) return;
    store.set (s);
    if (onSettingsChanged) onSettingsChanged();
    repaint();
}

float SettingsView::unit() const { return (float) getWidth() / 114.0f; }

juce::Rectangle<float> SettingsView::contentArea() const
{
    const float u = unit();
    return { u * 24.0f, u * 6.0f, (float) getWidth() - u * 26.0f, (float) getHeight() - u * 8.0f };
}

void SettingsView::resized()
{
    for (size_t e = 0; e < extras.size(); ++e)
        if (auto* c = extras[e].content)
        {
            c->setBounds (contentArea().toNearestInt());
            c->setVisible (isVisible() && current == numPages + (int) e);
        }
}

// ---------------------------------------------------------------- drawing helpers
void SettingsView::chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent,
                         std::function<void()> action, bool enabled)
{
    const float u = unit();
    g.setColour (on ? accent : colour::displayGrid.brighter (0.15f));
    g.fillRoundedRectangle (r, u * 0.6f);
    g.setColour ((on ? colour::display : colour::displayText).withAlpha (enabled ? 1.0f : 0.35f));
    g.setFont (labelFont (juce::jmin (r.getHeight() * 0.52f, u * 1.4f)));
    g.drawFittedText (text, r.reduced (u * 0.4f, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    if (enabled && action) hits.push_back ({ r, text, std::move (action) });
}

void SettingsView::legend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r)
{
    g.setColour (colour::displayDim);
    g.setFont (labelFont (unit() * 1.2f));
    g.drawText (i18n::upper (text), r, juce::Justification::centredLeft, true);
}

void SettingsView::label (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, bool dim)
{
    g.setColour (dim ? colour::displayDim : colour::displayText);
    g.setFont (labelFont (unit() * 1.4f));
    g.drawFittedText (text, r.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
}

void SettingsView::stepper (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& value, std::function<void()> down, std::function<void()> up)
{
    const float h = r.getHeight();
    chip (g, r.withWidth (h), "-", false, colour::displayText, std::move (down));
    chip (g, r.withTrimmedLeft (r.getWidth() - h), "+", false, colour::displayText, std::move (up));
    g.setColour (colour::displayText);
    g.setFont (labelFont (unit() * 1.4f));
    g.drawFittedText (value, r.reduced (h + unit() * 0.2f, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

float SettingsView::plain (const juce::String& id) const
{
    // the host parameter is current at once (the registry follows with the next audio block)
    if (auto* q = proc.parameter (id)) return q->convertFrom0to1 (q->getValue());
    const auto& reg = proc.core().params();
    const int i = reg.indexOf (id.toStdString());
    return i >= 0 ? reg.get (i) : 0.0f;
}

void SettingsView::setPlain (const juce::String& id, float v)
{
    if (auto* q = proc.parameter (id))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (q->convertTo0to1 (v));
        q->endChangeGesture();
    }
    repaint();
}

void SettingsView::step (const juce::String& id, int delta)
{
    if (auto* q = proc.parameter (id))
    {
        const auto range = q->getNormalisableRange();
        setPlain (id, juce::jlimit (range.start, range.end, std::round (plain (id)) + (float) delta));
    }
}

void SettingsView::paintText (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, float& scrollPos, float& maxScrollPos)
{
    const float u = unit();
    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);
    as.setLineSpacing (u * 0.25f);
    as.append (text, labelFont (u * 1.45f), colour::displayText.withAlpha (0.86f));
    juce::TextLayout layout;
    layout.createLayout (as, area.getWidth() - u);
    maxScrollPos = juce::jmax (0.0f, layout.getHeight() - area.getHeight());
    scrollPos = juce::jlimit (0.0f, maxScrollPos, scrollPos);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.toNearestInt());
    layout.draw (g, area.withHeight (layout.getHeight() + 1.0f).translated (0, -scrollPos));
    if (maxScrollPos > 0.0f)
    {
        const float h = area.getHeight() * area.getHeight() / layout.getHeight();
        const float y = area.getY() + (area.getHeight() - h) * (scrollPos / maxScrollPos);
        g.setColour (colour::displayDim.withAlpha (0.6f));
        g.fillRoundedRectangle ({ area.getRight() - u * 0.35f, y, u * 0.3f, h }, u * 0.15f);
    }
}

// ---------------------------------------------------------------- paint
void SettingsView::paint (juce::Graphics& g)
{
    hits.clear();
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    const float du = (float) getWidth() / 1152.0f;
    g.setGradientFill (juce::ColourGradient (colour::bezel, 0, b.getY(), colour::bezelLow, 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, radius::panel * du);
    g.setColour (colour::display);
    g.fillRoundedRectangle (b.reduced (u * 0.5f), radius::inset * du);

    g.setColour (colour::displayText);
    g.setFont (brandFont (u * 2.4f));
    g.drawText (i18n::upper (tr ("Settings")), juce::Rectangle<float> (u * 2.0f, u * 1.4f, u * 30.0f, u * 3.2f), juce::Justification::centredLeft, true);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    g.drawFittedText (tr ("Saved for you on this computer - not in projects or presets"),
                      juce::Rectangle<float> (u * 32.0f, u * 1.4f, u * 66.0f, u * 3.2f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    chip (g, { b.getWidth() - u * 12.5f, u * 1.4f, u * 10.5f, u * 3.2f }, i18n::upper (tr ("Close")), false, colour::displayText, [this] { close(); });

    // tabs
    float y = u * 6.0f;
    for (int t = 0; t < numTabs(); ++t)
    {
        const juce::Rectangle<float> r { u * 2.0f, y, u * 19.0f, u * 3.4f };
        const bool on = t == current;
        g.setColour (on ? colour::displayGrid.brighter (0.25f) : colour::displayGrid);
        g.fillRoundedRectangle (r, u * 0.6f);
        if (on)
        {
            g.setColour (colour::sunYellow); // yellow: the page you edit
            g.fillRoundedRectangle (r.getX() + u * 0.5f, r.getY() + u * 0.8f, u * 0.4f, r.getHeight() - u * 1.6f, u * 0.2f);
        }
        g.setColour (on ? colour::displayText : colour::displayText.withAlpha (0.7f));
        g.setFont (labelFont (u * 1.4f));
        const auto name = i18n::upper (tabName (t));
        g.drawFittedText (name, r.withTrimmedLeft (u * 1.6f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        hits.push_back ({ r, name, [this, t] { showPage (t); } });
        y += u * 4.0f;
    }

    const auto a = contentArea();
    switch (current)
    {
        case General: paintGeneral (g, a); break;
        case Display: paintDisplay (g, a); break;
        case Midi: paintMidi (g, a); break;
        case MidiLearn: paintLearn (g, a); break;
        case Updates: paintUpdates (g, a); break;
        case Help: paintHelp (g, a); break;
        case About: paintAbout (g, a); break;
        default: break; // an extra page: its component draws itself
    }
}

void SettingsView::paintGeneral (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.2f, row = u * 3.9f;
    const auto s = store.get();
    float y = a.getY();
    const float cw = u * 13.0f, right = a.getRight();
    auto chips3 = [&] (float yy, std::initializer_list<std::tuple<juce::String, bool, std::function<void()>>> list) {
        float x = right - (cw + u * 0.6f) * (float) list.size() + u * 0.6f;
        for (const auto& [text, on, fn] : list)
        {
            chip (g, { x, yy, cw, h }, i18n::upper (text), on, colour::sunYellow, fn);
            x += cw + u * 0.6f;
        }
    };
    using L = settings::Language;
    legend (g, tr ("Language"), { a.getX(), y, a.getWidth(), u * 1.6f });
    y += u * 2.0f;
    label (g, tr ("User interface language"), { a.getX(), y, a.getWidth() * 0.4f, h });
    chips3 (y, { { tr ("System"), s.language == L::System, [this] { changeSettings ([] (auto& v) { v.language = L::System; }); } },
                 { "English", s.language == L::English, [this] { changeSettings ([] (auto& v) { v.language = L::English; }); } },
                 { "Deutsch", s.language == L::German, [this] { changeSettings ([] (auto& v) { v.language = L::German; }); } } });
    y += row + u * 1.0f;
    legend (g, tr ("Help"), { a.getX(), y, a.getWidth(), u * 1.6f });
    y += u * 2.0f;
    label (g, tr ("Tooltips"), { a.getX(), y, a.getWidth() * 0.4f, h });
    chips3 (y, { { tr ("Off"), ! s.tooltips, [this] { changeSettings ([] (auto& v) { v.tooltips = false; }); } },
                 { tr ("On"), s.tooltips, [this] { changeSettings ([] (auto& v) { v.tooltips = true; }); } } });
    y += row;
    label (g, tr ("Help overlay (F1)"), { a.getX(), y, a.getWidth() * 0.4f, h });
    chips3 (y, { { tr ("Off"), ! s.helpOverlay, [this] { changeSettings ([] (auto& v) { v.helpOverlay = false; }); } },
                 { tr ("On"), s.helpOverlay, [this] { changeSettings ([] (auto& v) { v.helpOverlay = true; }); } } });
    y += row;
    label (g, tr ("Help pages"), { a.getX(), y, a.getWidth() * 0.4f, h });
    chips3 (y, { { tr ("Open help"), false, [this] { showPage (Help); } } });
    y += row + u * 1.0f;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    g.drawFittedText (tr ("Tooltips explain keys, encoders and light bars when the pointer rests on them. The help overlay names every area of the "
                          "instrument while you play; clicks go through to the controls."),
                      juce::Rectangle<float> (a.getX(), y, a.getWidth(), u * 6.0f).toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
}

void SettingsView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.2f, row = u * 3.9f;
    const auto s = store.get();
    float y = a.getY();
    using S = settings::Skin;
    using A = settings::Animations;
    auto chipsRow = [&] (float yy, float cw, const std::vector<std::tuple<juce::String, bool, std::function<void()>>>& list) {
        float x = a.getRight() - (cw + u * 0.6f) * (float) list.size() + u * 0.6f;
        for (const auto& [text, on, fn] : list)
        {
            chip (g, { x, yy, cw, h }, i18n::upper (text), on, colour::sunYellow, fn);
            x += cw + u * 0.6f;
        }
    };
    legend (g, tr ("Skin"), { a.getX(), y, a.getWidth(), u * 1.6f });
    y += u * 2.0f;
    label (g, tr ("Light, dark (night) or like the system"), { a.getX(), y, a.getWidth() * 0.4f, h });
    chipsRow (y, u * 13.0f, { { tr ("Light"), s.skin == S::Light, [this] { changeSettings ([] (auto& v) { v.skin = S::Light; }); } },
                              { tr ("Dark"), s.skin == S::Dark, [this] { changeSettings ([] (auto& v) { v.skin = S::Dark; }); } },
                              { tr ("Auto"), s.skin == S::Auto, [this] { changeSettings ([] (auto& v) { v.skin = S::Auto; }); } } });
    y += row + u * 1.0f;
    legend (g, tr ("Display"), { a.getX(), y, a.getWidth(), u * 1.6f });
    y += u * 2.0f;
    label (g, tr ("Display brightness"), { a.getX(), y, a.getWidth() * 0.4f, h });
    {
        std::vector<std::tuple<juce::String, bool, std::function<void()>>> list;
        for (float v : { 0.4f, 0.55f, 0.7f, 0.85f, 1.0f })
            list.emplace_back (percent (v), std::abs (s.brightness - v) < 0.075f, [this, v] { changeSettings ([v] (auto& x) { x.brightness = v; }); });
        chipsRow (y, u * 7.4f, list);
    }
    y += row;
    label (g, tr ("Animations"), { a.getX(), y, a.getWidth() * 0.4f, h });
    chipsRow (y, u * 13.0f, { { tr ("Full"), s.animations == A::Full, [this] { changeSettings ([] (auto& v) { v.animations = A::Full; }); } },
                              { tr ("Reduced"), s.animations == A::Reduced, [this] { changeSettings ([] (auto& v) { v.animations = A::Reduced; }); } },
                              { tr ("Off"), s.animations == A::Off, [this] { changeSettings ([] (auto& v) { v.animations = A::Off; }); } } });
    y += row;
    label (g, tr ("Animated 3D visuals"), { a.getX(), y, a.getWidth() * 0.4f, h });
    {
        const bool on = getAnimatedVisuals ? getAnimatedVisuals() : true;
        chipsRow (y, u * 13.0f, { { tr ("Off"), ! on, [this] { if (setAnimatedVisuals) setAnimatedVisuals (false); repaint(); } },
                                  { tr ("On"), on, [this] { if (setAnimatedVisuals) setAnimatedVisuals (true); repaint(); } } });
    }
    y += row;
    label (g, tr ("Window size"), { a.getX(), y, a.getWidth() * 0.4f, h });
    {
        std::vector<std::tuple<juce::String, bool, std::function<void()>>> list;
        for (float v : settings::windowScales)
            list.emplace_back (percent (v), s.windowScale == v, [this, v] { changeSettings ([v] (auto& x) { x.windowScale = v; }); });
        chipsRow (y, u * 9.0f, list);
    }
    y += row + u * 0.6f;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    g.drawFittedText (tr ("The dark skin keeps the glare low for night sessions: warm dark greys, no pure white, dimmed light bars. Animations: "
                          "Reduced slows the breathing lights, Off keeps them still."),
                      juce::Rectangle<float> (a.getX(), y, a.getWidth(), u * 6.0f).toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
}

void SettingsView::paintMidi (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.0f, row = u * 3.6f;
    float y = a.getY();
    // the four parts: channel, MPE, bend range, MPE bend range
    legend (g, tr ("Parts"), { a.getX(), y, u * 12.0f, u * 1.6f });
    const float xCh = a.getX() + u * 12.0f, xMpe = xCh + u * 20.0f, xBend = xMpe + u * 15.0f, xMpeBend = xBend + u * 20.0f;
    legend (g, tr ("MIDI channel"), { xCh, y, u * 18.0f, u * 1.6f });
    legend (g, "MPE", { xMpe, y, u * 13.0f, u * 1.6f });
    legend (g, tr ("Bend range"), { xBend, y, u * 18.0f, u * 1.6f });
    legend (g, tr ("MPE bend range"), { xMpeBend, y, u * 20.0f, u * 1.6f });
    y += u * 2.0f;
    for (int p = 0; p < Core::numParts; ++p)
    {
        const juce::String pre = "p" + juce::String (p + 1) + ".";
        label (g, tr ("Part") + " " + juce::String (p + 1), { a.getX(), y, u * 11.0f, h });
        const int ch = (int) std::lround (plain (pre + "midich"));
        stepper (g, { xCh, y, u * 18.0f, h }, ch == 0 ? juce::String ("Omni") : tr ("Ch") + " " + juce::String (ch),
                 [this, pre] { step (pre + "midich", -1); }, [this, pre] { step (pre + "midich", 1); });
        const bool mpe = plain (pre + "mpe") >= 0.5f;
        chip (g, { xMpe, y, u * 6.2f, h }, i18n::upper (tr ("Off")), ! mpe, colour::sunYellow, [this, pre] { setPlain (pre + "mpe", 0.0f); });
        chip (g, { xMpe + u * 6.8f, y, u * 6.2f, h }, i18n::upper (tr ("On")), mpe, colour::sunYellow, [this, pre] { setPlain (pre + "mpe", 1.0f); });
        stepper (g, { xBend, y, u * 18.0f, h }, juce::String ((int) std::lround (plain (pre + "bend"))) + " " + tr ("st"),
                 [this, pre] { step (pre + "bend", -1); }, [this, pre] { step (pre + "bend", 1); });
        stepper (g, { xMpeBend, y, u * 18.0f, h }, juce::String ((int) std::lround (plain (pre + "mpe.bend"))) + " " + tr ("st"),
                 [this, pre] { step (pre + "mpe.bend", -1); }, [this, pre] { step (pre + "mpe.bend", 1); });
        y += row;
    }
    y += u * 1.0f;
    const float half = (a.getWidth() - u * 3.0f) * 0.5f;
    // left: MIDI inputs
    {
        float yy = y;
        const float x = a.getX();
        legend (g, tr ("MIDI inputs"), { x, yy, half, u * 1.6f });
        yy += u * 2.0f;
        if (auto* dm = proc.standaloneDevices)
        {
            const auto devices = juce::MidiInput::getAvailableDevices();
            if (devices.isEmpty()) { label (g, tr ("No MIDI input found"), { x, yy, half, h }, true); yy += row; }
            for (int i = 0; i < devices.size() && i < 3; ++i)
            {
                const auto& d = devices.getReference (i);
                const bool on = dm->isMidiInputDeviceEnabled (d.identifier);
                label (g, d.name, { x, yy, half - u * 14.0f, h });
                chip (g, { x + half - u * 13.0f, yy, u * 6.2f, h }, i18n::upper (tr ("Off")), ! on, colour::sunYellow,
                      [this, id = d.identifier] { if (auto* m = proc.standaloneDevices) m->setMidiInputDeviceEnabled (id, false); repaint(); });
                chip (g, { x + half - u * 6.2f, yy, u * 6.2f, h }, i18n::upper (tr ("On")), on, colour::sunYellow,
                      [this, id = d.identifier] { if (auto* m = proc.standaloneDevices) m->setMidiInputDeviceEnabled (id, true); repaint(); });
                yy += row;
            }
            if (proc.openStandaloneAudioSettings)
                chip (g, { x, yy, half, h }, i18n::upper (tr ("Audio Settings ...")), false, colour::displayText, [this] { proc.openStandaloneAudioSettings(); });
        }
        else
        {
            g.setColour (colour::displayDim);
            g.setFont (labelFont (u * 1.3f));
            g.drawFittedText (tr ("In a plug-in the host program chooses the MIDI inputs and sends them to Ohpe. Each part plays the channel set above."),
                              juce::Rectangle<float> (x, yy, half, u * 7.0f).toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
        }
    }
    // right: clock and sync
    {
        float yy = y;
        const float x = a.getX() + half + u * 3.0f;
        legend (g, tr ("Clock and sync"), { x, yy, half, u * 1.6f });
        yy += u * 2.0f;
        const int src = (int) std::lround (plain (Core::Ids::syncSource));
        const float cw = (half - u * 1.2f) / 3.0f;
        const juce::String names[] = { tr ("Internal"), "MIDI Clock", "Link" };
        for (int i = 0; i < 3; ++i)
            chip (g, { x + (cw + u * 0.6f) * (float) i, yy, cw, h }, i18n::upper (names[i]), src == i, colour::sunYellow,
                  [this, i] { setPlain (Core::Ids::syncSource, (float) i); });
        yy += row;
        const bool out = plain (Core::Ids::clockOut) >= 0.5f;
        label (g, tr ("MIDI Clock Out"), { x, yy, half - u * 14.0f, h });
        chip (g, { x + half - u * 13.0f, yy, u * 6.2f, h }, i18n::upper (tr ("Off")), ! out, colour::sunYellow, [this] { setPlain (Core::Ids::clockOut, 0.0f); });
        chip (g, { x + half - u * 6.2f, yy, u * 6.2f, h }, i18n::upper (tr ("On")), out, colour::sunYellow, [this] { setPlain (Core::Ids::clockOut, 1.0f); });
        yy += row;
        const float sm = plain (Core::Ids::syncSmoothing);
        label (g, tr ("Clock Smoothing"), { x, yy, half - u * 19.0f, h });
        stepper (g, { x + half - u * 18.0f, yy, u * 18.0f, h }, percent (sm),
                 [this, sm] { setPlain (Core::Ids::syncSmoothing, juce::jlimit (0.0f, 1.0f, std::round (sm * 10.0f - 1.0f) / 10.0f)); },
                 [this, sm] { setPlain (Core::Ids::syncSmoothing, juce::jlimit (0.0f, 1.0f, std::round (sm * 10.0f + 1.0f) / 10.0f)); });
    }
}

juce::String SettingsView::mappingRow (const MidiMapping& m) const
{
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (m.paramId);
    const juce::String name = idx >= 0 ? i18n::pick (reg.descriptor (idx).nameEn, reg.descriptor (idx).nameDe) : juce::String (m.paramId);
    const juce::String src = m.type == MidiSourceType::CC ? "CC " + juce::String (m.number) : tr ("Note") + " " + juce::MidiMessage::getMidiNoteName (m.number, true, true, 3);
    const juce::String ch = m.channel == 0 ? juce::String ("Omni") : tr ("Ch") + " " + juce::String (m.channel);
    const juce::String mode = m.mode == MidiMapMode::Toggle ? tr ("Toggle") : m.mode == MidiMapMode::Relative ? tr ("Relative") : tr ("Absolute");
    const juce::String range = percent (m.minNorm) + juce::String::fromUTF8 (" \xe2\x86\x92 ") + percent (m.maxNorm);
    return name + "\t" + src + "\t" + ch + "\t" + mode + "\t" + range;
}

juce::StringArray SettingsView::learnRows() const
{
    juce::StringArray rows;
    for (const auto& m : proc.midiMappings()) rows.add (mappingRow (m));
    return rows;
}

void SettingsView::paintLearn (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.0f, row = u * 3.5f;
    const auto maps = proc.midiMappings();
    float y = a.getY();
    legend (g, tr ("MIDI Learn") + "  " + juce::String ((int) maps.size()), { a.getX(), y, u * 30.0f, h });
    const float cw = u * 13.0f;
    float x = a.getRight() - cw * 3.0f - u * 1.2f;
    chip (g, { x, y, cw, h }, i18n::upper (tr ("Import ...")), false, colour::displayText, [this] { mapFile (false); });
    chip (g, { x + cw + u * 0.6f, y, cw, h }, i18n::upper (tr ("Export ...")), false, colour::displayText, [this] { mapFile (true); }, ! maps.empty());
    chip (g, { x + (cw + u * 0.6f) * 2.0f, y, cw, h }, i18n::upper (tr ("Clear all")), false, colour::signalRed, [this] { proc.clearAllMidiMappings(); repaint(); },
          ! maps.empty());
    y += h + u * 1.2f;
    const float colName = a.getX(), colSrc = a.getX() + u * 30.0f, colCh = colSrc + u * 11.0f, colMode = colCh + u * 9.0f, colRange = colMode + u * 11.0f,
                colKeys = a.getRight() - u * 19.0f;
    legend (g, tr ("Parameter"), { colName, y, u * 28.0f, u * 1.6f });
    legend (g, tr ("Control"), { colSrc, y, u * 10.0f, u * 1.6f });
    legend (g, tr ("Channel"), { colCh, y, u * 8.0f, u * 1.6f });
    legend (g, tr ("Mode"), { colMode, y, u * 10.0f, u * 1.6f });
    legend (g, tr ("Range"), { colRange, y, u * 12.0f, u * 1.6f });
    y += u * 2.2f;
    const juce::Rectangle<float> list { a.getX(), y, a.getWidth(), a.getBottom() - y };
    if (maps.empty())
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.4f));
        g.drawFittedText (tr ("No MIDI mappings yet. Right-click an encoder, key or wheel and choose MIDI Learn, then move a control on your MIDI device."),
                          list.withHeight (u * 6.0f).toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
        learnMax = 0.0f;
        return;
    }
    learnMax = juce::jmax (0.0f, (float) maps.size() * row - list.getHeight());
    learnScroll = juce::jlimit (0.0f, learnMax, learnScroll);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (list.toNearestInt());
    for (size_t i = 0; i < maps.size(); ++i)
    {
        const float yy = list.getY() + (float) i * row - learnScroll;
        if (yy + h < list.getY() || yy > list.getBottom()) continue;
        if (i % 2 == 0)
        {
            g.setColour (colour::displayGrid.withAlpha (0.6f));
            g.fillRoundedRectangle (a.getX() - u * 0.5f, yy - u * 0.2f, a.getWidth() + u * 0.5f, h + u * 0.4f, u * 0.5f);
        }
        const auto cells = juce::StringArray::fromTokens (mappingRow (maps[i]), "\t", {});
        label (g, cells[0], { colName, yy, u * 29.0f, h });
        label (g, cells[1], { colSrc, yy, u * 10.5f, h });
        label (g, cells[2], { colCh, yy, u * 8.5f, h }, true);
        label (g, cells[3], { colMode, yy, u * 10.5f, h }, true);
        label (g, cells[4], { colRange, yy, colKeys - colRange - u, h }, true);
        const juce::Rectangle<float> editR { colKeys, yy, u * 9.0f, h };
        if (list.contains (editR.getCentre()))
        {
            chip (g, editR, i18n::upper (tr ("Edit")), false, colour::displayText, [this, i, editR] { editMapping ((int) i, editR); });
            chip (g, { colKeys + u * 9.6f, yy, u * 9.4f, h }, i18n::upper (tr ("Clear")), false, colour::signalRed,
                  [this, i] { proc.removeMidiMapping ((int) i); repaint(); });
        }
    }
}

void SettingsView::editMapping (int index, juce::Rectangle<float> near)
{
    const auto maps = proc.midiMappings();
    if (index < 0 || index >= (int) maps.size()) return;
    const auto m = maps[(size_t) index];
    juce::PopupMenu menu, chMenu, modeMenu, rangeMenu;
    chMenu.addItem (100, "Omni", true, m.channel == 0);
    for (int c = 1; c <= 16; ++c) chMenu.addItem (100 + c, tr ("Ch") + " " + juce::String (c), true, m.channel == c);
    modeMenu.addItem (200, tr ("Absolute"), true, m.mode == MidiMapMode::Absolute);
    modeMenu.addItem (201, tr ("Toggle"), true, m.mode == MidiMapMode::Toggle);
    modeMenu.addItem (202, tr ("Relative"), true, m.mode == MidiMapMode::Relative);
    rangeMenu.addItem (300, tr ("Full range"), true, m.minNorm == 0.0f && m.maxNorm == 1.0f);
    rangeMenu.addItem (301, tr ("Inverted"), true, m.minNorm == 1.0f && m.maxNorm == 0.0f);
    rangeMenu.addItem (302, tr ("Lower half"), true, m.minNorm == 0.0f && m.maxNorm == 0.5f);
    rangeMenu.addItem (303, tr ("Upper half"), true, m.minNorm == 0.5f && m.maxNorm == 1.0f);
    menu.addSectionHeader (mappingRow (m).upToFirstOccurrenceOf ("\t", false, false));
    menu.addSubMenu (tr ("Channel"), chMenu);
    menu.addSubMenu (tr ("Mode"), modeMenu);
    menu.addSubMenu (tr ("Range"), rangeMenu);
    menu.addSeparator();
    menu.addItem (400, tr ("Learn again"));
    menu.addItem (401, tr ("Clear"));
    juce::Component::SafePointer<SettingsView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (near.toNearestInt())), [safe, index, m] (int r) {
        if (safe == nullptr || r == 0) return;
        auto e = m;
        if (r >= 100 && r <= 116) e.channel = r - 100;
        else if (r >= 200 && r <= 202) e.mode = (MidiMapMode) (r - 200);
        else if (r == 300) { e.minNorm = 0.0f; e.maxNorm = 1.0f; }
        else if (r == 301) { e.minNorm = 1.0f; e.maxNorm = 0.0f; }
        else if (r == 302) { e.minNorm = 0.0f; e.maxNorm = 0.5f; }
        else if (r == 303) { e.minNorm = 0.5f; e.maxNorm = 1.0f; }
        else if (r == 400)
        {
            safe->proc.removeMidiMapping (index);
            safe->proc.armMidiLearn (juce::String (m.paramId));
            safe->repaint();
            return;
        }
        else if (r == 401) { safe->proc.removeMidiMapping (index); safe->repaint(); return; }
        safe->proc.setMidiMapping (index, e);
        safe->repaint();
    });
}

void SettingsView::mapFile (bool save)
{
    chooser = std::make_unique<juce::FileChooser> (save ? tr ("Export MIDI map ...") : tr ("Import MIDI map ..."),
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.ohpemidi");
    const int flags = (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode)
                      | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<SettingsView> safe (this);
    chooser->launchAsync (flags, [safe, save] (const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        if (save) safe->proc.exportMidiMap (f.withFileExtension ("ohpemidi"));
        else if (! safe->proc.importMidiMap (f))
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon).withTitle ("Ohpe")
                                              .withMessage (tr ("The file is not an Ohpe MIDI map.")).withButton ("OK"),
                                          nullptr);
        safe->repaint();
    });
}

void SettingsView::paintUpdates (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.2f, row = u * 3.9f;
    const auto s = updater.snapshot();
    const auto prefs = updater.settings();
    const bool busy = s.state == Updater::State::Checking || s.state == Updater::State::Downloading;
    float y = a.getY();
    legend (g, tr ("Updates"), { a.getX(), y, a.getWidth(), u * 1.6f });
    y += u * 2.0f;
    label (g, tr ("Installed") + " " + juce::String::fromUTF8 (Updater::currentVersion().toString().c_str()), { a.getX(), y, a.getWidth() * 0.5f, h });
    if (s.message.isNotEmpty())
    {
        const bool alert = s.state == Updater::State::Failed || s.state == Updater::State::Offline;
        g.setColour (alert ? colour::signalRed : s.hasRelease ? colour::sunYellow : colour::displayDim);
        g.setFont (labelFont (u * 1.4f));
        g.drawFittedText (tr (s.message) + (s.detail.isNotEmpty() && s.state != Updater::State::Downloading ? "  " + s.detail : juce::String()),
                          juce::Rectangle<float> (a.getCentreX(), y, a.getWidth() * 0.5f, h).toNearestInt(), juce::Justification::centredRight, 1, 0.8f);
    }
    y += row;
    const float cw = u * 13.0f;
    auto pair = [&] (float yy, const juce::String& l, const juce::String& t1, bool on1, std::function<void()> f1, const juce::String& t2, bool on2,
                     std::function<void()> f2) {
        label (g, l, { a.getX(), yy, a.getWidth() - cw * 2.0f - u * 2.0f, h });
        chip (g, { a.getRight() - cw * 2.0f - u * 0.6f, yy, cw, h }, i18n::upper (t1), on1, colour::sunYellow, std::move (f1));
        chip (g, { a.getRight() - cw, yy, cw, h }, i18n::upper (t2), on2, colour::sunYellow, std::move (f2));
    };
    pair (y, tr ("Check automatically (once a day)"), tr ("Off"), ! prefs.autoCheck, [this] { updater.setAutoCheck (false); repaint(); }, tr ("On"),
          prefs.autoCheck, [this] { updater.setAutoCheck (true); repaint(); });
    y += row;
    pair (y, tr ("Channel"), tr ("Stable"), prefs.channel == update::Channel::Stable, [this] { updater.setChannel (update::Channel::Stable); repaint(); },
          tr ("Preview"), prefs.channel == update::Channel::Preview, [this] { updater.setChannel (update::Channel::Preview); repaint(); });
    y += row;
    label (g, tr ("Last check"), { a.getX(), y, a.getWidth() * 0.5f, h });
    g.setColour (colour::displayDim);
    g.drawText (prefs.lastCheck > 0 ? juce::Time (prefs.lastCheck * 1000).formatted ("%Y-%m-%d %H:%M") : tr ("never"),
                juce::Rectangle<float> (a.getRight() - cw * 2.0f - u * 0.6f, y, cw * 2.0f + u * 0.6f, h), juce::Justification::centredRight, true);
    y += row + u * 0.8f;
    chip (g, { a.getX(), y, u * 18.0f, h }, i18n::upper (tr ("Check now")), false, colour::displayText, [this] { updater.checkNow(); repaint(); }, ! busy);
    chip (g, { a.getX() + u * 19.0f, y, u * 24.0f, h },
          i18n::upper (s.hasRelease ? tr ("Update to") + " " + juce::String::fromUTF8 (s.release.version.toString().c_str()) : tr ("Open updates")),
          s.hasRelease, colour::sunYellow, [this] { if (onOpenUpdates) onOpenUpdates(); });
    y += row + u * 1.0f;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    g.drawFittedText (tr ("No telemetry: Ohpe only reads the public release list. Your presets and settings are never touched."),
                      juce::Rectangle<float> (a.getX(), y, a.getWidth(), u * 4.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
}

void SettingsView::paintHelp (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.0f;
    float y = a.getY();
    const float listW = u * 24.0f;
    for (int t = 0; t < numHelpTopics(); ++t)
    {
        const juce::Rectangle<float> r { a.getX(), y, listW, h };
        const bool on = t == topic;
        chip (g, r, helpTitle (t), on, colour::sunYellow, [this, t] { setHelpTopic (t); });
        y += h + u * 0.6f;
    }
    const juce::Rectangle<float> text { a.getX() + listW + u * 2.5f, a.getY(), a.getWidth() - listW - u * 2.5f, a.getHeight() - u * 3.0f };
    g.setColour (colour::displayText);
    g.setFont (brandFont (u * 2.0f));
    g.drawFittedText (helpTitle (topic), text.withHeight (u * 3.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    paintText (g, text.withTrimmedTop (u * 4.0f), helpText (topic), helpScroll, helpMax);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.2f));
    g.drawText ("OHPE  " + aboutLine(), juce::Rectangle<float> (text.getX(), a.getBottom() - u * 2.0f, text.getWidth(), u * 2.0f), juce::Justification::centredLeft, true);
}

void SettingsView::paintAbout (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.2f;
    float y = a.getY() + u * 1.0f;
    g.setColour (colour::displayText);
    g.setFont (brandFont (u * 6.0f));
    g.drawText ("OHPE", juce::Rectangle<float> (a.getX(), y, a.getWidth(), u * 6.5f), juce::Justification::centredLeft, true);
    {
        const float w = juce::GlyphArrangement::getStringWidth (brandFont (u * 6.0f), "OHPE");
        g.setColour (colour::signalRed);
        g.fillEllipse (a.getX() + w + u * 0.8f, y + u * 4.2f, u * 1.0f, u * 1.0f);
    }
    y += u * 7.0f;
    g.setColour (colour::displayText.withAlpha (0.86f));
    g.setFont (labelFont (u * 2.0f));
    g.drawText (aboutLine(), juce::Rectangle<float> (a.getX(), y, a.getWidth(), u * 3.0f), juce::Justification::centredLeft, true);
    y += u * 4.4f;
    const juce::String format = standalone ? tr ("Standalone app") : tr ("Plug-in");
    label (g, tr ("Version") + " " + juce::String::fromUTF8 (Updater::currentVersion().toString().c_str()) + "   " + format, { a.getX(), y, a.getWidth(), h });
    y += u * 4.0f;
    // update status and the one button
    const auto s = updater.snapshot();
    const bool busy = s.state == Updater::State::Checking || s.state == Updater::State::Downloading;
    const juce::String status = s.hasRelease ? tr ("New version available") + "  " + juce::String::fromUTF8 (s.release.version.toString().c_str())
                              : s.state == Updater::State::UpToDate ? tr ("Ohpe is up to date")
                              : s.state == Updater::State::Checking ? tr ("Checking for updates")
                              : s.state == Updater::State::Offline  ? tr ("No connection to the update server")
                              : s.message.isNotEmpty()              ? tr (s.message)
                                                                    : tr ("Not checked yet");
    if (s.hasRelease)
    {
        g.setColour (colour::sunYellow);
        g.fillRoundedRectangle (a.getX(), y + u * 1.3f, u * 1.6f, u * 0.6f, u * 0.3f);
    }
    g.setColour (s.state == Updater::State::Failed || s.state == Updater::State::Offline ? colour::signalRed : colour::displayText);
    g.setFont (labelFont (u * 1.4f));
    g.drawFittedText (status, juce::Rectangle<float> (a.getX() + (s.hasRelease ? u * 2.4f : 0.0f), y, a.getWidth() * 0.5f, h).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.8f);
    if (s.hasRelease)
        chip (g, { a.getX() + a.getWidth() * 0.5f, y, u * 24.0f, h }, i18n::upper (tr ("Open updates")), true, colour::sunYellow,
              [this] { if (onOpenUpdates) onOpenUpdates(); });
    else
        chip (g, { a.getX() + a.getWidth() * 0.5f, y, u * 18.0f, h }, i18n::upper (tr ("Check now")), false, colour::displayText,
              [this] { updater.checkNow(); repaint(); }, ! busy);
    y += u * 5.0f;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    g.drawFittedText (tr ("Free software under the GNU Affero General Public License 3.0 or later. Source code: github.com/thegroovex/Ohpe") + "\n"
                          + tr ("Fonts and samples under open licences (SIL OFL, CC0) - see THIRD-PARTY.md.") + "\n"
                          + tr ("No telemetry: Ohpe only reads the public release list. Your presets and settings are never touched."),
                      juce::Rectangle<float> (a.getX(), y, a.getWidth(), u * 8.0f).toNearestInt(), juce::Justification::topLeft, 5, 0.9f);
}

// ---------------------------------------------------------------- input
void SettingsView::mouseDown (const juce::MouseEvent& e)
{
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            auto action = it->action;
            if (action) action();
            repaint();
            return;
        }
}

void SettingsView::paintOffscreen()
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    juce::Graphics g (img);
    paint (g);
}

bool SettingsView::clickChip (const juce::String& text)
{
    paintOffscreen(); // the hit list of the page shown now
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->text == text)
        {
            auto action = it->action;
            if (action) action();
            paintOffscreen();
            return true;
        }
    return false;
}

juce::StringArray SettingsView::chipTexts()
{
    paintOffscreen();
    juce::StringArray t;
    for (const auto& h : hits) t.add (h.text);
    return t;
}

void SettingsView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (current == Help) helpScroll = juce::jlimit (0.0f, helpMax, helpScroll - w.deltaY * unit() * 12.0f);
    if (current == MidiLearn) learnScroll = juce::jlimit (0.0f, learnMax, learnScroll - w.deltaY * unit() * 12.0f);
    repaint();
}

bool SettingsView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    if (k == juce::KeyPress::upKey) { showPage ((current + numTabs() - 1) % numTabs()); return true; }
    if (k == juce::KeyPress::downKey) { showPage ((current + 1) % numTabs()); return true; }
    return false;
}

// ---------------------------------------------------------------- help overlay
void HelpOverlay::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 1200.0f;
    for (const auto& it : items)
    {
        const juce::Rectangle<float> r { it.design.getX() * s, it.design.getY() * s, it.design.getWidth() * s, it.design.getHeight() * s };
        g.setColour (colour::sunYellow.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (1.0f * s), radius::panel * s, 1.5f * s);
        // a dark tag at the area's top left: name in yellow, one line of explanation
        const auto titleFont = brandFont (12.0f * s);
        const auto textFont = labelFont (11.0f * s);
        const float w = juce::jmin (r.getWidth() - 8.0f * s,
                                    juce::jmax (juce::GlyphArrangement::getStringWidth (titleFont, it.title), juce::GlyphArrangement::getStringWidth (textFont, it.text))
                                        + 16.0f * s);
        const bool twoLines = it.text.isNotEmpty();
        const juce::Rectangle<float> tag { r.getX() + 4.0f * s, r.getY() + 4.0f * s, w, (twoLines ? 34.0f : 20.0f) * s };
        g.setColour (colour::display.withAlpha (0.88f));
        g.fillRoundedRectangle (tag, radius::inset * s);
        g.setColour (colour::sunYellow);
        g.setFont (titleFont);
        g.drawFittedText (i18n::upper (it.title), tag.reduced (8.0f * s, 3.0f * s).withHeight (14.0f * s).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        if (twoLines)
        {
            g.setColour (colour::displayText);
            g.setFont (textFont);
            g.drawFittedText (it.text, tag.reduced (8.0f * s, 3.0f * s).withTrimmedTop (15.0f * s).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        }
    }
}
} // namespace ohpe::ui
