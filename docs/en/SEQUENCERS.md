# Sequencers, parts and scale mode

*English · Deutsch: [docs/de/SEQUENZER.md](../de/SEQUENZER.md)*

Ohpe has **four parts** that play at the same time, each with its own engine, envelopes, LFO and
mixer channel (part 1–4 → mixer channels 1–4 by default, each routable to Main or to its own
stereo output). Four sequencers drive them. Every sequencer has a **target part**, so a typical
set-up is

| Part | Engine (example) | Driven by |
|---|---|---|
| 1 | FM | Step sequencer (`seq.step.channel` = 1) |
| 2 | Analog | Euclid (`seq.euclid.part` = 2) |
| 3 | FM | Arpeggiator (`seq.arp.part` = 3), played from the keyboard |
| 4 | Analog | Orbit (`seq.orbit.part` = 4) |

All sequencers run from the same transport (internal clock, host, MIDI clock or Link – see the
[MIDI guide](MIDI.md)) and start and stop with **Sequencer Run** (`seq.run`). Their steps sit on
absolute positions of the transport, so they stay in phase with each other and with the host,
also across loops and jumps.

## Step sequencer – patterns and song mode

The 64-step sequencer from 0.1 now holds **16 patterns**.

* **Pattern** (`seq.step.pattern`, 1–16) chooses the pattern that plays. A new choice waits until
  the playing pattern has reached its end, so changes always land on the bar.
* Each pattern can have its **own length** (or follow `seq.step.length`).
* **Song mode** (`seq.step.song`) plays the **chain** instead: up to 64 entries of *pattern ×
  repeats*, looping. The song position is taken from the transport, so a host loop or a jump
  lands on the right pattern and step.
* Patterns, lengths and the chain are saved with the state. Patterns from 0.1 open as pattern 1.
* **Ties (0.5):** a step at gate 100 % ties into the next step when that step plays the same note –
  the note is held instead of struck again and ends with the next step's gate. Chains of 100 % steps
  hold a note (or a one-finger chord) for as long as you like. Not on the drum kit (pads are one-shots).

## Euclid

Four rings, each spreading **pulses** as evenly as possible over **steps** (1–32) with
Bjorklund's algorithm, turned by **rotation** and playing its own **note**. Rings of different
lengths form a polymeter: rings of 3 and 4 steps meet every 12 steps.

| Parameter | Range | |
|---|---|---|
| `seq.euclid.on` | off / on | |
| `seq.euclid.part` | 1–4 | target part (default 2) |
| `seq.euclid.rate` | 1/4 … 1/32, triplets, dotted | shared grid of all rings |
| `seq.euclid.gate`, `.vel` | 5–100 %, 1–127 | note length, velocity |
| `seq.euclid.rN.steps / .pulses / .rot / .note` | per ring 1–4 | |
| `seq.euclid.rN.vel` (0.5) | 0–127 | velocity of one ring; 0 = `seq.euclid.vel` (accents: loud kick, soft hats) |
| `seq.euclid.swing`, `.human` (0.5) | 0–75 %, 0–100 % | swing and humanise, see below |

Examples: 3 pulses in 8 steps `x..x..x.`, 5 in 8 `x.xx.xx.`, 7 in 16 `x..x.x.x..x.x.x.`.

## Arpeggiator

The notes played into the target part (keyboard, MIDI on the part's channel, one-finger chords)
are held and played one after another.

* **Mode**: Up, Down, Up/Down (turning points are not repeated), Random, As Played, Chord (all
  held notes on every step).
* **Octaves** 1–4, **Gate** 5–100 %, **Rate** 1/4 … 1/32 including triplets (T) and dotted (.).
* **Latch** keeps the notes after the keys are released; the next key pressed with no key held
  starts a new set.
* While the transport runs the steps sit on its grid. When it is stopped the arpeggiator runs
  free at the current tempo and starts exactly on the first key.

## Orbit (polymeter)

Four lanes of up to 16 steps, each with its own **length**, **rate** and **transpose**, all
playing into one part. Every lane has its own steps (note, velocity). Lanes drift against each
other and meet again after the least common multiple of their lengths – long, evolving
patterns from a few steps.

* **Lane target** (`seq.orbit.lN.part`, 0.5): 0 = the Orbit target part (or the drum kit); 1–4 = this
  lane plays that part, never the kit. With the drums switched on, lanes 1–3 can play the kit while
  lane 4 plays a bass line – one sequencer for a whole groove.
* **Ties** (0.5): at **Gate** 100 % (`seq.orbit.gate`) a step that repeats the note of the step before
  it continues that note (held, not struck again) – sustained bass notes and chords. Lanes playing the
  drum kit are never tied.
* **Swing** and **Humanise** (`seq.orbit.swing`, `seq.orbit.human`, 0.5), see below.

## Swing and humanise (0.5)

* **Swing** (`seq.orbit.swing`, `seq.euclid.swing`, 0–75 %) moves every note on an off-beat
  sixteenth late by swing ÷ 2 of a sixteenth – the same amount as the step sequencer's
  `seq.step.swing` (30 % ≈ a 57 % shuffle, 75 % = 3/8 of a sixteenth). Lanes and grids at 1/8 or
  slower never land on an off-beat sixteenth, so all parts of a groove swing together.
* **Humanise** (`seq.orbit.human`, `seq.euclid.human`, 0–100 %): at 100 % a note starts up to 10 ms
  late and its velocity moves by up to ±16. The variation is repeatable (it starts the same way every
  time the sequencer is reset), so a bounce sounds like what you heard.

## Sequencers playing the drum kit (0.4)

The step sequencer, Euclid and Orbit can play the **drum kit** instead of their target part:
`seq.step.drums`, `seq.euclid.drums`, `seq.orbit.drums` (off / on). Their notes then go to the pads
(note − `drum.base` = pad, by default C2 = kick, D2 = snare, F2 = closed hi-hat …), as if played on
MIDI channel 10; scale quantise and chords do not apply. Orbit makes a drum machine with one lane
per drum and its own length per lane; Euclid plays four polyrhythmic drums. The factory project
presets use both – see [FACTORY-SOUNDS.md](FACTORY-SOUNDS.md).

## Poly glide (0.5)

**Glide** (`pN.glide`) slides the pitch from note to note. With one voice (mono) every new note
slides from the last. With **Poly Glide** (`pN.polyglide`) on and several voices, every note of a new
chord takes over the voice of the nearest note just released and slides from there – chords glide
into each other voice by voice (the *sliding* pads of the factory bank). Without released voices
(the first chord, or notes added to held ones) notes start on their own pitch.

## Scale and chord mode

* **Key** (`key.root`) and **Scale** (`key.scale`: chromatic, major, minor, the church modes,
  harmonic and melodic minor, pentatonics, blues, whole tone) are global.
* **Scale Quantise** (`pN.quant`) moves every note going into a part to the nearest note of the
  scale – from the keyboard, MIDI and the sequencers.
* **Chord** (`pN.chord`): one key plays a chord – Triad, Seventh, Sus2, Sus4, Power, Octave,
  Sixth, Add9. In a scale the chords are diatonic (in C major a D plays D minor); with the
  chromatic scale they use fixed major intervals. Chords feed the arpeggiator, so one finger
  plays an arpeggiated chord.
* A key's release always ends exactly the notes it started, even if the settings changed in
  between.

## Parameter IDs

All IDs are listed in [PARAMETERS.md](../PARAMETERS.md) and by `ohpe_param_dump --markdown`.
