# Preset library

*English · Deutsch: [../de/PRESETS.md](../de/PRESETS.md)*

![Preset library](../images/ohpe-library.png)

## Using the library

Press **LIBRARY** (top of the instrument) to open the library. It covers the keys and the display;
the keyboard stays playable, so you can audition sounds while you browse.

| Area | What it does |
|---|---|
| **Categories** (left) | Bass, Keys, Pads, Leads, Plucks, Drums, FX, Arp, Textures, Vocal. Select one or several: the list shows presets that carry **any** of the selected tags. |
| **Engine** (left) | Only presets for one engine (or *All*). |
| **Search** (top) | Words are matched against name, author, tags, engine and comment (case does not matter; German category and engine names work too). All words must match. |
| **All / Factory / User** | Which bank: the built-in factory presets, your own presets, or both. |
| **Parts / Projects** | Only part presets (one sound) or only project presets (the whole instrument). |
| **★ Favourites** | Only presets you marked as favourite. |
| **Sort** | By name, rating (best first), engine or newest. |
| **List** | Selecting a preset **loads it at once** (preview) into the edit part – part presets into the part chosen with the part selector, project presets replace everything. Click the star on the left to mark a favourite, click the stars on the right to rate (click the same star again to clear). Arrow keys move through the list. |
| **Details** (right) | Name, bank, kind, engine, author, tags, comment, favourite and rating. User presets can be deleted (moved to the trash). |
| **Keep / Cancel** | *Keep* (or Enter, or a double-click on a preset) closes the library with the preset loaded. *Cancel* (or Esc) restores the sound you had before the library was opened. |

The **preset strip** next to the part selector shows the preset loaded last; its arrows load the
previous / next preset of the library's current list (with its filters and sort order), a click on
the name opens the library.

### Saving

*Save preset …* opens the save panel: name, author (remembered), tags (categories plus your own
tags, comma separated) and what to save – **This part** (the edit part's sound) or **Whole project**
(every part, drum kit, mixer, effects, sequencer patterns and settings). Presets with the same name
are never overwritten; a number is added instead.

User presets are files in the user folder, so you can copy, share and back them up:

| System | User folder |
|---|---|
| macOS | `~/Library/Application Support/groovex/Ohpe/Presets` |
| Windows | `%APPDATA%\groovex\Ohpe\Presets` |
| Linux | `~/.config/groovex/Ohpe/Presets` |

Part presets go to `Parts/`, project presets to `Projects/`; sub-folders are scanned too. The
environment variable `OHPE_PRESET_DIR` points Ohpe at another folder. Favourites, ratings and the
author name are kept in `library.json` in the same folder. You can also drop a `.ohpepreset` file
onto the instrument to load it.

### MIDI

Three remote-control parameters make the library playable from a controller (right-click the
LIBRARY key, or *MIDI → Remote controls*, then *MIDI Learn*):

| Parameter | ID | Action |
|---|---|---|
| Library | `lib.open` | opens / closes the library |
| Previous Preset | `lib.prev` | loads the previous preset of the current list |
| Next Preset | `lib.next` | loads the next preset of the current list |
| Edit Part | `ui.part` | selects the edit part 1–4 (where part presets load) |

A trigger fires when its control goes to "on" (CC value 64 or more, or a note); pressing and
releasing a button sends one step.

## File format (version 2)

A preset is one UTF-8 JSON file with the extension `.ohpepreset`:

```json
{
  "format": "ohpe-preset",
  "version": 2,
  "kind": "part",
  "name": "Glass Keys",
  "author": "groovex",
  "tags": ["Keys", "Plucks", "bright"],
  "engine": "fm",
  "comment": "",
  "params": { "engine": 0, "fm.algo": 2, "fm.index": 0.45, "amp.d": 1.2 },
  "sections": { }
}
```

| Field | Meaning |
|---|---|
| `format` | always `"ohpe-preset"` |
| `version` | `2`. Version-1 files (written by `state::savePreset`, without `kind` / `tags` / `engine`, with a `prefix` field) load as part presets. |
| `kind` | `"part"` – one part's sound; `"project"` – the whole instrument. Missing = part. |
| `name`, `author`, `comment` | text (UTF-8) |
| `tags` | list of strings. The **categories** `Bass`, `Keys`, `Pads`, `Leads`, `Plucks`, `Drums`, `FX`, `Arp`, `Textures`, `Vocal` are fixed English keys (append-only; the UI translates them). Any other string is a free tag. A preset may carry several. |
| `engine` | engine ID of a part preset (`fm`, `wavetable`, `analog`, `pluck`, `grain`, `vocal`, `cluster`, `sampler`); for a project the engine of part 1. Used by the engine filter. Derived from `params.engine` when missing. |
| `params` | parameter values as **plain values** (the unit shown on the display: Hz, seconds, dB …). Part presets use IDs **without** the part prefix (`"analog.cutoff"`, not `"p1.analog.cutoff"`), so they load into any part. Project presets use full IDs. |
| `sections` | project presets only: `"sequencer"` (step patterns, chain, parameter locks), `"orbit"` (Orbit steps) and optionally `"samples"` (paths of loaded sample files). |

Rules when loading:

* Parameters of the part (or, for a project, of the instrument) that the preset does **not** list
  return to their defaults – presets may be sparse.
* Unknown IDs are ignored, so presets from newer versions load in older ones and vice versa.
* A part preset keeps the part's MIDI channel (`pN.midich`, a setup value). The FX slot exists on
  part 1 only; its values are ignored when a preset loads into parts 2–4.
* Project presets neither save nor load the remote controls (`lib.*`, `ui.*`, `tape.edit.*`) and
  the sync setup (`sync.*`). The MIDI map and the tape audio are never part of a preset.
* Loading a preset never adds, removes or reorders parameters (tested).

### Factory bank

The factory presets are the `*.ohpepreset` files in
[`resources/presets/factory/`](../../resources/presets/factory). They are embedded into the
plug-in and the app as binary data (CMake target `ohpe_presets`, namespace `OhpePresets`) and are
read-only. To add or replace factory sounds, put files there and re-run CMake – nothing else needs
to change. The test *"library: the factory bank is embedded and every factory preset is valid"*
checks that every factory preset parses, uses only categories as tags and only existing parameter
IDs.

The presets shipped now are **placeholders** for testing the library; the real factory sounds
follow in a later version.
