# Playing the interface

*English · Deutsch: [../de/BEDIENUNG.md](../de/BEDIENUNG.md)*

Ohpe's surface (p14): engine keys with the **LIFE** bar (left), the display (centre), mode keys (right), four encoders below the
display, the **TRANSPORT** keys with the tape readout (bottom left), the **OCTAVE** keys, **MIDI** and the keyboard readout (bottom right) and the
keyboard row – **PITCH** and **MOD** wheels, a three-octave keyboard, **glide** mode and time. The header
holds the **part selector**, the **preset strip**, the **LIBRARY** and **PRO** keys, **RANDOM** and **SAVE**,
and EN / DE. Every function can be reached with the mouse and – through MIDI learn – from a controller.

![The plate](../images/ohpe-analog.png)

## Keyboard row: wheels, keyboard, glide

![Keyboard row](../images/ohpe-keyboard-row.png)

| Control | What it does |
|---|---|
| **PITCH** wheel | pitch bend for the edit part (its MIDI channel, like the on-screen keyboard); range = the part's *Bend Range*. Drag up / down; it springs back to the centre when you let go. |
| **MOD** wheel | the mod wheel (CC 1): a source in the [modulation matrix](MODULATION.md). Stays where you leave it; double-click = 0. |
| Keyboard | three octaves (C3–C6 by default); **OCTAVE −/+** moves it. |
| **PORTAMENTO**: AUTO / LEGATO / ALWAYS | glide mode of the edit part: *Auto* glides while the previous note still sounds, *Legato* only while another key is held, *Always* from every previous note. |
| **GLIDE** | glide time of the edit part (0 = off). |

Both wheels are host parameters (*Pitch Wheel* `perf.bend`, *Mod Wheel* `perf.mod`): right-click for MIDI
learn, Shift / Cmd / Ctrl for fine control. They are performance controls, not part of presets.

## Life

![LIFE at the foot of the engine column](../images/ohpe-life.png)

**LIFE** (p15) at the foot of the engine column makes the edit part's held notes keep evolving, the way a
played instrument never sounds exactly the same twice. At **0** the sound is static and exactly as without
it; turn it up and every voice starts to move on its own:

* a slow **pitch drift** of a few cents (at most about ±12 cents at 100 %; the voices of a chord drift apart),
* **filter breathing** – the brightness rises and falls slowly (filter engines: cutoff; FM: index; Vocal: vowel; Pluck: tone),
* **engine colour** – wavetable position and warp, FM index, formant, grain density wander a little,
* **grain scatter** – the Grain engine's grains start at slightly scattered positions,
* a hint of **pan** and **level** motion (about ±0.25 dB – breathing, never a jump).

The motion comes from slow random drifts and free "chaos" LFOs at uneven rates, different for every voice
(seeded per part, voice and note, so a bounce sounds the same every time). Around 15 – 35 % brings pads and
keys to life without sounding detuned; higher values give lush, drifting textures. Changes glide over about
60 ms, so you can ride it while notes hold.

Drag the bar (Shift / Cmd / Ctrl = fine), double-click = 0, right-click = MIDI learn. While the part plays,
the lit bar shimmers slowly – faster with more life; a fine light tick shows where the modulation matrix
moves it (target **Life**, [MODULATION.md](MODULATION.md)). LIFE is also on the Envelope pages (after
Poly Glide) and is part of part presets (`pN.life`, [PARAMETERS.md](../PARAMETERS.md#life-per-part-p15)).

## Light bars

Engine, mode, transport, octave and MIDI keys carry a short **light bar** at the bottom edge of the key,
under the pictogram. One colour language: **yellow** = what you are editing, **violet** = running or in use
elsewhere, **green** = playing, **red** = recording. A light that means "playing" or "recording" **breathes**
gently (a soft pulse of about 1.8 s, never fully off).

| Keys | Light bar |
|---|---|
| Engine | four segments, one per part (1 – 4, left to right): a segment lights when that part plays the engine – **yellow** for the edit part, **violet** for another part. |
| Mode | **yellow** = the page on the display. **Violet**: SEQUENCER = the sequencer runs; EFFECTS = a slot of any FX rack (a part or the master) is on; DRUM = a running sequencer plays the drum kit; LFO = a part's LFO has depth or a modulation routing is set. **Green** on TAPE = the tape plays, **red** on TAPE = recording (both breathing). The status colour wins; the shown page keeps its sunk key. |
| Transport | PLAY **green** while running (the triangle glows too), REC **red** (steady = ready; the circle glows), LOOP **yellow** while on. While PLAY runs and REC records, bar and sign breathe. |
| OCTAVE − / + · MIDI | OCTAVE **yellow** while the keyboard is shifted that way; MIDI **red** during MIDI learn. |

Hover a key: its tooltip says what the bar means (engine keys also list the parts that use them).

![Transport, keyboard row and octave block while recording](../images/ohpe-transport.png)

The **readouts** next to the keys: the tape readout above the transport keys shows the state (**REC**
red, **PLAY** green – both breathing while running – *TAPE*), the tape time (m:ss.t) and bar.beat; the keyboard readout in the octave
block shows the keyboard's range (C3–C6) and its octave shift, and **MIDI LEARN** in red while a control
waits for a MIDI message.

![Light bars: several parts with different engines](../images/ohpe-engine-leds.png)

## RANDOM and SAVE

**RANDOM** gives the edit part's engine new sound values (its Synth pages; level, tuning, voices and the
sample stay). **SAVE** opens the library's save panel for the edit part.

**Hold RANDOM** (or right-click it) to open **[breeding](BREEDING.md)**: cross two sounds, hear six children,
keep the best and breed further. While the breeding view is open, RANDOM rolls a new parent B and SAVE stores
the selected child in the library as a favourite.

![The breeding view](../images/ohpe-breeding.png)

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

The left half of the display shows one of four **views** – click it to cycle, right-click for the
view menu:

| View | What it shows |
|---|---|
| **Character** (default) | the animated character of the engine / mode. On the Synth and Mixer pages it stands on a slowly moving **spectrum terrain** of its sound (the edit part, the master). |
| **Stereo scope** | the master output: goniometer (mid up, side across, L upper left, R upper right), correlation meter (−1 … +1; red = out of phase) and the **M** / **S** level bars. |
| **Spectrum terrain** | the master spectrum as a perspective landscape: new rows rise at the front and recede to the horizon. It comes to rest when the sound stops. |
| **3D space** (1.0) | the head-centred view of the [3D stage](SPACE.md): every part that uses the space as a dot with its trail around the listener's head, the edit part labelled. |

The **stereo scope appears by itself** whenever the encoders show a **Mid/Side** effect in an FX rack
slot: in a part's rack it shows that part after its rack (marked *P1*–*P4*), in the master rack
(*MFX 1*–*MFX 4*) the master output (marked *Master*). The M and S bars use the colours of the
*Mid Level* and *Side Level* encoders. In the rack view of the FX page the strip below the effect
chain shows the stereo scope when a Mid/Side slot is selected there.

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

**BOUNCE** (third key, p23) puts tape or live-input audio into the engine you are looking at: a menu offers the edit
track, Tape 1 … 8, the tape mix and the input (1–8 bars), plus *Endless loop* (on by default – the bounce loops
seamlessly). See [Bounce to engine](BOUNCE.md).

## User wavetables and formants (p20)

With the Wavetable or Vocal engine on the edit part, the Synth page shows **IMPORT**, **USER** and
**CLEAR**. Drag a sound or a wavetable file onto the instrument (or use IMPORT: file, tape track, live
input, library; frames 8 … 256, smoothing): the sound is analysed in the background – a progress bar
on the display – into a user wavetable (Wavetable engine) or formant tracks (Vocal engine), saved in
the user wavetable library and switched on. The display then shows the frame stack (the frame at the
current position in yellow) or the five formant tracks. **USER** switches between the user and the
built-in material (`pN.usertable.wt` / `pN.usertable.vocal`), **CLEAR** removes it. Details:
[Wavetable engine](engines/wavetable.md#user-wavetables-audio-to-wavetable-p20).

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

## Transport keys

The **TRANSPORT** keys in the bottom-left panel (one row: TO START, STOP, PLAY, REC, LOOP under the tape readout) work on every page:

| Key | Action |
|---|---|
| **REC** (red) | one-touch record on the tape: arms the selected track when no track is armed, switches recording on and starts the tape. Pressed while recording: punch out (the tape keeps playing). Lit while recording ready, breathing while it records. |
| **PLAY** | starts / stops the sequencer and the tape together (*Play starts tape*, on by default; off: PLAY runs the sequencer alone) |
| **\|◀** | tape back to the start |
| **STOP** | stops tape, recording and sequencer; pressed again while stopped: back to the start |
| **LOOP** | tape loop on / off; without a loop range it loops what is recorded (or four bars) |

While the tape runs, every page shows **● REC 0:12.3** (red, breathing) or **▶ TAPE 0:12.3** (green) in the display
header. In the standalone app: **Space** = PLAY, **R** = REC, **Enter / Home** = to the start, **L** =
loop (in a plug-in the host keeps its own keys). Right-click any transport key (or a button on the
tape page) for MIDI learn – every transport function has a trigger, see [MIDI](MIDI.md).

## Tape page

![Tape page](../images/ohpe-tape.png)

The left half of the display is the **transport panel**: the state (**● REC** breathing, *REC READY*,
*PLAY* in green, *STOP*), the armed tracks, the time and bar.beat, the reels (they turn with the tape) and five
buttons – **|◀ · ■ · ▶ · ● · LOOP** – plus one line that says what to do next (*Pick a track, press
REC*). Below: the eight tracks with their content, the play position (red while recording), the loop
region (yellow) and the **arm dots** – click a dot to arm or disarm the track, click a lane to select it.

![Tape page while recording](../images/ohpe-tape-recording.png)

Encoder pages:

| Page | Encoders |
|---|---|
| Track | Arm, Source (input, part 1–4, drum), Level, Direct out of the selected track |
| Transport | Overdub, Speed (varispeed 0.5–2×), Reverse, Play starts tape |
| Loop | Loop on/off, Loop in, Loop out |
| Tape character | Saturation, Wow, Flutter, Hiss |
| Bounce | Bounce source, Bounce target (Grain / Sampler), Endless loop, Input length (p23) |

Soft keys on the display:

| Key | Action |
|---|---|
| **Lift** | copies loop in … loop out of the selected track to the clipboard and leaves silence |
| **Cut** | moves loop in … loop out of the selected track to the clipboard and closes the gap (later audio moves earlier) |
| **Drop** | places the clipboard at the play position (overwrites) |
| **Splice** | inserts the clipboard at the play position (the rest moves back) |
| **Undo / Redo** | unlimited undo of recordings and edits |
| **Export** | renders master + the eight tracks as **24-bit WAV** files into a folder you choose (offline, faster than real time; the live audio is not interrupted) |
| **Bounce** | moves the edit track (or the source on the Bounce page; the loop range while the loop is on) into the Grain or Sampler engine of the edit part, as an endless loop by default – [Bounce to engine](BOUNCE.md) |

The line left of the soft keys shows the loop range that Lift and Cut use. Every tape edit is
MIDI-mappable: *Tape Edit Track* (`tape.edit.track`), *Tape Lift / Cut / Drop / Splice / Undo / Redo /
Return to Zero* (`tape.edit.*`). Short guide: [first recording in 30 seconds](TAPE-MIXER-FX.md#first-recording-in-30-seconds).

## Drum kit page

![Drum kit page](../images/ohpe-drum.png)

The Drum mode shows all 24 pads. Click a pad to play and select it; the encoders edit it – model,
tune, decay, tone (page 1), level, pan, choke group, source (page 2) and the kit's volume and base
note (page 3). **Load** puts an audio file on the selected pad (or drag a file onto a pad),
**Clear** returns the pad to its factory sound.

## Mod view (modulation matrix, 0.7)

![Mod view](../images/ohpe-mod-view.png)

In **LFO** mode the tabs on the display switch between **LFO** (LFO 1, as before), **Mod**, the
[modulation matrix](MODULATION.md) of the edit part, and **Morph** (the [morph pad](#morph-view-p16)). The Mod view lists all **16 routings** in two
columns: source → target, a bar for the amount (to the right = positive, to the left = negative) and
the routing's live value. Targets on an engine or an effect show the real parameter name ("Cutoff",
"FX2 Hyper Detune"), so you see what moves without counting slots. Below the list a **trace** draws
the value of the selected routing over the last three seconds.

| Action | Result |
|---|---|
| Click a routing in the list | selects it; the encoders edit it |
| **◀ Mod** / **Mod ▶** (soft keys) | previous / next routing |
| **Clear** (soft key) | sets the selected routing back to Off (source, via, target, amount, curve) |
| Page arrows | turn the encoder pages |

Encoder pages of the Mod view: **source, via, target, amount** of the selected routing · **curve**
and the **sequencer lane** · **macros 1–4** · **LFO 2** (rate, sync, shape, depth / mode, phase,
fade) · **LFO 3** (the same) · **envelope 3** (delay, attack, hold, decay / sustain, release).
Everything is in the pro view too (LFO tab).

**Modulation rings.** Wherever a parameter is the target of a routing – on the encoders and on the
display rows of any page – a thin second ring (encoder) or line above the bar (display row) shows
the **reach** of all its routings around the set value, and a dot or tick shows the **live
position** while notes play. Turn the encoder to move the centre; the reach moves with it.

## FX rack views (0.7)

![FX rack of a part](../images/ohpe-rack-view.png)

In **Effects** mode the tabs switch between **Sends** (as before), **Rack** – the six slots of the
edit part's [FX rack](TAPE-MIXER-FX.md#fx-rack-07) – and **Master**, the four slots of the master
rack. The chain shows every slot as a block in signal order: effect name, mix bar, output level,
**BYPASS** when bypassed; empty slots are dimmed. The selected slot is outlined, and the lower half of
the display shows **what its effect does**: the transfer curve of the distortion, the response of the
EQ and the filter, the static curve and live gain reduction of the compressor, the gain of each band
of the multiband, the motion of the modulated effects.

| Action | Result |
|---|---|
| Click a block | selects the slot; the encoders edit it |
| Drag a block onto another one | moves the slot there (the others close up); undoable, part of the preset |
| **◀ Move** / **Move ▶** (soft keys) | moves the selected slot one place earlier / later |
| **Clear** (soft key) | empties the slot (type Off) |

Encoder pages of a slot: **type, mix, bypass** · then the parameters of its effect, four per page.
While an effect chosen for the first time is being prepared, the display says *Preparing effect…*
(a moment; the slot passes the sound dry meanwhile).

![Master rack](../images/ohpe-master-rack.png)

## 3D Space view (1.0)

![The 3D Space view](../images/ohpe-space-view.png)

The fourth tab of **Effects** mode, **3D Space**, edits the edit part's [3D stage](SPACE.md): on, motion
path, length and rate on the first page, then position (azimuth, elevation, distance, spread), the shape
of the motion, room, air and distance, and on the last page the global **3D Monitoring** (Headphones /
Speakers) and **Mono Check**. Instead of the effect chain the display shows the head-centred 3D view of
all four parts; the strip below sums up the motion and the monitoring. The rack soft keys (move, clear)
belong to the **Rack** and **Master** tabs only.

The **3D badge** in the display header (headphones pictogram + "3D") lights up in ultraviolet and pulses
gently while the edit part plays binaural 3D; on speaker monitoring it is outlined in yellow with a pair
of speakers, otherwise dim. Click it to jump to this view; hover it for *Binaural 3D – best with
headphones*. The part selector shows the same pictogram per part, the library shows it after the name
of every preset tagged **3D**.

![The 3D badge in the display header](../images/ohpe-space-badge.png)

## Body view (p19)

![The Body view](../images/ohpe-reso-view.png)

The fifth tab of **Effects** mode, **Body**, edits the edit part's [resonator body](RESONATOR.md): on, body model,
tuning (Fixed / Note / Chord) and mix on the first page, then tune, size, material and decay, then position,
inharmonicity, sympathetic strings and width, and on the last page drive and the MPE amounts (pressure, slide). The
display draws the body – the string pair with its sympathetic strings, the plate with its moving mode pattern or the
bore with its standing wave – with the excitation point as a yellow dot; the strip below names the model, the tuning
and the pitch the body follows. A *Resonator* slot in the rack shows the same body in the Rack view.

## Morph view (p16)

![The Morph view](../images/ohpe-morph-view.png)

The third tab of **LFO** mode, **Morph**, shows the edit part's [morph pad](MORPH.md): four corner chips (A top
left, B top right, C bottom left, D bottom right) in the encoder colours, the pad with the glided position, a
soft glow from each corner as strong as its share, and the edges between the corners – solid where two corners
morph their parameters (one sound), dotted where they cross-fade as parallel sounds (another engine or effect).
The strip below says what the pad does right now (one sound, or how many sounds play in parallel).

| Action | Result |
|---|---|
| Drag on the pad | moves the position (X, Y); right-click = MIDI learn |
| Click a corner chip, or **A … D** (soft keys) | menu: **Current sound**, **Preset ▸** (any part preset of the library), **Clear corner**, **Clear all corners** |
| Encoders, page *Morph* | Morph on, X, Y, Glide |
| Encoders, page *Control* | X / Y Control (Off, Pressure, Slide, Mod Wheel), MPE, expression smoothing |

Assigning a corner switches the morph on. An empty corner stands for the part's own sound.

## Keys and shortcuts

| Key | Action |
|---|---|
| Esc | closes the library (cancel) or the pro view, or ends step editing |
| Space (standalone) | PLAY / stop |
| R (standalone) | REC: record on the tape / punch out |
| Enter or Home (standalone) | tape to the start |
| L (standalone) | tape loop on / off |
| Enter (library) | keep the selected preset and close |
| ↑ / ↓ (library) | previous / next preset in the list |

See also: [preset library](PRESETS.md) · [sequencers](SEQUENCERS.md) · [tape, mixer and effects](TAPE-MIXER-FX.md) · [MIDI](MIDI.md).
