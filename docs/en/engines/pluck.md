# Pluck engine

*English · Deutsch: [../../de/engines/pluck.md](../../de/engines/pluck.md)*

Pluck models struck and plucked things. The **string** is a waveguide: a short delay line that
recirculates a burst of shaped noise, losing a little energy and brightness on every pass. Its
tuning compensates the delay of the loop filters exactly, so every note is in tune across the
keyboard. The **mallet** is a set of five tuned resonators (like a struck wooden or metal bar)
excited by a soft or hard mallet. **Material** blends between the two; a small resonant **body**
colours both. The string rings by itself: while the key is held it decays naturally, the amp
envelope only shapes the overall level and the release.

| Encoder | Parameter | Range | What it does |
|---|---|---|---|
| 1 | Material | 0–1 | 0 = string, 1 = mallet, in between a blend |
| 2 | Damping | 0–1 | decay time (long ringing → short thud) |
| 3 | Brightness | 0–1 | how fast the upper harmonics fade; mod envelope and LFO *Timbre* move it |
| 4 | Body | 0–1 | amount of body resonance |
| – | Excitation | 0–1 | soft → hard pluck / mallet (velocity adds to it) |
| – | Pick Position | 0.02–0.5 | where the string is plucked (near the end = thin and bright, middle = hollow) |
| – | Body Size | 0–1 | small → large body |

Parameter IDs: `p1.pluck.material`, `.damping`, `.bright`, `.body`, `.excite`, `.pickpos`,
`.bodysize`.

**Starting points:** nylon – Brightness 0.35, Body 0.5, Pick Position 0.25; harp – Damping 0.2,
Brightness 0.6, long amp release; marimba-like – Material 1, Excitation 0.4, Damping 0.5.
