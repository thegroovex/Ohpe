# Robustness and soak tests

Ohpe – designed by groovex. Deutsch: [DAUERTEST.md](../de/DAUERTEST.md)

## Automated (every CI run)

The core tests (`ohpe_core_tests`, no audio device needed) check on Linux, macOS and Windows:

| Test | What it proves |
|---|---|
| `robustness: damaged …` | Truncated, corrupted (flipped / inserted / deleted / repeated bytes), oversized (4 MB strings, nesting 50 000 deep) and wrong-type documents never crash, hang or throw: session states and their sections (MIDI map, sequencers, morph, motion, harmony, tape settings, UI), part / project presets, `.ohpetape` files and the tape audio in a session, `.ohpewt` files (also base64), `pack.json` and pack plans, the content registry, update settings and manifests, `settings.json`, the preset library's user data, sample and user-table assignments, breeding sessions. Loads that succeed leave every parameter inside its range and the core renders finite audio. |
| `stress: every sample rate and buffer size …` | 44.1 / 48 / 88.2 / 96 / 192 kHz, blocks of 1, 7, 333, 4096 samples and variable blocks (including empty ones), all eight output pairs: finite, bounded, audible, no denormal output. |
| `stress: silence tails …` | 60 s of reverb / delay tail after the notes: no denormals, and the last seconds cost no more than the first ones. |
| `stress: NaN / Inf at the input …` | NaN / Inf / huge input samples never reach the outputs or the tape; the engine plays normally afterwards. |
| `stress: random patches …` | Every parameter anywhere in its range: no NaN / Inf on the outputs. |
| `stress: soak …` | 10 minutes of audio rendered offline with sequencers, automation and a preset change every minute: finite, bounded, no denormals, the output guard never had to step in, no voice left sounding afterwards. |
| `robustness: damaged host states …` (plug-in tests) | `setStateInformation` with truncated, corrupted, oversized and wrong-type blobs never throws; the plug-in keeps processing. |

The fuzz mutations are seeded and reproducible. Locally:

```
OHPE_FUZZ_ITERATIONS=20000 OHPE_FUZZ_SEED=42 build-core/tests/core/ohpe_core_tests -tc="robustness:*"
OHPE_SOAK_MINUTES=480 build-core/tests/core/ohpe_core_tests -tc="stress: soak*"      # 8 hours of audio, offline
```

Protections in the engine: every output block is checked; a NaN / Inf silences that block and resets the sound
generators and effects (counted, never expected). The audio input is cleaned (NaN / Inf → 0, clamped) before anything
uses it. JSON documents nested deeper than 256 levels are refused before parsing.

## Manual 8-hour soak (before a release)

Run on each platform you ship (one machine per platform is enough), with the release build.

1. **Prepare.** Fresh user folder or note the current one. Close other audio programs. Audio device at 48 kHz, buffer
   128 (the standalone app's Audio Settings). Note the time, the version and the machine.
2. **Standalone, sound.** Load the factory project *Ambient Drift*, start PLAY. Turn on the step sequencer on part 1 and
   the arpeggiator on part 2 with a held chord (latch). Record 30 s onto tape track 1 and loop it.
3. **Automation.** Map one encoder to a filter cutoff and leave a slow motion-recorder lane running on it (the motion
   view), so a parameter moves for the whole run.
4. **Let it run for 8 hours.** Every 2 hours, without stopping playback: note the CPU meter (average and the slowest
   block from its tooltip), the app's memory (Activity Monitor / Task Manager / `top`), and listen for 30 s (no clicks,
   no dropouts, no silence, no growing noise). Change the preset once (library, any project) and back.
5. **Sample-rate change while running.** Once, switch the device to 44.1 kHz and back to 48 kHz, and the buffer to 64
   and back. Playback must continue cleanly.
6. **Quit normally.** Start again: Ohpe opens the session without a notice. Then end the app forcefully (kill the
   process) and start again: the *last session did not end properly* notice appears; RESTORE brings the session back.
7. **Plug-in.** Repeat steps 2–4 for 2 hours in a plug-in host with the VST3 (and AU on macOS), including saving and
   reopening the host project once.

**Pass:** no crash or hang, no audible artefact, CPU average within ±20 % of the first reading, memory grows by less
than 50 MB after the first hour, the notice behaves as described. Write the readings and the result into the release
issue (date, version, platform, device, buffer, readings every 2 hours).
