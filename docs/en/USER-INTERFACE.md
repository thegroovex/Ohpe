# Playing the interface

*English · Deutsch: [../de/BEDIENUNG.md](../de/BEDIENUNG.md)*

Ohpe's surface: engine keys (left), the display (centre), mode keys (right), four encoders below the
display, transport and octave keys, the keyboard. The header holds the **part selector**, the
**preset strip** and the **LIBRARY** and **PRO** keys. Every function can be reached with the mouse
and – through MIDI learn – from a controller.

## Display: drag the bars

The four rows on the display show what the four encoders control. The rows are controls too:

| Action | Result |
|---|---|
| Drag a row (right or up = more) | changes the value relative to where it was |
| Click on the bar | jumps to that position, then keep dragging |
| **Shift**, **Cmd** (macOS) or **Ctrl** while dragging | fine control: a tenth of the speed. You can press or release the key during a drag; the value never jumps. |
| Double-click | back to the default value |
| Right-click | MIDI Learn / clear mapping / reset |

The encoders behave the same: drag (fine with Shift / Cmd / Ctrl held at the start of the drag),
double-click resets, right-click opens MIDI learn. The small caption above the rows names the page
(*Macros*, *Amp Envelope*, *Track* …); the arrows in the header turn the pages.

## Display views: character, stereo scope, spectrum terrain

The display is built in layers like a real instrument screen: the dot grid lies behind the deep
glass, the values float in front of it, the character casts a faint shadow and the glass itself
carries a soft inner shadow and sheen. When you move the mouse over the display the back layers
shift a little against the front (parallax).

The left half of the display shows one of three **views** – click it to cycle, right-click for the
view menu:

| View | What it shows |
|---|---|
| **Character** (default) | the animated character of the engine / mode. On the Synth and Mixer pages it stands on a slowly moving **spectrum terrain** of its sound (the edit part, the master). |
| **Stereo scope** | the master output: goniometer (mid up, side across, L upper left, R upper right), correlation meter (−1 … +1; red = out of phase) and the **M** / **S** level bars. |
| **Spectrum terrain** | the master spectrum as a perspective landscape: new rows rise at the front and recede to the horizon. It comes to rest when the sound stops. |

The **stereo scope appears by itself** whenever the encoders show a **Mid/Side** effect in an FX rack
slot: in a part's rack it shows that part after its rack (marked *P1*–*P4*), in the master rack
(*MFX 1*–*MFX 4*) the master output (marked *Master*). The M and S bars use the colours of the
*Mid Level* and *Side Level* encoders.

![Mid/Side FX slot with the stereo scope](../images/ohpe-midside-scope.png)

![Master scope with Mid/Side in the master rack](../images/ohpe-master-scope.png)

![Spectrum terrain](../images/ohpe-terrain.png)

**Animated 3D visuals** (view menu) switches the terrain and the parallax off – the display then
stays still, the scope keeps working. The setting belongs to the computer (it applies to every
project); the chosen view is saved with the project. The visuals read the sound through a
lock-free buffer: nothing waits or allocates on the audio thread, the analysis runs on the screen
at 30 frames per second.

## Parts

Ohpe has four parts, each with its own engine and sound. The **part selector** (1–4, header)
chooses the **edit part**: the engine keys, Synth, Envelope, LFO and the FX slot pages, the
library (part presets load there) and the on-screen keyboard (it plays on the part's MIDI channel)
follow it. The display shows the edit part as a yellow badge (P1–P4). MIDI: *Edit Part* (`ui.part`).

**Synth mode** (0.6): after the engine's own pages come two pages every engine shares – **Glide**
(time, mode, timing, poly glide) and **Pitch Envelope** (amount, time, curve, voices). See
[SEQUENCERS.md](SEQUENCERS.md#portamento-and-pitch-envelope-06).

## Samples for the Sampler and Grain engines

With the Sampler or Grain engine on the edit part, the display shows **Load sample** and
**Clear** and the name of the loaded file. You can also **drag an audio file** (WAV, AIFF, FLAC,
Ogg) from your file manager onto the instrument: it goes to the edit part's Grain engine when Grain
is selected, otherwise to its Sampler (the part switches to the Sampler). Each part has its own
samples. Sample paths are saved with the project.

## Pro view

![Pro view](../images/ohpe-pro-view.png)

**PRO** opens the pro view: **every parameter of the current mode at once**, in groups – in Synth
mode the engine's macros and all its other parameters plus envelopes, LFO, pitch and MIDI of the
part; in Tape mode the transport, loop, tape character and all eight tracks; in Drum mode the kit and
all 24 pads. The tabs on top switch the mode. Each parameter is a bar: drag it (fine with Shift /
Cmd / Ctrl), click to jump, double-click to reset, right-click for MIDI learn. The layout uses as
many columns as the window is wide – make the window bigger to see more at once – and scrolls when
a mode has more parameters than fit.

## Sequencer views and parameter locks

![Step locks](../images/ohpe-step-locks.png)

In Sequencer mode the tabs on the display choose the view: **Step**, **Euclid**, **Arp**,
**Orbit** and **Sync**. Euclid and Orbit draw their rings / lanes; Arp and Sync show a summary line.

Step view:

* **Click** a step to switch it on or off (the four tabs on the left choose steps 1–16 … 49–64).
* **Shift-click** (or right-click) a step to **select it for editing**. The display shows
  *LOCK STEP n* and the encoders edit the step: **Note, Velocity, Gate, Probability** (page 1) and
  **Ratchet** (page 2).
* While a step is selected, switch to any other mode (Synth, Envelope, LFO, Mixer, Effects, Drum)
  and turn an encoder or drag a bar: the value is **locked on that step** instead of changing the
  parameter. Locked values show a square marker on the display and on the encoder. A step holds up
  to four locks; double-click a locked value to remove that lock.
* Steps with locks show small marks along their top edge (one per lock); the selected step is
  outlined.
* **Clear locks** removes all locks of the step; **Done**, a click on the *LOCK* badge, Esc or a
  second Shift-click ends the editing.

## Tape page

![Tape page](../images/ohpe-tape.png)

The Tape mode shows the eight tracks with their content, the play position, the loop region
(yellow) and the armed tracks (red dot). Click a track to select it. Encoder pages:

| Page | Encoders |
|---|---|
| Transport | Play, Record, Speed (varispeed 0.5–2×), Reverse |
| Track | Arm, Source (input, part 1–4, drum), Level, Direct out of the selected track |
| Loop | Loop on/off, Loop in, Loop out, Overdub |
| Tape character | Saturation, Wow, Flutter, Hiss |

Soft keys on the display:

| Key | Action |
|---|---|
| **Lift** | copies loop in … loop out of the selected track to the clipboard and leaves silence |
| **Cut** | moves loop in … loop out of the selected track to the clipboard and closes the gap (later audio moves earlier) |
| **Drop** | places the clipboard at the play position (overwrites) |
| **Splice** | inserts the clipboard at the play position (the rest moves back) |
| **Undo / Redo** | unlimited undo of recordings and edits |
| **\|◀** | back to the start of the tape |
| **Export** | renders master + the eight tracks as **24-bit WAV** files into a folder you choose (offline, faster than real time; the live audio is not interrupted) |

Every tape edit is MIDI-mappable: *Tape Edit Track* (`tape.edit.track`), *Tape Lift / Cut / Drop /
Splice / Undo / Redo / Return to Zero* (`tape.edit.*`).

## Drum kit page

![Drum kit page](../images/ohpe-drum.png)

The Drum mode shows all 24 pads. Click a pad to play and select it; the encoders edit it – model,
tune, decay, tone (page 1), level, pan, choke group, source (page 2) and the kit's volume and base
note (page 3). **Load** puts an audio file on the selected pad (or drag a file onto a pad),
**Clear** returns the pad to its factory sound.

## Keys and shortcuts

| Key | Action |
|---|---|
| Esc | closes the library (cancel) or the pro view, or ends step editing |
| Enter (library) | keep the selected preset and close |
| ↑ / ↓ (library) | previous / next preset in the list |

See also: [preset library](PRESETS.md) · [sequencers](SEQUENCERS.md) · [tape, mixer and effects](TAPE-MIXER-FX.md) · [MIDI](MIDI.md).
