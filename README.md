# Ohpe

*designed by groovex* · [Deutsch](README.de.md)

**Ohpe** is a compact, playful synthesizer workstation for your computer: eight synth engines on
four parts, a drum kit, four sequencers, an eight-track tape machine, an eight-channel mixer with
dynamics and effects, and a calm, minimalist interface built around four encoders and a keyboard. It runs as a **standalone app** (macOS, Windows,
Linux) and as a **VST3** and **Audio Unit** plug-in.

![Ohpe – analog engine](docs/images/ohpe-analog.png)

> **Status: phase 4 (0.4).** Eight engines, four parts, drum kit, step / Euclid / arpeggiator /
> Orbit sequencers, tape machine, mixer dynamics, FX slot, MPE and MIDI clock sync, preset library,
> pro view and direct manipulation – now with the **factory sounds**: 120 part presets, 7 demo
> projects, five drum kits and ten recorded instruments. Not released yet: build it yourself or
> grab the unsigned builds from a manual CI run.

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
| **Sampler engine** | key zones or automatic slices from WAV / AIFF / FLAC files, loop modes, built-in keys sound and ten recorded factory instruments (kalimba, marimba, vibraphone, harp, harpsichord, recorder …) |
| **Drum kit** | 24 pads on MIDI channel 10: five recorded factory kits (Studio, Tight, Room, Lo-Fi, Hybrid – CC0), own samples or synth models, choke groups |
| **Factory sounds** | 120 part presets across all engines and categories, level-matched; 7 demo projects using all four parts, the sequencers and the drum kit – see [Factory sounds](docs/en/FACTORY-SOUNDS.md) |
| **Four parts** | each with its own engine, envelopes, LFO, MIDI channel, scale / chord mode and MPE |
| **Shared per voice** | amp and modulation envelopes, LFO with several destinations, glide, 16-voice polyphony |
| **Sequencers** | step sequencer (up to 64 steps, probability, ratchets, parameter locks, 16 patterns, song mode), Euclid, arpeggiator and Orbit (polymeter), each with a target part – step, Euclid and Orbit can also play the drum kit |
| **Tape machine** | eight stereo tracks: record / overdub / punch from the input, parts or drums, varispeed, reverse, loop, tape character, undo, export as WAV master and stems |
| **Mixer** | eight channels with EQ, compressor, volume, pan, mute / solo, two sends (A: delay, B: reverb), master compressor, tape saturation and limiter; FX slot on part 1 (chorus, phaser, bitcrush, filter drive, delay, reverb) |
| **Sync** | host transport in the plug-in; MIDI clock in / out and network tempo sync (Link) in the standalone app |
| **Multi-out** | every channel can go to the main output or its own stereo output (8 aux outputs) – in the plug-in and in the standalone app |
| **MIDI** | per-part channels, MPE, every control is MIDI-learnable (right-click → MIDI Learn), maps can be exported / imported; all parameters are automatable |
| **Standalone app** | choose audio system, device, sample rate and buffer size, latency display, multi-out routing to device channels, MIDI inputs |
| **Preset library** | one LIBRARY key: categories (Bass, Keys, Pads, Leads, Plucks, Drums, FX, Arp, Textures, Vocal), engine filter, search, favourites, star ratings, sorting, factory and user banks, preview by selecting, part and whole-project presets, previous / next from MIDI |
| **Direct manipulation** | drag the value bars on the display, Shift / Cmd / Ctrl for fine control, double-click to reset; pro view with every parameter of a mode at once |
| **Languages** | English and German, switchable at any time |

## Screenshots

| | |
|---|---|
| ![Preset library](docs/images/ohpe-library.png) | ![Pro view](docs/images/ohpe-pro-view.png) |
| Preset library | Pro view (FM engine, part 2) |
| ![Tape page](docs/images/ohpe-tape.png) | ![Step parameter locks](docs/images/ohpe-step-locks.png) |
| Tape page | Step parameter locks |
| ![Drum kit page](docs/images/ohpe-drum.png) | ![FM engine](docs/images/ohpe-fm.png) |
| Drum kit page | FM engine |
| ![Mixer](docs/images/ohpe-mixer.png) | ![German interface](docs/images/ohpe-analog-de.png) |
| Mixer | German interface |

## Playing Ohpe

* **Part selector** (1–4, top) chooses the part you edit and play; **engine keys** (left) pick its
  sound engine.
* **Mode keys** (right) switch between Synth, Envelope, LFO, Mixer, Effects, Sequencer, Tape and
  Drum.
* The **four encoders** always control what the display shows; in Synth mode they are the
  engine's four macros. The **bars on the display** can be dragged directly (Shift / Cmd / Ctrl =
  fine). Double-click resets a value, right-click opens MIDI learn.
* **LIBRARY** opens the preset library, **PRO** shows every parameter of the mode at once.
* **Keyboard**: play with the mouse or any MIDI keyboard; the octave keys shift it.
* **Sequencer**: tap steps to switch them on, Shift-tap a step to edit it and lock parameters on
  it, press Play (in a host, the sequencer follows the host transport while Play is on).
* Drop an audio file on the instrument to load it into the Sampler / Grain engine or onto a drum pad.

Full guide: [playing the interface](docs/en/USER-INTERFACE.md).

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
* [Playing the interface](docs/en/USER-INTERFACE.md) – display, parts, pro view, step locks, tape and drum pages
* [Factory sounds](docs/en/FACTORY-SOUNDS.md) – every preset by category, drum kits, recorded instruments
* [Preset library](docs/en/PRESETS.md) – browsing, saving, MIDI, file format
* [Sequencers, parts and scale mode](docs/en/SEQUENCERS.md)
* [MIDI guide](docs/en/MIDI.md) – channels, MPE, clock, sync
* [Tape machine, mixer dynamics and FX slot](docs/en/TAPE-MIXER-FX.md)
* [Parameters](docs/PARAMETERS.md) – every host parameter ID
* [Architecture](docs/ARCHITECTURE.md) – core, plug-in shell, threading, parameters, extension points
* [Design language](docs/DESIGN.md) – colours, characters, components
* [Contributing](CONTRIBUTING.md) – tests, rules, workflow

## License

Ohpe is free software under the [GNU Affero General Public License v3.0 or later](LICENSE).
Built with [JUCE](https://juce.com) (AGPLv3), [nlohmann/json](https://github.com/nlohmann/json)
(MIT) and [doctest](https://github.com/doctest/doctest) (MIT); the standalone app can include the
Link library (GPL-2.0-or-later) for network tempo sync. Factory drum recordings: CC0. Font: Space
Grotesk (SIL OFL 1.1). Details in [THIRD-PARTY.md](THIRD-PARTY.md).
