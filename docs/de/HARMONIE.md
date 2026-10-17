# Harmonie-Hirn

*Deutsch · English: [../en/HARMONY.md](../en/HARMONY.md)*

![Die Harmonie-Ansicht mit der Raster-Tastatur](../images/ohpe-harmony-de.png)

Ohpe hört zu, was du spielst, und führt ein **Skalen- und Akkord-Gedächtnis für alle vier Parts** – das
*Harmonie-Hirn*. Spiele Akkorde auf einem Part (Tasten, MIDI, ein Sequenzer, der Arpeggiator), und die anderen
Parts können ihnen **folgen**: ein Bass, der nur Akkordtöne trifft, eine Melodie, die die Tonart nicht verlässt, ein
Arpeggio, das mit deinen Wechseln mitgeht – ohne eine Tonart von Hand einzustellen. Eine **isomorphe
Raster-Tastatur** mit MPE spielt es vom Bildschirm.

## Was das Hirn hört und behält

* **Wer gehört wird**: jede Note, die einen Part erreicht, dessen *Folgen* auf **Aus** steht – die führenden Parts.
  Folgende Parts werden nicht gehört (keine Rückkopplung). Das Drumkit wird nicht gehört.
* **Akkord**: Sobald zwei oder mehr verschiedene Töne gehalten werden, benennt das Hirn den Akkord – Dur, Moll,
  vermindert, übermäßig, sus2, sus4, maj7, m7, 7 oder eine leere Quinte; der tiefste Ton entscheidet zwischen zwei
  Lesarten (A–C–E über A ist *Am*). Ein Akkord zählt, wenn er 40 ms stand (die Töne eines gespielten Akkords kommen
  nacheinander). Er **bleibt im Gedächtnis**, wenn du loslässt, bis zum nächsten Akkord. Lässt du einen Ton eines
  Dreiklangs los, bleibt der Akkord.
* **Tonart**: Ein Tonklassen-Gedächtnis sammelt jede Note (ihren Anschlag und wie lange sie gehalten wird) und
  verblasst mit einer Halbwertszeit von **Gedächtnis** Takten (1 – 32, ab Werk 8). Das Hirn wählt die passendste Dur-
  oder Molltonart; eine andere Tonart muss deutlich gewinnen, bevor es wechselt. Solange nichts gehört wurde, ist die
  Tonart die globale **Tonart / Skala** (`key.root`, `key.scale`).
* **Verlauf**: die letzten acht Akkorde, der neueste zuerst.

## Folgen

Jeder Part hat **Folgen** (`harmony.pN.follow`):

| Folgen | Die Noten des Parts … |
|---|---|
| **Aus** | … spielen wie bisher (und werden vom Hirn gehört) – neutral, ab Werk |
| **Skala** | … rasten auf den nächsten Ton der gemerkten Tonart |
| **Akkord** | … rasten auf den nächsten Ton des gemerkten Akkords (vor dem ersten Akkord: der Tonika-Dreiklang) |
| **Grundton** | … rasten auf den Grundton des Akkords (in jeder Oktave) |

Gerastet wird auf den nächsten Ton (bei Gleichstand nach unten) – Live-Tasten, Host-MIDI und alle Sequenzer gleich.
Der Ein-Finger-**Akkord** des Parts funktioniert weiter und baut seinen Akkord in der gemerkten Tonart. Noten eines
Parts mit **MPE** behalten ihre Tonhöhe (Ausdruck je Note); dort hilft das Raster-Layout *In Tonart*.

## Bedienelemente

| Element | ID | Wirkung |
|---|---|---|
| **Folgen** (je Part) | `harmony.pN.follow` | Aus / Skala / Akkord / Grundton |
| **Gedächtnis** | `harmony.memory` | Halbwertszeit des Tonart-Gedächtnisses in Takten (1 – 32) |
| **Halten** | `harmony.hold` | friert das Gedächtnis ein: Neue Noten ändern weder Akkord noch Tonart |
| **Tonart übernehmen** | `harmony.take` | Auslöser: schreibt die gemerkte Tonart in die globale Tonart / Skala |
| **Vergessen** | `harmony.clear` | Auslöser: vergisst Tonart, Akkord und Verlauf |
| **Raster-Anordnung** | `harmony.kb.layout` | Quarten / Terzen / Ganztöne / In Tonart |

Alle sind Host-Parameter: automatisierbar und per MIDI-Learn zuweisbar (Rechtsklick). Das Gedächtnis (Tonart,
Akkord, Verlauf, Tonklassen-Gewichte) wird in Projekten und im Plug-in-Zustand gespeichert; Part-Presets tragen
*Folgen* weder mit noch ändern sie es.

## Die Harmonie-Ansicht

LFO-Modus, fünfter Reiter **Harmonie**: die gemerkte **Tonart** und der **Akkord** (er leuchtet beim Wechsel auf),
zwölf Balken für das Tonklassen-Gedächtnis ab dem Grundton der Tonart (Tonart-Töne hell, andere gedämpft,
**Akkordtöne violett**, **gehaltene Töne grün**, der Grundton **gelb** markiert) und die **Verlaufs**-Chips. Die
Leiste sagt, was das Hirn tut und ob der Edit-Part folgt. Softkeys: **Tonart übernehmen**, **Halten**,
**Vergessen**. Regler: Folgen (Edit-Part), Gedächtnis, Halten, Raster-Anordnung.

## Die Raster-Tastatur

![Die vier Anordnungen der Raster-Tastatur, die unteren 12 Spalten](../images/ohpe-grid-layouts-de.svg)

Die Taste **RASTER** (zwischen OKTAVE + und MIDI, gelber Balken, solange sie an ist) tauscht die Klaviertasten gegen
ein Raster aus **4 Reihen × 24 Feldern**. Es ist eine **isomorphe** Tastatur. Dieser Abschnitt erklärt sie von Grund
auf – an Musiktheorie brauchst du nur das Zählen von Halbtönen.

### Die Idee: eine Form, jede Tonart

Auf dem Klavier sehen C-Dur und D-Dur verschieden aus: andere weiße und schwarze Tasten, eine andere Handhaltung.
Auf einem isomorphen Raster liegt jedes Feld immer gleich weit von seinen Nachbarn: ein Schritt nach **rechts** ist
immer dasselbe Intervall, ein Schritt nach **oben** ebenso. Darum ist ein **Intervall, ein Akkord oder eine Skala eine
Form**, und die Form bleibt gleich, wenn du sie verschiebst. Lerne die Form eines Dur-Akkords einmal, und du spielst
jeden Dur-Akkord, indem du dieselbe Form woanders hinlegst. Verschieben heißt **transponieren**.

Zwei Begriffe für unten: Ein **Halbton** ist der kleinste Schritt (C → Cis, die nächste Klaviertaste); ein
**Intervall** ist der Abstand zweier Töne in Halbtönen (große Terz = 4, Quinte = 7, Oktave = 12).

### Einschalten

* **RASTER** zeigt das Raster an Stelle der Klaviertasten; noch einmal drücken zeigt wieder das Klavier. Ohpe merkt
  sich, was zu sehen war.
* Es spielt den **Edit-Part** (den mit den Part-Tasten gewählten) und wechselt mit, wenn du einen anderen Part wählst.
* **OKTAVE − / +** verschieben es wie die Klaviatur: Das Feld unten links ist das C der untersten Klavier-Oktave (ab
  Werk C3; C0 … C7). Die Anzeige zeigt `RASTER` und den tiefsten und höchsten Ton des Rasters.
* Die Anordnung ist der Parameter **Raster-Anordnung** (`harmony.kb.layout`): LFO-Modus, Ansicht **Harmonie**,
  Regler-Seite *Harmonie*. Wie jeder Parameter ist er automatisierbar und per MIDI steuerbar. Er gilt global (für
  alle Parts gleich).
* Raster ausblenden, Part oder Oktave wechseln lässt alle gehaltenen Felder los (keine hängenden Noten).

### Die Felder lesen

| Feld | Bedeutet |
|---|---|
| hell | ein Ton der **Tonart**, die sich das Harmonie-Hirn merkt (solange es nichts gehört hat: die globale Tonart / Skala) |
| graphit | ein Ton außerhalb der Tonart |
| fetter Name mit Oktave (z. B. **C3**) | der **Grundton** der Tonart |
| violetter Balken | ein Ton des **Akkords**, den sich das Hirn merkt |
| eingesunken, roter Balken | ein Feld, das du hältst |

Die Notennamen folgen der Sprache: Cis / H auf Deutsch, C# / B auf Englisch. **Wo** du ein Feld triffst, bestimmt die
Anschlagstärke: der untere Rand spielt leise (Velocity 56), der obere Rand voll (127).

### Die vier Anordnungen

Die Schritte, genau wie der Code sie rechnet (Reihe 1 = unterste Reihe, Spalte 1 = links):

| Anordnung | Ein Feld nach rechts | Eine Reihe nach oben | Umfang in der Werks-Oktave |
|---|---|---|---|
| **Quarten** (ab Werk) | +1 Halbton | +5 Halbtöne (eine Quarte) | C3 – D6 |
| **Terzen** | +1 Halbton | +4 Halbtöne (eine große Terz) | C3 – H5 |
| **Ganztöne** | +2 Halbtöne (ein Ganzton) | +5 Halbtöne (eine Quarte) | C3 – Cis8 |
| **In Tonart** | +1 Stufe der Skala der Tonart | +3 Stufen der Skala | C3 – G7 (C-Dur) |

**Quarten** – wie die tiefen Saiten von Bass oder Gitarre: Jede Reihe beginnt eine Quarte über der darunter.

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3    H3     C4    Cis4    D4    Dis4    E4     F4
2 |  F3    Fis3    G3    Gis3    A3    Ais3    H3     C4
1 |  C3    Cis3    D3    Dis3    E3     F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**Terzen** – Reihen eine große Terz auseinander: kompakte Akkorde, kleine Griffe.

```
4 |  C4    Cis4    D4    Dis4    E4     F4    Fis4    G4
3 | Gis3    A3    Ais3    H3     C4    Cis4    D4    Dis4
2 |  E3     F3    Fis3    G3    Gis3    A3    Ais3    H3
1 |  C3    Cis3    D3    Dis3    E3     F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**Ganztöne** – zwei Halbtöne pro Feld; jede Reihe für sich ist eine Ganztonleiter, die nächste Reihe (eine Quarte
höher) hält die anderen sechs Töne. Großer Umfang auf wenig Platz.

```
4 | Dis4    F4     G4     A4     H4    Cis5   Dis5    F5
3 | Ais3    C4     D4     E4    Fis4   Gis4   Ais4    C5
2 |  F3     G3     A3     H3    Cis4   Dis4    F4     G4
1 |  C3     D3     E3    Fis3   Gis3   Ais3    C4     D4
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**In Tonart** – nur die Töne der gemerkten Tonart, ein „falscher" Ton ist also nicht möglich. Ein Feld nach rechts ist
der nächste Ton der Skala, eine Reihe nach oben sind drei Skalenstufen. Was daraus im Code folgt:

* Das Feld unten links ist der **erste Tonart-Ton auf oder über dem C** der Oktave (in D-Dur: Cis3); der Grundton
  liegt also nicht immer in der Ecke – such das fette Feld.
* In einer siebentönigen Tonart sind drei Stufen überall eine **Quarte**, nur von der vierten Stufe aus ein Tritonus
  (in C-Dur F → H). In einer fünftönigen Tonart (Pentatonik) sind drei Stufen mehr als eine Quarte.
* Ist die Skala der Tonart *Chromatisch*, fällt In Tonart auf Halbtöne zurück: +1 nach rechts, +3 nach oben (eine
  kleine Terz).
* Wechselt das Hirn die Tonart, beschriftet sich das Raster sofort neu; In-Tonart-Felder spielen dann die Töne der
  neuen Tonart.

```
4 |  E4     F4     G4     A4     H4     C5     D5     E5
3 |  H3     C4     D4     E4     F4     G4     A4     H4
2 |  F3     G3     A3     H3     C4     D4     E4     F4
1 |  C3     D3     E3     F3     G3     A3     H3     C4
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

### Beispiele zum Nachspielen

Die Bilder zeigen die unteren linken 4 × 8 Felder in der Werks-Oktave (Tonart C-Dur); Felder in `[ ]` werden
gedrückt. Jede Form funktioniert überall auf dem Raster – läuft sie über einen Rand, nimm dieselbe Form an anderer
Stelle oder verschiebe die Oktave.

**Dur-Dreiklang (C – E – G).** Quarten: Grundton, vier Felder rechts (die Terz), dazwischen eine Reihe höher (die
Quinte).

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3    H3     C4    Cis4    D4    Dis4    E4     F4
2 |  F3    Fis3   [G3]   Gis3    A3    Ais3    H3     C4
1 | [C3]   Cis3    D3    Dis3   [E3]    F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

Derselbe Akkord hat eine zweite Form: Terz eine Reihe höher und ein Feld links, Quinte eine Reihe höher und zwei
rechts (hier F – A – C):

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3    H3     C4    Cis4    D4    Dis4    E4     F4
2 |  F3    Fis3    G3    Gis3   [A3]   Ais3    H3    [C4]
1 |  C3    Cis3    D3    Dis3    E3    [F3]   Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**Moll-Dreiklang (C – Es – G).** Nur die Terz wandert: ein Feld links der großen Terz (drei Felder rechts vom
Grundton).

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3    H3     C4    Cis4    D4    Dis4    E4     F4
2 |  F3    Fis3   [G3]   Gis3    A3    Ais3    H3     C4
1 | [C3]   Cis3    D3   [Dis3]   E3     F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**Powerchord / Quinte (C – G – C).** Grundton, eine Reihe höher und zwei rechts (die Quinte), das Feld darüber (die
Oktave).

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3    H3    [C4]   Cis4    D4    Dis4    E4     F4
2 |  F3    Fis3   [G3]   Gis3    A3    Ais3    H3     C4
1 | [C3]   Cis3    D3    Dis3    E3     F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**Oktave (C3 → C4).** In Quarten immer **zwei Reihen hoch, zwei Felder rechts**:

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3    H3    [C4]   Cis4    D4    Dis4    E4     F4
2 |  F3    Fis3    G3    Gis3    A3    Ais3    H3     C4
1 | [C3]   Cis3    D3    Dis3    E3     F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**Dur-Tonleiter (C D E F G A H C).** Drei Töne pro Reihe, ein Muster, das sich wiederholt:

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3   [H3]   [C4]   Cis4    D4    Dis4    E4     F4
2 | [F3]   Fis3   [G3]   Gis3   [A3]   Ais3    H3     C4
1 | [C3]   Cis3   [D3]   Dis3   [E3]    F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

**Transponieren.** Verschieb die ganze Form: zwei Felder rechts = zwei Halbtöne höher. Aus C-Dur wird D-Dur (D – Fis –
A) mit denselben Fingern:

```
4 | Dis4    E4     F4    Fis4    G4    Gis4    A4    Ais4
3 | Ais3    H3     C4    Cis4    D4    Dis4    E4     F4
2 |  F3    Fis3    G3    Gis3   [A3]   Ais3    H3     C4
1 |  C3    Cis3   [D3]   Dis3    E3     F3   [Fis3]   G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

Eine Reihe höher transponiert um das Intervall der Reihe (in Quarten eine Quarte: C-Dur → F-Dur).

**Dieselben Akkorde in den anderen Anordnungen** (Abstand als *Felder rechts, Reihen hoch* vom Grundton; negativ =
links):

| Form | Quarten | Terzen | Ganztöne | In Tonart |
|---|---|---|---|---|
| Dur-Dreiklang | Terz (4, 0) oder (−1, 1), Quinte (2, 1) | Terz (0, 1), Quinte (3, 1) | Terz (2, 0), Quinte (1, 1) | Terz (2, 0), Quinte (1, 1) – Dur oder Moll, wie die Tonart es will |
| Moll-Dreiklang | Terz (3, 0) oder (−2, 1), Quinte (2, 1) | Terz (3, 0) oder (−1, 1), Quinte (3, 1) | Terz (−1, 1), Quinte (1, 1) | dieselbe Form wie oben |
| Quinte | (2, 1) | (3, 1) | (1, 1) | (1, 1) – vermindert von der 7. Stufe |
| Oktave | (2, 2) | (0, 3) oder (4, 2) | (1, 2) | (1, 2) |

Terzen, Dur-Dreiklang (C – E – G) und Moll-Dreiklang (C – Es – G):

```
4 |  C4    Cis4    D4    Dis4    E4     F4    Fis4    G4
3 | Gis3    A3    Ais3    H3     C4    Cis4    D4    Dis4
2 | [E3]    F3    Fis3   [G3]   Gis3    A3    Ais3    H3
1 | [C3]   Cis3    D3    Dis3    E3     F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

```
4 |  C4    Cis4    D4    Dis4    E4     F4    Fis4    G4
3 | Gis3    A3    Ais3    H3     C4    Cis4    D4    Dis4
2 |  E3     F3    Fis3   [G3]   Gis3    A3    Ais3    H3
1 | [C3]   Cis3    D3   [Dis3]   E3     F3    Fis3    G3
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

Ganztöne, Dur-Dreiklang (C – E – G) und Moll-Dreiklang (D – F – A):

```
4 | Dis4    F4     G4     A4     H4    Cis5   Dis5    F5
3 | Ais3    C4     D4     E4    Fis4   Gis4   Ais4    C5
2 |  F3    [G3]    A3     H3    Cis4   Dis4    F4     G4
1 | [C3]    D3    [E3]   Fis3   Gis3   Ais3    C4     D4
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

```
4 | Dis4    F4     G4     A4     H4    Cis5   Dis5    F5
3 | Ais3    C4     D4     E4    Fis4   Gis4   Ais4    C5
2 | [F3]    G3    [A3]    H3    Cis4   Dis4    F4     G4
1 |  C3    [D3]    E3    Fis3   Gis3   Ais3    C4     D4
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

In Tonart ist die Tonleiter einfach eine Reihe von links nach rechts, und **eine** Dreiklang-Form spielt jeden Akkord
der Tonart: ab C ist es C-Dur, ein Feld nach rechts D-Moll (D – F – A) – Dur oder Moll entscheidet die Tonart.

```
4 |  E4     F4     G4     A4     H4     C5     D5     E5
3 |  H3     C4     D4     E4     F4     G4     A4     H4
2 |  F3    [G3]    A3     H3     C4     D4     E4     F4
1 | [C3]    D3    [E3]    F3     G3     A3     H3     C4
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

```
4 |  E4     F4     G4     A4     H4     C5     D5     E5
3 |  H3     C4     D4     E4     F4     G4     A4     H4
2 |  F3     G3    [A3]    H3     C4     D4     E4     F4
1 |  C3    [D3]    E3    [F3]    G3     A3     H3     C4
  +--------------------------------------------------------
      1      2      3      4      5      6      7      8   
```

### Spielen mit MPE

Was das Raster sendet, hängt davon ab, ob für den Edit-Part **MPE** an ist (`pN.mpe`, siehe [MIDI](MIDI.md#mpe-midi-polyphonic-expression)).

**MPE an** – jede Berührung (Maus oder Finger, bis zu 10 gleichzeitig) ist eine eigene Note mit eigenem Ausdruck:

* Jede Berührung bekommt einen freien **Mitgliedskanal** (2 – 16, reihum). Sie beginnt mit Bend in der Mitte und
  Slide auf 64.
* **Seitlich schieben = Pitchbend** nur dieser Note. Der Bend folgt den Feldern: eine Feldbreite biegt um den Schritt
  der Anordnung nach rechts – 1 Halbton in Quarten und Terzen, 2 in Ganztönen, etwa 1,7 (12 / 7) in In Tonart. Er
  geht als 14-Bit-Pitchbend hinaus, skaliert mit dem **MPE-Bend-Bereich** des Parts (`pN.mpe.bend`, ab Werk 48
  Halbtöne), und endet am Ende dieses Bereichs. Die Note bleibt die angeschlagene; Loslassen beendet sie.
* **Hoch / runter = Slide** (CC 74, Mitte 64): anderthalb Feldhöhen nach oben erreichen 127, nach unten 0. Der Part
  schickt es an sein Klangfarben-Ziel (`pN.mpe.timbre`).
* **Druck** (von einem druckempfindlichen Touchscreen oder Stift; eine Maus hat keinen) geht als Kanaldruck auf dem
  Kanal der Note hinaus und wirkt auf das Druck-Ziel des Parts (`pN.mpe.press`).
* MPE-Noten behalten ihre Tonhöhe: Skala, Akkord und Folgen rasten sie nicht.

**MPE aus** – die Noten spielen auf dem MIDI-Kanal des Parts und laufen durch den Part wie die Klaviertasten (Skala,
Ein-Finger-Akkord, Folgen, der Arpeggiator, wenn er diesen Part spielt). Schieben bewirkt nichts; es gibt keinen
Ausdruck je Note.

In beiden Fällen hört das Harmonie-Hirn die Noten des Rasters (sie formen die gemerkte Tonart und den Akkord). Wie die
Klaviertasten geht das Raster nicht an den MIDI-Ausgang.

### Später: dieselben Anordnungen auf einem Grid-Controller

Ein geplantes Controller-Profil (**Profil 1**) lässt ein externes Pad-Raster genauso spielen wie das Raster auf dem
Bildschirm:

* **dieselben Anordnungen** – Quarten, Terzen, Ganztöne und In Tonart, mit denselben Schritten nach rechts und oben;
  jede Form auf dieser Seite funktioniert dort genauso;
* **Tonart- / Skalen-Sperre** – In Tonart spielt nur Tonart-Töne; die anderen Anordnungen können die Tonart auf den
  Pads zeigen;
* **dieselbe Belegung** – ein Pad spielt den Ton des Felds in derselben Spalte und Reihe, ab demselben tiefsten C;
* **dieselben Farben** – Grundton, Tonart-Töne, andere Töne, Akkordtöne und gedrückte Pads leuchten wie die Felder
  hier;
* **Oktave / Shift** – Tasten am Controller verschieben es wie OKTAVE − / +.

Bis dahin ist das Raster auf dem Bildschirm die Referenz: Was du hier lernst, gilt dort unverändert.

## Modulation

Drei Matrix-Quellen (in jedem Part derselbe Wert):

| # | Quelle | Bereich | |
|---|---|---|---|
| 29 | **Akkord-Grundton** | 0 … 1 | der Grundton des Akkords, C = 0 … H = 1 |
| 30 | **Akkordwechsel** | 0 … 1 | 1 bei jedem neuen Akkord, verblasst in etwa 0,3 s – ein Filter-Akzent bei jedem Wechsel |
| 31 | **Spannung** | 0 … 1 | Anteil der gehaltenen Töne außerhalb der Tonart (geglättet) |
