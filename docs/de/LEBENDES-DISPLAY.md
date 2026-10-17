# Lebendes Display (p26)

Die fünfte Display-Ansicht, **Lebendes Objekt**, zeigt den Klang als 3D-Objekt in der linken Hälfte des Displays.
Sie hört auf das, was den Klang gerade formt, und macht es auf einen Blick sichtbar:

![Das lebende Display](../images/ohpe-living-de.png)

| Was sich bewegt | Was es bedeutet |
|---|---|
| **Größe, Leuchten** | die Lautstärke-Hüllkurve der neuesten Note und der Pegel – das Objekt schwillt beim Anschlag an, hält im Sustain und schrumpft im Release |
| **Stauchen / Strecken senkrecht** | LFO 1 |
| **Eine sich drehende Taille** | LFO 2 |
| **Neigung** | LFO 3 |
| **Wellen auf der Oberfläche** | die Modulationshüllkurve und wie stark die Modulationsmatrix gerade bewegt (mehr Tiefe = schnellere Wellen, schnellere Drehung) |
| **Breite** | die Stereobreite des Signals (Seitenanteil): Mono ist rund, breite Klänge strecken sich nach links und rechts |
| **Zwei Hälften** | die Kanäle laufen gegenphasig auseinander (Korrelation unter etwa 0,2) – das Objekt schnürt sich in eine linke und eine rechte Hälfte: eine Warnung für Mono-Wiedergabe |

Ein LFO bewegt das Objekt nur so weit, wie er genutzt wird: der Part-LFO mit seiner Tiefe, LFO 2 / 3 über die
Verbindungen der Matrix, die sie als Quelle oder *Via* nehmen. Ein frei laufender LFO, der nichts steuert, lässt das
Objekt ruhig.

Der Äquator-Ring trägt die Farbe des Parts (P1 rot, P2 gelb, P3 violett, P4 rosa). Drei kleine Balken unter dem
Objekt zeigen **Hüllkurve**, **Mod** und **Breite**. Ist der Klang verklungen, kommt das Objekt als ruhige Kugel zur
Ruhe (*Spielen, um es zu wecken*).

## Auswählen

Klick auf die linke Hälfte des Displays wechselt die Ansichten (*Figur → Stereobild → Spektrum-Landschaft → 3D-Raum →
Lebendes Objekt*), oder Rechtsklick und **Lebendes Objekt** wählen. Die Ansicht wird mit dem Projekt gespeichert.

* **Synth**-Seiten und die anderen Part-Seiten: der Bearbeitungs-Part (seine Hüllkurven, LFOs, Matrix und sein eigenes
  Signal nach seinem FX-Rack), beschriftet *P1*–*P4*.
* **Mixer**: das Master-Signal für das Stereobild (beschriftet *Master*), der Bearbeitungs-Part für die Modulation.
* Auf den Mitte/Seite-Slot-Seiten erscheint weiterhin von selbst das Stereobild; die Tape-Seite behält ihren Transport.

**Animierte 3D-Ansichten** aus (Ansichtsmenü): Das Objekt dreht sich nicht mehr und folgt dem Klang ohne Glättung.

## Sparsam mit der CPU

Das Objekt ist ein kleines Netz (9 Ringe x 28 Punkte), gezeichnet mit drei Strichen. Das Display zeichnet nur das
eigene Rechteck des Objekts (und seine Anzeigen) neu, und nur solange es sich bewegt: Ein ruhendes Objekt kostet
nichts – der Rest des Displays wird dafür nicht neu gezeichnet. Die Werte kommen aus der lock-freien UI-Rückmeldung
des Kerns und dem Scope-Abgriff; auf dem Audio-Thread wartet und alloziert nichts.

Das Objekt hat keine eigenen Host-Parameter – es ist eine Ansicht; alles, was es zeigt (Hüllkurven, LFOs, Matrix,
Breite), ist dort automatisierbar und per MIDI-Learn zuweisbar, wo es eingestellt wird.
