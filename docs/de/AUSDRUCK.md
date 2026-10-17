# Ausdrucksstark spielen – Anschlag und Aftertouch (p33)

[English](../en/EXPRESSION.md)

![Die Ausdrucks-Seite: Aftertouch auf Vibrato und Cutoff](../images/ohpe-expression-de.png)

Jeder Part hat eine kleine **Ausdrucks**-Seite: wie fest du anschlägst (**Anschlag**) und wie fest du auf eine
gehaltene Taste drückst (**Aftertouch**), bewegt jeweils bis zu **zwei musikalische Funktionen** – mit Menge und Kurve.
So reagiert ein Klang schnell auf deine Hände, ohne Verbindungen in der Modulationsmatrix zu bauen.

## Wo sie liegt

* Modus **Hüllkurve**, die letzten drei Seiten: **Anschlag** (Ziel 1, Menge 1, Ziel 2, Menge 2), **Aftertouch**
  (ebenso) und **Ausdruckskurven** (Anschlagskurve 1 / 2, Aftertouch-Kurve 1 / 2). Das Display zeigt das Ansprechen
  jedes Slots als Linie (links = leise / kein Druck, rechts = hart / voller Druck) und einen Punkt, der deinem Spiel
  folgt.
* **Profi-Ansicht** (Synth): die Gruppe **Ausdruck** mit allen zwölf Reglern.
* Alles ist Host-Parameter und per MIDI zuweisbar: `pN.expr.vel.dst1` … `pN.expr.at.curve2`
  ([PARAMETERS.md](../PARAMETERS.md#p33--expressive-playing)).

## Quellen

| Quelle | Was sie liest |
|---|---|
| Anschlag | die Anschlagstärke der Note, pro Stimme (jede Note eines Akkords ihre eigene) |
| Aftertouch | Kanaldruck, polyphoner Aftertouch und MPE-Druck gleichermaßen – pro Stimme, wo der Controller ihn pro Note sendet. Gehaltener Kanaldruck gilt auch für neue Noten. |

## Funktionen

| Funktion | Was sie bewegt |
|---|---|
| Cutoff | Filter / Klang der Engine (Analog: Cutoff; Wavetable, Grain, Sampler, Cluster: Ton; Pluck: Helligkeit; Vokal: Formant; FM: der Charakter HELL) |
| Resonanz | die Filterresonanz (Analog, Wavetable, Cluster; Pluck: Körper) |
| Pegel | der Pegel jeder Stimme |
| Klangfarbe | die eigene Helligkeit der Engine pro Stimme (siehe [Klangfarbe](MODULATION.md#ziele)) |
| Amp Attack / Amp Decay / Amp Release | die Lautstärke-Hüllkurve (z. B. leise Noten schwellen langsamer an) |
| Mod-Hüllkurve Menge | wie stark die Mod-Hüllkurve wirkt |
| LFO Tempo / LFO Tiefe | der LFO des Parts |
| Vibrato | ein Vibrato mit 5,5 Hz auf der Tonhöhe – ±1 Halbton bei 100 % (unabhängig von den LFO-Einstellungen) |
| Glide-Zeit | die Portamento-Zeit (zusätzlich zum Glide-Parameter oder einer CC-5-Zeit) |
| FM-Menge | FM-Index (FM), FM-Menge (Wavetable) |
| Position | Wavetable- / Grain-Position, Vokal (Vokal-Engine), Sample-Start, Anschlagpunkt |
| Grain-Streuung | Streuung (Grain), Jitter (Vokal), Drift (Cluster), sonst der Charakter LEBENDIG |
| Morph X / Morph Y | das [Morph-Pad](MORPH-RAUM.md) |
| Drive | Analog-Drive, FM-Feedback, Pluck-Anregung, Vokal-Spannung, sonst der Charakter HART |
| FX-Mischung | die Mischung des ersten FX-Rack-Slots des Parts |
| Sub-Pegel | der Suboszillator (die SUB-Ebene des Parts, sobald es sie gibt, sonst der Sub von Analog / Wavetable) |
| Panorama | das Panorama jeder Stimme |
| Tonhöhe | die Tonhöhe jeder Stimme (±12 Halbtöne bei 100 %) |
| Leben | die [Leben](BEDIENUNG.md#leben)-Menge des Parts |

Eine Funktion, die die aktive Engine nicht hat, bleibt dort stumm (z. B. Resonanz bei FM).

## Menge und Kurve

* **Menge** −100 … +100 %: wie weit sich die Funktion bei vollem Anschlag / Druck bewegt, über ihren ganzen Bereich
  (ein Cutoff bewegt sich in Oktaven). Negative Mengen bewegen sie in die andere Richtung (härter = dunkler, mehr
  Druck = kürzerer Attack …). **0 % (ab Werk) bewirkt nichts** – der Klang ist genau wie ohne die Seite.
* **Kurve**: Linear, Exponentiell (erst kräftiges Spiel bewegt wirklich), Logarithmisch (schon leichte Berührung
  bewegt), S-Kurve, Stufen.

## Voreinstellungen und Werksklänge

Die Slots stehen ab Werk auf musikalischen Zielen, damit nur noch die Menge aufgedreht werden muss: Anschlag 1 →
Cutoff, Anschlag 2 → Amp Attack, Aftertouch 1 → Vibrato, Aftertouch 2 → Cutoff – alle auf 0 %.

Werksklänge mit Glide (Portamento) nutzen **Aftertouch → Glide-Zeit** mit positiver Menge: **mehr Druck = längeres,
langsameres Gleiten**. Wer sich beim Legato-Spiel in die Taste lehnt, macht aus einem knappen Rutscher einen
langsamen, gesanglichen Bogen; leichte Berührung hält die Linie straff. (Ein längeres Gleiten unter Druck fühlte sich
natürlicher an als umgekehrt: Der Druck liegt meist schon an, wenn die nächste Note beginnt, also dehnt sich das
Gleiten genau dann, wenn man in die Phrase hineindrückt.)

## Ausdruck und Glide-Druck

Aftertouch kann das Gleiten auf zwei Wegen formen; werden beide genutzt, wirken sie zusammen:

- **Glide-Druck** (`pN.glide.press`, Profi-Ansicht, Gruppe **Glide**, siehe [MIDI](MIDI.md#mpe-midi-polyphonic-expression))
  *skaliert* die Glide-Zeit: bei vollem Druck bis zu 4× länger (positiv) oder bis zu gar keinem Gleiten (negativ). Er
  braucht keinen Slot und hält das Verhältnis zum Glide-Parameter bzw. zur CC-5-Zeit.
- **Ausdruck → Glide-Zeit** (ein Aftertouch-Slot hier) *addiert* Zeit über die Modulationsmatrix, mit eigener Menge
  und Kurve, und lässt sich neben einer zweiten Aftertouch-Funktion nutzen.

Sind beide eingestellt, wird zuerst der Ausdrucks-Anteil addiert und das Ergebnis dann vom Glide-Druck skaliert – das
Gleiten wird also doppelt gedehnt. Jeder gleitende Werksklang nutzt genau einen der beiden Wege (derzeit den
Ausdrucks-Slot; ein Test prüft das); für eigene Klänge besser einen wählen oder beide bewusst kombinieren.

## Wie es funktioniert

Jeder der vier Slots ist eine weitere Verbindung der [Modulationsmatrix](MODULATION.md) des Parts – gleiche Quellen,
Kurven, Skalierung und Glättung wie die 16 Verbindungen der Mod-Ansicht (Quelle Anschlag oder Notendruck, für
Vibrato die Vibrato-Quelle). Die Matrix hat dafür die Quelle **Vibrato (5,5 Hz)** (32) und die Ziele **Glide-Zeit**,
**Amp Attack**, **Amp Decay** und **Amp Release** (85 – 88) bekommen, auch in der Mod-Ansicht nutzbar.
