# Leistung (p35)

[English](../en/PERFORMANCE.md)

![Die CPU-Anzeige auf der Beschriftungszeile der Modus-Spalte](../images/ohpe-cpu-meter-de.png)

Ohpe berechnet vier Parts, ihre Racks, den Master und das Display auf dem Prozessor deines Rechners. Diese Seite
erklärt, was das kostet, wie du die CPU-Anzeige liest, was Ohpe tut, um leicht zu bleiben, und wie du selbst misst.

## Die CPU-Anzeige

`CPU 12 %` auf der Beschriftungszeile der Modus-Spalte (rechts, neben MODUS) zeigt, wie viel der Zeit jedes
Audioblocks Ohpe braucht, um seinen Klang zu berechnen – gemittelt über etwa eine halbe Sekunde. 100 % wäre die ganze
Zeit: Der Ton würde aussetzen. Der Tooltip nennt zusätzlich den langsamsten Block der letzten zwei Sekunden (zwei
Sekunden gehalten, dann fallend). Die Beschriftung wird rot ab 80 % im Mittel oder wenn ein Block länger als seine
eigene Zeit brauchte.

* Sie misst nur Ohpe (die Verarbeitung des Plug-ins selbst), nicht den Host oder andere Plug-ins.
* Sie ist nur eine Anzeige: kein Parameter, nicht automatisierbar, wird nicht gespeichert.
* Kurze Spitzen direkt nach dem Laden eines Presets oder Projekts sind normal (neue Klänge werden einmal vorbereitet).

## Was Ohpe tut, um leicht zu bleiben

| Maßnahme | Wirkung |
|---|---|
| **Stille Parts schlafen** | Ein Part, dessen Stimmen 4,5 s nicht gespielt haben und dessen Ausgang (nach Resonanzkörper, FX-Rack und 3D-Stufe) die ganze Zeit unter −120 dBFS blieb, berechnet seine Effektkette nicht mehr, bis wieder eine Note beginnt. Die Wartezeit ist länger als die längste Verzögerungsleitung aller Effekte – kein Echo- oder Hall-Ausklang wird je abgeschnitten, und eine Note weckt den Part im selben Block. |
| **Schnellere FM-Engine** | Die Operatoren nutzen einen Polynom-Sinus statt des Bibliotheksaufrufs, nur die Verbindungen, die ein Algorithmus wirklich hat, werden summiert, Operatoren mit Pegel 0 sparen ihren Sinus, und jeder Operator wird über einen ganzen Steuerblock auf einmal berechnet. Der Klang bleibt gleich (Tests vergleichen ihn mit der bisherigen Engine). |
| **Leichtere Effekte** | Die Leitungsmodulation des Halls und der Sweep des Phasers nutzen denselben Polynom-Sinus. Mixer-Sends auf 0 und ausgeschaltete Ausgänge kosten nichts. |
| **Nur geänderte Parameter** | Pro Audioblock liest der Kern nur die Parameter, die sich geändert haben (statt aller), und das Plug-in reicht nur geänderte Host-Werte weiter. Automation erreicht den Klang genau wie vorher. |
| **Gemeinsame eingebaute Klänge** | Eingebautes Sample-Material (die Grain-Textur, der Klang des Samplers) gibt es einmal pro Prozess, nicht pro Part oder Instanz: Eine zweite Instanz startet in wenigen Millisekunden. |

## Zahlen

Gemessen mit `ohpe_bench` (unten) bei 48 kHz, Block 256, 2 s pro Szenario, bester von 3 Durchläufen, auf einem
64-Bit-Linux-Rechner mit 2 Kernen. „Vorher“ ist der Stand vor diesem Durchgang (0.4.0 plus die seitdem
zusammengeführten Funktionen), „nachher“ dieser Durchgang; beide liefen direkt nacheinander auf demselben Rechner.
Prozent eines CPU-Kerns – weniger ist besser. Spitzen schwanken von Lauf zu Lauf stärker als Mittelwerte; für den
Vergleich zählen die Mittelwerte.

| Szenario | Vorher Mittel % | Nachher Mittel % | Änderung | Vorher Spitze % | Nachher Spitze % |
|---|---:|---:|---:|---:|---:|
| `idle` | 1,89 | 1,62 | −14 % | 2,05 | 2,08 |
| `typical.fm` | 12,76 | 7,46 | −42 % | 13,29 | 9,96 |
| `typical.wavetable` | 7,89 | 7,73 | −2 % | 8,47 | 9,22 |
| `typical.analog` | 6,94 | 6,79 | −2 % | 7,56 | 8,73 |
| `typical.pluck` | 6,59 | 6,44 | −2 % | 7,13 | 7,66 |
| `typical.grain` | 8,76 | 8,60 | −2 % | 9,48 | 10,26 |
| `typical.vocal` | 7,59 | 7,36 | −3 % | 8,30 | 8,34 |
| `typical.cluster` | 8,47 | 8,27 | −2 % | 9,06 | 10,05 |
| `typical.sampler` | 3,70 | 3,47 | −6 % | 4,52 | 4,06 |
| `worst.fm` | 27,15 | 20,98 | −23 % | 27,78 | 24,86 |
| `worst.wavetable` | 20,37 | 20,15 | −1 % | 21,02 | 21,49 |
| `worst.analog` | 18,86 | 18,67 | −1 % | 19,49 | 20,19 |
| `worst.pluck` | 21,04 | 20,90 | −1 % | 21,84 | 24,84 |
| `worst.grain` | 22,15 | 21,90 | −1 % | 22,90 | 23,54 |
| `worst.vocal` | 21,85 | 21,62 | −1 % | 22,61 | 22,56 |
| `worst.cluster` | 17,56 | 17,25 | −2 % | 18,45 | 18,03 |
| `worst.sampler` | 22,57 | 16,27 | −28 % | 23,54 | 17,16 |
| `one−part.fm` | 12,15 | 4,95 | −59 % | 12,80 | 10,27 |
| `silent−racks` | 9,11 | 1,61 | −82 % | 9,66 | 1,75 |
| `morph.4x4` | 30,09 | 25,02 | −17 % | 30,98 | 26,68 |
| **Summe** | **287,5** | **247,1** | **−14 %** | | |

Start (Kern aufbauen und vorbereiten): etwa 370 ms für die erste Instanz in einem Prozess und 18 ms für jede weitere
(nach diesem Durchgang; der frühere Benchmark hat den Start nicht gemessen).

Szenarien: *idle* = Standardklang, keine Noten; *typical* = 4 Parts × 8 Stimmen einer Engine, 2 Effekte pro Part;
*worst* = 4 Parts × 16 Stimmen plus Layer, 6 Effekte pro Part; *one-part.fm* = ein FM-Part spielt, drei stille Parts
mit vollen Racks; *silent-racks* = keine Noten, 6 Effekte auf jedem Part, Ausklänge längst vorbei; *morph.4x4* = Morph
an mit vier Klängen in jedem Part.

Was das praktisch heißt: Ein typisches Projekt mit vier spielenden Parts braucht auf diesem Rechner deutlich weniger
als ein Zehntel eines Kerns; Parts, die du nicht nutzt, kosten fast nichts, sobald ihre Ausklänge verklungen sind.

## Selbst messen

`ohpe_bench` wird mit den Kern-Tests gebaut (`cmake --build build --target ohpe_bench`, es landet in
`build/tests/core/`). Es berechnet die Szenarien nur mit dem Kern – ohne Plug-in, ohne Oberfläche – und gibt pro
Szenario Mittelwert und langsamsten Block aus.

| Option | Bedeutung |
|---|---|
| (keine) | alle Szenarien, 48 kHz, Block 256, je 2 s, bester von 3 Durchläufen |
| `--quick` | kürzere Durchläufe (wie in der CI) |
| `--block N` / `--rate HZ` | andere Blockgröße / Abtastrate |
| `--seconds S` / `--repeats N` | längere / mehr Durchläufe |
| `--filter TEXT` | nur Szenarien, deren Name TEXT enthält |
| `--csv` / `--list` | maschinenlesbare Ausgabe / nur die Namen der Szenarien |

Vergleiche nur Zahlen vom selben Rechner. Die CI gibt bei jedem Build zur Information einen kurzen Lauf in der
Zusammenfassung aus; nichts hängt von Zeiten ab. Das Werkzeug endet mit Code 2, wenn ein Szenario eine nicht endliche
oder davonlaufende Ausgabe erzeugt (eine Korrektheitsprüfung).

## Tipps

* Größere Puffer im Host senken die Last pro Block und machen Spitzen unwahrscheinlicher.
* Stimmen kosten am meisten: Stimmenzahl pro Part und Layer erhöhen die Last ungefähr im gleichen Maß.
* Lange Hall- und Delay-Fahnen halten einen Part wach, bis ihr Ausklang unter −120 dBFS fällt.
