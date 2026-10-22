# Key Gravity (p66)

[Deutsch](../de/TONART-GRAVITATION.md)

Every part has a **Key Gravity** stage right after its engines (any of the eight): it pulls every partial of the sound
toward the current key and the chord that is held. A slightly detuned pad locks into the key, a melody leans onto the
chord tones, and at high amounts noise and drum loops turn into pitched texture that plays the chord. It sits on the
Effects mode's last two encoder pages **Key Gravity** and in the pro view as the card **Key Gravity**.

| Control | Parameter | What it does |
|---|---|---|
| Gravity | `pN.grav.amt` | 0 = off (the part sounds exactly as without the stage: bit-identical, no latency, no CPU) … 100 % |
| Target | `pN.grav.target` | **Scale** (the key's notes), **Chord** (only the held chord's notes) or **Chord+Scale** (the chord tones pull harder, the scale fills between them) |
| Glide | `pN.grav.glide` | 5 ms … 2 s: how fast the partials flow to the new notes when the chord or key changes |
| Curve | `pN.grav.curve` | −100 % (gentle: partials only lean toward the notes) … +100 % (magnetic: even a little gravity snaps them on) |
| Formant | `pN.grav.formant` | formant keep: the tone colour (spectral envelope) stays where it was while the partials move |
| Mix | `pN.grav.mix` | blend of the pulled sound with the original (in time with each other) |
| Quality | `pN.grav.quality` | **Low Latency** (≈ 11 ms), **Normal** (≈ 21 ms), **High** (≈ 43 ms, resolves low notes best) |

## Where the notes come from

* **Key / scale**: the key and scale of the instrument (KEY on the plate), or the host's key when the host sends one.
* **Chord**: the notes held on all four parts right now, named like the chord display (a C with E and G is C major,
  stray notes do not count). With nothing held, the last chord the harmony memory heard; before any chord, the key's
  tonic triad.
* Every octave counts: a partial goes to the nearest target pitch above or below it.

## How it works

A short-time spectrum (Hann window, four times overlap) finds the peaks of the sound; the true frequency of each comes
from its phase from frame to frame. Each peak carries the bins around it (phase-locked, so the partial stays one
partial) and moves to its target pitch – partly, by Gravity and Curve. Partial tracks glide to a new target when the
chord changes. Attacks pass with their own phases, so drums keep their hit. Above 50 % the energy between the target
pitches fades and what lands on a target note takes that note's running phase – noise becomes a chord.

* **Latency**: one analysis frame (above). The host is told, and every part – and the drum kit – waits for the slowest
  running stage, so all stay in time. At Gravity 0 for one second the stage stops and the latency goes away again.
* **Real time**: no memory is taken while playing; any block size works (one sample too).
* **CPU**: about 1.5 % of one core per part (48 kHz, any quality); four parts about 6 % of one core.
* **Sample rates**: 44.1 – 192 kHz (the frame grows with the rate, so the pitch resolution stays the same).

## Modulation and MIDI

All seven controls are host parameters and MIDI-learnable. Matrix targets (appended): 90 **Key Gravity**, 91 **Gravity
Glide**, 92 **Gravity Curve**, 93 **Gravity Formant**, 94 **Gravity Mix** – e.g. the mod wheel → Key Gravity to play
a drum loop into the chord. See [Modulation](MODULATION.md).

## Factory sounds

*Gravity Loop Pad* (a drum loop becomes a pad in key), *Gravity Choir Noise*, *Gravity Bell Lock* and *Gravity Chord Rain*
show the stage; they live in the bank under the tag *gravity*.
