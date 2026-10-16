# Modulationsmatrix

*Deutsch · English: [../en/MODULATION.md](../en/MODULATION.md) · Parametertabellen:
[../PARAMETERS.md](../PARAMETERS.md)*

Jeder Part hat eine Modulationsmatrix mit **16 Verbindungen**, zwei weiteren LFOs, einer dritten
Hüllkurve, vier Makros und einer Sequencer-Spur. Gehaltene Töne bleiben in Bewegung: ein LFO auf
die Wavetable-Position, eine Hüllkurve auf den Cutoff, Druck auf den Pegel – jede Verbindung ist
eine Zeile der Matrix.

```
 Quelle ──Kurve──×──(Via)──× Menge ──▶ Ziel
   LFO 1-3, Hüllkurven, Anschlag, Tastatur, Rad, Bend, Druck, Slide, Zufall, Makros, Seq-Spur
```

## Eine Verbindung

| Feld | Werte | Hinweise |
|---|---|---|
| Quelle (`pN.mod.sK.src`) | siehe Quellen | Aus = die Verbindung schweigt |
| Via (`pN.mod.sK.via`) | jede Quelle | skaliert die Verbindung (z. B. LFO via Modulationsrad: das Rad öffnet das Vibrato); Aus = 1 |
| Ziel (`pN.mod.sK.dst`) | siehe Ziele | |
| Menge (`pN.mod.sK.amt`) | −100 … +100 % | bipolar; negative Mengen kehren um |
| Kurve (`pN.mod.sK.curve`) | Linear, Exponentiell, Logarithmisch, S-Kurve, Stufen | formt die Quelle (bei bipolaren Quellen gespiegelt); Stufen = 8 Stufen |

Alle Felder sind normale Parameter: im Host automatisierbar, per MIDI-Learn zuweisbar, in Presets
und Projekten gespeichert. Der Wert einer Verbindung ist `Kurve(Quelle) × Via × Menge`. Mehrere
Verbindungen auf dasselbe Ziel addieren sich.

## Quellen

| # | Quelle | Bereich | Pro Stimme | Hinweise |
|---|---|---|---|---|
| 1 | LFO 1 | ±1 | ja | der LFO des Parts (LFO-Seite), vor seiner Tiefe – auch mit LFO-Tiefe 0 nutzbar |
| 2, 3 | LFO 2, LFO 3 | ±1 | ja | siehe unten |
| 4 | Amp-Hüllkurve | 0 … 1 | ja | Lautstärke-Hüllkurve der Stimme |
| 5 | Mod-Hüllkurve | 0 … 1 | ja | Mod-Hüllkurve der Stimme |
| 6 | Hüllkurve 3 | 0 … 1 | ja | Verzögerung – Attack – Halten – Decay – Sustain – Release |
| 7 | Anschlag | 0 … 1 | ja | |
| 8 | Tastatur | ±1 | ja | C4 = 0, ±5 Oktaven = ±1 |
| 9 | Modulationsrad | 0 … 1 | – | CC 1 |
| 10 | Pitchbend | ±1 | – | |
| 11 | Kanaldruck | 0 … 1 | – | Channel Aftertouch des Parts |
| 12 | Notendruck | 0 … 1 | ja | Poly-Aftertouch oder MPE-Druck des Tons (Kanaldruck wirkt auf alle Töne) |
| 13 | Slide (Klangfarbe) | ±1 | ja | CC 74 / MPE-Slide, Mitte = 0 |
| 14 | Noten-Bend | ±1 | ja | Pitchbend pro Ton eines MPE-Kanals |
| 15 | Zufall | ±1 | ja | ein neuer Wert mit jedem Ton |
| 16 – 19 | Makro 1 – 4 | 0 … 1 | – | die Parameter `pN.macro1` … `pN.macro4`: Regler zum Automatisieren oder Zuweisen |
| 20 | Seq-Spur | 0 … 1 | – | der Parameter `pN.seqlane`, gedacht für Step-Locks: jeder Schritt setzt die Spur |
| 21, 22 | Morph X, Morph Y | 0 … 1 | – | p16: die geglättete Position des [Morph-Pads](MORPH-RAUM.md) des Parts |

Quellen **pro Stimme** haben in jeder Stimme ihren eigenen Wert: in einem Akkord bekommt jeder Ton
seine eigene Hüllkurve, seinen Anschlag und Zufallswert.

### LFO 2 und LFO 3 (`pN.lfo2.*`, `pN.lfo3.*`)

| Parameter | Werte |
|---|---|
| Tempo | 0,01 … 40 Hz (bei Sync = Frei) |
| Sync | Frei, 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, punktiert 1/2 … 1/16, Triolen 1/2 … 1/16 |
| Form | Sinus, Dreieck, Sägezahn auf, Sägezahn ab, Rechteck, S&H, Weicher Zufall |
| Phase | Startphase 0 … 1 |
| Einblenden | 0 … 10 s: der LFO wächst nach jedem Ton herein |
| Modus | Frei laufend (eine Phase für den Part; bei laufendem Transport an die Songposition gebunden), Neustart (beginnt mit jedem Ton neu, eigene Phase pro Stimme), Einmalig (ein Durchlauf, dann gehalten: eine weitere Hüllkurve) |
| Tiefe | 0 … 1 |

### Hüllkurve 3 (`pN.env3.*`)

Verzögerung 0 … 4 s, Attack 1 ms … 10 s (linear), Halten 0 … 4 s, Decay 1 ms … 10 s, Sustain
0 … 1, Release 1 ms … 10 s (exponentiell).

## Ziele

| # | Ziel | Pro Stimme | Bereich bei 100 % |
|---|---|---|---|
| 1 | Tonhöhe | ja | ±12 Halbtöne |
| 2 | Pegel | ja | Verstärkung × (1 + Wert), mindestens 0 – Sample für Sample gerampt |
| 3 | Panorama | ja | ganz links … rechts |
| 4 | Klangfarbe | ja | die Helligkeit der Engine (Analog / Cluster / Grain / Sampler / Wavetable: ±3 Oktaven Filter, FM: Index, Pluck: Helligkeit, Vocal: Vokal) |
| 5 – 28 | Engine 1 – 24 | – | Parameter 1 … 24 der aktiven Engine (in ihrer Reihenfolge, die ersten vier sind die Makros: z. B. Wavetable-Position, FM-Index, Grain-Position, Vocal-Formant), voller Bereich |
| 29 | Lautstärke | – | Part-Lautstärke |
| 30 – 35 | LFO 1 – 3 Tempo / Tiefe | – | |
| 36 | Mod-Hüllkurve Menge | – | |
| 37 – 42 | FX 1 – 6 Mischung | – | Mischung des FX-Rack-Slots |
| 43 – 66 | FX 1 – 6 Parameter 1 – 4 | – | vier Parameter des Effekts im Slot (folgt dem Typ des Slots: die ersten vier eines 0.2-Typs, die *Mod 1–4* eines [Rack-Typs](BAND-MISCHER-FX.md#effekttypen-des-racks)) |
| 67 – 71 | Azimut, Elevation, Entfernung, Bewegungspfad Tempo / Phase | – | 1.0: die 3D-Stufe des Parts `pN.space.*` ([RAUM.md](RAUM.md)), ganzer Bereich (Azimut ±180°, Elevation −60 … 90°, Entfernung 0,2 … 40 m logarithmisch, Tempo ×−4 … 4, Phase 0 … 360°); z. B. Slide → Elevation, Notendruck → Entfernung für MPE |
| 72 | Leben | – | p15: der Leben-Anteil des Parts `pN.life` ([BEDIENUNG.md](BEDIENUNG.md#leben)), 0 … 100 %; z. B. Mod-Rad → Leben, um gehaltene Töne zu beleben, Notendruck → Leben für MPE |
| 73, 74 | Morph X, Morph Y | – | p16: die Position des [Morph-Pads](MORPH-RAUM.md) des Parts (`pN.morph.x` / `.y`), ganze Breite; z. B. LFO 2 → Morph X, Notendruck → Morph Y |
| 75 – 80 | Körper-Stimmung, Körper-Größe, Körper-Material, Körper-Ausklang, Körper-Position, Körper-Mischung | – | p19: der [Resonanzkörper](RESONANZKOERPER.md) des Parts (`pN.reso.tune` … `.mix`), ganzer Bereich (Stimmung ±24 Halbtöne); z. B. LFO 2 → Körper-Position für einen wandernden Anregungspunkt, Hüllkurve 3 → Körper-Mischung |

Ziele ab 5 bewegen einen **Parameter** um `Menge × Quelle` seines vollen Bereichs (normiert, ein
Cutoff bewegt sich also in Oktaven): ein Wert für den Part, berechnet aus Part-Quellen – den frei
laufenden LFOs, Rad und Makros sowie Hüllkurven / Anschlag / Tastatur / Zufall / Ausdruck des
neuesten Tons. Ziele 1 – 4 werden in jeder Stimme berechnet. Für Helligkeit, die jedem Ton eines
Akkords einzeln folgt, das Ziel Klangfarbe nehmen.

## Glätte und Echtzeit

* Verbindungen pro Stimme werden alle 32 Samples ausgewertet; der Pegel wird pro Sample gerampt
  (kein Zippern, auch bei schnellem Tremolo), Tonhöhe, Panorama und Klangfarbe gleiten über etwa
  1 ms – gestufte Quellen (S&H, Stufen) bleiben knackig, ohne zu klicken.
* Parameter-Verbindungen wirken alle 32 Samples und gleiten über etwa 1,5 ms; die Effekte glätten
  ihre Parameter zusätzlich. Der Parameter selbst (Host-Automation, Wert in der Oberfläche) bleibt
  unverändert – die Oberfläche zeigt die Modulation als Ring um den Wert.
* Alles läuft im Audio-Thread ohne Speicheranforderung; eine Verbindung mit Menge 0, Quelle Aus
  oder Via auf 0 lässt den Klang bitgenau unverändert.

## Bewegung in Presets

Werks-Presets setzen Verbindungen wie jeden anderen Parameter (`"mod.s1.src": 2` …). Gute
Ausgangspunkte: LFO 2 (Weicher Zufall, 0,2 Hz) → Engine 1 (Position) 15 %; Hüllkurve 3 (langsamer
Attack) → Klangfarbe 20 %; Zufall → Panorama 30 %; LFO 3 (1/1 synchron, Frei laufend) → FX 1
Mischung 20 %; Notendruck → Pegel oder Klangfarbe für MPE.

## Erweitern (Entwicklung)

Quellen und Ziele sind Tabellen, an die nur angehängt wird (`src/core/mod/ModMatrix.cpp`,
`sourceTable()` / `destTable()`), jeder Eintrag mit festem Schlüssel. Eine neue Part-Quelle (etwa
ein Morph-Pad, eine Spur des Bewegungs-Recorders, ein Performance-Pad oder Harmonie-Ereignisse)
wird angehängt und mit `PartMatrix::setSourceValue` gespeist; ein neues Ziel, das einen
Part-Parameter bewegt, nennt die Endung der Parameter-ID und wirkt, sobald es den Parameter gibt.
Presets mit Verbindungen tragen `"modSchema"`; `mod::migrate()` ist der Haken für spätere
Schema-Versionen.
