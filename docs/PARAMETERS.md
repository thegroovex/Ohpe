# Ohpe parameters

*English · Deutsch: German names in the tables. The complete, ordered table of every parameter is
printed by `ohpe_param_dump --markdown`; IDs are append-only (see [ARCHITECTURE.md](ARCHITECTURE.md)).
Note names: C4 = MIDI 60.*

## Phase 2 – Grain, Vocal, Sampler engines and drum kit

Appended after `seq.step.channel` (0.1.0). The first four parameters of each engine are its macros.
Guides: [English](en/GRAIN-VOCAL-SAMPLER-DRUM.md) · [Deutsch](de/GRAIN-VOKAL-SAMPLER-DRUM.md).

### Grain (`p1.grain.*`)

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `p1.grain.pos` | Position | Position | 0 … 1 | 0.3 |
| `p1.grain.size` | Grain Size | Korngröße | 5 … 500 | 80 |
| `p1.grain.density` | Density | Dichte | 1 … 100 | 20 |
| `p1.grain.spray` | Spray | Streuung | 0 … 1 | 0.1 |
| `p1.grain.pitch` | Grain Pitch | Korn-Tonhöhe | -24 … 24 | 0 |
| `p1.grain.reverse` | Reverse Chance | Rückwärts-Anteil | 0 … 1 | 0 |
| `p1.grain.spread` | Stereo Spread | Stereobreite | 0 … 1 | 0.5 |
| `p1.grain.scan` | Scan Speed | Scan-Tempo | -2 … 2 | 0 |
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

`drum.vol`, `drum.base`, then for every pad N = 1 … 24 the seven parameters
`drum.padN.type`, `.tune`, `.decay`, `.level`, `.pan`, `.choke`, `.tone` (pad 1 shown; pads 2 – 24 are
identical, with the defaults of the factory kit below).

| ID | English | Deutsch | Range | Default |
|---|---|---|---|---|
| `drum.vol` | Drum Volume | Drum-Lautstärke | 0 … 1 | 0.8 |
| `drum.base` | Drum Base Note | Drum-Grundnote | 0 … 104 | 36 |
| `drum.pad1.type` | Pad 1 Sound | Pad 1 Klang | Kick, Snare, Hat, Clap, Tom, Perc, Sample | 0 |
| `drum.pad1.tune` | Pad 1 Tune | Pad 1 Stimmung | -24 … 24 | 0 |
| `drum.pad1.decay` | Pad 1 Decay | Pad 1 Abklingen | 0 … 1 | 0.4 |
| `drum.pad1.level` | Pad 1 Level | Pad 1 Pegel | 0 … 1 | 0.9 |
| `drum.pad1.pan` | Pad 1 Pan | Pad 1 Panorama | -1 … 1 | 0 |
| `drum.pad1.choke` | Pad 1 Choke Group | Pad 1 Choke-Gruppe | 0 … 8 | 0 |
| `drum.pad1.tone` | Pad 1 Tone | Pad 1 Klangfarbe | 0 … 1 | 0.5 |

Factory kit (fully synthetic; MIDI channel 10, `drum.base` = 36):

| Pad | Note | Type | Tune | Decay | Pan | Choke | Tone | Level | Sound |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 36 (C2) | Kick | +0 | 0.40 | +0.0 | 0 | 0.5 | 0.90 | kick |
| 2 | 37 (C#2) | Kick | -3 | 0.70 | +0.0 | 0 | 0.3 | 0.90 | long kick |
| 3 | 38 (D2) | Snare | +0 | 0.40 | +0.0 | 0 | 0.5 | 0.80 | snare |
| 4 | 39 (D#2) | Snare | +3 | 0.25 | +0.0 | 0 | 0.8 | 0.75 | bright snare |
| 5 | 40 (E2) | Clap | +0 | 0.40 | +0.0 | 0 | 0.5 | 0.75 | clap |
| 6 | 41 (F2) | Hat | +0 | 0.08 | +0.2 | 1 | 0.6 | 0.60 | closed hat |
| 7 | 42 (F#2) | Hat | +0 | 0.60 | +0.2 | 1 | 0.6 | 0.55 | open hat |
| 8 | 43 (G2) | Hat | -2 | 0.15 | +0.2 | 1 | 0.4 | 0.55 | pedal hat |
| 9 | 44 (G#2) | Tom | -5 | 0.50 | -0.4 | 0 | 0.5 | 0.80 | low tom |
| 10 | 45 (A2) | Tom | +0 | 0.45 | +0.0 | 0 | 0.5 | 0.80 | mid tom |
| 11 | 46 (A#2) | Tom | +5 | 0.40 | +0.4 | 0 | 0.5 | 0.80 | high tom |
| 12 | 47 (B2) | Perc | +0 | 0.20 | -0.3 | 0 | 0.3 | 0.70 | rim |
| 13 | 48 (C3) | Perc | +7 | 0.35 | +0.3 | 0 | 0.7 | 0.60 | bell |
| 14 | 49 (C#3) | Perc | +12 | 0.15 | +0.4 | 0 | 0.5 | 0.60 | high perc |
| 15 | 50 (D3) | Kick | +4 | 0.25 | +0.0 | 0 | 0.8 | 0.90 | tight kick |
| 16 | 51 (D#3) | Snare | -3 | 0.55 | +0.0 | 0 | 0.3 | 0.80 | deep snare |
| 17 | 52 (E3) | Clap | -2 | 0.60 | +0.0 | 0 | 0.3 | 0.70 | wide clap |
| 18 | 53 (F3) | Hat | +5 | 0.06 | -0.3 | 2 | 0.8 | 0.55 | closed hat 2 |
| 19 | 54 (F#3) | Hat | +5 | 0.70 | -0.3 | 2 | 0.8 | 0.50 | open hat 2 |
| 20 | 55 (G3) | Tom | -9 | 0.60 | -0.5 | 0 | 0.4 | 0.80 | floor tom |
| 21 | 56 (G#3) | Perc | -5 | 0.30 | -0.2 | 0 | 0.4 | 0.65 | low perc |
| 22 | 57 (A3) | Perc | +3 | 0.50 | +0.2 | 0 | 0.9 | 0.55 | metal perc |
| 23 | 58 (A#3) | Snare | +7 | 0.15 | +0.1 | 0 | 1.0 | 0.70 | snap |
| 24 | 59 (B3) | Kick | -7 | 0.90 | +0.0 | 0 | 0.2 | 0.90 | sub kick |
