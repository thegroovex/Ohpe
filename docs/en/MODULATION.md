# Modulation matrix

*English · Deutsch: [../de/MODULATION.md](../de/MODULATION.md) · parameter tables:
[../PARAMETERS.md](../PARAMETERS.md)*

Every part has a modulation matrix with **16 routings**, two extra LFOs, a third envelope, four
macros and a sequencer lane. Held notes keep moving: route an LFO to the wavetable position, an
envelope to the cutoff, pressure to the level – each routing is one line of the matrix.

```
 source ──curve──×──(via)──× amount ──▶ target
   LFO 1-3, envelopes, velocity, key, wheel, bend, pressure, slide, random, macros, seq lane
```

## A routing

| Field | Values | Notes |
|---|---|---|
| Source (`pN.mod.sK.src`) | see the source table | Off = the routing is silent |
| Via (`pN.mod.sK.via`) | any source | scales the routing (e.g. LFO via mod wheel: the wheel opens the vibrato); Off = 1 |
| Target (`pN.mod.sK.dst`) | see the target table | |
| Amount (`pN.mod.sK.amt`) | −100 … +100 % | bipolar; negative amounts invert |
| Curve (`pN.mod.sK.curve`) | Linear, Exponential, Logarithmic, S-Curve, Steps | shapes the source (mirrored for bipolar sources); Steps = 8 steps |

All fields are ordinary parameters: host-automatable, MIDI-learnable, stored in presets and
projects. The routing value is `curve(source) × via × amount`. Several routings to the same target
add up.

## Sources

| # | Source | Range | Per voice | Notes |
|---|---|---|---|---|
| 1 | LFO 1 | ±1 | yes | the part's LFO (LFO page), before its depth – usable even with LFO depth 0 |
| 2, 3 | LFO 2, LFO 3 | ±1 | yes | see below |
| 4 | Amp Env | 0 … 1 | yes | the amplitude envelope of the voice |
| 5 | Mod Env | 0 … 1 | yes | the mod envelope of the voice |
| 6 | Env 3 | 0 … 1 | yes | delay – attack – hold – decay – sustain – release |
| 7 | Velocity | 0 … 1 | yes | |
| 8 | Key Track | ±1 | yes | C4 = 0, ±5 octaves = ±1 |
| 9 | Mod Wheel | 0 … 1 | – | CC 1 |
| 10 | Pitch Bend | ±1 | – | |
| 11 | Channel Pressure | 0 … 1 | – | channel aftertouch of the part |
| 12 | Note Pressure | 0 … 1 | yes | poly aftertouch or MPE pressure of the note (channel pressure acts on all notes) |
| 13 | Slide (Timbre) | ±1 | yes | CC 74 / MPE slide, centre = 0 |
| 14 | Note Bend | ±1 | yes | per-note pitch bend of an MPE member channel |
| 15 | Random | ±1 | yes | a new value with every note |
| 16 – 19 | Macro 1 – 4 | 0 … 1 | – | the parameters `pN.macro1` … `pN.macro4`: knobs to automate or map |
| 20 | Seq Lane | 0 … 1 | – | the parameter `pN.seqlane`, made for step locks: each step sets the lane |

**Per voice** sources have their own value in every voice: with a chord, every note gets its own
envelope, velocity and random value.

### LFO 2 and LFO 3 (`pN.lfo2.*`, `pN.lfo3.*`)

| Parameter | Values |
|---|---|
| Rate | 0.01 … 40 Hz (when Sync = Free) |
| Sync | Free, 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, dotted 1/2 … 1/16, triplet 1/2 … 1/16 |
| Shape | Sine, Triangle, Saw Up, Saw Down, Square, S&H, Smooth Random |
| Phase | start phase 0 … 1 |
| Fade In | 0 … 10 s: the LFO grows in after each note |
| Mode | Free-Running (one phase for the part; synced to the song position while the transport runs), Retrigger (restarts with each note, own phase per voice), One-Shot (one cycle, then holds: an extra envelope) |
| Depth | 0 … 1 |

### Envelope 3 (`pN.env3.*`)

Delay 0 … 4 s, Attack 1 ms … 10 s (linear), Hold 0 … 4 s, Decay 1 ms … 10 s, Sustain 0 … 1,
Release 1 ms … 10 s (exponential).

## Targets

| # | Target | Per voice | Range at 100 % |
|---|---|---|---|
| 1 | Pitch | yes | ±12 semitones |
| 2 | Level | yes | gain × (1 + value), at least 0 – ramped sample by sample |
| 3 | Pan | yes | full left … right |
| 4 | Timbre | yes | the engine's own brightness (Analog / Cluster / Grain / Sampler / Wavetable: ±3 octaves of the filter, FM: index, Pluck: brightness, Vocal: vowel) |
| 5 – 28 | Engine 1 – 24 | – | parameter 1 … 24 of the active engine (its own order, the first four are the macros: e.g. Wavetable position, FM index, Grain position, Vocal formant), full range |
| 29 | Volume | – | part volume |
| 30 – 35 | LFO 1 – 3 Rate / Depth | – | |
| 36 | Mod Env Amount | – | |
| 37 – 42 | FX 1 – 6 Mix | – | mix of the FX rack slot |
| 43 – 66 | FX 1 – 6 Param 1 – 4 | – | four parameters of the effect in that slot (follows the slot's type: the first four of a 0.2 type, the *Mod 1–4* of a [rack type](TAPE-MIXER-FX.md#rack-effect-types)) |
| 67 – 71 | Azimuth, Elevation, Distance, Motion Path Rate / Phase | – | 1.0: the part's 3D stage `pN.space.*` ([SPACE.md](SPACE.md)), full range (azimuth ±180°, elevation −60 … 90°, distance 0.2 … 40 m logarithmic, rate ×−4 … 4, phase 0 … 360°); e.g. Slide → Elevation, Note Pressure → Distance for MPE |
| 72 | Life | – | p15: the part's life amount `pN.life` ([USER-INTERFACE.md](USER-INTERFACE.md#life)), 0 … 100 %; e.g. Mod Wheel → Life to bring held notes alive, Note Pressure → Life for MPE |

Targets 5 and up move a **parameter** by `amount × source` of its full range (normalised, so a
cutoff moves in octaves): one value for the part, computed from part-wide sources – the
free-running LFOs, the wheel and macros, and the envelopes / velocity / key / random / expression of
the newest note. Targets 1 – 4 are computed in every voice. Use Timbre for brightness that follows
each note of a chord on its own.

## Smoothness and real time

* Per-voice routings are evaluated every 32 samples; the level is ramped per sample (no zipper
  noise even on fast tremolo), pitch, pan and timbre glide over about 1 ms, so stepped sources
  (S&H, Steps) stay crisp without clicks.
* Parameter routings are applied every 32 samples and glide over about 1.5 ms; effects glide their
  own parameters on top. The parameter itself (host automation, UI value) is never changed – the
  UI shows the modulation as a ring around the value.
* Everything runs on the audio thread without allocation; a routing with amount 0, source Off or a
  via at 0 leaves the sound bit-identical.

## Movement presets

Factory presets set routings like any other parameter (`"mod.s1.src": 2` …). Useful starting
points: LFO 2 (Smooth Random, 0.2 Hz) → Engine 1 (position) 15 %; Env 3 (slow attack) → Timbre
20 %; Random → Pan 30 %; LFO 3 (synced 1/1, Free-Running) → FX 1 Mix 20 %; Note Pressure → Level
or Timbre for MPE.

## Extending (developers)

The source and target lists are append-only tables (`src/core/mod/ModMatrix.cpp`,
`sourceTable()` / `destTable()`), each entry with a stable key. A new part-wide source (for
example a morph pad, a motion-recorder lane, a performance pad or harmony events) is appended and
fed with `PartMatrix::setSourceValue`; a new target that moves a part parameter names that
parameter's ID suffix and becomes active as soon as the parameter exists. Presets that contain
routings carry `"modSchema"`; `mod::migrate()` is the hook for later schema versions.
