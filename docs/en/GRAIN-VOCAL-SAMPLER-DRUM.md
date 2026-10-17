# Grain, Vocal, Sampler and the drum kit

*English · Deutsch: [../de/GRAIN-VOKAL-SAMPLER-DRUM.md](../de/GRAIN-VOKAL-SAMPLER-DRUM.md)*

Phase 2 adds three engines for every part (selected with **Engine**, `pN.engine`) and the
**drum kit**, which has its own mixer channel. All of them sound right away: without loaded samples
the Grain engine granulates a built-in evolving pad texture, the Sampler plays a built-in
electric-piano-like keys tone, and the drum kit plays its factory recordings. Every parameter is listed in [PARAMETERS.md](../PARAMETERS.md).

## Grain

Granular synthesis: every voice sprays short, softly windowed grains read from a source.

| Control | What it does |
|---|---|
| **Position** (macro 1) | where in the source the grains are read (0 = start, 1 = end) |
| **Grain Size** (macro 2) | length of a grain, 5 – 500 ms |
| **Density** (macro 3) | grains started per second per voice, 1 – 100 |
| **Spray** (macro 4) | random scatter of the position (up to ±0.5 s) and of the grain timing |
| **Grain Pitch** | transposes the grains (±24 semitones) on top of the played note |
| **Reverse Chance** | share of grains that play backwards |
| **Stereo Spread** | random panning of the grains |
| **Scan Speed** | moves the position while a note is held (1 = real time, negative = backwards) |
| **Source** | *Sample* (the loaded file or the built-in pad texture) or *Tape* (a tape region, see below) |
| **Tone** | low-pass brightness; the mod envelope and the LFO target *Timbre* move it |

The played note transposes relative to **C4 (MIDI 60)**: C4 plays the source at its original pitch.
With *Source = Tape* the engine granulates the tape region the tape machine hands over through the
`IAudioRegion` interface; without one it falls back to the sample.

## Vocal

Formant synthesis of a singing voice: a glottal pulse plus breath noise drives five formant
resonators.

| Control | What it does |
|---|---|
| **Vowel** (macro 1) | morphs continuously A (0) – E (1) – I (2) – O (3) – U (4) |
| **Formant Shift** (macro 2) | moves all formants ±12 semitones (smaller or larger voice) |
| **Breath** (macro 3) | breath noise instead of a clear tone |
| **Tension** (macro 4) | lax and dark (0) to pressed and bright (1) |
| **Vibrato Depth / Rate** | pitch vibrato up to ±½ semitone, 0.5 – 10 Hz |
| **Jitter** | slow random pitch movement for a natural voice |

The mod envelope and the LFO target *Timbre* move the vowel position.

### User formants (p20)

The Vocal engine's Synth page has the soft keys **IMPORT**, **USER** and **CLEAR**, like the Wavetable
engine ([user wavetables](engines/wavetable.md#user-wavetables-audio-to-wavetable-p20)). An imported sound
(file, tape track, live input) is analysed by linear prediction: about every frame of the sound gives
five formants (frequency, level, bandwidth – the five strongest peaks of the spectral envelope between
150 Hz and 5 kHz), up to 64 formant frames. With **USER** on (`pN.usertable.vocal`) *Vowel* 0 … 4 moves
through these formant tracks from the start to the end of the sound instead of A – E – I – O – U;
formant shift, breath, tension and vibrato work as before. The display draws the five tracks with the
current vowel position. The material is saved like a user wavetable (library, presets, projects).

## Sampler

Plays samples across the keyboard, either as **zones** (multisampling) or as **slices**.

| Control | What it does |
|---|---|
| **Start** (macro 1) | start offset inside the sample or slice |
| **Tone** (macro 2) | low-pass brightness (mod envelope and LFO *Timbre* move it) |
| **Loop Start / Loop End** (macros 3, 4) | loop region as a share of the sample (when the file has no loop of its own) |
| **Root Key** | the note that plays a zone at its original pitch (zones may bring their own root key) |
| **Loop** | *Off* (one shot), *Forward*, *Ping-Pong* |
| **Reverse** | plays backwards |
| **Mode** | *Zones* or *Slices* |
| **Slice Sensitivity** | how small an attack still starts a new slice |
| **First Slice Key** | the key of slice 1 (default C2 = 36); each next key plays the next slice |

**Zones:** each zone has a key range, a velocity range, a root key and optional loop points. Loop
points stored in a WAV or AIFF file are used automatically.

**Slices:** Ohpe finds the attacks (transients) of the first sample and cuts it into slices, at most
64. Every slice plays on its own key at its original pitch and stops where the next slice begins.
Changing *Slice Sensitivity* re-slices the sample.

## Drum kit

24 pads, played on **MIDI channel 10** (pad 1 = `drum.base`, default C2 = 36, then one pad per
semitone). The kit plays on mixer channel 5 (source *Drum*).

**Real recordings first.** The factory kit is a recorded acoustic kit (CC0 recordings of a real drum
set, see [THIRD-PARTY.md](../../THIRD-PARTY.md)): kick, snare, rimshot, cross-stick, ghost note,
closed / half-open / pedal / open hi-hat, three toms, ride, ride bell, crash, sizzle crash, cowbell,
tambourine, shaker, claves, conga and bongo. Close, room-style and overhead microphones are mixed
per piece, and every hit is humanised a little (±4 cents, ±0.4 dB); soft hits are darker, like a
real drum. Load your own sample onto any pad to replace the recording; clearing it brings the
recording back.

**Synth models.** Every pad also has a synthetic drum model, used when the pad has no sample (pad 5,
the clap) or when **Source** is set to *Synth*:

| Model | Sound |
|---|---|
| Kick | sine with a pitch sweep and a click |
| Snare | two tuned body tones plus filtered noise |
| Hat | six inharmonic square oscillators and noise, high-passed |
| Clap | three noise bursts and a tail through a band-pass |
| Tom | sine with a short pitch drop and a stick noise |
| Perc | two-operator FM blip (rim, bell, woodblock) |

Per pad: **Source** (*Sample* / *Synth*), **Synth Model**, **Tune** (±24 semitones), **Decay**,
**Level**, **Pan**, **Tone** and **Choke Group** (0 = none, 1 – 8). *Decay* and *Tone* are centred:
0.5 is the natural sound; with samples, *Decay* below 0.5 shortens the hit and *Tone* below 0.5
darkens it. Every pad is monophonic, and pads of the same choke group cut each other – the closed,
half-open, pedal and open hi-hats (pads 6, 7, 8, 17) share group 1. Note-offs are ignored.

## Factory instruments and drum kits (0.4)

* **Instrument** (`pN.sampler.inst`): the Sampler plays one of ten recorded instruments (kalimba,
  marimba, vibraphone, glockenspiel, harp, harpsichord, recorder, singing glass, zither, bowed metal)
  instead of its built-in keys tone while no sample of your own is loaded. Recorder, singing glass and
  bowed metal have loop points: set *Loop* to *Forward* for endless notes.
* **Instrument** (`pN.grain.inst`): the Grain engine granulates one note of the chosen instrument
  instead of its built-in texture (a loaded sample or the tape source still wins). For plucked and
  struck instruments keep *Position* near the start, where the note is loudest.
* **Keyboard Instrument** (0.8, `pN.sampler.inst2` / `pN.grain.inst2`): Grand Piano, Upright Piano
  (recordings, every few semitones, the grand with two velocity layers), Tine Piano and Reed Piano
  (Ohpe's own modelled electric pianos, two velocity layers: soft notes ring like bells, hard notes
  bark). When not *Off* it replaces *Instrument*. In the Grain engine the pianos make breathing
  piano clouds (keep *Position* near the start).
* **Drum Kit** (`drum.kit`): Studio (the original kit), Tight, Room, Lo-Fi, Hybrid or Boom (0.5:
  synthesised deep kick / sub kit). All kits use the same pad layout; your own pad samples always win.

Details and sources: [FACTORY-SOUNDS.md](FACTORY-SOUNDS.md), [THIRD-PARTY.md](../../THIRD-PARTY.md).

## Loading samples

Samples are loaded from disk in the plug-in (WAV, AIFF, FLAC and Ogg Vorbis): one sample for the
whole keyboard, additional multisample zones, the grain source, or a sample per drum pad (it
replaces the pad's factory recording). Ohpe saves the **paths** of the loaded files with the project, not the
audio; when a project is opened again the files are reloaded. Files that were moved or deleted are
skipped and that engine or pad uses its built-in sound or factory recording.

**Bounce to engine** (p23): the BOUNCE key puts a tape track, the tape mix or a few bars of the live input straight
into the Grain or Sampler engine – by default as a seamless endless loop (the grains wrap round the seam, a held
Sampler note sustains it). Bounces are WAV files in the bounce library, so they are reloaded like any other sample.
See [Bounce to engine](BOUNCE.md).
