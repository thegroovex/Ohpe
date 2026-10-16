# Gestaltung von Ohpe

*Deutsch · English: [../DESIGN.md](../DESIGN.md)*

Ohpe ist ein Instrument, kein Fenster voller Bedienelemente. Die Gestaltung ist ein eigener
funktionaler Minimalismus: weniger, aber besser. Ein mattes warmweißes Gehäuse, graue eingelassene
Felder, matte helle Tasten mit sanfter Tiefe, ein großes schwarzes Display und vier funktionale
Farben. Nichts ist Dekoration: jede Linie, jeder Schatten und jede Farbe sagt, was ein Element ist
oder tut.

![Die neu gestaltete Platte (p14)](../images/ohpe-analog-de.png)

* **Ein Gehäuse (p14)** auf einem 8er-Raster (1200 × 800 Einheiten, gleichmäßig skaliert): matt
  warmweiß, großzügige Ränder, weicher Schatten auf dem Hintergrund – kein Metall, keine Textur.
  Funktionsgruppen liegen in grauen, **eingelassenen Feldern**: ENGINE (links), MODUS (rechts),
  TRANSPORT (unten links), OKTAVE + MIDI (unten rechts), das Klaviaturbett. Bibliothek und
  Profiansicht liegen wie ein Deckel in dunklem Rahmen auf der Platte; Kopfzeile und
  Klaviaturzeile bleiben sichtbar und spielbar. **Ein Radiensystem:** das Gehäuse hat 18 Einheiten
  Eckradius, jedes eingelassene Feld und jeder Rahmen (Spalten, Transport, Oktave, Klaviaturbett,
  Display-Rahmen, die Mulden der Kopfzeile, die Overlays) dieselben 8 Einheiten, was in einem Feld
  liegt (Tasten, Part-Tasten, Preset-Streifen, Display-Glas) 5 Einheiten – die Rundungen wirken konzentrisch.
* **Aufteilung (p14):** Kopfzeile mit OHPE, PART 1–4 (Segmente), Preset-Streifen, BIBLIOTHEK und PRO
  (dunkle Pillen), ZUFALL und SPEICHERN (runde Tasten mit Vierfarbring), EN / DE – eine Reihe: alle
  Bedienelemente auf der Mittellinie der Streifen (y 56), alle Beschriftungen (PART, PRESET, BIBLIOTHEK,
  PRO, ZUFALL, SPEICHERN, SPRACHE) darüber auf einer Grundlinie; 8 Einheiten zwischen den Tasten, rechts
  32 Einheiten Rand wie links vor dem Schriftzug. Links die acht
  Engines, rechts die acht Modi, in der Mitte das Display mit vier Encodern darunter. Klaviaturzeile:
  PITCH- und MOD-Rad links, drei Oktaven Klaviatur (C3–C6), rechts eine Spalte: der Glide-Schalter
  (Auto / Legato / Immer) mit der Beschriftung PORTAMENTO und darunter der GLIDE-Regler – beide
  Beschriftungen auf einer Textspalte, in derselben Typografie.
* **Farbe heißt Funktion.** Die vier Akzente gehören zu den vier Encodern: Signalrot (1),
  Sonnengelb (2), Ultraviolett (3), Rosé (4) – auf den Encoder-Ringen und in den Makrozeilen des
  Displays. Die Leuchtbalken der Tasten sprechen dieselbe Sprache: **Gelb = dein Fokus** (Engine des bearbeiteten
  Parts, die angezeigte Seite), **Ultraviolett = läuft / anderswo in Benutzung** (Engine eines anderen
  Parts, laufender Sequencer, ein Effekt-Platz an, eine geroutete Modulation), **Grün = spielt** (die
  START-Taste, das spielende Tape – die einzige Funktionsfarbe neben den vier Akzenten: ein ruhiges
  Jadegrün, `playGreen` #19A66C auf den Tasten, `playGreenLit` #3FD08E auf dem schwarzen Display),
  **Rot = Aufnahme**. Ein laufendes Licht (START spielt, REC nimmt auf) **atmet**: ein weicher Puls von
  etwa 1,8 s, nie ganz aus, kein hartes Blinken.
  Alles andere ist Warmweiß, Grau und Grafit.
* **Das Display ist der einzige Bildschirm:** groß, kontrastreich, in einem dunklen Rahmen
  versenkt; tiefes Glas mit feinem Punktraster, warmweiße Schrift.
* **Tiefe statt Dekoration:** Tasten sind matte helle Kappen mit beleuchteter Oberkante und weichem
  Schatten; gedrückte oder aktive Tasten sinken leicht ein. Engine-, Modus-, Transport-, Oktav- und
  MIDI-Tasten tragen einen **Leuchtbalken** (Rev. 3): ein kurzes waagrechtes Fenster am unteren Rand der
  Kappe, unter dem Piktogramm – unbeleuchtet ein feiner dunkler Schlitz, leuchtend eine flache Farbe mit
  weichem Schein, kein Glanzpunkt. Auf den Engine-Tasten ist der Balken in vier Segmente geteilt, eines je
  Part (1 – 4, von links): gelb = der bearbeitete Part spielt die Engine, violett = ein anderer Part.
  START leuchtet grün (auch das Dreieck glüht), REC rot (stetig = bereit; der Kreis glüht); solange START
  spielt und REC aufnimmt, atmen Balken und Zeichen sanft (Puls 1,8 s zwischen 35 % und 100 %, vom 30-Hz-Timer
  des Editors getrieben und so gestuft, dass eine Taste nur bei sichtbarer Änderung neu gezeichnet wird);
  TAPE leuchtet grün, solange das Tape spielt, rot bei Aufnahme; SCHLEIFE gelb, MIDI rot während MIDI-Learn, OKTAVE −/+ gelb, solange die Klaviatur in diese
  Richtung verschoben ist. Felder sind ins Gehäuse eingelassen. Das Glas des Displays hat Ebenen (Punktraster
  dahinter, Inhalt, Glasoberfläche) mit leichter Parallaxe. Kein Chrom.
* **Transport, Klaviaturzeile und Oktavfeld** während der Aufnahme (2x): REC rot und START grün, beide atmend:

  ![Transport, Klaviaturzeile und Oktavfeld](../images/ohpe-transport-de.png)

* **Räder und Glide (p14):** PITCH- und MOD-Rad sind Grafit-Daumenräder in dunklen Schlitzen; das
  Pitch-Rad federt zur Mitte zurück, das Mod-Rad bleibt stehen; Rechtsklick = MIDI Learn. Der
  Glide-Schalter und der kleine GLIDE-Regler folgen dem bearbeiteten Part.
* **Pixelgenau:** Das statische Gehäuse wird je Größe einmal in der physischen Pixelauflösung
  gezeichnet (scharf auf 2x-Bildschirmen, keine Zeichenarbeit beim Spielen); jede Beschriftung wird
  in ihre Breite eingepasst.
* **Bewegte Flächen (0.7):** Stereobild (Goniometer, Korrelation, M-/S-Balken in den Farben der
  Mitte-/Seite-Encoder) und Spektrum-Landschaft (perspektivischer Wasserfall, vorn warmweiß mit
  ultraviolettem Schimmer, zum Horizont grau; in der Stille flach und ruhig) auf der linken
  Display-Hälfte – Klick wechselt, Rechtsklick öffnet das Menü.
* **Direkte Bedienung:** Balken, Schritte, Pads, Spuren und Soft-Tasten reagieren auf die Maus;
  Feinregelung ist immer Shift / Cmd / Strg.
* **Figuren statt Icons:** Jede Engine und jeder Modus hat eine animierte Piktogramm-Figur, die auf
  die vier Makros ihrer Seite und auf gespielte Noten reagiert (Grain: Wolke; Vokal: singendes
  Gesicht; Sampler: Wellenform mit Start- und Schleifenmarken; Drum: Trommel mit Modell, Stimmung,
  Abklingen und Anschlagsblitz). Seit 0.7 tritt in der Mod-Ansicht die Liste der 16 Verbindungen mit
  Mengenbalken und Spur an die Stelle der LFO-Figur, im Rack die Kette der Slot-Blöcke (Mischung gelb,
  Pegel violett, BYPASS rot) mit dem Bild des gewählten Effekts (Kennlinie, Frequenzgang,
  Pegelreduktion); ein dünner Modulationsring im Encoder zeigt Reichweite und aktuelle Position der
  Modulation. Die vollständige Tabelle steht in der
  [englischen Fassung](../DESIGN.md#pictogram-characters).
* **Transport (p11, p14, Rev. 3):** im Feld unten links eine Tape-Anzeige (REC / START / TAPE, Zeit
  m:ss.z, Takt.Schlag) über einer segmentierten Reihe aus fünf verbundenen, gleich breiten Tasten
  ANFANG · STOPP · START · REC · SCHLEIFE (Tape-Deck). Das Feld
  unten rechts spiegelt es: Tastatur-Anzeige (Umfang, Oktavlage; MIDI-Learn rot) über OKTAVE − / + und MIDI;
  schlichte geometrische Zeichen (Kreis, Dreieck, Quadrat, Strich + Dreieck, offene Schleife mit
  Pfeil). Der Aufnahmekreis ist immer signalrot, START leuchtet grün; beide atmen, solange sie laufen. Auf der
  Tape-Seite ersetzt das Transportfeld die Spulen-Figur: Zustandsschild (rot atmend = Aufnahme, grün = spielt),
  aufnahmebereite Spuren in Rot, Zeit und Takt.Schlag in Monospace, Spulen, die sich mit dem Band
  drehen, fünf umrandete Tasten (gefüllt, wenn aktiv) und eine Hinweiszeile (gelb, solange keine Spur
  bereit ist).
* **Deckel:** Bibliothek und Profiansicht liegen in dunklem Rahmen und blenden alle Bedienelemente und Beschriftungen der Platte
  unter sich aus – keine Beschriftung schaut über die Kante.

* **3D-Raum (1.0):** Die Raum-Ansicht im Effekt-Modus (und die Display-Ansicht „3D-Raum“) zeigt die
  Kugel um den Hörer: einen Kopf von leicht hinten oben, Bodenringe und den Mittelbogen; jeder Part
  mit eingeschalteter Raum-Stufe ist ein Punkt in seiner Part-Farbe mit verblassender Spur und
  Lotlinie zum Boden, näher = größer, der bearbeitete Part obenauf und beschriftet. Beim Abhören über
  Kopfhörer trägt der Kopf Kopfhörer, im Lautsprecher-Modus stehen zwei Lautsprecher vor ihm.
* **Kopfhörer-Piktogramm und 3D-Abzeichen (1.0):** eine eigene Strichzeichnung im Stil der Figuren
  (Bügel, zwei Muscheln, Schallbögen, wenn es leuchtet). Das Abzeichen „3D“ hinter dem Part-Abzeichen im
  Display-Kopf leuchtet ultraviolett und pulsiert sanft, solange der bearbeitete Part binaurales 3D
  spielt, ist im Lautsprecher-Modus sonnengelb umrandet (mit Lautsprechern) und sonst nur schwach
  umrandet; ein Klick öffnet die Raum-Ansicht, der Tooltip sagt „Binaurales 3D – am besten mit
  Kopfhörern“. Ein langer Titel wird mit Auslassungspunkten gekürzt, bevor er die Abzeichen in die
  Tempoanzeige schiebt. Die Part-Tasten tragen das Piktogramm in der Ecke, solange der Part den Raum
  nutzt; Presets mit dem Schlagwort „3D“ tragen es in der Bibliothek hinter ihrem Namen.

Texte gibt es überall auf Englisch und Deutsch; lange Texte werden gekürzt oder eingepasst, nie
mitten im Zeichen abgeschnitten.
