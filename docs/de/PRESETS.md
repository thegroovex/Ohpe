# Preset-Bibliothek

*Deutsch · English: [../en/PRESETS.md](../en/PRESETS.md)*

![Preset-Bibliothek](../images/ohpe-library-de.png)

## Die Bibliothek benutzen

**BIBLIOTHEK** (oben am Instrument) öffnet die Bibliothek. Sie liegt über den Tasten und dem
Display; die Klaviatur bleibt spielbar, so kannst du Klänge beim Stöbern anspielen.

| Bereich | Funktion |
|---|---|
| **Kategorien** (links) | Bass, Tasten, Flächen, Leads, Zupfer, Drums, FX, Arp, Texturen, Vokal, Boom (0.6: tiefe gestimmte Kicks und Sub-Bässe), Hybrid (0.8: zwei Engines in einem Klang), Klavier (0.8: akustische und elektrische Pianos). Eine oder mehrere wählen: die Liste zeigt Presets, die **einen** der gewählten Tags tragen. |
| **Engine** (links) | Nur Presets einer Engine (oder *Alle*). |
| **Suchen** (oben) | Wörter werden in Name, Autor, Tags, Engine und Kommentar gesucht (Groß-/Kleinschreibung egal; deutsche und englische Kategorie- und Engine-Namen gehen). Alle Wörter müssen passen. |
| **Alle / Werk / Eigene** | Welche Bank: die eingebauten Werks-Presets, deine eigenen oder beide. |
| **Parts / Projekte** | Nur Part-Presets (ein Klang) oder nur Projekt-Presets (das ganze Instrument). |
| **★ Favoriten** | Nur Presets, die du als Favorit markiert hast. |
| **Sortieren** | Nach Name, Bewertung (beste zuerst), Engine oder Neueste. |
| **Liste** | Ein ausgewähltes Preset wird **sofort geladen** (Vorhören) – Part-Presets in den Part, der mit der Part-Auswahl gewählt ist, Projekt-Presets ersetzen alles. Der Stern links markiert einen Favoriten, die Sterne rechts bewerten (derselbe Stern noch einmal löscht die Bewertung). Pfeiltasten bewegen durch die Liste. |
| **Details** (rechts) | Name, Bank, Art, Engine, Autor, Tags, Kommentar, Favorit und Bewertung. Eigene Presets können gelöscht werden (in den Papierkorb). |
| **Behalten / Abbrechen** | *Behalten* (oder Enter, oder Doppelklick auf ein Preset) schließt die Bibliothek mit dem geladenen Preset. *Abbrechen* (oder Esc) stellt den Klang wieder her, der vor dem Öffnen da war. |

Der **Preset-Streifen** neben der Part-Auswahl zeigt das zuletzt in den **Edit-Part** geladene
Preset: Jeder Part merkt sich sein eigenes Preset, ein Wechsel des Parts (Part-Auswahl, MIDI,
Part-Tasten des Grid-Controllers) wechselt also auch den Namen – im Streifen, in der Bibliothek und
im Controller-Display. Ein Projekt-Preset benennt alle vier Parts; ein Part ohne geladenes Preset
zeigt „Init“. Seine Pfeile laden das vorige / nächste Preset der aktuellen Liste der Bibliothek (mit
ihren Filtern und ihrer Sortierung) in den Edit-Part, ausgehend von dessen Preset; ein Klick auf den
Namen öffnet die Bibliothek.

### Erster Start

Ein neues Ohpe – der erste Start der App oder eine neue Plug-in-Instanz in einem Song – startet mit
dem **ersten Werks-Preset der Bibliothek** (die eingebaute Bank in der Standard-Sortierung der
Bibliothek, nach Name), nicht mit einem leeren „Init“-Klang: Ein Part-Preset wird in alle vier
Parts geladen (jeder behält seinen MIDI-Kanal), ein Projekt-Preset würde alles ersetzen. Der
Preset-Streifen zeigt seinen Namen; Band und Sequenzer sind leer. Eine gespeicherte Sitzung – der
Song im Host, die letzte Sitzung der Standalone-App oder eine geladene Zustandsdatei – stellt sich
stattdessen selbst wieder her, mit den Namen der Presets, die geladen waren (einer je Part; eine
Sitzung aus Version 0.6.1 oder älter gibt ihren einen Namen allen Parts). In der Standalone-App
führt *Audio-Einstellungen … › Auf Grundzustand zurücksetzen* zu diesem Start zurück.

### Speichern

*Preset speichern …* öffnet das Speicherfeld: Name, Autor (wird gemerkt), Tags (Kategorien und
eigene Tags, durch Komma getrennt) und was gespeichert wird – **Dieser Part** (der Klang des
bearbeiteten Parts) oder **Ganzes Projekt** (alle Parts, Drum-Kit, Mixer, Effekte, Sequencer-Pattern
und Einstellungen). Das Feld übernimmt Name und Tags des geladenen Presets, ein bearbeiteter Klang
behält also seine Kategorien.

Gibt es schon ein eigenes Preset mit diesem Namen, fragt Ohpe nach: **Ersetzen** überschreibt es
(Favorit und Bewertung bleiben), **Beide behalten** speichert das neue als „Name 2“ (oder mit der
nächsten freien Nummer), **Abbrechen** führt zurück zum Feld. Werks-Presets werden nie überschrieben.

Nach dem Speichern ist das neue Preset in der Liste ausgewählt. Würde ein Filter es verbergen (eine
Kategorie, die Engine, der Suchtext, Favoriten, die Werksbank), werden die Filter zurückgesetzt und
die Liste zeigt **Eigene**. Die Zeile über der Liste bestätigt das Speichern – oder nennt den Ordner,
in den nicht geschrieben werden konnte.

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
  `Keys`, `Pads`, `Leads`, `Plucks`, `Drums`, `FX`, `Arp`, `Textures`, `Vocal`, `Boom`, `Hybrid`, `Piano` (0.8) – die Oberfläche
  übersetzt sie; alles andere ist ein freier Tag), `engine` (Engine-ID).
* `params`: Werte als **Klartextwerte** (Einheit wie im Display). Part-Presets ohne Part-Präfix
  (`"analog.cutoff"`), Projekt-Presets mit vollen IDs.
* `arp` (nur Part-Presets, optional, 0.7): Arpeggiator-Einstellungen, die zum Klang gehören –
  siehe [Arp-Block](#arp-block).
* `sections` (nur Projekte): `"sequencer"`, `"orbit"` und optional `"samples"`.

Regeln beim Laden: nicht genannte Parameter gehen auf ihren Standardwert; unbekannte IDs werden
ignoriert; der MIDI-Kanal eines Parts bleibt; Projekt-Presets lassen Fernsteuerung (`lib.*`,
`ui.*`, `tape.edit.*`) und Sync-Einstellungen (`sync.*`) unberührt; MIDI-Map und Bandaudio sind nie
Teil eines Presets; Laden ändert nie die Parameterliste.

### Arp-Block

Der Arpeggiator ist global (`seq.arp.*`, ein Arp, der einen Part spielt). Ein **Part**-Preset kann
die Arp-Einstellungen seines Klangs in einem optionalen Objekt `"arp"` mitbringen:

| Schlüssel | Parameter | Werte |
|---|---|---|
| `on` | `seq.arp.on` | `1` (Standard, wenn der Block da ist) oder `0`; `true` / `false` gehen auch |
| `mode` | `seq.arp.mode` | `0` Aufwärts, `1` Abwärts, `2` Auf/Ab, `3` Zufall, `4` Wie gespielt, `5` Akkord |
| `oct` | `seq.arp.oct` | Oktaven `1`–`4` |
| `gate` | `seq.arp.gate` | `0.05`–`1.0` (Notenlänge als Anteil eines Schritts) |
| `rate` | `seq.arp.rate` | `0` 1/4, `1` 1/8, `2` 1/16, `3` 1/32, `4` 1/4T, `5` 1/8T, `6` 1/16T, `7` 1/32T, `8` 1/4., `9` 1/8., `10` 1/16. |
| `latch` | `seq.arp.latch` | wird aus Kompatibilität gelesen, aber **nicht angewendet** (Fix-Paket p28): Halten ist die Einstellung des Spielers, standardmäßig aus; *Preset speichern* schreibt es nicht mehr |

Was beim Laden eines Part-Presets in Part *N* passiert:

* **Mit Arp-Block:** der Arpeggiator geht an und spielt Part *N* (`seq.arp.on` = 1,
  `seq.arp.part` = *N*) mit den genannten Einstellungen; nicht genannte Schlüssel gehen auf ihren
  Standardwert. Einen Schlüssel `part` gibt es nicht – Ziel ist immer der Part, in den das Preset
  lädt. Wie jede andere Parameteränderung beim Laden ist das eine Änderung, die der Host sieht.
* **Ohne Arp-Block:** der Arpeggiator bleibt, wie er ist – mit einer Ausnahme: hat der Arp-Block
  eines Presets den Arp für Part *N* eingeschaltet und wurde der Arp seitdem nicht verändert, dann
  schaltet ein Preset ohne Arp-Block in Part *N* ihn wieder aus (die Einstellungen bleiben). Ein von
  Hand eingerichteter, nach dem Laden veränderter oder einen anderen Part spielender Arp wird nie
  angefasst. Ohpe merkt sich nur bis zum Schließen des Instruments, welches Preset den Arp
  eingeschaltet hat; in einem wieder geöffneten Projekt gilt der Arp als von Hand eingestellt.
* Projekt-Presets haben den Arp in ihren `params` (`seq.arp.*`); ein `"arp"`-Block dort wird
  ignoriert.
* *Preset speichern → Dieser Part* schreibt den Arp-Block, wenn der Arp an ist und diesen Part
  spielt – ein gespeicherter Arp-Klang lädt mit seinem Arp.

Die Werks-Arp-Presets spielen, solange Tasten gehalten werden, und stoppen beim Loslassen. Damit ein
Arpeggio freihändig weiterläuft, *Halten* auf der Sequenzer-Seite einschalten (`seq.arp.latch`, per
MIDI steuerbar): dann beginnt die nächste Taste oder der nächste Akkord ohne gehaltene Taste ein neues.

### Morph-Block

Ein **Part**-Preset kann die Ecken des [Morph-Pads](MORPH-RAUM.md) des Parts in einem optionalen Objekt `"morph"`
mitbringen (p16):

```json
"morph": { "corners": [ { "name": "Glass Shimmer", "params": { "engine": 1, "wavetable.pos": 0.4 } },
                        { "name": "Breath Choir Pad", "params": { "engine": 5 } },
                        null,
                        { "name": "Crystal FM Pad", "params": { "engine": 0 } } ] }
```

* Vier Einträge, Ecken A (oben links), B (oben rechts), C (unten links), D (unten rechts); `null` = die Ecke steht
  für den eigenen Klang des Parts.
* `params` wie die eigenen `params` des Presets: lokale IDs ohne Part-Präfix, Klarwerte, lückenhaft erlaubt
  (fehlend = Standard). Der Morph-Block (`morph.*`) und das MIDI-Routing (`midich`, `quant`, `chord`, `mpe`,
  `mpe.bend`, `mpe.press`, `mpe.timbre`) gehören nicht zu einer Ecke.
* Ein Part-Preset, das in Part *N* geladen wird, setzt die Ecken von Part *N* aus dem Block; ein Part-Preset
  **ohne** Block leert sie. *Preset speichern → Dieser Part* schreibt den Block, wenn eine Ecke belegt ist.
* Projekte und der Plug-in-Zustand behalten die Ecken aller Parts im Abschnitt `"morph"`
  (`{"version": 1, "parts": [ {"corners": [...]}, ... ]}`).

### Werksbank

Die Werks-Presets sind die `*.ohpepreset`-Dateien in
[`resources/presets/factory/`](../../resources/presets/factory). Sie werden als Binärdaten in
Plug-in und App eingebettet (CMake-Ziel `ohpe_presets`, Namensraum `OhpePresets`) und sind
schreibgeschützt. Neue Werksklänge: Dateien dort ablegen und CMake neu ausführen. Die Werksbank
enthält 282 Part-Presets und 14 Projekt-Presets (0.8: 120 neue Part-Presets und vier neue Projekte; der freie Tag **MPE** markiert Presets mit MPE- / Aftertouch-Zuordnungen, siehe [MIDI.md](MIDI.md)) – siehe [FACTORY-SOUNDS.md](FACTORY-SOUNDS.md). Sie
werden von `tools/presets/make_factory.py` erzeugt; der Plug-in-Test rendert jedes davon und prüft
Pegel, Spitzen und Spektrum.
