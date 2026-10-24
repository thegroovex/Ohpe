# Performance (p35, p79)

[Deutsch](../de/LEISTUNG.md)

![The CPU meter on the mode column's legend line](../images/ohpe-cpu-meter.png)

Ohpe computes four parts, their racks, the master and the display on your computer's processor. This page explains
what it costs, how to read the CPU meter, what Ohpe does to stay light, and how to measure it yourself.

## The CPU meter

`CPU 12 %` on the legend line of the mode column (right, next to MODE) shows how much of each audio block's time Ohpe
needs to compute its sound, averaged over about half a second. 100 % would be the whole time: the audio would break
up. The tooltip adds the slowest block of the last two seconds (held for two seconds, then falling). The legend turns
red from 80 % on average or when a block took longer than its own time.

* It measures Ohpe alone (the plug-in's own processing), not the host or other plug-ins.
* It is a display only: no parameter, not automatable, not saved.
* Short peaks right after loading a preset or a project are normal (new sounds are prepared once).

## What Ohpe does to stay light

| Measure | Effect |
|---|---|
| **Silent parts sleep** | A part whose voices have not played for 4.5 s and whose output (after resonator body, FX rack and 3D stage) stayed below −120 dBFS all that time stops rendering its effect chain until a note starts again. The hold is longer than the longest delay line of any effect, so no echo or reverb tail is ever cut, and a note wakes the part within the same block. |
| **Faster FM engine** | The operators use a polynomial sine instead of the library call, only the links an algorithm actually has are summed, operators at level 0 skip their sine, and each operator is computed over a whole control block at once. The sound stays the same (tests compare it with the previous engine). |
| **Lighter effects** | The hall's line modulation and the phaser's sweep use the same polynomial sine. Mixer sends at 0 and switched-off outputs cost nothing. |
| **Only changed parameters** | Per audio block the core reads only the parameters that changed (instead of all of them), and the plug-in passes only changed host values on. Automation reaches the sound exactly as before. |
| **Shared built-in tones** | Built-in sample material (the Grain texture, the Sampler's tone) exists once per process, not once per part or instance: a second instance starts in a few milliseconds. |
| **Limiter only when needed** (p79) | The master limiter skips its true-peak interpolation in blocks where no peak can reach the ceiling. The output is bit-identical. |
| **Sends and master rack sleep** (p79) | Like the parts, the two send effects and the master rack stop computing after their input and output stayed below −120 dBFS for longer than their longest tail; a note wakes them in the same block. |
| **No denormals** (p79) | The core flushes tiny floating-point values to zero while it computes, so fading tails never slow the processor down. |
| **Cheaper voices** (p79) | Pluck recomputes its string filter only when a value changes (bit-identical); Grain's window and Vocal's glottal pulse use the polynomial sine; the 3D stage and Key Gravity compute once per block or per change what they computed per sample or per frame. |
| **Quieter display** (p79) | When nothing sounds, runs or changes for two seconds and nothing on the display moves with the clock (3D scene, character, meters, scopes, blinking keys), the display repaints at 10 instead of 30 frames per second. Any change brings it back at once. |

## Numbers (p79, path to 1.0)

Measured with `ohpe_bench --seconds 1.5 --hash` at 48 kHz, block 256, best of 3 runs, on a 2-core 64-bit Linux build
machine. "Before" is main before this pass (0.7.3), "after" is this pass; both ran one after the other on the same
machine. Percent of one CPU core – lower is better. "Output" compares a hash of the rendered audio: *bit-identical*
means the sound did not change at all.

| Scenario | Before avg % | After avg % | Change | Before peak % | After peak % | Output |
|---|---:|---:|---:|---:|---:|---|
| `idle` | 1.78 | 0.84 | -53 % | 2.50 | 1.07 | bit-identical |
| `typical.fm` | 7.59 | 7.25 | -4 % | 8.64 | 12.10 | bit-identical |
| `typical.wavetable` | 7.95 | 7.20 | -9 % | 20.38 | 10.14 | bit-identical |
| `typical.analog` | 6.98 | 6.06 | -13 % | 10.93 | 9.07 | bit-identical |
| `typical.pluck` | 6.66 | 3.93 | -41 % | 19.30 | 5.02 | bit-identical |
| `typical.grain` | 8.85 | 7.73 | -13 % | 23.15 | 11.58 | changed: polynomial sine (window) |
| `typical.vocal` | 7.51 | 7.07 | -6 % | 8.37 | 11.39 | changed: polynomial sine (glottal pulse) |
| `typical.cluster` | 8.39 | 7.73 | -8 % | 9.05 | 9.66 | bit-identical |
| `typical.sampler` | 3.83 | 2.97 | -22 % | 4.55 | 4.48 | bit-identical |
| `worst.fm` | 21.13 | 21.09 | 0 % | 21.92 | 22.45 | bit-identical |
| `worst.wavetable` | 20.29 | 20.14 | -1 % | 20.96 | 21.65 | bit-identical |
| `worst.analog` | 18.83 | 15.91 | -16 % | 19.61 | 17.80 | bit-identical |
| `worst.pluck` | 21.00 | 18.14 | -14 % | 21.79 | 21.01 | changed: grain layer |
| `worst.grain` | 22.12 | 21.94 | -1 % | 23.02 | 24.87 | changed: polynomial sine |
| `worst.vocal` | 21.76 | 21.89 | +1 % | 22.51 | 24.89 | changed: polynomial sine |
| `worst.cluster` | 17.71 | 17.59 | -1 % | 18.65 | 20.82 | bit-identical |
| `worst.sampler` | 16.69 | 16.56 | -1 % | 17.55 | 19.41 | bit-identical |
| `one-part.fm` | 8.69 | 7.88 | -9 % | 11.20 | 11.20 | bit-identical |
| `silent-racks` | 1.68 | 0.67 | -60 % | 2.01 | 1.02 | bit-identical |
| `morph.4x4` | 25.68 | 23.41 | -9 % | 26.66 | 26.68 | bit-identical |
| `chord16.fm` | 3.23 | 2.64 | -18 % | 3.63 | 3.33 | bit-identical |
| `chord16.wavetable` | 3.32 | 2.40 | -28 % | 3.66 | 2.92 | bit-identical |
| `chord16.analog` | 3.01 | 2.10 | -30 % | 3.41 | 2.59 | bit-identical |
| `chord16.pluck` | 2.93 | 1.50 | -49 % | 3.31 | 1.92 | bit-identical |
| `chord16.grain` | 3.58 | 2.61 | -27 % | 4.13 | 3.26 | changed: polynomial sine |
| `chord16.vocal` | 3.18 | 2.34 | -26 % | 3.43 | 2.93 | changed: polynomial sine |
| `chord16.cluster` | 3.46 | 2.56 | -26 % | 3.75 | 3.13 | bit-identical |
| `chord16.sampler` | 2.17 | 1.28 | -41 % | 2.67 | 1.91 | bit-identical |
| `drums` | 2.13 | 1.44 | -32 % | 2.66 | 2.17 | bit-identical |
| `fx.full` | 9.73 | 8.83 | -9 % | 12.16 | 13.78 | bit-identical |
| `space3d` | 11.46 | 11.19 | -2 % | 11.99 | 13.36 | bit-identical |
| `gravity` | 10.91 | 8.72 | -20 % | 11.59 | 9.95 | changed: Key Gravity weights cached (RMS equal to 6 digits) |
| `everything` | 26.55 | 25.27 | -5 % | 27.45 | 28.46 | bit-identical |
| `silent.after-play` | 3.06 | 0.75 | -75 % | 3.46 | 2.44 | changed: Key Gravity in the setup |
| `silent.master-fx` | 2.96 | 0.69 | -77 % | 3.42 | 0.79 | bit-identical |
| `silent.long-after` (new) | – | 0.72 | – | – | 0.83 | – |
| **sum** | **346.8** | **311.0** | **-10 %** | | | |

Startup: about 378 ms for the first instance in a process and 21 ms for each further one (369 / 19 ms before; within
run-to-run noise). Peaks vary more from run to run than averages; the averages are the comparison that counts.

The quick runs on the CI runners of all three systems (`--quick`, 0.5 s × 2; before = main 0.7.3, after = this pass,
different runs on shared machines):

| Scenario | Linux before → after | macOS before → after | Windows before → after |
|---|---:|---:|---:|
| `idle` | 1.09 → 1.12 | 1.42 → 0.74 | 1.82 → 1.17 |
| `typical.fm` | 5.34 → 8.52 | 8.53 → 6.41 | 9.36 → 9.24 |
| `typical.pluck` | 5.68 → 4.66 | 8.24 → 3.93 | 8.10 → 4.93 |
| `typical.grain` | 6.24 → 10.09 | 8.87 → 7.81 | 14.01 → 11.88 |
| `worst.analog` | 15.31 → 19.92 | 18.72 → 16.27 | 25.26 → 21.84 |
| `silent-racks` | 0.98 → 0.76 | 2.54 → 0.60 | 1.72 → 0.88 |
| `morph.4x4` | 19.48 → 29.23 | 36.63 → 26.58 | 37.24 → 34.78 |
| `chord16.pluck` | 2.19 → 1.86 | 3.26 → 1.40 | 3.29 → 1.89 |
| `drums` | 1.54 → 1.72 | 3.96 → 1.10 | 2.60 → 1.88 |
| `gravity` | 7.59 → 10.40 | 17.58 → 11.36 | 16.58 → 13.71 |
| `everything` | 19.14 → 31.48 | 31.62 → 17.34 | 36.85 → 34.26 |
| `silent.master-fx` | 1.95 → 0.77 | 2.45 → 0.52 | 3.57 → 0.88 |
| **sum, all 35** | **260 → 401** (+54 %) | **372 → 313** (-16 %) | **477 → 438** (-8 %) |

The Linux runner of the "after" run was a slower machine: bit-identical scenarios such as `typical.fm` rose just as
much, so read the Linux column as noise; the silent scenarios still fell. The macOS and Windows columns agree with the
local table.

New scenarios: *chord16.\** = one part, 16-note chord on 16 voices, no effects; *drums* = the drum kit, sixteenths at
120 BPM; *fx.full* = 6 effects on every part, 4 master effects, both sends; *space3d* = 3D stage moving with room on
every part; *gravity* = Key Gravity 80 % on every part; *everything* = all of it at once; *silent.after-play* = the full
setup played for 1 s and released, measured after 9 s; *silent.long-after* = the same after 19 s (everything asleep);
*silent.master-fx* = no notes, master effects and sends.

## Numbers (p35)

Measured with `ohpe_bench` (below) at 48 kHz, block 256, 2 s per scenario, best of 3 runs, on a 2-core 64-bit Linux
build machine. "Before" is the state before this pass (0.4.0 plus the features merged since), "after" is this pass;
both were run one after the other on the same machine. Percent of one CPU core – lower is better. Peaks vary more from
run to run than averages; the averages are the comparison that counts.

| Scenario | Before avg % | After avg % | Change | Before peak % | After peak % |
|---|---:|---:|---:|---:|---:|
| `idle` | 1.89 | 1.62 | -14 % | 2.05 | 2.08 |
| `typical.fm` | 12.76 | 7.46 | -42 % | 13.29 | 9.96 |
| `typical.wavetable` | 7.89 | 7.73 | -2 % | 8.47 | 9.22 |
| `typical.analog` | 6.94 | 6.79 | -2 % | 7.56 | 8.73 |
| `typical.pluck` | 6.59 | 6.44 | -2 % | 7.13 | 7.66 |
| `typical.grain` | 8.76 | 8.60 | -2 % | 9.48 | 10.26 |
| `typical.vocal` | 7.59 | 7.36 | -3 % | 8.30 | 8.34 |
| `typical.cluster` | 8.47 | 8.27 | -2 % | 9.06 | 10.05 |
| `typical.sampler` | 3.70 | 3.47 | -6 % | 4.52 | 4.06 |
| `worst.fm` | 27.15 | 20.98 | -23 % | 27.78 | 24.86 |
| `worst.wavetable` | 20.37 | 20.15 | -1 % | 21.02 | 21.49 |
| `worst.analog` | 18.86 | 18.67 | -1 % | 19.49 | 20.19 |
| `worst.pluck` | 21.04 | 20.90 | -1 % | 21.84 | 24.84 |
| `worst.grain` | 22.15 | 21.90 | -1 % | 22.90 | 23.54 |
| `worst.vocal` | 21.85 | 21.62 | -1 % | 22.61 | 22.56 |
| `worst.cluster` | 17.56 | 17.25 | -2 % | 18.45 | 18.03 |
| `worst.sampler` | 22.57 | 16.27 | -28 % | 23.54 | 17.16 |
| `one-part.fm` | 12.15 | 4.95 | -59 % | 12.80 | 10.27 |
| `silent-racks` | 9.11 | 1.61 | -82 % | 9.66 | 1.75 |
| `morph.4x4` | 30.09 | 25.02 | -17 % | 30.98 | 26.68 |
| **sum** | **287.5** | **247.1** | **-14 %** | | |

Startup (build and prepare the core): about 370 ms for the first instance in a process and 18 ms for each further one
(after this pass; the earlier bench did not measure startup).

Scenarios: *idle* = default sound, no notes; *typical* = 4 parts × 8 voices of one engine, 2 effects per part;
*worst* = 4 parts × 16 voices plus a layer, 6 effects per part; *one-part.fm* = one FM part playing, three silent parts
with full racks; *silent-racks* = no notes, 6 effects on every part, tails long gone; *morph.4x4* = morph on with four
sounds in every part.

What this means in practice: a typical project with four playing parts takes well under a tenth of one core on that
machine; parts you are not using cost close to nothing once their tails have died away.

## Measuring it yourself

`ohpe_bench` is built with the core tests (`cmake --build build --target ohpe_bench`, it lands in
`build/tests/core/`). It renders the scenarios with the core alone – no plug-in, no UI – and prints average and
slowest block per scenario.

| Option | Meaning |
|---|---|
| (none) | all scenarios, 48 kHz, block 256, 2 s each, best of 3 runs |
| `--quick` | shorter runs (as in CI) |
| `--block N` / `--rate HZ` | another block size / sample rate |
| `--seconds S` / `--repeats N` | longer runs / more runs |
| `--filter TEXT` | only scenarios whose name contains TEXT |
| `--csv` / `--list` | machine-readable output / scenario names only |
| `--hash` | adds a hash and the RMS of each scenario's output (to prove an optimisation does not change the sound) |
| `--baseline FILE` / `--write FILE` | compare the averages with an earlier `--csv` run / write this run as a new one |

Compare numbers from the same machine only. CI runs a quick bench on Linux, macOS and Windows, compares it with
`tools/bench/baseline-<os>.csv`, prints it in the build summary and uploads it as the artifact `bench-<os>`, for
information; nothing gates on timing. The tool exits with code 2 when a scenario renders a non-finite or runaway output (a correctness
check).

## Tips

* Larger buffer sizes in your host lower the load per block and make peaks less likely.
* Voices cost the most: the voice count per part and layers raise the load roughly in proportion.
* Long reverbs and delays keep a part awake until their tails fall below −120 dBFS.
