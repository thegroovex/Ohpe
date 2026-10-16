#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Writes the factory morph presets (p16, the morph space) into resources/presets/factory as morph-*.ohpepreset:
part presets whose "morph" block holds four corner sounds (A top left, B top right, C bottom left, D bottom right).
Each corner is a factory part preset (its sparse "params", gain-calibrated), or a variation of one. The part's own
sound is corner C and the pad starts there (x = 0, y = 0): the mod wheel moves it right (C -> D), pressure moves it
up (C -> A), so the presets play from a keyboard without touching the pad.

  python3 tools/presets/make_morph.py
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "resources", "presets", "factory")

CTRL_OFF, CTRL_PRESSURE, CTRL_SLIDE, CTRL_MODWHEEL = 0, 1, 2, 3


def load(name):
    with open(os.path.join(OUT, name + ".ohpepreset"), encoding="utf-8") as f:
        d = json.load(f)
    return d["name"], dict(d["params"])


def variant(base, name, **changes):
    n, p = load(base)
    p = dict(p)
    for k, v in changes.items():
        p[k.replace("__", ".")] = v
    return name, p


PRESETS = [
    {
        "name": "Morph Strings Filter", "file": "morph-strings-filter", "tags": ["Pads", "Morph"],
        "comment": "One string ensemble in four shapes: the pad morphs its filter, detune and attack - a single sound, "
                   "smooth everywhere. Mod wheel = right, pressure = up.",
        "corners": [
            variant("pads-warm-strings", "Strings Open", analog__cutoff=6500.0, analog__reso=0.22, analog__detune=0.22),
            variant("pads-warm-strings", "Strings Edge", analog__cutoff=3600.0, analog__shape=0.5, analog__reso=0.32),
            load("pads-warm-strings"),
            variant("pads-warm-strings", "Strings Slow", analog__cutoff=800.0, analog__detune=0.45, amp__a=1.4, lfo__depth=0.06),
        ],
        "glide": 0.25,
    },
    {
        "name": "Morph Four Worlds", "file": "morph-four-worlds", "tags": ["Pads", "Hybrid", "Morph"],
        "comment": "Four pads on four engines - glass wavetable, breathing choir, analog strings, FM crystal - played in "
                   "parallel and cross-faded at even loudness. Mod wheel = right, pressure = up.",
        "corners": [load("pads-glass-shimmer"), load("pads-breath-choir-pad"), load("pads-warm-strings"), load("pads-crystal-fm-pad")],
        "glide": 0.35,
    },
    {
        "name": "Morph Keys and Bells", "file": "morph-keys-and-bells", "tags": ["Keys", "Morph"],
        "comment": "Electric piano at home, vibes, glass keys and a dreamy tine in the other corners. "
                   "Mod wheel = right, pressure = up.",
        "corners": [load("keys-fm-vibes"), load("keys-glass-keys"), load("keys-digital-ep"), load("keys-dream-tine")],
        "glide": 0.15,
    },
    {
        "name": "Morph Bass Shapes", "file": "morph-bass-shapes", "tags": ["Bass", "Morph"],
        "comment": "A round sub that turns into acid, an FM growl or a wide reese. Mod wheel = right, pressure = up.",
        "corners": [load("bass-growl-fm-bass"), load("bass-classic-reese"), load("bass-round-sub"), load("bass-acid-line")],
        "glide": 0.08,
    },
]

ENGINES = ["fm", "wavetable", "analog", "pluck", "grain", "vocal", "cluster", "sampler"]


def main():
    for spec in PRESETS:
        corners = [{"name": n, "params": p} for n, p in spec["corners"]]
        home = dict(corners[2]["params"])  # corner C is the part's own sound
        home.update({"morph.on": 1, "morph.x": 0.0, "morph.y": 0.0, "morph.glide": spec["glide"],
                     "morph.xsrc": CTRL_MODWHEEL, "morph.ysrc": CTRL_PRESSURE})
        doc = {"format": "ohpe-preset", "version": 2, "kind": "part", "name": spec["name"], "author": "groovex",
               "tags": spec["tags"], "engine": ENGINES[int(home["engine"])], "comment": spec["comment"],
               "params": home, "morph": {"corners": corners}}
        path = os.path.join(OUT, spec["file"] + ".ohpepreset")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print("wrote", path)


if __name__ == "__main__":
    main()
