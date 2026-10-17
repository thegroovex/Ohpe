# Harmony brain

*English · Deutsch: [../de/HARMONIE.md](../de/HARMONIE.md)*

![The Harmony view with the grid keyboard](../images/ohpe-harmony.png)

Ohpe listens to what you play and keeps a **scale and chord memory shared by all four parts** – the *harmony
brain*. Play chords on one part (keys, MIDI, a sequencer, the arpeggiator) and the other parts can **follow** them:
a bass that only hits chord tones, a lead that cannot leave the key, an arpeggio that moves with your changes –
without setting a key by hand. An **isomorphic grid keyboard** with MPE plays it from the screen.

## What the brain hears and remembers

* **Who is heard**: every note that reaches a part whose *Follow* is **Off** – the leading parts. Parts that follow
  are not heard (no feedback). The drum kit is not heard.
* **Chord**: as soon as two or more different notes are held, the brain names the chord – major, minor, diminished,
  augmented, sus2, sus4, maj7, m7, 7 or a bare fifth; the lowest note decides between two readings (A–C–E over A is
  *Am*). A chord counts once it stood for 40 ms (the notes of a played chord arrive one by one). It **stays in
  memory** after you let go, until the next chord. Letting go of one note of a triad keeps the chord.
* **Key**: a pitch-class memory gathers every note (its start and how long it is held) and fades with a half-life of
  **Memory** bars (1 – 32, default 8). The brain picks the major or minor key that fits best; another key must win
  clearly before it switches. Until anything was heard, the key is the global **Key / Scale** (`key.root`,
  `key.scale`).
* **History**: the last eight chords, newest first.

## Follow

Each part has **Follow** (`harmony.pN.follow`):

| Follow | The part's notes … |
|---|---|
| **Off** | … play as before (and are heard by the brain) – neutral, the default |
| **Scale** | … snap to the nearest note of the remembered key |
| **Chord** | … snap to the nearest tone of the remembered chord (before any chord: the key's tonic triad) |
| **Root** | … snap to the chord's root (any octave) |

Snapping uses the nearest note (a tie goes down) – live keys, host MIDI and all sequencers alike. The part's
one-finger **Chord** still works and builds its chord in the remembered key. Notes of a part with **MPE** on keep
their pitch (per-note expression); use the grid's *In Key* layout there.

## Controls

| Control | ID | What it does |
|---|---|---|
| **Follow** (per part) | `harmony.pN.follow` | Off / Scale / Chord / Root |
| **Memory** | `harmony.memory` | half-life of the key memory in bars (1 – 32) |
| **Hold** | `harmony.hold` | freezes the memory: new notes change neither chord nor key |
| **Take Key** | `harmony.take` | trigger: writes the remembered key into the global Key / Scale |
| **Forget** | `harmony.clear` | trigger: forgets key, chord, history |
| **Grid Layout** | `harmony.kb.layout` | Fourths / Thirds / Whole Tones / In Key |

All are host parameters: automatable and MIDI-learnable (right-click). The memory (key, chord, history, pitch-class
weights) is stored in projects and the plug-in state; part presets neither carry nor change *Follow*.

## The Harmony view

LFO mode, fifth tab **Harmony**: the remembered **key** and **chord** (the chord glows when it changes), twelve bars
for the pitch-class memory starting at the key's root (key notes light, other notes dim, **chord tones violet**,
**held notes green**, the root marked **yellow**) and the **history** chips. The strip says what the brain does and
whether the edit part follows. Soft keys: **Take Key**, **Hold**, **Forget**. Encoders: Follow (edit part), Memory,
Hold, Grid Layout.

## Chord names and the key (p45)

While notes sound, the view's **Chord** shows the chord they make – every part together, with the notes following parts
and one-finger chords add – with the full naming of the display's voice strip: sixths, sevenths (also without the
fifth), m7b5, dim7, sus, add9, ninths, and **slash chords** for inversions (*D/F#*) or a foreign bass (*C/D*); the
legend adds the position (*1st inversion*). When nothing sounds it shows the remembered chord as before. The status
line adds *Playing F#m7*. Note and chord names are spelled in the global key: flats in flat keys (*Bb*, *Eb*), sharps in
sharp keys (*F#*, *C#*), German *H* / *B*. **Scale Lock** (on the display header, see
[Key, chord and Scale Lock](USER-INTERFACE.md#key-chord-and-scale-lock-p45)) uses the same global key; a following
part (Follow Scale / Chord / Root) follows the brain as before and ignores Scale Lock's snapping (White Keys still maps
its keys first).

## The grid keyboard

The **GRID** key (between OCTAVE + and MIDI, yellow bar while on) swaps the piano keys for a grid of 4 × 24
cells. On an isomorphic grid every chord and scale has **the same shape in every key** – learn a shape once,
move it anywhere.

| Layout | Right | Up |
|---|---|---|
| **Fourths** | +1 semitone | +5 (a fourth) |
| **Thirds** | +1 | +4 (a major third) |
| **Whole Tones** | +2 | +5 |
| **In Key** | +1 step of the remembered key | +3 steps (a fourth) – only key notes |

Key notes are light cells, other notes graphite; **chord tones** carry a **violet** bar, the key's root is
labelled with its octave, a pressed cell sinks with a red bar. **OCTAVE −/+** shifts the grid like the piano; the
readout shows its range. Where you strike a cell sets the velocity (higher = louder).

It plays the **edit part**. With **MPE** on for that part every touch gets its own member channel (2 – 16):
**slide sideways** to bend the note (one cell = the layout's step, scaled by the part's MPE bend range),
**up / down** sends slide (CC 74), touch or pen **pressure** sends pressure. Several fingers on a touch screen play
several notes, each with its own expression. Without MPE the notes play on the part's channel (through scale,
chord and follow).

## Modulation

Three matrix sources (the same value in every part):

| # | Source | Range | |
|---|---|---|---|
| 29 | **Chord Root** | 0 … 1 | the chord's root, C = 0 … B = 1 |
| 30 | **Chord Change** | 0 … 1 | 1 at every new chord, fades in about 0.3 s – a filter accent on each change |
| 31 | **Tension** | 0 … 1 | share of held notes outside the key (smoothed) |
