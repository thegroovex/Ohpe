# Ohpe architecture

*English. Deutsche Kurzfassung: [docs/de/ARCHITEKTUR.md](de/ARCHITEKTUR.md).*

Ohpe is split into a framework-free core and a thin JUCE shell. Everything that makes sound or
holds state lives in the core and is tested in seconds without JUCE; the shell only adapts it to
plug-in hosts, the standalone app and the screen.

```
            ┌────────────────────────────── ohpe_plugin (JUCE) ───────────────────────────────┐
 host /     │  OhpeProcessor  ── APVTS (1 host parameter per registry entry, same order)      │
 audio dev ─┤     │  buses: Main + Out 1..8 (stereo), MIDI in                                  │
            │     │  MIDI learn bridge, state <-> JSON                                          │
            │  OhpeEditor (src/ui) · Standalone app + AudioSettingsPage (src/standalone)       │
            └─────┼───────────────────────────────────────────────────────────────────────────┘
                  ▼
            ┌───────────────────────────── ohpe_core (C++17, no JUCE) ────────────────────────┐
            │ Core ── ParamRegistry ── ParamSnapshot (per block, + sequencer locks)            │
            │   ├── Part p1 ── engines: FM, Analog (+6 reserved slots)  ← ISynthEngine         │
            │   ├── StepSequencer                                         ← ISequencer         │
            │   ├── Mixer: 8 ch + master, sends A (Delay) / B (Reverb)    ← IEffect            │
            │   │          output routing: Main or Out 1..8                                    │
            │   ├── MidiMap (message thread) + MidiMapper (audio thread, lock-free)            │
            │   └── state / preset JSON (StateSerializer)                                      │
            │ tape/ITapeTrack.h: interface for the phase-2 tape machine                        │
            └─────────────────────────────────────────────────────────────────────────────────┘
```

## Folders

| Folder | Content | Depends on |
|---|---|---|
| `src/core/model` | `ParamDescriptor`, `ParamRegistry`, `ParamSnapshot`, `Events` (MIDI event, event list, transport), `SpscQueue`, `StateSerializer` | nlohmann/json |
| `src/core/dsp` | building blocks: ADSR, LFO, filters (SVF, ladder, one-pole), biquad, band-limited oscillators, utilities | – |
| `src/core/engines` | `ISynthEngine`, `VoiceAllocator`, `PolyEngine<Voice>` + `VoiceBase`, `AnalogEngine`, `FmEngine`, `EngineFactory`, `Part` | dsp, model |
| `src/core/fx` | `IEffect`, `DelayEffect`, `ReverbEffect`, `Limiter` | dsp |
| `src/core/mixer` | `Mixer` (channels, EQ, sends, master, multi-out routing) | fx, dsp, model |
| `src/core/seq` | `ISequencer`, `StepSequencer` | model, dsp |
| `src/core/midi` | `MidiMap`, `MidiMapper` (learn, lock-free table) | model |
| `src/core/tape` | `ITapeMachine` interface (phase 2) | model |
| `src/core/OhpeCore.*`, `ParameterLayout.cpp` | the instrument: owns everything, renders blocks, canonical parameter order | all of the above |
| `src/plugin` | `OhpeProcessor`, `ParameterBridge` (registry → APVTS), `Localisation` (EN/DE) | JUCE, core |
| `src/ui` | editor, look and feel, palette, pictograms, display, widgets, mode pages | JUCE, core, plugin |
| `src/standalone` | custom standalone app, audio settings page with multi-out routing | JUCE |
| `tests/core` | doctest unit tests for the core (no JUCE) | core |
| `tests/plugin` | processor / editor tests and a VST3 host test (JUCE) | plugin |
| `tools` | `ParamDump` (parameter table), `Screenshot` (renders the editor to PNG) | |

## Threading and real-time rules

* **Message thread**: building the registry, `prepare()`, editing MIDI maps and patterns,
  saving / loading state, the UI.
* **Audio thread**: `Core::process()` and everything it calls. Rules: no allocation, no locks,
  no I/O, no exceptions, no unbounded loops. Buffers are sized in `prepare()`.
* Data crossing threads:
  * parameter values – `std::atomic<float>` in `ParamRegistry` (plain values);
  * MIDI map – compiled into one of two fixed tables, published with an atomic index
    (`MidiMapper::publish`); learn results and parameter changes travel through `SpscQueue`s;
  * sequencer pattern – each step packed into atomics (`StepSequencer::setStep`);
  * on-screen notes – `Core::queueNote` (SPSC queue) or the plug-in's `MidiKeyboardState`;
  * meters / play position / voice count – atomics.
* `Core::process()` renders in chunks of at most 256 samples and splits chunks at every MIDI
  event and parameter lock, so notes are sample-accurate for any host block size.

## Parameters

* One table, `ParamRegistry`, holds every automatable and MIDI-mappable value. The plug-in
  creates exactly one host parameter per entry, in the same order, with the registry ID as
  the JUCE `ParameterID` (version hint 1). MIDI maps, presets, states and the UI all use the
  same string IDs.
* IDs are lowercase, dot-separated: `tempo`, `seq.run`, `p1.engine`, `p1.amp.a`,
  `p1.analog.cutoff`, `p1.fm.op3ratio`, `mix.ch4.senda`, `mix.master.vol`, `fxa.delay.time`,
  `fxb.reverb.size`, `seq.step.rate`. The full table: `ohpe_param_dump --markdown`.
* **Append-only.** `src/core/ParameterLayout.cpp` is the canonical order. Never remove,
  rename or reorder an entry, never change its range or meaning, never insert in the middle,
  never reorder the choices of a choice parameter (only append). New parameters go to the
  bottom section of `buildParameterLayout()`. `tests/core/golden/param_ids_v1.txt` lists the
  released IDs and the test *"parameter IDs are append-only"* checks it is a prefix of the
  current list. When you append parameters, append their IDs to the golden file
  (`ohpe_param_dump --ids` prints the list) – never regenerate or edit existing lines.
* Engines, effects and sequencers describe their own parameters with *local* IDs
  (`getParamDescriptors()`); the owner adds the prefix (`p1.analog.` …). The first four
  descriptors of an engine are its macros (the four encoders).
* Choice lists that will grow are fixed in size from the start (`EngineType` already has all
  eight engines; `mix.chN.src` is an integer 0–31 with reserved numbers).
* Values are stored as plain values; normalisation (`toNormalised` / `fromNormalised`) uses
  the same skew formula as `juce::NormalisableRange`.
* UI navigation (mode keys, pages) is not a parameter; everything that changes the sound or
  the transport is.

## Signal flow per block

1. Registry → `ParamSnapshot` (plus active sequencer locks) → parts, mixer, sequencer settings.
2. Transport: host position (plug-in) or internal clock (`tempo`, `seq.run`; standalone). In
   a host the sequencer runs while the host plays *and* `seq.run` is on.
3. The sequencer produces note events and parameter locks.
4. MIDI from the host passes the `MidiMapper` (mapped / learned controls are consumed), is
   merged with the on-screen keyboard and the sequencer and sorted by time.
5. Chunks: events are dispatched (phase 1: all channels → part 1), part 1 renders into its
   source buffer, the mixer mixes the sources into the output pairs.
6. Mixer: per channel source → EQ → volume / pan / mute / solo → output (Main via master, or
   aux 1–8 directly, post-fader) + post-fader sends A/B → delay / reverb returns → master
   volume → limiter → Main. A channel routed to a disabled aux bus (plug-in without that bus,
   or a standalone device with too few channels) plays on Main instead.

## State format

`Core::saveState()` writes JSON: `{"format":"ohpe-state","version":1,"params":{id: value},
"sections":{"midimap":…,"sequencer":…,"ui":…}}`. Unknown IDs are ignored, missing IDs return
to their defaults – old states open in new versions and vice versa. Presets
(`state::savePreset`) store all parameters of a prefix (e.g. `p1.`) and can be loaded into
another prefix. MIDI map files use `{"format":"ohpe-midimap",…}` (extension `.ohpemidi`).

## Interfaces for phase 2

### Adding a synth engine (example: Wavetable)

1. `src/core/engines/WavetableEngine.{h,cpp}`: derive the voice from `VoiceBase` and the engine
   from `PolyEngine<WavetableVoice>`; implement `info()` (ID must be `"wavetable"` – the
   `EngineType` slot already exists), `getParamDescriptors()` (first four = macros, local IDs,
   English and German names) and `setParam()`. Use amp / mod envelopes, LFO and glide from
   `VoiceBase::controlTick()`; map `LfoDest::Timbre` and `VoiceCommon::modEnvAmount` to the
   engine's own brightness control.
2. `EngineFactory.cpp`: return the engine in `createEngine()` and set `implemented = true`.
3. `ParameterLayout.cpp`: append `parts[0]->registerEngineParams (reg, EngineType::Wavetable);`
   at the **bottom** and append the new IDs to `tests/core/golden/param_ids_v1.txt`.
4. Add the source to `src/core/CMakeLists.txt`.
5. Tests in `tests/core/WavetableEngineTests.cpp` (add it to `tests/core/CMakeLists.txt`):
   silence when idle, pitch accuracy in cents, finite / no denormals at extreme settings,
   release to silence, the macros change the sound. The generic contract test in
   `VoiceTests.cpp` picks the engine up automatically.
6. UI: the engine key enables itself (`implemented`), the Synth mode lists the parameters, and
   `src/ui/Pictograms.cpp` has a placeholder character – make it react to the macros.

### Adding a sequencer (Euclid, Arp, Orbit …)

Implement `ISequencer` in `src/core/seq/`, keep pattern data lock-free (see `StepSequencer`),
register its settings with a new prefix (`seq.euclid.*`) at the bottom of
`ParameterLayout.cpp`, call it from `Core::process()` next to the step sequencer (its events
are merged and sorted with the others) and save its pattern as a new state section. Test
timing in samples, probability with the deterministic RNG, JSON round-trip and stop behaviour.

### Adding an effect

Implement `IEffect` in `src/core/fx/`. Send / master effects are registered by the mixer
(`fxa.<id>.*`, `fxb.<id>.*`, new slots such as `fxm.<id>.*`), always appended. A switchable
effect type for a slot needs an append-only type list and its own parameter block per effect.
Test: impulse response finite, tail decays, no denormals, parameters at their extremes.

### Tape (phase 2)

`src/core/tape/ITapeTrack.h` defines `ITapeMachine` (8 stereo tracks, transport, lift / drop /
cut / splice, undo / redo). Its tracks feed the mixer sources `source::tape1 … tape1 + 7`.
Recording external audio needs an input bus in the plug-in (decision for phase 2).

## Testing

* `tests/core` – fast, JUCE-free: `cmake -S . -B build-core -G Ninja -DOHPE_BUILD_PLUGIN=OFF`,
  then `cmake --build build-core && ctest --test-dir build-core`.
* `tests/plugin` – processor, editor, localisation, VST3 host test.
* pluginval at strictness 5 on the VST3.
* `scripts/test-all.sh` runs all of it; see [CONTRIBUTING.md](../CONTRIBUTING.md).

## Phase 2: tape, mixer dynamics, FX slot (0.2.0)

Implemented in `src/core/tape/TapeMachine.*` (implements `ITapeMachine`; `process()` now takes
`TapeInputs` – input bus, parts, drum), `src/core/fx/` (`Compressor`, `TapeSaturator`,
`InsertEffects` – chorus, phaser, bitcrush, filter drive – and `InsertSlot`), the mixer
(compressor-lite per channel, master compressor, tape stage, limiter ceiling, direct feeds for
tape direct outs) and `src/plugin/TapeIO.*` (tape audio in the plug-in state, offline bounce).
Decisions:

* **Input bus:** the plug-in has one stereo / mono input bus "Input", disabled by default;
  `Core::setAudioInput()` hands it to the core for the next block.
* **Core hook:** `Core::process()` calls `renderTapeChunk()` once per chunk after the parts
  rendered: part FX slots → tape (record + playback) → sources `tape1 … tape1 + 7` and direct
  outs. Parameters are pulled with the other modules (`pullPhase2Params()`).
* **Message-thread duty:** `TapeMachine::housekeeping()` (plug-in timer, 20 Hz) refills the
  block pool for recording and turns finished recording passes into undo steps.
* **Persistence:** tape audio is embedded in the plug-in state after the JSON (sparse, float32);
  no external files. Undo history is not saved.

Guide: [en/TAPE-MIXER-FX.md](en/TAPE-MIXER-FX.md) · parameters: [PARAMETERS.md](PARAMETERS.md).
