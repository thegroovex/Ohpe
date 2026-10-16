// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Localisation.h"

#include <atomic>
#include <map>

namespace ohpe::i18n
{
namespace
{
std::atomic<int> currentLang { -1 };

struct Entry
{
    const char* en; ///< UTF-8
    const char* de; ///< UTF-8
};

const std::map<juce::String, juce::String>& table()
{
    // the literals are UTF-8: convert explicitly (juce::String (const char*) would read them as ASCII)
    static const Entry entries[] = {
        // modes
        { "Synth", "Synth" },
        { "Envelope", "Hüllkurve" },
        { "LFO", "LFO" },
        { "Mixer", "Mixer" },
        { "Effects", "Effekte" },
        { "Sequencer", "Sequencer" },
        { "Tape", "Tape" },
        { "Drum", "Drum" },
        { "Engine", "Engine" },
        { "Mode", "Modus" },
        // display / keys
        { "Page", "Seite" },
        { "Channel", "Kanal" },
        { "Coming in a later version", "Kommt in einer späteren Version" },
        { "Voices", "Stimmen" },
        { "Step", "Schritt" },
        { "Play", "Start" },
        { "Stop", "Stopp" },
        { "Octave", "Oktave" },
        { "Main", "Haupt" },
        { "Out", "Ausgang" },
        { "Off", "Aus" },
        { "On", "Ein" },
        { "Part", "Part" },
        { "Source", "Quelle" },
        { "Output", "Ausgang" },
        { "Master", "Master" },
        { "Tap a step to switch it on or off", "Schritt antippen zum Ein- und Ausschalten" },
        { "Tap a channel to select it", "Kanal antippen zum Auswählen" },
        // MIDI
        { "MIDI", "MIDI" },
        { "MIDI Learn", "MIDI-Learn" },
        { "Clear MIDI mapping", "MIDI-Zuweisung löschen" },
        { "Cancel MIDI Learn", "MIDI-Learn abbrechen" },
        { "Move a control on your MIDI device ...", "Bewege einen Regler an deinem MIDI-Gerät ..." },
        { "Mapped to", "Zugewiesen an" },
        { "Export MIDI map ...", "MIDI-Map exportieren ..." },
        { "Import MIDI map ...", "MIDI-Map importieren ..." },
        { "Clear all MIDI mappings", "Alle MIDI-Zuweisungen löschen" },
        { "The file is not an Ohpe MIDI map.", "Die Datei ist keine Ohpe-MIDI-Map." },
        { "Language", "Sprache" },
        { "Reset to default", "Auf Standard zurücksetzen" },
        // standalone audio settings
        { "Audio Settings", "Audio-Einstellungen" },
        { "Audio Settings ...", "Audio-Einstellungen ..." },
        { "Audio system", "Audiosystem" },
        { "Output device", "Ausgabegerät" },
        { "Sample rate", "Samplerate" },
        { "Buffer size", "Puffergröße" },
        { "samples", "Samples" },
        { "Latency", "Latenz" },
        { "Output latency", "Ausgangslatenz" },
        { "Multi-out routing", "Multi-Out-Routing" },
        { "Every mixer channel can go to the main output or to its own stereo output.",
          "Jeder Mixer-Kanal kann auf den Hauptausgang oder auf einen eigenen Stereoausgang gehen." },
        { "Device channels", "Gerätekanäle" },
        { "not available on this device - plays on Main",
          "auf diesem Gerät nicht vorhanden - spielt auf Haupt" },
        { "MIDI inputs", "MIDI-Eingänge" },
        { "No device", "Kein Gerät" },
        { "Save state ...", "Zustand speichern ..." },
        { "Load state ...", "Zustand laden ..." },
        { "Reset to default state", "Auf Grundzustand zurücksetzen" },
        { "Close", "Schließen" },
        { "designed by groovex", "designed by groovex" },
        // engines (phase 2)
        { "Wavetable", "Wavetable" },
        { "Pluck", "Zupfen" },
        { "Cluster", "Cluster" },
        { "Grain", "Grain" },
        { "Vocal", "Vokal" },
        { "Sampler", "Sampler" },
        // phase 2, engines B: samples, slices, drum kit
        { "Load sample ...", "Sample laden ..." },
        { "Clear sample", "Sample entfernen" },
        { "Built-in sound", "Eingebauter Klang" },
        { "Sample", "Sample" },
        { "Zones", "Zonen" },
        { "Slices", "Slices" },
        { "Add zone ...", "Zone hinzufügen ..." },
        { "Root key", "Grundton" },
        { "Pad", "Pad" },
        { "Choke group", "Choke-Gruppe" },
        { "Granulate tape", "Tape granulieren" },
        { "Vowel", "Vokal" },
        { "The sample file was not found.", "Die Sample-Datei wurde nicht gefunden." },
        { "The file is not a supported audio file.", "Die Datei ist keine unterstützte Audiodatei." },
        { "The audio file is empty.", "Die Audiodatei ist leer." },
        { "The audio file could not be read.", "Die Audiodatei konnte nicht gelesen werden." },
        { "Some sample files are missing.", "Einige Sample-Dateien fehlen." },
        // 0.2.0 package C: tape bounce / export
        { "The tape is empty - nothing to export.", "Das Band ist leer - nichts zu exportieren." },
        { "Could not create the export folder.", "Der Export-Ordner konnte nicht angelegt werden." },
        { "Could not write the file:", "Die Datei konnte nicht geschrieben werden:" },
        { "Could not save the preset in", "Das Preset konnte nicht gespeichert werden in" },
        { "A user preset with this name exists already.", "Ein eigenes Preset mit diesem Namen gibt es schon." },
        { "Replace", "Ersetzen" },
        { "Keep both", "Beide behalten" },
        { "Saved", "Gespeichert" },
        { "Export tape ...", "Band exportieren ..." },
        { "Master and stems (24-bit WAV)", "Master und Stems (24-Bit-WAV)" },
        { "Tape exported", "Band exportiert" },
        { "Audio input", "Audioeingang" },
        // 0.2 (package D): sequencers, sync, MIDI
        { "Sync & MIDI", "Sync & MIDI" },
        { "Sync & MIDI ...", "Sync & MIDI ..." },
        { "Sync source", "Sync-Quelle" },
        { "Clock smoothing", "Clock-Glättung" },
        { "MIDI output", "MIDI-Ausgang" },
        { "Tempo", "Tempo" },
        { "Internal clock", "Interne Clock" },
        { "MIDI clock", "MIDI-Clock" },
        { "Link (not built)", "Link (nicht enthalten)" },
        { "Send MIDI clock", "MIDI-Clock senden" },
        { "Send sequencer notes", "Sequencer-Noten senden" },
        { "None", "Keiner" },
        { "running", "läuft" },
        { "stopped", "gestoppt" },
        { "peers", "Teilnehmer" },
        { "Euclid", "Euklid" },
        { "Arpeggiator", "Arpeggiator" },
        { "Orbit", "Orbit" },
        { "Song", "Song" },
        { "Pattern", "Pattern" },
        { "Part", "Part" },
        { "Scale", "Skala" },
        { "Chord", "Akkord" },
        // 0.3 (package E): pro view, library, lock editing, tape / drum pages
        { "All", "Alle" },
        { "All four locks of this step are in use", "Alle vier Locks dieses Schritts sind belegt" },
        { "Amp Envelope", "Amp-Hüllkurve" },
        { "Arp", "Arp" },
        { "Author", "Autor" },
        { "Back", "Zurück" },
        { "Built-in tone - drop an audio file here", "Eingebauter Klang – Audiodatei hierher ziehen" },
        { "Cancel", "Abbrechen" },
        { "Categories", "Kategorien" },
        { "Clear", "Leeren" },
        { "Clear locks", "Locks löschen" },
        { "Cut", "Schneiden" },
        { "Delay (Send A)", "Delay (Send A)" },
        { "Delete", "Löschen" },
        { "Delete this preset?", "Dieses Preset löschen?" },
        { "Done", "Fertig" },
        { "Drop", "Einsetzen" },
        { "EQ & Output", "EQ & Ausgang" },
        { "Export", "Export" },
        { "Export folder (master + 8 stems, 24-bit WAV)", "Export-Ordner (Master + 8 Stems, 24-Bit-WAV)" },
        { "Exported", "Exportiert:" },
        { "FX Slot", "FX-Slot" },
        { "Factory", "Werk" },
        { "Favourites", "Favoriten" },
        { "Gate", "Gate" },
        { "Groove", "Groove" },
        { "Keep", "Behalten" },
        { "Layer", "Layer" },
        { "Kit", "Kit" },
        { "Lane", "Spur" },
        { "Lane 1", "Spur 1" },
        { "Lane 2", "Spur 2" },
        { "Lane 3", "Spur 3" },
        { "Lane 4", "Spur 4" },
        { "Lanes", "Spuren" },
        { "Library", "Bibliothek" },
        { "Lift", "Heben" },
        { "Load", "Laden" },
        { "Load a sample onto the pad", "Sample auf das Pad laden" },
        { "Load sample", "Sample laden" },
        { "Loaded", "Geladen" },
        { "Lock", "Lock" },
        { "Loop", "Schleife" },
        { "MIDI & Key", "MIDI & Tonart" },
        { "MIDI Out", "MIDI-Ausgang" },
        { "MPE", "MPE" },
        { "Macros", "Makros" },
        { "Master Compressor", "Master-Kompressor" },
        { "Master Output", "Master-Ausgang" },
        { "Master Filter", "Master-Filter" },
        // 0.7: stereo scope, spectrum terrain, master stereo
        { "Correlation", "Korrelation" },
        { "Display view", "Display-Ansicht" },
        { "Character", "Figur" },
        { "Stereo scope", "Stereobild" },
        { "Spectrum terrain", "Spektrum-Landschaft" },
        { "Animated 3D visuals", "Animierte 3D-Ansichten" },
        // 1.0: 3D spatialiser
        { "3D Space", "3D-Raum" },
        // p16: the morph space
        { "Morph", "Morph" },
        { "Morph Space", "Morph-Raum" },
        { "Control", "Steuerung" },
        { "Own sound", "Eigener Klang" },
        { "Corner", "Ecke" },
        { "Morph off", "Morph aus" },
        { "Morph off - switch it on with the first encoder", "Morph aus – mit dem ersten Regler einschalten" },
        { "Click a corner to give it a sound", "Klicke auf eine Ecke, um ihr einen Klang zu geben" },
        { "One sound - the pad morphs its parameters", "Ein Klang – das Pad morpht seine Parameter" },
        { "sounds in parallel", "Klänge parallel" },
        { "playing", "spielen" },
        { "Current sound", "Aktueller Klang" },
        { "Clear corner", "Ecke leeren" },
        { "Clear all corners", "Alle Ecken leeren" },
        { "Other", "Weitere" },
        { "3D space", "3D-Raum" },
        { "3D Motion", "3D-Bewegung" },
        { "3D Position", "3D-Position" },
        { "Motion Shape", "Bewegungsform" },
        { "Room & Distance", "Raum & Entfernung" },
        { "Monitoring", "Abhöre" },
        { "3D Monitoring", "3D-Abhöre" },
        { "Motion", "Bewegung" },
        { "Mono Check", "Mono-Prüfung" },
        { "Mono", "Mono" },
        { "Headphones", "Kopfhörer" },
        { "Speakers", "Lautsprecher" },
        { "F", "V" },
        { "B", "H" },
        { "Switch Space on (encoder 1) to place and move this part", "Raum einschalten (Encoder 1), um diesen Part zu platzieren und zu bewegen" },
        { "Binaural 3D \xe2\x80\x93 best with headphones", "Binaurales 3D \xe2\x80\x93 am besten mit Kopfhörern" },
        { "3D for speakers \xe2\x80\x93 switch 3D Monitoring to Headphones for binaural sound",
          "3D für Lautsprecher \xe2\x80\x93 für binauralen Klang die 3D-Abhöre auf Kopfhörer stellen" },
        { "3D Space is off for this part", "3D-Raum ist für diesen Part aus" },
        // p19: the resonator body
        { "Body", "Körper" },
        { "Resonator", "Resonanzkörper" },
        { "Body Tuning", "Körper-Stimmung" },
        { "Body Character", "Körper-Charakter" },
        { "Drive & Expression", "Drive & Ausdruck" },
        { "Resonator off - switch it on with the first encoder", "Resonanzkörper aus – mit dem ersten Regler einschalten" },
        { "Glide", "Glide" },
        { "Pitch Envelope", "Tonhöhen-Hüllkurve" },
        { "Mod Envelope", "Mod-Hüllkurve" },
        { "More tags, comma separated", "Weitere Tags, durch Komma getrennt" },
        { "Name", "Name" },
        { "Newest", "Neueste" },
        { "Note", "Note" },
        { "Oct", "Okt." },
        { "Pad Mix", "Pad-Mix" },
        { "Parts", "Parts" },
        { "Pitch & Level", "Tonhöhe & Pegel" },
        { "Preset", "Preset" },
        { "Pro", "Pro" },
        { "Pro view", "Profiansicht" },
        { "Probability", "Wahrscheinlichkeit" },
        { "Project", "Projekt" },
        { "Projects", "Projekte" },
        { "Ratchet", "Ratchet" },
        { "Rating", "Bewertung" },
        { "Redo", "Wiederholen" },
        { "Remote controls", "Fernsteuerung" },
        { "Reverb (Send B)", "Hall (Send B)" },
        { "Ring", "Ring" },
        { "Ring 1", "Ring 1" },
        { "Ring 2", "Ring 2" },
        { "Ring 3", "Ring 3" },
        { "Ring 4", "Ring 4" },
        { "Ring Velocity", "Ring-Anschlag" },
        { "Save", "Speichern" },
        { "Save preset", "Preset speichern" },
        { "Scale & MPE", "Skala & MPE" },
        { "Search", "Suchen" },
        { "Select a preset to hear it. Keep closes the library with it, Cancel goes back.", "Preset auswählen, um es zu hören. Behalten schließt die Bibliothek damit, Abbrechen geht zurück." },
        { "Shift-tap: edit and lock parameters", "Shift-Tippen: bearbeiten und Parameter locken" },
        { "Sort", "Sortieren" },
        { "Source & Dynamics", "Quelle & Dynamik" },
        { "Splice", "Einfügen" },
        { "Sync", "Sync" },
        { "Tags", "Tags" },
        { "Tap a pad to play and edit it", "Pad antippen zum Spielen und Bearbeiten" },
        { "Tape Character", "Bandcharakter" },
        { "Tempo & Sync", "Tempo & Sync" },
        { "The file could not be loaded.", "Die Datei konnte nicht geladen werden." },
        { "The tape is empty - record something first", "Das Band ist leer – zuerst etwas aufnehmen" },
        { "This part", "Dieser Part" },
        { "Track", "Spur" },
        { "Transport", "Transport" },
        { "Turn any encoder to lock its value on this step", "Einen Encoder drehen, um seinen Wert auf diesem Schritt zu locken" },
        { "Undo", "Rückgängig" },
        { "User", "Eigene" },
        { "Velocity", "Anschlag" },
        { "Voice", "Stimme" },
        { "Whole project", "Ganzes Projekt" },
        { "by", "von" },
        { "files to", "Dateien nach" },
        { "presets", "Presets" },
        // 0.7: modulation matrix and FX racks
        { "Mod", "Mod" },
        { "Rack", "Rack" },
        { "Sends", "Sends" },
        { "Slot", "Slot" },
        { "Bypass", "Bypass" },
        { "Empty slot", "Leerer Slot" },
        { "Move", "Schieben" },
        { "Type", "Typ" },
        { "Low", "Tief" },
        { "Mid", "Mitte" },
        { "High", "Hoch" },
        { "Routing", "Verbindung" },
        { "Curve & Lane", "Kurve & Spur" },
        { "LFO 2", "LFO 2" },
        { "LFO 2 Mode", "LFO 2 Modus" },
        { "LFO 3", "LFO 3" },
        { "LFO 3 Mode", "LFO 3 Modus" },
        { "Env 3", "Hüllkurve 3" },
        { "Env 3 Release", "Hüllkurve 3 Release" },
        { "FX", "FX" },
        { "Master FX", "Master-FX" },
        { "Preparing effect...", "Effekt wird vorbereitet ..." },
        { "Chorus", "Chorus" },
        { "Phaser", "Phaser" },
        { "Bitcrush", "Bitcrusher" },
        { "Filter Drive", "Filter-Drive" },
        { "Delay", "Delay" },
        { "Reverb", "Hall" },
        { "Hyper", "Hyper" },
        { "Distortion", "Verzerrung" },
        { "Flanger", "Flanger" },
        { "Compressor", "Kompressor" },
        { "Multiband", "Multiband" },
        { "EQ", "EQ" },
        { "Filter", "Filter" },
        { "Echo", "Echo" },
        { "Hall", "Raum" },
        // p11: tape transport
        { "Rec", "Rec" },
        { "Rec ready", "Rec bereit" },
        { "Armed", "Aufnahmebereit" },
        { "No track armed", "Keine Spur aufnahmebereit" },
        { "Bar", "Takt" },
        { "To start", "Anfang" },
        { "Stopped", "Gestoppt" },
        { "Back to the start", "Zurück zum Anfang" },
        { "Recording stopped - the tape keeps playing", "Aufnahme beendet – das Tape läuft weiter" },
        { "Recording track", "Aufnahme auf Spur" },
        { "Loop on", "Schleife an" },
        { "Loop off", "Schleife aus" },
        { "Track armed", "Spur aufnahmebereit" },
        { "Track disarmed", "Spur nicht mehr aufnahmebereit" },
        { "Recording - REC again punches out, STOP ends", "Aufnahme – REC erneut steigt aus, STOPP beendet" },
        { "Record is on - press PLAY to start", "Aufnahme bereit – START drücken" },
        { "Playing - STOP stops, twice goes to the start", "Wiedergabe – STOPP hält an, zweimal: zum Anfang" },
        { "Pick a track, press REC", "Spur wählen, REC drücken" },
        { "PLAY plays the tape - REC records on the armed track", "START spielt das Tape – REC nimmt auf" },
        { "To the start of the tape", "Zum Anfang des Tapes" },
        { "Stop - press again to go to the start", "Stopp – nochmal drücken: zum Anfang" },
        { "Play / stop the tape", "Tape starten / stoppen" },
        { "Record on the armed track - arms the selected track when none is armed",
          "Aufnahme auf der aufnahmebereiten Spur – ist keine bereit, wird die gewählte Spur bereit geschaltet" },
        { "Record on the tape - arms the selected track when none is armed",
          "Aufnahme aufs Tape – ist keine Spur bereit, wird die gewählte bereit geschaltet" },
        { "Loop between loop in and out", "Schleife zwischen Loop-Anfang und -Ende" },
        // p14: redesign - header round keys, wheels, glide, octave keys
        { "Random", "Zufall" },
        { "Random sound", "Zufallsklang" },
        { "Random: new values for the sound of the edit part's engine", "Zufall: neue Werte für den Klang der Engine des bearbeiteten Parts" },
        { "Save the edit part as a user preset", "Den bearbeiteten Part als eigenes Preset speichern" },
        // p14: key LEDs
        { "Bar: one segment per part - yellow = the edit part, violet = another part uses the engine", "Leuchtbalken: ein Segment je Part – gelb = der bearbeitete Part, violett = ein anderer Part benutzt die Engine" },
        { "Bar: yellow = shown page", "Leuchtbalken: gelb = angezeigte Seite" },
        // p14 rev 3: light bars, readouts, portamento caption
        { "Bar: green = playing (breathing)", "Leuchtbalken: grün = spielt (atmet)" },
        { "Bar: red = record on, breathing = recording", "Leuchtbalken: rot = Aufnahme an, atmend = nimmt auf" },
        { "Bar: yellow = loop on", "Leuchtbalken: gelb = Schleife an" },
        { "Bar: red = MIDI learn", "Leuchtbalken: rot = MIDI-Learn" },
        { "Bar: yellow = keyboard shifted down", "Leuchtbalken: gelb = Tastatur nach unten verschoben" },
        { "Bar: yellow = keyboard shifted up", "Leuchtbalken: gelb = Tastatur nach oben verschoben" },
        { "Tape position (minutes:seconds) and bar.beat; REC = recording, PLAY = tape rolling", "Tape-Position (Minuten:Sekunden) und Takt.Schlag; REC = Aufnahme, START = Tape läuft" },
        { "Range of the on-screen keyboard and its octave shift; red during MIDI learn", "Umfang der Bildschirmtastatur und ihre Oktavlage; rot während MIDI-Learn" },
        { "Keys", "Tasten" },
        { "Portamento", "Portamento" },
        { "violet = sequencer running", "violett = Sequencer läuft" },
        { "green = tape playing, red = recording", "grün = Tape spielt, rot = Aufnahme" },
        { "violet = an FX rack slot is on", "violett = ein Effekt-Rack-Platz ist an" },
        { "violet = a sequencer plays the drum kit", "violett = ein Sequencer spielt das Drum-Kit" },
        { "violet = LFO or modulation routed", "violett = LFO oder Modulation geroutet" },
        { "Pitch", "Pitch" },
        { "Pitch bend - springs back to the centre; right-click: MIDI learn", "Pitchbend – federt zur Mitte zurück; Rechtsklick: MIDI Learn" },
        { "Mod wheel (modulation matrix source); right-click: MIDI learn", "Mod-Rad (Quelle der Modulationsmatrix); Rechtsklick: MIDI Learn" },
        { "Glide mode of the edit part: Auto, Legato or Always", "Glide-Modus des bearbeiteten Parts: Auto, Legato oder Immer" },
        { "Glide time of the edit part", "Glide-Zeit des bearbeiteten Parts" },
        // p15: life
        { "Life", "Leben" },
        { "Life of the edit part: 0 = static; more = held notes keep evolving (drift, breathing filter, grain scatter). Right-click: MIDI learn",
          "Leben des bearbeiteten Parts: 0 = statisch; mehr = gehaltene Töne entwickeln sich weiter (Drift, atmender Filter, Grain-Streuung). Rechtsklick: MIDI-Learn" },
        { "Octave -", "Oktave -" },
        { "Octave +", "Oktave +" },
        { "Keyboard one octave down", "Tastatur eine Oktave tiefer" },
        { "Keyboard one octave up", "Tastatur eine Oktave höher" },
        { "Arm the track for recording", "Spur aufnahmebereit schalten" },
        { "Select the track", "Spur wählen" },
        { "Play / stop - sequencer and tape", "Start / Stopp – Sequencer und Tape" },
        { "Play / stop the sequencer", "Sequencer starten / stoppen" },
        { "MIDI maps and remote controls", "MIDI-Zuordnungen und Fernsteuerung" },
        { "Preset library", "Preset-Bibliothek" },
        { "All parameters of the mode", "Alle Parameter des Modus" },
    };
    static const std::map<juce::String, juce::String> t = [] {
        std::map<juce::String, juce::String> m;
        for (const auto& e : entries)
            m[juce::String::fromUTF8 (e.en)] = juce::String::fromUTF8 (e.de);
        return m;
    }();
    return t;
}
} // namespace

Language systemDefault()
{
    return juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("de") ? Language::German : Language::English;
}

Language current() noexcept
{
    int v = currentLang.load();
    if (v < 0)
    {
        v = (int) systemDefault();
        if (auto* env = std::getenv ("OHPE_LANGUAGE"))
            v = juce::String (env).startsWithIgnoreCase ("de") ? 1 : 0;
        currentLang.store (v);
    }
    return (Language) v;
}

void setCurrent (Language lang) noexcept { currentLang.store ((int) lang); }

juce::String tr (const juce::String& english)
{
    if (current() == Language::English) return english;
    const auto& t = table();
    const auto it = t.find (english);
    return it != t.end() ? it->second : english;
}

juce::String tr (const char* english) { return tr (juce::String::fromUTF8 (english)); }

juce::String upper (const juce::String& text)
{
    auto u = text.toUpperCase();
    juce::String out;
    out.preallocateBytes (u.getNumBytesAsUTF8() + 8);
    for (auto p = u.getCharPointer(); ! p.isEmpty();)
    {
        auto c = p.getAndAdvance();
        switch (c)
        {
            case 0xe4: c = 0xc4; break; // ae
            case 0xf6: c = 0xd6; break; // oe
            case 0xfc: c = 0xdc; break; // ue
            case 0xe9: c = 0xc9; break; // e acute
            default: break;
        }
        out += juce::String::charToString (c);
    }
    return out;
}

juce::StringArray allKeys()
{
    juce::StringArray keys;
    for (const auto& [k, v] : table()) keys.add (k);
    return keys;
}
} // namespace ohpe::i18n
