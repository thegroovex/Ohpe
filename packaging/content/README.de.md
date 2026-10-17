# Format der Inhaltspakete (.ohpepack, Format 1)

Ohpe – designed by groovex. English: [README.md](README.md) · Anleitung: [Inhaltspakete](../../docs/de/INHALTSPAKETE.md)

Eine `.ohpepack`-Datei ist ein Zip. Ohpe liest sie mit `update::planPack` (src/core/update/PackFormat.h) und entpackt
sie nach `<Benutzerdaten>/groovex/Ohpe/Content/<id>/`. `tools/pack/make_ohpepack.py` schreibt und prüft dieses Format.

## Aufbau

```
pack.json                         Beschreibung + Prüfsummen (Pflicht)
presets/<…>.ohpepreset            Art "presets": die Presets (Unterordner erlaubt)
wavetables/<…>.wav                Art "wavetables": Single-Cycle-Sets, Frames zu 2048 Samples (Unterordner erlaubt)
readme.txt, licence.md …          optionale Hinweise im Wurzelordner (*.txt, *.md, je höchstens 256 KB)
```

Ein fester Ordner je Art; ein Paket hat genau eine Art. Dateien einer anderen Art oder eines anderen Typs (Bilder,
andere Ordner) werden nicht entpackt. Pfade: relativ, `/` als Trenner, kein `..`, keine `.`-Teile, keine leeren Teile,
keine Laufwerksbuchstaben, kein `:` `*` `?` `"` `<` `>` `|`, höchstens 512 Bytes; Zip-Einträge, die Links sind,
lassen das Paket ablehnen.

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
    "presets/pads-night-tide.ohpepreset": "<sha256, 64 Hex-Ziffern klein>",
    "readme.txt": "<sha256>"
  },
  "license": { "type": "paid", "productId": "ohpe-atmo" },
  "signature": { "alg": "ed25519", "keyId": "groovex-1", "value": "<base64>" }
}
```

| Schlüssel | Pflicht | Regel |
|---|---|---|
| `format` | nein (1) | Pakete mit höherem Format werden abgelehnt |
| `id` | ja | 1–64 Zeichen `a-z`, `0-9`, `-`, nicht mit `-` beginnend; der Ordnername in `Content/` |
| `version` | ja | `MAJOR.MINOR.PATCH` mit optionalem Zusatz `-pre.release` |
| `kind` | nein (`presets`) | `presets` oder `wavetables`; andere Arten: „Diese Art Inhaltspaket braucht ein neueres Ohpe“ |
| `name` | nein (id) | `{ "en", "de" }` (oder ein Text); DE fällt auf EN zurück |
| `description` | nein | `{ "en", "de" }` (oder ein Text) |
| `minApp` | nein | ältestes Ohpe, das das Paket nutzen kann; neuer als das laufende Ohpe = abgelehnt |
| `files` | ja (.ohpepack) | jede andere Datei des Zips → ihre SHA-256; nicht gelistete, fehlende oder abweichende Dateien lassen das Paket ablehnen |
| `license` | nein (`free`) | `{ "type": "free" \| "paid", "productId" }` – angezeigt, **nicht erzwungen** |
| `signature` | nein | `{ "alg": "ed25519", "keyId", "value" }` – angezeigt, noch **nicht geprüft** |

Die Signatur deckt das **kanonische pack.json** ab: das Objekt ohne `signature`, Schlüssel auf jeder Ebene sortiert,
ohne Leerraum, UTF-8 unverändert (`json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False)`;
`make_ohpepack.py canonical` gibt es aus). Da `files` die Prüfsumme jeder Datei enthält, deckt eine gültige Signatur
das ganze Paket ab.

## Grenzen

| Art | Ganzes Paket | Eine Datei | Dateien |
|---|---|---|---|
| `presets` | 64 MB | 2 MB | 5000 |
| `wavetables` | 512 MB | 16 MB | 2000 |

Alle installierten Pakete zusammen: 2 GB (Sicherungen nicht mitgezählt).

## Referenzen aus Presets

`"<Paket-ID>/<Element>"`, das Element ist der Pfad im Ordner der Art ohne Endung: `atmo/glass-choir` =
`Content/atmo/wavetables/glass-choir.wav`, `atmo/vowels/aah` = `Content/atmo/wavetables/vowels/aah.wav`. Wavetables
eines Pakets stehen in Presets / Projekten als `{"name": "...", "content": "atmo/glass-choir", "frames": 64}` – nie
eingebettet. Ein fehlendes Paket wird mit seiner ID gemeldet.

## Ältere Zips

Zips ohne `pack.json`, deren `*.ohpepreset`-Dateien im Wurzelordner liegen (die ersten Inhalts-Zips,
`Ohpe-content-<id>-<version>.zip`), lassen sich weiter über die Aktualisierung und als `.zip` über „Paket installieren
…“ installieren: ID und Version kommen aus dem Release-Manifest oder dem Dateinamen, die Presets wandern nach
`presets/`. Bibliothekseinträge behalten ihre ID (`content:<id>/<Datei>`), Favoriten und Bewertungen bleiben.
