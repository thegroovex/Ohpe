# Ohpe design language

*English · Deutsch: [de/DESIGN.md](de/DESIGN.md)*

Ohpe is an instrument, not a window full of widgets. Its look follows a minimalist,
Scandinavian-industrial idea of its own: a light anodised body, graphite keys, one deep display and
four functional colours. Code: `src/ui/Palette.h` (colours, fonts), `src/ui/Pictograms.cpp`
(characters), `src/ui/Display.cpp`, `src/ui/ProView.cpp`, `src/ui/LibraryBrowser.cpp`.

## Principles

1. **One surface.** Everything sits on one aluminium plate on a 10-unit grid (1200 × 800 design
   units, scaled uniformly). Overlays (library, pro view) lie *on* the plate like a lid, the
   keyboard stays visible and playable.
2. **Colour means function.** The four accents belong to the four encoders and appear only where
   the matching value is: signal red (1), sun yellow (2), ultraviolet (3), rose (4). Red also marks
   recording and the active mode, yellow the engine and the edit part.
3. **The display is the only screen.** Deep "night" glass with a faint dot grid, warm paper-white
   text. Values in a monospace face, names in Space Grotesk.
4. **Depth, not decoration (0.7).** The display is a sunk window with layers: an inset bezel (shaded
   above, lit at the lower lip), glass with light falling in from above and darker corners, the dot
   grid behind the content, the content, the glass surface on top (inner shadow, a faint diagonal
   sheen). The back layers shift slightly against the front with the mouse (parallax); the
   character casts a soft shadow. No textures, no chrome, no reflections of objects.
5. **Direct manipulation.** What you see is the control: display bars, pro-view bars, steps, pads,
   track lanes and soft keys react to the mouse; fine control is always Shift / Cmd / Ctrl.
6. **Characters, not icons.** Each engine and mode has an animated pictogram character drawn from
   lines and dots. On the display it reacts to the four macros of its page and to the notes being
   played; on the keys it is a static icon in the same style.

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
| Keys | graphite caps, pictogram, engraved legend below; active = accent dot + coloured pictogram |
| Encoders | flat round knobs with the accent ring; small dot = MIDI-mapped; square marker = step lock |
| Modulation ring (0.7) | a thin translucent arc inside the encoder ring (a line above the bar on display rows) = reach of the matrix routings; ink dot / tick = live position |
| Display rows | dot (square when locked) · name · value in the accent colour · bar with handle |
| Display views (0.7) | character / stereo scope / spectrum terrain on the left half; click cycles, right-click = menu |
| Stereo scope | diamond goniometer (mid up, L upper left, R upper right), trace in paper white with an ultraviolet glow, correlation bar (red when negative), M / S bars in the colours of the Mid / Side encoders |
| Spectrum terrain | perspective waterfall: lines from paper white (front, ultraviolet glow) to dim grey (horizon), each row hides the rows behind it; flat and still in silence; off with "Animated 3D visuals" |
| Soft keys | outlined pills at the bottom of the display; red = active |
| Transport keys (p11) | REC / PLAY large, then to-the-start / stop / loop, bottom left; plain geometric signs (circle, triangle, square, bar + triangle, open loop with arrow); the record circle is always signal red and the key blinks while recording |
| Tape transport panel (p11) | the tape page's left half: state badge (red fill blinking = recording), armed tracks in red, monospace clock and bar.beat, reels turning with the tape position, five outlined buttons (filled when active; record filled red), a hint line in yellow while nothing is armed |
| Overlays | the library and the pro view hide every control and legend of the plate under them (no labels peeking over the lid's edge) |
| Part badge | yellow pill "P1"–"P4" in the display header |
| 3D badge (1.0) | pill with the headphones pictogram and "3D" after the part badge in the display header: filled ultraviolet and gently pulsing while the edit part plays binaural 3D, outlined sun yellow with speakers in speaker mode, a dim outline otherwise; a click opens the Space view, the tooltip says "Binaural 3D – best with headphones". A long title is shortened with an ellipsis before it can push the badges into the tempo |
| Part selector, library (1.0) | the part keys carry the headphones pictogram in their corner while the part uses the space (lit and pulsing = binaural, speakers = speaker mode); presets tagged "3D" carry it after their name in the library list |
| Pro view | dark panel, mode tabs, groups in columns with a coloured tick; bars like the display rows |
| Library | dark panel: category chips in the accent colours, engine chips, list with stars, details / save panel |

## Text

English and German everywhere (`src/plugin/Localisation.cpp`, English text = key). Legends on the
body are small caps in engraving grey; German upper case maps umlauts (`i18n::upper`). Texts that
may be long are elided or fitted, never clipped mid-glyph.
