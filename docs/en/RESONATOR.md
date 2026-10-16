# Resonator body (p19)

*English · Deutsch: [../de/RESONANZKOERPER.md](../de/RESONANZKOERPER.md)*

Every part can play through a **physically modelled body**: a string, a plate or a tube that the sound
of the engine excites. A pluck strikes a tuned metal plate, noise bows a bore, a pad wakes a set of
sympathetic strings tuned to the chord you hold. The body follows your keyboard – the note, the chord,
pitch bend and MPE expression – so it is an instrument, not just an effect.

![The Body view](../images/ohpe-reso-view.png)

## Quick start

1. Press **Effects**, then the **Body** tab of the display.
2. Turn **Resonator** on (encoder 1, first page). The part now plays through a string body tuned to
   each note you play.
3. Choose the **Body** (encoder 2): *String*, *Plate*, *Open Tube* or *Closed Tube*.
4. Set **Body Mix** (encoder 4): 0 = the dry part (an exact bypass), 1 = only the body.

Or load one of the resonator factory presets: *Plucked Plate*, *Gamelan Plate*, *Bowed Tube*,
*Breath Bore*, *Sympathetic Pad*, *Drone Strings*, *Glass Body Keys*.

## Where it sits

The body is the first stage after the engine and the layer: **engine → resonator → volume → FX rack →
bass mono → 3D space → mixer**. So the rack's effects colour the body like a microphone and a room
would. The same body is also an FX rack type, **Resonator** (DE *Resonanzkörper*), for any slot of a
part rack or the master rack – put it after a distortion, a reverb or another body. In a part rack it
follows that part's keyboard; on the master rack it stays at its fixed tuning.

## The models

| Body | What it is | Sound |
|---|---|---|
| **String** | two coupled strings (a slightly detuned pair that exchanges energy at the bridge), stiffness for stretched partials, a frequency-dependent loss | plucked and struck strings, a beating, two-stage decay like a piano unison |
| **Plate** | 48 modes of a rectangular plate, excited at one point and picked up at two (natural stereo) | bells, gongs, metal and glass plates, gamelan |
| **Open Tube** | a bore open at both ends (all harmonics) | flutes, pipes, breathy organ tones |
| **Closed Tube** | a bore closed at one end (odd harmonics, a quarter-wave resonance) | clarinet-like, hollow, woody |

Every body can add up to six **sympathetic strings** that the body's vibration drives: they ring on at
the octaves and fifths of the note, or at the notes of the chord you hold – and keep ringing after you
let go (in *Chord* tuning they keep the last chord).

## Parameters

| Page | Encoder 1 | Encoder 2 | Encoder 3 | Encoder 4 |
|---|---|---|---|---|
| Resonator | Resonator (on / off) | Body (model) | Tuning (Fixed, Note, Chord) | Body Mix |
| Body Tuning | Body Tune (± 24 semitones) | Size | Material | Decay (0.05 – 20 s) |
| Body Character | Position | Inharmonicity | Sympathetic Strings | Body Width |
| Drive & Expression | Drive | Pressure to Body | Slide to Position | – |

* **Tuning** – *Note*: the body follows the newest note (plus pitch bend, per-note bend and the part's
  tuning); *Chord*: as *Note*, and the sympathetic strings tune to the held keys; *Fixed*: C3 plus
  **Body Tune**, whatever you play.
* **Size** – String: how many sympathetic strings (2 – 6) and how far the string pair is detuned;
  Plate: its proportions (square and sparse … long and dense); Tubes: the bore width (wider = darker
  inside, more radiated highs).
* **Material** – how the highs decay: low = soft, dark materials (gut, wood), high = steel, bronze, glass.
* **Decay** – how long the fundamental rings (T60). Higher partials fade faster unless Material is high.
* **Position** – where the body is excited: near the end of a string (thin, bright) or in the middle
  (round, hollow); on the plate it changes which modes speak; on a tube where along the bore.
* **Inharmonicity** – String: stiffness (upper partials sharp, piano- and bell-like); Plate: the mode
  pattern from almost harmonic (low) over the ideal plate (middle) to stretched (high); Tubes: bore
  deviations.
* **Drive** – a smooth saturation of the excitation and, for strings and tubes, inside the loop (it
  never adds energy); on the plate the modes bend upward when it rings loud (gong-like pitch glide).
* **Pressure to Body** – MPE / aftertouch pressure of the newest note: positive = it rings longer and is
  excited harder, negative = pressure damps the body (like a palm mute).
* **Slide to Position** – MPE slide (CC 74) moves the excitation point.

All parameters are automatable and MIDI-mappable. The modulation matrix has the targets **Body Tune**,
**Body Size**, **Body Material**, **Body Decay**, **Body Position** and **Body Mix** (75 – 80). For the
rack type, *FX Param 1 – 4* are Body Tune, Material, Decay and Position.

## The display

The Body view draws the body on the dark display: the string pair between its bridges with the
sympathetic strings below, the plate with its moving mode pattern, or the bore with its standing wave
(closed end as a solid cap, open ends radiating). The yellow dot is the excitation point; the motion
grows with the level. The line under the rows shows the model, the tuning and the pitch the body is
tuned to. A Resonator rack slot shows the same picture in the Rack view.

## Sound and safety

* Stable at every setting: every loop loses energy, coupling is lossless, saturation never amplifies,
  states are flushed (no denormals) and guarded; the body's output passes a peak limiter (it never goes
  above full scale).
* **Body Mix 0** or **Resonator off** is an exact bypass: the part sounds bit-identical to no body at all.
  Switching on and off crossfades over 20 ms.
* One body per part (as a real instrument has one body for all its strings), so the cost does not grow
  with the number of voices.
