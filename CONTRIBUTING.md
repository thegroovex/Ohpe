# Contributing to Ohpe

*English · Deutsch: [docs/de/MITWIRKEN.md](docs/de/MITWIRKEN.md)*

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) first – it explains the core / shell split, the
real-time rules and how to add engines, sequencers and effects.

## Before every push: run all tests

```sh
PLUGINVAL=/path/to/pluginval scripts/test-all.sh
```

The script builds and runs, in this order:

1. **Core tests** (`build-core`, no JUCE, seconds) – DSP, engines, sequencer, mixer, MIDI map,
   state, parameter registry, including *"parameter IDs are append-only"*.
2. **Plug-in tests** (`build`) – processor, editor, localisation, multi-out, state round trip.
3. **VST3 host test** – loads the built `Ohpe.vst3` like a DAW: instrument, 9 output buses,
   parameters, a note plays, state restored in a second instance.
4. **pluginval** at strictness level 5 on the VST3 (skipped with a notice if `PLUGINVAL` is not
   set; CI always runs it).

Useful variables: `OHPE_JUCE_PATH` (local JUCE 8.0.15), `OHPE_CMAKE_ARGS="-DOHPE_LTO=OFF"`
(much faster local links), `OHPE_CORE_ONLY=1` (core tests only). On headless Linux the GUI
steps run under `xvfb-run` automatically.

Never push a red build. CI runs the same steps on Linux, macOS universal (app, AU incl.
`auval -strict`, VST3) and Windows for every push to main, every pull request and every manual run
of the CI workflow. Merge only when all three are green ("CI result"). Each platform is built once
(ccache, cached dependencies); the tests then run in parallel jobs, every doctest test case as its
own process (`scripts/ci/run-tests.py`, also used by `test-all.sh`). Long test cases that loop over
many items (every factory preset ...) are tagged `doctest::test_suite ("sliced")` and split with
`tests/TestSlice.h`. The macOS installer is built on main, on manual runs and for pull requests with
the label `installer`. Pull requests that only touch `docs/**` or `*.md` skip the builds.

Factory presets are `*.ohpepreset` files in `resources/presets/factory/` (embedded at build time);
the format is described in [docs/en/PRESETS.md](docs/en/PRESETS.md).

## Rules

* **Parameter IDs are append-only.** Never remove, rename, reorder or re-range a parameter, never
  reorder choices. Append new parameters at the bottom of `src/core/ParameterLayout.cpp` and
  append their IDs to `tests/core/golden/param_ids_v1.txt`.
* **Real-time safety** in everything called from `Core::process()`: no allocation, locks, I/O
  or exceptions.
* **Every user-visible text in English and German.** Strings go through `i18n::tr()`, the German
  text into `src/plugin/Localisation.cpp`; parameter descriptors carry both names. The localisation
  test checks that every key has a German text.
* **Own design only.** Ohpe has its own name, layout, palette and pictograms. Do not name or
  imitate other instruments or their makers – in code, comments, docs, commit messages or assets.
* New code comes with tests (see the checklists in the architecture document).
* Style: C++17, 4 spaces, Allman braces, `camelCase` members, a space before call parentheses
  as in the existing code. Every source file starts with the two-line header
  (`// Ohpe - designed by groovex` and the SPDX line).

## Documentation screenshots

```sh
xvfb-run -a build/ohpe_screenshot_artefacts/Release/ohpe_screenshot docs/images
```

renders every view in English and German into `docs/images/`. Re-render them when the UI changes.

## License

By contributing you agree that your contribution is licensed under the AGPL-3.0-or-later.
