# Tape memory (p67)

Ohpe – designed by groovex. [Deutsch](../de/TAPE-MEMORY.md)

Ohpe never forgets the last half minute. While it runs it keeps a rolling memory of its own output, and one key –
**CAPTURE** – turns what you just played into a new instrument. Played a phrase you liked by accident, a happy
accident on the FX, a chord that rang out just right? Press CAPTURE and play it back across the keyboard.

## What it keeps

* **Source** (`tapemem.src`): the main output (before the metronome click) or one part after its FX rack.
* **Length** (`tapemem.len`): how much a capture takes – 5, 10, 20 or 30 s. The memory always holds 30 s.
* The memory is a preallocated ring buffer with 16-bit frames (+6 dB headroom): about 6 MB at 48 kHz. Writing it costs
  the audio thread no allocation and no lock; nothing is written to disk until you capture.

## CAPTURE

* Tape page: **CAPTURE** at the end of the first soft-key row, **MEMORY** (the settings) at the end of the second.
* Sampler page: **CAPTURE** (fourth key) captures into the part you are looking at.
* MIDI: `tapemem.go` (*Capture*) – MIDI learn from the MEMORY menu or the MIDI menu's remote controls.
* Grid-Controller: the *Add Track* button. It is dim while the memory holds sound and blinks while a capture is
  analysed.

The target part (`tapemem.to`) is the edit part unless you choose Part 1–4. The analysis – trimming the silence,
finding the onsets and the pitch, retuning – runs on a worker thread; the display reports the result
("Captured into Part 2: chromatic, root A3"). The part switches to the Sampler.

## Two modes (`tapemem.mode`)

* **Chromatic** – one playable sound: the strongest note of the memory (the stretch between two onsets with the most
  energy, at most 8 s; the whole memory when it has no onsets). Its pitch is detected (50 Hz – 2 kHz) and corrected
  to the nearest semitone, and the zone's root key is set to that note – so the keyboard plays it in tune. Sounds
  without a clear pitch (noise, drums) keep their pitch with root C4.
* **Slices** – the memory (silence trimmed, normalised) in the Sampler's slice mode: one slice per onset, from C2
  upwards (the drum pads). *Slice Sensitivity* on the Sampler page re-slices it.

## Saved with the session, exported as WAV

The last capture of each part is saved inside the session or project (state section `tapememory`, see
[file formats](../FORMATS.md)) as long as the part's Sampler still plays it – no external file is needed. MEMORY >
*Export last capture as WAV* writes a 24-bit WAV (the root as its unity note) into the capture library:
`<user application data>/groovex/Ohpe/Captures` (the environment variable `OHPE_CAPTURE_DIR` overrides it).
MEMORY > *Clear the memory* forgets what is held.

## Loading another sound after a capture

A preset chosen in the library or with the preset strip arrows loads completely into the part – engine, factory
instrument or sample, all parameters – even while the part plays a capture: the capture leaves the part's Sampler,
so it never covers the new sound. It is not lost: it stays in this session's memory, MEMORY > *Restore last
capture* puts it back into its part (engine Sampler, mode and root as captured) and *Export last capture as WAV*
still writes it; the display says so when it happens. Browsing the library without loading brings the capture back
when the library closes. A project loads its own sound into every part the same way; a project saved while a part
plays a capture keeps that capture (audio inside) and brings it back. Part presets store the sound's settings, not
the captured audio. RANDOM and BREED shape the captured sound (they never change the engine or the sample choice),
so the capture stays. A capture belongs to its part only; other parts and presets never pick it up.

## Parameters

`tapemem.len`, `tapemem.src`, `tapemem.mode`, `tapemem.to`, `tapemem.go` – see [PARAMETERS.md](../PARAMETERS.md#p67--tape-memory).
They are tools, not sound: presets do not store them.
