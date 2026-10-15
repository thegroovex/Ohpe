# Sequencer, Parts und Skalen-Modus

*Deutsch · English: [docs/en/SEQUENCERS.md](../en/SEQUENCERS.md)*

Ohpe hat **vier Parts**, die gleichzeitig spielen – jeder mit eigener Engine, Hüllkurven, LFO
und Mixer-Kanal (Part 1–4 → Mixer-Kanal 1–4, jeweils auf Haupt oder einen eigenen
Stereo-Ausgang routbar). Vier Sequencer steuern sie. Jeder Sequencer hat einen **Ziel-Part**,
ein typisches Setup:

| Part | Engine (Beispiel) | Gesteuert von |
|---|---|---|
| 1 | FM | Step-Sequencer (`seq.step.channel` = 1) |
| 2 | Analog | Euklid (`seq.euclid.part` = 2) |
| 3 | FM | Arpeggiator (`seq.arp.part` = 3), von der Tastatur gespielt |
| 4 | Analog | Orbit (`seq.orbit.part` = 4) |

Alle Sequencer laufen am selben Transport (interne Clock, Host, MIDI-Clock oder Link – siehe
[MIDI-Leitfaden](MIDI.md)) und starten und stoppen mit **Sequencer Start** (`seq.run`). Ihre
Schritte liegen auf absoluten Positionen des Transports: Sie bleiben untereinander und mit dem
Host in Phase, auch bei Loops und Sprüngen.

## Step-Sequencer – Pattern und Song-Modus

Der 64-Schritt-Sequencer aus 0.1 hat jetzt **16 Pattern**.

* **Pattern** (`seq.step.pattern`, 1–16) wählt das spielende Pattern. Ein Wechsel wartet, bis
  das laufende Pattern sein Ende erreicht – Wechsel landen immer auf dem Takt.
* Jedes Pattern kann eine **eigene Länge** haben (oder `seq.step.length` folgen).
* **Song-Modus** (`seq.step.song`) spielt stattdessen die **Kette**: bis zu 64 Einträge
  *Pattern × Wiederholungen*, als Schleife. Die Song-Position kommt vom Transport – Loops und
  Sprünge im Host landen auf dem richtigen Pattern und Schritt.
* Pattern, Längen und Kette werden im Zustand gespeichert. Pattern aus 0.1 öffnen als Pattern 1.

## Euklid

Vier Ringe; jeder verteilt **Impulse** so gleichmäßig wie möglich auf **Schritte** (1–32,
Bjorklund-Algorithmus), gedreht um die **Rotation**, mit eigener **Note**. Ringe
unterschiedlicher Länge bilden ein Polymetrum: Ringe mit 3 und 4 Schritten treffen sich alle
12 Schritte.

| Parameter | Bereich | |
|---|---|---|
| `seq.euclid.on` | aus / an | |
| `seq.euclid.part` | 1–4 | Ziel-Part (Standard 2) |
| `seq.euclid.rate` | 1/4 … 1/32, Triolen, punktiert | gemeinsames Raster aller Ringe |
| `seq.euclid.gate`, `.vel` | 5–100 %, 1–127 | Notenlänge, Anschlag |
| `seq.euclid.rN.steps / .pulses / .rot / .note` | je Ring 1–4 | |

Beispiele: 3 Impulse auf 8 Schritte `x..x..x.`, 5 auf 8 `x.xx.xx.`, 7 auf 16 `x..x.x.x..x.x.x.`.

## Arpeggiator

Die in den Ziel-Part gespielten Noten (Tastatur, MIDI auf dem Kanal des Parts, Ein-Finger-Akkorde)
werden gehalten und nacheinander gespielt.

* **Modus**: Aufwärts, Abwärts, Auf/Ab (Umkehrpunkte ohne Wiederholung), Zufall, Wie gespielt,
  Akkord (alle gehaltenen Noten auf jedem Schritt).
* **Oktaven** 1–4, **Gate** 5–100 %, **Raster** 1/4 … 1/32 mit Triolen (T) und punktiert (.).
* **Halten** (Latch) behält die Noten nach dem Loslassen; die nächste Taste ohne gehaltene Taste
  beginnt einen neuen Satz.
* Läuft der Transport, liegen die Schritte auf seinem Raster. Steht er, läuft der Arpeggiator
  frei im aktuellen Tempo und beginnt genau mit der ersten Taste.

## Orbit (Polymetrum)

Vier Spuren mit bis zu 16 Schritten, jede mit eigener **Länge**, eigenem **Raster** und
**Transponieren**, alle spielen in einen Part. Jede Spur hat eigene Schritte (Note, Anschlag).
Die Spuren verschieben sich gegeneinander und treffen sich erst nach dem kleinsten gemeinsamen
Vielfachen ihrer Längen wieder – lange, sich entwickelnde Muster aus wenigen Schritten.

## Skalen- und Akkord-Modus

* **Tonart** (`key.root`) und **Skala** (`key.scale`: chromatisch, Dur, Moll, Kirchentonarten,
  harmonisch und melodisch Moll, Pentatoniken, Blues, Ganzton) gelten global.
* **Skala quantisieren** (`pN.quant`) setzt jede Note, die in einen Part geht, auf den nächsten
  Ton der Skala – von Tastatur, MIDI und Sequencern.
* **Akkord** (`pN.chord`): Eine Taste spielt einen Akkord – Dreiklang, Septakkord, Sus2, Sus4,
  Quinte, Oktave, Sexte, Add9. In einer Skala sind die Akkorde leitereigen (in C-Dur spielt ein
  D d-Moll); mit der chromatischen Skala gelten feste Dur-Intervalle. Akkorde gehen in den
  Arpeggiator – ein Finger spielt einen arpeggierten Akkord.
* Das Loslassen einer Taste beendet immer genau die Noten, die sie gestartet hat, auch wenn sich
  die Einstellungen dazwischen geändert haben.

## Parameter-IDs

Alle IDs stehen in [PARAMETERS.md](../PARAMETERS.md) und in `ohpe_param_dump --markdown`.
