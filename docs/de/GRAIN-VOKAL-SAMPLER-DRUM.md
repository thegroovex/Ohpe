# Grain, Vokal, Sampler und das Drum-Kit

*Deutsch · English: [../en/GRAIN-VOCAL-SAMPLER-DRUM.md](../en/GRAIN-VOCAL-SAMPLER-DRUM.md)*

Phase 2 bringt drei Engines für den Synth-Part (Auswahl mit **Engine**, `p1.engine`) und das
**Drum-Kit**, das einen eigenen Mixer-Kanal hat. Alle klingen sofort: Ohne geladene Samples nutzen
Grain und Sampler einen eingebauten harmonischen Klang, und das Drum-Kit ist vollständig
synthetisch. Ohpe liefert keine Audio-Samples mit. Alle Parameter stehen in
[PARAMETERS.md](../PARAMETERS.md).

## Grain

Granularsynthese: Jede Stimme verstreut kurze, weich gefensterte Körner (Grains) aus einer Quelle.

| Regler | Wirkung |
|---|---|
| **Position** (Makro 1) | Stelle in der Quelle, an der gelesen wird (0 = Anfang, 1 = Ende) |
| **Korngröße** (Makro 2) | Länge eines Korns, 5 – 500 ms |
| **Dichte** (Makro 3) | gestartete Körner pro Sekunde und Stimme, 1 – 100 |
| **Streuung** (Makro 4) | zufällige Streuung der Position (bis ±0,5 s) und des Zeitpunkts |
| **Korn-Tonhöhe** | transponiert die Körner (±24 Halbtöne) zusätzlich zur gespielten Note |
| **Rückwärts-Anteil** | Anteil der Körner, die rückwärts spielen |
| **Stereobreite** | zufälliges Panorama der Körner |
| **Scan-Tempo** | bewegt die Position, solange eine Note gehalten wird (1 = Echtzeit, negativ = rückwärts) |
| **Quelle** | *Sample* (geladene Datei oder eingebauter Klang) oder *Tape* (ein Tape-Bereich, siehe unten) |
| **Klang** | Tiefpass-Helligkeit; Mod-Hüllkurve und LFO-Ziel *Klangfarbe* bewegen sie |

Die gespielte Note transponiert relativ zu **C4 (MIDI 60)**: C4 spielt die Quelle in Originaltonhöhe.
Mit *Quelle = Tape* granuliert die Engine den Tape-Bereich, den die Tape-Maschine über die
Schnittstelle `IAudioRegion` übergibt; ohne Bereich nimmt sie das Sample.

## Vokal

Formantsynthese einer Singstimme: Ein Stimmlippen-Impuls und Atemrauschen speisen fünf
Formant-Resonatoren.

| Regler | Wirkung |
|---|---|
| **Vokal** (Makro 1) | stufenlos A (0) – E (1) – I (2) – O (3) – U (4) |
| **Formant-Verschiebung** (Makro 2) | verschiebt alle Formanten um ±12 Halbtöne (kleinere oder größere Stimme) |
| **Atem** (Makro 3) | Atemrauschen statt klarem Ton |
| **Spannung** (Makro 4) | locker und dunkel (0) bis gepresst und hell (1) |
| **Vibrato-Tiefe / -Tempo** | Tonhöhen-Vibrato bis ±½ Halbton, 0,5 – 10 Hz |
| **Jitter** | langsame zufällige Tonhöhenbewegung für eine natürliche Stimme |

Mod-Hüllkurve und LFO-Ziel *Klangfarbe* verschieben die Vokal-Position.

## Sampler

Spielt Samples über die Tastatur, entweder als **Zonen** (Multisampling) oder als **Slices**.

| Regler | Wirkung |
|---|---|
| **Start** (Makro 1) | Startversatz im Sample oder Slice |
| **Klang** (Makro 2) | Tiefpass-Helligkeit (Mod-Hüllkurve und LFO *Klangfarbe* bewegen sie) |
| **Loop-Start / Loop-Ende** (Makros 3, 4) | Loop-Bereich als Anteil des Samples (wenn die Datei keinen eigenen Loop hat) |
| **Grundton** | Note, die eine Zone in Originaltonhöhe spielt (Zonen können einen eigenen Grundton haben) |
| **Loop** | *Aus* (One-Shot), *Vorwärts*, *Ping-Pong* |
| **Rückwärts** | spielt rückwärts |
| **Modus** | *Zonen* oder *Slices* |
| **Slice-Empfindlichkeit** | wie kleine Anschläge noch einen neuen Slice beginnen |
| **Erste Slice-Taste** | Taste von Slice 1 (Standard C2 = 36); jede weitere Taste spielt den nächsten Slice |

**Zonen:** Jede Zone hat einen Tastenbereich, einen Anschlagsbereich, einen Grundton und optionale
Loop-Punkte. In WAV- oder AIFF-Dateien gespeicherte Loop-Punkte werden automatisch übernommen.

**Slices:** Ohpe findet die Anschläge (Transienten) des ersten Samples und schneidet es in Slices,
höchstens 64. Jeder Slice spielt auf seiner eigenen Taste in Originaltonhöhe und endet dort, wo der
nächste beginnt. Eine Änderung der *Slice-Empfindlichkeit* schneidet das Sample neu.

## Drum-Kit

24 Pads auf **MIDI-Kanal 10** (Pad 1 = `drum.base`, Standard C2 = 36, dann ein Pad pro Halbton).
Das Kit spielt auf Mixer-Kanal 5 (Quelle *Drum*). Jedes Pad ist ein synthetisches Drum-Modell oder
ein Sample:

| Typ | Klang |
|---|---|
| Bassdrum | Sinus mit Tonhöhen-Sweep und Klick; *Klangfarbe* gibt Klick und Sättigung dazu |
| Snare | zwei gestimmte Kesseltöne plus gefiltertes Rauschen; *Klangfarbe* färbt das Rauschen |
| Hi-Hat | sechs unharmonische Rechteck-Oszillatoren und Rauschen, hochpassgefiltert; *Abklingen* macht sie geschlossen oder offen |
| Klatschen | drei Rauschstöße und eine Fahne durch einen Bandpass |
| Tom | Sinus mit kurzem Tonhöhenabfall und Stockgeräusch |
| Perkussion | Zwei-Operator-FM-Klang (Rim, Glocke, Holzblock); *Klangfarbe* bestimmt das Modulator-Verhältnis |
| Sample | das Sample des Pads in der Stimmung des Pads; *Abklingen* = 1 spielt es bis zum Ende |

Pro Pad: **Stimmung** (±24 Halbtöne), **Abklingen**, **Pegel**, **Panorama**, **Klangfarbe** und
**Choke-Gruppe** (0 = keine, 1 – 8). Jedes Pad ist monofon, und Pads derselben Choke-Gruppe
schneiden sich gegenseitig ab – im Werks-Kit teilen sich geschlossene, offene und Pedal-Hi-Hat
(Pads 6 – 8) die Gruppe 1. Note-Offs werden ignoriert.

## Samples laden

Samples werden im Plug-in von der Festplatte geladen (WAV, AIFF, FLAC und Ogg Vorbis): ein Sample
für die ganze Tastatur, weitere Multisample-Zonen, die Grain-Quelle oder ein Sample pro Drum-Pad
(das Pad schaltet auf Typ *Sample*). Ohpe speichert die **Pfade** der geladenen Dateien mit dem
Projekt, nicht das Audio; beim erneuten Öffnen werden die Dateien neu geladen. Verschobene oder
gelöschte Dateien werden übersprungen, und die Engine oder das Pad nutzt ihren eingebauten Klang.
