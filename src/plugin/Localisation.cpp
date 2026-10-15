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
