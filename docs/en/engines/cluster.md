# Cluster engine

*English · Deutsch: [../../de/engines/cluster.md](../../de/engines/cluster.md)*

Cluster plays a whole stack of oscillators for every key: up to seven band-limited saw / pulse
oscillators, detuned against each other and spread across the stereo field. The stack can be a
thick unison or a chord shape, so one finger plays octaves, fifths, triads, sevenths or stacked
fourths. A slow random **drift** keeps the stack alive; a stereo resonant low-pass shapes it.

| Encoder | Parameter | Range | What it does |
|---|---|---|---|
| 1 | Detune | 0–1 | detune between the stack voices (fine near 0, up to ± 55 cents) |
| 2 | Spread | 0–1 | stereo width of the stack |
| 3 | Tone | 0–1 | low-pass cutoff; mod envelope and LFO *Timbre* move it |
| 4 | Chord | list | Unison, Octave, Fifth, Major, Minor, Sus2, Sus4, Major 7, Minor 7, Dominant 7, Add 9, Fourths |
| – | Stack Voices | 1–7 | oscillators per note (the first ones of the chord shape) |
| – | Shape | 0–1 | saw → square → narrow pulse |
| – | Side Level | 0–1 | level of the stack voices against the centre voice |
| – | Resonance | 0–1 | filter resonance |
| – | Key Track | 0–1 | cutoff follows the keyboard |
| – | Drift | 0–1 | slow random pitch movement of the stack voices |

Parameter IDs: `p1.cluster.detune`, `.spread`, `.tone`, `.chord`, `.stack`, `.shape`, `.side`,
`.reso`, `.keytrack`, `.drift`.

**Starting points:** big lead – Unison, Detune 0.35, Spread 0.7; chord stab – Minor 7, Stack
Voices 4, short amp decay, Tone 0.6 with some mod envelope; organ-like – Octave, Detune 0.05,
Shape 0.5.
