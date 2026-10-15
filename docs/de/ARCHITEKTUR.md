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
  sortiert und beim Rendern an `MpeInput` → `Part` verteilt.
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
