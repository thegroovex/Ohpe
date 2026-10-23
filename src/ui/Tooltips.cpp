// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Tooltips.h"

#include "plugin/Localisation.h"

#include <algorithm>

namespace ohpe::ui::tips
{
juce::String t (const char* en, const char* de)
{
    return juce::String::fromUTF8 (i18n::current() == i18n::Language::German ? de : en);
}

juce::String learnHint() { return t ("Right-click: MIDI learn", "Rechtsklick: MIDI-Learn"); }

juce::String withLearn (const juce::String& tip) { return tip.isEmpty() ? learnHint() : tip + "\n" + learnHint(); }

juce::String regionTipAt (const std::vector<Region>& regions, juce::Point<float> p)
{
    for (const auto& r : regions)
        if (r.area.contains (p)) return r.tip;
    return {};
}

void markPassive (juce::Component& c) { c.getProperties().set ("ohpeTipPassive", true); }
bool isPassive (const juce::Component& c) { return (bool) c.getProperties().getWithDefault ("ohpeTipPassive", false); }

// ---------------------------------------------------------------- the plate's controls
const std::vector<Control>& controls()
{
    // the literals are UTF-8 (read with juce::String::fromUTF8)
    static const std::vector<Control> list = {
        // header
        { "part.selector", "Parts", "Parts",
          "Four parts, each with its own sound - click one to edit it (yellow = edit part)",
          "Vier Parts mit je eigenem Sound - einen anklicken, um ihn zu bearbeiten (gelb = Edit-Part)" },
        { "preset.strip", "Preset", "Preset",
          "The edit part's preset - the arrows step through the library, a click on the name opens it",
          "Preset des Edit-Parts - die Pfeile blättern durch die Bibliothek, ein Klick auf den Namen öffnet sie" },
        { "key.library", "Library", "Bibliothek",
          "Preset library: browse, search, audition and load sounds",
          "Preset-Bibliothek: Sounds durchsuchen, anhören und laden" },
        { "key.pro", "Pro view", "Profiansicht",
          "Pro view: every parameter of the mode at once, with pictures you edit directly",
          "Profiansicht: alle Parameter des Modus auf einmal, mit Bildern zum direkten Bearbeiten" },
        { "key.breed", "Breeding", "Zucht",
          "Breeding: cross two sounds, hear six children, keep the best - it learns what you like",
          "Zucht: zwei Sounds kreuzen, sechs Kinder anhören, die besten behalten - lernt, was dir gefällt" },
        { "key.random", "Random", "Zufall",
          "Random: new values for the sound of the edit part's engine (in the breeding view: a new parent B)",
          "Zufall: neue Werte für den Sound der Engine des Edit-Parts (in der Zucht: ein neues Elternteil B)" },
        { "key.save", "Save", "Speichern",
          "Save the edit part as a user preset",
          "Den Edit-Part als User-Preset speichern" },
        { "key.settings", "Settings", "Einstellungen",
          "Settings: language, help and help search, skin, display, MIDI, controller, updates, about",
          "Einstellungen: Sprache, Hilfe und Hilfesuche, Erscheinung, Display, MIDI, Controller, Updates, Info" },
        { "version.badge", "Version", "Version",
          "Version and updates - a click opens the About page",
          "Version und Updates - ein Klick öffnet die Info-Seite" },
        { "cpu.meter", "CPU meter", "CPU-Anzeige",
          "Share of the audio block's time the processing takes",
          "Anteil der Zeit eines Audioblocks, den die Berechnung braucht" },
        // columns, display, encoders
        { "engine.column", "Engines", "Engines",
          "The sound engine of the edit part - the light bar shows which parts play it",
          "Die Klangerzeugung (Engine) des Edit-Parts - der Leuchtbalken zeigt, welche Parts sie spielen" },
        { "mode.column", "Modes", "Modi",
          "What the display and the four encoders edit: Synth, Envelope, LFO, Mixer, Effects, Sequencer, Tape, Drum",
          "Was Display und die vier Encoder bearbeiten: Synth, Hüllkurve, LFO, Mixer, Effekte, Sequencer, Tape, Drum" },
        { "display", "Display", "Display",
          "Pages and values: drag a bar, click to jump, double-click resets; arrows change the page",
          "Seiten und Werte: Balken ziehen, Klick springt, Doppelklick setzt zurück; Pfeile wechseln die Seite" },
        { "encoders", "Encoders", "Encoder",
          "Four encoders - the four colours are the four values on the display",
          "Vier Encoder - die vier Farben sind die vier Werte auf dem Display" },
        { "life.bar", "Life", "Life",
          "Life of the edit part: 0 = static; more = held notes keep evolving (drift, breathing filter, grain scatter)",
          "Life des Edit-Parts: 0 = statisch; mehr = gehaltene Töne leben weiter (Drift, atmendes Filter, Grain-Streuung)" },
        // transport
        { "key.play", "Play", "Start",
          "Play / stop the sequencer (and the tape when linked)",
          "Sequencer starten / stoppen (und das Tape, wenn verknüpft)" },
        { "key.rec", "Record", "Aufnahme",
          "Record on the tape - arms the selected track when none is armed",
          "Auf das Tape aufnehmen - schärft die gewählte Spur, wenn keine scharf ist" },
        { "key.stop", "Stop", "Stopp",
          "Stop - press again to go to the start",
          "Stopp - nochmals drücken springt an den Anfang" },
        { "key.tostart", "To start", "Zum Anfang",
          "To the start of the tape",
          "An den Anfang des Tapes" },
        { "key.loop", "Loop", "Loop",
          "Loop between loop in and out",
          "Loop zwischen Loop-Anfang und -Ende" },
        { "tape.readout", "Tape position", "Tape-Position",
          "Tape position (minutes:seconds) and bar.beat; REC = recording, PLAY = tape rolling",
          "Tape-Position (Minuten:Sekunden) und Takt.Schlag; REC = Aufnahme, PLAY = Tape läuft" },
        { "tempo.panel", "Tempo", "Tempo",
          "Tempo: drag up / down (Shift: fine), double-click to type, mouse wheel",
          "Tempo: nach oben / unten ziehen (Shift: fein), Doppelklick zum Eintippen, Mausrad" },
        { "click.strip", "Metronome", "Metronom",
          "Metronome and count-in: click on, count-in bars, level",
          "Metronom und Einzähler: Klick an, Einzähltakte, Pegel" },
        // keyboard row
        { "key.octdown", "Octave -", "Oktave -",
          "Keyboard one octave down",
          "Tastatur eine Oktave tiefer" },
        { "key.octup", "Octave +", "Oktave +",
          "Keyboard one octave up",
          "Tastatur eine Oktave höher" },
        { "key.midi", "MIDI", "MIDI",
          "MIDI maps and remote controls: learn, export, import, clear",
          "MIDI-Maps und Fernsteuerung: lernen, exportieren, importieren, löschen" },
        { "key.grid", "Grid keyboard", "Grid-Tastatur",
          "Grid keyboard: 4 x 24 pads with MPE - every chord has the same shape in every key; swaps the piano keys",
          "Grid-Tastatur: 4 x 24 Pads mit MPE - jeder Akkord hat in jeder Tonart dieselbe Form; ersetzt die Klaviertasten" },
        { "keys.readout", "Keyboard range", "Tastaturbereich",
          "Range of the on-screen keyboard and its octave shift; red during MIDI learn",
          "Bereich der Bildschirmtastatur und ihre Oktavlage; rot während MIDI-Learn" },
        { "keyboard", "Keyboard", "Tastatur",
          "Play the edit part - click or drag over the keys",
          "Den Edit-Part spielen - Tasten anklicken oder darüberziehen" },
        { "grid.keyboard", "Grid", "Grid",
          "Every chord has the same shape everywhere. With MPE on: slide sideways = bend, up / down = slide",
          "Jeder Akkord hat überall dieselbe Form. Mit MPE: seitlich gleiten = Bend, auf / ab = Slide" },
        { "wheel.pitch", "Pitch bend", "Pitch Bend",
          "Pitch bend - springs back to the centre",
          "Pitch Bend - federt in die Mitte zurück" },
        { "wheel.mod", "Mod wheel", "Modrad",
          "Mod wheel - a source of the modulation matrix",
          "Modrad - eine Quelle der Modulationsmatrix" },
        { "glide.switch", "Glide mode", "Glide-Modus",
          "Glide mode of the edit part: Auto, Legato or Always",
          "Glide-Modus des Edit-Parts: Auto, Legato oder Immer" },
        { "glide.knob", "Glide time", "Glide-Zeit",
          "Glide time of the edit part (portamento)",
          "Glide-Zeit des Edit-Parts (Portamento)" },
        // display header
        { "header.key", "Key and scale", "Tonart und Skala",
          "Key, scale and Scale Lock of the song",
          "Tonart, Skala und Scale Lock des Songs" },
        { "header.gravity", "Key Gravity", "Tonart-Gravitation",
          "Key Gravity of the edit part: pulls every partial to the key",
          "Tonart-Gravitation des Edit-Parts: zieht jeden Teilton zur Tonart" },
        { "header.3d", "3D badge", "3D-Abzeichen",
          "The edit part's 3D Space - a click opens the 3D view",
          "Der 3D-Raum des Edit-Parts - ein Klick öffnet die 3D-Ansicht" },
        // views and pages a search can open
        { "view.lfo.mod", "Modulation matrix", "Modulationsmatrix",
          "LFO mode, view Mod: the 16 routings of the modulation matrix",
          "LFO-Modus, Ansicht Mod: die 16 Verbindungen der Modulationsmatrix" },
        { "view.lfo.morph", "Morph pad", "Morph-Pad",
          "LFO mode, view Morph: blend four sounds on a pad",
          "LFO-Modus, Ansicht Morph: vier Sounds auf einem Pad überblenden" },
        { "view.lfo.motion", "Motion recorder", "Bewegungsrekorder",
          "LFO mode, view Motion: record control moves as loops",
          "LFO-Modus, Ansicht Bewegung: Reglerbewegungen als Loops aufnehmen" },
        { "view.lfo.harmony", "Harmony", "Harmonie",
          "LFO mode, view Harmony: chords, voicings and the harmony brain",
          "LFO-Modus, Ansicht Harmonie: Akkorde, Voicings und das Harmonie-Gehirn" },
        { "view.lfo.voice", "Voice control", "Stimmsteuerung",
          "LFO mode, view Voice: steer the sound with your voice",
          "LFO-Modus, Ansicht Stimme: den Sound mit der Stimme steuern" },
        { "view.fx.sends", "Sends", "Sends",
          "Effects mode, view Sends: send levels and Key Gravity of the edit part",
          "Effekte-Modus, Ansicht Sends: Send-Pegel und Tonart-Gravitation des Edit-Parts" },
        { "view.fx.rack", "FX rack", "FX-Rack",
          "Effects mode, view Rack: the edit part's effect slots (EQ with Low / High Shelf and Q, delay, reverb ...)",
          "Effekte-Modus, Ansicht Rack: die Effekt-Slots des Edit-Parts (EQ mit Low / High Shelf und Q, Delay, Hall ...)" },
        { "view.fx.master", "Master rack", "Master-Rack",
          "Effects mode, view Master: the effects on the Main output",
          "Effekte-Modus, Ansicht Master: die Effekte auf dem Main-Ausgang" },
        { "view.fx.space", "3D Space", "3D-Raum",
          "Effects mode, view 3D Space: position and motion paths of the edit part",
          "Effekte-Modus, Ansicht 3D-Raum: Position und Bewegungspfade des Edit-Parts" },
        { "view.fx.body", "Body", "Körper",
          "Effects mode, view Body: the resonator body of the edit part",
          "Effekte-Modus, Ansicht Körper: der Resonanzkörper des Edit-Parts" },
        { "view.fx.pads", "Performance pads", "Performance-Pads",
          "Effects mode, view Pads: hold a pad for a performance effect",
          "Effekte-Modus, Ansicht Pads: ein Pad halten für einen Performance-Effekt" },
        { "view.tape.memory", "Tape-Memory", "Tape-Memory",
          "Tape mode, MEMORY key: capture what you just played into a part or onto the tape",
          "Tape-Modus, Taste MEMORY: gerade Gespieltes in einen Part oder auf das Tape übernehmen" },
        // settings pages
        { "settings.general", "Settings: General", "Einstellungen: Allgemein",
          "Language, tooltips, help overlay, tour",
          "Sprache, Tooltips, Hilfe-Overlay, Tour" },
        { "settings.display", "Settings: Display", "Einstellungen: Display",
          "Skin (light / night), display brightness, animations, window size",
          "Erscheinung (hell / Nacht), Display-Helligkeit, Animationen, Fenstergröße" },
        { "settings.midi", "Settings: MIDI", "Einstellungen: MIDI",
          "MIDI channel, MPE and bend range of the parts, MIDI inputs, clock and sync",
          "MIDI-Kanal, MPE und Bend-Bereich der Parts, MIDI-Eingänge, Clock und Sync" },
        { "settings.learn", "Settings: MIDI learn", "Einstellungen: MIDI-Learn",
          "Every MIDI mapping: edit, learn again, clear; import / export map files",
          "Alle MIDI-Zuweisungen: bearbeiten, neu lernen, löschen; Map-Dateien importieren / exportieren" },
        { "settings.controller", "Settings: Controller", "Einstellungen: Controller",
          "Grid controller: ports, touch strip, master, virtual controller",
          "Grid-Controller: Ports, Touch-Strip, Master, virtueller Controller" },
        { "settings.updates", "Settings: Updates", "Einstellungen: Updates",
          "Update checks and content packs",
          "Update-Prüfung und Inhaltspakete" },
        { "settings.help", "Help", "Hilfe",
          "Help pages and the help search",
          "Hilfeseiten und die Hilfesuche" },
        { "settings.about", "About", "Info",
          "Version, licence, credits",
          "Version, Lizenz, Danksagung" },
        { "tour", "Tour", "Tour",
          "The first-start tour again: four steps through the plate",
          "Die Tour vom ersten Start noch einmal: vier Schritte über die Oberfläche" },
    };
    return list;
}

const Control* control (const juce::String& key)
{
    for (const auto& c : controls())
        if (key == c.key) return &c;
    return nullptr;
}

juce::String controlTip (const char* key)
{
    if (const auto* c = control (key)) return t (c->tipEn, c->tipDe);
    return {};
}

juce::String controlName (const char* key)
{
    if (const auto* c = control (key)) return t (c->nameEn, c->nameDe);
    return {};
}

// ---------------------------------------------------------------- search
juce::String fold (const juce::String& text)
{
    juce::String out;
    out.preallocateBytes ((size_t) text.length() + 8);
    bool space = true;
    for (auto p = text.getCharPointer(); ! p.isEmpty();)
    {
        const auto c = juce::CharacterFunctions::toLowerCase (p.getAndAdvance());
        const char* rep = nullptr;
        switch ((int) c)
        {
            case 0xe4: case 0xc4: rep = "ae"; break;
            case 0xf6: case 0xd6: rep = "oe"; break;
            case 0xfc: case 0xdc: rep = "ue"; break;
            case 0xdf: rep = "ss"; break;
            case 0xe9: case 0xe8: rep = "e"; break;
            default: break;
        }
        if (rep != nullptr) { out << rep; space = false; continue; }
        if (juce::CharacterFunctions::isLetterOrDigit (c)) { out << juce::String::charToString (c); space = false; }
        else if (! space) { out << " "; space = true; }
    }
    return out.trimEnd();
}

int editDistance (const juce::String& a, const juce::String& b, int cap)
{
    const int n = a.length(), m = b.length();
    if (std::abs (n - m) > cap) return cap + 1;
    std::vector<std::vector<int>> d ((size_t) n + 1, std::vector<int> ((size_t) m + 1, 0));
    for (int i = 0; i <= n; ++i) d[(size_t) i][0] = i;
    for (int j = 0; j <= m; ++j) d[0][(size_t) j] = j;
    for (int i = 1; i <= n; ++i)
    {
        int rowMin = cap + 1;
        for (int j = 1; j <= m; ++j)
        {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            int v = std::min ({ d[(size_t) i - 1][(size_t) j] + 1, d[(size_t) i][(size_t) j - 1] + 1, d[(size_t) i - 1][(size_t) j - 1] + cost });
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) v = std::min (v, d[(size_t) i - 2][(size_t) j - 2] + 1);
            d[(size_t) i][(size_t) j] = v;
            rowMin = std::min (rowMin, v);
        }
        if (rowMin > cap) return cap + 1;
    }
    return std::min (d[(size_t) n][(size_t) m], cap + 1);
}

namespace
{
float wordScore (const juce::String& w, const juce::String& q)
{
    if (w == q) return 1.0f;
    if (w.startsWith (q)) return q.length() >= 2 ? 0.85f : 0.4f;
    if (q.length() >= 3 && w.contains (q)) return 0.6f;
    const int typos = q.length() >= 8 ? 2 : (q.length() >= 4 ? 1 : 0);
    if (typos > 0)
    {
        if (editDistance (w, q, typos) <= typos) return 0.55f;
        if (w.length() > q.length() && editDistance (w.substring (0, q.length()), q, typos) <= typos) return 0.45f;
    }
    return 0.0f;
}

float foldedScore (const juce::StringArray& words, const juce::String& joined, const juce::StringArray& query)
{
    if (query.isEmpty() || words.isEmpty()) return 0.0f;
    float sum = 0.0f;
    for (const auto& q : query)
    {
        float best = 0.0f;
        for (const auto& w : words) best = std::max (best, wordScore (w, q));
        if (best <= 0.0f && q.length() >= 4 && joined.contains (q)) best = 0.5f; // across a word boundary
        if (best <= 0.0f) return 0.0f;
        sum += best;
    }
    return sum / (float) query.size();
}

juce::StringArray words (const juce::String& folded) { return juce::StringArray::fromTokens (folded, " ", ""); }
} // namespace

float matchScore (const juce::String& text, const juce::String& query)
{
    const auto f = fold (text);
    return foldedScore (words (f), f.removeCharacters (" "), words (fold (query)));
}

std::vector<Result> search (const std::vector<Entry>& index, const juce::String& query, int maxResults)
{
    std::vector<Result> out;
    const auto q = words (fold (query));
    if (q.isEmpty()) return out;
    for (const auto& e : index)
    {
        float best = 0.0f;
        for (const auto* title : { &e.titleEn, &e.titleDe })
        {
            const auto f = fold (*title);
            best = std::max (best, foldedScore (words (f), f.removeCharacters (" "), q));
        }
        for (const auto* text : { &e.textEn, &e.textDe })
        {
            if (best >= 0.6f) break;
            const auto f = fold (*text);
            best = std::max (best, 0.6f * foldedScore (words (f), f.removeCharacters (" "), q));
        }
        if (best <= 0.0f) continue;
        // a help topic or a control a little before a parameter of the same quality
        if (e.kind == Kind::Topic) best += 0.04f;
        else if (e.kind == Kind::Control) best += 0.02f;
        out.push_back ({ e, best });
    }
    std::stable_sort (out.begin(), out.end(), [] (const Result& a, const Result& b) { return a.score > b.score; });
    if ((int) out.size() > maxResults) out.resize ((size_t) juce::jmax (0, maxResults));
    return out;
}

juce::String kindName (Kind k)
{
    switch (k)
    {
        case Kind::Topic: return t ("Help", "Hilfe");
        case Kind::Parameter: return t ("Parameter", "Parameter");
        case Kind::Control: return t ("Control", "Bedienelement");
    }
    return {};
}
} // namespace ohpe::ui::tips
