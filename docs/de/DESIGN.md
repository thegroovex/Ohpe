# Gestaltung von Ohpe

*Deutsch · English: [../DESIGN.md](../DESIGN.md)*

Ohpe ist ein Instrument, kein Fenster voller Bedienelemente. Die Gestaltung folgt einer eigenen,
minimalistischen skandinavisch-industriellen Idee: heller eloxierter Korpus, Grafit-Tasten, ein
tiefes Display und vier funktionale Farben.

* **Eine Fläche** auf einem 10er-Raster (1200 × 800 Einheiten, gleichmäßig skaliert). Bibliothek
  und Profiansicht liegen wie ein Deckel auf der Platte; die Klaviatur bleibt spielbar.
* **Farbe heißt Funktion.** Die vier Akzente gehören zu den vier Encodern: Signalrot (1),
  Sonnengelb (2), Ultraviolett (3), Rosé (4). Rot markiert außerdem Aufnahme und aktiven Modus,
  Gelb Engine und bearbeiteten Part.
* **Das Display ist der einzige Bildschirm:** tiefes Glas mit feinem Punktraster, warmweiße Schrift.
* **Tiefe statt Dekoration (0.7):** Das Display ist ein versenktes Fenster mit Ebenen: eingelassene
  Fassung (oben beschattet, an der unteren Kante beleuchtet), Glas mit von oben einfallendem Licht
  und dunkleren Ecken, das Punktraster hinter dem Inhalt, der Inhalt, die Glasoberfläche darüber
  (innerer Schatten, ein feiner diagonaler Glanz). Die hinteren Ebenen verschieben sich mit der Maus
  leicht gegen die vorderen (Parallaxe); die Figur wirft einen weichen Schatten. Keine Texturen,
  kein Chrom.
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
