# Bedienung

*Deutsch · English: [../en/USER-INTERFACE.md](../en/USER-INTERFACE.md)*

Die Oberfläche von Ohpe: Engine-Tasten (links), Display (Mitte), Modus-Tasten (rechts), vier
Encoder unter dem Display, Transport- und Oktavtasten, die Klaviatur. In der Kopfzeile liegen die
**Part-Auswahl**, der **Preset-Streifen** und die Tasten **BIBLIOTHEK** und **PRO**. Jede Funktion
ist mit der Maus und – über MIDI Learn – vom Controller aus erreichbar.

## Display: Balken ziehen

Die vier Zeilen im Display zeigen, was die vier Encoder steuern. Die Zeilen sind selbst Regler:

| Aktion | Ergebnis |
|---|---|
| Zeile ziehen (nach rechts oder oben = mehr) | ändert den Wert relativ zum bisherigen |
| Klick auf den Balken | springt an diese Stelle, dann weiterziehen |
| **Shift**, **Cmd** (macOS) oder **Strg** beim Ziehen | Feinregelung: ein Zehntel der Geschwindigkeit. Die Taste darf während des Ziehens gedrückt oder losgelassen werden; der Wert springt nie. |
| Doppelklick | zurück auf den Standardwert |
| Rechtsklick | MIDI Learn / Zuordnung löschen / Zurücksetzen |

Die Encoder verhalten sich genauso: ziehen (fein mit Shift / Cmd / Strg zu Beginn des Ziehens),
Doppelklick setzt zurück, Rechtsklick öffnet MIDI Learn. Die kleine Überschrift über den Zeilen
nennt die Seite (*Makros*, *Amp-Hüllkurve*, *Spur* …); die Pfeile in der Display-Kopfzeile blättern.

## Parts

Ohpe hat vier Parts, jeder mit eigener Engine und eigenem Klang. Die **Part-Auswahl** (1–4,
Kopfzeile) wählt den **bearbeiteten Part**: Engine-Tasten, die Seiten Synth, Hüllkurve, LFO und
FX-Slot, die Bibliothek (Part-Presets laden dorthin) und die Bildschirm-Klaviatur (sie spielt auf
dem MIDI-Kanal des Parts) folgen ihr. Das Display zeigt den Part als gelbes Schild (P1–P4).
MIDI: *Bearbeiteter Part* (`ui.part`).

## Samples für Sampler und Grain

Ist auf dem bearbeiteten Part die Sampler- oder Grain-Engine gewählt, zeigt das Display
**Sample laden** und **Leeren** sowie den Namen der geladenen Datei. Eine **Audiodatei** (WAV, AIFF,
FLAC, Ogg) kann auch aus dem Dateimanager auf das Instrument **gezogen** werden: Sie geht an die
Grain-Engine des Parts, wenn Grain gewählt ist, sonst an seinen Sampler (der Part wechselt zum
Sampler). Jeder Part hat eigene Samples. Die Pfade werden mit dem Projekt gespeichert.

## Profiansicht

![Profiansicht](../images/ohpe-pro-view-de.png)

**PRO** öffnet die Profiansicht: **alle Parameter des aktuellen Modus auf einmal**, in Gruppen –
im Synth-Modus die Makros und alle weiteren Parameter der Engine sowie Hüllkurven, LFO, Tonhöhe
und MIDI des Parts; im Tape-Modus Transport, Schleife, Bandcharakter und alle acht Spuren; im
Drum-Modus das Kit und alle 24 Pads. Die Reiter oben wechseln den Modus. Jeder Parameter ist ein
Balken: ziehen (fein mit Shift / Cmd / Strg), klicken zum Springen, Doppelklick setzt zurück,
Rechtsklick für MIDI Learn. Die Ansicht nutzt so viele Spalten, wie das Fenster breit ist – ein
größeres Fenster zeigt mehr auf einmal – und scrollt, wenn ein Modus mehr Parameter hat als passen.

## Sequencer-Ansichten und Parameter-Locks

![Step-Locks](../images/ohpe-step-locks-de.png)

Im Sequencer-Modus wählen die Reiter im Display die Ansicht: **Schritt**, **Euklid**, **Arp**,
**Orbit** und **Sync**. Euklid und Orbit zeichnen ihre Ringe / Spuren; Arp und Sync zeigen eine
Zusammenfassung.

Schritt-Ansicht:

* **Klick** auf einen Schritt schaltet ihn an oder aus (die vier Reiter links wählen die Schritte
  1–16 … 49–64).
* **Shift-Klick** (oder Rechtsklick) auf einen Schritt **wählt ihn zum Bearbeiten**. Das Display
  zeigt *LOCK SCHRITT n*, die Encoder bearbeiten den Schritt: **Note, Anschlag, Gate,
  Wahrscheinlichkeit** (Seite 1) und **Ratchet** (Seite 2).
* Solange ein Schritt gewählt ist, in einen anderen Modus wechseln (Synth, Hüllkurve, LFO, Mixer,
  Effekte, Drum) und einen Encoder drehen oder einen Balken ziehen: der Wert wird **auf diesem
  Schritt gelockt**, statt den Parameter zu ändern. Gelockte Werte tragen eine eckige Marke im
  Display und am Encoder. Ein Schritt hält bis zu vier Locks; Doppelklick auf einen gelockten Wert
  entfernt diesen Lock.
* Schritte mit Locks tragen kleine Marken an der Oberkante (eine pro Lock); der gewählte Schritt
  ist umrandet.
* **Locks löschen** entfernt alle Locks des Schritts; **Fertig**, ein Klick auf das *LOCK*-Schild,
  Esc oder ein zweiter Shift-Klick beenden das Bearbeiten.

## Tape-Seite

![Tape-Seite](../images/ohpe-tape-de.png)

Der Tape-Modus zeigt die acht Spuren mit ihrem Inhalt, die Abspielposition, den Schleifenbereich
(gelb) und die aufnahmebereiten Spuren (roter Punkt). Klick wählt eine Spur. Encoder-Seiten:

| Seite | Encoder |
|---|---|
| Transport | Wiedergabe, Aufnahme, Geschwindigkeit (Varispeed 0,5–2×), Rückwärts |
| Spur | Aufnahmebereit, Quelle (Eingang, Part 1–4, Drum), Pegel, Direktausgang der gewählten Spur |
| Schleife | Schleife an/aus, Start, Ende, Overdub |
| Bandcharakter | Sättigung, Wow, Flutter, Rauschen |

Soft-Tasten im Display:

| Taste | Aktion |
|---|---|
| **Heben** | kopiert Schleifenstart … -ende der gewählten Spur in die Zwischenablage und lässt Stille |
| **Schneiden** | nimmt Schleifenstart … -ende der Spur in die Zwischenablage und schließt die Lücke (späteres rückt vor) |
| **Einsetzen** | legt die Zwischenablage an die Abspielposition (überschreibt) |
| **Einfügen** | fügt die Zwischenablage an der Abspielposition ein (späteres rückt nach hinten) |
| **Rückgängig / Wiederholen** | unbegrenztes Rückgängig für Aufnahmen und Schnitte |
| **\|◀** | zurück an den Bandanfang |
| **Export** | rendert Master + die acht Spuren als **24-Bit-WAV** in einen wählbaren Ordner (offline, schneller als Echtzeit; das laufende Audio wird nicht unterbrochen) |

Jede Bandbearbeitung ist per MIDI steuerbar: *Tape-Bearbeitungsspur* (`tape.edit.track`), *Tape
Heben / Schneiden / Einsetzen / Einfügen / Rückgängig / Wiederholen / zum Anfang* (`tape.edit.*`).

## Drum-Kit-Seite

![Drum-Kit-Seite](../images/ohpe-drum-de.png)

Der Drum-Modus zeigt alle 24 Pads. Klick spielt und wählt ein Pad; die Encoder bearbeiten es –
Modell, Stimmung, Abklingen, Klangfarbe (Seite 1), Pegel, Panorama, Choke-Gruppe, Quelle (Seite 2)
und Lautstärke und Grundnote des Kits (Seite 3). **Laden** legt eine Audiodatei auf das gewählte Pad
(oder eine Datei auf ein Pad ziehen), **Leeren** gibt dem Pad seinen Werksklang zurück.

## Tasten

| Taste | Aktion |
|---|---|
| Esc | schließt die Bibliothek (Abbrechen) oder die Profiansicht oder beendet das Schritt-Bearbeiten |
| Enter (Bibliothek) | gewähltes Preset behalten und schließen |
| ↑ / ↓ (Bibliothek) | voriges / nächstes Preset der Liste |

Siehe auch: [Preset-Bibliothek](PRESETS.md) · [Sequencer](SEQUENZER.md) · [Band, Mixer und FX](BAND-MISCHER-FX.md) · [MIDI](MIDI.md).
