# Ohpe

*designed by groovex* · [Deutsch](README.de.md)

**Ohpe** is a compact, playful synthesizer workstation for your computer: eight synth engines on
four parts, a drum kit, four sequencers, an eight-track tape machine, an eight-channel mixer with
dynamics and effects, and a calm, minimalist interface built around four encoders and a keyboard. It runs as a **standalone app** (macOS, Windows,
Linux) and as a **VST3** and **Audio Unit** plug-in.

![Ohpe – analog engine](docs/images/ohpe-analog.png)

> **Status: phase 2 (0.2).** Eight engines, four parts, drum kit, step / Euclid / arpeggiator /
> Orbit sequencers, tape machine, mixer dynamics, FX slot, MPE and MIDI clock sync. Not released
> yet: build it yourself or grab the unsigned builds from a manual CI run.

## Features

| | |
|---|---|
| **Analog engine** | two band-limited oscillators (shape morph, detune, octave, mix), sub, noise, ladder and state-variable filters (LP / BP / HP), drive, key tracking |
| **FM engine** | six operators, eight algorithms with smooth morphing between neighbours, feedback, per-operator ratio and level |
| **Wavetable engine** | six built-in tables of eight frames with smooth morphing, warp modes, unison, sub oscillator |
| **Pluck engine** | waveguide string and tuned mallet resonators for plucked and struck sounds |
| **Cluster engine** | up to seven detuned oscillators per key as unison or chord shapes, slow drift, resonant low-pass |
| **Grain engine** | granular synthesis from a loaded sample, the tape or a built-in texture |
| **Vocal engine** | formant voice with vowels, breath, vibrato and jitter |
| **Sampler engine** | key zones or automatic slices from WAV / AIFF / FLAC files, loop modes, built-in keys sound |
| **Drum kit** | 24 pads on MIDI channel 10: factory recordings (CC0), own samples or synth models, choke groups |
| **Four parts** | each with its own engine, envelopes, LFO, MIDI channel, scale / chord mode and MPE |
| **Shared per voice** | amp and modulation envelopes, LFO with several destinations, glide, 16-voice polyphony |
| **Sequencers** | step sequencer (up to 64 steps, probability, ratchets, parameter locks, 16 patterns, song mode), Euclid, arpeggiator and Orbit (polymeter), each with a target part |
| **Tape machine** | eight stereo tracks: record / overdub / punch from the input, parts or drums, varispeed, reverse, loop, tape character, undo, export as WAV master and stems |
| **Mixer** | eight channels with EQ, compressor, volume, pan, mute / solo, two sends (A: delay, B: reverb), master compressor, tape saturation and limiter; FX slot on part 1 (chorus, phaser, bitcrush, filter drive, delay, reverb) |
| **Sync** | host transport in the plug-in; MIDI clock in / out and network tempo sync (Link) in the standalone app |
| **Multi-out** | every channel can go to the main output or its own stereo output (8 aux outputs) – in the plug-in and in the standalone app |
| **MIDI** | per-part channels, MPE, every control is MIDI-learnable (right-click → MIDI Learn), maps can be exported / imported; all parameters are automatable |
| **Standalone app** | choose audio system, device, sample rate and buffer size, latency display, multi-out routing to device channels, MIDI inputs |
| **Languages** | English and German, switchable at any time |

## Screenshots

| | |
|---|---|
| ![FM engine](docs/images/ohpe-fm.png) | ![Step sequencer](docs/images/ohpe-sequencer.png) |
| FM engine | Step sequencer |
| ![Mixer](docs/images/ohpe-mixer.png) | ![German interface](docs/images/ohpe-analog-de.png) |
| Mixer | German interface |

## Playing Ohpe

* **Engine keys** (left) pick the sound engine of the part.
* **Mode keys** (right) switch between Synth, Envelope, LFO, Mixer, Effects and Sequencer.
* The **four encoders** always control what the display shows; in Synth mode they are the
  engine's four macros. Double-click resets a value, right-click opens MIDI learn.
* **Keyboard**: play with the mouse or any MIDI keyboard; the octave keys shift it.
* **Sequencer**: tap steps to switch them on, press Play (in a host, the sequencer follows the
  host transport while Play is on).

## Building

Requirements: CMake ≥ 3.22, a C++17 compiler (Xcode 15+, Visual Studio 2022, GCC 11+ / Clang 14+),
on Linux the JUCE dependencies (ALSA, FreeType, fontconfig, X11, Mesa). JUCE 8.0.15 is downloaded
automatically, or pass a local checkout with `-DOHPE_JUCE_PATH=/path/to/JUCE`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The app and plug-ins are in `build/Ohpe_artefacts/Release/` (`Standalone`, `VST3`, `AU` on macOS).
For a universal macOS build add `"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"`; to copy the plug-ins
into the system folders after building add `-DOHPE_COPY_PLUGIN=ON`.

Run all tests with `scripts/test-all.sh` – see [CONTRIBUTING.md](CONTRIBUTING.md).

## Documentation

* Engines: [Wavetable](docs/en/engines/wavetable.md), [Pluck](docs/en/engines/pluck.md),
  [Cluster](docs/en/engines/cluster.md), [Grain, Vocal, Sampler and drum kit](docs/en/GRAIN-VOCAL-SAMPLER-DRUM.md)
* [Sequencers, parts and scale mode](docs/en/SEQUENCERS.md)
* [MIDI guide](docs/en/MIDI.md) – channels, MPE, clock, sync
* [Tape machine, mixer dynamics and FX slot](docs/en/TAPE-MIXER-FX.md)
* [Parameters](docs/PARAMETERS.md) – every host parameter ID
* [Architecture](docs/ARCHITECTURE.md) – core, plug-in shell, threading, parameters, extension points
* [Contributing](CONTRIBUTING.md) – tests, rules, workflow

## License

Ohpe is free software under the [GNU Affero General Public License v3.0 or later](LICENSE).
Built with [JUCE](https://juce.com) (AGPLv3), [nlohmann/json](https://github.com/nlohmann/json)
(MIT) and [doctest](https://github.com/doctest/doctest) (MIT); the standalone app can include the
Link library (GPL-2.0-or-later) for network tempo sync. Factory drum recordings: CC0. Font: Space
Grotesk (SIL OFL 1.1). Details in [THIRD-PARTY.md](THIRD-PARTY.md).
