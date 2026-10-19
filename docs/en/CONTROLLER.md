# Grid controller (profile 1)

Ohpe (the standalone app and, since p49, the plug-in) can be played and edited from an external **8 × 8 pad grid controller with a four-line
character display**: 64 pads with velocity and pressure, 8 touch-sensitive encoders and a master encoder, two rows
of 8 buttons under the display, transport and navigation buttons and a touch strip. In Ohpe this is
**Grid-Controller (Profil 1)** / **Grid controller profile 1**.

The pads play exactly like the on-screen [grid keyboard](HARMONY.md#the-grid-keyboard): same layouts, same notes,
same colours for root and key notes – what you learn on one works on the other.

![Settings – Controller: ports, connection, controls and the virtual controller](../images/ohpe-settings-controller.png)

## Setting it up

**Nothing to do in most cases.** Since 0.6.3 the profile is **on by default with automatic ports**: plug the
controller in and start Ohpe (standalone app, or open the plug-in's window). Ohpe finds the controller's *user port*
(input **and** output of the same device), switches the controller into its **User mode** by itself (no need to
press its *User* button), shows **OHPE · connecting ...** and then **connected** on its display for a moment
(German UI: *verbindet ...* / *verbunden*), and then the normal Ohpe display. Plugged in later? Ohpe connects within
a second. When Ohpe quits, the profile is switched off or the instance goes, it clears the display and lights and
hands the controller back to its own mode.

**Settings > Controller** (its own page):

- **Off** / **Grid controller profile 1**.
- **Ports**: **Choose ports automatically** (on by default), then a menu for **Input** and **Output** with the full
  port names. Picking a port in a menu switches automatic off (the other side keeps the port in use); choosing
  automatic in the menu switches it back on.
- The **status line**: **Connected · first generation, text display** (or *later generation, no text display*),
  **Checking the device ...**, **No controller found**, **Port in use by another program** (standalone) /
  **Port in use - free it in the host** (plug-in), **Waiting for the controller**.
- **Controls**: **Touch strip** Bend / Mod; **Master** encoder Volume / Tempo.
- The **virtual controller**, large on the right: the four display lines, the two button rows under the display and
  the 64 pads in the controller's colours, live – pressed pads light green, the mode / part buttons light as on the
  device. It works with the mouse too: click a pad to play it, click a button, turn the mouse wheel over an eighth of
  the display to turn that encoder. It also follows the pads when no output is connected.

**How the controller is found.** Port names that match the pattern `User; MIDIIN2; MIDIOUT2` (any part, case does
not matter; editable as `"pattern"` under `"controller"` in `settings.json`) are candidates; the input and output of
the same device are paired (on macOS / Linux its *… User Port*, on Windows *MIDIIN2 (…)* / *MIDIOUT2 (…)*). Ohpe
sends a standard identity request: the answer tells the controller's **generation**. A device that also has its
*live port* next to the user port connects at once; another matching device must answer as the controller within
two seconds, otherwise Ohpe leaves it alone and tries the next candidate.

**Generations.** The first generation has the four-line character display written over MIDI – Ohpe uses all of it.
A later generation (pixel display over USB) gets pads, buttons, encoders, touch and lights, but no text on its
display (its pad colours may differ).

The settings are saved per user in `settings.json` (never in projects or presets). Ohpe switches the controller's
ports (user and live port) off in the plain MIDI input list while it holds them, so pads don't play twice and the
encoder / strip touch notes never reach the sound as notes, and switches them back on when it lets go.

## In a plug-in

Since p49 the controller also works inside the **plug-in** (VST3 / AU in a DAW). The controller has two modes: in
its usual mode it talks to the DAW; in its **User mode** (the controller's *User* button) it hands its pads, display
and lights to whatever program opens its *user port*. Ohpe opens that port itself – past the host – with the full
display, pad colours, pressure, encoders (with touch), touch strip and buttons, exactly as in the standalone app.

![Settings – Controller in a plug-in](../images/ohpe-settings-controller-plugin.png)

1. **In the DAW**: in its MIDI preferences, switch the controller's *user port* (often called *… User Port*, on
   Windows *MIDIIN2 (…)* / *MIDIOUT2 (…)*) **off for tracks and remote** (input and output), so the DAW releases it
   and the pads don't play twice. Leave the controller's main port as it is.
2. **In Ohpe**: nothing (**Settings > Controller**: the profile and automatic ports are on by default; pick the ports
   in the menus to choose them yourself).
3. Ohpe switches the controller to its **User mode** itself. The page shows **Active in this instance** once both
   ports are open.

**One instance at a time.** Only one Ohpe of the DAW can hold the controller: the one whose window you **opened or
clicked last**. The others show **Active in another instance** and a **Take over here** button; when the holding
instance is removed, the next one takes over. The holding instance keeps the controller while its window is
**closed** (the encoders keep the last page, the part, mode and transport buttons work). After loading a project,
open the window of the instance you want to play once – until then no instance holds the controller.

**Port in use.** On Windows a MIDI port can be open in only one program. If the DAW still holds the user port, the
legend shows **Port in use - free it in the host**; free the port in the DAW's MIDI preferences – Ohpe tries again
every second, and also reconnects when the controller is unplugged and plugged in again. MIDI from the host keeps
working in parallel. Closing the instance or switching the profile off clears the display and the lights.

## Display

| Line | Shows |
|---|---|
| 1 | names of the parameters on the 8 encoders (two over each display segment) |
| 2 | their values |
| 3 | preset · part and engine · tempo · key (and **Lock** with Scale Lock) – while an encoder is touched: its full name and value |
| 4 | page (e.g. *Filter 2/5*) · transport (Play / Stop / REC / Count-in, *Click* with the metronome) · tape position · pad mode, grid layout and octave |

Umlauts are written as ae / oe / ue (the display has no umlauts).

## Encoders

* **Encoders 1 – 8**: the parameters of the shown page and the next one – encoders 1 – 4 are the four screen encoders,
  5 – 8 the page after it (in the order of the pages on screen). Hold **Select** (or Shift) for fine steps.
* **Master encoder**: master volume; **Master** button switches it to the tempo.
* **Tempo encoder**: tempo (1 BPM per step, Shift / Select 0.1).

## Pads

The second button row, keys 5 – 8 (or **Note** / **Session**), chooses the pad mode:

| Mode | Pads |
|---|---|
| **Keys** | the on-screen grid keyboard on 8 × 8 pads |
| **Pads** | the four performance pads (stutter, tape stop, sweep, freeze) – one 4 × 4 quarter each, held = on |
| **Drum** | the 24 drum pads on the three bottom rows |
| **Steps** | steps 1 – 16 of the step sequencer on the two top rows (top row 1 – 8); the six rows below play like Keys |

### Keys – the grid keyboard on the pads

* **Same layout** as *Grid Layout* (`harmony.kb.layout`): **Fourths** (right +1, up +5), **Thirds** (+1 / +4),
  **Whole Tones** (+2 / +5) or **In Key** (scale steps of the key, three steps per row – only key notes).
  The **Scales** button steps through the layouts. Since p53 the pads follow the on-screen keyboard: while the
  **grid keyboard** is shown (GRID key) they take its layout, root, scale and colours; with the **piano keys** shown
  they use the default layout (Fourths). Scales with the piano keys shown first switches the screen to the grid
  keyboard, then steps its layout – screen and pads always show the same.
* **Same mapping**: pad (column, row) plays the note of the screen grid's cell at the same column and row, from the
  same lowest C. The four lower pad rows are exactly the screen grid's four rows; the upper four continue it.
* **Same key**: the key the grid shows (the remembered key – the set key until anything was heard). With **Scale
  Lock** (Shift + Scales) the other notes stay dark and silent; In Key has only key notes anyway.
* **Colours**: the key's root **amber**, key notes **yellow (dim)**, tones of the remembered chord **green (dim)**,
  held pads **green**, other notes dark (graphite on screen).
* **Octave − / +** moves pads, screen keyboard and screen grid together (the bottom-left pad is the C of that octave;
  In Key: the first key note from there).
* **Velocity and pressure** play the **edit part**. With MPE on for the part each pad gets its own member channel
  and pressure is that note's pressure; without MPE the notes go through scale, chord and follow and pressure is
  sent as poly aftertouch of the part.

## Buttons

| Button | Action |
|---|---|
| first row under the display | the eight mode keys in screen order, Synth to Drum (lit like the screen keys) |
| second row 1 – 4 | parts 1 – 4 |
| second row 5 – 8 | pad mode Keys / Pads / Drum / Steps |
| ◀ / ▶ | page − / + |
| ▲ / ▼ | previous / next preset |
| Octave − / + | octave of the keyboard, the grid and the pads |
| Play | play / stop (Shift: tape play / stop) |
| Record | record (Shift: arm the edit track) |
| Stop | stop (Shift: to the start) |
| Repeat | tape loop on / off |
| Metronome | metronome on / off (Shift: count-in Off / 1 bar / 2 bars) |
| Tap Tempo | tap the tempo |
| Undo | tape undo (Shift: redo) |
| Scales | next grid layout (Shift: Scale Lock) |
| Master | master encoder: volume ↔ tempo |
| Shift / Select | modifiers (Select: fine encoder steps) |

## Touch strip

**Bend**: pitch bend of the edit part (the screen's pitch wheel follows), back to the middle when you let go.
**Mod**: the mod wheel.

## Notes

* No new host parameters: the controller only changes existing parameters (host notified, automatable as before).
* Own implementation of the controller's publicly documented MIDI / display protocol; Ohpe contains no third-party code.
