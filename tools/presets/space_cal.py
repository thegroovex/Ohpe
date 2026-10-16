#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Level calibration of the "3D" factory presets (factory_parts_space.py): renders each one with ohpe_render in both
windows of the level tests (4 beats + 1 s tail as the plug-in test, 2 beats + 0.4 s as the core test; master limiter
off), measures the active RMS and the peak (the louder window counts for the peak, the quieter one for the level), and writes the part gain (dB) that brings
it to -15 dBFS active RMS without letting the peak exceed -1.2 dBFS into SPACE_GAINS (into SPACE_OUT, the Multiband
output, for presets that end in it: the compressor makes the part gain ineffective there). Run make_factory.py afterwards
(this script does it with --write); two passes settle presets whose chain is not linear (drive, compressor).

  python3 tools/presets/space_cal.py --render build-core/tests/core/ohpe_render --wavdir DIR [--passes 2] [--write]
"""
import argparse, json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
TARGET, PEAK_MAX = -15.0, -1.2
SRC = os.path.join(HERE, "factory_parts_space.py")


def load():
    for m in [m for m in list(sys.modules) if m.startswith("factory_")]:
        del sys.modules[m]
    import factory_parts  # noqa: F401  (imports factory_parts_space in the right order)
    import factory_parts_space as s
    return s


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def write_dict(name, values, note):
    src = open(SRC, encoding="utf-8").read()
    body = "%s = {  # %s\n" % (name, note)
    body += "".join('    "%s": %.1f,\n' % (k, values[k]) for k in sorted(values)) + "}\n"
    src2, n = re.subn(name + r" = \{[^}]*\}[^\n]*\n", lambda _m: body, src, count=1)
    assert n == 1, name + " not found"
    open(SRC, "w", encoding="utf-8").write(src2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--wavdir", required=True)
    ap.add_argument("--passes", type=int, default=2)
    ap.add_argument("--write", action="store_true", help="run make_factory.py after every pass")
    a = ap.parse_args()
    rows = []
    for it in range(a.passes):
        s = load()
        gains = dict(s.SPACE_GAINS)
        files = [os.path.join(ROOT, "resources", "presets", "factory", "%s-%s.ohpepreset" % (p["tags"][0].lower(), slug(p["name"])))
                 for p in s.PRESETS_SPACE]
        windows = []
        for beats, tail in (("4", "1"), ("2", "0.4")):
            cmd = [a.render, "--bank", os.path.join(ROOT, "resources", "samples", "bank.json"), "--wavdir", a.wavdir,
                   "--beats", beats, "--tail", tail] + files
            res = subprocess.run(cmd, capture_output=True, text=True)
            sys.stderr.write(res.stderr)
            got = [json.loads(l) for l in res.stdout.splitlines() if l.startswith("{")]
            assert len(got) == len(files), "rendered %d of %d" % (len(got), len(files))
            windows.append(got)
        rows = [dict(r, active=min(r["active"], q["active"]), peak=max(r["peak"], q["peak"])) for r, q in zip(*windows)]
        outs = dict(s.SPACE_OUT)
        for r in rows:
            params = json.load(open(next(f for f in files if f.endswith(r["file"] + ".ohpepreset"))))["params"]
            delta = min(TARGET - r["active"], PEAK_MAX - r["peak"])
            mb = [k for k in params if k.endswith(".multiband.output")]
            if mb:  # compressed at the end of the chain: its output is the level control
                cur = outs.get(r["name"], params[mb[0]])
                outs[r["name"]] = round(min(12.0, max(-12.0, cur + delta)), 1)
                new, what = outs[r["name"]], "out "
            else:
                cur = gains.get(r["name"], params.get("gain", 0.0))
                gains[r["name"]] = round(min(24.0, max(-24.0, cur + delta)), 1)
                new, what = gains[r["name"]], "gain"
            print("pass %d  %-18s act %6.1f pk %5.1f  %s %+5.1f -> %+5.1f" % (it + 1, r["name"], r["active"], r["peak"], what, cur, new))
        write_dict("SPACE_GAINS", gains, "level calibration (space_cal.py): -15 dBFS active RMS, peaks <= -1.2 dBFS")
        write_dict("SPACE_OUT", outs, "Multiband output (dB) from the same calibration")
        if a.write:
            subprocess.run([sys.executable, os.path.join(HERE, "make_factory.py")], check=True)
    print(json.dumps({r["name"]: {"active": r["active"], "peak": r["peak"]} for r in rows}, indent=1))


if __name__ == "__main__":
    main()
