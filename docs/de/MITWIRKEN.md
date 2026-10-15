# Bei Ohpe mitwirken

*Deutsch · English: [CONTRIBUTING.md](../../CONTRIBUTING.md)*

Lies zuerst die [Architektur](ARCHITEKTUR.md) (ausführlich: [ARCHITECTURE.md](../ARCHITECTURE.md)) –
dort stehen die Trennung von Kern und Plug-in-Hülle, die Echtzeitregeln und wie neue Engines,
Sequencer und Effekte dazukommen.

## Vor jedem Push: alle Tests

```sh
PLUGINVAL=/pfad/zu/pluginval scripts/test-all.sh
```

Das Skript baut und startet nacheinander:

1. **Kern-Tests** (`build-core`, ohne JUCE, Sekunden) – DSP, Engines, Sequencer, Mixer, MIDI-Map,
   Zustand, Parameterregister, inklusive *„parameter IDs are append-only"*.
2. **Plug-in-Tests** (`build`) – Prozessor, Editor, Lokalisierung, Multi-Out, Zustand speichern / laden.
3. **VST3-Host-Test** – lädt das gebaute `Ohpe.vst3` wie eine DAW: Instrument, 9 Ausgangsbusse,
   Parameter, eine Note klingt, Zustand in einer zweiten Instanz wiederhergestellt.
4. **pluginval** mit Strenge 5 auf das VST3 (ohne `PLUGINVAL` mit Hinweis übersprungen; die CI
   führt es immer aus).

Nützliche Variablen: `OHPE_JUCE_PATH` (lokales JUCE 8.0.15), `OHPE_CMAKE_ARGS="-DOHPE_LTO=OFF"`
(viel schnelleres Linken lokal), `OHPE_CORE_ONLY=1` (nur Kern-Tests). Unter Linux ohne Bildschirm
laufen die GUI-Schritte automatisch mit `xvfb-run`.

Nie einen roten Build pushen. Die CI führt bei jedem Push dasselbe Skript unter Linux aus; ein
manueller Lauf des CI-Workflows baut zusätzlich macOS Universal (App, AU inkl. `auval`, VST3)
und Windows.

## Regeln

* **Parameter-IDs nur anhängen.** Nie einen Parameter entfernen, umbenennen, umsortieren oder
  seinen Bereich ändern, nie Auswahllisten umsortieren. Neue Parameter kommen ans Ende von
  `src/core/ParameterLayout.cpp`, ihre IDs ans Ende von `tests/core/golden/param_ids_v1.txt`.
* **Echtzeitsicherheit** in allem, was `Core::process()` aufruft: keine Speicherreservierung,
  keine Locks, kein I/O, keine Exceptions.
* **Jeder sichtbare Text auf Deutsch und Englisch.** Texte laufen über `i18n::tr()`, der deutsche
  Text kommt in `src/plugin/Localisation.cpp`; Parameterbeschreibungen tragen beide Namen.
* **Nur eigenes Design.** Ohpe hat eigenen Namen, eigenes Layout, eigene Farben und Piktogramme.
  Keine anderen Instrumente oder Hersteller nennen oder nachahmen – weder in Code, Kommentaren,
  Doku, Commit-Nachrichten noch in Grafiken.
* Neuer Code kommt mit Tests (siehe Checklisten in der Architektur).
* Stil: C++17, 4 Leerzeichen, Allman-Klammern, `camelCase`-Member, Leerzeichen vor der
  Aufrufklammer wie im bestehenden Code; jede Quelldatei beginnt mit dem zweizeiligen Kopf.

## Bilder für die Doku

```sh
xvfb-run -a build/ohpe_screenshot_artefacts/Release/ohpe_screenshot docs/images
```

rendert alle Ansichten auf Englisch und Deutsch nach `docs/images/`. Nach UI-Änderungen neu rendern.

## Lizenz

Mit einem Beitrag stimmst du zu, dass er unter der AGPL-3.0-or-later steht.
