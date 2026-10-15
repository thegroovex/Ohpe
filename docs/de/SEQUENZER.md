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
* **Haltebögen (0.5):** Ein Schritt mit Gate 100 % wird an den nächsten Schritt gebunden, wenn dieser
  dieselbe Note spielt – die Note klingt weiter, statt neu angeschlagen zu werden, und endet mit dem
  Gate des nächsten Schritts. Ketten von 100-%-Schritten halten eine Note (oder einen Ein-Finger-Akkord)
  beliebig lange. Nicht auf dem Drum-Kit (Pads sind One-Shots).

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
| `seq.euclid.rN.vel` (0.5) | 0–127 | Anschlag eines Rings; 0 = `seq.euclid.vel` (Akzente: laute Kick, leise Hats) |
| `seq.euclid.swing`, `.human` (0.5) | 0–75 %, 0–100 % | Swing und Humanisieren, siehe unten |

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

* **Spur-Ziel** (`seq.orbit.lN.part`, 0.5): 0 = der Ziel-Part von Orbit (bzw. das Drum-Kit); 1–4 = diese
  Spur spielt diesen Part, nie das Kit. Mit eingeschalteten Drums spielen so z. B. die Spuren 1–3 das Kit
  und Spur 4 eine Basslinie – ein Sequenzer für einen ganzen Groove.
* **Haltebögen** (0.5): Bei **Gate** 100 % (`seq.orbit.gate`) setzt ein Schritt, der die Note des
  vorigen Schritts wiederholt, diese Note fort (gehalten, nicht neu angeschlagen) – lange Bassnoten und
  Akkorde. Spuren, die das Drum-Kit spielen, werden nie gebunden.
* **Swing** und **Humanisieren** (`seq.orbit.swing`, `seq.orbit.human`, 0.5), siehe unten.

## Swing und Humanisieren (0.5)

* **Swing** (`seq.orbit.swing`, `seq.euclid.swing`, 0–75 %) verschiebt jede Note auf einer
  Offbeat-Sechzehntel um Swing ÷ 2 einer Sechzehntel nach hinten – so viel wie `seq.step.swing` des
  Step-Sequenzers (30 % ≈ 57 % Shuffle, 75 % = 3/8 einer Sechzehntel). Spuren und Raster mit 1/8 oder
  langsamer treffen nie eine Offbeat-Sechzehntel, so swingen alle Teile eines Grooves gemeinsam.
* **Humanisieren** (`seq.orbit.human`, `seq.euclid.human`, 0–100 %): Bei 100 % beginnt eine Note bis zu
  10 ms später und ihr Anschlag ändert sich um bis zu ±16. Die Abweichung ist wiederholbar (sie beginnt
  nach jedem Zurücksetzen gleich), ein Bounce klingt also wie das Gehörte.

## Sequenzer spielen das Drum-Kit (0.4)

Step-Sequenzer, Euklid und Orbit können statt ihres Ziel-Parts das **Drum-Kit** spielen:
`seq.step.drums`, `seq.euclid.drums`, `seq.orbit.drums` (aus / an). Ihre Noten gehen dann an die
Pads (Note − `drum.base` = Pad, standardmäßig C2 = Kick, D2 = Snare, F2 = geschlossene Hi-Hat …),
als kämen sie auf MIDI-Kanal 10; Skalen-Quantisierung und Akkorde wirken dabei nicht. Orbit wird so
zur Drum-Machine mit einer Spur pro Trommel und eigener Länge je Spur; Euklid spielt vier
polyrhythmische Trommeln. Die Werks-Projekt-Presets nutzen beides – siehe
[FACTORY-SOUNDS.md](FACTORY-SOUNDS.md).

## Poly-Glide (0.5)

**Glide** (`pN.glide`) lässt die Tonhöhe von Note zu Note gleiten. Mit einer Stimme (mono) gleitet
jede neue Note von der letzten. Mit **Poly-Glide** (`pN.polyglide`) und mehreren Stimmen übernimmt
jede Note eines neuen Akkords die Stimme der nächstgelegenen gerade losgelassenen Note und gleitet von
dort – Akkorde gleiten Stimme für Stimme ineinander (die *gleitenden* Flächen der Werksklänge). Ohne
losgelassene Stimmen (erster Akkord oder zu gehaltenen hinzugefügte Noten) beginnen Noten auf ihrer
eigenen Tonhöhe.

## Portamento und Tonhöhen-Hüllkurve (0.6)

Alle melodischen Engines teilen dasselbe Portamento und dieselbe Tonhöhen-Hüllkurve; beide liegen für
jede Engine auf den letzten beiden Seiten des **Synth**-Modus (*Glide*, *Tonhöhen-Hüllkurve*).

| Parameter | Wirkung |
|---|---|
| **Glide** (`pN.glide`, 0 … 2 s) | Portamento-Zeit. 0 = aus. |
| **Glide-Modus** (`pN.glide.mode`) | **Auto** (Standard, wie vor 0.6): eine Note gleitet, solange die vorige noch klingt (auch im Release). **Legato**: eine Note gleitet nur, solange eine andere Taste gehalten wird; mit einer Stimme laufen die Hüllkurven weiter (echtes Legato), abgesetzte Noten beginnen neu. **Immer**: jede Note gleitet von der zuletzt gespielten, auch nach einer Pause. |
| **Glide-Timing** (`pN.glide.rate`) | **Feste Zeit**: jedes Gleiten dauert die Glide-Zeit. **Feste Rate**: die Glide-Zeit gilt je Oktave – kleine Schritte kommen schnell an, große Sprünge gleiten länger. |
| **Poly-Glide** (`pN.polyglide`) | Akkorde gleiten Stimme für Stimme (siehe oben). |
| **Tonhöhen-Hüllkurve Menge** (`pN.pitchenv.amt`, −24 … +24 Halbtöne) | Jede neue Note beginnt so weit entfernt und gleitet in ihre Tonhöhe: negativ = **steigt** von unten, positiv = **fällt** von oben. 0 = aus. |
| **Tonhöhen-Hüllkurve Zeit** (`pN.pitchenv.time`, 5 ms … 4 s) | Wie lange das Gleiten dauert. |
| **Tonhöhen-Hüllkurve Kurve** (`pN.pitchenv.curve`) | 0 = linear, 1 = schneller Start, langsames Ankommen. |

**Mono-Notenpriorität (0.6):** mit einer Stimme kehrt das Loslassen der neuesten Taste zur vorigen,
noch gehaltenen Taste zurück – legato, zurückgleitend. **MIDI:** CC 65 schaltet Portamento aus / an,
CC 5 setzt die Glide-Zeit (siehe [MIDI.md](MIDI.md)). **Sequencer:** ein Step mit Gate 100 %
(Step-Sequenzer, Orbit) endet jetzt ein Sample nach dem Beginn des nächsten Steps; eine *andere*
nächste Note überlappt ihn also – Mono-Parts im Glide-Modus *Legato* gleiten genau dort.

Die Werksklänge enthalten *steigende* und *fallende* Flächen und Synths (Rise Strings, Rise Choir,
Rise Glass, Rise Warmth, Rise Bloom, Fall Haze, Rise Lead, Scoop Keys, Rise Bass, Zip Pluck, Fall Lead)
und Portamento-Leads und -Bässe (Legato Glide Lead, Portamento Square, Glide Sine Bass, Glide Poly Synth).

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
