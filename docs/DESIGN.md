# Ohpe design language

*English · Deutsch: [de/DESIGN.md](de/DESIGN.md)*

Ohpe is an instrument, not a window full of widgets. Its look is functional minimalism of its own:
less, but better. A matte warm-white housing, grey recessed panels, matte light keys with soft
depth, one big black display and four functional colours. Nothing is decoration: every line,
shadow and colour says what a control is or what it does. Code: `src/ui/Palette.h` (colours,
fonts), `src/ui/Widgets.cpp` (keys, encoders, wheels, keyboard), `src/ui/PluginEditor.cpp`
(layout, housing), `src/ui/Pictograms.cpp` (characters), `src/ui/Display.cpp`, `src/ui/ProView.cpp`,
`src/ui/LibraryBrowser.cpp`.

![The redesigned plate (p14)](images/ohpe-analog.png)

## Principles

1. **One housing (p14).** Everything sits on one matte warm-white body with generous margins and a
   soft shadow on the backdrop, laid out on an 8-unit grid (1200 × 800 design units, scaled
   uniformly). No metal, no grain, no texture. Functional groups lie in grey **recessed panels**:
   ENGINE (left), MODE (right), TRANSPORT + MIDI (bottom left), OCTAVE (bottom right), the keyboard
   bed. Overlays (library, pro view) lie *on* the plate like a lid in a dark frame; the header and
   the keyboard row stay visible and playable. **One radius system:** the housing has an 18-unit
   corner radius, every recessed panel and frame (columns, transport, octave, keyboard bed, display
   frame, the header slots, the overlays) the same 8 units, and what sits inside a panel (keys,
   part keys, preset strip, display glass) 5 units - the curves read as concentric.
2. **Colour means function.** The four accents belong to the four encoders and appear only where
   the matching value is: signal red (1), sun yellow (2), ultraviolet (3), rose (4). The encoder
   rings and the display's macro rows share them. The key LEDs speak the same language: **yellow =
   your focus** (the edit part's engine, the page on the display), **ultraviolet = running / in use
   elsewhere** (another part's engine, a running sequencer, the tape playing, an FX slot on, a
   routed modulation), **red = recording**. Everything else is warm white, grey and graphite.
3. **The display is the only screen.** Big, high-contrast and recessed in a dark frame: deep
   "night" glass with a faint dot grid, warm paper-white text. Values in a monospace face, names in
   Space Grotesk.
4. **Depth, not decoration.** Keys are matte light caps with a lit upper edge and a soft two-layer
   shadow; pressed or active keys sink a little. Panels are sunk into the housing (shadow under the
   upper edge, light at the lower lip). The display's glass has layers (dot grid behind, content,
   glass surface on top) with a slight parallax. No chrome, no reflections of objects.
5. **Direct manipulation.** What you see is the control: display bars, pro-view bars, steps, pads,
   track lanes, soft keys and the wheels react to the mouse; fine control is always Shift / Cmd /
   Ctrl.
6. **Characters, not icons.** Each engine and mode has an animated pictogram character drawn from
   lines and dots. On the display it reacts to the four macros of its page and to the notes being
   played; on the keys it is a static ink icon in the same style.
7. **Pixel-clean.** The static housing is rendered once per size at the physical pixel scale (crisp
   on 2x screens, no repaint work while playing); every legend is fitted to its width, never clipped.

## Layout (p14)

| Area | Content |
|---|---|
| Header | OHPE wordmark, PART 1–4 (segmented), preset strip (black, yellow arrows), LIBRARY and PRO (dark pills), RANDOM and SAVE (round keys with the four-colour ring), EN / DE. One row: every control is centred on the strips' centre line (y 56), every legend (PART, PRESET, LIBRARY, PRO, RANDOM, SAVE, LANGUAGE) sits above its control on one baseline (y 20 – 32; a long legend is narrowed, never lowered); 8 units between the keys, and the row ends 32 units from the housing's right edge as the wordmark starts 32 units from its left edge |
| Left column | ENGINE: eight engine keys, 2 × 4 |
| Centre | the display; four encoders below it, 168 units apart |
| Right column | MODE: Synth, Envelope, LFO, Mixer, Effects, Sequencer, Tape, Drum |
| Bottom left | TRANSPORT: REC (red), PLAY, STOP / TO START, LOOP, MIDI |
| Bottom right | OCTAVE − / + |
| Keyboard row | PITCH and MOD wheels left, three-octave keyboard (C3–C6 by default), glide mode switch (Auto / Legato / Always) and GLIDE knob right |

## Pictogram characters

| Character | Macros (encoder 1–4) |
|---|---|
| Analog – wave creature | shape morphs the wave, the filter is a lowering curtain, resonance ripples |
| FM – planets | algorithm = orbit levels, ratio = speed, index = wobble, feedback = halo |
| Wavetable – stacked frames | position morphs the front frame, warp bends it, tone fades the back frames, unison fans out |
| Pluck – string / bar | material, damping (ringing time), brightness (wiggles), body (sound box) |
| Cluster – rows of teeth | detune jitters, spread fans out, tone fades upper rows, chord sets rows and spacing |
| Grain – cloud | position moves the cloud along the sample, size = grain size, density = number of grains, spray scatters |
| Vocal – singing face | vowel shapes the mouth (a e i o u), formant moves it, breath blows puffs, tension raises the brows |
| Sampler – waveform | start marker, tone darkens the tail, loop start / end bracket the loop |
| Drum – drum | model changes the shape (hat = cymbal), tune its size, decay the ringing, tone the strike flash |
| Envelope, LFO, Mixer, Effects, Sequencer, Tape | the curve, the moving dot, faders, echoes, the running step, the turning reels |
| Mod view – routing list (0.7) | takes the LFO character's place: 16 lines in two columns, source → target with an amount bar growing from the centre and the live value; below, the trace of the selected routing |
| Rack – chain of blocks (0.7) | takes the Effects character's place: one block per slot in signal order, joined by arrows; effect name, mix bar (yellow), output level (violet), BYPASS in red; empty slots dimmed, the selected one outlined |
| 3D space – the listener's sphere (1.0) | the Space view of the Effects mode (and the "3D space" display view): a head seen from slightly behind and above, inside floor rings and the median arc; every part with its stage on is a dot in its part colour with a fading trail and a drop line to the floor, nearer = larger, the edit part on top and labelled. While monitoring on headphones the head wears headphones, in speaker mode two speakers stand in front of it |
| Headphones pictogram (1.0) | an own line drawing in the style of the characters: band, two cups, sound arcs when lit. Lit in ultraviolet with a slow pulsing halo while a part plays binaural 3D, a pair of speakers in sun yellow while the 3D monitoring is set to speakers, dimmed when the part does not use the space |
| Rack – effect pictures (0.7) | the selected slot drawn as what it does: distortion transfer curve, EQ and filter response, compressor curve with its gain-reduction bar, multiband band gains, the output motion of the modulated effects |

## Components

| Component | Notes |
|---|---|
| Keys (p14) | matte light square caps, ink pictogram, engraved legend below; active = sunk cap. Engine, mode and the PLAY / REC / LOOP keys carry an **LED** instead of a lit bar |
| Key LEDs (p14) | a 7-unit matte lens in the top-left corner of the cap: unlit a grey pit, lit a flat colour with a soft halo on the cap - no glare, no specular dot. **Engine keys:** yellow = the edit part's engine, ultraviolet = used only by other parts; four tiny dots in the top-right corner show which parts (1 – 4, left to right) play the engine. **Mode keys:** yellow = the page on the display; ultraviolet = SEQUENCER running, TAPE playing, EFFECTS (any rack slot of a part or the master has a type and is not bypassed), DRUM (a running sequencer plays the drum kit), LFO (a part's LFO 1 has depth or a matrix routing has source, target and amount); red = TAPE recording (blinking). Status wins over the page colour; the page stays visible as the sunk cap. **Transport:** PLAY ultraviolet while running, REC red (steady = ready, blinking = recording), LOOP yellow while on. The tooltips say what each LED means |
| Pills (p14) | LIBRARY and PRO: dark graphite pills with a light pictogram; active = the pictogram and a short bar in the accent |
| Round keys (p14) | RANDOM (a die) and SAVE (a preset card with a plus): light round caps inside a ring of the four accent colours |
| Encoders | matte light knob on a soft shadow with an ink pointer line, the value arc in the encoder's accent on a light track; small dot = MIDI-mapped; square marker = step lock |
| Modulation ring (0.7) | a thin translucent arc inside the encoder ring (a line above the bar on display rows) = reach of the matrix routings; ink dot / tick = live position |
| Wheels (p14) | PITCH and MOD: graphite thumb wheels in dark recessed slots, ribs that roll with the value; a paper-white centre mark (pitch, springs back) or a yellow position line (mod); right-click = MIDI learn, a red dot = mapped |
| Glide (p14) | three-position slide switch Auto / Legato / Always (the selected label in ink) and a small knob with a graphite ring; both follow the edit part |
| Keyboard (p14) | matte warm-white and graphite keys with a narrow gap; pressed keys sink and show a short red bar; C labels at the left of each C |
| Display rows | dot (square when locked) · name · value in the accent colour · bar with handle |
| Display views (0.7) | character / stereo scope / spectrum terrain on the left half; click cycles, right-click = menu |
| Stereo scope | diamond goniometer (mid up, L upper left, R upper right), trace in paper white with an ultraviolet glow, correlation bar (red when negative), M / S bars in the colours of the Mid / Side encoders |
| Spectrum terrain | perspective waterfall: lines from paper white (front, ultraviolet glow) to dim grey (horizon), each row hides the rows behind it; flat and still in silence; off with "Animated 3D visuals" |
| Soft keys | outlined pills at the bottom of the display; red = active |
| Transport keys (p11, p14) | in the bottom-left panel: REC / PLAY / STOP, TO START / LOOP / MIDI; plain geometric signs (circle, triangle, square, bar + triangle, open loop with arrow); the record circle is always signal red and the key blinks while recording |
| Tape transport panel (p11) | the tape page's left half: state badge (red fill blinking = recording), armed tracks in red, monospace clock and bar.beat, reels turning with the tape position, five outlined buttons (filled when active; record filled red), a hint line in yellow while nothing is armed |
| Overlays | the library and the pro view lie in a dark frame over the plate and hide every control and legend under them (no labels peeking over the lid's edge) |
| Part badge | yellow pill "P1"–"P4" in the display header; the 3D badge follows it |
| 3D badge (1.0) | pill with the headphones pictogram and "3D" after the part badge in the display header: filled ultraviolet and gently pulsing while the edit part plays binaural 3D, outlined sun yellow with speakers in speaker mode, a dim outline otherwise; a click opens the Space view, the tooltip says "Binaural 3D – best with headphones". A long title is shortened with an ellipsis before it can push the badges into the tempo |
| Part selector, library (1.0) | the part keys carry the headphones pictogram in their corner while the part uses the space (lit and pulsing = binaural, speakers = speaker mode); presets tagged "3D" carry it after their name in the library list |
| Pro view | dark panel, mode tabs, groups in columns with a coloured tick; bars like the display rows |
| Library | dark panel: category chips in the accent colours, engine chips, list with stars, details / save panel |

## Text

English and German everywhere (`src/plugin/Localisation.cpp`, English text = key). Legends on the
body are small caps in engraving grey; German upper case maps umlauts (`i18n::upper`). Texts that
may be long are elided or fitted, never clipped mid-glyph.
