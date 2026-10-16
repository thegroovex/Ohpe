# Resonanzkörper (p19)

*Deutsch · English: [../en/RESONATOR.md](../en/RESONATOR.md)*

Jeder Part kann durch einen **physikalisch modellierten Körper** spielen: eine Saite, eine Platte oder
ein Rohr, das der Klang der Engine anregt. Ein Zupfer schlägt eine gestimmte Metallplatte an, Rauschen
streicht eine Bohrung, eine Fläche weckt Resonanzsaiten, die auf den gehaltenen Akkord gestimmt sind.
Der Körper folgt deiner Tastatur – Note, Akkord, Pitchbend und MPE-Ausdruck –, er ist also ein
Instrument und nicht nur ein Effekt.

![Die Körper-Ansicht](../images/ohpe-reso-view-de.png)

## Schnellstart

1. **Effects** drücken, dann den Reiter **Körper** im Display.
2. **Resonanzkörper** einschalten (Encoder 1, erste Seite). Der Part spielt jetzt durch einen
   Saitenkörper, der auf jede gespielte Note gestimmt ist.
3. Den **Körper** wählen (Encoder 2): *Saite*, *Platte*, *Offenes Rohr* oder *Gedacktes Rohr*.
4. **Körper-Mischung** einstellen (Encoder 4): 0 = der trockene Part (exakter Bypass), 1 = nur der Körper.

Oder eines der Resonanzkörper-Factory-Presets laden: *Plucked Plate*, *Gamelan Plate*, *Bowed Tube*,
*Breath Bore*, *Sympathetic Pad*, *Drone Strings*, *Glass Body Keys*.

## Wo er sitzt

Der Körper ist die erste Stufe nach Engine und Layer: **Engine → Resonanzkörper → Lautstärke →
FX-Rack → Bass mono → 3D-Raum → Mischer**. Die Effekte des Racks färben den Körper also wie Mikrofon und
Raum. Derselbe Körper ist auch ein Effekttyp des Racks, **Resonanzkörper** (EN *Resonator*), für jeden
Slot eines Part-Racks oder des Master-Racks – etwa nach einer Verzerrung, einem Hall oder einem zweiten
Körper. Im Part-Rack folgt er der Tastatur dieses Parts, im Master-Rack bleibt er auf seiner festen
Stimmung.

## Die Modelle

| Körper | Was er ist | Klang |
|---|---|---|
| **Saite** | zwei gekoppelte Saiten (ein leicht verstimmtes Paar, das am Steg Energie austauscht), Steifigkeit für gespreizte Teiltöne, frequenzabhängige Dämpfung | gezupfte und angeschlagene Saiten, Schwebung und zweistufiges Ausklingen wie ein Klavier-Chor |
| **Platte** | 48 Moden einer rechteckigen Platte, an einem Punkt angeregt, an zweien abgenommen (natürliches Stereo) | Glocken, Gongs, Metall- und Glasplatten, Gamelan |
| **Offenes Rohr** | eine an beiden Enden offene Bohrung (alle Obertöne) | Flöten, Pfeifen, hauchige Orgeltöne |
| **Gedacktes Rohr** | eine an einem Ende geschlossene Bohrung (ungerade Obertöne, Viertelwellen-Resonanz) | klarinettenartig, hohl, holzig |

Jeder Körper kann bis zu sechs **Resonanzsaiten** dazunehmen, die von seiner Schwingung angeregt
werden: Sie klingen auf Oktaven und Quinten der Note mit oder auf den Tönen des gehaltenen Akkords – und
klingen nach dem Loslassen weiter (in der Stimmung *Akkord* behalten sie den letzten Akkord).

## Parameter

| Seite | Encoder 1 | Encoder 2 | Encoder 3 | Encoder 4 |
|---|---|---|---|---|
| Resonanzkörper | Resonanzkörper (an / aus) | Körper (Modell) | Stimmung (Fest, Note, Akkord) | Körper-Mischung |
| Körper-Stimmung | Körper-Stimmung (± 24 Halbtöne) | Größe | Material | Ausklang (0,05 – 20 s) |
| Körper-Charakter | Position | Inharmonizität | Resonanzsaiten | Körper-Breite |
| Drive & Ausdruck | Drive | Druck auf Körper | Slide auf Position | – |

* **Stimmung** – *Note*: Der Körper folgt der neuesten Note (plus Pitchbend, Pro-Noten-Bend und Stimmung
  des Parts); *Akkord*: wie *Note*, und die Resonanzsaiten stimmen sich auf die gehaltenen Tasten;
  *Fest*: C3 plus **Körper-Stimmung**, egal was du spielst.
* **Größe** – Saite: wie viele Resonanzsaiten (2 – 6) und wie weit das Saitenpaar verstimmt ist;
  Platte: ihre Proportionen (quadratisch und licht … lang und dicht); Rohre: die Weite der Bohrung
  (weiter = innen dunkler, mehr abgestrahlte Höhen).
* **Material** – wie die Höhen ausklingen: niedrig = weiche, dunkle Materialien (Darm, Holz), hoch =
  Stahl, Bronze, Glas.
* **Ausklang** – wie lange der Grundton klingt (T60). Höhere Teiltöne klingen schneller aus, außer bei
  hohem Material.
* **Position** – wo der Körper angeregt wird: nahe am Saitenende (dünn, hell) oder in der Mitte (rund,
  hohl); auf der Platte ändert sie, welche Moden sprechen; beim Rohr die Stelle entlang der Bohrung.
* **Inharmonizität** – Saite: Steifigkeit (obere Teiltöne zu hoch, klavier- und glockenartig); Platte:
  das Modenmuster von fast harmonisch (niedrig) über die ideale Platte (Mitte) bis gespreizt (hoch);
  Rohre: Abweichungen der Bohrung.
* **Drive** – eine weiche Sättigung der Anregung und, bei Saite und Rohren, in der Schleife (sie fügt nie
  Energie hinzu); auf der Platte steigen die Moden, wenn sie laut klingt (gongartiges Gleiten).
* **Druck auf Körper** – MPE- / Aftertouch-Druck der neuesten Note: positiv = klingt länger und wird
  stärker angeregt, negativ = Druck dämpft den Körper (wie Abdämpfen mit dem Handballen).
* **Slide auf Position** – MPE-Slide (CC 74) verschiebt den Anregungspunkt.

Alle Parameter sind automatisierbar und per MIDI zuweisbar. Die Modulationsmatrix hat die Ziele
**Körper-Stimmung**, **Körper-Größe**, **Körper-Material**, **Körper-Ausklang**, **Körper-Position** und
**Körper-Mischung** (75 – 80). Beim Rack-Typ sind *FX-Parameter 1 – 4* Körper-Stimmung, Material,
Ausklang und Position.

## Das Display

Die Körper-Ansicht zeichnet den Körper auf das dunkle Display: das Saitenpaar zwischen seinen Stegen mit
den Resonanzsaiten darunter, die Platte mit ihrem bewegten Modenmuster oder die Bohrung mit ihrer
stehenden Welle (geschlossenes Ende als Kappe, offene Enden strahlen ab). Der gelbe Punkt ist der
Anregungspunkt; die Bewegung wächst mit dem Pegel. Die Zeile unter den Reglern zeigt Modell, Stimmung und
die Tonhöhe, auf die der Körper gestimmt ist. Ein Resonanzkörper-Slot zeigt dasselbe Bild in der
Rack-Ansicht.

## Klang und Sicherheit

* Stabil bei jeder Einstellung: Jede Schleife verliert Energie, die Kopplung ist verlustfrei, die
  Sättigung verstärkt nie, Zustände werden geleert (keine Denormals) und überwacht; der Ausgang des Körpers
  läuft durch einen Spitzenbegrenzer (nie über Vollaussteuerung).
* **Körper-Mischung 0** oder **Resonanzkörper aus** ist ein exakter Bypass: Der Part klingt bitgenau wie
  ohne Körper. Ein- und Ausschalten blendet über 20 ms über.
* Ein Körper pro Part (wie ein echtes Instrument einen Korpus für alle Saiten hat), daher wachsen die
  Kosten nicht mit der Stimmenzahl.
