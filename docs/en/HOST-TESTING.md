# Host testing

Ohpe – designed by groovex. Deutsch: [HOST-TESTS.md](../de/HOST-TESTS.md)

## Automated host matrix (every CI run)

`ohpe_hosttest` is a small headless host built on JUCE's own plug-in hosting. It loads the **built** plug-in
bundle – the VST3 on Linux, macOS and Windows, and the installed Audio Unit on macOS – exactly as a DAW does, and
fails CI on a crash, a hang (watchdog per section), a NaN / Inf / runaway sample (> +12 dBFS) or a mismatch.

| Section | What it checks |
|---|---|
| `basic` | One instrument in the bundle, MIDI in, 9 stereo output buses + 1 input bus, editor present. Every parameter ID of `tests/core/golden/param_ids_v1.txt` is exposed under its host ID and no two IDs share one (automation lanes stay put). Silent while idle, a note plays, the state restores in a second instance. |
| `lifecycle` | Create / prepare / play / release / destroy 8 times (two sample rates, block sizes, main bus only and all buses), create / destroy without prepare, prepare twice without release, release twice, several instances destroyed out of order. |
| `state` | A rich session – the tape-audio fixture, a different preset in each of the four parts, settings in all parts, the mixer, master FX, tape and metronome – saved and restored: `getStateInformation` is **byte-identical** in a second instance, in a third one with other block sizes and only the main bus and when loaded back into the same instance after playing. The presets per part, the tape audio and the host-set values are in the state; the host sees the restored values; empty / damaged states never stop the instance. |
| `automation` | A sample of about 170 parameter IDs (every 41st sound setting plus key ones) ramped 0 → 1 → default in 32-sample steps while all four parts play. Sample-accurate ramps: part 1's volume automated per sample (1-sample blocks) – identical on two fresh instances, nothing changes before the ramp starts, the ramp reaches the sound, 1- and 64-sample automation end at the same level. Automated values land in the saved state and are restored for the host. |
| `bounce` | Offline (non-realtime) renders of a session with tape playback and notes, all 18 output channels: two instances, and two passes of one instance, are **bit-identical**. |
| `rates` | 44.1 / 48 / 96 kHz × blocks 32, 64 … 2048; odd and changing block sizes (1 … 2048) inside one prepare; 1-sample blocks; the same note is about as loud at 44.1 and 96 kHz. |
| `bypass` | The host bypass parameter: silent while bypassed, plays again afterwards, toggled every block without glitches. |
| `latency` | The reported latency at each sample rate is plausible, constant while playing, and no sound appears before it. |
| `multi` | Four instances processed in parallel on their own threads each sound bit-identical to the same instance alone; a change in one stays in that one. |
| `editor` | Plays with no editor; the editor opens and closes three times while audio runs on its own thread; plays afterwards (on Linux the editor is created and destroyed without a window: the headless X server has no window manager to embed it). |
| `transport` | Host transport stopped → playing at 120 BPM → tempo change to 90 → loop jumps → stop → play at 140 from beat 16 → relocate, with Ohpe's sequencer Run on: the metronome ("click in plug-in") clicks on every host beat within 2 samples (after the reported latency) and stays silent while the host is stopped. |

On Linux, macOS and Windows every section is its own CTest (`vst3_host_test`, `vst3_host_<section>`); the macOS
auval job runs all sections against the installed Audio Unit. Locally:

```
build/ohpe_hosttest_artefacts/Release/ohpe_hosttest build/Ohpe_artefacts/Release/VST3/Ohpe.vst3 --section all --source .
build/ohpe_hosttest_artefacts/Release/ohpe_hosttest ~/Library/Audio/Plug-Ins/Components/Ohpe.component --section state --source .
```

`OHPE_HOSTTEST_DUMP=/tmp/ohpe` writes both states of a failed comparison (`/tmp/ohpe.<tag>.A.bin` / `.B.bin`).
pluginval (strictness 5, Linux) and `auval -strict` (macOS) run in addition.

## Manual test plan (testers, before a release)

The automated host covers what can run without a screen and a person. Please go through this list in **your DAW**
(any host that loads VST3 or Audio Unit instruments) and note the DAW, its version, your system and the plug-in
format. Report failures as a GitHub issue with the steps and, if possible, the project file.

### 1. Load and scan
- [ ] A fresh plug-in scan finds **Ohpe** as an instrument (VST3; on macOS also the Audio Unit).
- [ ] Insert Ohpe on an instrument track: the editor opens, the start sound plays from your MIDI keyboard.
- [ ] Close and reopen the editor several times while playing: no dropouts, no crash.
- [ ] Insert a second and a third instance: each plays its own sound.

### 2. Save and reload
- [ ] Load a different preset in each of the four parts, change some knobs, record a few seconds onto the tape.
- [ ] Save the project, close it, reopen it: all four parts, the preset names, the knob positions and the tape audio
      are back. Play it – it sounds the same.
- [ ] Duplicate the track (or copy the instance): the copy sounds identical.
- [ ] Save a preset of your DAW for the instance (if it offers this), load it on a new instance: same sound.

### 3. Automation
- [ ] Record automation of a knob by moving it in Ohpe's editor: the lane is written and plays back.
- [ ] Draw a ramp in the lane (e.g. part 1 volume, a filter cutoff): the sound follows smoothly, also in a loop.
- [ ] Automate a parameter in each of the four parts: the right part reacts.
- [ ] A MIDI-mapped knob (MIDI learn in Ohpe) also moves the DAW's lane.

### 4. Transport and tempo
- [ ] Switch Ohpe's sequencer **Run** on (Ohpe follows the host transport while it is on) and the metronome with "click in plug-in": it clicks on your DAW's beats. Start, stop, loop a region,
      jump around the timeline – it follows and stays on the grid.
- [ ] Change the project tempo (also a tempo change in the middle of the song): tempo-synced LFOs, delays and the
      sequencers follow.
- [ ] With Run on, your DAW's play button starts the sequencer pattern; stop stops it.

### 5. Offline bounce / export
- [ ] Export (render) a song section faster than real time twice: both files are identical (null test: invert one,
      sum – silence).
- [ ] Export in real time: sounds like the offline export.
- [ ] Export with Ohpe's aux outputs routed to their own tracks: every stem is there.

### 6. Sample rate, buffer size, latency
- [ ] Switch the project between 44.1, 48 and 96 kHz: Ohpe keeps playing and sounds the same.
- [ ] Change the audio buffer size (small and large, e.g. 32 and 2048): no crackles beyond what your system shows
      with other instruments, no crash.
- [ ] Record a MIDI part, then a click track: with your DAW's latency compensation on, Ohpe is in time.

### 7. Bypass, outputs, freeze
- [ ] Bypass the instance in your DAW while notes sound: silence; un-bypass: plays again, no stuck notes.
- [ ] Enable the multi-outputs: each mixer channel routed to an aux output arrives on its own DAW channel.
- [ ] Freeze / unfreeze (or "render in place") the track: the frozen audio matches the live sound.

### 8. Stress
- [ ] A project with 8 instances plays for 30 minutes without dropouts beyond your system's limits.
- [ ] Load a big session while the transport runs: no crash.
- [ ] Quit the DAW with the editor open: no crash; the next start loads the project normally.
