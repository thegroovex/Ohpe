# Ohpe

*designed by groovex* · [English](README.md)

**Ohpe** ist eine kompakte, verspielte Synthesizer-Workstation für deinen Computer: zwei
Klangerzeuger, ein Step-Sequencer, ein Achtkanal-Mixer mit Delay und Hall und eine ruhige,
minimalistische Oberfläche rund um vier Drehregler und eine Klaviatur. Ohpe läuft als
**Standalone-App** (macOS, Windows, Linux) und als **VST3**- und **Audio-Unit**-Plug-in.

![Ohpe – Analog-Engine](docs/images/ohpe-analog-de.png)

> **Stand: Phase 1 (0.1).** Das Fundament steht – zwei Engines, Sequencer, Mixer, Effekte,
> MIDI-Learn, Multi-Out. Weitere Engines, Sequencer, ein Drum-Sampler und eine Bandmaschine
> folgen in Phase 2. Noch kein Release: selbst bauen oder die unsignierten Builds aus einem
> manuellen CI-Lauf laden.

## Funktionen

| | |
|---|---|
| **Analog-Engine** | zwei bandbegrenzte Oszillatoren (Wellenform-Morph, Verstimmung, Oktave, Mischung), Sub, Rauschen, Ladder- und State-Variable-Filter (TP / BP / HP), Drive, Keytracking |
| **FM-Engine** | sechs Operatoren, acht Algorithmen mit weichem Überblenden zwischen Nachbarn, Feedback, Verhältnis und Pegel je Operator |
| **Je Stimme** | Lautstärke- und Modulationshüllkurve, LFO mit mehreren Zielen, Glide, 16 Stimmen |
| **Step-Sequencer** | bis zu 64 Schritte, je Schritt Note, Anschlag, Gate, Wahrscheinlichkeit und Ratchets, bis zu vier Parameter-Locks je Schritt, folgt dem Transport des Hosts |
| **Mixer** | acht Kanäle mit EQ, Lautstärke, Panorama, Mute / Solo, zwei Sends (A: Delay, B: Hall), Master mit Limiter |
| **Multi-Out** | jeder Kanal geht auf den Hauptausgang oder auf einen eigenen Stereoausgang (8 Zusatzausgänge) – im Plug-in und in der Standalone-App |
| **MIDI** | jeder Regler ist per MIDI-Learn zuweisbar (Rechtsklick → MIDI-Learn), Zuweisungen lassen sich exportieren / importieren; alle Parameter sind automatisierbar |
| **Standalone-App** | Audiosystem, Gerät, Samplerate und Puffergröße wählen, Latenzanzeige, Multi-Out-Routing auf Gerätekanäle, MIDI-Eingänge |
| **Sprachen** | Deutsch und Englisch, jederzeit umschaltbar |

## Bilder

| | |
|---|---|
| ![FM-Engine](docs/images/ohpe-fm-de.png) | ![Step-Sequencer](docs/images/ohpe-sequencer-de.png) |
| FM-Engine | Step-Sequencer |
| ![Mixer](docs/images/ohpe-mixer-de.png) | ![Englische Oberfläche](docs/images/ohpe-analog.png) |
| Mixer | Englische Oberfläche |

## So spielst du Ohpe

* **Engine-Tasten** (links) wählen den Klangerzeuger des Parts; noch nicht gebaute Engines sind abgedunkelt.
* **Modus-Tasten** (rechts) schalten zwischen Synth, Hüllkurve, LFO, Mixer, Effekte und Sequencer.
* Die **vier Drehregler** steuern immer das, was das Display zeigt; im Synth-Modus sind es die
  vier Makros der Engine. Doppelklick setzt einen Wert zurück, Rechtsklick öffnet MIDI-Learn.
* **Klaviatur**: mit der Maus oder einem MIDI-Keyboard spielen; die Oktavtasten verschieben sie.
* **Sequencer**: Schritte antippen zum Einschalten, Start drücken (im Host folgt der Sequencer
  dem Transport des Hosts, solange Start an ist).

## Bauen

Voraussetzungen: CMake ≥ 3.22, ein C++17-Compiler (Xcode 15+, Visual Studio 2022, GCC 11+ /
Clang 14+), unter Linux die JUCE-Abhängigkeiten (ALSA, FreeType, fontconfig, X11, Mesa). JUCE
8.0.15 wird automatisch geladen, oder du gibst mit `-DOHPE_JUCE_PATH=/pfad/zu/JUCE` eine lokale
Kopie an.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

App und Plug-ins liegen in `build/Ohpe_artefacts/Release/` (`Standalone`, `VST3`, `AU` unter
macOS). Für einen Universal-Build unter macOS `"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"`
ergänzen; mit `-DOHPE_COPY_PLUGIN=ON` werden die Plug-ins nach dem Bauen in die Systemordner
kopiert.

Alle Tests laufen mit `scripts/test-all.sh` – siehe [Mitwirken](docs/de/MITWIRKEN.md).

## Dokumentation

* [Architektur](docs/de/ARCHITEKTUR.md) – Kurzfassung; ausführlich auf Englisch in [ARCHITECTURE.md](docs/ARCHITECTURE.md)
* [Mitwirken](docs/de/MITWIRKEN.md) – Tests, Regeln, Ablauf

## Lizenz

Ohpe ist freie Software unter der [GNU Affero General Public License v3.0 oder neuer](LICENSE).
Gebaut mit [JUCE](https://juce.com) (AGPLv3), [nlohmann/json](https://github.com/nlohmann/json)
(MIT) und [doctest](https://github.com/doctest/doctest) (MIT). Schrift: Space Grotesk (SIL OFL 1.1).
