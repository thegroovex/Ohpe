#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p88: after-silence peak trim of the factory part presets.

  ohpe_render --dyn --bank resources/samples/bank.json --wavdir DIR resources/presets/factory/*.ohpepreset > dyn.jsonl
  python3 tools/presets/dyn_trim.py dyn.jsonl [--target -1.3] [--dry] [--skip FILE,...]
  python3 tools/presets/dyn_trim.py --add-mbcomp       # puts the Multiband Comp on the presets listed in MBCOMP below

Reads the after-silence audit (render::dynamicsAudit: a note, a 4-note and an 8-note chord at velocity 127 after silence,
master limiter off) and lowers every preset whose loudest material passes -1 dBTP to `target` (default -1.3 dBTP, a little
headroom under the -1 dBTP test). The trim goes where it is linear: the make-up of the part's last Compressor slot when
nothing that distorts follows it, otherwise the part level ("gain", in front of the rack - a compressor there takes part of
the trim, so a preset may need a second round). Every change is recorded in tools/presets/bank_curation.json ("changed" ->
"params") and written to the preset, so `bank_curate.py apply` keeps it.
"""
import argparse, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BANK = os.path.join(ROOT, "resources", "presets", "factory")
CURATION = os.path.join(HERE, "bank_curation.json")

COMPRESSOR = 10
# types that do not scale linearly with their input (a trim in front of them changes the sound, not only the level)
NONLINEAR = {3, 4, 7, 8, 10, 11, 24}
SLOTS = ["fx."] + ["fx%d." % i for i in range(2, 7)]


MBCOMP_TYPE = 24
# p88: the Multiband Comp where it adds punch / density (drums, keys, a bass): preset file -> (slot, settings). Its OUT
# then carries the peak trim (after it nothing distorts).
DRUMS = {"depth": 0.35, "time": 1.0, "upward": 0.5, "downward": 1.0, "lowattack": 8.0, "midattack": 5.0, "highattack": 2.0,
         "lowrelease": 80.0, "midrelease": 60.0, "highrelease": 50.0}
KEYS = {"depth": 0.3, "time": 1.5, "upward": 0.6, "downward": 0.8}
BASS = {"depth": 0.3, "time": 1.0, "upward": 0.4, "downward": 1.0, "lowattack": 6.0, "lowrelease": 120.0}
MBCOMP = {
    "drums-wood-block.ohpepreset": ("fx2.", DRUMS),
    "drums-tuned-tom.ohpepreset": ("fx2.", DRUMS),
    "drums-fm-tom.ohpepreset": ("fx2.", DRUMS),
    "drums-round-toms.ohpepreset": ("fx2.", DRUMS),
    "drums-hand-drum-ring.ohpepreset": ("fx2.", DRUMS),
    "keys-alive-tine-keys.ohpepreset": ("fx3.", KEYS),
    "keys-velocity-glass-ep.ohpepreset": ("fx3.", KEYS),
    "bass-deep-pluck-sub.ohpepreset": (None, BASS),
}


def trim_target(params, delta):
    """(key, new value) for a trim of `delta` dB (> 0 = quieter)."""
    types = [int(round(params.get(s + "type", 0))) for s in SLOTS]
    last = max((i for i, t in enumerate(types) if t in NONLINEAR), default=-1)
    if last >= 0 and types[last] == MBCOMP_TYPE:
        key = SLOTS[last] + "mbcomp.output"
        return key, round(max(-24.0, float(params.get(key, 0.0)) - delta), 1)
    if last >= 0 and types[last] == COMPRESSOR:
        key = SLOTS[last] + "compressor.makeup"
        mk = float(params.get(key, 0.0))
        if mk >= delta:
            return key, round(mk - delta, 1)
    g = float(params.get("gain", 0.0))
    return "gain", round(max(-24.0, g - delta), 1)


def add_mbcomp(cur):
    changed = cur.setdefault("changed", {})
    for f, (slot, values) in MBCOMP.items():
        path = os.path.join(BANK, f)
        p = json.load(open(path, encoding="utf-8"))
        prm = p["params"]
        if slot is None: # the first free slot
            slot = next(s for s in SLOTS if int(round(prm.get(s + "type", 0))) == 0)
        assert int(round(prm.get(slot + "type", 0))) in (0, MBCOMP_TYPE), (f, slot)
        rec = {slot + "type": MBCOMP_TYPE}
        rec.update({slot + "mbcomp." + k: v for k, v in values.items()})
        prm.update(rec)
        changed.setdefault(f, {}).setdefault("params", {}).update(rec)
        print("%-40s Multiband Comp in %s" % (p["name"], slot))
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(p, fh, indent=2, ensure_ascii=False)
            fh.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", nargs="?")
    ap.add_argument("--add-mbcomp", action="store_true")
    ap.add_argument("--skip", default="", help="comma-separated preset files (no extension) to leave alone")
    ap.add_argument("--target", type=float, default=-1.3)
    ap.add_argument("--limit", type=float, default=-1.0, help="trim only presets above this (dBTP)")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    cur = json.load(open(CURATION, encoding="utf-8"))
    if a.add_mbcomp:
        add_mbcomp(cur)
        json.dump(cur, open(CURATION, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
        return 0
    changed = cur.setdefault("changed", {})
    skip = set(x for x in a.skip.split(",") if x)
    n = 0
    for line in open(a.rows, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        r = json.loads(line)
        worst = max(r["tp_note"], r["tp_chord4"], r["tp_chord8"])
        if worst <= a.limit or r["file"] in skip:
            continue
        f = r["file"] + ".ohpepreset"
        path = os.path.join(BANK, f)
        p = json.load(open(path, encoding="utf-8"))
        delta = round(worst - a.target, 1)
        key, val = trim_target(p["params"], delta)
        print("%-40s worst %+6.2f dBTP  %s %s -> %s" % (r["name"], worst, key, p["params"].get(key, 0.0), val))
        n += 1
        if a.dry:
            continue
        p["params"][key] = val
        changed.setdefault(f, {}).setdefault("params", {})[key] = val
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(p, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
    if not a.dry:
        json.dump(cur, open(CURATION, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print("%d presets trimmed" % n)


if __name__ == "__main__":
    sys.exit(main())
