# Multiband-Kompressor

*Ohpe – designed by groovex.* [English](../en/MULTIBAND-COMP.md)

**Multiband-Kompressor** (englisch *Multiband Comp*) ist ein FX-Slot-Typ (jeder Slot jedes Part-Racks und des
Master-Racks) für Druck und Dichte bei kontrolliertem Pegel – „mehr Druck“, ohne dass der Pegel davonläuft. Er teilt den
Klang in drei Bänder – **Tief**, **Mitte** und **Hoch** – zwischen zwei phasenkohärenten Linkwitz-Riley-Weichen
(4. Ordnung: die Bänder summieren sich zu einem Allpass, eine neutrale Einstellung ist im Pegel flach). Jedes Band
arbeitet in beide Richtungen:

* **Abwärts** – über seiner **Abwärts-Schwelle** wird das Band heruntergedrückt, mit sehr hoher Ratio (Vorgabe 60:1).
* **Aufwärts** – unter seiner **Aufwärts-Schwelle** wird das Band angehoben (Vorgabe 4:1, höchstens +24 dB; die
  Anhebung blendet unter -78 dBFS zum Rauschteppich hin aus, echte Stille bleibt still).

Leise Details und Ausklänge kommen nach vorn, Spitzen werden gehalten: der Klang wird dichter und klingt lauter – bei
gleichem Spitzenpegel. Jedes Band hat eigene **Attack**, **Release**, **Pegel** und **Solo**. Die Vorgaben sind sehr
schnell (Attack 2 / 1 / 0,5 ms), damit Einschwingvorgänge gefangen werden.

## Globale Regler

| Regler | Was er tut | Bereich | Vorgabe |
|---|---|---|---|
| **Tiefe** | Nass / Trocken: wie viel der bearbeiteten Bänder du hörst (pro Band angewendet, die Phase der Weichen bleibt kohärent) | 0 – 100 % | 40 % |
| **Zeit** | skaliert alle Attack- und Release-Zeiten gemeinsam | 0 – 1000 % | 100 % |
| **Aufwärts** | wie viel der Aufwärts-Hälfte wirkt | 0 – 100 % | 100 % |
| **Abwärts** | wie viel der Abwärts-Hälfte wirkt | 0 – 100 % | 100 % |
| **Eingang** | treibt die Bänder in die Schwellen | −24 – +24 dB | 0 dB |
| **Ausgang** | Ausgangspegel | −24 – +24 dB | 0 dB |
| **Trennung tief / hoch** | die Trennfrequenzen | 40 – 1000 Hz / 1 – 12 kHz | 120 Hz / 2,5 kHz |

Pro Band (Tief, Mitte, Hoch): Schwelle abwärts (−60 – 0 dB), Schwelle aufwärts (−80 – 0 dB), Ratio abwärts und Ratio
aufwärts (1:1 – 100:1), Attack (0,1 – 200 ms), Release (5 – 2000 ms), Pegel (−24 – +24 dB), Solo.

**Ausgangssicherung:** eine latenzfreie Stufe am Ende (sofortiger Attack, 50 ms Release, Grenze -1,5 dBFS
Sample-Spitze – etwa -1 dBTP) greift nur, wenn der Ausgang darüber ginge; ein roter Punkt über OUT im Editor zeigt es.

**Einschwingen:** jeder Band-Detektor hat denselben Einschwing-Wächter wie der Kompressor (siehe unten): nach einer
leisen Stelle wird der erste Schlag sofort gefangen – weder die Anhebung der leisen Stelle noch eine ausgeklungene
Verstärkung lassen ihn durch.

## Im FX-Editor

Drei Band-Anzeigen, Pegel −80 – 0 dBFS von unten nach oben:

* der Pegel des Bands jetzt (violett), seine Pegeländerung jetzt am rechten Rand (gelb = angehoben, rot =
  heruntergedrückt);
* die Zone über der **Abwärts-Schwelle** (rot getönt) und unter der **Aufwärts-Schwelle** (gelb getönt); der rechte
  Punkt eines Bands ist die Abwärts-Schwelle, der linke die Aufwärts-Schwelle; Alt-Ziehen oder das Mausrad auf einem
  Punkt ändern seine Ratio (unter dem Bandnamen angezeigt);
* die Zeile unter jedem Band ist sein Pegel; die Trennfrequenzen stehen zwischen den Bändern; OUT rechts (Tiefe auf
  seinem Mausrad / Alt-Ziehen). Alle anderen Regler sind Balken neben dem Bild.

**STIL** (neben dem Slotnamen) lädt einen Startpunkt: **Squash** (alles auf einmal, dicht und laut), **Glue** (sanfter
Bus-Kleber), **Luft & Gewicht** (Tiefen und Höhen angehoben, Mitten zurück), **Punch** (langsamere Attacks lassen den
Anschlag durch, der Körper wird gehalten), **Dichte** (vor allem aufwärts) und **Sanft** (leichte Hand für Keys und
Pads). Ein Stil setzt die Parameter des Effekts wie eine normale Bearbeitung (Undo / Automation des Hosts).

Alle Parameter sind im Host automatisierbar und MIDI-zuweisbar; Matrix FX-Param 1 – 4 = Tiefe, Zeit, Abwärts,
Aufwärts. Parameter-IDs: [PARAMETERS](../PARAMETERS.md#multiband-comp-p88).

## Die Kompressoren fangen den ersten Schlag (p88)

Der Rack-Typ Kompressor, der Kanalkompressor und der Master-Kompressor teilen einen Detektor. Er behält den gehaltenen
Spitzen-Detektor (kein Pegelflattern auf einem Bass) und bekommt einen **Einschwing-Wächter**: ein schneller
Spitzenpfad mit sofortigem Attack und gleichem Release folgt der statischen Kennlinie; die angewendete Pegelreduktion
ist nie kleiner als die des Wächters abzüglich eines kleinen Transienten-Spielraums (0,3 dB pro ms Attack, höchstens
3 dB). Vorher ließ ein in der Stille ganz ausgeklungener Kompressor den ersten Schlag einer Note oder eines Akkords mit
der ganzen Aufholverstärkung durch (+24 dB Aufholverstärkung: Akkorde bis +14 dBTP). Das Release ist
programmabhängig: ein einzelner Schlag lässt bis zu 3,3× schneller los, anhaltende Kompression nutzt das eingestellte
Release. Es kommt keine Latenz dazu; der gehaltene Klang bleibt, wie er war.
