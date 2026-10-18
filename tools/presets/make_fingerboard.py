#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p50: the Fingerboard presets - sounds tuned for continuous-surface MPE controllers (Fingerboard mode, MPE HD 14/21-bit).
Each one starts from a factory sound of the same family (its level calibration and mod-wheel routing stay) and gets
a fingerboard expression setup: polyphonic voices, pressure (Z) and slide (Y) on musical targets, the classic
smoothing as a fallback (the controller's own NRPN cutoffs replace it), little velocity (the controller always sends
127; in Fingerboard mode the level follows the pressure anyway).

The presets do not set "mpe", "mpe.bend" or "mpe.onset": that is the part's controller setup (the Fingerboard mode
button on the MIDI settings page), and a part in Fingerboard mode keeps it when a sound loads.

  python3 tools/presets/make_fingerboard.py           (writes resources/presets/factory/*-fingerboard-*.ohpepreset)
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "resources", "presets", "factory")
GAIN_FILE = os.path.join(HERE, "fingerboard_levels.json")  # calibrated part gains (dB), written by hand after rendering

# MpeInput::Target
TIMBRE, AMP, LFO, PAN, CUTOFF, POSITION, FMINDEX, DENSITY, DRIVE, VIBRATO, FORMANT, BLEND = range(1, 13)

COMMON = {
    "mpe.slide": 0.0,     # pitch follows X at once (the controller's X cutoff smooths it)
    "mpe.smooth": 0.015,  # classic fallback until the controller's cutoffs arrive
    "mpe.hd": 1,          # Auto
    "mpe.round": 0,
    "glide": 0.0,         # no portamento: the slide is the finger's
    "velsens": 0.15,
}

PRESETS = [
    {
        "file": "leads-fingerboard-bowed-string.ohpepreset",
        "base": "leads-bowed-body-lead.ohpepreset",
        "name": "Fingerboard Bowed String",
        "tags": ["Leads", "MPE", "Fingerboard"],
        "comment": "A bowed string for continuous fingerboards: pressure is the bow (level and brightness), sliding forward "
                   "and back moves the bowing position, the slide is free. Vibrato comes from your finger. The mod wheel "
                   "slides the bowing position too.",
        "params": {"voices": 6, "mpe.press": CUTOFF, "mpe.pressamt": 0.45, "mpe.timbre": TIMBRE, "mpe.timbreamt": 0.55,
                   "mpe.pcurve": 0.25, "lfo.depth": 0.06},
    },
    {
        "file": "leads-fingerboard-breath-lead.ohpepreset",
        "base": "leads-bowed-tube.ohpepreset",
        "name": "Fingerboard Breath Lead",
        "tags": ["Leads", "MPE", "Fingerboard"],
        "comment": "A breathy reed lead for continuous fingerboards: pressure is the breath (level, air and a brighter "
                   "bore), forward / back opens the tone. Glide-free slides, quick response.",
        "params": {"voices": 4, "glide": 0.0, "mpe.press": DRIVE, "mpe.pressamt": 0.35, "mpe.timbre": CUTOFF, "mpe.timbreamt": 0.5,
                   "mpe.pcurve": 0.35},
    },
    {
        "file": "bass-fingerboard-cello.ohpepreset",
        "base": "leads-bowed-body-lead.ohpepreset",
        "name": "Fingerboard Cello",
        "tags": ["Bass", "MPE", "Fingerboard"],
        "comment": "A dark, woody cello for continuous fingerboards in the low register: pressure bows harder (level, "
                   "rosin and body), forward / back darkens or opens the tone. Slide into notes and lean in for vibrato.",
        "params": {"voices": 4, "analog.cutoff": 2200, "analog.noise": 0.16, "reso.material": 0.35, "reso.size": 0.7,
                   "mpe.press": CUTOFF, "mpe.pressamt": 0.4, "mpe.timbre": TIMBRE, "mpe.timbreamt": 0.45, "mpe.pcurve": 0.2},
    },
    {
        "file": "bass-fingerboard-expressive-bass.ohpepreset",
        "base": "bass-analog-reese.ohpepreset",
        "name": "Fingerboard Expressive Bass",
        "tags": ["Bass", "MPE", "Fingerboard"],
        "comment": "A warm analog bass for continuous fingerboards: pressure opens the filter as it raises the level, "
                   "forward / back adds drive. Slides glide exactly with your finger over the whole range.",
        "params": {"voices": 4, "mpe.press": CUTOFF, "mpe.pressamt": 0.5, "mpe.timbre": DRIVE, "mpe.timbreamt": 0.5,
                   "mpe.pcurve": 0.0, "mpe.bend": 48},
    },
    {
        "file": "pads-fingerboard-opening-pad.ohpepreset",
        "base": "pads-bright-wide-pad.ohpepreset",
        "name": "Fingerboard Opening Pad",
        "tags": ["Pads", "MPE", "Fingerboard"],
        "comment": "A wide wavetable pad for continuous fingerboards that opens as you slide your finger forward (Y opens "
                   "the filter and the table position); pressure swells it. Hold chords and move single fingers.",
        "params": {"voices": 10, "mpe.press": POSITION, "mpe.pressamt": 0.3, "mpe.timbre": CUTOFF, "mpe.timbreamt": 0.8,
                   "mpe.pcurve": -0.2, "amp.a": 0.25},
    },
]


def main():
    gains = json.load(open(GAIN_FILE)) if os.path.exists(GAIN_FILE) else {}
    for p in PRESETS:
        base = json.load(open(os.path.join(OUT, p["base"])))
        params = dict(base["params"])
        params.pop("mpe.bend", None)  # the part's controller setup
        params.update(COMMON)
        params.update(p["params"])
        params.pop("mpe.bend", None)
        if p["file"] in gains:
            params["gain"] = gains[p["file"]]
        out = {
            "format": "ohpe-preset",
            "version": 2,
            "kind": "part",
            "name": p["name"],
            "author": "groovex",
            "tags": p["tags"],
            "engine": base["engine"],
            "comment": p["comment"],
            "params": params,
        }
        with open(os.path.join(OUT, p["file"]), "w") as f:
            json.dump(out, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print("wrote", p["file"])


if __name__ == "__main__":
    main()
