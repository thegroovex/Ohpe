# Ohpe parameters

*English · Deutsch: German names in the tables. The complete, ordered table of every parameter is
printed by `ohpe_param_dump --markdown`; IDs are append-only (see [ARCHITECTURE.md](ARCHITECTURE.md)).
Note names: C4 = MIDI 60.*

## Phase 2 – Tape machine, mixer dynamics, FX slot (0.2.0)

85 parameters, appended in this order after the 0.1.0 list. Guides:
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
