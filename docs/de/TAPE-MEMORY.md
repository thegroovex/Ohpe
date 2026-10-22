# Tape-Memory (p67)

Ohpe – designed by groovex. [English](../en/TAPE-MEMORY.md)

Ohpe vergisst die letzte halbe Minute nie. Solange es läuft, hält es ein mitlaufendes Memory seines eigenen
Ausgangs, und eine Taste – **MERKEN** – macht aus dem gerade Gespielten ein neues Instrument. Eine Phrase, die
zufällig gut war, ein glücklicher Zufall im FX, ein Akkord, der genau richtig ausklang? MERKEN drücken und über die
ganze Tastatur weiterspielen.

## Was es behält

* **Quelle** (`tapemem.src`): der Hauptausgang (vor dem Metronom-Klick) oder ein Part nach seinem FX-Rack.
* **Länge** (`tapemem.len`): wie viel beim Merken genommen wird – 5, 10, 20 oder 30 s. Das Memory hält immer 30 s.
* Das Memory ist ein vorab angelegter Ringpuffer mit 16-Bit-Frames (+6 dB Reserve): etwa 6 MB bei 48 kHz. Das
  Schreiben kostet den Audio-Thread keine Speicheranforderung und keine Sperre; auf die Platte kommt nichts, bis du
  merkst.

## MERKEN

* Tape-Seite: **MERKEN** am Ende der ersten Softkey-Reihe, **MEMORY** (die Einstellungen) am Ende der zweiten.
* Sampler-Seite: **MERKEN** (vierte Taste) merkt in den angezeigten Part.
* MIDI: `tapemem.go` (*Merken*) – MIDI-Learn im MEMORY-Menü oder bei den Fernbedienungen des MIDI-Menüs.
* Grid-Controller: die Taste *Add Track*. Sie leuchtet schwach, solange das Memory Klang hält, und blinkt während
  der Analyse.

Der Ziel-Part (`tapemem.to`) ist der bearbeitete Part, außer du wählst Part 1–4. Die Analyse – Stille abschneiden,
Einsätze und Tonhöhe finden, umstimmen – läuft in einem eigenen Thread; das Display meldet das Ergebnis („Gemerkt in
Part 2: chromatisch, Grundton A3“). Der Part wechselt in den Sampler.

## Zwei Modi (`tapemem.mode`)

* **Chromatisch** – ein spielbarer Klang: der stärkste Ton im Memory (der Abschnitt zwischen zwei Einsätzen mit
  der meisten Energie, höchstens 8 s; das ganze Memory, wenn es keine Einsätze hat). Seine Tonhöhe wird erkannt
  (50 Hz – 2 kHz) und auf den nächsten Halbton korrigiert, und der Grundton der Zone wird auf diesen Ton gesetzt – so
  spielt die Tastatur ihn sauber gestimmt. Klänge ohne klare Tonhöhe (Rauschen, Drums) behalten ihre Tonhöhe mit
  Grundton C4.
* **Slices** – das Memory (Stille abgeschnitten, normalisiert) im Slice-Modus des Samplers: ein Slice pro
  Einsatz, ab C2 aufwärts (die Drum-Pads). *Slice-Empfindlichkeit* auf der Sampler-Seite schneidet neu.

## Mit der Session gespeichert, als WAV exportiert

Das zuletzt Gemerkte jedes Parts wird in der Session oder im Projekt gespeichert (Zustandsabschnitt `tapememory`,
siehe [Dateiformate](FORMATE.md)), solange der Sampler des Parts es noch spielt – eine externe Datei ist nicht nötig.
MEMORY > *Zuletzt Gemerktes als WAV exportieren* schreibt eine 24-Bit-WAV (mit dem Grundton als Unity-Note) in
die Merk-Bibliothek: `<Benutzer-Anwendungsdaten>/groovex/Ohpe/Captures` (die Umgebungsvariable `OHPE_CAPTURE_DIR`
ersetzt den Ort). MEMORY > *Memory leeren* vergisst, was gehalten wird.

## Nach dem Merken einen anderen Klang laden

Ein Preset aus der Bibliothek oder über die Pfeile der Preset-Leiste lädt vollständig in den Part – Engine,
Werksinstrument oder Sample, alle Parameter –, auch wenn der Part gerade Gemerktes spielt: Das Gemerkte verlässt den
Sampler des Parts und überdeckt den neuen Klang nie. Verloren ist es nicht: Es bleibt im Memory dieser Session,
MEMORY > *Zuletzt Gemerktes zurückholen* setzt es wieder in seinen Part (Engine Sampler, Modus und Grundton wie
gemerkt), und *Zuletzt Gemerktes als WAV exportieren* schreibt es weiterhin; das Display meldet es. Wer in der
Bibliothek nur stöbert, ohne zu laden, bekommt das Gemerkte beim Schließen zurück. Ein Projekt lädt seinen Klang
genauso in alle Parts; ein Projekt, das gespeichert wurde, während ein Part Gemerktes spielte, behält es (mit dem
Audio) und bringt es zurück. Part-Presets speichern die Einstellungen des Klangs, nicht das gemerkte Audio. ZUFALL
und ZUCHT formen den gemerkten Klang (Engine und Sample-Wahl ändern sie nie), das Gemerkte bleibt also. Gemerktes
gehört nur zu seinem Part; andere Parts und Presets übernehmen es nie.

## Parameter

`tapemem.len`, `tapemem.src`, `tapemem.mode`, `tapemem.to`, `tapemem.go` – siehe [PARAMETERS.md](../PARAMETERS.md#p67--tape-memory).
Sie sind Werkzeuge, kein Klang: Presets speichern sie nicht.
