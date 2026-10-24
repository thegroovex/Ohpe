# Accessibility and UI size

Ohpe can be played and edited without a mouse, read by a screen reader, and shown at six sizes. None of this changes
the sound or the project: the UI size lives in the settings of this computer (`settings.json`), never in a project,
preset or host session, and no host parameter was added.

## UI size (75 – 200 %)

Six sizes: **75 / 100 / 125 / 150 / 175 / 200 %** of the 1200 × 800 plate.

- **Settings › Display › Window size**, or
- **right-click on the housing** (any empty spot of the plate) or on the **settings key** – the menu *UI size*.

The window (standalone) or the plug-in editor takes the size at once and can still be dragged at the corner; it keeps
the plate's aspect ratio (3 : 2) between 67 % and 200 %. Everything is drawn as vectors: legends, pictograms, light
bars and the display stay sharp at every size, and on high-density (Retina / HiDPI) screens the housing is rendered at
the screen's own pixel density. On a small screen choose a size that fits it – the window is never larger than you set.

## Keyboard operation

A click never moves the keyboard focus: after using the mouse, the computer keyboard still plays notes. The focus
moves only with these keys:

| Key | Action |
|---|---|
| Tab / Shift+Tab | next / previous control, area by area: header (version, parts, preset, LIBRARY … settings), engines and LIFE, display (display, Key Gravity, TAP, tempo), the four encoders, the mode keys, transport and tempo, octave / MIDI / grid keys, wheels, portamento, glide and the keyboard |
| F6 / Shift+F6 (or Ctrl+Tab) | first control of the next / previous area |
| ← → ↑ ↓ | on a value control (encoder, wheel, LIFE, portamento, glide, Key Gravity, tempo, part): one step – a choice one position, a continuous value 1 % |
| Shift + arrow | finer (0.1 %) |
| Page Up / Page Down | ten steps (choices: three positions; tempo: 10 BPM) |
| Delete / Backspace | the default value (like a double-click) |
| Enter / Space | press the focused key; on the preset strip: open the library; on the tempo: type the tempo |
| Shift+F10 | the focused key's menu (MIDI learn, as a right-click) |
| 1 – 4 | on the part selector: part 1 – 4 |
| ← → and 1 – 9 | on the display: page back / forward and the soft keys (numbered from the left) |
| Esc | the focus back to the keyboard (notes on the computer keyboard again) |

The focused control carries a **focus ring**: a dark outline with the yellow edit light inside, so it stands out on the
light housing, the night housing and the dark display. It shows only while you move with Tab / F6; a click hides it.

**Notes on the computer keyboard** play while the on-screen keyboard has the focus (by default, after Esc or a click
on the keys): A W S E D F T G Y H U J K O L P play C to D♯ an octave up. None of the navigation keys above is a note
key, so Tab, F6, the arrows, Page, Enter, Space, Delete and Esc never play notes. While another control has the focus,
the letters do not play notes – they go on to the shortcuts (standalone: R, L, I, O, T, K). The letters T, K, L and O
are both notes and standalone shortcuts: with the keyboard focused they play notes; press Tab once to use them as
shortcuts. Space is PLAY / stop in the standalone app – unless a key has the focus, then Space presses that key.

## Screen reader

Every control has a name and a value in the UI language (English or German, switched with the language): keys
(with their state, e.g. a selected engine or mode is *checked*), encoders (the bound parameter's full name and its
value as the plate shows it), wheels, LIFE, portamento, glide, part selector, preset strip, tempo, TAP, Key Gravity, the
key readout, the metronome strip, the tape and keyboard readouts and the display (header title, page, status and the
numbered soft keys). Value controls can be set by the screen reader (in percent of their range). Tooltips are read as
the control's help text (every control has one, English and German); **Ctrl / Cmd + F** opens the help search, which
finds topics, parameters and controls by typing (see *Tooltips and help search* in the user interface chapter). Ohpe uses the operating system's accessibility interface (VoiceOver on macOS, Narrator / NVDA
on Windows; on Linux as far as the desktop supports it).

## Contrast

Both skins meet WCAG AA where the design allows it:

- legends on the housing and on the keys, display text and the dim display texts: at least 4.5 : 1 (light and night);
- every lit light bar has a fine edge in its own colour, deepened in the light skin and lifted in the night skin, to at
  least 3 : 1 on its key (non-text contrast) – the yellow bar on the warm-white housing is no longer only a hint;
- the focus ring: at least 3 : 1 on every surface (housing, panels, keys, display).

The coloured display accents (violet, red) keep their colour; they always come with text or a symbol, never as the only
sign.

See also: [user interface](USER-INTERFACE.md) · [settings](SETTINGS.md).
