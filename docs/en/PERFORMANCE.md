# Performance (p35)

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

## Numbers

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

Compare numbers from the same machine only. CI prints a quick run in every build's summary for information; nothing
gates on timing. The tool exits with code 2 when a scenario renders a non-finite or runaway output (a correctness
check).

## Tips

* Larger buffer sizes in your host lower the load per block and make peaks less likely.
* Voices cost the most: the voice count per part and layers raise the load roughly in proportion.
* Long reverbs and delays keep a part awake until their tails fall below −120 dBFS.
