# MIDI guide

*English · Deutsch: [docs/de/MIDI.md](../de/MIDI.md)*

This guide covers how Ohpe receives and sends MIDI: channels per part, MPE, MIDI clock, host
sync and Link. Mapping controllers to parameters (MIDI learn) is described in the main manual.

## Channels and parts

Every part has its own **MIDI channel** (`pN.midich`): part 1 listens to channel 1, part 2 to
channel 2 and so on. `0` means **omni** (all channels). A controller on channel 3 therefore
plays part 3; set several parts to the same channel to layer them. The on-screen keyboard plays
on channel 1.

**Channel 10 plays the drum kit.** Omni parts ignore it; a part set to channel 10 (or in MPE
mode) hears it as well, layered with the kit.

The sequencers address their part directly (their *target part*), whatever its channel.

| Message | Effect |
|---|---|
| Note on / off | plays the part (through scale / chord mode and the arpeggiator, if active) |
| Pitch bend | bends the part (range `pN.bend`) |
| CC 5 | portamento time (0.6): overrides the part's Glide time (0 … 2 s) until `pN.glide` itself changes |
| CC 64 | sustain |
| CC 65 | portamento off (< 64) / on (≥ 64, the default) (0.6) |
| CC 74 | timbre (target `pN.mpe.timbre`) for all notes of the part |
| Channel pressure | pressure (target `pN.mpe.press`) for all notes of the part |
| CC 120 / 123 | all sound off / all notes off |
| Mapped CCs and notes | change their parameter (MIDI learn) and do not play |

## MPE (MIDI Polyphonic Expression)

Switch on **MPE** (`pN.mpe`) for a part to play it from an MPE controller (lower zone: master
channel 1, member channels 2–16). The part then listens to all channels, and every note keeps
its own expression:

* **Per-note pitch bend** on the member channel, range `pN.mpe.bend` (default 48 semitones,
  the MPE standard). Pitch bend on the master channel bends the whole part.
* **Pressure** (channel pressure on the member channel, or polyphonic aftertouch) goes to the
  **pressure target** (`pN.mpe.press`, default *Level*).
* **Timbre** (CC 74 on the member channel, centre 64) goes to the **timbre target**
  (`pN.mpe.timbre`, default *Timbre*).

Targets: *Off*, *Timbre* (the engine's brightness: filter cutoff in Analog, modulation index in
FM), *Level* (up to +6 dB), *LFO Depth* (adds vibrato / tremolo per note), *Pan*.

Set your controller's pitch-bend range to the same value as `pN.mpe.bend`.

## Tempo and transport

| Situation | Transport source |
|---|---|
| Plug-in in a host | always the host: tempo and position (ppq), sample-accurate |
| Standalone, `sync.src` = Internal | Ohpe's own clock (`tempo`, start with `seq.run`) |
| Standalone, `sync.src` = MIDI Clock | an external MIDI clock on any enabled MIDI input |
| Standalone, `sync.src` = Link | the Link session on the local network |

In every case the sequencers start and stop with **Sequencer Run** (`seq.run`) – in a host they
run while the host plays *and* `seq.run` is on.

### Host sync (plug-in)

Ohpe reads the host's tempo and song position every block and places every step at the exact
sample where the host's timeline reaches it, for any tempo and buffer size. Loops and jumps
in the host land every sequencer on the right step (and in song mode on the right pattern).

### MIDI clock in

With `sync.src` = **MIDI Clock** Ohpe follows 24 clock ticks per quarter note, *Start*,
*Stop*, *Continue* and *Song Position Pointer*. The tempo is averaged over recent ticks and the
position is pulled gently towards each tick; **Clock Smoothing** (`sync.smooth`) sets how much:
low values react at once, high values ignore jittery clocks (USB, wireless). A single missing or
doubled tick is ignored; a real tempo change is picked up within a few ticks.

### MIDI out (standalone)

Choose a **MIDI output** under *Sync & MIDI …* in the standalone menu.

* **Send MIDI clock** (`sync.clockout`): 24 ticks per quarter note while the transport runs,
  *Start* (or *Song Position* + *Continue* when starting mid-song), *Stop*.
* **Send sequencer notes** (`sync.notesout`): the notes of all sequencers, each on its part's
  MIDI channel – to play external instruments from Ohpe's sequencers.

### Link (standalone)

Ohpe's standalone app can join a **Link** session (Ableton Link, a third-party library, see
[THIRD-PARTY.md](../../THIRD-PARTY.md)) with other apps on the same network. With `sync.src` =
**Link** the transport takes tempo and beat phase from the session, so Ohpe's bars line up with
everybody else's. Changing Ohpe's tempo changes the session tempo. Start and stop remain local.
Link is a build option (`-DOHPE_WITH_LINK=ON`, default on); the plug-in always follows its host.

## Settings at a glance

| ID | Range | Default |
|---|---|---|
| `pN.midich` | 0 (omni) … 16 | N |
| `pN.mpe` | off / on | off |
| `pN.mpe.bend` | 0 … 96 semitones | 48 |
| `pN.mpe.press`, `pN.mpe.timbre` | Off, Timbre, Level, LFO Depth, Pan | Level, Timbre |
| `sync.src` | Internal, MIDI Clock, Link | Internal |
| `sync.smooth` | 0 … 1 | 0.5 |
| `sync.clockout`, `sync.notesout` | off / on | off |
