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
