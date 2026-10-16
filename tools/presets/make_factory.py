#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Writes the factory presets (0.4, package F) into resources/presets/factory:
part presets from factory_parts.py (part gain in dB from levels.json, see level_match.py) and project
presets from factory_projects.py. Run it after editing either file, then re-run CMake.
Presets listed in peak_guard.json (a held 10 s chord / note went above -1.5 dBFS: risers, grain clouds, stacked
chords, resonant combs) get a fast peak compressor in the first free rack slot after their chain, so a held chord
stays below -0.5 dBFS without the master limiter (threshold -4 dB, -7 dB for the risers / clouds that
build far above it). The 3D arps tame their attacks with their own fast-release compressor instead: the binaural
stage after the rack raises the near-ear transients again, which a slow guard only turns into a constant gain cut.
Its makeup stays 0 dB (the level calibration never trims it). An entry {"thresh", "release"} is a transient guard
(punch pass, punch_cal.py crest): released fast (40 ms), it shaves only the loudest attacks of struck keys / mallets
so their body can sit inside the level window.

  python3 tools/presets/make_factory.py
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "resources", "presets", "factory")
sys.path.insert(0, HERE)
import factory_parts  # noqa: E402
import factory_parts_life  # noqa: E402

try:
    import factory_projects  # noqa: E402
    PROJECTS = factory_projects.PROJECTS
except ImportError:
    PROJECTS = []


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def doc(p, kind):
    return {"format": "ohpe-preset", "version": 2, "kind": kind, "name": p["name"], "author": "groovex",
            "tags": p["tags"], "engine": p["engine"], "comment": p["comment"], "params": p["params"],
            **({"arp": p["arp"]} if kind == "part" and p.get("arp") else {}),
            **({"sections": p["sections"]} if p.get("sections") else {})}


GUARD = {"thresh": -7.0, "ratio": 8.0, "attack": 0.1, "release": 180.0, "knee": 6.0, "makeup": 0.0, "mix": 1.0}
MAX_SLOTS = 6


def add_peak_guard(params, guard):
    """Appends the peak compressor (rack type 10) after the last used slot; False when the rack is full."""
    k = 0
    while k < MAX_SLOTS and params.get(("fx." if k == 0 else "fx%d." % (k + 1)) + "type", 0) != 0:
        k += 1
    if k >= MAX_SLOTS:
        return False
    pre = "fx." if k == 0 else "fx%d." % (k + 1)
    params[pre + "type"] = 10
    g = dict(GUARD, **guard) if isinstance(guard, dict) else dict(GUARD, thresh=guard)
    params.update({pre + "compressor." + key: v for key, v in g.items()})
    return True


def main():
    levels_path = os.path.join(HERE, "levels.json")
    levels = json.load(open(levels_path)) if os.path.exists(levels_path) else {}
    guard_path = os.path.join(HERE, "peak_guard.json")
    guarded = json.load(open(guard_path)) if os.path.exists(guard_path) else {}  # name -> threshold (dB)
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".ohpepreset"):
            os.remove(os.path.join(OUT, f))
    names = set()
    for p in factory_parts.PRESETS:
        assert p["name"] not in names, p["name"]
        names.add(p["name"])
        params = dict(p["params"])
        if "gain" not in params:
            params["gain"] = round(levels.get(p["name"], 0.0), 1)
        factory_parts_life.apply(p["name"], params)  # p15: LIFE on a few pads / keys
        if p["name"] in guarded:
            assert add_peak_guard(params, guarded[p["name"]]), p["name"] + ": no free rack slot for the peak guard"
        p = dict(p, params=params)
        fn = "%s-%s.ohpepreset" % (p["tags"][0].lower(), slug(p["name"]))
        with open(os.path.join(OUT, fn), "w", encoding="utf-8") as f:
            json.dump(doc(p, "part"), f, indent=2, ensure_ascii=False)
            f.write("\n")
    import factory_parts_space  # 1.0: 3D motion added to two projects
    for p in PROJECTS:
        p = factory_parts_space.apply_project(p)
        assert p["name"] not in names, p["name"]
        names.add(p["name"])
        fn = "project-%s.ohpepreset" % slug(p["name"])
        with open(os.path.join(OUT, fn), "w", encoding="utf-8") as f:
            json.dump(doc(p, "project"), f, indent=2, ensure_ascii=False)
            f.write("\n")
    print("%d part presets, %d project presets -> %s" % (len(factory_parts.PRESETS), len(PROJECTS), OUT))


if __name__ == "__main__":
    main()
