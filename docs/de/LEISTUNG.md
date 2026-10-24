# Leistung (p35, p79)

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
| **Limiter nur bei Bedarf** (p79) | Der Master-Limiter spart seine True-Peak-Interpolation in Blöcken, in denen keine Spitze die Grenze erreichen kann. Die Ausgabe bleibt bitgleich. |
| **Sends und Master-Rack schlafen** (p79) | Wie die Parts hören die beiden Send-Effekte und das Master-Rack auf zu rechnen, wenn Ein- und Ausgang länger als ihr längster Ausklang unter −120 dBFS blieben; eine Note weckt sie im selben Block. |
| **Keine Denormals** (p79) | Der Kern setzt winzige Gleitkommawerte beim Rechnen auf null, damit ausklingende Fahnen den Prozessor nie bremsen. |
| **Günstigere Stimmen** (p79) | Pluck berechnet sein Saitenfilter nur neu, wenn sich ein Wert ändert (bitgleich); das Fenster von Grain und der Glottis-Puls von Vocal nutzen den Polynom-Sinus; 3D-Stufe und Key Gravity rechnen einmal pro Block bzw. pro Änderung, was sie pro Sample bzw. pro Frame gerechnet haben. |
| **Ruhigeres Display** (p79) | Wenn zwei Sekunden lang nichts klingt, läuft oder sich ändert und sich auf dem Display nichts mit der Uhr bewegt (3D-Szene, Figur, Anzeigen, Scopes, blinkende Tasten), zeichnet das Display 10 statt 30 Bilder pro Sekunde. Jede Änderung holt die volle Rate sofort zurück. |

## Zahlen (p79, Weg zu 1.0)

Gemessen mit `ohpe_bench --seconds 1.5 --hash` bei 48 kHz, Block 256, bester von 3 Durchläufen, auf einem
64-Bit-Linux-Rechner mit 2 Kernen. „Vorher“ ist main vor diesem Durchgang (0.7.3), „nachher“ dieser Durchgang; beide
liefen direkt nacheinander auf demselben Rechner. Prozent eines CPU-Kerns – weniger ist besser. „Ausgabe“ vergleicht
eine Prüfsumme des berechneten Audios: *bitgleich* heißt, der Klang hat sich überhaupt nicht geändert.

| Szenario | Vorher Mittel % | Nachher Mittel % | Änderung | Vorher Spitze % | Nachher Spitze % | Ausgabe |
|---|---:|---:|---:|---:|---:|---|
| `idle` | 1,78 | 0,84 | −53 % | 2,50 | 1,07 | bitgleich |
| `typical.fm` | 7,59 | 7,25 | −4 % | 8,64 | 12,10 | bitgleich |
| `typical.wavetable` | 7,95 | 7,20 | −9 % | 20,38 | 10,14 | bitgleich |
| `typical.analog` | 6,98 | 6,06 | −13 % | 10,93 | 9,07 | bitgleich |
| `typical.pluck` | 6,66 | 3,93 | −41 % | 19,30 | 5,02 | bitgleich |
| `typical.grain` | 8,85 | 7,73 | −13 % | 23,15 | 11,58 | geändert: Polynom-Sinus (Fenster) |
| `typical.vocal` | 7,51 | 7,07 | −6 % | 8,37 | 11,39 | geändert: Polynom-Sinus (Glottis-Puls) |
| `typical.cluster` | 8,39 | 7,73 | −8 % | 9,05 | 9,66 | bitgleich |
| `typical.sampler` | 3,83 | 2,97 | −22 % | 4,55 | 4,48 | bitgleich |
| `worst.fm` | 21,13 | 21,09 | 0 % | 21,92 | 22,45 | bitgleich |
| `worst.wavetable` | 20,29 | 20,14 | −1 % | 20,96 | 21,65 | bitgleich |
| `worst.analog` | 18,83 | 15,91 | −16 % | 19,61 | 17,80 | bitgleich |
| `worst.pluck` | 21,00 | 18,14 | −14 % | 21,79 | 21,01 | geändert: Grain-Layer |
| `worst.grain` | 22,12 | 21,94 | −1 % | 23,02 | 24,87 | geändert: Polynom-Sinus |
| `worst.vocal` | 21,76 | 21,89 | +1 % | 22,51 | 24,89 | geändert: Polynom-Sinus |
| `worst.cluster` | 17,71 | 17,59 | −1 % | 18,65 | 20,82 | bitgleich |
| `worst.sampler` | 16,69 | 16,56 | −1 % | 17,55 | 19,41 | bitgleich |
| `one-part.fm` | 8,69 | 7,88 | −9 % | 11,20 | 11,20 | bitgleich |
| `silent-racks` | 1,68 | 0,67 | −60 % | 2,01 | 1,02 | bitgleich |
| `morph.4x4` | 25,68 | 23,41 | −9 % | 26,66 | 26,68 | bitgleich |
| `chord16.fm` | 3,23 | 2,64 | −18 % | 3,63 | 3,33 | bitgleich |
| `chord16.wavetable` | 3,32 | 2,40 | −28 % | 3,66 | 2,92 | bitgleich |
| `chord16.analog` | 3,01 | 2,10 | −30 % | 3,41 | 2,59 | bitgleich |
| `chord16.pluck` | 2,93 | 1,50 | −49 % | 3,31 | 1,92 | bitgleich |
| `chord16.grain` | 3,58 | 2,61 | −27 % | 4,13 | 3,26 | geändert: Polynom-Sinus |
| `chord16.vocal` | 3,18 | 2,34 | −26 % | 3,43 | 2,93 | geändert: Polynom-Sinus |
| `chord16.cluster` | 3,46 | 2,56 | −26 % | 3,75 | 3,13 | bitgleich |
| `chord16.sampler` | 2,17 | 1,28 | −41 % | 2,67 | 1,91 | bitgleich |
| `drums` | 2,13 | 1,44 | −32 % | 2,66 | 2,17 | bitgleich |
| `fx.full` | 9,73 | 8,83 | −9 % | 12,16 | 13,78 | bitgleich |
| `space3d` | 11,46 | 11,19 | −2 % | 11,99 | 13,36 | bitgleich |
| `gravity` | 10,91 | 8,72 | −20 % | 11,59 | 9,95 | geändert: Key-Gravity-Gewichte zwischengespeichert (RMS auf 6 Stellen gleich) |
| `everything` | 26,55 | 25,27 | −5 % | 27,45 | 28,46 | bitgleich |
| `silent.after-play` | 3,06 | 0,75 | −75 % | 3,46 | 2,44 | geändert: Key Gravity im Aufbau |
| `silent.master-fx` | 2,96 | 0,69 | −77 % | 3,42 | 0,79 | bitgleich |
| `silent.long-after` (neu) | – | 0,72 | – | – | 0,83 | – |
| **Summe** | **346,8** | **311,0** | **−10 %** | | | |

Start: etwa 378 ms für die erste Instanz in einem Prozess und 21 ms für jede weitere (vorher 369 / 19 ms; im Rahmen
der Messschwankung). Spitzen schwanken von Lauf zu Lauf stärker als Mittelwerte; für den Vergleich zählen die
Mittelwerte.

Die kurzen Läufe auf den CI-Rechnern aller drei Systeme (`--quick`, 0,5 s × 2; vorher = main 0.7.3, nachher = dieser
Durchgang, verschiedene Läufe auf geteilten Maschinen):

| Szenario | Linux vorher → nachher | macOS vorher → nachher | Windows vorher → nachher |
|---|---:|---:|---:|
| `idle` | 1,09 → 1,12 | 1,42 → 0,74 | 1,82 → 1,17 |
| `typical.fm` | 5,34 → 8,52 | 8,53 → 6,41 | 9,36 → 9,24 |
| `typical.pluck` | 5,68 → 4,66 | 8,24 → 3,93 | 8,10 → 4,93 |
| `typical.grain` | 6,24 → 10,09 | 8,87 → 7,81 | 14,01 → 11,88 |
| `worst.analog` | 15,31 → 19,92 | 18,72 → 16,27 | 25,26 → 21,84 |
| `silent-racks` | 0,98 → 0,76 | 2,54 → 0,60 | 1,72 → 0,88 |
| `morph.4x4` | 19,48 → 29,23 | 36,63 → 26,58 | 37,24 → 34,78 |
| `chord16.pluck` | 2,19 → 1,86 | 3,26 → 1,40 | 3,29 → 1,89 |
| `drums` | 1,54 → 1,72 | 3,96 → 1,10 | 2,60 → 1,88 |
| `gravity` | 7,59 → 10,40 | 17,58 → 11,36 | 16,58 → 13,71 |
| `everything` | 19,14 → 31,48 | 31,62 → 17,34 | 36,85 → 34,26 |
| `silent.master-fx` | 1,95 → 0,77 | 2,45 → 0,52 | 3,57 → 0,88 |
| **Summe, alle 35** | **260 → 401** (+54 %) | **372 → 313** (−16 %) | **477 → 438** (−8 %) |

Der Linux-Rechner des „Nachher“-Laufs war langsamer: bitgleiche Szenarien wie `typical.fm` stiegen genauso, die
Linux-Spalte ist also Rauschen; die stillen Szenarien sanken trotzdem. Die Spalten für macOS und Windows passen zur
lokalen Tabelle.

Neue Szenarien: *chord16.\** = ein Part, 16-Ton-Akkord auf 16 Stimmen, keine Effekte; *drums* = das Drumkit,
Sechzehntel bei 120 BPM; *fx.full* = 6 Effekte auf jedem Part, 4 Master-Effekte, beide Sends; *space3d* = bewegte
3D-Stufe mit Raum auf jedem Part; *gravity* = Key Gravity 80 % auf jedem Part; *everything* = alles zugleich;
*silent.after-play* = der volle Aufbau 1 s gespielt und losgelassen, gemessen nach 9 s; *silent.long-after* = dasselbe
nach 19 s (alles schläft); *silent.master-fx* = keine Noten, Master-Effekte und Sends.

## Zahlen (p35)

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
| `--hash` | ergänzt Prüfsumme und RMS der Ausgabe jedes Szenarios (zeigt, dass eine Optimierung den Klang nicht ändert) |
| `--baseline DATEI` / `--write DATEI` | vergleicht die Mittelwerte mit einem früheren `--csv`-Lauf / schreibt diesen Lauf als neuen |

Vergleiche nur Zahlen vom selben Rechner. Die CI misst kurz auf Linux, macOS und Windows, vergleicht mit
`tools/bench/baseline-<os>.csv`, zeigt das Ergebnis in der Zusammenfassung und lädt es als Artefakt `bench-<os>` hoch,
zur Information; nichts hängt von Zeiten ab. Das Werkzeug endet mit Code 2, wenn ein Szenario eine nicht endliche
oder davonlaufende Ausgabe erzeugt (eine Korrektheitsprüfung).

## Tipps

* Größere Puffer im Host senken die Last pro Block und machen Spitzen unwahrscheinlicher.
* Stimmen kosten am meisten: Stimmenzahl pro Part und Layer erhöhen die Last ungefähr im gleichen Maß.
* Lange Hall- und Delay-Fahnen halten einen Part wach, bis ihr Ausklang unter −120 dBFS fällt.
