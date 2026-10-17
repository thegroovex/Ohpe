# Grid controller (profile 1)

Ohpe's standalone app can be played and edited from an external **8 × 8 pad grid controller with a four-line
character display**: 64 pads with velocity and pressure, 8 touch-sensitive encoders and a master encoder, two rows
of 8 buttons under the display, transport and navigation buttons and a touch strip. In Ohpe this is
**Grid-Controller (Profil 1)** / **Grid controller profile 1**.

The pads play exactly like the on-screen [grid keyboard](HARMONY.md#the-grid-keyboard): same layouts, same notes,
same colours for root and key notes – what you learn on one works on the other.

![Settings – MIDI with the controller section and the virtual controller](../images/ohpe-settings-controller.png)

## Setting it up

1. Connect the controller and start the Ohpe **standalone app** (plug-ins leave controllers to the host).
2. **Settings > MIDI > Controller**: choose **Grid controller profile 1**.
3. **In** / **Out**: step to the controller's MIDI ports (usually its first port for both; if the display stays
   empty, try its other port). The legend shows **Connected** once both ports are open.
4. **Bend / Mod**: what the touch strip does.
5. **View** shows the *virtual controller*: the four display lines, the 64 pad lights and the last MIDI messages –
   handy to check what the controller should show.

Nothing is detected automatically; the choice is saved per user in `settings.json` (never in projects or presets)
and the controller is reopened at the next start. When it is unplugged the lights and display stop; when it comes
back, Ohpe reconnects within a second and sends everything again. Ohpe switches the controller's port off in the
plain MIDI input list so its pads don't play twice. The profile is **off by default**.

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
  The **Scales** button steps through the layouts.
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
