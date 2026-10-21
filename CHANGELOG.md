# Changelog / Änderungen

Ohpe – designed by groovex. Each release's full notes (English and Deutsch) are in
[docs/release-notes](docs/release-notes/) and on the [releases page](https://github.com/thegroovex/Ohpe/releases).
Die vollständigen Hinweise jeder Version (Deutsch und English) stehen in [docs/release-notes](docs/release-notes/)
und auf der [Release-Seite](https://github.com/thegroovex/Ohpe/releases).

## Unreleased

- EN: Factory bank curated for 1.0 (p65): every preset rendered offline and compared; three near-duplicates retired
  (Sine Slide, Phase Reese, Sparkle Bell – sessions that used them keep their sound and show no preset name), names
  unique and free of maker names (Funk Clav → Funk Snap Keys, Lo-Fi spelling), tags fixed (FM electric pianos under
  Piano and more), and every preset level-matched to one target – −11 LUFS short-term (EBU R128), true peak at or
  below −1 dBTP – through its level parameter only. Clipping presets fixed. Details: docs/en/FACTORY-SOUNDS.md.
- DE: Werksbank für 1.0 kuratiert (p65): jedes Preset offline gerendert und verglichen; drei Beinahe-Dubletten
  zurückgezogen (Sine Slide, Phase Reese, Sparkle Bell – Sitzungen damit behalten ihren Klang und zeigen keinen
  Preset-Namen), Namen eindeutig und ohne Herstellernamen (Funk Clav → Funk Snap Keys, Schreibweise Lo-Fi), Tags
  korrigiert (FM-E-Pianos unter Klavier u. a.) und jedes Preset auf ein Ziel eingepegelt – −11 LUFS Kurzzeit
  (EBU R128), True Peak höchstens −1 dBTP – nur über seinen Pegelparameter. Übersteuernde Presets behoben. Details:
  docs/de/FACTORY-SOUNDS.md.
- EN: Robustness (p63): damaged sessions, presets, tape files, wavetables, content packs and settings never crash Ohpe
  (refused, or loaded as far as readable with defaults for the rest); NaN / Inf can no longer reach the outputs (input
  cleaned, output guard); crash-safe start in the standalone app – after a session that did not end cleanly, or with a
  damaged saved state, Ohpe starts with the start sound and offers to restore the last session. New fuzz and audio
  stress tests in CI (all sample rates, buffer sizes down to 1 sample, denormals, a 10-minute soak) and a documented
  manual 8-hour soak.
- DE: Robustheit (p63): beschädigte Sitzungen, Presets, Banddateien, Wavetables, Inhaltspakete und Einstellungen bringen
  Ohpe nie zum Absturz (abgelehnt, oder soweit lesbar geladen, der Rest mit Standardwerten); NaN / Inf erreichen die
  Ausgänge nicht mehr (Eingang bereinigt, Ausgangsschutz); absturzsicherer Start der Standalone-App – nach einer nicht
  sauber beendeten Sitzung oder bei beschädigtem gespeicherten Zustand startet Ohpe mit dem Startklang und bietet an,
  die letzte Sitzung wiederherzustellen. Neue Fuzz- und Audio-Stresstests in der CI (alle Abtastraten, Puffer bis 1
  Sample, Denormals, 10-Minuten-Dauerlauf) und ein beschriebener manueller 8-Stunden-Dauertest.
- EN: File formats frozen for 1.0 (p62): every saved format (state / sessions, presets, .ohpetape, .ohpewt,
  .ohpepack, .ohpemidi, settings.json, update.json, the state sections) has an explicit version in one place; older
  files always load and migrate, unknown newer fields are ignored. Golden fixture files of every format (current,
  older and newer shapes) with load / migrate / round-trip tests, and a CI check that versions and fixtures never
  change silently. settings.json and update.json now carry "version". See docs/FORMATS.md.
- DE: Dateiformate für 1.0 eingefroren (p62): Jedes gespeicherte Format (Zustand / Sessions, Presets, .ohpetape,
  .ohpewt, .ohpepack, .ohpemidi, settings.json, update.json, die Zustandsabschnitte) hat eine ausdrückliche Version
  an einer Stelle; ältere Dateien laden und werden migriert, unbekannte neuere Felder ignoriert. Goldene Testdateien
  jedes Formats (aktuelle, ältere und neuere Formen) mit Lade-, Migrations- und Rundlauf-Tests sowie eine CI-Prüfung,
  dass sich Versionen und Testdateien nie stillschweigend ändern. settings.json und update.json tragen jetzt
  "version". Siehe docs/de/FORMATE.md.

## 0.6.4 Preview – 2026-10-20

[Notes / Hinweise](docs/release-notes/0.6.4.md)

- EN: Grid keyboard layouts Sequential, In Key / Chromatic and Fixed C with a chord guide; Grid-Controller grid options on Scales, many more buttons and a browse mode on its display, sustain pedal in its pedal jack; arpeggiator held by sustain / sostenuto; smoother FX editor.
- DE: Raster-Anordnungen Sequenziell, Nur Tonart / Chromatisch und C fest mit Akkord-Leitfaden; Grid-Controller mit Raster-Optionen auf Scales, vielen weiteren Tasten und Bibliotheks-Modus im Display, Haltepedal an seiner Pedalbuchse; Arpeggiator von Sustain / Sostenuto gehalten; flüssigerer FX-Editor.

- EN: Arpeggiator (p60): the sustain pedal (CC 64, from MIDI, host, grid controller or an MPE master channel) now
  holds the arpeggio's notes – released keys stay in the pattern until pedal up, keys pressed meanwhile are added;
  sostenuto (CC 66) holds only the keys down when pressed; pedal up with no key held stops the arpeggio cleanly.
  The pedal used to reach only the part's voices, so the arpeggio stopped with the keys while its notes smeared.
  With Latch on, keys under the pedal add to the latched set.
- DE: Arpeggiator (p60): das Haltepedal (CC 64, von MIDI, Host, Grid-Controller oder einem MPE-Master-Kanal) hält
  jetzt die Noten des Arpeggios – losgelassene Tasten bleiben bis Pedal hoch im Muster, in der Zeit gedrückte kommen
  dazu; Sostenuto (CC 66) hält nur die beim Treten gedrückten Tasten; Pedal hoch ohne gehaltene Taste stoppt das
  Arpeggio sauber. Das Pedal erreichte vorher nur die Stimmen des Parts: das Arpeggio stoppte mit den Tasten, seine
  Noten verschmierten. Mit Halten ergänzen Tasten unter dem Pedal den gehaltenen Satz.
- EN: FX editor: the picture now follows encoder, MIDI and host changes smoothly (it redrew only by chance before); curves are cached; long parameter names fit the bar list; German EQ terms use the studio words (Low Shelf, High Shelf, Q, Low Cut, High Cut).
- DE: FX-Editor: das Bild folgt Encoder-, MIDI- und Host-Änderungen jetzt flüssig (vorher nur zufällig neu gezeichnet); Kurven zwischengespeichert; lange Parameternamen passen in die Balkenliste; deutsche EQ-Begriffe wie im Studio (Low Shelf, High Shelf, Q, Low Cut, High Cut).
- EN: Grid controller (p58): a sustain pedal in the controller's pedal jack now holds notes (CC 64 sustain, CC 66
  sostenuto, CC 67 passed on) like from any MIDI keyboard, also in MPE / Fingerboard parts; it was taken as a button
  press and dropped. Display line 3: a long preset name is cut to its column with a space before "Part 1".
- DE: Grid-Controller (p58): ein Haltepedal an der Pedalbuchse des Controllers hält jetzt Noten (CC 64 Sustain, CC 66
  Sostenuto, CC 67 weitergereicht) wie von jedem MIDI-Keyboard, auch in MPE-/Fingerboard-Parts; es wurde als
  Tastendruck genommen und verworfen. Display-Zeile 3: ein langer Presetname wird auf seine Spalte gekürzt, mit
  Leerzeichen vor „Part 1".
- EN: Grid keyboard layouts (p59): new layout Sequential (+1 / +8, continuous rows); new switches In Key / Chromatic
  (`harmony.kb.inkey`, every layout: only key notes with rows in scale degrees, or every note with the others dark) and
  Fixed C (`harmony.kb.fixed`; off, the new default: the bottom-left cell is the key's root, so chord shapes are the same
  in every key). The old In Key layout opens as Fourths + In Key. Encoder page Harmony > Grid, Grid-Controller display
  line 4 shows layout / In Key / Fixed. Chord guide with grip diagrams in Harmony and the in-app help.
- EN: Grid-Controller (p59): holding Scales shows the grid options, the upper buttons choose layout, In Key, Fixed C,
  root and scale (tap = next layout). More buttons mapped and lit by state: Mute / Solo (+ part buttons), Volume and
  Pan & Send encoder pages for the four parts, Device (FX editor), Browse (preset browser on the display: list with
  neighbours, category / engine / source / favourites / tag / rating / sort on the encoders, up / down audition, Select
  or a part button loads, Shift = favourite; the screen library follows), Track (mixer), Clip (tape), Accent (full
  velocity), Quantize, Duplicate / Double (step pattern), Delete + step, New (start sound), Fixed Length (step length),
  Automation (motion record), Add Effect, User (breeding), timing buttons 1/4 ... 1/32t = note repeat while held.
- DE: Raster-Anordnungen (p59): neue Anordnung Sequenziell (+1 / +8, fortlaufende Reihen); neue Schalter Nur Tonart /
  Chromatisch (`harmony.kb.inkey`, jede Anordnung) und C fest (`harmony.kb.fixed`; aus, neu ab Werk: unten links liegt
  der Grundton der Tonart, Akkordformen sind in jeder Tonart gleich). Die frühere Anordnung In Tonart öffnet als Quarten +
  Nur Tonart. Akkord-Leitfaden mit Griffbildern. Grid-Controller: Scales halten zeigt die Raster-Optionen; weitere Tasten
  belegt (Mute / Solo, Volume, Pan & Send, Device, Browse mit Preset-Browser im Display, Track, Clip, Accent, Quantize,
  Duplicate, Double, Delete, New, Fixed Length, Automation, Add Effect, User, Notenwiederholung 1/4 ... 1/32t).

## 0.6.3 Preview – 2026-10-19

[Notes / Hinweise](docs/release-notes/0.6.3.md)

- EN: Library search across all categories and tags with MPE / Fingerboard / Morph chips, Grid-Controller connected automatically with a Controller settings page, graphical FX editor.
- DE: Bibliotheks-Suche über alle Kategorien und Tags mit Chips MPE / Fingerboard / Morph, Grid-Controller automatisch verbunden mit Einstellungsseite Controller, grafischer FX-Editor.

- EN: Grid controller (p53): on by default and connected automatically in the standalone app and the plug-in
  (user port in and out of the same device, identity request tells the generation, hot-plug); switched into its User
  mode on connect with a short "connecting ... / connected" splash on its display, back to its own mode on release.
  New Settings > Controller page: port menus with full names, status line (Connected / Checking / Not found / Port in
  use), strip and master encoder, a large live virtual controller usable with the mouse. The pads follow the on-screen
  keyboard (grid layout while the grid keyboard is shown, Fourths with the piano keys; Scales shows the grid); held
  notes show on both (pad held = cell pressed on the screen, cell clicked = its pads lit).
  Fixes: output port stayed "-" (standalone used no automatic matching); the standalone MIDI page showed the
  plug-in text and the controller also played as a plain MIDI input (encoder / strip touch notes reached the sound
  and retriggered held notes); disabled chips looked selected; the controller section was clipped.
- DE: Grid-Controller (p53): ab Werk an und automatisch verbunden in Standalone-App und Plug-in (User-Port Eingang
  und Ausgang desselben Geräts, Identitätsanfrage nennt die Generation, Anstecken im Betrieb); beim Verbinden in
  seinen User-Modus geschaltet, mit kurzem „verbindet ... / verbunden" auf seinem Display, beim Loslassen zurück in
  seinen eigenen Modus. Neue Seite Einstellungen > Controller: Port-Menüs mit vollen Namen, Statuszeile (Verbunden /
  Wird geprüft / Nicht gefunden / Port belegt), Strip und Master-Regler, großer virtueller Controller live, mit der
  Maus bedienbar. Die Pads folgen der Bildschirm-Tastatur (Raster-Anordnung bei gezeigter Raster-Tastatur, Quarten
  bei den Klaviertasten; Scales zeigt das Raster); gehaltene Töne auf beiden (Pad gehalten = Feld gedrückt, Feld
  geklickt = seine Pads leuchten). Behoben: Ausgang blieb „-" (Standalone ohne automatische Zuordnung);
  die MIDI-Seite der Standalone-App zeigte den Plug-in-Text und der Controller spielte zusätzlich als normaler
  MIDI-Eingang (Berührungs-Noten von Reglern / Strip erreichten den Klang und lösten gehaltene Töne neu aus);
  ausgeschaltete Chips sahen gewählt aus; der Controller-Bereich war abgeschnitten.
- EN: FX editor (p55): the selected slot of the Rack / Master view large, with every parameter at once (picture +
  labelled bars) instead of four per encoder page. Edit directly in the picture: EQ band points on the response curve
  (drag = frequency / gain, Alt-drag or wheel = Q, double-click resets), shelves and output; compressor threshold and
  ratio on the transfer curve, knee, makeup, attack / release, live gain reduction; multiband band levels and splits;
  filter cutoff / resonance; distortion drive / bias / tone, pre-filter; rate / depth waves of chorus, phaser, flanger,
  hyper, tremolo, wobble, formant, comb; echo time / feedback taps, cuts, ratio; reverb / hall pre-delay, decay, damping;
  mid/side and width stereo pictures. A clicked point goes onto the encoders (extra page after *Slot*); the encoders
  stay live below the editor. Opens with the FX editor soft key or a click on the slot picture. Every edit is a normal
  parameter change (automation, MIDI learn, host undo). No new host parameters.
- DE: FX-Editor (p55): der gewählte Slot der Ansicht Rack / Master groß, mit allen Parametern auf einmal (Bild +
  beschriftete Balken) statt vier pro Encoder-Seite. Direkt im Bild bearbeiten: EQ-Bandpunkte auf dem Frequenzgang
  (Ziehen = Frequenz / Verstärkung, Alt-Ziehen oder Rad = Güte, Doppelklick setzt zurück), Kuhschwänze und Ausgang;
  Schwelle und Ratio des Kompressors auf der Kennlinie, Knie, Aufholverstärkung, Attack / Release, aktuelle
  Pegelreduktion; Pegel und Trennungen des Multiband; Cutoff / Resonanz des Filters; Drive / Bias / Klang der
  Verzerrung, Vorfilter; Tempo-/Tiefe-Wellen von Chorus, Phaser, Flanger, Hyper, Tremolo, Wobble, Formant, Kamm;
  Echos mit Zeit / Feedback, Sperren, Verhältnis; Pre-Delay, Nachhall, Dämpfung von Reverb / Raum; Stereobilder von
  Mitte/Seite und Breite. Ein angeklickter Punkt liegt auf den Encodern (zusätzliche Seite nach *Slot*); die Encoder
  bleiben unter dem Editor bedienbar. Öffnen mit dem Softkey FX-Editor oder einem Klick auf das Slot-Bild. Jede
  Änderung ist eine normale Parameteränderung (Automation, MIDI-Learn, Rückgängig des Hosts). Keine neuen
  Host-Parameter.
- EN: Library search (p54): every word of the search must match (AND) – name, category and tags in English and German
  (singular, umlauts and "ae" spellings work: Pad, Fläche, Flaeche), MPE, Fingerboard / MPE-HD, Morph, engine, author,
  comment, content pack, bank, kind, favourite and rating (`***`). New quick chips MPE / Fingerboard / Morph (all
  selected must match, combine with the categories). No new host parameters.
- DE: Bibliotheks-Suche (p54): Jedes Wort der Suche muss passen (UND) – Name, Kategorie und Tags deutsch und englisch
  (Einzahl, Umlaute und „ae"-Schreibweisen gehen: Pad, Fläche, Flaeche), MPE, Fingerboard / MPE-HD, Morph, Engine, Autor,
  Kommentar, Inhaltspaket, Bank, Art, Favorit und Bewertung (`***`). Neue Schnell-Chips MPE / Fingerboard / Morph (alle
  gewählten müssen passen, kombinierbar mit den Kategorien). Keine neuen Host-Parameter.

## 0.6.2 Preview – 2026-10-19

[Notes / Hinweise](docs/release-notes/0.6.2.md)

- EN: Grid controller inside the plug-in, the preset name per part, own BREED key in the header.
- DE: Grid-Controller im Plug-in, Preset-Name pro Part, eigene ZUCHT-Taste in der Kopfzeile.

- EN: Grid controller in the plug-in (p49): in VST3 / AU the plug-in opens the controller's user port itself (full
  display, pad colours, pressure, encoders, strip, buttons); ports chosen or found automatically by name (on by
  default), one instance of the DAW holds the controller (window opened or clicked last, Take over here), it keeps
  working with the window closed; "port in use" status with a retry every second. Standalone unchanged.
- DE: Grid-Controller im Plug-in (p49): in VST3 / AU öffnet das Plug-in den User-Port des Controllers selbst (volles
  Display, Pad-Farben, Druck, Regler, Strip, Tasten); Ports gewählt oder automatisch per Name gefunden (ab Werk an),
  eine Instanz der DAW hält den Controller (zuletzt geöffnetes oder angeklicktes Fenster, Hierher holen), er arbeitet
  bei geschlossenem Fenster weiter; Status „Port belegt" mit neuem Versuch jede Sekunde. Standalone unverändert.
- EN: Fix (p50): the preset name follows the edit part. Every part remembers the preset loaded into it (a project
  preset names all four); switching parts – part selector, MIDI, grid controller part buttons – updates the preset
  strip, the library highlight, the controller display and where ▲ / ▼ step from. Sessions keep the four names;
  older sessions give their one name to every part. No new host parameters.
- DE: Fehlerbehebung (p50): Der Preset-Name folgt dem Edit-Part. Jeder Part merkt sich das in ihn geladene Preset
  (ein Projekt-Preset benennt alle vier); ein Part-Wechsel – Part-Auswahl, MIDI, Part-Tasten des Grid-Controllers –
  aktualisiert Preset-Streifen, Markierung in der Bibliothek, Controller-Display und den Ausgangspunkt von ▲ / ▼.
  Sitzungen behalten die vier Namen; ältere Sitzungen geben ihren einen Namen allen Parts. Keine neuen Host-Parameter.
- EN: Breeding has its own header key, **BREED** (next to RANDOM, with LIBRARY and PRO): a click opens / closes the
  breeding view, its light bar is lit while the view is open, right-click = MIDI learn of `lib.breed`. RANDOM only
  rolls now – the hidden hold / right-click shortcut to breeding is gone (in the breeding view RANDOM still rolls a
  new parent B). The preset strip is a little narrower to make room.
- DE: Die Zucht hat eine eigene Taste in der Kopfzeile, **ZUCHT** (neben ZUFALL, bei BIBLIOTHEK und PRO): ein Klick
  öffnet / schließt die Zucht-Ansicht, ihr Lichtbalken leuchtet, solange die Ansicht offen ist, Rechtsklick =
  MIDI-Lernen von `lib.breed`. ZUFALL würfelt jetzt nur noch – das versteckte Halten / der Rechtsklick zur Zucht
  entfällt (in der Zucht-Ansicht würfelt ZUFALL weiterhin einen neuen Elternteil B). Der Preset-Streifen ist dafür
  etwas schmaler.

## 0.6.1 Preview – 2026-10-18

[Notes / Hinweise](docs/release-notes/0.6.1.md)

- EN: Fingerboard mode (MPE HD 14/21-bit, controller smoothing, bend 96, pressure onset, pitch rounding, sostenuto,
  5 Fingerboard presets).
- DE: Fingerboard-Modus (MPE HD 14/21 Bit, Controller-Glättung, Bend 96, Druck-Einsatz, Tonhöhen-Rundung, Sostenuto,
  5 Fingerboard-Presets).

- EN: Fingerboard mode (p50) for continuous-surface fingerboard MPE controllers: MPE HD (14-bit pressure / Y, 21-bit
  bend via CC 87), the controller's smoothing cutoffs (NRPN 100–102), RPN 0 bend range and RPN 6 zone, note level from
  pressure, optional pitch rounding, the matrix source Key + Bend, sostenuto (CC 66), a one-tap button per part on
  the MIDI settings page with HD status, five Fingerboard presets.
- DE: Fingerboard-Modus (p50) für MPE-Fingerboard-Controller mit durchgehender Fläche: MPE HD (14 Bit Druck / Y,
  21 Bit Bend über CC 87), die Glättungs-Grenzfrequenzen des Controllers (NRPN 100–102), RPN-0-Bend-Bereich und
  RPN-6-Zone, Notenpegel aus dem Druck, wahlweise Tonhöhen-Rundung, die Matrix-Quelle Taste + Bend, Sostenuto (CC 66),
  ein Knopf pro Part auf der MIDI-Einstellungsseite mit HD-Status, fünf Fingerboard-Presets.

## 0.6.0 Preview – 2026-10-18

[Notes / Hinweise](docs/release-notes/0.6.0.md)

- EN: RANDOM rolls usable sounds (musical ranges, no glide, little noise, loudness-matched); tape: clear track / clear
  all, save / load the whole tape (`.ohpetape`); tempo editable on screen, TAP, metronome and count-in; first start
  loads the first factory preset (Aah Choir); own app icon; the 3D switch per part; tape loop by hand; live chord
  names, key / scale on the header and Scale Lock; wordmark and header redesign; 52 new factory presets; grid keyboard
  guide and help; Grid-Controller (Profile 1) in the standalone app (experimental, not yet tested on a real device);
  fix: wavetable packs install from the file dialog.
- DE: ZUFALL würfelt brauchbare Klänge (musikalische Bereiche, kein Glide, wenig Rauschen, lautheitsangeglichen); Band:
  Spur löschen / alles löschen, ganzes Band speichern / laden (`.ohpetape`); Tempo am Display einstellbar, TAP,
  Metronom und Einzähler; der erste Start lädt das erste Werks-Preset (Aah Choir); eigenes App-Symbol; der
  3D-Schalter pro Part; Schleife von Hand; Akkordnamen live, Tonart / Skala im Kopf und Scale Lock („In Tonart“);
  Schriftzug und Kopfzeile neu; 52 neue Werks-Presets; Anleitung und Hilfe zur Raster-Tastatur; Grid-Controller
  (Profil 1) in der Standalone-App (experimentell, noch nicht am echten Gerät getestet); Korrektur:
  Wavetable-Pakete lassen sich über den Dateidialog installieren.

- EN: The 3D switch – per part Off / Headphones / Speakers (`p1.space.mode` … `p4.space.mode`, automatable and
  MIDI-mappable) as a segmented switch on the Space page; a front for `pN.space.on` and `space.out`, older presets and
  sessions load unchanged; speakers icon on the 3D badge when monitoring on speakers – see
  [Space](docs/en/SPACE.md#the-3d-switch-p41).
- DE: Der 3D-Schalter – pro Part Aus / Kopfhörer / Lautsprecher (`p1.space.mode` … `p4.space.mode`, automatisierbar
  und per MIDI steuerbar) als Segmentschalter auf der Raum-Seite; Front für `pN.space.on` und `space.out`, ältere
  Presets und Sessions laden unverändert; Lautsprecher-Symbol am 3D-Badge bei Abhören über Lautsprecher – siehe
  [Raum](docs/de/RAUM.md#der-3d-schalter-p41).
- EN: 52 new factory presets that show off the newer features (48 part presets, 4 projects): Life, the morph pad, user
  wavetables analysed from the factory recordings, the resonator body (stage and rack effect), the character macros,
  3D motion paths, the SUB layer, the Expression page, pressure glide and tempo-synced movement; the projects use the
  harmony brain's Follow, recorded motion lanes, the performance pads and four 3D parts at once. Level-matched to the
  factory median, every one with a musical mod wheel; appended to the bank (the first preset stays the start sound) –
  see [Factory sounds](docs/en/FACTORY-SOUNDS.md#showcase-presets-p44).
- DE: 52 neue Werks-Presets, die die neueren Funktionen zeigen (48 Part-Presets, 4 Projekte): Leben, das Morph-Pad,
  eigene Wavetables aus den Werksaufnahmen, der Resonanzkörper (Stufe und Rack-Effekt), die Charakter-Makros,
  3D-Bewegungspfade, die SUB-Ebene, die Ausdrucks-Seite, Glide-Druck und tempo-synchrone Bewegung; die Projekte nutzen
  das Folgen des Harmonie-Gehirns, aufgenommene Bewegungsspuren, die Performance-Pads und vier 3D-Parts gleichzeitig.
  Auf den Werks-Median eingepegelt, jedes mit musikalischem Modrad; an die Bank angehängt (das erste Preset bleibt der
  Startklang) – siehe [Werksklänge](docs/de/FACTORY-SOUNDS.md#showcase-presets-p44).
- EN: Tape loop by hand – the loop readout on the tape page opens the loop keys: Set In / Set Out at the playhead (also
  while playing; snapped to bar / beat, Shift free; Set Out loops at once), Snap, To In, Half, Double, Fit, Undo / Redo
  of the loop range; drag the loop edges on the lanes; shortcuts I / O; MIDI triggers `tape.edit.loop*`,
  `tape.loopsnap` – see [Setting the loop](docs/en/TAPE-MIXER-FX.md#setting-the-loop-p42).
- DE: Schleife von Hand – die Schleifen-Anzeige der Tape-Seite öffnet die Loop-Tasten: Start / Ende setzen am
  Abspielkopf (auch beim Abspielen; im Takt- / Schlagraster, Shift frei; Ende setzen schleift sofort), Raster, Zum
  Start, Halb, Doppelt, Anpassen, Rückgängig / Wiederherstellen des Bereichs; Kanten auf den Spuren ziehen;
  Kurzbefehle I / O; MIDI-Trigger `tape.edit.loop*`, `tape.loopsnap` – siehe
  [Schleife setzen](docs/de/BAND-MISCHER-FX.md#schleife-setzen-p42).

## 0.5.0 Preview – 2026-10-17

[Notes / Hinweise](docs/release-notes/0.5.0.md)

- EN: pressure glide, SUB layer per part, Expression page (velocity / aftertouch freely assignable, aftertouch → glide
  time on gliding factory parts), mod wheel on all 320 factory presets, six new binaural motion paths, performance pass
  with CPU meter, Settings (night mode, About, Help, MIDI setup and MIDI learn), `.ohpepack` content pack import with
  drag & drop, pack manager and SHA-256 check, wavetable banks from content packs.
- DE: Glide-Druck, SUB-Ebene pro Part, Expression-Seite (Anschlag / Aftertouch frei zuweisbar, Aftertouch → Glide-Zeit
  bei gleitenden Werks-Parts), Mod-Rad auf allen 320 Werks-Presets, sechs neue binaurale Bewegungspfade,
  Leistungsdurchgang mit CPU-Anzeige, Einstellungen (Nachtmodus, Über, Hilfe, MIDI-Einrichtung und MIDI-Learn),
  Import von `.ohpepack`-Inhaltspaketen per Drag & Drop, Paketverwaltung und SHA-256-Prüfung, Wavetable-Bänke aus
  Inhaltspaketen.

- EN: Wavetable banks – content packs of kind `wavetables` add read-only tables to the Wavetable engine's menu
  (Content packs › pack); presets and projects store only `"<pack>/<table>"`; `make_ohpepack.py` /
  `make_manifest.py` build wavetable packs (frame check) – see [Wavetable](docs/en/engines/wavetable.md).
- DE: Wavetable-Bänke – Inhaltspakete der Art `wavetables` bringen schreibgeschützte Tabellen ins Menü der
  Wavetable-Engine (Inhaltspakete › Paket); Presets und Projekte speichern nur `"<Paket>/<Tabelle>"`;
  `make_ohpepack.py` / `make_manifest.py` bauen Wavetable-Pakete (Frame-Prüfung) – siehe
  [Wavetable](docs/de/engines/wavetable.md).
- EN: Content packs – install `.ohpepack` files (file dialog or drop on the window) with a SHA-256 check of every
  file, pack manager (version, size, license / signature shown, remove), library filter by pack, `"<pack>/<item>"`
  references with a clear "content pack missing" message, `tools/pack/make_ohpepack.py` – see
  [Content packs](docs/en/CONTENT-PACKS.md).
- DE: Inhaltspakete – `.ohpepack`-Dateien installieren (Dateidialog oder auf das Fenster ziehen) mit SHA-256-Prüfung
  jeder Datei, Paketverwaltung (Version, Größe, Lizenz / Signatur angezeigt, Entfernen), Bibliotheksfilter nach Paket,
  Referenzen `"<Paket>/<Element>"` mit klarer Meldung „Inhaltspaket fehlt“, `tools/pack/make_ohpepack.py` – siehe
  [Inhaltspakete](docs/de/INHALTSPAKETE.md).
- EN: 3D Space – six new motion paths (Ping-Pong F/B, Swarm, Whisper, Distance Walk, Over the Head, Weave) and a path
  preview in the 3D view.
- DE: 3D-Raum – sechs neue Bewegungspfade (Ping-Pong v/h, Schwarm, Flüstern, Entfernungsgang, Über den Kopf,
  Wellenorbit) und eine Pfad-Vorschau in der 3D-Ansicht.
- EN: Performance pass – CPU meter on the mode column's legend line, silent parts sleep (no tail is cut), faster FM
  engine, hall and phaser on a polynomial sine, only changed parameters per block, shared built-in tones, `ohpe_bench`
  benchmark (typical FM −42 %, silent racks −82 %, sum of all scenarios −14 %) – see [Performance](docs/en/PERFORMANCE.md).
- DE: Leistungsdurchgang – CPU-Anzeige auf der Beschriftungszeile der Modus-Spalte, stille Parts schlafen (kein
  Ausklang wird abgeschnitten), schnellere FM-Engine, Hall und Phaser mit Polynom-Sinus, nur geänderte Parameter pro
  Block, gemeinsame eingebaute Klänge, Benchmark `ohpe_bench` (typisch FM −42 %, stille Racks −82 %, Summe aller
  Szenarien −14 %) – siehe [Leistung](docs/de/LEISTUNG.md).

## 0.4.0 Preview – 2026-10-17

[Notes / Hinweise](docs/release-notes/0.4.0.md)

- EN: user audio to wavetable, resonator body, preset breeding, Pro view 2.0 with Discover, bounce to engine, motion
  recorder, living display, performance pads, harmony brain with grid keyboard, in-app updates; fixes (Grain tape
  source, wheels follow external MIDI, arpeggiator not latched by default, pads not restored as held, stray ring in
  the Pro view).
- DE: eigene Klänge als Wavetables, Resonanzkörper, Preset-Zucht, Pro-Ansicht 2.0 mit Entdecken, Bounce ins
  Instrument, Bewegungs-Recorder, Lebendes Display, Performance-Pads, Harmonie-Hirn mit Raster-Tastatur, Updates in
  der App; Korrekturen (Grain-Quelle Band, Räder folgen externem MIDI, Arpeggiator nicht mehr standardmäßig gehalten,
  Pads nicht als gehalten wiederhergestellt, verirrter Ring in der Pro-Ansicht).

## 0.3.0 Preview – 2026-10-17

- EN: life per part, morph space. DE: Leben je Part, Morph-Raum.

## 0.2.0 Preview – 2026-10-16

- EN: redesign. DE: Neugestaltung.
