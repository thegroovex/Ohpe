# Ohpe-Dateiformate

*Deutsch · [English](../FORMATS.md)*

Seit 1.0 sind die Formate von allem, was Ohpe speichert, **eingefroren**: Jede Datei und jedes Dokument trägt eine
ausdrückliche Version, jede ältere Form lädt und wird migriert, und kein Format ändert sich, ohne dass es bei der
Durchsicht auffällt. Diese Seite nennt jedes Format, seine aktuelle Version und was sich zwischen den Versionen
geändert hat.

## Das Kompatibilitätsversprechen

1. **Ältere Dateien laden immer.** Eine Version von Ohpe liest jede Version eines Formats von der ersten bis zu ihrer
   eigenen und migriert sie beim Laden. Erneut gespeichert, wird die Datei in der aktuellen Version geschrieben.
2. **Unbekannte neuere Felder werden ignoriert.** Ein JSON-Dokument einer neueren Version lädt auch in einer älteren:
   Felder, Abschnitte und Parameter-IDs, die die ältere nicht kennt, werden übersprungen (oder, wo das Format es
   sagt, unverändert behalten und zurückgeschrieben). Werte, die beide kennen, behalten ihre Bedeutung.
3. **Host-Parameter-IDs werden nur angehängt.** `tests/core/golden/param_ids_v1.txt` führt jede ID in ihrer festen
   Reihenfolge; IDs werden nie entfernt, umbenannt oder umsortiert, und die Einträge von Auswahllisten sowie die
   Quellen- und Ziellisten der Modulationsmatrix werden nur ergänzt. Automation, MIDI-Zuordnungen und Presets zeigen
   weiter auf dasselbe.
4. **Binäre Layouts ändern sich nur mit neuer Version.** Ein Leser weist eine binäre Version zurück, deren Layout er
   nicht lesen kann (mit Meldung, nichts ändert sich), statt sie falsch zu lesen; `.ohpewt` behält ein festes vorderes
   Layout und darf nur am Ende wachsen.

*Nicht* versprochen: Eine Datei einer neueren Version kann Funktionen nutzen, die eine ältere nicht hat; diese Teile
fehlen dort beim Öffnen einfach.

## Versionen

Alle Konstanten stehen in `src/core/model/FormatVersions.h`; `tests/formats/FORMATS.lock` hält eine Kopie, die die
CI vergleicht.

| Schlüssel | Version | Was | Erkennbar an |
| --- | --- | --- | --- |
| `state` | 1 | Zustand von Plug-in / Standalone, Host-Sessions (das „Projekt“, das ein Host speichert) | JSON `"format": "ohpe-state"` |
| `preset` | 2 | Part- und Projekt-Presets, `.ohpepreset` | JSON `"format": "ohpe-preset"` |
| `modSchema` | 1 | Bedeutung der Modulationsmatrix-Routings in einem Preset | Preset-Feld `"modSchema"` |
| `tapeFile` | 1 | Band-Datei, `.ohpetape` (Kopf) | Kennung `OHPETPF1` + JSON `"format": "ohpe-tape"` |
| `tapeAudio` | 1 | Band-Audio (in `.ohpetape` und hinter dem Zustands-JSON) | Kennung `OHPETAPE` + uint32-Version |
| `wavetable` | 1 | Eigene Wavetable / Formantspur, `.ohpewt` | Kennung `OHPEWT` + uint16-Version |
| `pack` | 1 | Inhaltspaket, `.ohpepack` (Zip) | `pack.json`-Feld `"format"` |
| `midiMap` | 1 | MIDI-Zuordnung, `.ohpemidi`, und der Zustandsabschnitt `midimap` | JSON `"format": "ohpe-midimap"` |
| `settings` | 1 | Benutzereinstellungen, `<Benutzerdaten>/groovex/Ohpe/Settings/settings.json` | Feld `"version"` (fehlt = 0) |
| `updateSettings` | 1 | Updater-Einstellungen, `Settings/update.json` | Feld `"version"` (fehlt = 0) |
| `library` | 1 | Favoriten und Bewertungen, `library.json` | JSON `"format": "ohpe-library"` |
| `installedPacks` | 1 | Installierte Inhaltspakete, `Content/installed.json` | Feld `"format"` |
| `taste` | 1 | Geschmacksprofil der lernenden Zucht, `<Benutzerdaten>/groovex/Ohpe/Taste/taste.json` | JSON `"format": "ohpe-taste"` |
| `sequencer` | 1 | Zustands-/Projektabschnitt `sequencer` | Abschnittsfeld `"version"` |
| `orbit` | 1 | Zustands-/Projektabschnitt `orbit` | Abschnittsfeld `"version"` |
| `morph` | 1 | Abschnitt `morph` (und der `morph`-Block eines Part-Presets) | Abschnittsfeld `"version"` |
| `motion` | 1 | Abschnitt `motion` | Abschnittsfeld `"version"` |
| `harmony` | 1 | Abschnitt `harmony` | Abschnittsfeld `"version"` |
| `samples` | 1 | Abschnitt `samples` | JSON `"format": "ohpe-samples"` |
| `userTables` | 1 | Abschnitt `usertables` (und der `usertables`-Block eines Part-Presets) | JSON `"format": "ohpe-usertables"` |
| `genome` | 1 | Klang-Genom (Zucht) | Feld `"version"` |

## Die Formate

### Zustand, Session, Projekt (`state` 1)

`{"format": "ohpe-state", "version": 1, "app": "Ohpe", "params": {"<id>": <Klarwert>, …}, "sections": {…}}`.
Parameter stehen mit ID und Klarwert (Hz, Sekunden, Auswahl-Index); Abschnitte gehören ihrem Modul (`midimap`,
`sequencer`, `orbit`, `tape`, `morph`, `motion`, `harmony`, `samples`, `usertables`, `ui`), unbekannte Abschnitte
werden behalten und zurückgeschrieben. Im Plug-in und in der Standalone-App ist der Zustand
`<Zustands-JSON> NUL <Band-Audio>` (der Bandteil nur, wenn das Band Audio enthält).

Beim Laden kehren zuerst alle Parameter auf ihren Standard zurück, dann gilt das Dokument. Migrationen beim Laden:

| Seit | Ältere Form | Wird zu |
| --- | --- | --- |
| 0.2 | Zustand ohne Bandteil | ein leeres Band |
| p28 | beim Speichern gehaltene Performance-Pads | Pads aus (eine Geste, kein Teil des Songs) |
| p41 | Sessions vor den 3D-Schaltern | die Schalter folgen `space.on` / `space.out` |
| p50 | `ui`-Block nur mit `"preset"` (das Preset des Bearbeitungs-Parts) | dieses Preset benennt alle vier Parts (`"presets"`) |
| p59 | `harmony.kb.layout` = 3 ohne `harmony.kb.inkey` (die alte Anordnung *In Tonart*) | Anordnung Quarten + Nur Tonart an |

### Presets (`preset` 2, `modSchema` 1)

Die Felder beschreibt [PRESETS.md](PRESETS.md). Version 1 (0.1 / 0.2) hatte kein `kind`, `tags`, `engine`, `author`
oder `comment` und nannte den Herkunfts-Part in `"prefix"`: Sie lädt als Part-Preset, die Engine ergibt sich aus dem
Parameter `engine`. Version 2 (0.3) brachte `kind` (`part` / `project`) und die Metadaten; spätere Ergänzungen
innerhalb von Version 2 sind optionale Blöcke, die ältere Versionen ignorieren: `arp` und `modSchema` (0.7), `morph`
(p16), `usertables` (p20). Ein Preset ohne `modSchema` (= 0) läuft durch die Matrix-Migration (`mod::migrate`);
Projekt-Presets bekommen dieselbe p59-Rastermigration wie Zustände.

### Band-Datei (`tapeFile` 1, `tapeAudio` 1)

`.ohpetape`: 8 Byte `OHPETPF1`, uint32-Kopflänge, der JSON-Kopf (`format`, `version`, `sampleRate`, `lengthSeconds`,
`loopIn`, `loopOut`, `loop`, `editTrack`; fehlende Felder nehmen ihren Standard), dann das Band-Audio: `OHPETAPE`,
uint32-Version, uint32 Spuren (8), uint32 Frames pro Block (4096), float64-Samplerate, uint64-Länge, dann
`(uint32 Spur, uint32 Seite, float32 L[4096], float32 R[4096])` für jeden Block mit Audio, abgeschlossen mit
`0xFFFFFFFF`. Little-Endian. Ein Band mit anderer Samplerate wird beim Laden umgerechnet. Eine neuere Kopf- oder
Audio-Version als die eigene wird zurückgewiesen.

### Wavetable (`wavetable` 1)

`.ohpewt`, Little-Endian: `OHPEWT`, uint16-Version, uint32-Framegröße (2048), uint32 Frames (0 – 256), uint32
Formant-Frames (0 – 64), uint32-Namenslänge + UTF-8-Name, dann die Stufe-0-Samples jedes Frames (float32) und 15
float32 pro Formant-Frame (5 Frequenzen, 5 Pegel in dB, 5 Bandbreiten). Das vordere Layout ist fest: Eine spätere
Version darf nur Daten anhängen, die diese Version nicht liest.

### Inhaltspaket (`pack` 1)

Ein Zip mit `pack.json` (`format`, `id`, `version`, `kind`, `name`, `description`, `minApp`, `files` mit der SHA-256
jeder Datei, optional `license` / `signature`) und einem Ordner je Art; siehe [INHALTSPAKETE.md](INHALTSPAKETE.md).
`pack.json` ohne `format` gilt als Format 1; ein neueres Format wird zurückgewiesen („pack.json has a newer format“).
Die ersten Pakete waren *flache* Zips namens `Ohpe-content-<id>-<version>.zip`, Presets im Hauptordner und ohne
`pack.json`: Sie installieren weiterhin, der Inhalt wandert in den Ordner seiner Art.

### Einstellungen (`settings` 1, `updateSettings` 1)

`settings.json` und `update.json` gelten pro Benutzer, nie pro Projekt. Dateien von vor 1.0 haben kein `"version"`
(= 0). Für `settings.json` läuft bei Version 0 die Controller-Migration von 0.6.3: Ein Controller, der eingerichtet
wurde, bevor es `controller.chosen` gab, startet mit dem Grid-Controller-Profil und automatischen Ports. p64 hat den Schlüssel
`"tour"` (`{"standalone": bool, "plugin": bool}`, die Einführungstour wurde gesehen) ohne Versionssprung ergänzt:
Dateien ohne ihn zeigen die Tour einmal, ältere Versionen behalten ihn als unbekannten Schlüssel. Unbekannte
Schlüssel von `settings.json` bleiben erhalten und werden zurückgeschrieben; wer die Datei speichert, schreibt die
eigene Version.

### Geschmacksprofil (`taste` 1)

Das Profil der lernenden Zucht, pro Benutzer, nie pro Projekt und nie irgendwohin gesendet:
`{"format": "ohpe-taste", "version": 1, "learning": bool, "curiosity": 0..1, "sounds": {"<Schlüssel>": {"f": Fitness,
"n": Ladevorgänge, "e": Engine, "t": Aktualität}}, "engines": {"<Engine-Typ>": {"<Gen-ID>": [Gewicht gemocht,
Mittel gemocht, Gewicht abgelehnt, Mittel abgelehnt]}}}`. Schlüssel sind Bibliotheks-UIDs oder `breed:<Seed>`;
Gen-IDs sind part-lokale Parameter-IDs, die Mittelwerte normiert (0..1). Eine beschädigte Datei (kein JSON, falsches
`format`) lädt als leeres Profil; falsche Werte werden verworfen oder begrenzt; unbekannte Gen-IDs oder Engines
werden ignoriert (und nicht zurückgeschrieben). Fixtures: `taste/taste-v1.json`, `taste/taste-v1-damaged.json`.

### Alles Weitere (Version 1)

`.ohpemidi` / `midimap`, `library.json`, `Content/installed.json` und die Zustandsabschnitte tragen seit ihrer
Einführung Version 1; zu migrieren gibt es noch nichts.

## Goldene Testdateien und die CI-Prüfung

`tests/formats/fixtures` enthält eine eingefrorene Datei jedes Formats in seiner aktuellen Form sowie ältere Formen
(Preset der Version 1, 0.1-Zustand ohne Abschnitte, der 0.6-Zustand mit der alten Anordnung *In Tonart* und dem
`ui`-Block mit nur `preset`, `settings.json` ohne `version`, das alte flache Paket-Zip, der kleinste Band-Kopf) und
neuere Formen (ein Zustand und ein Preset einer künftigen Version, ein neueres Band- und Paketformat, das
zurückgewiesen werden muss). `tests/core/FormatTests.cpp` und `tests/plugin/FormatPluginTests.cpp` laden, migrieren
und speichern jede davon erneut.

`scripts/ci/check-formats.py` (Linux-Core-Tests, ohne Build) schlägt fehl, wenn eine Versionskonstante, eine
Testdatei oder die Sperrdatei voneinander abweichen, wenn eine Testdatei geändert oder entfernt wird, wenn eine
Version sinkt oder ohne neue Testdatei steigt, oder wenn sich die Sperrdatei ohne beide Formatdokumente ändert.

### Ein Format ändern

1. Die Konstante in `FormatVersions.h` erhöhen (nie senken) und die Migration von der vorigen Version schreiben.
2. Eine Testdatei der **vorigen** Form (und eine der neuen) in `tools/formats/make_fixtures.py` ergänzen, das Skript
   ausführen und ihre Zeilen an `FORMATS.lock` anhängen (`make_fixtures.py --lock`). Bestehende Testdateien bleiben,
   wie sie sind.
3. Die Versionszeile in `FORMATS.lock`, diese Seite und [../FORMATS.md](../FORMATS.md) aktualisieren und einen Test
   ergänzen.
