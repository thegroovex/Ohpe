# Ohpe app icon / App-Symbol

*Own artwork, designed by groovex – AGPL-3.0-or-later like the rest of the repository.*

| File | Use |
|---|---|
| `ohpe-icon.svg` | vector master, macOS layout (tile with margin, soft shadow) – `.icns` from 64 px |
| `ohpe-icon-square.svg` | square tile – Windows `.ico`, Linux, docs, from 48 px |
| `ohpe-icon-small.svg` | 24 and 32 px, redrawn for the pixel grid |
| `ohpe-icon-16.svg` | 16 px |
| `Ohpe.icns` | macOS, 16 – 1024 incl. @2x (copied over JUCE's generated `AppIcon.icns`) |
| `Ohpe.ico` | Windows, 16, 24, 32, 48, 64, 128, 256 (copied over JUCE's generated `icon.ico`) |
| `png/` | PNGs 16 – 1024 (square) and `ohpe-icon-mac-1024.png`; `ohpe-icon-128.png` is embedded (About page, Linux window icon) |

Rebuild after editing an SVG / Nach dem Ändern einer SVG neu erzeugen:

```sh
pip install cairosvg   # or have rsvg-convert (librsvg) on the PATH; ImageMagick packs the .ico
python3 tools/app-icon/make_icons.py
```

Details (EN): [docs/DESIGN.md](../../docs/DESIGN.md#app-icon-p43) · (DE): [docs/de/DESIGN.md](../../docs/de/DESIGN.md#app-symbol-p43)
