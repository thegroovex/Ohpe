# Grid-Controller (Profil 1)

Ohpe (die Standalone-App und seit p49 das Plug-in) lässt sich mit einem externen **8 × 8-Pad-Raster-Controller mit vierzeiligem
Zeichendisplay** spielen und bedienen: 64 Pads mit Anschlag und Druck, 8 berührungsempfindliche Drehregler und ein
Master-Regler, zwei Reihen mit je 8 Tasten unter dem Display, Transport- und Navigationstasten und ein Touch-Strip.
In Ohpe heißt er **Grid-Controller (Profil 1)**.

Die Pads spielen genau wie die [Raster-Tastatur](HARMONIE.md#die-raster-tastatur) auf dem Bildschirm: dieselben
Anordnungen, dieselben Töne, dieselben Farben für Grundton und Tonart-Töne – was du am einen lernst, gilt am anderen.

![Einstellungen – MIDI mit dem Controller-Bereich und dem virtuellen Controller](../images/ohpe-settings-controller-de.png)

## Einrichten

1. Controller anschließen und die **Standalone-App** von Ohpe starten (für das Plug-in siehe [Im Plug-in](#im-plug-in)).
2. **Einstellungen > MIDI > Controller**: **Grid-Controller (Profil 1)** wählen.
3. **Ein** / **Ausgang**: zu den MIDI-Ports des Controllers schalten (meist für beide sein erster Port; bleibt das
   Display leer, den anderen Port probieren). Die Überschrift zeigt **Verbunden**, sobald beide Ports offen sind.
4. **Bend / Mod**: was der Touch-Strip tut.
5. **Ansicht** zeigt den *virtuellen Controller*: die vier Displayzeilen, die 64 Pad-Lichter und die letzten
   MIDI-Nachrichten – praktisch, um zu prüfen, was der Controller zeigen soll.

In der Standalone-App wird nichts automatisch erkannt; die Wahl wird je Benutzer in `settings.json` gespeichert (nie in Projekten oder
Presets) und der Controller beim nächsten Start wieder geöffnet. Wird er abgezogen, stehen Lichter und Display;
kommt er zurück, verbindet sich Ohpe innerhalb einer Sekunde neu und schickt alles noch einmal. Ohpe schaltet den
Port des Controllers in der normalen Liste der MIDI-Eingänge aus, damit seine Pads nicht doppelt spielen. Das Profil
ist **ab Werk aus**.

## Im Plug-in

Seit p49 arbeitet der Controller auch im **Plug-in** (VST3 / AU in einer DAW). Der Controller hat zwei Modi: im
üblichen Modus spricht er mit der DAW; im **User-Modus** (die *User*-Taste des Controllers) überlässt er Pads,
Display und Lichter dem Programm, das seinen *User-Port* öffnet. Ohpe öffnet diesen Port selbst – am Host vorbei –
mit vollem Display, Pad-Farben, Druck, Reglern (mit Berührung), Touch-Strip und Tasten, genau wie in der Standalone-App.

![Einstellungen – MIDI mit dem Controller-Bereich im Plug-in](../images/ohpe-settings-controller-plugin-de.png)

1. **In der DAW**: in den MIDI-Einstellungen den *User-Port* des Controllers (oft *… User Port*, unter Windows
   *MIDIIN2 (…)* / *MIDIOUT2 (…)*) für **Spuren und Fernsteuerung ausschalten** (Eingang und Ausgang), damit die DAW
   ihn freigibt und die Pads nicht doppelt spielen. Den Haupt-Port des Controllers so lassen.
2. **In Ohpe**: **Einstellungen > MIDI > Controller**: **Grid-Controller (Profil 1)** wählen.
3. **Ports automatisch wählen** (ab Werk an) nimmt den Eingang und Ausgang, deren Namen zum Muster passen
   (`User; MIDIIN2; MIDIOUT2` – irgendein Teil im Portnamen; änderbar als `"pattern"` unter `"controller"` in
   `settings.json`). Ausschalten und **Ein** / **Ausgang** schalten, um die Ports selbst zu wählen (das Schalten eines
   Ports schaltet die Automatik aus).
4. Den Controller in seinen **User-Modus** schalten. Die Überschrift zeigt **Aktiv in dieser Instanz**, sobald beide
   Ports offen sind.

**Eine Instanz zur Zeit.** Nur ein Ohpe der DAW kann den Controller halten: das, dessen Fenster du **zuletzt geöffnet
oder angeklickt** hast. Die anderen zeigen **In anderer Instanz aktiv** und einen Knopf **Hierher holen**; wird die
haltende Instanz entfernt, übernimmt die nächste. Die haltende Instanz behält den Controller, während ihr Fenster
**geschlossen** ist (die Regler behalten die letzte Seite, Part-, Modus- und Transport-Tasten wirken). Nach dem Laden
eines Projekts das Fenster der gewünschten Instanz einmal öffnen – bis dahin hält keine Instanz den Controller.

**Port belegt.** Unter Windows kann ein MIDI-Port nur in einem Programm offen sein. Hält die DAW den User-Port noch,
zeigt die Überschrift **Port belegt – in der DAW freigeben**; den Port in den MIDI-Einstellungen der DAW freigeben –
Ohpe versucht es jede Sekunde erneut und verbindet sich auch neu, wenn der Controller ab- und wieder angesteckt wird.
MIDI vom Host wirkt weiter parallel. Instanz schließen oder Profil ausschalten löscht Display und Lichter.

## Display

| Zeile | Zeigt |
|---|---|
| 1 | Namen der Parameter auf den 8 Reglern (je zwei über einem Display-Segment) |
| 2 | ihre Werte |
| 3 | Preset · Part und Engine · Tempo · Tonart (mit **Lock** bei Scale Lock) – solange ein Regler berührt wird: sein voller Name und Wert |
| 4 | Seite (z. B. *Filter 2/5*) · Transport (Start / Stopp / REC / Vorzähler, *Klick* mit Metronom) · Bandposition · Pad-Modus, Raster-Anordnung und Oktave |

Umlaute stehen als ae / oe / ue da (das Display kennt keine Umlaute).

## Regler

* **Regler 1 – 8**: die Parameter der gezeigten Seite und der nächsten – Regler 1 – 4 sind die vier Bildschirm-Regler,
  5 – 8 die Seite danach (in der Reihenfolge der Seiten auf dem Bildschirm). **Select** (oder Shift) halten = feine
  Schritte.
* **Master-Regler**: Gesamtlautstärke; die Taste **Master** schaltet ihn aufs Tempo um.
* **Tempo-Regler**: Tempo (1 BPM je Schritt, Shift / Select 0,1).

## Pads

Die zweite Tastenreihe, Tasten 5 – 8 (oder **Note** / **Session**), wählt den Pad-Modus:

| Modus | Pads |
|---|---|
| **Tasten** | die Raster-Tastatur des Bildschirms auf 8 × 8 Pads |
| **Pads** | die vier Performance-Pads (Stottern, Bandstopp, Sweep, Freeze) – je ein 4 × 4-Viertel, gehalten = an |
| **Drum** | die 24 Drum-Pads auf den drei unteren Reihen |
| **Steps** | Schritte 1 – 16 des Step-Sequenzers auf den beiden oberen Reihen (oben 1 – 8); die sechs Reihen darunter spielen wie Tasten |

### Tasten – die Raster-Tastatur auf den Pads

* **Dieselbe Anordnung** wie *Raster-Anordnung* (`harmony.kb.layout`): **Quarten** (rechts +1, hoch +5), **Terzen**
  (+1 / +4), **Ganztöne** (+2 / +5) oder **In Tonart** (Stufen der Tonart, drei Stufen je Reihe – nur Tonart-Töne).
  Die Taste **Scales** schaltet die Anordnungen weiter.
* **Dieselbe Zuordnung**: Pad (Spalte, Reihe) spielt den Ton des Bildschirm-Felds in derselben Spalte und Reihe, ab
  demselben tiefsten C. Die vier unteren Pad-Reihen sind genau die vier Reihen des Bildschirm-Rasters; die oberen vier
  setzen es fort.
* **Dieselbe Tonart**: die Tonart, die das Raster zeigt (die gemerkte Tonart – bis etwas gehört wurde die eingestellte).
  Mit **Scale Lock** (Shift + Scales) bleiben die anderen Töne dunkel und stumm; In Tonart hat ohnehin nur Tonart-Töne.
* **Farben**: Grundton der Tonart **bernstein**, Tonart-Töne **gelb (gedimmt)**, Töne des gemerkten Akkords **grün
  (gedimmt)**, gehaltene Pads **grün**, andere Töne dunkel (auf dem Bildschirm graphit).
* **Oktave − / +** verschiebt Pads, Bildschirm-Klaviatur und Bildschirm-Raster gemeinsam (das Pad unten links ist das
  C dieser Oktave; In Tonart: der erste Tonart-Ton ab dort).
* **Anschlag und Druck** spielen den **Edit-Part**. Ist für ihn MPE an, bekommt jedes Pad seinen eigenen
  Mitgliedskanal und der Druck gehört zu dieser Note; ohne MPE laufen die Noten durch Skala, Akkord und Folgen, der
  Druck geht als polyphoner Aftertouch an den Part.

## Tasten

| Taste | Wirkung |
|---|---|
| erste Reihe unter dem Display | die acht Modus-Tasten in der Reihenfolge des Bildschirms, Synth bis Drum (leuchten wie die Bildschirm-Tasten) |
| zweite Reihe 1 – 4 | Parts 1 – 4 |
| zweite Reihe 5 – 8 | Pad-Modus Tasten / Pads / Drum / Steps |
| ◀ / ▶ | Seite − / + |
| ▲ / ▼ | voriges / nächstes Preset |
| Oktave − / + | Oktave von Klaviatur, Raster und Pads |
| Play | Start / Stopp (Shift: Band Start / Stopp) |
| Record | Aufnahme (Shift: Edit-Spur scharf schalten) |
| Stop | Stopp (Shift: zum Anfang) |
| Repeat | Band-Schleife an / aus |
| Metronome | Metronom an / aus (Shift: Vorzähler Aus / 1 Takt / 2 Takte) |
| Tap Tempo | Tempo tippen |
| Undo | Band-Rückgängig (Shift: Wiederholen) |
| Scales | nächste Raster-Anordnung (Shift: Scale Lock) |
| Master | Master-Regler: Lautstärke ↔ Tempo |
| Shift / Select | Umschalter (Select: feine Regler-Schritte) |

## Touch-Strip

**Bend**: Pitchbend des Edit-Parts (das Pitch-Rad auf dem Bildschirm folgt), beim Loslassen zurück zur Mitte.
**Mod**: das Mod-Rad.

## Hinweise

* Keine neuen Host-Parameter: Der Controller ändert nur bestehende Parameter (Host wird informiert, automatisierbar wie
  bisher).
* Eigene Umsetzung des öffentlich dokumentierten MIDI- / Display-Protokolls des Controllers; Ohpe enthält keinen
  fremden Code.
