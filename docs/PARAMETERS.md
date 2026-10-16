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

## 0.4 – factory sounds (package F)

Appended after `tape.edit.rtz`. Guides: [English](en/FACTORY-SOUNDS.md) · [Deutsch](de/FACTORY-SOUNDS.md).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `p1.sampler.inst`, `p1.grain.inst` … `p4.sampler.inst`, `p4.grain.inst` | Instrument | Instrument | Built-in, Kalimba, Marimba, Vibraphone, Glockenspiel, Harp, Harpsichord, Recorder, Singing Glass, Zither, Bowed Metal | Built-in |
| `drum.kit` | Drum Kit | Drum-Kit | Studio, Tight, Room, Lo-Fi, Hybrid, Boom (0.5: the list grows at the end) | Studio |
| `seq.step.drums` | Step Seq to Drums | Step-Sequenzer an Drums | off / on | off |
| `seq.euclid.drums` | Euclid to Drums | Euklid an Drums | off / on | off |
| `seq.orbit.drums` | Orbit to Drums | Orbit an Drums | off / on | off |
| `p1.gain` … `p4.gain` | Part N Gain | Part N Gain | −24 … +24 dB | 0 dB |

Order: `p1.sampler.inst`, `p1.grain.inst`, `p2.sampler.inst`, `p2.grain.inst`, … `p4.grain.inst`,
`drum.kit`, `seq.step.drums`, `seq.euclid.drums`, `seq.orbit.drums`, `p1.gain` … `p4.gain`.

## 0.5 – sound revision 1: groove, poly glide, synced delay

Appended after `p4.gain` (index 900 onwards). Guides: [English](en/SEQUENCERS.md) · [Deutsch](de/SEQUENZER.md),
delay sync: [English](en/TAPE-MIXER-FX.md) · [Deutsch](de/BAND-MISCHER-FX.md).

| # | ID | English | Deutsch | Range | Default |
|---|---|---|---|---|---|
| 900 | `seq.orbit.swing` | Orbit Swing | Orbit Swing | 0–75 % (off-beat sixteenths late by swing ÷ 2 of a sixteenth) | 0 % |
| 901 | `seq.orbit.human` | Orbit Humanise | Orbit Humanisieren | 0–100 % (up to 10 ms late, velocity ±16) | 0 % |
| 902 | `seq.euclid.swing` | Euclid Swing | Euklid Swing | 0–75 % | 0 % |
| 903 | `seq.euclid.human` | Euclid Humanise | Euklid Humanisieren | 0–100 % | 0 % |
| 904–907 | `p1.polyglide` … `p4.polyglide` | Part N Poly Glide | Part N Poly-Glide | off / on (chords glide voice by voice) | off |
| 908–911 | `seq.euclid.r1.vel` … `seq.euclid.r4.vel` | Ring N Velocity | Ring N Anschlag | 0–127 (0 = `seq.euclid.vel`) | 0 |
| 912–915 | `seq.orbit.l1.part` … `seq.orbit.l4.part` | Lane N Target Part | Spur N Ziel-Part | 0–4 (0 = the Orbit target / drum kit) | 0 |
| 916 | `fxa.delay.sync` | Delay Sync | Delay-Sync | Off, 1/16, 1/8T, 1/8, 1/4T, 1/8., 1/4, 1/4., 1/2 | Off |

Behaviour change in 0.5: a step-sequencer step at gate 100 % (and every Orbit step at `seq.orbit.gate`
100 %) ties into the next step when it plays the same note (not on the drum kit).

## 0.6 – sound revision 2: portamento, pitch envelope, crackle, master low-pass

Appended after `fxa.delay.sync` (index 917 onwards). Guides: portamento and pitch envelope
[English](en/SEQUENCERS.md) · [Deutsch](de/SEQUENZER.md), master low-pass
[English](en/TAPE-MIXER-FX.md) · [Deutsch](de/BAND-MISCHER-FX.md), crackle / Boom / lo-fi
[English](en/FACTORY-SOUNDS.md) · [Deutsch](de/FACTORY-SOUNDS.md).

| # | ID | English | Deutsch | Range | Default |
|---|---|---|---|---|---|
| 917, 922, 927, 932 | `p1.glide.mode` … `p4.glide.mode` | Part N Glide Mode | Part N Glide-Modus | Auto, Legato, Always (DE: Auto, Legato, Immer) | Auto |
| 918, 923, 928, 933 | `p1.glide.rate` … `p4.glide.rate` | Part N Glide Timing | Part N Glide-Timing | Constant Time, Constant Rate (DE: Feste Zeit, Feste Rate) | Constant Time |
| 919, 924, 929, 934 | `p1.pitchenv.amt` … `p4.pitchenv.amt` | Part N Pitch Env Amount | Part N Tonhöhen-Hüllkurve Menge | −24 … +24 semitones (negative = rise from below) | 0 |
| 920, 925, 930, 935 | `p1.pitchenv.time` … `p4.pitchenv.time` | Part N Pitch Env Time | Part N Tonhöhen-Hüllkurve Zeit | 0.005 … 4 s | 0.25 s |
| 921, 926, 931, 936 | `p1.pitchenv.curve` … `p4.pitchenv.curve` | Part N Pitch Env Curve | Part N Tonhöhen-Hüllkurve Kurve | 0 (linear) … 1 (fast start) | 0.5 |
| 937, 939, 941, 943 | `p1.analog.osclevel` … `p4.analog.osclevel` | Osc Level | Osz.-Pegel | 0 … 1 (0 = noise / sub / crackle only) | 1 |
| 938, 940, 942, 944 | `p1.analog.crackle` … `p4.analog.crackle` | Crackle | Knistern | 0 … 1 (vinyl-style clicks into the filter) | 0 |
| 945 | `mix.master.lowpass` | Master Low-Pass | Master-Tiefpass | 500 … 20000 Hz (20 kHz = off) | 20000 |

Order: the five glide / pitch-envelope parameters of part 1, then parts 2, 3, 4; then
`p1.analog.osclevel`, `p1.analog.crackle`, … `p4.analog.crackle`; then `mix.master.lowpass`.

Behaviour changes in 0.6: with one voice, releasing the newest key returns to the previous key still
held (legato). A step-sequencer or Orbit step at gate 100 % ends one sample after the next step
starts, so a different next note overlaps it (legato). MIDI CC 65 switches portamento off / on and
CC 5 sets the glide time of the part on that channel.

## 0.8 – sound revision 3: FX slots for every part, new FX types, layer engine, keyboard instruments, MPE

Appended after `mix.master.lowpass` (index 946 onwards). Guides: FX slot types
[English](en/TAPE-MIXER-FX.md) · [Deutsch](de/BAND-MISCHER-FX.md), layer engine
[English](en/engines/layer.md) · [Deutsch](de/engines/layer.md), wavetable unison / FM
[English](en/engines/wavetable.md) · [Deutsch](de/engines/wavetable.md), keyboard instruments
[English](en/GRAIN-VOCAL-SAMPLER-DRUM.md) · [Deutsch](de/GRAIN-VOKAL-SAMPLER-DRUM.md), MPE
[English](en/MIDI.md) · [Deutsch](de/MIDI.md).

Order: the FX slots of parts 2, 3, 4 with the 0.2 FX types (`pN.fx.type` … `pN.fx.reverb.mix`); then the
parameters of the eight new FX types (Shaper, Squash, Comb, Formant, Flanger, Wobble, Tremolo, Width) for
parts 1, 2, 3, 4; then the layer engine of parts 1–4; then per part the late Wavetable, FM, Sampler and
Grain parameters; then the MPE parameters of parts 1–4; then the bass mono frequency of parts 1–4. `ohpe_param_dump --markdown` prints every row
with its own index.

| # | ID | English | Deutsch | Range | Default |
|---|---|---|---|---|---|
| 946, 971, 996 | `p2.fx.type` … `p4.fx.type` | FX Slot | FX-Slot | Off, Chorus, Phaser, Bitcrush, Filter Drive, Delay, Reverb, Shaper, Squash, Comb, Formant, Flanger, Wobble, Tremolo, Width | 0 |
| 947, 972, 997 | `p2.fx.chorus.rate` … `p4.fx.chorus.rate` | Chorus Rate | Chorus-Tempo | 0.05 … 5 | 0.6 |
| 948, 973, 998 | `p2.fx.chorus.depth` … `p4.fx.chorus.depth` | Chorus Depth | Chorus-Tiefe | 0 … 1 | 0.5 |
| 949, 974, 999 | `p2.fx.chorus.mix` … `p4.fx.chorus.mix` | Chorus Mix | Chorus-Mischung | 0 … 1 | 0.5 |
| 950, 975, 1000 | `p2.fx.phaser.rate` … `p4.fx.phaser.rate` | Phaser Rate | Phaser-Tempo | 0.05 … 8 | 0.4 |
| 951, 976, 1001 | `p2.fx.phaser.depth` … `p4.fx.phaser.depth` | Phaser Depth | Phaser-Tiefe | 0 … 1 | 0.7 |
| 952, 977, 1002 | `p2.fx.phaser.feedback` … `p4.fx.phaser.feedback` | Phaser Feedback | Phaser-Feedback | 0 … 0.95 | 0.5 |
| 953, 978, 1003 | `p2.fx.phaser.mix` … `p4.fx.phaser.mix` | Phaser Mix | Phaser-Mischung | 0 … 1 | 0.5 |
| 954, 979, 1004 | `p2.fx.bitcrush.bits` … `p4.fx.bitcrush.bits` | Bit Depth | Bittiefe | 1 … 16 | 8 |
| 955, 980, 1005 | `p2.fx.bitcrush.downsample` … `p4.fx.bitcrush.downsample` | Downsample | Downsampling | 1 … 64 | 1 |
| 956, 981, 1006 | `p2.fx.bitcrush.mix` … `p4.fx.bitcrush.mix` | Bitcrush Mix | Bitcrusher-Mischung | 0 … 1 | 1 |
| 957, 982, 1007 | `p2.fx.filterdrive.cutoff` … `p4.fx.filterdrive.cutoff` | Drive Cutoff | Drive-Cutoff | 20 … 20000 | 2000 |
| 958, 983, 1008 | `p2.fx.filterdrive.reso` … `p4.fx.filterdrive.reso` | Drive Resonance | Drive-Resonanz | 0 … 1 | 0.3 |
| 959, 984, 1009 | `p2.fx.filterdrive.drive` … `p4.fx.filterdrive.drive` | Drive | Drive | 0 … 1 | 0.3 |
| 960, 985, 1010 | `p2.fx.filterdrive.mode` … `p4.fx.filterdrive.mode` | Drive Filter Type | Drive-Filtertyp | Low-pass, Band-pass, High-pass | 0 |
| 961, 986, 1011 | `p2.fx.filterdrive.mix` … `p4.fx.filterdrive.mix` | Drive Mix | Drive-Mischung | 0 … 1 | 1 |
| 962, 987, 1012 | `p2.fx.delay.time` … `p4.fx.delay.time` | Delay Time | Delay-Zeit | 0.01 … 2 | 0.375 |
| 963, 988, 1013 | `p2.fx.delay.feedback` … `p4.fx.delay.feedback` | Feedback | Feedback | 0 … 0.95 | 0.4 |
| 964, 989, 1014 | `p2.fx.delay.tone` … `p4.fx.delay.tone` | Tone | Klang | 0 … 1 | 0.6 |
| 965, 990, 1015 | `p2.fx.delay.pingpong` … `p4.fx.delay.pingpong` | Ping-Pong | Ping-Pong | 0 … 1 | 1 |
| 966, 991, 1016 | `p2.fx.delay.mix` … `p4.fx.delay.mix` | Delay Mix | Delay-Mischung | 0 … 1 | 0.35 |
| 967, 992, 1017 | `p2.fx.reverb.size` … `p4.fx.reverb.size` | Size | Größe | 0 … 1 | 0.6 |
| 968, 993, 1018 | `p2.fx.reverb.damping` … `p4.fx.reverb.damping` | Damping | Dämpfung | 0 … 1 | 0.4 |
| 969, 994, 1019 | `p2.fx.reverb.predelay` … `p4.fx.reverb.predelay` | Pre-Delay | Pre-Delay | 0 … 0.2 | 0.01 |
| 970, 995, 1020 | `p2.fx.reverb.mix` … `p4.fx.reverb.mix` | Reverb Mix | Hall-Mischung | 0 … 1 | 0.35 |
| 1021, 1075, 1129, 1183 | `p1.fx.shaper.type` … `p4.fx.shaper.type` | Shaper Curve | Shaper-Kurve | Soft, Hard, Fold, Tube, Rectify, Sine, Crush | 0 |
| 1022, 1076, 1130, 1184 | `p1.fx.shaper.drive` … `p4.fx.shaper.drive` | Shaper Drive | Shaper-Drive | 0 … 1 | 0.3 |
| 1023, 1077, 1131, 1185 | `p1.fx.shaper.bias` … `p4.fx.shaper.bias` | Shaper Bias | Shaper-Bias | -0.5 … 0.5 | 0 |
| 1024, 1078, 1132, 1186 | `p1.fx.shaper.tone` … `p4.fx.shaper.tone` | Shaper Tone | Shaper-Klang | 500 … 20000 | 12000 |
| 1025, 1079, 1133, 1187 | `p1.fx.shaper.mix` … `p4.fx.shaper.mix` | Shaper Mix | Shaper-Mischung | 0 … 1 | 1 |
| 1026, 1080, 1134, 1188 | `p1.fx.shaper.output` … `p4.fx.shaper.output` | Shaper Output | Shaper-Ausgang | -24 … 12 | 0 |
| 1027, 1081, 1135, 1189 | `p1.fx.squash.depth` … `p4.fx.squash.depth` | Squash Depth | Squash-Tiefe | 0 … 1 | 0.5 |
| 1028, 1082, 1136, 1190 | `p1.fx.squash.time` … `p4.fx.squash.time` | Squash Time | Squash-Zeit | 0 … 1 | 0.4 |
| 1029, 1083, 1137, 1191 | `p1.fx.squash.upward` … `p4.fx.squash.upward` | Squash Upward | Squash aufwärts | 0 … 1 | 0.6 |
| 1030, 1084, 1138, 1192 | `p1.fx.squash.low` … `p4.fx.squash.low` | Squash Low | Squash Tiefen | -12 … 12 | 0 |
| 1031, 1085, 1139, 1193 | `p1.fx.squash.mid` … `p4.fx.squash.mid` | Squash Mid | Squash Mitten | -12 … 12 | 0 |
| 1032, 1086, 1140, 1194 | `p1.fx.squash.high` … `p4.fx.squash.high` | Squash High | Squash Höhen | -12 … 12 | 0 |
| 1033, 1087, 1141, 1195 | `p1.fx.squash.mix` … `p4.fx.squash.mix` | Squash Mix | Squash-Mischung | 0 … 1 | 1 |
| 1034, 1088, 1142, 1196 | `p1.fx.squash.output` … `p4.fx.squash.output` | Squash Output | Squash-Ausgang | -24 … 12 | 0 |
| 1035, 1089, 1143, 1197 | `p1.fx.squash.monolow` … `p4.fx.squash.monolow` | Squash Low Mono | Squash Tiefen mono | 0 … 1 | 0 |
| 1036, 1090, 1144, 1198 | `p1.fx.comb.freq` … `p4.fx.comb.freq` | Comb Frequency | Kamm-Frequenz | 20 … 5000 | 300 |
| 1037, 1091, 1145, 1199 | `p1.fx.comb.feedback` … `p4.fx.comb.feedback` | Comb Feedback | Kamm-Feedback | -0.95 … 0.95 | 0.6 |
| 1038, 1092, 1146, 1200 | `p1.fx.comb.damp` … `p4.fx.comb.damp` | Comb Damping | Kamm-Dämpfung | 0 … 1 | 0.3 |
| 1039, 1093, 1147, 1201 | `p1.fx.comb.rate` … `p4.fx.comb.rate` | Comb Rate | Kamm-Tempo | 0.01 … 10 | 0.2 |
| 1040, 1094, 1148, 1202 | `p1.fx.comb.depth` … `p4.fx.comb.depth` | Comb Depth | Kamm-Tiefe | 0 … 1 | 0.3 |
| 1041, 1095, 1149, 1203 | `p1.fx.comb.spread` … `p4.fx.comb.spread` | Comb Spread | Kamm-Breite | 0 … 1 | 0.3 |
| 1042, 1096, 1150, 1204 | `p1.fx.comb.mix` … `p4.fx.comb.mix` | Comb Mix | Kamm-Mischung | 0 … 1 | 0.5 |
| 1043, 1097, 1151, 1205 | `p1.fx.formant.vowel` … `p4.fx.formant.vowel` | Vowel | Vokal | 0 … 1 | 0 |
| 1044, 1098, 1152, 1206 | `p1.fx.formant.shift` … `p4.fx.formant.shift` | Formant Shift | Formant-Verschiebung | -12 … 12 | 0 |
| 1045, 1099, 1153, 1207 | `p1.fx.formant.reso` … `p4.fx.formant.reso` | Formant Resonance | Formant-Resonanz | 0 … 1 | 0.5 |
| 1046, 1100, 1154, 1208 | `p1.fx.formant.rate` … `p4.fx.formant.rate` | Formant Rate | Formant-Tempo | 0.01 … 20 | 1 |
| 1047, 1101, 1155, 1209 | `p1.fx.formant.sync` … `p4.fx.formant.sync` | Formant Sync | Formant-Sync | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/2T, 1/4T, 1/8T, 1/16T, 1/2., 1/4., 1/8., 2/1, 4/1 | 0 |
| 1048, 1102, 1156, 1210 | `p1.fx.formant.depth` … `p4.fx.formant.depth` | Formant LFO Depth | Formant-LFO-Tiefe | 0 … 1 | 0 |
| 1049, 1103, 1157, 1211 | `p1.fx.formant.mix` … `p4.fx.formant.mix` | Formant Mix | Formant-Mischung | 0 … 1 | 1 |
| 1050, 1104, 1158, 1212 | `p1.fx.flanger.rate` … `p4.fx.flanger.rate` | Flanger Rate | Flanger-Tempo | 0.01 … 5 | 0.2 |
| 1051, 1105, 1159, 1213 | `p1.fx.flanger.depth` … `p4.fx.flanger.depth` | Flanger Depth | Flanger-Tiefe | 0 … 1 | 0.7 |
| 1052, 1106, 1160, 1214 | `p1.fx.flanger.feedback` … `p4.fx.flanger.feedback` | Flanger Feedback | Flanger-Feedback | -0.95 … 0.95 | 0.5 |
| 1053, 1107, 1161, 1215 | `p1.fx.flanger.delay` … `p4.fx.flanger.delay` | Flanger Delay | Flanger-Verzögerung | 0.5 … 10 | 2 |
| 1054, 1108, 1162, 1216 | `p1.fx.flanger.mix` … `p4.fx.flanger.mix` | Flanger Mix | Flanger-Mischung | 0 … 1 | 0.5 |
| 1055, 1109, 1163, 1217 | `p1.fx.wobble.rate` … `p4.fx.wobble.rate` | Wobble Rate | Wobble-Tempo | 0.05 … 20 | 2 |
| 1056, 1110, 1164, 1218 | `p1.fx.wobble.sync` … `p4.fx.wobble.sync` | Wobble Sync | Wobble-Sync | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/2T, 1/4T, 1/8T, 1/16T, 1/2., 1/4., 1/8., 2/1, 4/1 | 4 |
| 1057, 1111, 1165, 1219 | `p1.fx.wobble.shape` … `p4.fx.wobble.shape` | Wobble Shape | Wobble-Form | Sine, Triangle, Saw Down, Saw Up, Square, S&H | 0 |
| 1058, 1112, 1166, 1220 | `p1.fx.wobble.low` … `p4.fx.wobble.low` | Wobble Low | Wobble tief | 30 … 8000 | 150 |
| 1059, 1113, 1167, 1221 | `p1.fx.wobble.high` … `p4.fx.wobble.high` | Wobble High | Wobble hoch | 100 … 18000 | 3000 |
| 1060, 1114, 1168, 1222 | `p1.fx.wobble.reso` … `p4.fx.wobble.reso` | Wobble Resonance | Wobble-Resonanz | 0 … 1 | 0.5 |
| 1061, 1115, 1169, 1223 | `p1.fx.wobble.mode` … `p4.fx.wobble.mode` | Wobble Filter | Wobble-Filter | Low-pass, Band-pass | 0 |
| 1062, 1116, 1170, 1224 | `p1.fx.wobble.drive` … `p4.fx.wobble.drive` | Wobble Drive | Wobble-Drive | 0 … 1 | 0.2 |
| 1063, 1117, 1171, 1225 | `p1.fx.wobble.retrig` … `p4.fx.wobble.retrig` | Wobble Retrigger | Wobble-Neustart | 0 … 1 | 1 |
| 1064, 1118, 1172, 1226 | `p1.fx.wobble.mix` … `p4.fx.wobble.mix` | Wobble Mix | Wobble-Mischung | 0 … 1 | 1 |
| 1065, 1119, 1173, 1227 | `p1.fx.tremolo.rate` … `p4.fx.tremolo.rate` | Tremolo Rate | Tremolo-Tempo | 0.05 … 20 | 4 |
| 1066, 1120, 1174, 1228 | `p1.fx.tremolo.sync` … `p4.fx.tremolo.sync` | Tremolo Sync | Tremolo-Sync | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/2T, 1/4T, 1/8T, 1/16T, 1/2., 1/4., 1/8., 2/1, 4/1 | 0 |
| 1067, 1121, 1175, 1229 | `p1.fx.tremolo.depth` … `p4.fx.tremolo.depth` | Tremolo Depth | Tremolo-Tiefe | 0 … 1 | 0.5 |
| 1068, 1122, 1176, 1230 | `p1.fx.tremolo.mode` … `p4.fx.tremolo.mode` | Tremolo Mode | Tremolo-Modus | Tremolo, Autopan | 0 |
| 1069, 1123, 1177, 1231 | `p1.fx.tremolo.shape` … `p4.fx.tremolo.shape` | Tremolo Shape | Tremolo-Form | Sine, Triangle, Square | 0 |
| 1070, 1124, 1178, 1232 | `p1.fx.width.width` … `p4.fx.width.width` | Stereo Width | Stereobreite | 0 … 2 | 1 |
| 1071, 1125, 1179, 1233 | `p1.fx.width.spread` … `p4.fx.width.spread` | Stereo Spread | Stereo-Spreizung | 0 … 1 | 0 |
| 1072, 1126, 1180, 1234 | `p1.fx.width.haas` … `p4.fx.width.haas` | Spread Time | Spreizungszeit | 1 … 25 | 12 |
| 1073, 1127, 1181, 1235 | `p1.fx.width.mono` … `p4.fx.width.mono` | Mono Below | Mono unter | 20 … 500 | 20 |
| 1074, 1128, 1182, 1236 | `p1.fx.width.output` … `p4.fx.width.output` | Width Output | Breite-Ausgang | -24 … 12 | 0 |
| 1237, 1249, 1261, 1273 | `p1.layer.engine` … `p4.layer.engine` | Part N Layer Engine | Part N Layer-Engine | Off, FM, Wavetable, Analog, Pluck, Grain, Vocal, Cluster, Sampler | 0 |
| 1238, 1250, 1262, 1274 | `p1.layer.level` … `p4.layer.level` | Part N Layer Level | Part N Layer-Pegel | 0 … 1 | 0.7 |
| 1239, 1251, 1263, 1275 | `p1.layer.tune` … `p4.layer.tune` | Part N Layer Tune | Part N Layer-Stimmung | -24 … 24 | 0 |
| 1240, 1252, 1264, 1276 | `p1.layer.fine` … `p4.layer.fine` | Part N Layer Fine | Part N Layer-Feinstimmung | -50 … 50 | 0 |
| 1241, 1253, 1265, 1277 | `p1.layer.xmode` … `p4.layer.xmode` | Part N Cross-Mod Mode | Part N Kreuzmod.-Art | Ring, AM, Fold | 0 |
| 1242, 1254, 1266, 1278 | `p1.layer.xmod` … `p4.layer.xmod` | Part N Cross-Mod | Part N Kreuzmodulation | 0 … 1 | 0 |
| 1243, 1255, 1267, 1279 | `p1.layer.width` … `p4.layer.width` | Part N Layer Width | Part N Layer-Breite | 0 … 1 | 0.5 |
| 1244, 1256, 1268, 1280 | `p1.layer.ownenv` … `p4.layer.ownenv` | Part N Layer Own Envelope | Part N Layer eigene Hüllkurve | 0 … 1 | 0 |
| 1245, 1257, 1269, 1281 | `p1.layer.a` … `p4.layer.a` | Part N Layer Attack | Part N Layer Attack | 0.001 … 10 | 0.005 |
| 1246, 1258, 1270, 1282 | `p1.layer.d` … `p4.layer.d` | Part N Layer Decay | Part N Layer Decay | 0.001 … 10 | 0.3 |
| 1247, 1259, 1271, 1283 | `p1.layer.s` … `p4.layer.s` | Part N Layer Sustain | Part N Layer Sustain | 0 … 1 | 0.8 |
| 1248, 1260, 1272, 1284 | `p1.layer.r` … `p4.layer.r` | Part N Layer Release | Part N Layer Release | 0.001 … 10 | 0.3 |
| 1285, 1294, 1303, 1312 | `p1.wavetable.univoices` … `p4.wavetable.univoices` | Unison Voices | Unisono-Stimmen | 1 … 8 | 3 |
| 1286, 1295, 1304, 1313 | `p1.wavetable.uniwidth` … `p4.wavetable.uniwidth` | Unison Width | Unisono-Breite | 0 … 1 | 0.7 |
| 1287, 1296, 1305, 1314 | `p1.wavetable.fm` … `p4.wavetable.fm` | FM Amount | FM-Menge | 0 … 1 | 0 |
| 1288, 1297, 1306, 1315 | `p1.wavetable.fmratio` … `p4.wavetable.fmratio` | FM Ratio | FM-Verhältnis | 0.25 … 8 | 1 |
| 1289, 1298, 1307, 1316 | `p1.wavetable.lfopos` … `p4.wavetable.lfopos` | LFO > Position | LFO > Position | -1 … 1 | 0 |
| 1290, 1299, 1308, 1317 | `p1.wavetable.envfm` … `p4.wavetable.envfm` | Env > FM | Hüllk. > FM | -1 … 1 | 0 |
| 1291, 1300, 1309, 1318 | `p1.fm.velidx` … `p4.fm.velidx` | Velocity > Index | Anschlag > Index | 0 … 1 | 0 |
| 1292, 1301, 1310, 1319 | `p1.sampler.inst2` … `p4.sampler.inst2` | Keyboard Instrument | Tasteninstrument | Off, Grand Piano, Upright Piano, Tine Piano, Reed Piano | 0 |
| 1293, 1302, 1311, 1320 | `p1.grain.inst2` … `p4.grain.inst2` | Keyboard Instrument | Tasteninstrument | Off, Grand Piano, Upright Piano, Tine Piano, Reed Piano | 0 |
| 1321, 1325, 1329, 1333 | `p1.mpe.pressamt` … `p4.mpe.pressamt` | Part N Pressure Amount | Part N Druck-Menge | -1 … 1 | 1 |
| 1322, 1326, 1330, 1334 | `p1.mpe.timbreamt` … `p4.mpe.timbreamt` | Part N Timbre Amount | Part N Timbre-Menge | -1 … 1 | 1 |
| 1323, 1327, 1331, 1335 | `p1.mpe.smooth` … `p4.mpe.smooth` | Part N Expression Smoothing | Part N Ausdrucks-Glättung | 0 … 0.5 s | 0 |
| 1324, 1328, 1332, 1336 | `p1.mpe.slide` … `p4.mpe.slide` | Part N Per-Note Slide | Part N Pro-Noten-Slide | 0 … 1 s | 0 |
| 1337, 1338, 1339, 1340 | `p1.monobass` … `p4.monobass` | Part N Bass Mono Below | Part N Bass mono unter | 20 … 400 Hz (20 = off; the side signal below it is removed after the FX slot) | 20 |

Behaviour changes in 0.8: `pN.fx.type` gains the types Shaper … Width after Reverb (existing values keep
their meaning). The MPE pressure / timbre targets (`pN.mpe.press`, `pN.mpe.timbre`) gain Cutoff, Position, FM Index,
Grain Density, Drive, Vibrato, Formant and Layer Blend after Pan. Per-note pitch bend glides over
`pN.mpe.slide` seconds (at least 2 ms); the other per-note expressions glide over `pN.mpe.smooth` seconds.
