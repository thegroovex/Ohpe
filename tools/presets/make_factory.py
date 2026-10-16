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
build far above it). Its makeup stays 0 dB (the level calibration never trims it).

  python3 tools/presets/make_factory.py
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "resources", "presets", "factory")
sys.path.insert(0, HERE)
import factory_parts  # noqa: E402

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


def add_peak_guard(params, thresh):
    """Appends the peak compressor (rack type 10) after the last used slot; False when the rack is full."""
    k = 0
    while k < MAX_SLOTS and params.get(("fx." if k == 0 else "fx%d." % (k + 1)) + "type", 0) != 0:
        k += 1
    if k >= MAX_SLOTS:
        return False
    pre = "fx." if k == 0 else "fx%d." % (k + 1)
    params[pre + "type"] = 10
    params.update({pre + "compressor." + key: v for key, v in dict(GUARD, thresh=thresh).items()})
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
        if p["name"] in guarded:
            assert add_peak_guard(params, guarded[p["name"]]), p["name"] + ": no free rack slot for the peak guard"
        p = dict(p, params=params)
        fn = "%s-%s.ohpepreset" % (p["tags"][0].lower(), slug(p["name"]))
        with open(os.path.join(OUT, fn), "w", encoding="utf-8") as f:
            json.dump(doc(p, "part"), f, indent=2, ensure_ascii=False)
            f.write("\n")
    for p in PROJECTS:
        assert p["name"] not in names, p["name"]
        names.add(p["name"])
        fn = "project-%s.ohpepreset" % slug(p["name"])
        with open(os.path.join(OUT, fn), "w", encoding="utf-8") as f:
            json.dump(doc(p, "project"), f, indent=2, ensure_ascii=False)
            f.write("\n")
    print("%d part presets, %d project presets -> %s" % (len(factory_parts.PRESETS), len(PROJECTS), OUT))


if __name__ == "__main__":
    main()
