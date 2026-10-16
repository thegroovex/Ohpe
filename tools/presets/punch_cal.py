#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Punch-pass calibration of the glue compressors (factory_parts_r3.GLUE) -> punch.json, levels.json.
  punch_cal.py thresh            peak reaching each glue (glue ratio 1, makeup 0) -> threshold = peak - GLUE "below"
  punch_cal.py makeup N          N rounds: make_factory, render (plug-in test conditions), makeup += error (a level
                                 below makeup 0 moves the part gain and the threshold together)
  punch_cal.py levels N          N rounds: the part gain of the presets WITHOUT a glue compressor outside the level window
  punch_cal.py crest N NAMES     high-crest presets (struck keys, mallets, arps) the peaks hold under the level window: a
                                 transient guard (peak_guard.json) shaves the loudest attacks, the part gain lifts the body
                                 (the glue threshold and the guard move with the gain); N rounds
  punch_cal.py report OUT.json   every part preset in test conditions: active RMS, peak, crest; lists those outside
Needs a build-core build (ohpe_render). Never run while make_factory or another render runs."""
import glob, json, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", ".."))
FAC = WT + "/resources/presets/factory"
sys.path.insert(0, HERE)
RENDER = [WT + "/build-core/tests/core/ohpe_render", "--bank", WT + "/resources/samples/bank.json", "--wavdir",
          os.environ.get("OHPE_WAVDIR", "/workspace/work/ohpe-r3-wavall"), "--beats", "4", "--tail", "1", "--seconds", "4", "--limit"]
TARGET = {"Bass": -14.5, "Keys": -16.0, "Pads": -16.5, "Leads": -15.0, "Plucks": -16.0, "Drums": -15.0, "FX": -16.5,
          "Arp": -16.0, "Textures": -17.0, "Vocal": -16.0, "Boom": -14.5}
PJ = HERE + "/punch.json"
LJ = HERE + "/levels.json"


def r3mod():
    import factory_parts  # noqa: F401 (imports factory_parts_r3; importing r3 first is circular)
    import factory_parts_r3
    return factory_parts_r3


def glue_slot(params, cat, r3):
    if cat not in r3.GLUE:
        return None
    ratio, attack, release = r3.GLUE[cat][:3]
    for k in range(6):
        pre = r3.slot_prefix(k)
        if params.get(pre + "type") == 10 and params.get(pre + "compressor.ratio") == ratio and \
                params.get(pre + "compressor.attack") == attack and params.get(pre + "compressor.release") == release:
            return pre
    return None


def factory_parts_files():
    out = {}
    for f in glob.glob(FAC + "/*.ohpepreset"):
        d = json.load(open(f))
        if d["kind"] == "part":
            out[d["name"]] = (f, d)
    return out


def render(files):
    tmp = tempfile.mkdtemp(prefix="pcal-")
    res = subprocess.run(RENDER + ["--out", tmp] + files, capture_output=True, text=True)
    rows = {}
    for line in res.stdout.splitlines():
        if line.startswith("{"):
            r = json.loads(line)
            rows[r["name"]] = r
    subprocess.run(["rm", "-rf", tmp])
    if not rows:
        print(res.stderr[-1500:])
    return rows


def make_factory():
    subprocess.run([sys.executable, "make_factory.py"], cwd=HERE, check=True, stdout=subprocess.DEVNULL)


def glued():
    r3 = r3mod()
    g = {}
    for name, (f, d) in factory_parts_files().items():
        pre = glue_slot(d["params"], d["tags"][0], r3)
        if pre:
            g[name] = (f, d, pre)
    return g, r3


def cmd_thresh(names=None):
    make_factory()
    g, r3 = glued()
    tmp = tempfile.mkdtemp(prefix="pthr-")
    files = []
    for name, (f, d, pre) in g.items():
        if names and name not in names:
            continue
        d = json.loads(json.dumps(d))
        d["params"][pre + "compressor.ratio"] = 1.0
        d["params"][pre + "compressor.makeup"] = 0.0
        p = os.path.join(tmp, os.path.basename(f))
        json.dump(d, open(p, "w"))
        files.append(p)
    rows = render(files)
    punch = json.load(open(PJ)) if os.path.exists(PJ) else {}
    n = 0
    for name, (f, d, pre) in g.items():
        r = rows.get(name)
        if not r:
            continue
        below = r3.GLUE[d["tags"][0]][4]
        punch.setdefault(name, {})["thresh"] = round(max(-50.0, r["peak"] - below), 1)
        punch[name]["makeup"] = 0.0
        n += 1
    json.dump(punch, open(PJ, "w"), indent=1, sort_keys=True)
    make_factory()
    print("threshold of %d glue compressors" % n)


def cmd_makeup(rounds):
    for k in range(rounds):
        make_factory()
        g, r3 = glued()
        rows = render([v[0] for v in g.values()])
        punch = json.load(open(PJ))
        levels = json.load(open(LJ))
        moved = 0
        for name, (f, d, pre) in g.items():
            r = rows.get(name)
            if not r:
                continue
            delta = min(TARGET.get(d["tags"][0], -16.0) - r["active"], -1.5 - r["peak"])
            if abs(delta) < 0.3:
                continue
            moved += 1
            pz = punch.setdefault(name, {})
            m = pz.get("makeup", 0.0) + delta
            if m < 0.0:
                levels[name] = round(max(-24.0, levels.get(name, 0.0) + m), 1)
                pz["thresh"] = round(pz.get("thresh", -20.0) + m, 1)
                m = 0.0
            pz["makeup"] = round(min(24.0, m), 1)
        json.dump(punch, open(PJ, "w"), indent=1, sort_keys=True)
        json.dump(levels, open(LJ, "w"), indent=1, sort_keys=True)
        print("round %d: %d moved" % (k + 1, moved), flush=True)
        if not moved:
            break
    make_factory()


def cmd_levels(rounds):
    for k in range(rounds):
        make_factory()
        g, r3 = glued()
        files = {n: v for n, v in factory_parts_files().items() if n not in g}
        rows = render([v[0] for v in files.values()])
        levels = json.load(open(LJ))
        moved = 0
        for name, (f, d) in files.items():
            r = rows.get(name)
            if not r or (-20.0 <= r["active"] <= -12.0 and r["peak"] <= -1.2):
                continue
            delta = min(TARGET.get(d["tags"][0], -16.0) - r["active"], -1.5 - r["peak"])
            if abs(delta) < 0.3:
                continue
            moved += 1
            levels[name] = round(max(-24.0, min(24.0, levels.get(name, 0.0) + delta)), 1)
        json.dump(levels, open(LJ, "w"), indent=1, sort_keys=True)
        print("levels round %d: %d moved" % (k + 1, moved), flush=True)
        if not moved:
            break
    make_factory()


GJ = HERE + "/peak_guard.json"
CREST_ACTIVE = -19.0  # the body of a high-crest preset, 2 dB inside the plug-in level window
CREST_PEAK = -1.5


def cmd_crest(rounds, names):
    for k in range(rounds):
        make_factory()
        g, r3 = glued()
        files = factory_parts_files()
        rows = render([files[n][0] for n in names])
        levels, guard, punch = json.load(open(LJ)), json.load(open(GJ)), json.load(open(PJ))
        moved = 0
        for n in names:
            r = rows.get(n)
            if not r:
                continue
            d_l = CREST_ACTIVE - r["active"]
            excess = r["peak"] + d_l - CREST_PEAK
            if abs(d_l) < 0.3 and abs(excess) < 0.3:
                continue
            moved += 1
            levels[n] = round(max(-24.0, min(24.0, levels.get(n, 0.0) + d_l)), 1)
            if n in g:
                punch[n]["thresh"] = round(punch[n]["thresh"] + d_l, 1)
            old = guard.get(n)
            if isinstance(old, dict):
                gt = old["thresh"]
            elif old is None:
                gt = r["peak"] + 0.5  # first round: the guard starts at the current peak
            else:
                gt = old  # a held-chord guard: kept, moved with the gain
            new = round(min(-2.0, gt + d_l - 1.15 * excess), 1)
            guard[n] = new if isinstance(old, (int, float)) else {"thresh": new, "release": 40.0}
            print("  %-18s act %6.1f pk %5.1f -> gain %+.1f guard %.1f" % (n, r["active"], r["peak"], levels[n], new), flush=True)
        for f, o in ((LJ, levels), (GJ, guard), (PJ, punch)):
            json.dump(o, open(f, "w"), indent=1, sort_keys=True)
        print("crest round %d: %d moved" % (k + 1, moved), flush=True)
        if not moved:
            break
    make_factory()


def cmd_report(out):
    files = factory_parts_files()
    rows = render([v[0] for v in files.values()])
    res = {}
    for name, (f, d) in files.items():
        r = rows.get(name)
        if r:
            res[name] = dict(cat=d["tags"][0], active=r["active"], peak=r["peak"], crest=round(r["peak"] - r["active"], 1))
    json.dump(res, open(out, "w"), indent=1, sort_keys=True)
    bad = [(n, v) for n, v in res.items() if not (-20.5 <= v["active"] <= -11.5) or v["peak"] > -1.2]
    print("%d parts, %d outside (active -20.5..-11.5, peak <= -1.2)" % (len(res), len(bad)))
    for n, v in sorted(bad):
        print("  %-24s %-8s act %6.1f pk %5.1f" % (n, v["cat"], v["active"], v["peak"]))


if __name__ == "__main__":
    c = sys.argv[1]
    if c == "thresh":
        cmd_thresh(set(sys.argv[2].split(",")) if len(sys.argv) > 2 else None)
    elif c == "makeup":
        cmd_makeup(int(sys.argv[2]))
    elif c == "levels":
        cmd_levels(int(sys.argv[2]))
    elif c == "crest":
        cmd_crest(int(sys.argv[2]), sys.argv[3].split(","))
    elif c == "report":
        cmd_report(sys.argv[2])
