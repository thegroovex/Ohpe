# Content pack format (.ohpepack, format 1)

Ohpe – designed by groovex. Deutsch: [README.de.md](README.de.md) · User guide: [Content packs](../../docs/en/CONTENT-PACKS.md)

An `.ohpepack` is a zip file. Ohpe reads it with `update::planPack` (src/core/update/PackFormat.h) and unpacks it to
`<user data>/groovex/Ohpe/Content/<id>/`. `tools/pack/make_ohpepack.py` writes and checks this format.

## Layout

```
pack.json                         description + checksums (required)
presets/<…>.ohpepreset            kind "presets": the presets (sub folders allowed)
wavetables/<…>.wav                kind "wavetables": single-cycle sets, frames of 2048 samples (sub folders allowed)
readme.txt, licence.md …          optional notes at the root (*.txt, *.md, at most 256 KB each)
```

One fixed folder per kind; a pack has exactly one kind. Files of another kind or type (pictures, other folders) are
not unpacked. Paths: relative, `/` as separator, no `..`, no `.` parts, no empty parts, no drive letters, no `:` `*`
`?` `"` `<` `>` `|`, at most 512 bytes; zip entries that are links refuse the pack.

## pack.json

```json
{
  "format": 1,
  "id": "atmo",
  "version": "1.0.0",
  "kind": "presets",
  "name": { "en": "Atmo", "de": "Atmo" },
  "description": { "en": "Cinematic, dark and wide.", "de": "Filmisch, dunkel und weit." },
  "minApp": "0.5.0",
  "files": {
    "presets/pads-night-tide.ohpepreset": "<sha256, 64 lower-case hex digits>",
    "readme.txt": "<sha256>"
  },
  "license": { "type": "paid", "productId": "ohpe-atmo" },
  "signature": { "alg": "ed25519", "keyId": "groovex-1", "value": "<base64>" }
}
```

| Key | Required | Rule |
|---|---|---|
| `format` | no (1) | packs with a higher format are refused |
| `id` | yes | 1–64 characters `a-z`, `0-9`, `-`, not starting with `-`; the folder name in `Content/` |
| `version` | yes | `MAJOR.MINOR.PATCH` with an optional `-pre.release` suffix |
| `kind` | no (`presets`) | `presets` or `wavetables`; other kinds: "This kind of content pack needs a newer Ohpe" |
| `name` | no (id) | `{ "en", "de" }` (or a string); DE falls back to EN |
| `description` | no | `{ "en", "de" }` (or a string) |
| `minApp` | no | oldest Ohpe that can use the pack; newer than the running Ohpe = refused |
| `files` | yes (.ohpepack) | every other file of the zip → its SHA-256; unlisted, missing or different files refuse the pack |
| `license` | no (`free`) | `{ "type": "free" \| "paid", "productId" }` – shown, **not enforced** |
| `signature` | no | `{ "alg": "ed25519", "keyId", "value" }` – shown, **not verified** yet |

The signature covers the **canonical pack.json**: the object without `signature`, keys sorted at every level, no
white space, UTF-8 as is (`json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False)`;
`make_ohpepack.py canonical` prints it). Because `files` holds every file's checksum, a valid signature covers the
whole pack.

## Limits

| Kind | Whole pack | One file | Files |
|---|---|---|---|
| `presets` | 64 MB | 2 MB | 5000 |
| `wavetables` | 512 MB | 16 MB | 2000 |

All installed packs together: 2 GB (backups not counted).

## References from presets

`"<pack id>/<item>"`, the item being the path inside the kind folder without its extension:
`atmo/glass-choir` = `Content/atmo/wavetables/glass-choir.wav`, `atmo/vowels/aah` =
`Content/atmo/wavetables/vowels/aah.wav`. User wavetables of a pack are stored in presets / projects as
`{"name": "...", "content": "atmo/glass-choir", "frames": 64}` – never embedded. A missing pack is reported by id.

## Older zips

Zips without `pack.json` whose `*.ohpepreset` files sit at the root (the first content zips,
`Ohpe-content-<id>-<version>.zip`) still install from the updater and from "Install pack …" as `.zip`: id and version
come from the release manifest or the file name, the presets move into `presets/`. Library entries keep their id
(`content:<id>/<file>`), so favourites and ratings stay.
