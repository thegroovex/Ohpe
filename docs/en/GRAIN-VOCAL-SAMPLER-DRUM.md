# Grain, Vocal, Sampler and the drum kit

*English · Deutsch: [../de/GRAIN-VOKAL-SAMPLER-DRUM.md](../de/GRAIN-VOKAL-SAMPLER-DRUM.md)*

Phase 2 adds three engines for the synth part (selected with **Engine**, `p1.engine`) and the
**drum kit**, which has its own mixer channel. All of them sound right away: without loaded samples
the Grain and Sampler engines use a built-in harmonic tone and the drum kit is fully synthetic.
Ohpe ships no audio samples. Every parameter is listed in [PARAMETERS.md](../PARAMETERS.md).

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
| **Source** | *Sample* (the loaded file or the built-in tone) or *Tape* (a tape region, see below) |
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
semitone). The kit plays on mixer channel 5 (source *Drum*). Each pad is a synthetic drum model or a
sample:

| Type | Sound |
|---|---|
| Kick | sine with a pitch sweep and a click; *Tone* adds click and drive |
| Snare | two tuned body tones plus filtered noise; *Tone* sets the noise colour |
| Hat | six inharmonic square oscillators and noise, high-passed; *Decay* makes it closed or open |
| Clap | three noise bursts and a tail through a band-pass |
| Tom | sine with a short pitch drop and a stick noise |
| Perc | two-operator FM blip (rim, bell, woodblock); *Tone* sets the modulator ratio |
| Sample | the pad's sample at the pad's tune; *Decay* = 1 plays it to the end |

Per pad: **Tune** (±24 semitones), **Decay**, **Level**, **Pan**, **Tone** and **Choke Group**
(0 = none, 1 – 8). Every pad is monophonic, and pads of the same choke group cut each other – in the
factory kit the closed, open and pedal hats (pads 6 – 8) share group 1. Note-offs are ignored.

## Loading samples

Samples are loaded from disk in the plug-in (WAV, AIFF, FLAC and Ogg Vorbis): one sample for the
whole keyboard, additional multisample zones, the grain source, or a sample per drum pad (the pad
switches to type *Sample*). Ohpe saves the **paths** of the loaded files with the project, not the
audio; when a project is opened again the files are reloaded. Files that were moved or deleted are
skipped and that engine or pad uses its built-in sound.
