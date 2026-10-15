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
