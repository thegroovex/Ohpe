#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p59: the grid keyboard's grip diagrams for docs/en/HARMONY.md and docs/de/HARMONIE.md.

Mirrors harmony::gridNote (src/core/harmony/HarmonyBrain.cpp): layouts Fourths / Thirds / Whole Tones / Sequential,
In Key (scale steps) or Chromatic (semitones), fixed C or the key's root bottom left. Prints ASCII diagrams of the
bottom-left 4 x 8 cells; cells in [ ] are the ones to press.  Usage: grid_diagrams.py [en|de]
"""
import sys

STEPS = {"fourths": (1, 5), "thirds": (1, 4), "whole": (2, 5), "sequential": (1, 8)}
DEGREES = {"fourths": (1, 3), "thirds": (1, 2), "whole": (1, 3), "sequential": (1, 8)}
MAJOR = [0, 2, 4, 5, 7, 9, 11]
MINOR = [0, 2, 3, 5, 7, 8, 10]
EN = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
DE = ["C", "Cis", "D", "Dis", "E", "F", "Fis", "G", "Gis", "A", "Ais", "H"]


def in_scale(n, root, scale):
    return (n - root) % 12 in scale


def degree_above(n, d, root, scale):
    for _ in range(d):
        n += 1
        while not in_scale(n, root, scale):
            n += 1
    return n


def grid_note(layout, col, row, base, root, scale, in_key, fixed):
    if not in_key:
        c, r = STEPS[layout]
        return base + (0 if fixed else root) + col * c + row * r
    n = base + (0 if fixed else root)
    while not in_scale(n, root, scale):
        n += 1
    c, r = DEGREES[layout]
    return degree_above(n, col * c + row * r, root, scale)


def name(n, lang):
    return (DE if lang == "de" else EN)[n % 12] + str(n // 12 - 1)


def diagram(layout, in_key, pressed=(), root=0, scale=MAJOR, fixed=False, lang="en", rows=4, cols=8):
    out = []
    for row in range(rows - 1, -1, -1):
        cells = []
        for col in range(cols):
            t = name(grid_note(layout, col, row, 48, root, scale, in_key, fixed), lang)
            cells.append(("[" + t + "]" if (col, row) in pressed else " " + t + " ").ljust(7))
        out.append(f"{row + 1} |" + "".join(cells).rstrip())
    out.append("  +" + "-" * 56)
    out.append("    " + "".join(str(c + 1).ljust(7) for c in range(cols)).rstrip())
    return "\n".join(out)


def intervals(layout, in_key, shape, root=0, scale=MAJOR):
    base = grid_note(layout, 0, 0, 48, root, scale, in_key, False)
    return [grid_note(layout, c, r, 48, root, scale, in_key, False) - base for c, r in shape]


SHAPES = [  # (title, layout, in key, cells, key scale)
    ("Layout: Fourths, In Key", "fourths", True, (), MAJOR),
    ("Layout: Thirds, In Key", "thirds", True, (), MAJOR),
    ("Layout: Sequential, Chromatic", "sequential", False, (), MAJOR),
    ("In Key triad (major key)", "fourths", True, ((0, 0), (2, 0), (1, 1)), MAJOR),
    ("In Key triad (minor key)", "fourths", True, ((0, 0), (2, 0), (1, 1)), MINOR),
    ("In Key triad moved one cell right (D minor in C major)", "fourths", True, ((1, 0), (3, 0), (2, 1)), MAJOR),
    ("In Key seventh chord", "fourths", True, ((0, 0), (2, 0), (1, 1), (3, 1)), MAJOR),
    ("In Key first inversion", "fourths", True, ((2, 0), (1, 1), (4, 1)), MAJOR),
    ("In Key second inversion", "fourths", True, ((1, 1), (4, 1), (3, 2)), MAJOR),
    ("In Key power chord", "fourths", True, ((0, 0), (1, 1), (1, 2)), MAJOR),
    ("In Key octave", "fourths", True, ((0, 0), (1, 2)), MAJOR),
    ("Chromatic major", "fourths", False, ((0, 0), (4, 0), (2, 1)), MAJOR),
    ("Chromatic minor", "fourths", False, ((0, 0), (3, 0), (2, 1)), MAJOR),
    ("Chromatic dominant 7", "fourths", False, ((0, 0), (4, 0), (2, 1), (0, 2)), MAJOR),
    ("Chromatic major 7", "fourths", False, ((0, 0), (4, 0), (2, 1), (1, 2)), MAJOR),
]

if __name__ == "__main__":
    lang = sys.argv[1] if len(sys.argv) > 1 else "en"
    for title, layout, in_key, cells, scale in SHAPES:
        root = 9 if scale is MINOR else 0  # the minor example in A minor
        print(f"## {title}  intervals {intervals(layout, in_key, cells, root, scale)}\n```")
        print(diagram(layout, in_key, cells, root, scale, lang=lang))
        print("```\n")
