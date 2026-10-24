# Multiband Comp

*Ohpe – designed by groovex.* [Deutsch](../de/MULTIBAND-KOMPRESSOR.md)

**Multiband Comp** is an FX slot type (every slot of every part rack and of the master rack) for punch and density at a
controlled level – "more pressure" without the level running away. It splits the sound into three bands – **Low**,
**Mid** and **High** – between two phase-coherent Linkwitz-Riley crossovers (4th order: the bands add up to an
all-pass, so a neutral setting is flat in level). Every band works in both directions:

* **Downward** – above its **Down threshold** the band is pushed down, with a very high ratio (60:1 by default).
* **Upward** – below its **Up threshold** the band is lifted (4:1 by default, at most +24 dB; the lift fades out
  towards the noise floor below -78 dBFS, so true silence stays silent).

Quiet details and tails come forward, peaks are held: the sound gets denser and louder-sounding at the same peak level.
Every band has its own **attack**, **release**, **gain** and **solo**. The defaults are very fast (attack 2 / 1 /
0.5 ms) so onsets are caught.

## Global controls

| Control | What it does | Range | Default |
|---|---|---|---|
| **Depth** | wet / dry: how much of the processed bands you hear (applied per band, so the crossover phase stays coherent) | 0 – 100 % | 40 % |
| **Time** | scales every attack and release together | 0 – 1000 % | 100 % |
| **Upward** | how much of the upward half acts | 0 – 100 % | 100 % |
| **Downward** | how much of the downward half acts | 0 – 100 % | 100 % |
| **In** | drives the bands into the thresholds | −24 – +24 dB | 0 dB |
| **Out** | output level | −24 – +24 dB | 0 dB |
| **Low Split / High Split** | the crossover frequencies | 40 – 1000 Hz / 1 – 12 kHz | 120 Hz / 2.5 kHz |

Per band (Low, Mid, High): Down Threshold (−60 – 0 dB), Up Threshold (−80 – 0 dB), Down Ratio and Up Ratio (1:1 –
100:1), Attack (0.1 – 200 ms), Release (5 – 2000 ms), Gain (−24 – +24 dB), Solo.

**Output safety:** a zero-latency stage at the end (instant attack, 50 ms release, ceiling -1.5 dBFS sample peak –
about -1 dBTP) only acts when the output would go over; a red dot above OUT in the editor shows it working.

**Onsets:** every band detector has the same onset guard as the Compressor (see below): after a quiet passage the
first hit is caught at once – neither the lift of the quiet part nor a released gain lets it through.

## In the FX editor

Three band meters, level −80 – 0 dBFS from bottom to top:

* the band's level now (violet), its gain change now on the right edge (yellow = lifted, red = pushed down);
* the zone above the **Down threshold** (red tint) and below the **Up threshold** (yellow tint); drag the right point
  of a band for the Down threshold, the left point for the Up threshold; Alt-drag or the mouse wheel on a point
  changes its ratio (shown under the band name);
* the row under each band is its gain; the split frequencies sit between the bands; OUT on the right (Depth on its
  wheel / Alt-drag). Every other control is a bar next to the picture.

**STYLE** (next to the slot name) loads a starting point: **Squash** (everything at once, dense and loud), **Glue**
(gentle bus glue), **Air & Weight** (lows and highs lifted, mids held back), **Punch** (slower attacks let the
transient through, the body is held), **Density** (mostly upward) and **Gentle** (a light touch for keys and pads).
A style sets the effect's parameters like a normal edit (undo / automation of the host).

All parameters are host-automatable and MIDI-mappable; matrix FX Param 1 – 4 = Depth, Time, Downward, Upward.
Parameter IDs: [PARAMETERS](../PARAMETERS.md#multiband-comp-p88).

## The compressors catch the first hit (p88)

The Compressor rack type, the channel compressor and the master compressor share one detector. It keeps the held peak
detector (no gain ripple on a bass) and adds an **onset guard**: a fast peak path with instant attack and the same
release follows the static curve; the applied gain reduction is never smaller than the guard's minus a small transient
allowance (0.3 dB per ms of attack, at most 3 dB). Before, a compressor fully released during silence let the first
hit of a note or chord pass with the whole make-up gain on top (+24 dB make-up: chords up to +14 dBTP). The release is
program-dependent: a lone hit lets go up to 1.7× faster, sustained compression uses the set release. No latency is
added; the sustained sound stays as it was.
