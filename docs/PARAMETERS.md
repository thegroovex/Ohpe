# Ohpe parameters

*English · Deutsch: German names in the tables. The complete, ordered table of every parameter is
printed by `ohpe_param_dump --markdown`; IDs are append-only (see [ARCHITECTURE.md](ARCHITECTURE.md)).
Note names: C4 = MIDI 60.*

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
