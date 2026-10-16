# Ohpe-Architektur (Kurzfassung)

*Deutsch · ausführlich auf Englisch: [docs/ARCHITECTURE.md](../ARCHITECTURE.md)*

Ohpe besteht aus einem Kern ohne Framework und einer dünnen JUCE-Hülle. Alles, was klingt oder
Zustand hält, lebt im Kern und ist in Sekunden ohne JUCE testbar; die Hülle verbindet ihn nur
mit Plug-in-Hosts, der Standalone-App und dem Bildschirm.

## Bausteine

| Ordner | Inhalt |
|---|---|
| `src/core/model` | Parameterbeschreibung, Parameterregister, Snapshot je Block, MIDI-Events, lock-freie Queue, JSON-Zustand |
| `src/core/dsp` | Hüllkurven, LFO, Filter, Biquad, bandbegrenzte Oszillatoren |
| `src/core/engines` | Engine-Schnittstelle, Stimmenverwaltung, Analog- und FM-Engine, Part |
| `src/core/fx` | Delay, Hall, Limiter |
| `src/core/mixer` | acht Kanäle, EQ, Sends, Master, Multi-Out-Routing |
| `src/core/seq` | Step-Sequencer (Muster lock-frei in Atomics) |
| `src/core/midi` | MIDI-Map und MIDI-Learn (lock-freie Tabelle) |
| `src/core/tape` | Schnittstelle der Bandmaschine (Phase 2) |
| `src/plugin` | Plug-in-Prozessor, Brücke Register → Host-Parameter, Lokalisierung DE/EN |
| `src/ui` | Editor, Look-and-Feel, Farben, Piktogramme, Display, Bedienelemente, Modus-Seiten |
| `src/standalone` | eigene Standalone-App mit Audio-Einstellungen und Multi-Out-Routing |
| `tests/core`, `tests/plugin` | Kern-Tests (doctest, ohne JUCE), Plug-in-, Editor- und VST3-Host-Tests |
| `tools` | Parametertabelle, Screenshot-Werkzeug |

## Wichtige Regeln

* **Threads:** Der Audio-Thread (`Core::process()`) reserviert keinen Speicher, sperrt nicht und
  macht kein I/O. Daten wechseln den Thread über Atomics, lock-freie Queues und doppelt
  gepufferte MIDI-Tabellen. Gerendert wird in Stücken von höchstens 256 Samples, geteilt an
  jedem MIDI-Event und Parameter-Lock – Noten sind sample-genau.
* **Parameter:** Ein Register enthält jeden automatisierbaren und MIDI-zuweisbaren Wert; das
  Plug-in erzeugt genau einen Host-Parameter je Eintrag, in gleicher Reihenfolge, mit derselben
  ID (z. B. `p1.analog.cutoff`, `mix.ch4.senda`). **IDs werden nur angehängt** – geprüft durch
  die Golden-Datei `tests/core/golden/param_ids_v1.txt`.
* **Zustand:** JSON (`ohpe-state`, Version 1). Unbekannte IDs werden ignoriert, fehlende
  bekommen ihren Standardwert – alte Zustände öffnen in neuen Versionen und umgekehrt.

## Erweitern (Phase 2)

Neue Engines (z. B. Wavetable), Sequencer, Effekte und die Bandmaschine docken über
Schnittstellen an (`ISynthEngine`, `ISequencer`, `IEffect`, `ITapeMachine`). Die Schritt-für-
Schritt-Anleitungen stehen im englischen Dokument.

## Phase 2: Band, Mixer-Dynamik, FX-Slot (0.2.0)

Umgesetzt in `src/core/tape/TapeMachine.*` (implementiert `ITapeMachine`; `process()` erhält
jetzt `TapeInputs` – Eingangsbus, Parts, Drum), `src/core/fx/` (Kompressor, Bandsättigung,
Chorus, Phaser, Bitcrusher, Filter-Drive, `InsertSlot`), im Mixer (Kompressor-lite je Kanal,
Master-Kompressor, Bandstufe, Limiter-Obergrenze, Direkt-Einspeisung für Band-Direktausgänge)
und in `src/plugin/TapeIO.*` (Bandaudio im Plug-in-Zustand, Offline-Export). Entscheidungen:

* **Eingangsbus:** ein Stereo-/Mono-Eingang „Input“, standardmäßig aus;
  `Core::setAudioInput()` reicht ihn für den nächsten Block an den Kern.
* **Kern-Anbindung:** `Core::process()` ruft je Teilstück nach dem Rendern der Parts
  `renderTapeChunk()` auf: FX-Slots → Band (Aufnahme + Wiedergabe) → Quellen `tape1 …` und
  Direktausgänge.
* **Message-Thread:** `TapeMachine::housekeeping()` (Plug-in-Timer, 20 Hz) füllt den
  Blockvorrat und macht aus beendeten Aufnahmen Undo-Schritte.
* **Speichern:** Bandaudio steckt im Plug-in-Zustand hinter dem JSON (nur Blöcke mit Audio,
  float32); keine externen Dateien. Der Undo-Verlauf wird nicht gespeichert.

Anleitung: [BAND-MISCHER-FX.md](BAND-MISCHER-FX.md) · Parameter: [../PARAMETERS.md](../PARAMETERS.md).
## Phase 2: Parts, Sequencer und MIDI (0.2)

* **Vier Parts** (`Core::numParts = 4`, IDs `p1.*` … `p4.*`; Parts 2–4 in 0.2 angehängt). Eine
  neue Engine wird für jeden Part registriert:
  `for (auto& p : parts) p->registerEngineParams (reg, EngineType::X);` (nur anhängen, ganz
  unten). Part N rendert in die Mixer-Quelle `source::part1 + N - 1`.
* **Routing in `Core::process()`**: Live-MIDI (Bildschirm-Tastatur, Host / Gerät) geht an jeden
  Part mit passendem Kanal (`pN.midich`, 0 = Omni; MPE-Parts nehmen alle Kanäle), durch den
  `NoteShaper` des Parts (Skala quantisieren, Ein-Finger-Akkorde) und beim Ziel-Part des
  Arpeggiators in den `Arpeggiator`. Sequencer-Noten adressieren ihren Part über MIDI-Kanal 1–4
  (= Ziel-Part). Jedes Ereignis wird ein `RoutedEvent {event, part}`; die Liste wird nach Zeit
  sortiert und beim Rendern an `MpeInput` → `Part` verteilt. MIDI-Kanal 10 (`Core::drumChannel`)
  geht zusätzlich an das Drum-Kit (`RoutedEvent::part` = `Core::drumPart`); Omni-Parts
  überspringen ihn.
* **Band und FX-Slots mit vier Parts:** jeder Part rendert in einen eigenen Puffer, durchläuft
  seinen `InsertSlot` (bisher hat nur Part 1 FX-Slot-Parameter; die anderen bleiben aus) und kann
  Bandquelle sein; `renderTapeChunk()` läuft nach den Parts und dem Drum-Kit.
* **Sequencer** (`src/core/seq`): `StepSequencer` (16 Pattern, Kette / Song-Modus),
  `EuclidSequencer`, `Arpeggiator`, `OrbitSequencer`. Gemeinsam (`SeqCommon.h`): `GridClock`
  leitet Schrittgrenzen aus der absoluten ppq-Position ab (phasenstarr, sicher bei Loops und
  Sprüngen), `NoteQueue` plant Note-Offs mit Referenzzählung pro Note. Pattern-Daten sind
  lock-frei (Atomics); Orbit-Schritte bilden den Zustandsabschnitt `"orbit"`, Step-Pattern und
  Kette bleiben in `"sequencer"` (Pattern 1 im Format von 0.1).
* **Transport** (`Core::makeTransport`): Host / Link (`Transport::hostDriven`), MIDI-Clock
  (`MidiClockFollower`) oder interne Clock. `MidiClockGenerator` erzeugt Clock-Out;
  `Core::midiOutput()` enthält Clock und Sequencer-Noten für den MIDI-Ausgang der Standalone-App.
* **MPE** (`src/core/midi/MpeInput.h`): Pitchbend, Druck und Timbre pro Note werden eine
  `NoteExpression` auf den Stimmen der Note (`ISynthEngine::noteExpression`, umgesetzt in
  `PolyEngine` / `VoiceBase`; Engines addieren `expr.timbre` zu ihrer Helligkeitssteuerung).
* **Standalone** (`src/standalone`): `LinkSync` (Link-Bibliothek, siehe THIRD-PARTY.md; CMake-Option `OHPE_WITH_LINK`)
  implementiert `OhpeProcessor::ExternalTransport`; `SyncSettingsPage` wählt Sync-Quelle,
  MIDI-Ausgang, Clock- / Noten-Ausgabe.
* Leitfäden: [Sequencer](SEQUENZER.md), [MIDI](MIDI.md), [Parameter](../PARAMETERS.md).

## Phase 3: Oberfläche und Preset-Bibliothek (0.3)

* `src/core/preset`: Preset-Dateiformat (Version 2, Part / Projekt, Tags, Autor, Engine) und die
  Bibliothekslogik (Filter, Sortierung, Favoriten / Bewertungen) – ohne JUCE, getestet in
  `tests/core/PresetTests.cpp`. Format: [PRESETS.md](PRESETS.md).
* `src/plugin/PresetManager`: Werksbank als Binärdaten aus `resources/presets/factory/*.ohpepreset`
  (CMake-Ziel `ohpe_presets`), Benutzerordner (`OHPE_PRESET_DIR` überschreibt), Laden über
  `OhpeProcessor::editRegistry()`, voriges / nächstes, Vorhören mit Rücksprung.
* 12 angehängte Fernsteuer-Parameter (`lib.*`, `ui.part`, `tape.edit.*`): MIDI-Tastendrücke werden
  im Audio-Thread gezählt, Host- / UI-Änderungen als steigende Flanke erkannt; die Aktion läuft im
  Timer des Prozessors (auch ohne geöffneten Editor).
* `src/ui`: ziehbare Display-Zeilen mit Feinregelung, Soft-Tasten, Sequencer-Ansichten, Tape- und
  Drum-Seite, Profiansicht (`ProView`), Bibliothek (`LibraryBrowser`), Part-Auswahl, Drag & Drop.
  Schritt-Locks nutzen den vorhandenen Lock-Speicher des Step-Sequencers.

## 0.8: Sound-Revision 3

* **FX-Slot-Typen** (`src/core/fx/InsertEffects2.*`): Waveshaper, Squash, Kammfilter, Formant,
  Flanger, Wobble, Tremolo, Stereobreite – eigenständige `IEffect`-Klassen, an die Typliste des Slots
  angehängt; ihre Parameterblöcke werden spät registriert (`InsertSlot::registerLateParams`). `IEffect`
  bekam `setTempo()` (der Core ruft es in jedem Block) und `noteOn()` (eine Note im Part des Slots:
  LFO-Neustart). Die Parts 2–4 bekamen ihren Slot (`p2.fx.*` …, angehängt).
* **Layer-Engine** (`Part`, `pN.layer.*`): eine zweite Engine des Parts spielt jede Note; sie rendert
  in einen eigenen Puffer und wird mit Ring- / AM- / Falt-Kreuzmodulation gemischt. Aus = bitgleich
  zu 0.7. Siehe [engines/layer.md](engines/layer.md).
* **Späte Engine-Parameter** (`ISynthEngine::firstLate08Param()`, `Part::registerLate08EngineParams`):
  Wavetable-Unisono-Stapel / FM / LFO > Position, FM Anschlag > Index, Tasteninstrumente für Sampler /
  Grain (`inst2`; die Instrumentliste der Bank wuchs auf 15, die Auswahl `inst` von 0.4 behält ihre 11).
* **Ausdruck** (`NoteExpression`, `VoiceBase`): acht weitere Ziele (Cutoff, Position, FM-Index, Dichte,
  Drive, Vibrato, Formant, Layer-Mischung), Mengen pro Quelle und Gleiten: eine Stimme hält den
  Ziel-Ausdruck und den aktuellen; die Tonhöhe pro Note gleitet über `pN.mpe.slide` (mindestens 2 ms),
  der Rest über `pN.mpe.smooth`; der erste Ausdruck einer Note gilt sofort.
