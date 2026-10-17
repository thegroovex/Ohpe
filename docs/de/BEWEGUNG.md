# Bewegungs-Recorder

*Deutsch · English: [../en/MOTION.md](../en/MOTION.md)*

![Die Bewegungs-Ansicht](../images/ohpe-motion-view-de.png)

Jeder Part hat einen **Bewegungs-Recorder**: Dreh an seinen Reglern, während der Transport läuft, und Ohpe nimmt
die Bewegungen als **Schleifen-Spuren** auf – einen Filter-Sweep, ein anschwellendes Resonanz-Spiel, eine gestufte
Verstimmung – und spielt sie im Takt der Musik ab. Die Schleife hat ihre **eigene Länge** (1 – 64 Sechzehntel):
Eine Spur von 5 Schritten gegen ein Pattern von 16 Schritten wandert durch den Takt und trifft sich nach fünf
Takten wieder – polymetrische Bewegung ganz ohne Programmieren.

## Aufnehmen

1. LFO-Modus, vierter Reiter **Bewegung** (die vier Spuren des Edit-Parts).
2. Softkey **Rec** drücken (oder Regler 2 drehen) – die Spuren warten, der Abspielkopf wird rot, sobald der
   Transport läuft.
3. **START** drücken und an einem beliebigen Regler des Parts drehen: Der erste **stufenlose** Parameter, den du
   berührst, bekommt Spur 1, der nächste Spur 2 … bis zu vier Spuren. Solange du einen Regler hältst oder bewegst,
   folgt ihm seine Spur (die Zeile leuchtet rot) und du hörst den Regler; lässt du los, spielt die Spur, was du
   aufgenommen hast.
4. Denselben Regler später noch einmal anfassen = **Überspielen**: Nur der Teil der Schleife, den du berührst,
   wird ersetzt.
5. **Rec** erneut drücken beendet die Aufnahme – **Start** bleibt an.

Lücken zwischen zwei Bildschirm-Aktualisierungen werden weich überbrückt. Auswahl-, Schalter- und
Ganzzahl-Parameter (Engine, Filtertyp, Stimmen …), die Bewegungs-Einstellungen selbst und das MIDI-Setup des Parts
werden nie aufgenommen. Ändern sich viele Parameter auf einmal (Preset laden, ein Zuchtschritt), wird nichts
aufgenommen.

## Abspielen

| Bedienelement | Wirkung |
|---|---|
| **Start** (`pN.motion.on`) | spielt die Spuren als Schleife mit dem Transport (auch während der Aufnahme an) |
| **Rec** (`pN.motion.rec`) | nimmt Berührungen in die Spuren auf |
| **Länge** (`pN.motion.len`) | Schleifenlänge in Sechzehnteln, 1 – 64 (acht Punkte je Sechzehntel); gilt für alle Spuren des Parts. Eine kürzere Schleife spielt den Anfang der Aufnahme, eine längere spielt den Regler, wo nichts aufgenommen ist – nichts geht verloren |
| **Tiefe** (`pN.motion.amt`) | 0 = der eigene Wert des Reglers, 1 = die aufgenommene Bewegung; dazwischen wird die Bewegung zum Regler hin skaliert |
| **Leeren** (Softkey oder `pN.motion.clear`) | leert die vier Spuren des Parts |

Alle fünf sind Host-Parameter: automatisierbar oder per MIDI steuerbar (Rechtsklick → MIDI-Lernen) – zum
Beispiel ein Fußschalter auf Rec.

Eine Spur legt ihren Wert über den Parameter, **solange der Transport läuft**; sie liegt unter den Locks des
Step-Sequenzers (ein gelockter Schritt gewinnt) und über dem Regler, und die [Modulationsmatrix](MODULATION.md)
bewegt den Parameter weiterhin um sie herum. Der Regler auf dem Bildschirm und die Host-Automation behalten ihren
eigenen Wert. Bei gestopptem Transport, Start aus oder ohne Aufnahme klingt der Part genau wie ohne Recorder.

## Modulationsquellen

Die vier Spuren sind auch Quellen der [Modulationsmatrix](MODULATION.md): **Bewegung 1 … Bewegung 4** (0 … 1,
der aufgenommene Wert an der Abspielposition, 0, wo nichts aufgenommen ist oder der Transport steht). Nimm eine
Geste auf und lass sie auch andere Ziele bewegen – das Panorama, eine FX-Mischung, die Morph-Position.

## Speichern

Die Spuren gehören zum **Projekt**: Sie werden mit dem Plug-in-Zustand und in Projekt-Presets gespeichert.
Part-Presets enthalten die Bewegungs-Einstellungen nicht und setzen sie nicht zurück – lädst du einen anderen
Klang in einen Part, bewegen seine Spuren die Parameter des neuen Klangs weiter. Morph-Ecken und die Zucht
übergehen den Bewegungs-Block.

## Die Bewegungs-Ansicht

Vier Zeilen, eine je Spur: die Spurnummer in ihrer Reglerfarbe, der Name des Parameters (oder *Leer*) und die Spur
mit einem Sechzehntel-Raster über die Schleife (Zählzeiten heller), der aufgenommenen Bewegung als gefüllte Kurve
und dem Abspielkopf – grün beim Abspielen, rot bei der Aufnahme. Eine gerade berührte Spur hat einen atmenden roten
Rand. Die Leiste darunter sagt, was der Recorder gerade tut. Softkeys: **Rec**, **Start**, **Leeren**; Regler:
Start, Rec, Länge, Tiefe.
