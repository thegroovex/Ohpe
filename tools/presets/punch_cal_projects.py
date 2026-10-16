#!/usr/bin/env python3
"""Project mix calibration (punch pass) -> tools/presets/punch_projects.json, then make_factory.
  cal_proj.py balance [names]   per-channel LUFS (solo) -> channel volumes by role relative to the drums
  cal_proj.py master [names]    master glue threshold + makeup -> -10 LUFS integrated (3 rounds)
  cal_proj.py report OUT.json   LUFS-I, crest, peak, true peak, active (4 s test condition) of every project
Run under the build lock."""
import json, os, subprocess, sys, copy
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from punch_render import render, stats, lufs, truepeak, db, find, WT
import numpy as np
PRE = WT + "/tools/presets"
PJ = PRE + "/punch_projects.json"
REL = {"Bass": 0.0, "Boom": 0.0, "Drums": 0.0, "Leads": -2.0, "Vocal": -3.0, "Keys": -3.0, "Plucks": -4.5, "Arp": -4.5,
       "Pads": -6.5, "Textures": -8.5, "FX": -9.0}
TARGET_LUFS = -10.0
SECS = 24


def mk():
    subprocess.run(["python3", "make_factory.py"], cwd=PRE, check=True, stdout=subprocess.DEVNULL)


def projects():
    import importlib
    sys.path.insert(0, PRE)
    import factory_projects
    importlib.reload(factory_projects)
    return {p["name"]: p for p in factory_projects.PROJECTS}


def load():
    return json.load(open(PJ)) if os.path.exists(PJ) else {}


def save(d):
    json.dump(d, open(PJ, "w"), indent=1, sort_keys=True)


def balance(names):
    mk()
    projs = projects()
    pj = load()
    for name in names:
        src = projs[name]
        f, d = find(name)
        prm = d["params"]
        chans = [c for c in range(1, 6) if prm.get("mix.ch%d.vol" % c, 0.0) > -59.0]
        roles = dict(src["roles"])
        drums = "drum.kit" in prm and 5 in chans
        if drums:
            roles[5] = "Drums"
        meas = {}
        for c in chans:
            if c not in roles:
                continue
            e = copy.deepcopy(d)
            e["params"]["mix.ch%d.solo" % c] = 1
            e["params"]["mix.master.comp"] = 0
            e["params"]["mix.master.limit"] = 0
            a = render(e, ["--seconds", str(SECS)])
            meas[c] = lufs(a) if np.max(np.abs(a)) > 1e-4 else None
        ref_c = 5 if drums else min(c for c in meas if meas[c] is not None)
        ref = meas[ref_c] - REL[roles[ref_c]]
        vols = {}
        for c, m in meas.items():
            if m is None or c == ref_c:
                continue
            want = ref + REL.get(roles[c], -4.0)
            cur = prm.get("mix.ch%d.vol" % c, 0.0)
            vols[str(c)] = round(max(-30.0, min(6.0, cur + (want - m))), 1)
        if drums:
            vols["5"] = prm.get("mix.ch5.vol", 0.0)
        pj.setdefault(name, {})["vols"] = vols
        pj[name]["master"] = 0.0
        print(name, {c: (roles[c], None if m is None else round(m, 1)) for c, m in meas.items()}, "->", vols, flush=True)
        save(pj)
    mk()


def master(names, rounds=4):
    for k in range(rounds):
        mk()
        pj = load()
        for name in names:
            f, d = find(name)
            cal = pj.setdefault(name, {})
            if k == 0:  # threshold: 5 dB above the mix's active level (peaks of the drums get ~3 dB of glue)
                e = copy.deepcopy(d)
                e["params"]["mix.master.comp.ratio"] = 1.0
                e["params"]["mix.master.comp.makeup"] = 0.0
                e["params"]["mix.master.limit"] = 0
                a = render(e, ["--seconds", str(SECS)])
                cal["thresh"] = round(max(-40.0, min(0.0, stats(a)["rms"] + 5.0)), 1)
                cal["makeup"] = 0.0
                save(pj)
                continue
            a = render(d, ["--seconds", str(SECS), "--limit"])
            L = lufs(a)
            cal["makeup"] = round(max(0.0, min(24.0, cal.get("makeup", 0.0) + (TARGET_LUFS - L))), 1)
            print("round %d %-20s LUFS %.1f -> makeup %.1f" % (k, name, L, cal["makeup"]), flush=True)
        save(pj)
    mk()


def report(out):
    mk()
    res = {}
    for name in projects():
        f, d = find(name)
        a = render(d, ["--seconds", "32", "--limit"])
        s = stats(a)
        t = render(d, ["--seconds", "4", "--limit"])
        st = stats(t)
        res[name] = dict(lufs=lufs(a), peak=round(s["peak"], 2), tp=round(truepeak(a[: 48000 * 12]), 2), rms=round(s["rms"], 1),
                         crest=round(s["crest"], 1), corr=round(float(s["corr"]), 2), test_active=round(st["rms"], 1))
        print("%-20s %s" % (name, res[name]), flush=True)
    json.dump(res, open(out, "w"), indent=1)


if __name__ == "__main__":
    c = sys.argv[1]
    if c == "report":
        report(sys.argv[2])
    else:
        names = sys.argv[2].split(",") if len(sys.argv) > 2 and sys.argv[2] else list(projects())
        {"balance": balance, "master": master}[c](names)
