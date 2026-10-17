# Performance-Pads

*Deutsch · English: [../en/PADS.md](../en/PADS.md)*

![Die Pads-Ansicht](../images/ohpe-pads-view-de.png)

Vier **Performance-Pads** werfen Effekte auf die ganze Hauptsumme – die Griffe für ein Live-Set, einen Aufbau oder
einen Break: ein **Stottern**, ein **Bandstopp**, ein **Filter-Sweep** und ein **Hall-Freeze**. Pad halten, und es
spielt; loslassen, und die Summe ist zurück. Die Pads rasten im Takt ein: Ein gedrücktes Pad wartet bei laufendem
Transport auf die nächste Rasterlinie der **Quantisierung**, damit ein Stottern immer ein sauberes Stück erwischt.

## Die Pads

| Pad | Was es tut | Einstellungen |
|---|---|---|
| **Stottern** | fängt ab der Rasterlinie ein Stück der Summe und wiederholt es, solange gehalten | *Stotter-Länge*: 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T |
| **Bandstopp** | die Summe wird langsamer bis zum Stillstand – Tonhöhe und Tempo fallen zusammen wie bei einem anhaltenden Band; danach Stille | *Bandstopp-Dauer*: 1/8, 1/4, 1/2, 1 Takt, 2 Takte |
| **Sweep** | ein resonanter Tiefpass schließt (bis ~150 Hz) oder ein Hochpass öffnet (bis ~8 kHz) über die Sweep-Dauer; beim Loslassen fährt er in einem Viertel der Zeit zurück | *Sweep-Filter* (Tiefpass / Hochpass), *Sweep-Dauer* (1/8 … 4 Takte), *Sweep-Resonanz* |
| **Freeze** | wirft die nächste Viertelsekunde der Summe in einen Hall und hält sie endlos, solange gehalten – unter der weiterlaufenden trockenen Summe; nach dem Loslassen klingt die Fläche in etwa drei Sekunden aus | *Freeze-Pegel* |

Die Pads lassen sich stapeln: Stottern → Bandstopp → Sweep → Freeze. Eine eingefrorene Fläche unter einem
schließenden Tiefpass, ein Stottern, das dann per Bandstopp anhält – alles zugleich geht.

## Spielen

1. Modus **Effekte**, sechster Reiter **Pads**.
2. Ein Pad im Display **drücken und halten**: Es spielt, solange du es hältst. Ein violett blinkender Rand heißt, es
   wartet auf das Raster; grün heißt, es spielt.
3. **Umschalt-Klick** auf ein Pad oder sein **Softkey** rastet es ein; noch einmal löst es.
4. **Rechtsklick** auf ein Pad für MIDI-Lernen: Lege es auf einen Taster deines Controllers (Taster gedrückt = Pad an).

**Quantisierung** (Aus, 1/16, 1/8, 1/4, 1/2, 1 Takt; Standard 1/16) bestimmt das Raster. Bei stehendem Transport
oder Quantisierung Aus setzt ein Pad sofort ein. Loslassen wirkt immer sofort, mit kurzer Blende. Die Zeiten folgen
dem Tempo.

## Wo sie sitzen

Die Pads wirken auf die **Hauptsumme** in der Master-Kette: nach dem Master-FX-Rack, vor Master-Kompressor,
Bandsättigung und Limiter – der Limiter hält einen resonanten Sweep oder einen dichten Freeze im Zaum. Kanäle auf
den Aux-Ausgängen bleiben unberührt. Ist kein Pad gehalten und klingt nichts nach, läuft die Summe unverändert durch.

## Automation, MIDI, Modulation

Jedes Pad und jede Einstellung ist ein Host-Parameter (`pads.*`, siehe [PARAMETERS.md](../PARAMETERS.md#performance-pads-p25)):
Automatisiere die Würfe in deiner DAW oder lege sie per MIDI-Lernen auf Taster. Die Modulationsmatrix bekommt zwei
Quellen – **Pad-Sweep** (die Sweep-Position, 0 … 1) und **Pads gehalten** (1, solange ein Pad spielt) –, so kann ein
Part etwa sein eigenes Filter gegen den Master-Sweep öffnen oder seinen Hall-Send absenken, solange ein Pad gehalten wird.

Die vier Pads selbst sind Performance: Projekt-Presets behalten die Einstellungen, aber nicht, ob ein Pad gehalten war.
