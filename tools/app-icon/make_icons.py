#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Ohpe app icon: render the vector masters in resources/icon/ into the platform files.

    python3 tools/app-icon/make_icons.py            # rebuild everything in resources/icon/
    python3 tools/app-icon/make_icons.py --check    # exit 1 if the committed files are stale

Masters (hand-authored SVG, own artwork):
    ohpe-icon.svg         macOS layout (tile with the platform margin, soft shadow) - .icns from 64 px up
    ohpe-icon-square.svg  square tile for Windows / Linux / docs - .ico and PNGs from 48 px up
    ohpe-icon-small.svg   24 and 32 px (thicker lines, wider gap, flat halo)
    ohpe-icon-16.svg      16 px

Outputs (committed, so a build needs no SVG renderer):
    Ohpe.icns             16, 32, 64, 128, 256, 512, 1024 incl. the @2x entries
    Ohpe.ico              16, 24, 32, 48, 64, 128, 256
    png/ohpe-icon-<n>.png 16 ... 1024 (square) for Linux and the docs, ohpe-icon-mac-1024.png

Open-source tools only: rsvg-convert (librsvg) when present, otherwise the cairosvg Python module;
ImageMagick (`magick`) packs the .ico; the .icns container is written here (it is just PNG chunks).
"""
import argparse, io, os, shutil, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ICON = os.path.join(ROOT, "resources", "icon")


def master_for(size, layout):
    if size <= 16:
        return "ohpe-icon-16.svg"
    if size <= 32:
        return "ohpe-icon-small.svg"
    return "ohpe-icon.svg" if layout == "mac" else "ohpe-icon-square.svg"


def render(svg, size):
    path = os.path.join(ICON, svg)
    if shutil.which("rsvg-convert"):
        return subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), path],
                              check=True, capture_output=True).stdout
    import cairosvg  # pip install cairosvg
    return cairosvg.svg2png(url=path, output_width=size, output_height=size)


def icns(entries):
    """entries: list of (OSType, png bytes)."""
    body = b"".join(t.encode("ascii") + struct.pack(">I", 8 + len(p)) + p for t, p in entries)
    return b"icns" + struct.pack(">I", 8 + len(body)) + body


# OSType -> pixel size (PNG-based icon types; ic11..ic14 are the @2x entries)
ICNS_TYPES = [("icp4", 16), ("ic11", 32), ("icp5", 32), ("ic12", 64), ("icp6", 64),
              ("ic07", 128), ("ic13", 256), ("ic08", 256), ("ic14", 512), ("ic09", 512), ("ic10", 1024)]
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
PNG_SIZES = [16, 24, 32, 48, 64, 128, 256, 512, 1024]


def build(out):
    os.makedirs(os.path.join(out, "png"), exist_ok=True)
    files = {}
    cache = {}

    def png(size, layout):
        key = (master_for(size, layout), size)
        if key not in cache:
            cache[key] = render(*key)
        return cache[key]

    files["Ohpe.icns"] = icns([(t, png(s, "mac")) for t, s in ICNS_TYPES])
    for s in PNG_SIZES:
        files[f"png/ohpe-icon-{s}.png"] = png(s, "square")
    files["png/ohpe-icon-mac-1024.png"] = png(1024, "mac")
    with tempfile.TemporaryDirectory() as tmp:
        srcs = []
        for s in ICO_SIZES:
            p = os.path.join(tmp, f"{s}.png")
            with open(p, "wb") as f:
                f.write(png(s, "square"))
            srcs.append(p)
        ico = os.path.join(tmp, "Ohpe.ico")
        magick = shutil.which("magick") or shutil.which("convert")
        subprocess.run([magick, *srcs, ico], check=True)
        with open(ico, "rb") as f:
            files["Ohpe.ico"] = f.read()
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    files = build(ICON)
    stale = []
    for name, data in files.items():
        path = os.path.join(ICON, name)
        if a.check:
            if not os.path.exists(path):
                stale.append(name)
            continue
        with open(path, "wb") as f:
            f.write(data)
        print(f"{name:32s} {len(data):8d} bytes")
    if a.check and stale:
        print("missing:", ", ".join(stale))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
