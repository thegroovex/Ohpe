# MIDI-Leitfaden

*Deutsch · English: [docs/en/MIDI.md](../en/MIDI.md)*

Dieser Leitfaden beschreibt, wie Ohpe MIDI empfängt und sendet: Kanäle pro Part, MPE,
MIDI-Clock, Host-Sync und Link. Das Zuweisen von Controllern zu Parametern (MIDI-Learn) steht im
Hauptbuch.

## Kanäle und Parts

Jeder Part hat einen eigenen **MIDI-Kanal** (`pN.midich`): Part 1 hört auf Kanal 1, Part 2 auf
Kanal 2 usw. `0` bedeutet **Omni** (alle Kanäle). Ein Controller auf Kanal 3 spielt also Part 3;
mehrere Parts auf demselben Kanal werden geschichtet. Die Bildschirm-Tastatur spielt auf Kanal 1.

Die Sequencer sprechen ihren Part direkt an (ihren *Ziel-Part*), unabhängig von dessen Kanal.

| Nachricht | Wirkung |
|---|---|
| Note an / aus | spielt den Part (über Skalen-/Akkord-Modus und Arpeggiator, wenn aktiv) |
| Pitchbend | verstimmt den Part (Bereich `pN.bend`) |
| CC 64 | Haltepedal |
| CC 74 | Timbre (Ziel `pN.mpe.timbre`) für alle Noten des Parts |
| Kanal-Aftertouch | Druck (Ziel `pN.mpe.press`) für alle Noten des Parts |
| CC 120 / 123 | Klang aus / alle Noten aus |
| Zugewiesene CCs und Noten | ändern ihren Parameter (MIDI-Learn) und spielen nicht |

## MPE (MIDI Polyphonic Expression)

Mit **MPE** (`pN.mpe`) spielt ein Part von einem MPE-Controller (untere Zone: Master-Kanal 1,
Member-Kanäle 2–16). Der Part hört dann auf alle Kanäle, und jede Note behält ihren eigenen
Ausdruck:

* **Pitchbend pro Note** auf dem Member-Kanal, Bereich `pN.mpe.bend` (Standard 48 Halbtöne,
  wie im MPE-Standard). Pitchbend auf dem Master-Kanal verstimmt den ganzen Part.
* **Druck** (Kanal-Aftertouch auf dem Member-Kanal oder polyphoner Aftertouch) geht zum
  **Druck-Ziel** (`pN.mpe.press`, Standard *Pegel*).
* **Timbre** (CC 74 auf dem Member-Kanal, Mitte 64) geht zum **Timbre-Ziel**
  (`pN.mpe.timbre`, Standard *Klangfarbe*).

Ziele: *Aus*, *Klangfarbe* (Helligkeit der Engine: Filter-Cutoff bei Analog,
Modulationsindex bei FM), *Pegel* (bis +6 dB), *LFO-Tiefe* (Vibrato / Tremolo pro Note),
*Panorama*.

Stelle den Pitchbend-Bereich des Controllers auf denselben Wert wie `pN.mpe.bend`.

## Tempo und Transport

| Situation | Transport-Quelle |
|---|---|
| Plug-in im Host | immer der Host: Tempo und Position (ppq), samplegenau |
| Standalone, `sync.src` = Intern | Ohpes eigene Clock (`tempo`, Start mit `seq.run`) |
| Standalone, `sync.src` = MIDI-Clock | eine externe MIDI-Clock an einem aktiven MIDI-Eingang |
| Standalone, `sync.src` = Link | die Link-Session im lokalen Netzwerk |

In jedem Fall starten und stoppen die Sequencer mit **Sequencer Start** (`seq.run`) – im Host
laufen sie, solange der Host spielt *und* `seq.run` an ist.

### Host-Sync (Plug-in)

Ohpe liest in jedem Block Tempo und Song-Position des Hosts und setzt jeden Schritt genau auf
das Sample, an dem die Zeitachse des Hosts ihn erreicht – bei jedem Tempo und jeder
Puffergröße. Loops und Sprünge im Host setzen jeden Sequencer auf den richtigen Schritt (im
Song-Modus auf das richtige Pattern).

### MIDI-Clock empfangen

Mit `sync.src` = **MIDI-Clock** folgt Ohpe 24 Clock-Ticks pro Viertelnote sowie *Start*,
*Stop*, *Continue* und *Song Position Pointer*. Das Tempo wird über die letzten Ticks gemittelt,
die Position sanft an jeden Tick herangezogen; **Clock-Glättung** (`sync.smooth`) bestimmt, wie
stark: kleine Werte reagieren sofort, große ignorieren zitternde Clocks (USB, Funk). Ein
einzelner fehlender oder doppelter Tick wird ignoriert, ein echter Tempowechsel nach wenigen
Ticks übernommen.

### MIDI senden (Standalone)

Den **MIDI-Ausgang** wählst du unter *Sync & MIDI …* im Menü der Standalone-App.

* **MIDI-Clock senden** (`sync.clockout`): 24 Ticks pro Viertelnote, solange der Transport
  läuft, *Start* (oder *Song Position* + *Continue* beim Start mitten im Song), *Stop*.
* **Sequencer-Noten senden** (`sync.notesout`): die Noten aller Sequencer, jeweils auf dem
  MIDI-Kanal ihres Parts – so spielen Ohpes Sequencer externe Instrumente.

### Link (Standalone)

Die Standalone-App kann einer **Link**-Session beitreten (Ableton Link, eine Drittanbieter-
Bibliothek, siehe [THIRD-PARTY.md](../../THIRD-PARTY.md)) – mit anderen Apps im selben Netzwerk.
Mit `sync.src` = **Link** übernimmt der Transport Tempo und Beat-Phase der Session; Ohpes Takte
liegen auf denen aller anderen. Ändert man Ohpes Tempo, ändert sich das Session-Tempo. Start und
Stop bleiben lokal. Link ist eine Build-Option (`-DOHPE_WITH_LINK=ON`, Standard an); das Plug-in
folgt immer seinem Host.

## Einstellungen im Überblick

| ID | Bereich | Standard |
|---|---|---|
| `pN.midich` | 0 (Omni) … 16 | N |
| `pN.mpe` | aus / an | aus |
| `pN.mpe.bend` | 0 … 96 Halbtöne | 48 |
| `pN.mpe.press`, `pN.mpe.timbre` | Aus, Klangfarbe, Pegel, LFO-Tiefe, Panorama | Pegel, Klangfarbe |
| `sync.src` | Intern, MIDI-Clock, Link | Intern |
| `sync.smooth` | 0 … 1 | 0,5 |
| `sync.clockout`, `sync.notesout` | aus / an | aus |
