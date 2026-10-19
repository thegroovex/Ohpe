// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SettingsView.h"

#include "GridController.h"

#include "Palette.h"
#include "Brand.h"
#include "plugin/Localisation.h"
#include "BinaryData.h"

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
      "LIBRARY opens the presets, PRO shows every parameter of the mode, BREED crosses two sounds into new ones, RANDOM rolls a new sound, "
      "SAVE keeps the edit part as a user preset.",
      "Ohpe hat vier Parts. Jeder Part spielt eine Klangerzeugung; den Part oben wählen, die Engine links.\n\n"
      "Die vier farbigen Encoder unter dem Display ändern die vier Werte in denselben Farben im Display. "
      "Mit Shift in feinen Schritten. Die Pfeile im Display blättern die Seiten.\n\n"
      "Die Tasten rechts wählen den Modus: Synth, Hüllkurve, LFO, Mixer, Effekte, Sequencer, Tape und Drum.\n\n"
      "BIBLIOTHEK öffnet die Presets, PRO zeigt alle Parameter des Modus, ZUCHT kreuzt zwei Klänge zu neuen, ZUFALL würfelt einen neuen Klang, "
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
      "The display can show the stereo scope, the spectrum terrain, the 3D space or the living object - click the visual area to switch.\n\n"
      "FX editor (Effects mode, Rack / Master): the FX editor key or a click on the slot picture shows every parameter of the slot at once. "
      "Drag the points in the picture (EQ bands, threshold, rate / depth ...); Alt-drag or the wheel changes the second value (Q, knee, shape); "
      "a clicked point goes onto the encoders.",
      "Die Balken im Display lassen sich ziehen wie die Encoder. Doppelklick setzt einen Wert zurück.\n\n"
      "Rechtsklick auf Encoder, Taste oder Rad öffnet MIDI-Learn: einen Regler am MIDI-Gerät bewegen, schon ist er zugewiesen. "
      "Einstellungen > MIDI-Learn zeigt alle Zuweisungen.\n\n"
      "Das Display zeigt Stereo-Scope, Spektrum-Landschaft, 3D-Raum oder das lebende Objekt - Klick auf die Grafik wechselt.\n\n"
      "FX-Editor (Effekte-Modus, Rack / Master): die Taste FX-Editor oder ein Klick auf das Slot-Bild zeigt alle Parameter des Slots auf einmal. "
      "Die Punkte im Bild ziehen (EQ-Bänder, Schwelle, Tempo / Tiefe ...); Alt-Ziehen oder das Rad ändert den zweiten Wert (Güte, Knie, Form); "
      "ein angeklickter Punkt liegt auf den Encodern." },
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
      "Fingerboard mode (p50): for continuous fingerboard controllers set to MPE HD (14/21-bit). Tap the part under Settings > MIDI > "
      "Fingerboard mode: MPE on, bend range 96, the level follows the pressure. Slide = pitch, forward / back = timbre target, "
      "pressure = level and pressure target; pedals on channel 1 act on every note.\n\n"
      "Sync: internal tempo, MIDI clock from outside, or network sync (standalone). Clock out sends MIDI clock to other devices.",
      "Jeder Part hört auf seinem MIDI-Kanal (Omni = alle Kanäle). Einstellungen > MIDI setzt Kanal, MPE und Bend-Bereiche jedes Parts.\n\n"
      "Mit MPE bekommt jede Note eigenes Pitchbend, Druck und Klangfarbe (untere Zone: Kanal 1 ist der Master-Kanal).\n\n"
      "Fingerboard-Modus (p50): für Fingerboard-Controller mit durchgehender Fläche im Modus MPE HD (14/21 Bit). Unter Einstellungen > MIDI > "
      "Fingerboard-Modus den Part antippen: MPE ein, Bend-Bereich 96, der Pegel folgt dem Druck. Gleiten = Tonhöhe, vor / zurück = "
      "Timbre-Ziel, Druck = Pegel und Druck-Ziel; Pedale auf Kanal 1 wirken auf alle Noten.\n\n"
      "Sync: internes Tempo, MIDI-Clock von außen oder Netzwerk-Sync (Standalone). Clock senden schickt MIDI-Clock an andere Geräte." },
    { "Grid keyboard", "Raster-Tastatur", // p48
      "GRID (between OCTAVE + and MIDI) swaps the piano keys for 4 rows of 24 cells. It is isomorphic: every interval, chord and "
      "scale has the same shape wherever you play it - learn a shape once, move it to change key.\n\n"
      "Layouts (LFO > Harmony > Grid Layout):\n"
      "Fourths: one cell right = +1 semitone, one row up = +5 (a fourth).\n"
      "Thirds: right +1, up +4 (a major third).\n"
      "Whole Tones: right +2, up +5.\n"
      "In Key: only the notes of the remembered key; right = the next scale note, up = three scale steps.\n\n"
      "Shapes in Fourths: major triad = the root, 4 cells right, and 2 right + 1 up. Minor triad = the root, 3 right, 2 right + 1 up. "
      "Fifth = 2 right + 1 up. Octave = 2 right + 2 up. Moving a shape two cells right plays it a whole tone higher.\n\n"
      "Cells: light = key note, graphite = other note, bold with octave = the key's root, violet bar = chord tone, red bar = held. "
      "Strike higher in a cell for more velocity. OCTAVE - / + shifts the grid: the bottom-left cell is the C of that octave "
      "(In Key: the first key note from there).\n\n"
      "The grid plays the edit part. With MPE on for that part every touch has its own channel: slide sideways = pitch bend (one cell "
      "= one step of the layout, within the part's bend range), up / down = slide (CC 74), touch or pen pressure = pressure. "
      "Without MPE the notes go through scale, chord and follow, and sliding does nothing.\n\n"
      "The full guide with diagrams: Harmony in the documentation.",
      "RASTER (zwischen OKTAVE + und MIDI) tauscht die Klaviertasten gegen 4 Reihen mit 24 Feldern. Es ist isomorph: Jedes Intervall, "
      "jeder Akkord und jede Skala hat überall dieselbe Form - eine Form einmal lernen, verschieben wechselt die Tonart.\n\n"
      "Anordnungen (LFO > Harmonie > Raster-Anordnung):\n"
      "Quarten: ein Feld nach rechts = +1 Halbton, eine Reihe hoch = +5 (eine Quarte).\n"
      "Terzen: rechts +1, hoch +4 (eine große Terz).\n"
      "Ganztöne: rechts +2, hoch +5.\n"
      "In Tonart: nur die Töne der gemerkten Tonart; rechts = der nächste Skalenton, hoch = drei Skalenstufen.\n\n"
      "Formen in Quarten: Dur-Dreiklang = Grundton, 4 Felder rechts und 2 rechts + 1 hoch. Moll-Dreiklang = Grundton, 3 rechts, "
      "2 rechts + 1 hoch. Quinte = 2 rechts + 1 hoch. Oktave = 2 rechts + 2 hoch. Eine Form zwei Felder nach rechts spielt sie "
      "einen Ganzton höher.\n\n"
      "Felder: hell = Tonart-Ton, graphit = anderer Ton, fett mit Oktave = Grundton der Tonart, violetter Balken = Akkordton, "
      "roter Balken = gehalten. Weiter oben im Feld anschlagen = lauter. OKTAVE - / + verschiebt das Raster: Das Feld unten links ist "
      "das C dieser Oktave (In Tonart: der erste Tonart-Ton ab dort).\n\n"
      "Das Raster spielt den Edit-Part. Ist für ihn MPE an, hat jede Berührung ihren eigenen Kanal: seitlich schieben = Pitchbend "
      "(ein Feld = ein Schritt der Anordnung, im Bend-Bereich des Parts), hoch / runter = Slide (CC 74), Touch- oder Stiftdruck = "
      "Druck. Ohne MPE laufen die Noten durch Skala, Akkord und Folgen, und Schieben bewirkt nichts.\n\n"
      "Die ganze Anleitung mit Bildern: Harmonie in der Dokumentation." },
    { "Presets and library", "Presets und Bibliothek",
      "The library sorts the presets by category (Bass, Keys, Pads, Drums, Plucks, FX ...). Click a tag to filter, double-click a preset to load it into the edit part.\n\n"
      "Search: every word must match - name, category, tags like MPE or Fingerboard, engine, author, pack, favourite (e.g. \"bass mpe\"). The chips MPE, Fingerboard and Morph show presets that carry all selected ones.\n\n"
      "Your own presets are kept in your user folder and never overwritten by updates.",
      "Die Bibliothek sortiert die Presets nach Kategorien (Bass, Keys, Pads, Drums, Plucks, FX ...). Ein Tag filtert, Doppelklick lädt ein Preset in den Edit-Part.\n\n"
      "Suchen: Jedes Wort muss passen - Name, Kategorie, Tags wie MPE oder Fingerboard, Engine, Autor, Paket, Favorit (z. B. \"fläche mpe\"). Die Chips MPE, Fingerboard und Morph zeigen Presets, die alle gewählten tragen.\n\n"
      "Eigene Presets liegen in deinem Benutzerordner und werden von Updates nie überschrieben." },
    { "Settings and night skin", "Einstellungen und Nachtmodus",
      "Settings are saved for you on this computer - not in projects or presets - and apply to every Ohpe you open.\n\n"
      "Display > Skin: Light, Dark (low glare for night sessions) or Auto (follows your system). Brightness dims the display.\n\n"
      "General > Help overlay names every area of the instrument; F1 switches it on and off.",
      "Einstellungen werden für dich auf diesem Computer gespeichert - nicht in Projekten oder Presets - und gelten für jedes geöffnete Ohpe.\n\n"
      "Anzeige > Erscheinung: Hell, Dunkel (blendarm für Nachtsessions) oder Auto (folgt dem System). Helligkeit dimmt das Display.\n\n"
      "Allgemein > Hilfe-Overlay benennt jeden Bereich des Instruments; F1 schaltet es ein und aus." },
    { "Grid controller", "Grid-Controller", // p46
      "Grid controller profile 1: an 8 x 8 pad controller with a four-line display. On by default: plug it in and Ohpe finds its "
      "user port (input and output), switches it to its User mode, shows connecting ... / connected on its display, then the Ohpe "
      "display; plugged in later, it connects within a second. Settings > Controller: profile, automatic ports or a menu for input "
      "and output, the connection status, touch strip and master encoder, and the virtual controller (click pads and buttons, "
      "mouse wheel over the display turns an encoder).\n\n"
      "In a plug-in: turn the controller's user port off for tracks and remote in the DAW; Ohpe "
      "opens the port itself. One instance holds the controller: the window "
      "opened or clicked last, or Take over here. Port in use: free it in the DAW, Ohpe tries again every second.\n\n"
      "Pads (Keys mode) = the on-screen grid keyboard: the same layout (Scales steps through Fourths, Thirds, Whole Tones, In Key), "
      "the same note for the same column and row, the same key (Shift + Scales: Scale Lock). Root amber, key notes yellow, chord tones "
      "green dim, held pads green. Octave - / + moves pads and screen grid together. Velocity and pressure play the edit part.\n\n"
      "Second button row: parts 1-4, pad modes Keys / Pads / Drum / Steps. First row: the mode keys. Encoders 1-8: the shown page and "
      "the next; arrows: page and preset; Play, Record, Stop, Metronome, Tap, Undo as their names say (Shift: tape play, arm, to start, "
      "count-in, redo). Touch strip: bend or mod.",
      "Grid-Controller (Profil 1): ein 8 x 8-Pad-Controller mit vierzeiligem Display. Ab Werk an: anstecken, Ohpe findet seinen "
      "User-Port (Eingang und Ausgang), schaltet ihn in den User-Modus, zeigt verbindet ... / verbunden auf seinem Display, dann das "
      "Ohpe-Display; später angesteckt, verbindet er sich innerhalb einer Sekunde. Einstellungen > Controller: Profil, automatische "
      "Ports oder je ein Menü für Eingang und Ausgang, der Verbindungsstatus, Touch-Strip und Master-Regler und der virtuelle "
      "Controller (Pads und Tasten anklicken, Mausrad über dem Display dreht einen Regler).\n\n"
      "Im Plug-in: den User-Port des Controllers in der DAW für Spuren und Fernsteuerung "
      "ausschalten; Ohpe öffnet den Port selbst. Eine Instanz hält den "
      "Controller: das zuletzt geöffnete oder angeklickte Fenster, oder Hierher holen. Port belegt: in der DAW freigeben, Ohpe "
      "versucht es jede Sekunde erneut.\n\n"
      "Pads (Modus Tasten) = die Raster-Tastatur des Bildschirms: dieselbe Anordnung (Scales schaltet Quarten, Terzen, Ganztöne, "
      "In Tonart weiter), derselbe Ton für dieselbe Spalte und Reihe, dieselbe Tonart (Shift + Scales: Scale Lock). Grundton bernstein, "
      "Tonart-Töne gelb, Akkordtöne grün gedimmt, gehaltene Pads grün. Oktave - / + verschiebt Pads und Bildschirm-Raster gemeinsam. "
      "Anschlag und Druck spielen den Edit-Part.\n\n"
      "Zweite Tastenreihe: Parts 1-4, Pad-Modi Tasten / Pads / Drum / Steps. Erste Reihe: die Modus-Tasten. Regler 1-8: die gezeigte "
      "Seite und die nächste; Pfeile: Seite und Preset; Play, Record, Stop, Metronome, Tap, Undo wie benannt (Shift: Band-Start, "
      "scharf schalten, zum Anfang, Vorzähler, Wiederholen). Touch-Strip: Bend oder Mod." },
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

juce::Image SettingsView::appIcon()
{
    // p43: the app icon (resources/icon), 128 px - the About header and the standalone window on Linux
    return juce::ImageCache::getFromMemory (BinaryData::ohpeicon128_png, BinaryData::ohpeicon128_pngSize);
}

juce::String SettingsView::aboutLine() { return u8 ("Beat tactics \xe2\x80\x94 designed by groovex"); }

// ---------------------------------------------------------------- view
SettingsView::SettingsView (OhpeProcessor& p, Updater& u, UserSettingsStore& s) : proc (p), updater (u), store (s)
{
    updater.addChangeListener (this);
    store.addChangeListener (this);
    proc.addChangeListener (this);
    setWantsKeyboardFocus (true);
    startTimer (100); // p50: fingerboard status on the MIDI page (p53: the virtual controller on the Controller page)
}

SettingsView::~SettingsView()
{
    stopTimer();
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
        case Controller: return tr ("Controller"); // p53
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
    // p53: a disabled chip is clearly off - outlined, no fill, dim text (also when it is the chosen one)
    if (! enabled)
    {
        g.setColour (colour::displayGrid.brighter (0.15f));
        g.drawRoundedRectangle (r.reduced (u * 0.05f), u * 0.6f, juce::jmax (1.0f, u * 0.1f));
        if (on)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillRoundedRectangle (r.getX() + u * 0.6f, r.getBottom() - u * 0.55f, r.getWidth() - u * 1.2f, u * 0.25f, u * 0.12f);
        }
        g.setColour (colour::displayText.withAlpha (0.32f));
        g.setFont (labelFont (juce::jmin (r.getHeight() * 0.52f, u * 1.4f)));
        g.drawFittedText (text, r.reduced (u * 0.4f, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        return;
    }
    g.setColour (on ? accent : colour::displayGrid.brighter (0.15f));
    g.fillRoundedRectangle (r, u * 0.6f);
    g.setColour (on ? colour::display : colour::displayText);
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
        case Controller: paintController (g, a); break; // p53
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
    {
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
    paintFingerboard (g, { a.getX(), y, a.getWidth(), u * 2.0f + row }, xCh); // p50
    y += u * 2.0f + row;
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
            // p53: as many as fit above the audio settings key; the Grid-Controller's own ports are its (Controller page)
            const int fit = juce::jmax (1, (int) ((a.getBottom() - yy - h) / row));
            const juce::String ctlKey = gridCtl != nullptr && gridCtl->connected() ? juce::String (gridctl::deviceKey (gridCtl->inputPort().toStdString())) : juce::String();
            for (int i = 0; i < devices.size() && i < fit; ++i)
            {
                const auto& d = devices.getReference (i);
                const bool on = dm->isMidiInputDeviceEnabled (d.identifier);
                if (ctlKey.isNotEmpty() && juce::String (gridctl::deviceKey (d.name.toStdString())) == ctlKey)
                {
                    label (g, d.name, { x, yy, half - u * 14.0f, h });
                    chip (g, { x + half - u * 13.0f, yy, u * 13.0f, h }, i18n::upper (tr ("Controller")), true, colour::ultraviolet,
                          [this] { showPage (Controller); });
                    yy += row;
                    continue;
                }
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
                              juce::Rectangle<float> (x, yy, half, u * 3.6f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
            // p53: the Grid-Controller has its own page (Settings > Controller)
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
        yy += row + u * 1.0f;
        // p53: the Grid-Controller: a pointer to its own page
        legend (g, tr ("Controller"), { x, yy, half, u * 1.6f });
        yy += u * 2.0f;
        const juce::String st = gridCtl != nullptr ? gridCtl->status() : tr ("Off");
        label (g, st, { x, yy, half - u * 14.0f, h }, gridCtl == nullptr || ! gridCtl->connected());
        chip (g, { x + half - u * 13.0f, yy, u * 13.0f, h }, i18n::upper (tr ("Controller")), false,
              colour::displayText, [this] { showPage (Controller); });
    }
}

// ---------------------------------------------------------------- p50: Fingerboard mode
void SettingsView::paintFingerboard (juce::Graphics& g, juce::Rectangle<float> a, float xParts)
{
    const float u = unit(), h = u * 3.0f;
    legend (g, tr ("Fingerboard mode") + juce::String::fromUTF8 ("  \xc2\xb7  ") + "MPE HD (14/21-bit)", { a.getX(), a.getY(), a.getWidth(), u * 1.6f });
    const float y = a.getY() + u * 2.0f;
    label (g, tr ("One tap"), { a.getX(), y, xParts - a.getX() - u, h });
    int first = -1;
    const float cw = u * 8.4f;
    for (int p = 0; p < Core::numParts; ++p)
    {
        const juce::String pre = "p" + juce::String (p + 1) + ".";
        const bool on = plain (pre + "mpe") >= 0.5f && std::lround (plain (pre + "mpe.onset")) != 0 && std::lround (plain (pre + "mpe.bend")) == 96;
        if (on && first < 0) first = p;
        // one tap: MPE on, bend range 96, level from pressure, HD auto - again: back to velocity and 48
        chip (g, { xParts + (cw + u * 0.6f) * (float) p, y, cw, h }, i18n::upper (tr ("Part")) + " " + juce::String (p + 1), on, colour::sunYellow,
              [this, pre, on] {
                  if (on)
                  {
                      setPlain (pre + "mpe.onset", 0.0f);
                      setPlain (pre + "mpe.bend", 48.0f);
                      return;
                  }
                  setPlain (pre + "mpe", 1.0f);
                  setPlain (pre + "mpe.bend", 96.0f);
                  setPlain (pre + "mpe.onset", 1.0f);
                  setPlain (pre + "mpe.hd", 1.0f);
              });
    }
    const float xs = xParts + (cw + u * 0.6f) * (float) Core::numParts + u * 0.6f;
    const juce::Rectangle<float> status { xs, y, a.getRight() - xs, h };
    if (first < 0) { label (g, tr ("Tap a part for a fingerboard controller set to MPE HD"), status, true); return; }
    const auto& m = proc.core().mpeInput (first);
    const int bits = m.statusBits();
    juce::String text = tr ("Part") + " " + juce::String (first + 1) + ": ";
    if ((bits & MpeInput::StatusHdData) != 0) text << tr ("HD data received");
    else text << tr ("waiting for HD data");
    if ((bits & MpeInput::StatusCutoffs) != 0)
    {
        auto hz = [&m] (int d) { return juce::String (juce::roundToInt (m.receivedCutoff (d) > 0.0f ? m.receivedCutoff (d) : MpeInput::defaultCutoffHz[d])); };
        text << juce::String::fromUTF8 (" \xc2\xb7 ") << tr ("smoothing") << " X " << hz (0) << " / Y " << hz (1) << " / Z " << hz (2) << " Hz";
    }
    if (m.receivedBendRange() >= 0) text << juce::String::fromUTF8 (" \xc2\xb7 ") << tr ("bend") << " " << juce::String (m.receivedBendRange()) << " " << tr ("st");
    label (g, text, status, (bits & MpeInput::StatusHdData) == 0);
}

// ---------------------------------------------------------------- p46: Grid-Controller (Profil 1) - p53: its own page
void SettingsView::dropdown (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, std::function<void()> action, bool enabled)
{
    const float u = unit();
    g.setColour (colour::displayGrid.brighter (enabled ? 0.15f : 0.05f));
    g.fillRoundedRectangle (r, u * 0.6f);
    g.setColour (colour::displayText.withAlpha (enabled ? 1.0f : 0.4f));
    g.setFont (labelFont (u * 1.35f));
    const auto textArea = r.reduced (u * 0.9f, 0).withTrimmedRight (u * 2.2f);
    g.drawFittedText (text, textArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    // the arrow of a menu
    juce::Path tri;
    const float cx = r.getRight() - u * 1.6f, cy = r.getCentreY(), w = u * 0.55f;
    tri.addTriangle (cx - w, cy - w * 0.5f, cx + w, cy - w * 0.5f, cx, cy + w * 0.6f);
    g.fillPath (tri);
    if (enabled && action) hits.push_back ({ r, text, std::move (action) });
}

void SettingsView::chooseControllerPort (bool input, const juce::String& name)
{
    const auto chosen = name.toStdString();
    const auto cur = store.get();
    // the ports in use now become the fixed choice (automatic off); the other side keeps what automatic found
    const auto otherNow = gridCtl != nullptr ? (input ? gridCtl->outputPort() : gridCtl->inputPort()).toStdString()
                                             : (input ? cur.controllerOut : cur.controllerIn);
    changeSettings ([chosen, input, otherNow] (settings::UserSettings& s) {
        (input ? s.controllerIn : s.controllerOut) = chosen;
        if (s.controllerAuto) (input ? s.controllerOut : s.controllerIn) = otherNow;
        s.controllerAuto = false;
        s.controllerChosen = true;
    });
}

void SettingsView::portMenu (bool input, juce::Rectangle<float> near)
{
    const auto names = input ? GridController::inputNames() : GridController::outputNames();
    const auto& us = store.get();
    const juce::String cur = gridCtl != nullptr ? (input ? gridCtl->inputPort() : gridCtl->outputPort()) : juce::String (input ? us.controllerIn : us.controllerOut);
    juce::PopupMenu m;
    m.addItem (1, tr ("Choose ports automatically"), true, us.controllerAuto);
    m.addSeparator();
    if (names.isEmpty()) m.addItem (2, input ? tr ("No MIDI input found") : tr ("No MIDI output found"), false, false);
    for (int i = 0; i < names.size(); ++i) m.addItem (10 + i, names[i], true, ! us.controllerAuto && names[i] == cur);
    m.addSeparator();
    m.addItem (3, "-", true, ! us.controllerAuto && cur.isEmpty());
    juce::Component::SafePointer<SettingsView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (near.toNearestInt())),
                     [safe, names, input] (int r) {
                         if (safe == nullptr || r == 0) return;
                         if (r == 1)
                             safe->changeSettings ([] (settings::UserSettings& s) { s.controllerAuto = true; s.controllerChosen = true; });
                         else if (r == 3) safe->chooseControllerPort (input, {});
                         else if (r >= 10 && r - 10 < names.size()) safe->chooseControllerPort (input, names[r - 10]);
                     });
}

void SettingsView::paintController (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit(), h = u * 3.0f, row = u * 3.6f, gap = u * 0.6f;
    const auto& us = store.get();
    // left column: setup; right column: the virtual controller, as large as the page allows
    const float leftW = juce::jmin (u * 40.0f, a.getWidth() * 0.44f);
    const float x = a.getX();
    float y = a.getY();
    paintControllerPreview (g, { x + leftW + u * 3.0f, a.getY(), a.getWidth() - leftW - u * 3.0f, a.getHeight() });

    legend (g, tr ("Controller"), { x, y, leftW, u * 1.6f });
    y += u * 2.0f;
    if (gridCtl == nullptr)
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.3f));
        g.drawFittedText (tr ("Controllers are set up in the standalone app. In a plug-in the host program sends their MIDI to Ohpe."),
                          juce::Rectangle<float> (x, y, leftW, u * 6.0f).toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
        return;
    }
    const bool plugin = ! standalone;
    const bool on = us.controllerProfile != settings::ControllerOff;
    auto set = [this] (std::function<void (settings::UserSettings&)> fn) {
        changeSettings ([fn] (settings::UserSettings& s) { fn (s); s.controllerChosen = true; });
    };
    const float wOff = u * 7.0f;
    chip (g, { x, y, wOff, h }, i18n::upper (tr ("Off")), ! on, colour::sunYellow,
          [set] { set ([] (settings::UserSettings& s) { s.controllerProfile = settings::ControllerOff; }); });
    chip (g, { x + wOff + gap, y, leftW - wOff - gap, h }, i18n::upper (tr ("Grid controller profile 1")), on, colour::sunYellow,
          [set] { set ([] (settings::UserSettings& s) { s.controllerProfile = settings::ControllerGrid1; }); });
    y += row + u * 0.6f;

    // ports: automatic, then a menu each with the full port names
    legend (g, tr ("Ports"), { x, y, leftW, u * 1.6f });
    y += u * 2.0f;
    chip (g, { x, y, leftW, h }, i18n::upper (tr ("Choose ports automatically")), us.controllerAuto, colour::sunYellow,
          [set, autoOn = us.controllerAuto] { set ([autoOn] (settings::UserSettings& s) { s.controllerAuto = ! autoOn; }); }, on);
    y += row;
    const float wLabel = u * 9.0f;
    for (const bool input : { true, false })
    {
        label (g, input ? tr ("Input") : tr ("Output"), { x, y, wLabel - gap, h }, ! on);
        const juce::String cur = gridCtl->inputPort().isNotEmpty() || gridCtl->outputPort().isNotEmpty() || us.controllerAuto
                                     ? (input ? gridCtl->inputPort() : gridCtl->outputPort())
                                     : juce::String (input ? us.controllerIn : us.controllerOut);
        juce::String shown = cur.isEmpty() ? (us.controllerAuto ? tr ("not found") : tr ("choose ...")) : cur;
        if (us.controllerAuto && cur.isNotEmpty()) shown << "  (" << tr ("auto") << ")";
        const juce::Rectangle<float> r { x + wLabel, y, leftW - wLabel, h };
        dropdown (g, r, shown, [this, input, r] { portMenu (input, r); }, on);
        y += row;
    }
    // the connection: connected (generation) / checking / not found / port in use / waiting
    {
        const auto st = gridCtl->state();
        const bool ok = st == GridController::State::Connected;
        const bool bad = st == GridController::State::PortBusy || st == GridController::State::NotFound || st == GridController::State::ChoosePorts;
        const juce::Colour dot = ! on ? colour::displayDim : ok ? colour::playGreen : bad ? colour::signalRed : colour::sunYellow;
        g.setColour (dot);
        g.fillEllipse (x + u * 0.3f, y + h * 0.5f - u * 0.5f, u, u);
        label (g, on ? gridCtl->statusLine() : tr ("Off"), { x + u * 2.0f, y, leftW - u * 2.0f, h }, ! ok);
        y += row + u * 0.6f;
    }

    // how the controller plays: touch strip, master encoder
    legend (g, tr ("Controls"), { x, y, leftW, u * 1.6f });
    y += u * 2.0f;
    const float wPair = (leftW - wLabel - gap) * 0.5f;
    label (g, tr ("Touch strip"), { x, y, wLabel + u * 3.0f, h }, ! on);
    chip (g, { x + wLabel, y, wPair, h }, i18n::upper (tr ("Bend")), us.controllerStrip == 0, colour::sunYellow,
          [set] { set ([] (settings::UserSettings& s) { s.controllerStrip = 0; }); }, on);
    chip (g, { x + wLabel + wPair + gap, y, wPair, h }, i18n::upper (tr ("Mod")), us.controllerStrip == 1, colour::sunYellow,
          [set] { set ([] (settings::UserSettings& s) { s.controllerStrip = 1; }); }, on);
    y += row;
    label (g, tr ("Master"), { x, y, wLabel, h }, ! on);
    chip (g, { x + wLabel, y, wPair, h }, i18n::upper (tr ("Volume")), us.controllerMaster == 0, colour::sunYellow,
          [set] { set ([] (settings::UserSettings& s) { s.controllerMaster = 0; }); }, on);
    chip (g, { x + wLabel + wPair + gap, y, wPair, h }, i18n::upper (tr ("Tempo")), us.controllerMaster == 1, colour::sunYellow,
          [set] { set ([] (settings::UserSettings& s) { s.controllerMaster = 1; }); }, on);
    y += row;

    if (! plugin) return;
    // p49: which instance holds the controller, take it over
    y += u * 0.6f;
    const bool elsewhere = on && gridCtl->ownedElsewhere();
    const float wTake = u * 13.0f;
    juce::String where;
    if (on) where = elsewhere ? tr ("Active in another instance") : tr ("Active in this instance");
    label (g, where, { x, y, leftW - (elsewhere ? wTake + gap : 0.0f), h }, ! on);
    if (elsewhere)
        chip (g, { x + leftW - wTake, y, wTake, h }, i18n::upper (tr ("Take over here")), false, colour::ultraviolet,
              [this] { if (gridCtl != nullptr) gridCtl->claim(); repaint(); });
}

void SettingsView::paintControllerPreview (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float u = unit();
    legend (g, tr ("Virtual controller"), { a.getX(), a.getY(), a.getWidth(), u * 1.6f });
    padGrid = lcdArea = {};
    if (gridCtl == nullptr) return;
    auto body = a.withTrimmedTop (u * 2.0f);
    // the device from top to bottom: the four display lines, the two button rows under it, the 8 x 8 pads
    const float lineH = juce::jmin (u * 1.6f, body.getWidth() / 68.0f * 1.75f);
    const auto lines = gridCtl->displayLines();
    const auto panel = body.removeFromTop (lineH * 4.0f + u * 1.2f);
    lcdArea = panel;
    g.setColour (juce::Colour (0xff0b0d0c));
    g.fillRoundedRectangle (panel, u * 0.5f);
    g.setColour (colour::displayGrid);
    g.drawRoundedRectangle (panel, u * 0.5f, 1.0f);
    g.setFont (monoFont (lineH * 0.82f));
    for (int l = 0; l < gridctl::numLines; ++l)
    {
        g.setColour (juce::Colour (0xfff2b54a)); // amber text, like the device's character display
        g.drawFittedText (juce::String (lines[(size_t) l]),
                          panel.reduced (u * 0.6f, u * 0.6f).withHeight (lineH).translated (0.0f, lineH * (float) l).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.6f);
    }
    body.removeFromTop (u * 0.6f);
    // button rows: upper (the eight modes), lower (parts 1-4, pad modes) - lit as on the device, clickable
    auto buttonRow = [&] (int firstCc) {
        const auto r = body.removeFromTop (u * 2.2f);
        const float w = r.getWidth() / 8.0f;
        for (int k = 0; k < 8; ++k)
        {
            const int cc = firstCc + k;
            const auto c = juce::Rectangle<float> (r.getX() + w * (float) k, r.getY(), w, r.getHeight()).reduced (u * 0.2f);
            const int code = gridCtl->buttonCode (cc);
            // colour codes: red 1-6, amber 7-12, yellow 13-18, green 19-24 (dim / full / blink)
            juce::Colour col = colour::displayGrid.brighter (0.1f);
            if (code > 0)
            {
                const juce::Colour hues[] = { colour::signalRed, juce::Colour (0xfff08a24), colour::sunYellow, colour::playGreen };
                const int hue = juce::jlimit (0, 3, (code - 1) / 6);
                const bool full = ((code - 1) % 6) >= 3;
                col = hues[hue].withAlpha (full ? 1.0f : 0.4f);
            }
            g.setColour (col);
            g.fillRoundedRectangle (c, u * 0.3f);
            hits.push_back ({ c, "BTN " + juce::String (cc), [this, cc] {
                                 if (gridCtl == nullptr) return;
                                 gridCtl->receive (juce::MidiMessage::controllerEvent (1, cc, 127));
                                 gridCtl->receive (juce::MidiMessage::controllerEvent (1, cc, 0));
                                 gridCtl->update();
                             } });
        }
        body.removeFromTop (u * 0.3f);
    };
    buttonRow (gridctl::cc::upperRow);
    buttonRow (gridctl::cc::lowerRow);
    body.removeFromTop (u * 0.5f);
    // the 8 x 8 pads, square, centred in what is left; pressed with the mouse they play like the device's pads
    const float side = juce::jmin (body.getWidth(), body.getHeight() - u * 1.6f);
    const float cell = side / 8.0f;
    padGrid = { body.getCentreX() - side * 0.5f, body.getY(), side, side };
    const auto codes = gridCtl->padCodes();
    for (int pad = 0; pad < gridctl::numPads; ++pad)
    {
        const int col = pad % gridctl::cols, r = pad / gridctl::cols;
        const auto c = juce::Rectangle<float> (padGrid.getX() + cell * (float) col, padGrid.getY() + cell * (float) (7 - r), cell, cell).reduced (cell * 0.07f);
        juce::Colour col2 = colour::displayGrid;
        if (const auto pc = gridctl::padColourOf (codes[(size_t) pad]); pc != gridctl::Colour::Off)
        {
            const int k = (int) pc - 1; // red dim, red, amber dim, amber, yellow dim, yellow, green dim, green
            const juce::Colour hues[] = { colour::signalRed, juce::Colour (0xfff08a24), colour::sunYellow, colour::playGreenLit };
            col2 = hues[juce::jlimit (0, 3, k / 2)].withAlpha (k % 2 == 1 ? 1.0f : 0.42f);
        }
        g.setColour (col2);
        g.fillRoundedRectangle (c, cell * 0.12f);
    }
    // the last message in / out
    g.setColour (colour::displayDim);
    g.setFont (monoFont (u * 1.05f));
    const auto log = gridCtl->log();
    if (! log.isEmpty())
        g.drawText (log[log.size() - 1], juce::Rectangle<float> (a.getX(), padGrid.getBottom() + u * 0.3f, a.getWidth(), u * 1.3f),
                    juce::Justification::centred, true);
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
    // content packs (.ohpepack): install, and the pack manager page
    legend (g, tr ("Content packs"), { a.getX(), y, a.getWidth(), u * 1.6f });
    y += u * 2.0f;
    {
        const auto packs = updater.installedPacks();
        label (g, juce::String ((int) packs.size()) + " " + tr ("installed"), { a.getX(), y, a.getWidth() * 0.4f, h });
        chip (g, { a.getRight() - cw * 2.0f - u * 0.6f, y, cw, h }, i18n::upper (tr ("Install pack") + juce::String::fromUTF8 (" \xe2\x80\xa6")), true,
              colour::sunYellow, [this] { if (onInstallPack) onInstallPack(); }, ! busy && ! updater.busy());
        chip (g, { a.getRight() - cw, y, cw, h }, i18n::upper (tr ("Manage packs")), false, colour::displayText, [this] { if (onManagePacks) onManagePacks(); });
    }
    y += row + u * 0.6f;
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
    // p43: the app icon, small, in front of the wordmark
    const float iconSize = u * 5.6f, wx = a.getX() + iconSize + u * 1.6f;
    if (const auto icon = appIcon(); icon.isValid())
        g.drawImage (icon, juce::Rectangle<float> (a.getX(), y + u * 0.45f, iconSize, iconSize), juce::RectanglePlacement::centred);
    {   // p47: the wordmark with its dot set like a full stop (bottom on the baseline), centred on the icon
        const auto wordFont = brandFont (u * 6.0f);
        const float baseline = brand::baselineCentredOn (wordFont, y + u * 3.25f);
        brand::drawWordmark (g, wordFont, wx, baseline, colour::displayText, colour::signalRed);
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
    // p53: a virtual pad pressed with the mouse plays like the device's pad (velocity from the height in the pad)
    if (current == Controller && gridCtl != nullptr && padGrid.contains (e.position))
    {
        const float cell = padGrid.getWidth() / 8.0f;
        const int col = juce::jlimit (0, 7, (int) ((e.position.x - padGrid.getX()) / cell));
        const int r = juce::jlimit (0, 7, 7 - (int) ((e.position.y - padGrid.getY()) / cell));
        pressedPad = r * gridctl::cols + col;
        gridCtl->receive (juce::MidiMessage::noteOn (1, gridctl::padNote (col, r), (juce::uint8) 100));
        gridCtl->update();
        repaint();
        return;
    }
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            auto action = it->action;
            if (action) action();
            repaint();
            return;
        }
}

void SettingsView::mouseUp (const juce::MouseEvent&)
{
    if (pressedPad < 0) return;
    if (gridCtl != nullptr)
    {
        gridCtl->receive (juce::MidiMessage::noteOff (1, gridctl::firstPadNote + pressedPad));
        gridCtl->update();
    }
    pressedPad = -1;
    repaint();
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

std::vector<juce::Rectangle<float>> SettingsView::clickableAreas()
{
    paintOffscreen();
    std::vector<juce::Rectangle<float>> a;
    for (const auto& h : hits) a.push_back (h.area);
    return a;
}

void SettingsView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // p53: the wheel over the virtual display turns the encoder of that eighth (as the device's relative encoders)
    if (current == Controller && gridCtl != nullptr && lcdArea.contains (e.position))
    {
        const int enc = juce::jlimit (0, 7, (int) ((e.position.x - lcdArea.getX()) / (lcdArea.getWidth() / 8.0f)));
        const int d = w.deltaY > 0.0f ? 1 : w.deltaY < 0.0f ? 127 : 0;
        if (d != 0)
        {
            gridCtl->receive (juce::MidiMessage::controllerEvent (1, gridctl::cc::firstEncoder + enc, d));
            gridCtl->update();
            repaint();
        }
        return;
    }
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
float HelpOverlay::tagWidthNeeded (const Item& it, float s)
{
    const auto titleFont = brandFont (12.0f * s);
    const auto textFont = labelFont (11.0f * s);
    return std::ceil (juce::jmax (juce::GlyphArrangement::getStringWidth (titleFont, i18n::upper (it.title)),
                                  juce::GlyphArrangement::getStringWidth (textFont, it.text)))
           + 18.0f * s;
}

juce::Rectangle<float> HelpOverlay::tagBounds (const Item& it, juce::Rectangle<float> overlay)
{
    const float s = overlay.getWidth() / 1200.0f;
    const juce::Rectangle<float> r { overlay.getX() + it.design.getX() * s, overlay.getY() + it.design.getY() * s, it.design.getWidth() * s,
                                     it.design.getHeight() * s };
    const float h = (it.text.isNotEmpty() ? 34.0f : 20.0f) * s;
    const float w = juce::jmin (tagWidthNeeded (it, s), overlay.getWidth() - 8.0f * s);
    if (w <= r.getWidth() - 8.0f * s)
        return { r.getX() + 4.0f * s, r.getY() + 4.0f * s, w, h }; // fits inside, top left (as before)
    // a narrow area (e.g. the gear key): the tag sits outside, below the area (above it at the bottom edge), right-aligned
    const float x = juce::jlimit (overlay.getX() + 4.0f * s, overlay.getRight() - 4.0f * s - w, r.getRight() - w);
    float y = r.getBottom() + 4.0f * s;
    if (y + h > overlay.getBottom() - 4.0f * s) y = r.getY() - 4.0f * s - h;
    return { x, juce::jmax (overlay.getY() + 4.0f * s, y), w, h };
}

void HelpOverlay::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 1200.0f;
    for (const auto& it : items)
    {
        const juce::Rectangle<float> r { it.design.getX() * s, it.design.getY() * s, it.design.getWidth() * s, it.design.getHeight() * s };
        g.setColour (colour::sunYellow.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (1.0f * s), radius::panel * s, 1.5f * s);
    }
    for (const auto& it : items) // tags over all frames (a tag outside its area may cross a neighbour's frame)
    {
        // a dark tag: name in yellow, one line of explanation - always in full
        const auto titleFont = brandFont (12.0f * s);
        const auto textFont = labelFont (11.0f * s);
        const bool twoLines = it.text.isNotEmpty();
        const auto tag = tagBounds (it, getLocalBounds().toFloat());
        g.setColour (colour::display.withAlpha (0.9f));
        g.fillRoundedRectangle (tag, radius::inset * s);
        g.setColour (colour::sunYellow);
        g.setFont (titleFont);
        g.drawText (i18n::upper (it.title), tag.reduced (8.0f * s, 3.0f * s).withHeight (14.0f * s), juce::Justification::centredLeft, false);
        if (twoLines)
        {
            g.setColour (colour::displayText);
            g.setFont (textFont);
            g.drawText (it.text, tag.reduced (8.0f * s, 3.0f * s).withTrimmedTop (15.0f * s), juce::Justification::centredLeft, false);
        }
    }
}
} // namespace ohpe::ui
