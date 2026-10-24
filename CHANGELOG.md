# Changelog / Änderungen

Ohpe – designed by groovex. Each release's full notes (English and Deutsch) are in
[docs/release-notes](docs/release-notes/) and on the [releases page](https://github.com/thegroovex/Ohpe/releases).
Die vollständigen Hinweise jeder Version (Deutsch und English) stehen in [docs/release-notes](docs/release-notes/)
und auf der [Release-Seite](https://github.com/thegroovex/Ohpe/releases).

## Unreleased / Unveröffentlicht

- EN: Accessibility and UI size (p83) – six UI sizes 75 / 100 / 125 / 150 / 175 / 200 % (Settings › Display, or a
  right-click on the housing or the settings key; per computer in settings.json, never in projects), vector drawing
  sharp at every size and on HiDPI screens, the window keeps its aspect ratio. Full keyboard operation: Tab / Shift+Tab
  walk the controls area by area, F6 jumps between the areas, a yellow focus ring in the Ohpe design shows where you
  are; arrows / Page Up / Page Down change values, Delete resets, Enter / Space press keys, Shift+F10 opens a key's
  menu, Esc gives the computer keyboard back to the notes (a click never steals it). Screen reader names and values in
  English and German for every control and the display. Contrast to WCAG AA: brighter dim display texts, a fine edge
  on every lit light bar (≥ 3:1 on its key), night skin legends ≥ 4.5:1. No new host parameters. See
  [accessibility](docs/en/ACCESSIBILITY.md).
- DE: Barrierefreiheit und UI-Größe (p83) – sechs UI-Größen 75 / 100 / 125 / 150 / 175 / 200 % (Einstellungen ›
  Display oder Rechtsklick aufs Gehäuse bzw. die Zahnrad-Taste; pro Computer in settings.json, nie im Projekt),
  Vektorgrafik scharf in jeder Größe und auf HiDPI-Bildschirmen, das Fenster behält sein Seitenverhältnis. Volle
  Bedienung per Tastatur: Tab / Shift+Tab wandern Bereich für Bereich durch die Bedienelemente, F6 springt zwischen den
  Bereichen, ein gelber Fokusring im Ohpe-Design zeigt die Stelle; Pfeile / Bild ↑ / Bild ↓ ändern Werte, Entf setzt
  zurück, Enter / Leertaste drücken Tasten, Shift+F10 öffnet das Menü einer Taste, Esc gibt die Computertastatur an die
  Noten zurück (ein Klick nimmt sie ihr nie). Screenreader-Namen und -Werte auf Deutsch und Englisch für jedes
  Bedienelement und das Display. Kontrast nach WCAG AA: hellere gedimmte Display-Texte, eine feine Kante an jedem
  leuchtenden Leuchtbalken (≥ 3:1 zur Taste), Beschriftungen der Nacht-Erscheinung ≥ 4,5:1. Keine neuen
  Host-Parameter. Siehe [Barrierefreiheit](docs/de/BARRIEREFREIHEIT.md).
- EN: Tooltips on every control and a help search (p82) – every interactive control (keys, encoders, wheels, the
  display's rows, arrows, soft keys, tabs, steps, pads and tracks, pro view, FX editor, library, breeding, settings,
  update and pack pages, dialogs, tour) has a concise English and German tooltip: what it does, plus *Right-click: MIDI
  learn* where a right-click learns it. A test walks every view and fails on a control without both tooltips and on
  any German text missing from the translation table. Settings > Help has a search field (Ctrl / Cmd + F): English or
  German words, fuzzy (case, umlauts, word beginnings, typos), finds help topics, parameters and controls and jumps to
  them – the parameter's page with its encoder ringed in yellow, the control ringed on the plate. No new parameters.
- DE: Tooltips an jedem Bedienelement und eine Hilfesuche (p82) – jedes Bedienelement (Tasten, Encoder, Räder, die
  Zeilen, Pfeile, Softkeys, Reiter, Schritte, Pads und Spuren des Displays, Profiansicht, FX-Editor, Bibliothek, Zucht,
  Einstellungs-, Update- und Paketseiten, Dialoge, Tour) hat einen kurzen deutschen und englischen Tooltip: was es tut,
  dazu *Rechtsklick: MIDI-Learn*, wo ein Rechtsklick es lernt. Ein Test geht durch jede Ansicht und schlägt fehl, wenn
  einem Element einer der beiden Tooltips fehlt oder ein deutscher Text in der Übersetzungstabelle fehlt. Einstellungen
  > Hilfe hat ein Suchfeld (Strg / Cmd + F): deutsche oder englische Wörter, fehlertolerant (Groß-/Kleinschreibung,
  Umlaute, Wortanfänge, Tippfehler), findet Hilfethemen, Parameter und Bedienelemente und springt hin – zur Seite des
  Parameters mit gelb umrandetem Encoder, zum umrandeten Element auf der Oberfläche. Keine neuen Parameter.

## 0.7.3 – 2026-10-23

[Notes / Hinweise](docs/release-notes/0.7.3.md)

- EN: 3D motion retrigger and 3D amount (p77) – the binaural motion can now restart with the notes, like the key
  sync of an arpeggiator or LFO: new per-part **Motion Retrigger** (`pN.space.retrig`: *Free* = runs on with the song
  as before, *First* = restarts from its start position on a note after all keys were up, *Note* = on every note; mono
  legato notes keep it running). The position glides to the start in about 8 ms – no clicks. New per-part **Amount**
  (`pN.space.amount`, 0–100 %, default 100 % = unchanged): sets the strength of the 3D effect, from full binaural
  position, motion and colouring down to the plain stereo sound, at about the same level; smoothed and click-free,
  mod matrix target 95 *3D Amount*. Both on the new 3D page *Amount & Retrigger*, MIDI-mappable and automatable. The
  motion speed already reaches one cycle per 1/64 note (Motion Length 1/16 at ×4, tempo-synced) – no range change.
  12 factory 3D presets whose motion was too slow to hear on normal playing restart with the notes and/or move
  faster (Comet Lead, Ear to Ear Lead, Pendulum Lead, Vowel Orbit Lead, Overhead Flute, Orbiting Tines, Glass
  Pendulum, Ceiling Harpsichord, Vibes in the Round, Ping-Pong Space Pluck, Doppler Fly-By, Breath Circle); names kept.
- DE: 3D-Bewegungs-Retrigger und 3D-Stärke (p77) – die binaurale Bewegung kann jetzt mit den Noten neu starten, wie
  der Key-Sync eines Arpeggiators oder LFOs: neuer **Bewegung Retrigger** je Part (`pN.space.retrig`: *Frei* = läuft
  wie bisher mit dem Song weiter, *Erste* = beginnt wieder am Startpunkt bei einer Note, nachdem alle Tasten
  losgelassen waren, *Note* = bei jeder Note; Mono-Legato-Noten lassen sie weiterlaufen). Die Position gleitet in etwa
  8 ms zum Start – kein Knacken. Neue **Stärke** je Part (`pN.space.amount`, 0–100 %, Standard 100 % = unverändert):
  stellt die Stärke des 3D-Effekts ein, von voller binauraler Position, Bewegung und Färbung bis zum normalen
  Stereoklang, bei etwa gleichem Pegel; geglättet und klickfrei, Modulationsziel 95 *3D-Stärke*. Beide auf der neuen
  3D-Seite *Stärke & Retrigger*, MIDI-zuweisbar und automatisierbar. Das Bewegungstempo reicht schon bis zu einem
  Durchlauf pro 1/64-Note (Bewegungslänge 1/16 bei ×4, im Tempo) – keine Bereichsänderung. 12 3D-Werkspresets, deren
  Bewegung beim normalen Spielen zu langsam war, starten mit den Noten neu und/oder bewegen sich schneller; Namen
  bleiben.

## 0.7.2 – 2026-10-22

[Notes / Hinweise](docs/release-notes/0.7.2.md)

- EN: Clean chords (p74) – fixed grit / crackle when several notes (4 and up) play at once on sounds such as Dust
  Orchestra. Cause: chords summed without headroom ran into the fast peak guard of those presets (a compressor with
  0.1 ms attack that squashed held chords by up to 11 dB) or, on other presets, far above full scale into the master
  limiter (up to 12 dB of limiting). New per-part **Chord Headroom** (`pN.chordroom`, 0–24 dB at 8 notes): a smooth,
  voice-count-aware gain right after the engines (1 note untouched, glides down within a few ms when notes start,
  back over ~250 ms – no pumping). Every factory part preset is calibrated so a held 8-note chord at velocity 127
  stays at or below −1 dBTP without the limiter while single notes keep their level; presets with a peak guard get
  enough chord headroom that chords reach the guard at most 3 dB hotter than single notes (Dust Orchestra: no gain
  reduction on chords any more, gentler guard, single notes near −11 LUFS). New regression test (all part presets, 8-note chord at velocity 127,
  ≤ −1 dBTP, no hard clip) and an offline polyphony audit (`tools/presets/poly_audit.py`). Older sessions get 4.5 dB
  of chord headroom: single notes sound the same, big chords a little quieter and cleaner. MIDI-mappable.
- DE: Saubere Akkorde (p74) – Kratzen / Knistern behoben, wenn mehrere Noten (ab 4) gleichzeitig klingen, etwa bei
  Dust Orchestra. Ursache: Akkorde wurden ohne Reserve summiert und liefen in den schnellen Spitzenschutz dieser
  Presets (ein Kompressor mit 0,1 ms Attack, der gehaltene Akkorde um bis zu 11 dB drückte) oder, bei anderen Presets,
  weit über Vollaussteuerung in den Master-Limiter (bis zu 12 dB Begrenzung). Neu pro Part: **Akkord-Reserve**
  (`pN.chordroom`, 0–24 dB bei 8 Noten): eine weiche, stimmenzahl-abhängige Verstärkung direkt nach den Engines
  (1 Note unberührt, in wenigen ms herunter, wenn Noten beginnen, in ~250 ms zurück – kein Pumpen). Jedes
  Werks-Part-Preset ist so eingemessen, dass ein gehaltener 8-Noten-Akkord mit Anschlag 127 ohne Limiter bei höchstens
  −1 dBTP bleibt und einzelne Noten ihren Pegel behalten; Presets mit Spitzenschutz bekommen so viel Akkord-Reserve,
  dass Akkorde ihn höchstens 3 dB heißer erreichen als einzelne Noten (Dust Orchestra: keine Pegelreduktion mehr auf
  Akkorden, sanfterer Schutz, einzelne Noten nahe −11 LUFS). Neuer Regressionstest (alle Part-Presets, 8-Noten-Akkord mit Anschlag 127,
  ≤ −1 dBTP, kein hartes Clipping) und ein Offline-Polyphonie-Audit (`tools/presets/poly_audit.py`). Ältere Sessions
  bekommen 4,5 dB Akkord-Reserve: einzelne Noten klingen gleich, große Akkorde etwas leiser und sauberer.
  MIDI-zuweisbar.

- EN: Pluck engine – a part set to one voice that receives a chord is plucked once (the newest key wins) instead
  of once per key.
- DE: Pluck-Engine – ein Part mit einer Stimme, der einen Akkord bekommt, wird einmal gezupft (die neueste Taste
  gewinnt) statt einmal pro Taste.
- EN: Interface (p75) – German interface: **Behalten** is now **Übernehmen**, the mixer output **Haupt** is now
  **Main**; mode and engine titles on the display are never truncated; a key passed in by a host turns the key
  readout violet.
- DE: Oberfläche (p75) – deutsche Oberfläche: **Behalten** heißt jetzt **Übernehmen**, der Mixer-Ausgang **Haupt**
  heißt jetzt **Main**; Modus- und Engine-Titel im Display werden nie abgeschnitten; gibt ein Host eine Tonart mit,
  wird die Tonart-Anzeige violett.

## 0.7.1 Preview – 2026-10-22

[Notes / Hinweise](docs/release-notes/0.7.1.md)

- EN: Eight new 3D motion paths, 49 binaural factory presets, RANDOM loudness fix, Key Gravity ring on the header,
  Tape-Memory fix (a preset loaded after CAPTURE loads completely; restore last capture).
- DE: Acht neue 3D-Bewegungspfade, 49 binaurale Werks-Presets, Lautheits-Fix für ZUFALL, Ring der
  Tonart-Gravitation in der Kopfzeile, Tape-Memory-Fehlerbehebung (ein Preset nach MERKEN lädt vollständig; zuletzt
  Gemerktes zurückholen).

- EN: 3D Space – eight new motion paths (p73), appended to the path list (choices 13–20; older sounds and automation
  keep their path and sound identical): **Ping-Pong L/R** (ear to ear on the beat, in front one way, behind the other),
  **Four Corners** (front left → front right → back right → back left, one per quarter), **Breathe** (near ↔ far in
  front), **Helix** (circles up above your head and back down), **Fly-Over** (a straight line over your head, front to
  back), **Bounce** (hops like a ball left ↔ right), **Wander** (a smooth random loop that repeats every cycle) and
  **Loop Orbit** (an orbit with four small loops). All tempo-synced, continuous at the loop point, MIDI-mappable,
  drawn ahead of the dot in the 3D view; new help topic *3D motion paths*. Eight binaural presets now show them off
  (Ear to Ear Lead, Helix Sequence, Marimba Ricochet, Bell Ricochet, Distant Voices, Ember Field, Satellite Arp, Glass
  Horizon). No new parameters.
- DE: 3D-Raum – acht neue Bewegungspfade (p73), hinten an die Pfadliste angehängt (Auswahl 13–20; ältere Klänge und
  Automationen behalten ihren Pfad und klingen gleich): **Ping-Pong l/r** (im Takt von Ohr zu Ohr, in eine Richtung vor
  dir, zurück hinter dir), **Vier Ecken** (vorn links → vorn rechts → hinten rechts → hinten links, eine pro Viertel),
  **Atmen** (nah ↔ fern vor dir), **Helix** (kreist über deinen Kopf hinauf und wieder hinab), **Überflug** (eine
  gerade Linie über deinem Kopf, von vorn nach hinten), **Hüpfen** (springt wie ein Ball links ↔ rechts), **Wandern**
  (eine weiche Zufallsschleife, die sich jeden Durchlauf wiederholt) und **Schleifenorbit** (ein Orbit mit vier
  kleinen Schleifen). Alle tempo-synchron, stetig am Schleifenpunkt, MIDI-zuweisbar, in der 3D-Ansicht vor dem Punkt
  gezeichnet; neues Hilfethema *3D-Bewegungspfade*. Acht binaurale Presets zeigen sie (Ear to Ear Lead, Helix Sequence,
  Marimba Ricochet, Bell Ricochet, Distant Voices, Ember Field, Satellite Arp, Glass Horizon). Keine neuen Parameter.
- EN: 49 new binaural factory presets (p71), made for headphones and built around the part's 3D stage: pads circling
  the head, near whispers and far-off voices, rain-like grain fields, plucks and bells placed around the listener,
  percussion whose hits land at different places (pitch or chance picks the direction), basses with a centred mono
  low end and moving overtones, travelling arps, plus Key Gravity and Voice Control combinations. Every engine and
  ten categories; each carries its category plus **3D** (library 3D chip, search *3D* / *binaural*, headphones
  pictogram), has an audible mod wheel and is calibrated to the bank target (−11 LUFS short-term, true peak ≤ −1 dBTP).
  The factory bank now holds 412 part presets (72 with 3D). Generator tools/presets/make_binaural.py; list in
  docs/en/FACTORY-SOUNDS.md.
- DE: 49 neue binaurale Werks-Presets (p71) für Kopfhörer, um die 3D-Stufe des Parts gebaut: Flächen, die um den Kopf
  kreisen, nahes Flüstern und ferne Stimmen, regenartige Grain-Felder, Zupfer und Glocken rund um dich, Percussion,
  deren Schläge an verschiedenen Orten landen (Tonhöhe oder Zufall wählt die Richtung), Bässe mit mittigem Mono-
  Tiefbass und wandernden Obertönen, reisende Arps, dazu Kombinationen mit Tonart-Gravitation und Stimmsteuerung.
  Alle Engines und zehn Kategorien; jedes trägt seine Kategorie und **3D** (3D-Chip der Bibliothek, Suche *3D* /
  *binaural*, Kopfhörer-Piktogramm), hat ein hörbares Modrad und ist auf das Bank-Ziel eingepegelt (−11 LUFS
  Kurzzeit, True Peak ≤ −1 dBTP). Die Werksbank hat jetzt 412 Part-Presets (72 mit 3D). Generator
  tools/presets/make_binaural.py; Liste in docs/de/FACTORY-SOUNDS.md.
- EN: Key Gravity on the header (p70): a small ring right next to the key and its lock shows the edit part's gravity
  amount (empty at 0 %). Click opens the Key Gravity page directly, drag / mouse wheel set the amount, right-click =
  MIDI learn; tooltip and help topic EN / DE. The Sends pages and the pro view card stay. No new parameters.
- DE: Tonart-Gravitation in der Kopfzeile (p70): ein kleiner Ring direkt neben der Tonart und ihrem Schloss zeigt die
  Gravitation des Edit-Parts (bei 0 % leer). Klick öffnet die Seite Tonart-Gravitation direkt, Ziehen / Mausrad
  stellen den Wert ein, Rechtsklick = MIDI-Learn; Tooltip und Hilfethema DE / EN. Die Sends-Seiten und die Karte der
  Pro-Ansicht bleiben. Keine neuen Parameter.
- EN: Fix (Tape Memory): a preset loaded after CAPTURE (library, preset strip arrows, project) now loads completely –
  engine, factory instrument or sample, parameters – instead of the captured sound playing on. The capture is set
  aside, not lost: MEMORY > *Restore last capture* puts it back, export still works, library browsing without
  loading brings it back, and a project saved with a capture keeps it. RANDOM and BREED keep the capture (they never
  change the sample choice); a capture never reaches other parts or presets.
- DE: Fehlerbehebung (Tape-Memory): Ein Preset, das nach MERKEN geladen wird (Bibliothek, Pfeile der Preset-Leiste,
  Projekt), lädt jetzt vollständig – Engine, Werksinstrument oder Sample, Parameter –, statt dass der gemerkte Klang
  weiterspielt. Das Gemerkte wird beiseitegelegt, nicht verworfen: MEMORY > *Zuletzt Gemerktes zurückholen* holt es
  zurück, der Export geht weiter, Stöbern ohne Laden bringt es zurück, und ein mit Gemerktem gespeichertes Projekt
  behält es. ZUFALL und ZUCHT behalten das Gemerkte (sie ändern die Sample-Wahl nie); Gemerktes gelangt nie in andere
  Parts oder Presets.
- DE: Die Funktion heißt in Oberfläche, Hilfe und Dokumentation jetzt durchgehend „Tape-Memory“;
  die Anleitung liegt unter [docs/de/TAPE-MEMORY.md](docs/de/TAPE-MEMORY.md).

## 0.7.0 Preview – 2026-10-22

[Notes / Hinweise](docs/release-notes/0.7.0.md)

- EN: Key Gravity (partials pulled to key and chord), Voice Control (a live voice as seven modulation sources), Tape Memory (capture the last 30 s as a playable instrument), learning BREED key, first-start tour, factory bank curated to 374 presets at −11 LUFS, loader hardening with crash-safe start, versioned file formats with golden fixtures.
- DE: Tonart-Gravitation (Teiltöne zu Tonart und Akkord gezogen), Stimmsteuerung (eine Live-Stimme als sieben Modulationsquellen), Tape-Memory (die letzten 30 s als spielbares Instrument festhalten), lernende BREED-Taste, Einführung beim ersten Start, Werksbank auf 374 Presets bei −11 LUFS kuratiert, robustes Laden mit absturzsicherem Start, versionierte Dateiformate mit Referenzdateien.

- EN: Key Gravity (p66): a spectral stage per part right after the engines pulls every partial toward the key / scale
  and the held chord (Scale, Chord, Chord+Scale; host key when available). Gravity, Target, Glide (partials flow on
  chord changes), Curve (gentle … magnetic), Formant keep, Mix, Quality (Low Latency / Normal / High). Transients
  pass; at high amounts noise and drums become pitched texture in key. Gravity 0 is bit-identical with no latency and
  no CPU; while a stage runs, the host is told its latency and all parts and the drum kit stay aligned. Effects mode
  pages and pro card "Key Gravity", MIDI-learnable, matrix targets 90 – 94, four factory sounds (found via the "Gravity …" name prefix). New
  host parameters pN.grav.* appended. Details: docs/en/KEY-GRAVITY.md.
- DE: Tonart-Gravitation (p66): eine Spektral-Stufe pro Part direkt nach den Engines zieht jeden Teilton zur Tonart /
  Skala und zum gehaltenen Akkord (Skala, Akkord, Akkord+Skala; Tonart des Hosts, wenn vorhanden). Gravitation, Ziel,
  Glide (Teiltöne fließen bei Akkordwechseln), Kurve (sanft … magnetisch), Formant-Erhalt, Mischung, Qualität
  (Geringe Latenz / Normal / Hoch). Anschläge bleiben; bei hohen Werten werden Rauschen und Drums zu tonalen Texturen
  in der Tonart. Gravitation 0 ist bit-identisch, ohne Latenz und ohne CPU; läuft eine Stufe, erfährt der Host ihre
  Latenz und alle Parts und das Drum-Kit bleiben zeitgleich. Effekt-Seiten und Pro-Karte „Tonart-Gravitation", per
  MIDI lernbar, Matrix-Ziele 90 – 94, vier Werksklänge (zu finden über das Namenspräfix „Gravity …“). Neue Host-Parameter pN.grav.* angehängt.
  Details: docs/de/TONART-GRAVITATION.md.
- EN: Voice Control (p68): a live audio input is analysed in real time (about 8 ms) into seven new modulation sources –
  Voice Level, Vowel X / Y (formants on an A-E-I-O-U map), Voice Brightness, Voice Noisiness (consonants), Voice Pitch
  (optional) and Voice Onset. Not a vocoder: the voice only modulates; monitoring is off by default. LFO > Voice shows
  the vowel map and the input meter; *Vowel → Filter + Morph* maps it to the edit part in one press. Gain, gate,
  smoothing, pitch tracking and monitor are host / MIDI parameters (appended, not stored in presets). The standalone
  app gains an input device choice (none by default, macOS microphone text in English and German); plug-ins use their
  existing optional input bus (unchanged layouts). Three factory presets (names start with "Voice"). Docs:
  docs/en/VOICE-CONTROL.md.
- DE: Stimmsteuerung (p68): Ein Live-Audio-Eingang wird in Echtzeit (etwa 8 ms) in sieben neue Modulationsquellen
  zerlegt – Stimme Pegel, Vokal X / Y (Formanten auf einer A-E-I-O-U-Karte), Stimme Helligkeit, Stimme Rauschanteil
  (Konsonanten), Stimme Tonhöhe (optional) und Stimme Einsatz. Kein Vocoder: Die Stimme moduliert nur; Abhören ist
  anfangs aus. LFO > Stimme zeigt Vokalkarte und Eingangsanzeige; *Vokal → Filter + Morph* legt sie mit einem Druck auf
  den Edit-Part. Eingangspegel, Schwelle, Glättung, Tonhöhenerkennung und Abhören sind Host-/MIDI-Parameter
  (angehängt, nicht in Presets gespeichert). Die Standalone-App bekommt eine Eingangsgeräte-Wahl (anfangs keins,
  macOS-Mikrofontext auf Deutsch und Englisch); Plug-ins nutzen ihren vorhandenen optionalen Eingangsbus
  (unveränderte Layouts). Drei Werks-Presets (Namen beginnen mit „Voice“). Doku: docs/de/STIMMSTEUERUNG.md.
- EN: Learning breed (p69): the BREED key learns locally from use – time played with notes, saved, loaded again,
  favourited, kept, quickly skipped (negative) – and keeps a small taste profile per engine. BREED / AGAIN breed more
  candidates and show the best-scoring ones, mutation leans towards the liked region, the die's parent B follows the
  taste. Breeding view foot row: LEARN on/off, CURIOSITY 0–100 % (100 % = unbiased), WHY (top learned traits), RESET.
  Levels stay matched, safety caps stay. New local file `Taste/taste.json` (format `taste` 1); nothing is sent anywhere.
- DE: Lernende Zucht (p69): die ZUCHT-Taste lernt lokal aus der Nutzung – Spielzeit mit Noten, gespeichert, wieder
  geladen, Favorit, behalten, schnell übersprungen (negativ) – und führt ein kleines Geschmacksprofil pro Engine.
  ZÜCHTEN / WEITER züchten mehr Kandidaten und zeigen die bestbewerteten, die Mutation neigt sich zum gemochten
  Bereich, der Würfel-Elternteil B folgt dem Geschmack. Fußzeile der Zucht-Ansicht: LERNEN an/aus, NEUGIER 0–100 %
  (100 % = unbeeinflusst), WARUM (stärkste gelernte Merkmale), VERGESSEN. Pegel bleiben angeglichen, Sicherheitsgrenzen
  bleiben. Neue lokale Datei `Taste/taste.json` (Format `taste` 1); nichts wird gesendet.
- EN: Tape Memory (p67): Ohpe always keeps the last 30 s of its own output (main out or one part, 16-bit ring buffer,
  about 6 MB at 48 kHz). CAPTURE (tape page, Sampler page, MIDI-mappable `tapemem.go`, Grid-Controller *Add Track*)
  turns it into an instrument in a part's Sampler: Chromatic (the strongest note, pitch detected and tuned to the
  nearest semitone, root set) or Slices (one slice per onset from C2). The analysis runs off the audio thread;
  captures are saved inside the session (new state section `tapememory`, format `tapeMemory` 1) and export as WAV to
  the capture library. 5 IDs `tapemem.len/src/mode/to/go` appended after `harmony.kb.fixed`.
- DE: Tape-Memory (p67): Ohpe behält immer die letzten 30 s seines eigenen Ausgangs (Hauptausgang oder ein Part,
  16-Bit-Ringpuffer, etwa 6 MB bei 48 kHz). MERKEN (Tape-Seite, Sampler-Seite, MIDI-zuweisbar `tapemem.go`,
  Grid-Controller *Add Track*) macht daraus ein Instrument im Sampler eines Parts: Chromatisch (der stärkste Ton,
  Tonhöhe erkannt und auf den nächsten Halbton gestimmt, Grundton gesetzt) oder Slices (ein Slice pro Einsatz ab C2).
  Die Analyse läuft außerhalb des Audio-Threads; Gemerktes wird in der Session gespeichert (neuer Zustandsabschnitt
  `tapememory`, Format `tapeMemory` 1) und lässt sich als WAV in die Merk-Bibliothek exportieren. 5 IDs
  `tapemem.len/src/mode/to/go` nach `harmony.kb.fixed` angehängt.

- EN: First-start tour (p64): four short steps over the plate (parts, preset strip and LIBRARY; encoders, soft keys
  and the FX editor; tape, sequencer, tempo and key; help, settings and the Grid-Controller), English and German, in
  both skins. Shown once on the first standalone start and the first plug-in window, skippable (SKIP TOUR / Esc),
  started again from Settings > General or the Help tab. settings.json gains the key "tour" (no version change).
- DE: Einführungstour (p64): vier kurze Schritte über der Platte (Parts, Preset-Leiste und BIBLIOTHEK; Encoder,
  Softkeys und FX-Editor; Tape, Sequencer, Tempo und Tonart; Hilfe, Einstellungen und Grid-Controller), Deutsch und
  Englisch, in beiden Erscheinungen. Erscheint einmal beim ersten Start der Standalone-App und beim ersten
  Plug-in-Fenster, überspringbar (TOUR ÜBERSPRINGEN / Esc), wieder zu starten unter Einstellungen > Allgemein oder im
  Reiter Hilfe. settings.json bekommt den Schlüssel "tour" (keine Versionsänderung).
- EN: Factory bank curated for 1.0 (p65): every preset rendered offline and compared; three near-duplicates retired
  (Sine Slide, Phase Reese, Sparkle Bell – sessions that used them keep their sound and show no preset name), names
  unique and free of maker names (one preset renamed, Lo-Fi spelling), tags fixed (FM electric pianos under
  Piano and more), and every preset level-matched to one target – −11 LUFS short-term (EBU R128), true peak at or
  below −1 dBTP – through its level parameter only. Clipping presets fixed. Details: docs/en/FACTORY-SOUNDS.md.
- DE: Werksbank für 1.0 kuratiert (p65): jedes Preset offline gerendert und verglichen; drei Beinahe-Dubletten
  zurückgezogen (Sine Slide, Phase Reese, Sparkle Bell – Sitzungen damit behalten ihren Klang und zeigen keinen
  Preset-Namen), Namen eindeutig und ohne Herstellernamen (ein Preset umbenannt, Schreibweise Lo-Fi), Tags
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
