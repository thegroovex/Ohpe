# Preset-Bibliothek

*Deutsch · English: [../en/PRESETS.md](../en/PRESETS.md)*

![Preset-Bibliothek](../images/ohpe-library-de.png)

## Die Bibliothek benutzen

**BIBLIOTHEK** (oben am Instrument) öffnet die Bibliothek. Sie liegt über den Tasten und dem
Display; die Klaviatur bleibt spielbar, so kannst du Klänge beim Stöbern anspielen.

| Bereich | Funktion |
|---|---|
| **Kategorien** (links) | Bass, Tasten, Flächen, Leads, Zupfer, Drums, FX, Arp, Texturen, Vokal. Eine oder mehrere wählen: die Liste zeigt Presets, die **einen** der gewählten Tags tragen. |
| **Engine** (links) | Nur Presets einer Engine (oder *Alle*). |
| **Suchen** (oben) | Wörter werden in Name, Autor, Tags, Engine und Kommentar gesucht (Groß-/Kleinschreibung egal; deutsche und englische Kategorie- und Engine-Namen gehen). Alle Wörter müssen passen. |
| **Alle / Werk / Eigene** | Welche Bank: die eingebauten Werks-Presets, deine eigenen oder beide. |
| **Parts / Projekte** | Nur Part-Presets (ein Klang) oder nur Projekt-Presets (das ganze Instrument). |
| **★ Favoriten** | Nur Presets, die du als Favorit markiert hast. |
| **Sortieren** | Nach Name, Bewertung (beste zuerst), Engine oder Neueste. |
| **Liste** | Ein ausgewähltes Preset wird **sofort geladen** (Vorhören) – Part-Presets in den Part, der mit der Part-Auswahl gewählt ist, Projekt-Presets ersetzen alles. Der Stern links markiert einen Favoriten, die Sterne rechts bewerten (derselbe Stern noch einmal löscht die Bewertung). Pfeiltasten bewegen durch die Liste. |
| **Details** (rechts) | Name, Bank, Art, Engine, Autor, Tags, Kommentar, Favorit und Bewertung. Eigene Presets können gelöscht werden (in den Papierkorb). |
| **Behalten / Abbrechen** | *Behalten* (oder Enter, oder Doppelklick auf ein Preset) schließt die Bibliothek mit dem geladenen Preset. *Abbrechen* (oder Esc) stellt den Klang wieder her, der vor dem Öffnen da war. |

Der **Preset-Streifen** neben der Part-Auswahl zeigt das zuletzt geladene Preset; seine Pfeile
laden das vorige / nächste Preset der aktuellen Liste der Bibliothek (mit ihren Filtern und ihrer
Sortierung), ein Klick auf den Namen öffnet die Bibliothek.

### Speichern

*Preset speichern …* öffnet das Speicherfeld: Name, Autor (wird gemerkt), Tags (Kategorien und
eigene Tags, durch Komma getrennt) und was gespeichert wird – **Dieser Part** (der Klang des
bearbeiteten Parts) oder **Ganzes Projekt** (alle Parts, Drum-Kit, Mixer, Effekte, Sequencer-Pattern
und Einstellungen). Presets mit gleichem Namen werden nie überschrieben; es wird eine Nummer angehängt.

Eigene Presets sind Dateien im Benutzerordner – kopieren, teilen und sichern ist einfach:

| System | Benutzerordner |
|---|---|
| macOS | `~/Library/Application Support/groovex/Ohpe/Presets` |
| Windows | `%APPDATA%\groovex\Ohpe\Presets` |
| Linux | `~/.config/groovex/Ohpe/Presets` |

Part-Presets liegen in `Parts/`, Projekt-Presets in `Projects/`; Unterordner werden mit gelesen. Die
Umgebungsvariable `OHPE_PRESET_DIR` lässt Ohpe einen anderen Ordner benutzen. Favoriten,
Bewertungen und der Autorname stehen in `library.json` im selben Ordner. Eine `.ohpepreset`-Datei
kann auch einfach auf das Instrument gezogen werden.

### MIDI

Drei Fernsteuer-Parameter machen die Bibliothek vom Controller aus spielbar (Rechtsklick auf die
Taste BIBLIOTHEK oder *MIDI → Fernsteuerung*, dann *MIDI Learn*):

| Parameter | ID | Aktion |
|---|---|---|
| Bibliothek | `lib.open` | öffnet / schließt die Bibliothek |
| Vorheriges Preset | `lib.prev` | lädt das vorige Preset der aktuellen Liste |
| Nächstes Preset | `lib.next` | lädt das nächste Preset der aktuellen Liste |
| Bearbeiteter Part | `ui.part` | wählt den bearbeiteten Part 1–4 (dorthin laden Part-Presets) |

Ein Trigger löst aus, wenn sein Regler auf „an“ geht (CC-Wert ab 64 oder eine Note); Drücken und
Loslassen einer Taste ergibt einen Schritt.

## Dateiformat (Version 2)

Ein Preset ist eine UTF-8-JSON-Datei mit der Endung `.ohpepreset`. Aufbau und Beispiel stehen in
der [englischen Fassung](../en/PRESETS.md#file-format-version-2); kurz:

* `format` = `"ohpe-preset"`, `version` = `2`; Dateien der Version 1 (ohne `kind` / `tags`) laden
  als Part-Presets.
* `kind`: `"part"` (ein Part) oder `"project"` (das ganze Instrument).
* `name`, `author`, `comment`, `tags` (Liste; die Kategorien sind feste englische Schlüssel `Bass`,
  `Keys`, `Pads`, `Leads`, `Plucks`, `Drums`, `FX`, `Arp`, `Textures`, `Vocal` – die Oberfläche
  übersetzt sie; alles andere ist ein freier Tag), `engine` (Engine-ID).
* `params`: Werte als **Klartextwerte** (Einheit wie im Display). Part-Presets ohne Part-Präfix
  (`"analog.cutoff"`), Projekt-Presets mit vollen IDs.
* `sections` (nur Projekte): `"sequencer"`, `"orbit"` und optional `"samples"`.

Regeln beim Laden: nicht genannte Parameter gehen auf ihren Standardwert; unbekannte IDs werden
ignoriert; der MIDI-Kanal eines Parts bleibt; Projekt-Presets lassen Fernsteuerung (`lib.*`,
`ui.*`, `tape.edit.*`) und Sync-Einstellungen (`sync.*`) unberührt; MIDI-Map und Bandaudio sind nie
Teil eines Presets; Laden ändert nie die Parameterliste.

### Werksbank

Die Werks-Presets sind die `*.ohpepreset`-Dateien in
[`resources/presets/factory/`](../../resources/presets/factory). Sie werden als Binärdaten in
Plug-in und App eingebettet (CMake-Ziel `ohpe_presets`, Namensraum `OhpePresets`) und sind
schreibgeschützt. Neue Werksklänge: Dateien dort ablegen und CMake neu ausführen. Die Werksbank
enthält 120 Part-Presets und 7 Projekt-Presets – siehe [FACTORY-SOUNDS.md](FACTORY-SOUNDS.md). Sie
werden von `tools/presets/make_factory.py` erzeugt; der Plug-in-Test rendert jedes davon und prüft
Pegel, Spitzen und Spektrum.
