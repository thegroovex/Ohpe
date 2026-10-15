# Ohpe parameters

*English / Deutsch.* The complete, current table is printed by `ohpe_param_dump --markdown`;
`tests/core/golden/param_ids_v1.txt` lists every released ID in its permanent order. IDs are
append-only (see [ARCHITECTURE.md](ARCHITECTURE.md)). Sections are added at the end of this file.

## 0.2 – sequencers, parts, MIDI and sync / Sequencer, Parts, MIDI und Sync

| ID | EN | DE | Range / Bereich | Default |
|---|---|---|---|---|
| `seq.step.pattern` | Pattern | Pattern | 1–16 | 1 |
| `seq.step.song` | Song Mode | Song-Modus | off / on | off |
| `p2.*`, `p3.*`, `p4.*` | parts 2–4: same common and engine parameters as `p1.*` | Parts 2–4: dieselben Parameter wie `p1.*` | | |
| `pN.midich` | Part N MIDI Channel | Part N MIDI-Kanal | 0 (omni) – 16 | N |
| `pN.quant` | Part N Scale Quantise | Part N Skala quantisieren | off / on | off |
| `pN.chord` | Part N Chord | Part N Akkord | Off, Triad, Seventh, Sus2, Sus4, Power, Octave, Sixth, Add9 | Off |
| `pN.mpe` | Part N MPE | Part N MPE | off / on | off |
| `pN.mpe.bend` | Part N MPE Bend Range | Part N MPE-Bend-Bereich | 0–96 st | 48 |
| `pN.mpe.press` | Part N Pressure Target | Part N Druck-Ziel | Off, Timbre, Level, LFO Depth, Pan | Level |
| `pN.mpe.timbre` | Part N Timbre Target | Part N Timbre-Ziel | Off, Timbre, Level, LFO Depth, Pan | Timbre |
| `key.root` | Key | Tonart | C – B | C |
| `key.scale` | Scale | Skala | Chromatic, Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic Minor, Melodic Minor, Major / Minor Pentatonic, Blues, Whole Tone | Major |
| `seq.euclid.on` | Euclid On | Euklid an | off / on | off |
| `seq.euclid.part` | Euclid Target Part | Euklid Ziel-Part | 1–4 | 2 |
| `seq.euclid.rate` | Euclid Rate | Euklid Raster | 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T, 1/16T, 1/32T, 1/4., 1/8., 1/16. | 1/16 |
| `seq.euclid.gate` | Euclid Gate | Euklid Gate | 5–100 % | 50 % |
| `seq.euclid.vel` | Euclid Velocity | Euklid Anschlag | 1–127 | 100 |
| `seq.euclid.rN.steps` | Ring N Steps | Ring N Schritte | 1–32 | 16 |
| `seq.euclid.rN.pulses` | Ring N Pulses | Ring N Impulse | 0–32 | 4 / 0 / 0 / 0 |
| `seq.euclid.rN.rot` | Ring N Rotation | Ring N Rotation | 0–31 | 0 |
| `seq.euclid.rN.note` | Ring N Note | Ring N Note | 0–127 | 36 / 38 / 42 / 46 |
| `seq.arp.on` | Arp On | Arp an | off / on | off |
| `seq.arp.part` | Arp Target Part | Arp Ziel-Part | 1–4 | 3 |
| `seq.arp.mode` | Arp Mode | Arp Modus | Up, Down, Up/Down, Random, As Played, Chord | Up |
| `seq.arp.oct` | Arp Octaves | Arp Oktaven | 1–4 | 1 |
| `seq.arp.gate` | Arp Gate | Arp Gate | 5–100 % | 50 % |
| `seq.arp.rate` | Arp Rate | Arp Raster | as Euclid | 1/16 |
| `seq.arp.latch` | Arp Latch | Arp Halten | off / on | off |
| `seq.orbit.on` | Orbit On | Orbit an | off / on | off |
| `seq.orbit.part` | Orbit Target Part | Orbit Ziel-Part | 1–4 | 4 |
| `seq.orbit.gate` | Orbit Gate | Orbit Gate | 5–100 % | 50 % |
| `seq.orbit.lN.len` | Lane N Length | Spur N Länge | 1–16 | 16 / 12 / 10 / 7 |
| `seq.orbit.lN.rate` | Lane N Rate | Spur N Raster | as Euclid | 1/16 |
| `seq.orbit.lN.transpose` | Lane N Transpose | Spur N Transponieren | ±24 st | 0 |
| `sync.src` | Sync Source | Sync-Quelle | Internal, MIDI Clock, Link | Internal |
| `sync.smooth` | Clock Smoothing | Clock-Glättung | 0–1 | 0.5 |
| `sync.clockout` | MIDI Clock Out | MIDI-Clock senden | off / on | off |
| `sync.notesout` | Sequencer Notes Out | Sequencer-Noten senden | off / on | off |
