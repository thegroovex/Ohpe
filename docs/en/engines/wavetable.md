# Wavetable engine

*English · Deutsch: [../../de/engines/wavetable.md](../../de/engines/wavetable.md)*

Wavetable plays through a table of eight single-cycle frames and glides smoothly between them.
Six tables are built in (generated when Ohpe starts, no sample files): **Classic** (sine →
triangle → saw → square → narrowing pulse), **Harmonic** (an additive stack that grows frame by
frame), **Formant** (vowel-like a – e – i – o – u), **Digital** (sparse, comb-like spectra),
**Glass** (bell-like overtones) and **Grit** (seeded noisy spectra that get brighter).

Every table is stored as a set of band-limited copies, one per octave; the engine crossfades
between them depending on the note and on how much the phase is warped, so high notes stay free
of aliasing. **Warp** is a smooth phase distortion (*Bend*: one bulge per cycle, *Twist*: two),
which brightens the wave without adding foldover; it eases off over the top octaves for that
reason. Three unison oscillators, a sub sine and a resonant low-pass complete the voice.

| Encoder | Parameter | Range | What it does |
|---|---|---|---|
| 1 | Position | 0–1 | position in the table (frame morph) |
| 2 | Warp | 0–1 | phase distortion depth |
| 3 | Tone | 0–1 | low-pass cutoff (≈ 40 Hz – 20 kHz); mod envelope and LFO *Timbre* move it |
| 4 | Unison | 0–1 | adds two detuned side oscillators, detune and stereo width |
| – | Table | list | Classic, Harmonic, Formant, Digital, Glass, Grit |
| – | Warp Mode | list | Bend, Twist |
| – | Env > Position | −1 – +1 | how far the mod envelope sweeps the position |
| – | Resonance | 0–1 | filter resonance |
| – | Sub | 0–1 | sine one octave below |
| – | Key Track | 0–1 | cutoff follows the keyboard |
| – | Unison Voices (0.8) | 1–8 | 3 = the classic centre + two sides; 1, 2, 4 … 8 = an evenly spread stack (reese, supersaw) |
| – | Unison Width (0.8) | 0–1 | stereo spread of the stack (not used by the classic three) |
| – | FM Amount (0.8) | 0–1 | phase modulation of the table by a sine (growls, metallic edges); fades out on the top notes so it never aliases |
| – | FM Ratio (0.8) | 0.25–8 | frequency of the FM sine relative to the note |
| – | LFO > Position (0.8) | −1 – +1 | the part LFO moves the table position (slow scans, talking growls) |
| – | Env > FM (0.8) | −1 – +1 | the mod envelope adds (or removes) FM |

Parameter IDs: `p1.wavetable.pos`, `.warp`, `.tone`, `.unison`, `.table`, `.warpmode`, `.envpos`,
`.reso`, `.sub`, `.keytrack`, 0.8: `.univoices`, `.uniwidth`, `.fm`, `.fmratio`, `.lfopos`, `.envfm`.

**Bass with a wide stack (0.8):** the stack spreads the low end too – set the part's *Bass Mono*
(`pN.monobass`, e.g. 150 Hz) or a *Width* slot with *Mono Below* to keep the bass mono-safe.

**Starting points:** pad – Formant, Position 0.3, Env > Position 0.4, slow amp attack; bass –
Classic, Position 0.45, Unison 0, Sub 0.5, Tone 0.5; bright lead – Digital, Warp 0.4, Unison 0.5.
