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

## 0.7 – FX rack and modulation matrix

Appended after `mix.master.lowpass`. Guides: FX rack [English](en/TAPE-MIXER-FX.md) ·
[Deutsch](de/BAND-MISCHER-FX.md), modulation matrix [English](en/MODULATION.md) · [Deutsch](de/MODULATION.md).
Order of the block (registry indices; 1151 parameters, 946–2096):

| Range | Block |
|---|---|
| 946–1082 | part 1 rack: slots 2–6, then mix / bypass of slots 1–6 |
| 1083–1244, 1245–1406, 1407–1568 | part 2, 3, 4 rack: slot 1 (`pN.fx.*`), slots 2–6, mix / bypass |
| 1569–1676 | master rack `mfx1` … `mfx4` |
| 1677–1781, 1782–1886, 1887–1991, 1992–2096 | modulation matrix of part 1, 2, 3, 4 (105 each) |


1. **Part racks** – for part 1, then 2, 3, 4: slot 1 (`pN.fx.type` + the 0.2 type blocks; parts 2–4 only, part 1
   has it since 0.2), slots 2–6 (`pN.fx2.type` … `pN.fx6.*`, each the type + the 0.2 type blocks in the order of
   the 0.2 slot: chorus, phaser, bitcrush, filter drive, delay (+ `mix`), reverb (+ `mix`)), then `mix` and `bypass`
   of slots 1–6 (`pN.fx.mix`, `pN.fx.bypass`, `pN.fx2.mix` …).
2. **Master rack** – `mfx1.*` … `mfx4.*` (type + 0.2 blocks each), then `mfx1.mix`, `mfx1.bypass` … `mfx4.bypass`.
3. **Modulation matrix** – for part 1, then 2, 3, 4: routings 1–16 (`pN.mod.sK.src`, `.via`, `.dst`, `.amt`,
   `.curve`), LFO 2 and LFO 3 (`rate`, `sync`, `shape`, `phase`, `fade`, `mode`, `depth`), envelope 3
   (`pN.env3.delay`, `.a`, `.hold`, `.d`, `.s`, `.r`), `pN.macro1` … `pN.macro4`, `pN.seqlane`.

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `pN.fxK.type` (K = 2…6), `mfxK.type` | P*N* FX*K* FX Slot / Master FX *K* FX Slot | P*N* FX*K* FX-Slot / Master-FX *K* FX-Slot | as `p1.fx.type` | Off |
| `pN.fxK.mix`, `mfxK.mix` | Part *N* FX *K* Mix / Master FX *K* Mix | … Mischung | 0 … 100 % (dry / wet of the slot) | 100 % |
| `pN.fxK.bypass`, `mfxK.bypass` | … Bypass | … Bypass | off / on | off |
| `pN.mod.sK.src` | Part *N* Mod *K* Source | Part *N* Mod *K* Quelle | Off, LFO 1, LFO 2, LFO 3, Amp Env, Mod Env, Env 3, Velocity, Key Track, Mod Wheel, Pitch Bend, Channel Pressure, Note Pressure, Slide (Timbre), Note Bend, Random, Macro 1–4, Seq Lane | Off |
| `pN.mod.sK.via` | … Via | … Via | as the source | Off |
| `pN.mod.sK.dst` | … Target | … Ziel | Off, Pitch, Level, Pan, Timbre, Engine 1–24, Volume, LFO 1–3 Rate / Depth, Mod Env Amount, FX 1–6 Mix, FX 1–6 Param 1–4, Azimuth, Elevation, Distance, Motion Path Rate, Motion Path Phase, Life (p15) | Off |
| `pN.mod.sK.amt` | … Amount | … Menge | −100 … +100 % | 0 |
| `pN.mod.sK.curve` | … Curve | … Kurve | Linear, Exponential, Logarithmic, S-Curve, Steps | Linear |
| `pN.lfo2.rate` | Part *N* LFO 2 Rate | Part *N* LFO 2 Tempo | 0.01 … 40 Hz | 1 Hz |
| `pN.lfo2.sync` | … LFO 2 Sync | … LFO 2 Sync | Free, 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/2., 1/4., 1/8., 1/16., 1/2T, 1/4T, 1/8T, 1/16T | Free |
| `pN.lfo2.shape` | … LFO 2 Shape | … LFO 2 Form | Sine, Triangle, Saw Up, Saw Down, Square, S&H, Smooth Random | Sine |
| `pN.lfo2.phase` | … LFO 2 Phase | … LFO 2 Phase | 0 … 1 | 0 |
| `pN.lfo2.fade` | … LFO 2 Fade In | … LFO 2 Einblenden | 0 … 10 s | 0 s |
| `pN.lfo2.mode` | … LFO 2 Mode | … LFO 2 Modus | Free-Running, Retrigger, One-Shot | Retrigger |
| `pN.lfo2.depth` | … LFO 2 Depth | … LFO 2 Tiefe | 0 … 1 | 1 |
| `pN.lfo3.*` | LFO 3 – as LFO 2 | | | |
| `pN.env3.delay` / `.hold` | Env 3 Delay / Hold | Hüllkurve 3 Verzögerung / Halten | 0 … 4 s | 0 s |
| `pN.env3.a` | Env 3 Attack | Hüllkurve 3 Attack | 1 ms … 10 s | 0.5 s |
| `pN.env3.d` / `.r` | Env 3 Decay / Release | Hüllkurve 3 Decay / Release | 1 ms … 10 s | 1 s |
| `pN.env3.s` | Env 3 Sustain | Hüllkurve 3 Sustain | 0 … 1 | 0.5 |
| `pN.macro1` … `pN.macro4` | Part *N* Macro 1–4 | Part *N* Makro 1–4 | 0 … 1 | 0 |
| `pN.seqlane` | Part *N* Seq Lane | Part *N* Seq-Spur | 0 … 1 | 0 |

Behaviour changes in 0.7: the 0.2 FX slot is slot 1 of the part's rack (identical sound); parts 2–4 have FX slots
(before 0.7 their slot parameters did not exist). The MPE / aftertouch target "Timbre" now also brightens the
Cluster, Grain, Pluck, Sampler, Vocal and Wavetable engines (before: Analog and FM only). A change of the part
volume glides over one block instead of jumping.

## 0.7 – rack effect types / Effekttypen des Racks

Appended after `p4.seqlane`. Every slot of every rack (slot 1 – 6 of each part, `mfx1` … `mfx4`) has its own block
for each of the nine rack types, 75 parameters per slot, in this order: hyper, distortion, flanger, compressor,
multiband, EQ, filter, echo, hall. The block of a slot is `<slot prefix>.<type>.<parameter>`, for example
`p2.fx4.echo.time` or `mfx1.hall.decay`. Guides: [English](en/TAPE-MIXER-FX.md#rack-effect-types) ·
[Deutsch](de/BAND-MISCHER-FX.md#effekttypen-des-racks).

| Range | Block |
|---|---|
| 2097–2546 | part 1: `p1.fx.*`, `p1.fx2.*` … `p1.fx6.*` (450 = 6 × 75) |
| 2547–2996, 2997–3446, 3447–3896 | part 2, 3, 4 – the same |
| 3897–4196 | master rack: `mfx1.*` … `mfx4.*` (300 = 4 × 75) |

The table shows the block with the prefix `p1.fx2`; host names carry the slot ("P1 FX2 Echo Time",
"Master FX 1 Hall Decay").

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `p1.fx2.hyper.rate` | Hyper Rate | Hyper-Tempo | 0.05 … 5 Hz | 0.6 Hz |
| `p1.fx2.hyper.detune` | Hyper Detune | Hyper-Verstimmung | 0 … 1 | 0.35 |
| `p1.fx2.hyper.voices` | Hyper Voices | Hyper-Stimmen | 2 … 7 | 5 |
| `p1.fx2.hyper.spread` | Hyper Spread | Hyper-Breite | 0 … 1 | 0.8 |
| `p1.fx2.hyper.dimension` | Dimension | Dimension | 0 … 1 | 0.4 |
| `p1.fx2.hyper.retrig` | Hyper Retrigger | Hyper-Neustart | off / on | off |
| `p1.fx2.hyper.mix` | Hyper Mix | Hyper-Mischung | 0 … 1 | 0.5 |
| `p1.fx2.distortion.mode` | Distortion Mode | Verzerrungs-Modus | Tube, Soft Clip, Hard Clip, Diode, Linear Fold, Sine Fold, Asymmetric, Downsample | Tube |
| `p1.fx2.distortion.drive` | Distortion Drive | Verzerrung Drive | 0 … 1 | 0.4 |
| `p1.fx2.distortion.bias` | Distortion Bias | Verzerrung Bias | −1 … 1 | 0 |
| `p1.fx2.distortion.prefilter` | Distortion Pre-Filter | Verzerrung Vorfilter | Off, Low-pass, Band-pass, High-pass | Off |
| `p1.fx2.distortion.prefreq` | Distortion Pre-Filter Freq | Verzerrung Vorfilter-Frequenz | 40 … 18000 Hz | 800 Hz |
| `p1.fx2.distortion.tone` | Distortion Tone | Verzerrung Klang | 0 … 1 | 0.8 |
| `p1.fx2.distortion.output` | Distortion Output | Verzerrung Ausgang | −24 … +12 dB | 0 dB |
| `p1.fx2.distortion.mix` | Distortion Mix | Verzerrung Mischung | 0 … 1 | 1 |
| `p1.fx2.flange.rate` | Flanger Rate | Flanger-Tempo | 0.02 … 8 Hz | 0.25 Hz |
| `p1.fx2.flange.sync` | Flanger Sync | Flanger-Sync | Free, 1/1 … 1/32, dotted, triplets | Free |
| `p1.fx2.flange.depth` | Flanger Depth | Flanger-Tiefe | 0 … 1 | 0.7 |
| `p1.fx2.flange.delay` | Flanger Delay | Flanger-Verzögerung | 0.3 … 12 ms | 2 ms |
| `p1.fx2.flange.feedback` | Flanger Feedback | Flanger-Feedback | −0.95 … 0.95 | 0.5 |
| `p1.fx2.flange.stereo` | Flanger Stereo | Flanger-Stereo | 0 … 0.5 (LFO phase offset) | 0.25 |
| `p1.fx2.flange.mix` | Flanger Mix | Flanger-Mischung | 0 … 1 | 0.5 |
| `p1.fx2.compressor.thresh` | Compressor Threshold | Kompressor Schwelle | −60 … 0 dB | −18 dB |
| `p1.fx2.compressor.ratio` | Compressor Ratio | Kompressor Ratio | 1 … 20 | 4 |
| `p1.fx2.compressor.attack` | Compressor Attack | Kompressor Attack | 0.1 … 200 ms | 10 ms |
| `p1.fx2.compressor.release` | Compressor Release | Kompressor Release | 5 … 2000 ms | 120 ms |
| `p1.fx2.compressor.knee` | Compressor Knee | Kompressor Knie | 0 … 24 dB | 6 dB |
| `p1.fx2.compressor.makeup` | Compressor Makeup | Kompressor Aufholverstärkung | 0 … 24 dB | 0 dB |
| `p1.fx2.compressor.mix` | Compressor Mix | Kompressor Mischung | 0 … 1 | 1 |
| `p1.fx2.multiband.depth` | Multiband Depth | Multiband Tiefe | 0 … 1 | 0.5 |
| `p1.fx2.multiband.upward` / `.downward` | Multiband Upward / Downward | Multiband aufwärts / abwärts | 0 … 1 | 1 |
| `p1.fx2.multiband.time` | Multiband Time | Multiband Zeit | 0 … 1 | 0.5 |
| `p1.fx2.multiband.low` / `.mid` / `.high` | Multiband Low / Mid / High | Multiband Tiefen / Mitten / Höhen | −12 … +12 dB | 0 dB |
| `p1.fx2.multiband.lowsplit` | Multiband Low Split | Multiband Trennung tief | 40 … 500 Hz | 120 Hz |
| `p1.fx2.multiband.highsplit` | Multiband High Split | Multiband Trennung hoch | 800 … 8000 Hz | 2500 Hz |
| `p1.fx2.multiband.output` | Multiband Output | Multiband Ausgang | −12 … +12 dB | 0 dB |
| `p1.fx2.multiband.mix` | Multiband Mix | Multiband Mischung | 0 … 1 | 1 |
| `p1.fx2.eq.lowfreq` / `.lowgain` | EQ Low Shelf Freq / Gain | EQ Tiefen-Kuhschwanz Frequenz / Gain | 20 … 1000 Hz / ±18 dB | 100 Hz / 0 dB |
| `p1.fx2.eq.p1freq` / `.p1gain` / `.p1q` | EQ Band 1 Freq / Gain / Q | EQ Band 1 Frequenz / Gain / Güte | 30 … 18000 Hz / ±18 dB / 0.3 … 12 | 300 Hz / 0 dB / 1 |
| `p1.fx2.eq.p2freq` / `.p2gain` / `.p2q` | EQ Band 2 … | EQ Band 2 … | as band 1 | 1200 Hz / 0 dB / 1 |
| `p1.fx2.eq.p3freq` / `.p3gain` / `.p3q` | EQ Band 3 … | EQ Band 3 … | as band 1 | 4500 Hz / 0 dB / 1 |
| `p1.fx2.eq.highfreq` / `.highgain` | EQ High Shelf Freq / Gain | EQ Höhen-Kuhschwanz Frequenz / Gain | 1000 … 20000 Hz / ±18 dB | 8000 Hz / 0 dB |
| `p1.fx2.eq.output` | EQ Output | EQ Ausgang | −18 … +18 dB | 0 dB |
| `p1.fx2.filter.type` | Filter Type | Filtertyp | Low-pass 12, Low-pass 24, High-pass 12, High-pass 24, Band-pass, Notch, Comb +, Comb −, Formant | Low-pass 24 |
| `p1.fx2.filter.cutoff` | Filter Cutoff | Filter-Cutoff | 20 … 20000 Hz | 2000 Hz |
| `p1.fx2.filter.reso` | Filter Resonance | Filter-Resonanz | 0 … 1 | 0.3 |
| `p1.fx2.filter.drive` | Filter Drive | Filter-Drive | 0 … 1 | 0 |
| `p1.fx2.filter.mix` | Filter Mix | Filter-Mischung | 0 … 1 | 1 |
| `p1.fx2.echo.mode` | Echo Mode | Echo-Modus | Stereo, Ping-Pong, Tap | Ping-Pong |
| `p1.fx2.echo.sync` | Echo Sync | Echo-Sync | Free, 1/1 … 1/32, dotted, triplets | 1/4. |
| `p1.fx2.echo.time` | Echo Time | Echo-Zeit | 1 … 2000 ms (Free) | 375 ms |
| `p1.fx2.echo.ratio` | Echo Right Ratio | Echo Verhältnis rechts | 0.25 … 1 (Tap) | 0.75 |
| `p1.fx2.echo.feedback` | Echo Feedback | Echo-Feedback | 0 … 1.05 | 0.4 |
| `p1.fx2.echo.lowcut` / `.highcut` | Echo Low Cut / High Cut | Echo Tiefensperre / Höhensperre | 20 … 2000 Hz / 1000 … 20000 Hz | 120 Hz / 8000 Hz |
| `p1.fx2.echo.width` | Echo Width | Echo-Breite | 0 … 1 | 1 |
| `p1.fx2.echo.mix` | Echo Mix | Echo-Mischung | 0 … 1 | 0.3 |
| `p1.fx2.hall.mode` | Hall Mode | Raum-Modus | Plate, Hall | Hall |
| `p1.fx2.hall.size` | Hall Size | Raumgröße | 0 … 1 | 0.6 |
| `p1.fx2.hall.decay` | Hall Decay | Raum Nachhall | 0.2 … 20 s | 2.5 s |
| `p1.fx2.hall.damping` | Hall Damping | Raum Dämpfung | 0 … 1 | 0.4 |
| `p1.fx2.hall.predelay` | Hall Pre-Delay | Raum Pre-Delay | 0 … 250 ms | 12 ms |
| `p1.fx2.hall.width` | Hall Width | Raum-Breite | 0 … 1 | 1 |
| `p1.fx2.hall.mix` | Hall Mix | Raum-Mischung | 0 … 1 | 0.3 |

The modulation targets *FX K Param 1–4* move the first four parameters of the slot's selected type in this table's
order (for the echo: mode, sync, time, ratio).

## 0.7 – Mid/Side stereo effect

Appended after the blocks of the nine 0.7 rack effect types (Hyper … Hall, last ID `mfx4.hall.mix`). Guide: [English](en/TAPE-MIXER-FX.md#midside-stereo-07) ·
[Deutsch](de/BAND-MISCHER-FX.md#mitteseite-stereo-07). The FX type list gains **Mid/Side** (DE: Mitte/Seite) at index 16
(append-only list, after Hall at 15) in every rack slot. Its block of 10 parameters is registered for every slot, in rack order: part 1
slots 1–6 (`p1.fx.midside.*`, `p1.fx2.midside.*` … `p1.fx6.midside.*`), then parts 2, 3, 4, then the master slots
`mfx1.midside.*` … `mfx4.midside.*` – 28 slots × 10 = 280 parameters. Host names carry the slot prefix like the other
rack blocks ("P2 FX3 Stereo Width", "Master FX 1 Stereo Width").

| Range | Block |
|---|---|
| 4197–4256 | part 1: `p1.fx.midside.*`, `p1.fx2.midside.*` … `p1.fx6.midside.*` (60 = 6 × 10) |
| 4257–4316, 4317–4376, 4377–4436 | part 2, 3, 4 – the same |
| 4437–4476 | master rack: `mfx1.midside.*` … `mfx4.midside.*` (40 = 4 × 10) |

| ID (`<slot>.midside.*`) | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `width` | Stereo Width | Stereobreite | 0 … 200 % (0 = mono) | 100 % |
| `mid` | Mid Level | Mitte-Pegel | −24 … +12 dB | 0 dB |
| `side` | Side Level | Seite-Pegel | −24 … +12 dB | 0 dB |
| `balance` | Balance | Balance | −100 … +100 % | 0 |
| `bassmono` | Bass Mono | Bass mono | Off, On | Off |
| `monofreq` | Mono Below | Mono unter | 40 … 500 Hz | 120 Hz |
| `haas` | Micro-Shift | Mikroversatz | 0 … 100 % | 0 |
| `haastime` | Shift Time | Versatzzeit | 1 … 30 ms | 12 ms |
| `midtilt` | Mid Tilt | Mitte-Tilt | −6 … +6 dB | 0 dB |
| `sidetilt` | Side Tilt | Seite-Tilt | −6 … +6 dB | 0 dB |

All defaults together are an exact (bit-identical) bypass. The modulation targets *FX k Param 1–4* of a Mid/Side slot
are `width`, `mid`, `side`, `balance`.

## 0.8 – sound revision 3: FX types, layer engine, keyboard instruments, MPE, bass mono

Appended after the 0.7 block (index 4477 onwards, 1056 parameters). Guides: FX types
[English](en/TAPE-MIXER-FX.md) · [Deutsch](de/BAND-MISCHER-FX.md), layer engine
[English](en/engines/layer.md) · [Deutsch](de/engines/layer.md), wavetable unison / FM
[English](en/engines/wavetable.md) · [Deutsch](de/engines/wavetable.md), keyboard instruments
[English](en/GRAIN-VOCAL-SAMPLER-DRUM.md) · [Deutsch](de/GRAIN-VOKAL-SAMPLER-DRUM.md), MPE
[English](en/MIDI.md) · [Deutsch](de/MIDI.md). Order of the block:

1. **FX types Comb, Formant, Wobble, Tremolo, Width** (`type` 16 – 20) – their parameter blocks for every
   slot of every rack: part 1 slots 1 – 6, parts 2, 3, 4, then the master slots (`FxRack::registerTypeBlocks`).
2. **Layer engine** of parts 1 – 4 (`pN.layer.*`).
3. **Late engine parameters**, per part: Wavetable (unison stack, FM, LFO > position, Env > FM), FM
   (velocity > index), Sampler and Grain (keyboard instrument).
4. **MPE / aftertouch** of parts 1 – 4: target amounts, expression smoothing, per-note slide.
5. **Bass mono** of parts 1 – 4 (`pN.monobass`).

`ohpe_param_dump --markdown` prints every row with its own index. N = part 1 – 4, K = slot (`fx.` = slot 1,
`fx2.` … `fx6.`), master slots `mfx1.` … `mfx4.`.

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `pN.fxK.comb.freq`, `mfxK.comb.freq` | Comb Frequency | Kamm-Frequenz | 20 … 5000 | 300 |
| `pN.fxK.comb.feedback`, `mfxK.comb.feedback` | Comb Feedback | Kamm-Feedback | -0.95 … 0.95 | 0.6 |
| `pN.fxK.comb.damp`, `mfxK.comb.damp` | Comb Damping | Kamm-Dämpfung | 0 … 1 | 0.3 |
| `pN.fxK.comb.rate`, `mfxK.comb.rate` | Comb Rate | Kamm-Tempo | 0.01 … 10 | 0.2 |
| `pN.fxK.comb.depth`, `mfxK.comb.depth` | Comb Depth | Kamm-Tiefe | 0 … 1 | 0.3 |
| `pN.fxK.comb.spread`, `mfxK.comb.spread` | Comb Spread | Kamm-Breite | 0 … 1 | 0.3 |
| `pN.fxK.comb.mix`, `mfxK.comb.mix` | Comb Mix | Kamm-Mischung | 0 … 1 | 0.5 |
| `pN.fxK.formant.vowel`, `mfxK.formant.vowel` | Vowel | Vokal | 0 … 1 | 0 |
| `pN.fxK.formant.shift`, `mfxK.formant.shift` | Formant Shift | Formant-Verschiebung | -12 … 12 | 0 |
| `pN.fxK.formant.reso`, `mfxK.formant.reso` | Formant Resonance | Formant-Resonanz | 0 … 1 | 0.5 |
| `pN.fxK.formant.rate`, `mfxK.formant.rate` | Formant Rate | Formant-Tempo | 0.01 … 20 | 1 |
| `pN.fxK.formant.sync`, `mfxK.formant.sync` | Formant Sync | Formant-Sync | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/2., 1/4., 1/8., 1/16., 1/2T, 1/4T, 1/8T, 1/16T, 2/1, 4/1 | 0 |
| `pN.fxK.formant.depth`, `mfxK.formant.depth` | Formant LFO Depth | Formant-LFO-Tiefe | 0 … 1 | 0 |
| `pN.fxK.formant.mix`, `mfxK.formant.mix` | Formant Mix | Formant-Mischung | 0 … 1 | 1 |
| `pN.fxK.wobble.rate`, `mfxK.wobble.rate` | Wobble Rate | Wobble-Tempo | 0.05 … 20 | 2 |
| `pN.fxK.wobble.sync`, `mfxK.wobble.sync` | Wobble Sync | Wobble-Sync | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/2., 1/4., 1/8., 1/16., 1/2T, 1/4T, 1/8T, 1/16T, 2/1, 4/1 | 4 |
| `pN.fxK.wobble.shape`, `mfxK.wobble.shape` | Wobble Shape | Wobble-Form | Sine, Triangle, Saw Down, Saw Up, Square, S&H | 0 |
| `pN.fxK.wobble.low`, `mfxK.wobble.low` | Wobble Low | Wobble tief | 30 … 8000 | 150 |
| `pN.fxK.wobble.high`, `mfxK.wobble.high` | Wobble High | Wobble hoch | 100 … 18000 | 3000 |
| `pN.fxK.wobble.reso`, `mfxK.wobble.reso` | Wobble Resonance | Wobble-Resonanz | 0 … 1 | 0.5 |
| `pN.fxK.wobble.mode`, `mfxK.wobble.mode` | Wobble Filter | Wobble-Filter | Low-pass, Band-pass | 0 |
| `pN.fxK.wobble.drive`, `mfxK.wobble.drive` | Wobble Drive | Wobble-Drive | 0 … 1 | 0.2 |
| `pN.fxK.wobble.retrig`, `mfxK.wobble.retrig` | Wobble Retrigger | Wobble-Neustart | 0 … 1 | 1 |
| `pN.fxK.wobble.mix`, `mfxK.wobble.mix` | Wobble Mix | Wobble-Mischung | 0 … 1 | 1 |
| `pN.fxK.tremolo.rate`, `mfxK.tremolo.rate` | Tremolo Rate | Tremolo-Tempo | 0.05 … 20 | 4 |
| `pN.fxK.tremolo.sync`, `mfxK.tremolo.sync` | Tremolo Sync | Tremolo-Sync | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/2., 1/4., 1/8., 1/16., 1/2T, 1/4T, 1/8T, 1/16T, 2/1, 4/1 | 0 |
| `pN.fxK.tremolo.depth`, `mfxK.tremolo.depth` | Tremolo Depth | Tremolo-Tiefe | 0 … 1 | 0.5 |
| `pN.fxK.tremolo.mode`, `mfxK.tremolo.mode` | Tremolo Mode | Tremolo-Modus | Tremolo, Autopan | 0 |
| `pN.fxK.tremolo.shape`, `mfxK.tremolo.shape` | Tremolo Shape | Tremolo-Form | Sine, Triangle, Square | 0 |
| `pN.fxK.width.width`, `mfxK.width.width` | Stereo Width | Stereobreite | 0 … 2 | 1 |
| `pN.fxK.width.spread`, `mfxK.width.spread` | Stereo Spread | Stereo-Spreizung | 0 … 1 | 0 |
| `pN.fxK.width.haas`, `mfxK.width.haas` | Spread Time | Spreizungszeit | 1 … 25 | 12 |
| `pN.fxK.width.mono`, `mfxK.width.mono` | Mono Below | Mono unter | 20 … 500 | 20 |
| `pN.fxK.width.output`, `mfxK.width.output` | Width Output | Breite-Ausgang | -24 … 12 | 0 |
| `pN.layer.engine` | Layer Engine | Layer-Engine | Off, FM, Wavetable, Analog, Pluck, Grain, Vocal, Cluster, Sampler | 0 |
| `pN.layer.level` | Layer Level | Layer-Pegel | 0 … 1 | 0.7 |
| `pN.layer.tune` | Layer Tune | Layer-Stimmung | -24 … 24 | 0 |
| `pN.layer.fine` | Layer Fine | Layer-Feinstimmung | -50 … 50 | 0 |
| `pN.layer.xmode` | Cross-Mod Mode | Kreuzmod.-Art | Ring, AM, Fold | 0 |
| `pN.layer.xmod` | Cross-Mod | Kreuzmodulation | 0 … 1 | 0 |
| `pN.layer.width` | Layer Width | Layer-Breite | 0 … 1 | 0.5 |
| `pN.layer.ownenv` | Layer Own Envelope | Layer eigene Hüllkurve | 0 … 1 | 0 |
| `pN.layer.a` | Layer Attack | Layer Attack | 0.001 … 10 | 0.005 |
| `pN.layer.d` | Layer Decay | Layer Decay | 0.001 … 10 | 0.3 |
| `pN.layer.s` | Layer Sustain | Layer Sustain | 0 … 1 | 0.8 |
| `pN.layer.r` | Layer Release | Layer Release | 0.001 … 10 | 0.3 |
| `pN.wavetable.univoices` | Unison Voices | Unisono-Stimmen | 1 … 8 | 3 |
| `pN.wavetable.uniwidth` | Unison Width | Unisono-Breite | 0 … 1 | 0.7 |
| `pN.wavetable.fm` | FM Amount | FM-Menge | 0 … 1 | 0 |
| `pN.wavetable.fmratio` | FM Ratio | FM-Verhältnis | 0.25 … 8 | 1 |
| `pN.wavetable.lfopos` | LFO > Position | LFO > Position | -1 … 1 | 0 |
| `pN.wavetable.envfm` | Env > FM | Hüllk. > FM | -1 … 1 | 0 |
| `pN.fm.velidx` | Velocity > Index | Anschlag > Index | 0 … 1 | 0 |
| `pN.sampler.inst2` | Keyboard Instrument | Tasteninstrument | Off, Grand Piano, Upright Piano, Tine Piano, Reed Piano | 0 |
| `pN.grain.inst2` | Keyboard Instrument | Tasteninstrument | Off, Grand Piano, Upright Piano, Tine Piano, Reed Piano | 0 |
| `pN.mpe.pressamt` | Pressure Amount | Druck-Menge | -1 … 1 | 1 |
| `pN.mpe.timbreamt` | Timbre Amount | Timbre-Menge | -1 … 1 | 1 |
| `pN.mpe.smooth` | Expression Smoothing | Ausdrucks-Glättung | 0 … 0.5 | 0 |
| `pN.mpe.slide` | Per-Note Slide | Pro-Noten-Slide | 0 … 1 | 0 |
| `pN.monobass` | Bass Mono Below | Bass mono unter | 20 … 400 | 20 |

Behaviour changes in 0.8: `pN.fxK.type` / `mfxK.type` gain Comb, Formant, Wobble, Tremolo and Width after
Hall (existing values keep their meaning). The MPE pressure / timbre targets (`pN.mpe.press`, `pN.mpe.timbre`)
gain Cutoff, Position, FM Index, Grain Density, Drive, Vibrato, Formant and Layer Blend after Pan. Per-note pitch
bend glides over `pN.mpe.slide` seconds (at least 2 ms); the other per-note expressions glide over
`pN.mpe.smooth` seconds.

## Tape transport (p11)

Appended after `p4.monobass`. MIDI-mappable transport keys (triggers: fire when the value rises to
"on", CC ≥ 64) and the *Play starts tape* setting; none of them is stored in presets. Guides:
[English](en/TAPE-MIXER-FX.md#transport) · [Deutsch](de/BAND-MISCHER-FX.md#transport).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `tape.tx.play` | Transport Play/Stop | Transport Start/Stopp | trigger | off |
| `tape.tx.tplay` | Tape Play/Stop | Tape Start/Stopp | trigger | off |
| `tape.tx.stop` | Transport Stop | Transport Stopp | trigger | off |
| `tape.tx.rec` | Transport Record | Transport Aufnahme | trigger | off |
| `tape.tx.loop` | Transport Loop | Transport Schleife | trigger | off |
| `tape.tx.arm` | Arm Edit Track | Bearbeitungsspur aufnahmebereit | trigger | off |
| `tape.tx.link` | Play Starts Tape | Start startet Tape | on / off | on |

## 1.0 – 3D spatialiser

Appended after `tape.tx.link`. Guide: [English](en/SPACE.md) · [Deutsch](de/RAUM.md). 518 parameters:

1. **Per-part stage** `pN.space.*` for parts 1 – 4 (17 each, group `partN/space`; host names "Part N 3D …"):
   `pN.space.on` and the Space 3D block below.
2. **Monitoring** (group `master`; not stored in project presets):

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `space.out` | 3D Monitoring | 3D-Abhöre | Headphones, Speakers | 0 |
| `space.monocheck` | Mono Check | Mono-Prüfung | 0 … 1 | 0 |

3. **Rack type Space 3D** (DE 3D-Raum, `pN.fxK.type` / `mfxK.type` value 22, appended after Width): its block
   `<slot>space3d.*` for every slot of every rack, in the order parts 1 – 4 slots 1 – 6, then master slots 1 – 4
   (28 × 16). FX Param 1 – 4 (matrix) = Azimuth, Elevation, Distance, Motion Rate.

### The Space 3D block (`pN.space.*`, `<slot>space3d.*`)

| Local ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `on` (stage only) | Space | Raum | 0 … 1 | 0 |
| `azimuth` | Azimuth | Azimut | -180 … 180 ° | 0 |
| `elevation` | Elevation | Elevation | -60 … 90 ° | 0 |
| `distance` | Distance | Entfernung | 0.2 … 40 m (skew 0.3) | 1.5 |
| `pathrate` | Motion Rate | Bewegungstempo | -4 … 4 | 1 |
| `pathphase` | Motion Phase | Bewegungsphase | 0 … 360 ° | 0 |
| `path` | Motion Path | Bewegungspfad | Off, Circle, Figure-8, Fly-By, Random Orbit, Pendulum, Spiral Up | 0 |
| `pathlen` | Motion Length | Bewegungslänge | 1/16, 1/8, 1/4, 1/2, 1 Bar, 2 Bars, 4 Bars, 8 Bars, 16 Bars, 32 Bars, 1/4. | 5 (2 Bars) |
| `pathsize` | Motion Size | Bewegungsgröße | 0 … 1 | 0.5 |
| `spread` | Source Spread | Quellenbreite | 0 … 180 ° | 40 |
| `room` | Room | Raumanteil | 0 … 1 | 0.25 |
| `roomsize` | Room Size | Raumgröße | 0 … 1 | 0.4 |
| `doppler` | Doppler | Doppler | 0 … 1 | 0.5 |
| `air` | Air Absorption | Luftdämpfung | 0 … 1 | 0.6 |
| `rolloff` | Distance Rolloff | Entfernungsabfall | 0 … 1 | 0.7 |
| `lowkeep` | Mono Below | Mono unter | 40 … 300 Hz (skew 0.5) | 120 |
| `glide` | Motion Glide | Bewegungsglättung | 0.005 … 2 s (skew 0.3) | 0.06 |

Behaviour changes in 1.0: the matrix targets 67 – 71 (Azimuth, Elevation, Distance, Motion Path Rate / Phase),
reserved in 0.7, now move `pN.space.azimuth` … `pN.space.pathphase`. The preset category list gains **3D**.
New parameter units: degrees (°) and metres (m).

## 0.8 punch pass – sidechain ducking and chorus ensemble

Appended after the 3D block (`mfx4.space3d.glide`). Guides: [English](en/TAPE-MIXER-FX.md) · [Deutsch](de/BAND-MISCHER-FX.md).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `mix.duck.src` | Ducking Key | Ducking-Auslöser | Off, Ch 1 … Ch 8 | Off |
| `mix.duck.release` | Ducking Release | Ducking-Release | 0.03 … 1 s | 0.16 |
| `mix.chN.duck` (N = 1 … 8) | Ch N Ducking | Kanal N Ducking | 0 … 1 | 0 |
| `pN.fxK.chorus.voices`, `mfxK.chorus.voices` | Chorus Voices | Chorus-Stimmen | 1 … 4 | 1 |
| `pN.fxK.chorus.drift`, `mfxK.chorus.drift` | Chorus Drift | Chorus-Drift | 0 … 1 | 0 |
| `pN.fxK.chorus.spread`, `mfxK.chorus.spread` | Chorus Spread | Chorus-Breite | 0 … 1 | 0.8 |
| `pN.fxK.chorus.lowcut`, `mfxK.chorus.lowcut` | Chorus Low Cut | Chorus-Tiefensperre | 20 … 1000 Hz | 20 |

The chorus IDs exist for every slot of every rack (`p1.fx.` … `p4.fx6.`, `mfx1.` … `mfx4.`), registered in that
order after the ducking parameters. Their defaults give the 0.2 chorus.

Behaviour changes in the punch pass: the master limiter looks ahead 1.5 ms (latency 83 samples at 48 kHz - the look-ahead plus a 24-tap true-peak interpolator -, constant -
also with `mix.master.limit` off and on the aux outputs - and reported to the host) and limits true peaks; the compressors (master, compressor-lite, rack Compressor) hold
a detected peak for 12 ms; the Hyper effect restarts its voices on a new note only with *Retrigger* on.
## Performance wheels (p14)

Appended after the 0.8 punch pass IDs (last ID `mfx4.chorus.lowcut`), as the last two IDs. The PITCH and MOD wheels left of the on-screen keyboard. A change is sent as
pitch bend / mod wheel (CC 1) on the edit part's MIDI channel, the same way the on-screen keyboard plays; the
range of the bend is each part's `pN.bend`. The pitch wheel springs back to the centre when released on the
screen. Both are MIDI-learnable (right-click) and are not stored in presets.

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `perf.bend` | Pitch Wheel | Pitch-Rad | -1 … +1 | 0 |
| `perf.mod` | Mod Wheel | Mod-Rad | 0 … 1 | 0 |

## Life per part (p15)

Appended after the performance wheels (last ID `perf.mod`), one per part in part order. Guides:
[English](en/USER-INTERFACE.md#life) · [Deutsch](de/BEDIENUNG.md#leben).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `pN.life` (N = 1 … 4) | Part N Life | Part N Leben | 0 … 1 (0 – 100 %) | 0 |

`pN.life` makes held notes keep evolving: every voice of the part gets its own slow pitch drift (at most about ±12 cents),
free chaos LFOs at incommensurate rates, filter / brightness breathing, a little colour motion of the engine (wavetable
position, FM index, formant, grain density), extra grain scatter (Grain engine) and a hint of pan and level motion
(about ±0.25 dB). 0 = static: the sound is exactly the same as without the parameter. Each voice is seeded from its
part, its voice slot, its note and its start count, so a render is repeatable. Changes are followed over about 60 ms
(no steps). The value is part of part presets and projects and MIDI-learnable.

The modulation matrix gains the target 72 **Life** (`pN.mod.sK.dst`, key `life`), which moves `pN.life` of the routing's part.

## Morph space (p16)

Appended after the life block (`p4.life`), part by part (`p1.morph.*` … `p4.morph.*`, 24 IDs). Guides:
[English](en/MORPH.md) · [Deutsch](de/MORPH-RAUM.md). The corners themselves are not parameters: they live in
part presets (`"morph"` block) and in projects / the plug-in state (section `"morph"`).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `pN.morph.on` | Part N Morph | Part N Morph | Off / On | Off |
| `pN.morph.x` | Part N Morph X | Part N Morph X | 0 … 1 (left … right) | 0.5 |
| `pN.morph.y` | Part N Morph Y | Part N Morph Y | 0 … 1 (bottom … top) | 0.5 |
| `pN.morph.glide` | Part N Morph Glide | Part N Morph-Gleiten | 0 … 2 s | 0.05 |
| `pN.morph.xsrc` | Part N Morph X Control | Part N Morph X Steuerung | Off, Pressure, Slide, Mod Wheel | Off |
| `pN.morph.ysrc` | Part N Morph Y Control | Part N Morph Y Steuerung | Off, Pressure, Slide, Mod Wheel | Off |

The modulation matrix gains the sources **Morph X / Morph Y** (21, 22) and the targets **Morph X / Morph Y**
(73, 74); the source and target choice lists grow at their end.
