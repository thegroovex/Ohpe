# Changelog / Änderungen

Ohpe – designed by groovex. Each release's full notes (English and Deutsch) are in
[docs/release-notes](docs/release-notes/) and on the [releases page](https://github.com/thegroovex/Ohpe/releases).
Die vollständigen Hinweise jeder Version (Deutsch und English) stehen in [docs/release-notes](docs/release-notes/)
und auf der [Release-Seite](https://github.com/thegroovex/Ohpe/releases).

## Unreleased

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

## 0.5.0 Preview – 2026-10-17

[Notes / Hinweise](docs/release-notes/0.5.0.md)

- EN: Tape loop by hand – the loop readout on the tape page opens the loop keys: Set In / Set Out at the playhead (also
  while playing; snapped to bar / beat, Shift free; Set Out loops at once), Snap, To In, Half, Double, Fit, Undo / Redo
  of the loop range; drag the loop edges on the lanes; shortcuts I / O; MIDI triggers `tape.edit.loop*`,
  `tape.loopsnap` – see [Setting the loop](docs/en/TAPE-MIXER-FX.md#setting-the-loop-p42).
- DE: Schleife von Hand – die Schleifen-Anzeige der Tape-Seite öffnet die Loop-Tasten: Start / Ende setzen am
  Abspielkopf (auch beim Abspielen; im Takt- / Schlagraster, Shift frei; Ende setzen schleift sofort), Raster, Zum
  Start, Halb, Doppelt, Anpassen, Rückgängig / Wiederherstellen des Bereichs; Kanten auf den Spuren ziehen;
  Kurzbefehle I / O; MIDI-Trigger `tape.edit.loop*`, `tape.loopsnap` – siehe
  [Schleife setzen](docs/de/BAND-MISCHER-FX.md#schleife-setzen-p42).

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
