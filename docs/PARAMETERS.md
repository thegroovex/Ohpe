# Ohpe parameters

*English · Deutsch: German names in the tables. The complete, ordered table of every parameter is
printed by `ohpe_param_dump --markdown`; `tests/core/golden/param_ids_v1.txt` lists every ID in its
permanent order. IDs are append-only (see [ARCHITECTURE.md](ARCHITECTURE.md)); sections are added at
the end of this file. Note names: C4 = MIDI 60.*

## Phase 2 – Grain, Vocal, Sampler engines and drum kit

Appended after the Wavetable, Pluck and Cluster parameters (`p1.cluster.drift`). The first four parameters of each engine are its macros.
Guides: [English](en/GRAIN-VOCAL-SAMPLER-DRUM.md) · [Deutsch](de/GRAIN-VOKAL-SAMPLER-DRUM.md).

### Grain (`p1.grain.*`)

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `p1.grain.pos` | Position | Position | 0 … 1 | 0.3 |
| `p1.grain.size` | Grain Size | Korngröße | 5 … 500 | 120 |
| `p1.grain.density` | Density | Dichte | 1 … 100 | 24 |
| `p1.grain.spray` | Spray | Streuung | 0 … 1 | 0.15 |
| `p1.grain.pitch` | Grain Pitch | Korn-Tonhöhe | -24 … 24 | 0 |
| `p1.grain.reverse` | Reverse Chance | Rückwärts-Anteil | 0 … 1 | 0 |
| `p1.grain.spread` | Stereo Spread | Stereobreite | 0 … 1 | 0.6 |
| `p1.grain.scan` | Scan Speed | Scan-Tempo | -2 … 2 | 0.1 |
| `p1.grain.source` | Source | Quelle | Sample, Tape | 0 |
| `p1.grain.tone` | Tone | Klang | 0 … 1 | 1 |

### Vocal (`p1.vocal.*`)

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `p1.vocal.vowel` | Vowel | Vokal | 0 … 4 | 0 |
| `p1.vocal.formant` | Formant Shift | Formant-Verschiebung | -12 … 12 | 0 |
| `p1.vocal.breath` | Breath | Atem | 0 … 1 | 0.15 |
| `p1.vocal.tension` | Tension | Spannung | 0 … 1 | 0.5 |
| `p1.vocal.vibdepth` | Vibrato Depth | Vibrato-Tiefe | 0 … 1 | 0.2 |
| `p1.vocal.vibrate` | Vibrato Rate | Vibrato-Tempo | 0.5 … 10 | 5.5 |
| `p1.vocal.jitter` | Jitter | Jitter | 0 … 1 | 0.1 |

### Sampler (`p1.sampler.*`)

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `p1.sampler.start` | Start | Start | 0 … 1 | 0 |
| `p1.sampler.tone` | Tone | Klang | 0 … 1 | 1 |
| `p1.sampler.loopstart` | Loop Start | Loop-Start | 0 … 1 | 0 |
| `p1.sampler.loopend` | Loop End | Loop-Ende | 0 … 1 | 1 |
| `p1.sampler.root` | Root Key | Grundton | 0 … 127 | 60 |
| `p1.sampler.loop` | Loop | Loop | Off, Forward, Ping-Pong | 0 |
| `p1.sampler.reverse` | Reverse | Rückwärts | 0 … 1 | 0 |
| `p1.sampler.mode` | Mode | Modus | Zones, Slices | 0 |
| `p1.sampler.slicesens` | Slice Sensitivity | Slice-Empfindlichkeit | 0 … 1 | 0.5 |
| `p1.sampler.slicebase` | First Slice Key | Erste Slice-Taste | 0 … 127 | 36 |

### Drum kit (`drum.*`)

`drum.vol`, `drum.base`, then for every pad N = 1 … 24 the eight parameters
`drum.padN.src`, `.type`, `.tune`, `.decay`, `.level`, `.pan`, `.choke`, `.tone` (pad 1 shown; pads 2 – 24 are
identical apart from the defaults of the factory kit below).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `drum.vol` | Drum Volume | Drum-Lautstärke | 0 … 1 | 0.8 |
| `drum.base` | Drum Base Note | Drum-Grundnote | 0 … 104 | 36 |
| `drum.pad1.src` | Pad 1 Source | Pad 1 Quelle | Sample, Synth | 0 |
| `drum.pad1.type` | Pad 1 Synth Model | Pad 1 Synth-Modell | Kick, Snare, Hat, Clap, Tom, Perc | 0 |
| `drum.pad1.tune` | Pad 1 Tune | Pad 1 Stimmung | -24 … 24 | 0 |
| `drum.pad1.decay` | Pad 1 Decay | Pad 1 Abklingen | 0 … 1 | 0.5 |
| `drum.pad1.level` | Pad 1 Level | Pad 1 Pegel | 0 … 1 | 0.95 |
| `drum.pad1.pan` | Pad 1 Pan | Pad 1 Panorama | -1 … 1 | 0 |
| `drum.pad1.choke` | Pad 1 Choke Group | Pad 1 Choke-Gruppe | 0 … 8 | 0 |
| `drum.pad1.tone` | Pad 1 Tone | Pad 1 Klangfarbe | 0 … 1 | 0.5 |

Factory kit (MIDI channel 10, `drum.base` = 36). "Recording" = the pad plays a recorded sample
(CC0, see [THIRD-PARTY.md](../THIRD-PARTY.md)); the synth model plays without a sample or with
*Source = Synth*.

| Pad | Note | Sound | Recording | Synth model | Choke | Level | Pan |
|---|---|---|---|---|---|---|---|
| 1 | 36 (C2) | kick | yes | Kick | 0 | 0.95 | +0.00 |
| 2 | 37 (C#2) | kick, snares on | yes | Kick | 0 | 0.90 | +0.00 |
| 3 | 38 (D2) | snare | yes | Snare | 0 | 0.85 | +0.00 |
| 4 | 39 (D#2) | rimshot | yes | Snare | 0 | 0.75 | +0.00 |
| 5 | 40 (E2) | clap | no (model) | Clap | 0 | 0.70 | +0.00 |
| 6 | 41 (F2) | closed hat | yes | Hat | 1 | 0.60 | +0.00 |
| 7 | 42 (F#2) | open hat | yes | Hat | 1 | 0.50 | +0.00 |
| 8 | 43 (G2) | pedal hat | yes | Hat | 1 | 0.50 | +0.00 |
| 9 | 44 (G#2) | low tom | yes | Tom | 0 | 0.80 | +0.00 |
| 10 | 45 (A2) | high tom | yes | Tom | 0 | 0.80 | +0.00 |
| 11 | 46 (A#2) | floor tom | yes | Tom | 0 | 0.80 | +0.00 |
| 12 | 47 (B2) | cross-stick | yes | Perc | 0 | 0.70 | +0.00 |
| 13 | 48 (C3) | ride | yes | Hat | 0 | 0.45 | +0.00 |
| 14 | 49 (C#3) | ride bell | yes | Perc | 0 | 0.45 | +0.00 |
| 15 | 50 (D3) | crash | yes | Hat | 0 | 0.45 | +0.00 |
| 16 | 51 (D#3) | sizzle crash | yes | Hat | 0 | 0.40 | +0.00 |
| 17 | 52 (E3) | half-open hat | yes | Hat | 1 | 0.50 | +0.00 |
| 18 | 53 (F3) | ghost snare | yes | Snare | 0 | 0.40 | +0.00 |
| 19 | 54 (F#3) | cowbell | yes | Perc | 0 | 0.45 | +0.30 |
| 20 | 55 (G3) | tambourine | yes | Hat | 0 | 0.40 | -0.30 |
| 21 | 56 (G#3) | shaker | yes | Hat | 0 | 0.35 | +0.35 |
| 22 | 57 (A3) | claves | yes | Perc | 0 | 0.45 | -0.25 |
| 23 | 58 (A#3) | conga | yes | Tom | 0 | 0.65 | -0.35 |
| 24 | 59 (B3) | bongo | yes | Tom | 0 | 0.60 | +0.35 |
## Phase 2 – Tape machine, mixer dynamics, FX slot (0.2.0)

85 parameters, appended in this order after the drum kit parameters (`drum.pad24.tone`). Guides:
[English](en/TAPE-MIXER-FX.md) · [Deutsch](de/BAND-MISCHER-FX.md).

### Mixer dynamics (`mix.*`)

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `mix.ch1.comp` | Ch 1 Compressor | Kanal 1 Kompressor | 0 … 1 | 0 |
| `mix.ch2.comp` | Ch 2 Compressor | Kanal 2 Kompressor | 0 … 1 | 0 |
| `mix.ch3.comp` | Ch 3 Compressor | Kanal 3 Kompressor | 0 … 1 | 0 |
| `mix.ch4.comp` | Ch 4 Compressor | Kanal 4 Kompressor | 0 … 1 | 0 |
| `mix.ch5.comp` | Ch 5 Compressor | Kanal 5 Kompressor | 0 … 1 | 0 |
| `mix.ch6.comp` | Ch 6 Compressor | Kanal 6 Kompressor | 0 … 1 | 0 |
| `mix.ch7.comp` | Ch 7 Compressor | Kanal 7 Kompressor | 0 … 1 | 0 |
| `mix.ch8.comp` | Ch 8 Compressor | Kanal 8 Kompressor | 0 … 1 | 0 |
| `mix.master.comp` | Master Compressor | Master-Kompressor | 0 … 1 | 0 |
| `mix.master.comp.thresh` | Comp Threshold | Kompressor Schwelle | -40 … 0 | -12 |
| `mix.master.comp.ratio` | Comp Ratio | Kompressor Ratio | 1 … 20 | 3 |
| `mix.master.comp.attack` | Comp Attack | Kompressor Attack | 0.0001 … 0.1 | 0.01 |
| `mix.master.comp.release` | Comp Release | Kompressor Release | 0.01 … 2 | 0.15 |
| `mix.master.comp.makeup` | Comp Makeup | Kompressor Aufholverstärkung | 0 … 24 | 0 |
| `mix.master.tapesat` | Master Tape Saturation | Master-Bandsättigung | 0 … 1 | 0 |
| `mix.master.ceiling` | Limiter Ceiling | Limiter-Obergrenze | -12 … 0 | -0.3 |

### Tape transport and character (`tape.*`)

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `tape.play` | Tape Play | Tape Wiedergabe | 0 … 1 | 0 |
| `tape.rec` | Tape Record | Tape Aufnahme | 0 … 1 | 0 |
| `tape.speed` | Tape Speed | Tape-Geschwindigkeit | 0.5 … 2 | 1 |
| `tape.reverse` | Tape Reverse | Tape rückwärts | 0 … 1 | 0 |
| `tape.loop` | Tape Loop | Tape-Schleife | 0 … 1 | 0 |
| `tape.loopin` | Loop In | Schleife Start | 0 … 3600 | 0 |
| `tape.loopout` | Loop Out | Schleife Ende | 0 … 3600 | 8 |
| `tape.overdub` | Overdub | Overdub | 0 … 1 | 1 |
| `tape.sat` | Tape Saturation | Tape-Sättigung | 0 … 1 | 0 |
| `tape.wow` | Wow | Wow | 0 … 1 | 0 |
| `tape.flutter` | Flutter | Flutter | 0 … 1 | 0 |
| `tape.hiss` | Tape Hiss | Bandrauschen | 0 … 1 | 0 |

### Tape tracks (`tape.tr1.*` … `tape.tr8.*`)

Shown for track 1; tracks 2–8 are identical (`tape.tr2.arm` …). Order: track 1 arm, source,
level, direct out, then track 2 …

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `tape.tr1.arm` | Track 1 Arm | Spur 1 Aufnahmebereit | 0 … 1 | 0 |
| `tape.tr1.src` | Track 1 Source | Spur 1 Quelle | Input, Part 1, Part 2, Part 3, Part 4, Drum | 1 |
| `tape.tr1.level` | Track 1 Level | Spur 1 Pegel | -60 … 6 | 0 |
| `tape.tr1.out` | Track 1 Direct Out | Spur 1 Direktausgang | Mixer only, Out 1, Out 2, Out 3, Out 4, Out 5, Out 6, Out 7, Out 8 | 0 |

### FX slot of part 1 (`p1.fx.*`)

`p1.fx.type`: Off, Chorus, Phaser, Bitcrush, Filter Drive, Delay, Reverb (append-only list).
Each effect keeps its own parameters, so switching the type and back restores the sound.

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `p1.fx.type` | FX Slot | FX-Slot | Off, Chorus, Phaser, Bitcrush, Filter Drive, Delay, Reverb | 0 |
| `p1.fx.chorus.rate` | Chorus Rate | Chorus-Tempo | 0.05 … 5 | 0.6 |
| `p1.fx.chorus.depth` | Chorus Depth | Chorus-Tiefe | 0 … 1 | 0.5 |
| `p1.fx.chorus.mix` | Chorus Mix | Chorus-Mischung | 0 … 1 | 0.5 |
| `p1.fx.phaser.rate` | Phaser Rate | Phaser-Tempo | 0.05 … 8 | 0.4 |
| `p1.fx.phaser.depth` | Phaser Depth | Phaser-Tiefe | 0 … 1 | 0.7 |
| `p1.fx.phaser.feedback` | Phaser Feedback | Phaser-Feedback | 0 … 0.95 | 0.5 |
| `p1.fx.phaser.mix` | Phaser Mix | Phaser-Mischung | 0 … 1 | 0.5 |
| `p1.fx.bitcrush.bits` | Bit Depth | Bittiefe | 1 … 16 | 8 |
| `p1.fx.bitcrush.downsample` | Downsample | Downsampling | 1 … 64 | 1 |
| `p1.fx.bitcrush.mix` | Bitcrush Mix | Bitcrusher-Mischung | 0 … 1 | 1 |
| `p1.fx.filterdrive.cutoff` | Drive Cutoff | Drive-Cutoff | 20 … 20000 | 2000 |
| `p1.fx.filterdrive.reso` | Drive Resonance | Drive-Resonanz | 0 … 1 | 0.3 |
| `p1.fx.filterdrive.drive` | Drive | Drive | 0 … 1 | 0.3 |
| `p1.fx.filterdrive.mode` | Drive Filter Type | Drive-Filtertyp | Low-pass, Band-pass, High-pass | 0 |
| `p1.fx.filterdrive.mix` | Drive Mix | Drive-Mischung | 0 … 1 | 1 |
| `p1.fx.delay.time` | Delay Time | Delay-Zeit | 0.01 … 2 | 0.375 |
| `p1.fx.delay.feedback` | Feedback | Feedback | 0 … 0.95 | 0.4 |
| `p1.fx.delay.tone` | Tone | Klang | 0 … 1 | 0.6 |
| `p1.fx.delay.pingpong` | Ping-Pong | Ping-Pong | 0 … 1 | 1 |
| `p1.fx.delay.mix` | Delay Mix | Delay-Mischung | 0 … 1 | 0.35 |
| `p1.fx.reverb.size` | Size | Größe | 0 … 1 | 0.6 |
| `p1.fx.reverb.damping` | Damping | Dämpfung | 0 … 1 | 0.4 |
| `p1.fx.reverb.predelay` | Pre-Delay | Pre-Delay | 0 … 0.2 | 0.01 |
| `p1.fx.reverb.mix` | Reverb Mix | Hall-Mischung | 0 … 1 | 0.35 |

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

## 0.2 – engines of parts 2–4 / Engines der Parts 2–4

Appended last: `pN.wavetable.*`, `pN.pluck.*`, `pN.cluster.*`, `pN.grain.*`, `pN.vocal.*` and
`pN.sampler.*` for N = 2, 3, 4 (in this order per part) – the same parameters, ranges and defaults
as their `p1.*` counterparts (tables above; Wavetable, Pluck and Cluster in the engine guides,
`docs/en/engines/`). / Zuletzt angehängt: dieselben Parameter, Bereiche und Standardwerte wie bei
`p1.*` (Tabellen oben; Wavetable, Pluck und Cluster in den Engine-Anleitungen, `docs/de/engines/`),
für die Parts 2, 3 und 4.

## Phase 3 – remote controls (0.3)

Appended after `p4.sampler.slicebase`. They make UI functions MIDI-mappable and are not part of
presets. A trigger (`off / on`) fires when it rises to "on" (CC ≥ 64 or a note); see
[en/PRESETS.md](en/PRESETS.md) and [en/USER-INTERFACE.md](en/USER-INTERFACE.md).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `lib.open` | Library | Bibliothek | trigger | off |
| `lib.prev` | Previous Preset | Vorheriges Preset | trigger | off |
| `lib.next` | Next Preset | Nächstes Preset | trigger | off |
| `ui.part` | Edit Part | Bearbeiteter Part | 1–4 | 1 |
| `tape.edit.track` | Tape Edit Track | Tape-Bearbeitungsspur | 1–8 | 1 |
| `tape.edit.lift` | Tape Lift | Tape Heben | trigger | off |
| `tape.edit.cut` | Tape Cut | Tape Schneiden | trigger | off |
| `tape.edit.drop` | Tape Drop | Tape Einsetzen | trigger | off |
| `tape.edit.splice` | Tape Splice | Tape Einfügen | trigger | off |
| `tape.edit.undo` | Tape Undo | Tape Rückgängig | trigger | off |
| `tape.edit.redo` | Tape Redo | Tape Wiederholen | trigger | off |
| `tape.edit.rtz` | Tape Return to Zero | Tape zum Anfang | trigger | off |
