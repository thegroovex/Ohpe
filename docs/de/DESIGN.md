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
* **Transport (p11):** REC und START groß, darunter zum Anfang / Stopp / Schleife, unten links;
  schlichte geometrische Zeichen (Kreis, Dreieck, Quadrat, Strich + Dreieck, offene Schleife mit
  Pfeil). Der Aufnahmekreis ist immer signalrot, die Taste blinkt während der Aufnahme. Auf der
  Tape-Seite ersetzt das Transportfeld die Spulen-Figur: Zustandsschild (rot blinkend = Aufnahme),
  aufnahmebereite Spuren in Rot, Zeit und Takt.Schlag in Monospace, Spulen, die sich mit dem Band
  drehen, fünf umrandete Tasten (gefüllt, wenn aktiv) und eine Hinweiszeile (gelb, solange keine Spur
  bereit ist).
* **Deckel:** Bibliothek und Profiansicht blenden alle Bedienelemente und Beschriftungen der Platte
  unter sich aus – keine Beschriftung schaut über die Kante.

Texte gibt es überall auf Englisch und Deutsch; lange Texte werden gekürzt oder eingepasst, nie
mitten im Zeichen abgeschnitten.
