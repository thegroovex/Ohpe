# Ohpe file formats

*English · [Deutsch](de/FORMATE.md)*

Since 1.0 the formats of everything Ohpe saves are **frozen**: every file and document carries an explicit version,
every older shape loads and is migrated, and nothing about a format changes without it showing up in a review. This
page lists every format, its current version and what changed between versions.

## The compatibility promise

1. **Older files always load.** A build reads every version of a format from the first one up to its own and
   migrates it on load. Saved again, a file is written in the current version.
2. **Unknown newer fields are ignored.** A JSON document from a newer build loads in an older one: fields, sections
   and parameter IDs the older build does not know are skipped (or, where the format says so, kept and written back
   unchanged). Values both builds know keep their meaning.
3. **Host parameter IDs are append-only.** `tests/core/golden/param_ids_v1.txt` lists every ID in its permanent
   order; IDs are never removed, renamed or reordered, and the entries of choice lists and of the modulation matrix's
   source and target lists are only appended. Automation, MIDI maps and presets keep pointing at the same thing.
4. **Binary layouts change only with a version bump.** A reader refuses a binary version whose layout it cannot read
   (with a message, nothing changes) instead of reading it wrongly; `.ohpewt` keeps a fixed leading layout and may only
   grow at the end.

What is *not* promised: a file written by a newer build may use features an older build does not have; those parts
are simply missing when it is opened there.

## Versions

All constants live in `src/core/model/FormatVersions.h`; `tests/formats/FORMATS.lock` holds a copy that CI compares.

| Key | Version | What | Identified by |
| --- | --- | --- | --- |
| `state` | 1 | Plug-in / standalone state, host sessions (the "project" a host saves) | JSON `"format": "ohpe-state"` |
| `preset` | 2 | Part and project presets, `.ohpepreset` | JSON `"format": "ohpe-preset"` |
| `modSchema` | 1 | Meaning of the modulation matrix routings inside a preset | preset field `"modSchema"` |
| `tapeFile` | 1 | Tape recorder file, `.ohpetape` (header) | magic `OHPETPF1` + JSON `"format": "ohpe-tape"` |
| `tapeAudio` | 1 | Tape audio blob (inside `.ohpetape` and after the state JSON) | magic `OHPETAPE` + uint32 version |
| `wavetable` | 1 | User wavetable / formant track, `.ohpewt` | magic `OHPEWT` + uint16 version |
| `pack` | 1 | Content pack, `.ohpepack` (zip) | `pack.json` field `"format"` |
| `midiMap` | 1 | MIDI map, `.ohpemidi`, and the state section `midimap` | JSON `"format": "ohpe-midimap"` |
| `settings` | 1 | User settings, `<user data>/groovex/Ohpe/Settings/settings.json` | field `"version"` (absent = 0) |
| `updateSettings` | 1 | Updater settings, `Settings/update.json` | field `"version"` (absent = 0) |
| `library` | 1 | Favourites and ratings, `library.json` | JSON `"format": "ohpe-library"` |
| `installedPacks` | 1 | Installed content packs, `Content/installed.json` | field `"format"` |
| `sequencer` | 1 | State / project section `sequencer` | section field `"version"` |
| `orbit` | 1 | State / project section `orbit` | section field `"version"` |
| `morph` | 1 | Section `morph` (and a part preset's `morph` block) | section field `"version"` |
| `motion` | 1 | Section `motion` | section field `"version"` |
| `harmony` | 1 | Section `harmony` | section field `"version"` |
| `samples` | 1 | Section `samples` | JSON `"format": "ohpe-samples"` |
| `userTables` | 1 | Section `usertables` (and a part preset's `usertables` block) | JSON `"format": "ohpe-usertables"` |
| `genome` | 1 | Sound genome (breeding) | field `"version"` |
| `tapeMemory` | 1 | Section `tapememory` (p67) | JSON `"format": "ohpe-tapememory"` |

## The formats

### State, session, project (`state` 1)

`{"format": "ohpe-state", "version": 1, "app": "Ohpe", "params": {"<id>": <plain value>, …}, "sections": {…}}`.
Parameters are stored by ID with plain values (Hz, seconds, choice index); sections are owned by their module
(`midimap`, `sequencer`, `orbit`, `tape`, `morph`, `motion`, `harmony`, `samples`, `usertables`, `tapememory`, `ui`), and unknown
sections are kept and written back. In a plug-in or the standalone app the state is
`<state JSON> NUL <tape audio blob>` (the tape part only when the tape holds audio).

Loading resets every parameter to its default first, then applies the document. Migrations on load:

| Since | Older shape | Becomes |
| --- | --- | --- |
| 0.2 | state without the tape part | an empty tape |
| p28 | performance pads held while saving | pads off (a gesture, not part of the song) |
| p41 | sessions before the 3D switches | the switches follow `space.on` / `space.out` |
| p50 | `ui` block with only `"preset"` (the edit part's preset) | that preset names all four parts (`"presets"`) |
| p59 | `harmony.kb.layout` = 3 without `harmony.kb.inkey` (the old *In Key* layout) | layout Fourths + In Key on |

### Presets (`preset` 2, `modSchema` 1)

See [en/PRESETS.md](en/PRESETS.md) for the fields. Version 1 (0.1 / 0.2) had no `kind`, `tags`, `engine`, `author`
or `comment` and named the part it came from in `"prefix"`: it loads as a part preset, its engine is derived from the
`engine` parameter. Version 2 (0.3) added `kind` (`part` / `project`) and the metadata; later additions inside version
2 are optional blocks that older builds ignore: `arp` and `modSchema` (0.7), `morph` (p16), `usertables` (p20).
A preset without `modSchema` (= 0) runs through the matrix migration (`mod::migrate`); project presets get the same
p59 grid-layout migration as states.

### Tape file (`tapeFile` 1, `tapeAudio` 1)

`.ohpetape`: 8 bytes `OHPETPF1`, uint32 header length, the JSON header (`format`, `version`, `sampleRate`,
`lengthSeconds`, `loopIn`, `loopOut`, `loop`, `editTrack`; missing fields take their defaults), then the tape audio
blob: `OHPETAPE`, uint32 version, uint32 tracks (8), uint32 frames per block (4096), float64 sample rate, uint64
length, then `(uint32 track, uint32 page, float32 L[4096], float32 R[4096])` for every block that holds audio, ended
by `0xFFFFFFFF`. Little-endian. A tape recorded at another sample rate is resampled on load. A header or blob version
newer than the build's is refused.

### Wavetable (`wavetable` 1)

`.ohpewt`, little-endian: `OHPEWT`, uint16 version, uint32 frame size (2048), uint32 frames (0 – 256), uint32
formant frames (0 – 64), uint32 name length + UTF-8 name, then the level-0 samples of every frame (float32) and 15
float32 per formant frame (5 frequencies, 5 levels in dB, 5 bandwidths). The leading layout is fixed: a later version
may only append data, which this build does not read.

### Content pack (`pack` 1)

A zip with `pack.json` (`format`, `id`, `version`, `kind`, `name`, `description`, `minApp`, `files` with the SHA-256
of every file, optional `license` / `signature`) and one folder per kind; see
[en/CONTENT-PACKS.md](en/CONTENT-PACKS.md). `pack.json` without `format` reads as format 1; a newer format is refused
("pack.json has a newer format"). The first packs were *flat* zips named `Ohpe-content-<id>-<version>.zip`, presets at
the root and no `pack.json`: they still install, the content moves into the kind folder.

### Settings (`settings` 1, `updateSettings` 1)

`settings.json` and `update.json` are per user, never per project. Files written before 1.0 have no `"version"` (= 0).
For `settings.json` version 0 runs the 0.6.3 controller migration: a controller set up before `controller.chosen`
existed starts with the Grid-Controller profile and automatic ports. p64 added the key `"tour"`
(`{"standalone": bool, "plugin": bool}`, the first-start tour was seen) without a version bump: files without it show
the tour once, older builds keep it as an unknown key. Unknown keys of `settings.json` are kept and
written back; a build saving the file writes its own version.

### Tape memory captures (`tapeMemory` 1, p67)

Section `tapememory`: `{"format": "ohpe-tapememory", "version": 1, "parts": {"p1": {"mode": 0, "root": 62,
"slices": 1, "name": "Capture 1", "rate": 48000, "channels": 2, "frames": 96000, "pcm16": "<base64>"}}}` – the last
capture of each part whose Sampler still plays it. `pcm16` is the audio itself (little-endian int16, full scale 32767,
channels interleaved), so a session needs no external file. `mode` 0 = Chromatic (`root` = the zone's root key),
1 = Slices (first slice key C2). Readers accept a missing `version` (0, before the field: read as 1; `format`,
`frames` and `name` may be missing too) and newer versions (unknown fields are ignored); entries with a bad rate,
channel count or fewer than 64 frames are skipped. Fixtures: `tapememory/tapememory-v1.json`,
`tapememory-v0-noversion.json`, `tapememory-v2-future.json`.

### Everything else (version 1)

`.ohpemidi` / `midimap`, `library.json`, `Content/installed.json` and the state sections carry version 1 since they
were introduced; there is nothing to migrate yet.

## Golden fixtures and the CI check

`tests/formats/fixtures` holds a frozen file of every format in its current shape plus older shapes (version-1 preset,
0.1 state without sections, the 0.6 state with the old *In Key* layout and the single-`preset` ui block, `settings.json`
before `version`, the old flat pack zip, the smallest tape header) and newer shapes (a state and a preset from a
future version, a newer tape and pack format that must be refused). `tests/core/FormatTests.cpp` and
`tests/plugin/FormatPluginTests.cpp` load, migrate and round-trip each of them.

`scripts/ci/check-formats.py` (Linux core tests, no build needed) fails when a version constant, a fixture or the lock
differ, when a fixture is changed or removed, when a version goes down or goes up without a new fixture, or when the
lock changes without both format documents.

### Changing a format

1. Raise the constant in `FormatVersions.h` (never lower it) and write the migration from the previous version.
2. Add a fixture of the **previous** shape (and one of the new) to `tools/formats/make_fixtures.py`, run it and append
   its lines to `FORMATS.lock` (`make_fixtures.py --lock`). Existing fixtures stay as they are.
3. Update the version line in `FORMATS.lock`, this page and [de/FORMATE.md](de/FORMATE.md), and add a test.
