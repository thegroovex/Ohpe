# 3D-Raum – binaurale Bewegung (1.0)

*Deutsch · English: [../en/SPACE.md](../en/SPACE.md)*

Jeder Part kann die Stereo-Linie verlassen und sich **um deinen Kopf** bewegen: vorn, seitlich, hinter
dir, über dir, nah und fern – mit Bewegungspfaden im Takt des Songs. Auf Kopfhörern rendert Ohpe den Part
**binaural**: deine Ohren bekommen die Zeit- und Pegelunterschiede, den Kopfschatten und die
Richtungsmerkmale des Ohrs einer echten Quelle an dieser Stelle. Auf Lautsprechern wird dieselbe Szene ohne
binaurale Färbung gerendert und bleibt voll monokompatibel.

![Die 3D-Raum-Ansicht](../images/ohpe-space-view-de.png)

## Schnellstart

1. **Effekte** drücken, dann den Reiter **3D-Raum** im Display (oder auf das **3D**-Abzeichen im
   Display-Kopf klicken).
2. Den **3D-Raum**-Schalter (Encoder 1, erste Seite) auf *Kopfhörer* oder *Lautsprecher* stellen – Encoder 1
   drehen oder ein Segment des Schalters im Display anklicken. Der Part steht jetzt 1,5 m vor dir.
3. Einen **Bewegungspfad** (Encoder 2) und seine **Bewegungslänge** (Encoder 3) wählen – z. B. *Kreis* über
   *2 Takte*. Play drücken: der Punkt des Parts fliegt im Display um den Kopf.
4. Für *Kopfhörer* (binaural) Kopfhörer aufsetzen; *Lautsprecher* wählen, wenn du über Lautsprecher hörst.

Oder eines der Presets mit dem Tag **3D** aus der Bibliothek laden – sie tragen ein Kopfhörer-Piktogramm. Die Werksbank hat 75 davon (72 Part-Presets und drei Projekte; 49 binaurale Showcase-Presets kamen mit p71 dazu, aufgelistet in [FACTORY-SOUNDS.md](FACTORY-SOUNDS.md#binaurale-presets-p71)).

## Das 3D-Abzeichen und das Kopfhörer-Piktogramm

| Wo | Leuchtet (ultraviolett, pulsierend) | Gelb umrandet, Lautsprecher | Gedimmt |
|---|---|---|---|
| Display-Kopf, nach dem Part-Abzeichen: Kopfhörer-Piktogramm + **3D** | der bearbeitete Part spielt binaurales 3D (Raum an oder ein *3D-Raum*-Rack-Slot, Abhöre auf Kopfhörer) | 3D ist an, die Abhöre steht auf Lautsprecher | der Part nutzt kein 3D (das schwache Piktogramm zeigt die Abhöre, die ein Einschalten nutzen würde: Kopfhörer oder Lautsprecher) |
| Part-Wahl 1–4 (oben) | dasselbe pro Part, ein kleines Kopfhörer-Piktogramm in der Ecke | ein kleines Lautsprecher-Piktogramm | nichts |
| Bibliotheksliste | Presets mit dem Tag **3D** zeigen das Kopfhörer-Piktogramm nach dem Namen; der Kategorie-Chip **3D** listet sie | | |

Mit der Maus über dem Abzeichen erscheint der Tooltip (*Binaurales 3D – am besten mit Kopfhörern*); ein
Klick öffnet die 3D-Raum-Ansicht des bearbeiteten Parts.

![Das 3D-Abzeichen im Display-Kopf](../images/ohpe-space-badge-de.png)

## Das Display: kopfzentrierte 3D-Ansicht

Du schaust dem Hörer über die Schulter: der Kopf in der Mitte (mit Kopfhörern, solange du binaural abhörst,
bei Lautsprecher-Abhöre zwei Lautsprecher davor), die Bodenringe bei 1 m, 5 m und 40 m, der Ring 45° oben,
der Bogen vorn / hinten. Jeder Part mit 3D ist ein Punkt in seiner Farbe (Part 1 rot, 2 gelb, 3 ultraviolett,
4 rosé) mit einer verblassenden **Spur** und einer **Lotlinie** zum Boden, die die Höhe zeigt. Nähere Punkte
sind größer, das Leuchten folgt dem Pegel. Oben rechts: Azimut, Elevation und Entfernung des bearbeiteten
Parts. Der dünne Balken unten ist die **Master-Korrelation** (rot = gegenphasig, siehe *Monokompatibilität*).
Die Ansicht driftet langsam und folgt leicht der Maus (Parallaxe; aus mit *Animierte 3D-Ansichten*).

Dieselbe Ansicht kann in jedem Modus das linke Bild des Displays sein: Klick auf das Bild schaltet *Figur →
Stereobild → Spektrum-Landschaft → 3D-Raum*, Rechtsklick → *3D-Raum*. Ein *3D-Raum*-Rack-Slot zeigt seine
eigene Quelle im Streifen unter dem Rack.

## Parameter – die Stufe des Parts `pN.space.*`

Die Stufe sitzt ganz am Ende des Parts: Engine → FX-Rack → Bass mono → **3D** → Mixer. Raum aus ist ein exakter
Bypass (jeder ältere Klang spielt bitgleich); Umschalten blendet über 20 ms über.

| Seite | Parameter | Bereich | Wirkung |
|---|---|---|---|
| 3D-Bewegung | 3D-Raum (`pN.space.mode`, p41) | Aus, Kopfhörer, Lautsprecher | der 3D-Schalter: *Aus* = Stufe aus; *Kopfhörer* = an, binaural; *Lautsprecher* = an, Lautsprecher-Rendering. Als Segmentschalter gezeichnet; Encoder 1 schaltet die drei Stellungen durch |
| | Bewegungspfad | Aus, Kreis, Acht, Vorbeiflug, Zufallsorbit, Pendel, Spirale aufwärts, Ping-Pong v/h, Schwarm, Flüstern, Entfernungsgang, Über den Kopf, Wellenorbit, Ping-Pong l/r, Vier Ecken, Atmen, Helix, Überflug, Hüpfen, Wandern, Schleifenorbit | siehe unten |
| | Bewegungslänge | 1/16 … 32 Takte, 1/4. | ein Durchlauf des Pfads, im Takt des Songs |
| | Bewegungstempo | ×−4 … ×4 (Standard ×1) | multipliziert das Tempo; negativ = rückwärts, 0 = steht. Bei genau ×1 rastet die Bewegung auf die Song-Position ein (sie wiederholt sich bei jedem Abspielen gleich) |
| 3D-Position | Azimut | −180 … 180° | Richtung (0 = vorn, +90 = rechts, ±180 = hinten); dreht den ganzen Pfad |
| | Elevation | −60 … 90° | Höhe; kippt den ganzen Pfad |
| | Entfernung | 0,2 … 40 m | Grundentfernung (der Pfad skaliert davon aus) |
| | Quellenbreite | 0 … 180° | wie weit linker und rechter Kanal des Parts auseinander stehen (0 = Punktquelle) |
| Bewegungsform | Bewegungsgröße | 0 … 100 % | wie groß der Pfad ist (je Pfad, siehe unten) |
| | Bewegungsphase | 0 … 360° | Versatz des Pfads – zwei Parts mit demselben Pfad um einen halben Durchlauf versetzt |
| | Bewegungsglättung | 5 ms … 2 s | Glättung jeder Positionsänderung (Automation, Modulation, Sprünge) |
| | Doppler | 0 … 100 % | Tonhöhenverschiebung bewegter Quellen (die Entfernung wird zur Verzögerung; 0 = gar keine Verzögerung) |
| Raum & Entfernung | Raumanteil | 0 … 100 % | frühe Reflexionen eines Raums, die mit der Quelle wandern, plus ein kurzer diffuser Nachhall – sie holen den Klang aus dem Kopf |
| | Raumgröße | 0 … 100 % | vom kleinen Zimmer (4 m) bis zur Halle (30 m) |
| | Luftdämpfung | 0 … 100 % | ferne Quellen verlieren Höhen |
| | Entfernungsabfall | 0 … 100 % | wie viel leiser ferne Quellen werden (100 % = 1/r-Gesetz; 1,5 m spielt mit dem Eingangspegel) |
| Abhöre | Mono unter | 40 … 300 Hz | alles darunter bleibt **mono und unbewegt** (Linkwitz-Riley-Weiche, Standard 120 Hz) – der Sub bewegt sich nie |
| | 3D-Abhöre (`space.out`, global) | Kopfhörer, Lautsprecher | binaurales oder Lautsprecher-Rendering für jeden 3D-Part und Rack-Slot |
| | Mono-Prüfung (`space.monocheck`, global) | aus / an | faltet den Hauptausgang zu mono – hör, was Lautsprecher in mono spielen würden |

*3D-Abhöre* und *Mono-Prüfung* gehören dem Hörer: Projekt-Presets speichern und ändern sie nicht.

### Der 3D-Schalter (p41)

Der Schalter auf Encoder 1 ist ein Bedienelement für zwei ältere Parameter, die weiterhin das sind, was Presets
und Sessions speichern: die Stufe des Parts an / aus (`pN.space.on`) und die globale **3D-Abhöre**
(`space.out`). *Aus* schaltet die Stufe des Parts aus; *Kopfhörer* und *Lautsprecher* schalten sie ein und
stellen die Abhöre. Die Abhöre ist, wie **du** hörst – sie gilt deshalb für alle Parts: *Lautsprecher* auf
Part 2 stellt auch den Schalter jedes anderen Parts mit 3D auf *Lautsprecher* (das Abzeichen im Kopf und der
Kopf in der 3D-Ansicht wechseln mit – Kopfhörer auf dem Kopf oder zwei Lautsprecher davor). Die drei bleiben in
beide Richtungen gleich: den Schalter (`pN.space.mode`) automatisieren oder per MIDI steuern geht, die älteren
`pN.space.on` / `space.out` ebenso – die Schalter folgen ihnen. Presets und Sessions von vor dem Schalter zeigen
beim Laden die richtige Stellung.

![Der 3D-Schalter auf Lautsprecher: Abzeichen und Beschriftung zeigen das Lautsprecher-Piktogramm](../images/ohpe-space-switch-speakers-de.png)

![3D aus: der Hinweis hat die erste Zeile des Streifens für sich](../images/ohpe-space-switch-off-de.png)

* **Kopfhörer** – binaural: Laufzeitunterschied zwischen den Ohren, Kopfschatten (Kugelkopf), Ohrmuschel-Kerben,
  Schulterreflexion, Nahfeld-Pegel (siehe *So funktioniert es*).
* **Lautsprecher** – der Lautsprecher-Weg derselben Stufe: leistungsgleiches Amplituden-Panning nach Richtung,
  **keine** Laufzeit zwischen den Ohren und **keine** Kopf- / Ohrmuschel-Färbung (das machen Lautsprecher und
  Raum wirklich), Tiefe über Pegel, Luft und dunklere Rückseite, die mitwandernden Raumreflexionen;
  monokompatibel (siehe unten).

### Bewegungspfade

| Pfad | Bewegung (ein Durchlauf) | Bewegungsgröße |
|---|---|---|
| Kreis | ein voller Orbit um den Kopf | kippt die Bahnebene (0 = waagerecht, 100 % = 60°: sie steigt auf einer Seite und sinkt auf der anderen) |
| Acht | eine liegende Acht: weite Bögen links / rechts, die sich vorn kreuzen, steigend und fallend | Breite des Bogens (30 … 180°) |
| Vorbeiflug | eine gerade Linie vor dir, links → rechts und zurück, an den Enden am langsamsten | Länge der Linie (2 … 12 × die Entfernung) – mit Doppler der klassische Vorbeiflug |
| Zufallsorbit | ein weicher Zufallsweg, vier neue Punkte pro Durchlauf (jeder Part seinen eigenen) | wie weit er wandert (Richtung, Höhe, Entfernung) |
| Pendel | schwingt von vorn über deinen Kopf nach hinten und zurück | wie weit es schwingt (100 % = vorn bis hinten) |
| Spirale aufwärts | steigt in 1–4 Windungen von unterhalb der Ohren bis über den Kopf, dann zurück | Zahl der Windungen |
| Ping-Pong v/h | springt im Takt von vorn nach hinten und zurück: jeder halbe Durchlauf beginnt auf dem Schlag mit einer schnellen Bewegung, dann bleibt der Klang stehen. Vorn → hinten führt rechts oben vorbei, hinten → vorn links oben | wie zackig (0 = weiches Schwingen über den ganzen halben Durchlauf, 100 % = kurzer Hüpfer in den ersten 30 %) |
| Schwarm | ein Flirren um die Vorderseite: zwölf kleine Zufallssprünge pro Durchlauf auf einer langsamen Zufallsdrift – wie eine Biene oder eine Funkenwolke (jeder Part seinen eigenen, nie genau gleich) | wie weit es flirrt und driftet (Richtung, Höhe, Entfernung) |
| Flüstern | ganz nah hinter deinem Kopf (0,4 m bei der Standard-Entfernung), verweilt am nächsten an einem Ohr, dann am anderen – der Nahfeld-Pegelunterschied macht es intim. *Entfernung* kleiner = noch näher | wie weit es an den Ohren nach vorn reicht |
| Entfernungsgang | geht geradeaus in die Ferne und kommt zurück, pendelt dabei leicht links / rechts – Pegel, Luftdämpfung und Doppler ändern sich unterwegs | wie weit (2 … 16 × die Entfernung; am nächsten 0,35 ×) |
| Über den Kopf | ein senkrechter Kreis durch beide Ohren: über dem Kopf → rechtes Ohr → unter dir → linkes Ohr | kippt die Oberseite des Kreises nach vorn (0 = genau durch die Ohren, 100 % = 45°) |
| Wellenorbit | ein voller Orbit, der pro Runde dreimal auf und ab webt und an den Tiefpunkten etwas näher kommt | Höhe der Wellen (10 … 40°) und ihre Tiefe |
| Ping-Pong l/r | springt im Takt von deinem linken zum rechten Ohr und zurück: jeder halbe Durchlauf beginnt auf dem Schlag mit einer schnellen Bewegung, dann bleibt der Klang stehen. Links → rechts führt vor dir vorbei, rechts → links hinter dir | wie zackig (0 = weiches Schwingen über den ganzen halben Durchlauf, 100 % = schnelle Bewegung in den ersten 45 %) |
| Vier Ecken | schreitet durch vier Ecken um dich – vorn links → vorn rechts → hinten rechts → hinten links –, eine pro Viertel-Durchlauf, hebt sich bei jeder Bewegung etwas und bleibt dann stehen | wie zackig (0 = weich, 100 % = schnelle Bewegung in den ersten 40 % jedes Viertels) |
| Atmen | atmet vor dir ein und aus: von weit weg bis nah heran (0,45 × die Entfernung) und zurück, logarithmisch gleichmäßig, nah etwas höher | wie weit es hinaus atmet (1,2 … 3 × die Entfernung) |
| Helix | kreist zwei- bis viermal, während es gleichmäßig von unterhalb der Ohren bis über deinen Kopf steigt (erste Hälfte) und wieder sinkt (zweite Hälfte), oben etwas näher | Windungen pro Durchlauf (2 … 4) |
| Überflug | eine gerade Linie über deinem Kopf (eine Entfernung hoch): von weit vorn, genau über dir, nach weit hinten und zurück, an den Enden am langsamsten – mit Doppler ein Flugzeug, das über dich hinwegzieht | Länge der Linie (2 … 12 × die Entfernung) |
| Hüpfen | hüpft wie ein Ball vor dir von links → rechts → links, sechs Sprünge pro Durchlauf | Sprunghöhe (bis 60° über dem Aufsetzpunkt) |
| Wandern | eine weiche Zufallsschleife – Richtung, Höhe und Entfernung –, die sich jeden Durchlauf genau wiederholt und so im Takt des Songs bleibt (jeder Part seine eigene Form) | wie weit es wandert |
| Schleifenorbit | ein voller Orbit mit vier kleinen Schleifen: auf der nahen Seite jeder Schleife dreht der Klang gegen die Runde zurück, auf der fernen hebt er sich etwas | Größe der Schleifen (und wie nah er kommt: 0,75 … 0,33 × die Entfernung) |

Die sechs Pfade ab Ping-Pong v/h und die acht ab Ping-Pong l/r kamen später dazu; sie stehen am Ende der Liste,
damit Klänge und Automationen mit den älteren Pfaden ihren Pfad behalten und gleich klingen. Alle Pfade sind stetig
(auch dort, wo ein Durchlauf neu beginnt) und bleiben im Takt des Songs.

**In der 3D-Ansicht** ist der Pfad des Edit-Parts vor seinem Punkt als zarte gepunktete Kurve gezeichnet –
ein ganzer Durchlauf (bei Schwarm und Zufallsorbit: der nächste Durchlauf ab jetzt) – so siehst du Form,
Größe und Ausrichtung eines Pfads, noch bevor ein Ton spielt. Der Name des Pfads steht unten links.

![Wellenorbit, Schwarm und Über den Kopf in der 3D-Ansicht – der Pfad von Part 1 vor seinem Punkt gezeichnet](../images/ohpe-space-paths-de.png)

Azimut und Elevation drehen jeden Pfad: ein Pendel bei Azimut 90° schwingt von links über rechts.

## Modulation und MPE

Die Modulationsmatrix (LFO-Modus → Mod) bewegt **Azimut, Elevation, Entfernung, Bewegungspfad Tempo** und
**Bewegungspfad Phase** der Stufe (Ziele 67 – 71, [MODULATION.md](MODULATION.md)). Mit einem MPE-Controller
oder polyphonem Aftertouch:

* **Slide (Klangfarbe) → Elevation**: nach oben sliden hebt den Klang über deinen Kopf;
* **Notendruck → Entfernung** mit negativer Menge: fester drücken holt den Klang näher;
* ein LFO auf *Bewegungspfad Tempo* lässt einen Orbit beschleunigen und abbremsen; ein Makro auf *Azimut*
  dreht die ganze Szene.

Die Modulation nutzt die neueste Note (eine Position pro Part). Jedes 3D-Werkspreset hat Slide → Elevation und
Druck → Entfernung geroutet.

## 3D-Raum als Rack-Effekt

Das FX-Rack hat den Typ **3D-Raum** (EN *Space 3D*, Typ 22) in jedem Slot der Part-Racks und des Master-Racks,
mit denselben Parametern (`pN.fxK.space3d.*`, `mfxK.space3d.*`). Damit lässt sich ein einzelner Effekt oder der
ganze Master im Raum platzieren. Seine Ziele *FX-Parameter 1 – 4* sind Azimut, Elevation, Entfernung und
Bewegungstempo. Den Mix des Slots auf 100 % lassen (das trockene Signal ist nicht wie das 3D-Signal verzögert).

## Monokompatibilität und Bassbereich

* Das Band unter *Mono unter* wird mono summiert und nie räumlich bewegt – Bässe behalten einen zentrierten,
  festen Sub, während sich ihre Mitten bewegen (die 3D-Bass-Presets nutzen 150 – 220 Hz).
* **Lautsprecher**: Amplituden-Panning ohne Zeitunterschiede – die Mono-Faltung verliert überall auf dem Kreis
  höchstens 3 dB (getestet).
* **Kopfhörer**: binaurale Signale enthalten kleine Zeitunterschiede zwischen den Ohren. Sie falten ohne hohle
  Kammfilter zu mono (geradeaus sind beide Ohren gleich, seitlich ist das ferne Ohr viel leiser), wichtige
  Mischungen trotzdem mit **Mono-Prüfung** und dem Korrelationsbalken prüfen.

## So funktioniert es

Ohpe nutzt ein **eigenes parametrisches Modell** von Kopf und Ohren – es ist kein gemessener
Kopf-Datensatz enthalten oder nötig:

* **Kugelkopf**: interaurale Laufzeitdifferenz (Woodworth-Formel, 0,65 ms bei 90°) über fraktionale,
  kubisch interpolierte Verzögerungen, und ein Kopfschatten-Filter erster Ordnung pro Ohr, dessen Koeffizienten
  Sample für Sample gleiten;
* **Ohr-Merkmale**: zwei Ohrmuschel-Kerben, die mit der Elevation von etwa 6 kHz auf 11 kHz steigen,
  Richtungsbänder für vorn (Präsenz), hinten (Ohrmuschel-Schatten, 1-kHz-Band) und oben (8-kHz-Band), eine
  Schulterreflexion und ein Nahfeld-Pegelunterschied unter 1 m;
* **Entfernung**: Pegelabfall, Luftdämpfung, eine Entfernungsverzögerung, die den Doppler-Effekt erzeugt (ihre
  Geschwindigkeit ist begrenzt, die Tonhöhe biegt sich nie mehr als etwa ±12 %);
* **Raum**: sechs Spiegelquellen-Reflexionen eines Quaderraums, die der Quelle folgen und mit ihren eigenen
  Richtungen gerendert werden, in einen kleinen diffusen Nachhall aus vier Leitungen;
* **Stereo-Quellen**: das Mittensignal des Parts ist eine Punktquelle, sein Seitensignal ein Quellenpaar im
  Abstand der *Quellenbreite* – Monomaterial erzeugt nie Kammfilter.

Alles wird interpoliert – Positionen gleiten, Verzögerungen rampen pro Sample, Filter werden alle 16 Samples
nachgeführt – bewegte Quellen bleiben klickfrei (die Tests fahren, springen und schalten und messen dabei die
zweite Differenz des Ausgangs). CPU: etwa 1,3 % eines Kerns pro Stufe bei 48 kHz auf der Build-Maschine; vier
Part-Stufen plus acht Rack-Instanzen bleiben klar im Echtzeit-Budget (von den Core-Tests gemessen).

## Tipps für Bassmusik

* Neurofunk- / Dubstep-Bässe: ein kurzer Pfad (*Kreis* oder *Acht*, 1 – 2 Takte) mit kleiner Entfernung
  (1,2 – 1,5 m), wenig Raum und *Mono unter* 150 – 180 Hz: der Growl bewegt sich, der Sub drückt mono.
* Atmosphären: *Zufallsorbit* über 8 – 16 Takte, Entfernung 3 – 5 m, Raumanteil 35 – 50 %, Breite 70 – 90°.
* Riser: *Spirale aufwärts* über die Länge des Risers; Vorbeiflüge: *Vorbeiflug* mit Doppler 100 %.
* Zwei Parts mit demselben Pfad und Bewegungsphase 0° / 180° jagen sich um den Kopf.
