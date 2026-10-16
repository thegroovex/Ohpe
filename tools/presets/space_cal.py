#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Level calibration of the "3D" factory presets (factory_parts_space.py): renders each one with ohpe_render (the
factory phrase, master limiter off), measures the active RMS and the peak, and writes the part gain (dB) that brings
it to -15 dBFS active RMS without letting the peak exceed -1.2 dBFS into SPACE_GAINS. Run make_factory.py afterwards
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


def write_gains(gains):
    src = open(SRC, encoding="utf-8").read()
    body = "SPACE_GAINS = {  # level calibration (space_cal.py): -15 dBFS active RMS, peaks <= -1.2 dBFS\n"
    body += "".join('    "%s": %.1f,\n' % (k, gains[k]) for k in sorted(gains)) + "}\n"
    src2, n = re.subn(r"SPACE_GAINS = \{[^}]*\}[^\n]*\n", lambda _m: body, src, count=1)
    assert n == 1, "SPACE_GAINS not found"
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
        cmd = [a.render, "--bank", os.path.join(ROOT, "resources", "samples", "bank.json"), "--wavdir", a.wavdir] + files
        res = subprocess.run(cmd, capture_output=True, text=True)
        sys.stderr.write(res.stderr)
        rows = [json.loads(l) for l in res.stdout.splitlines() if l.startswith("{")]
        assert len(rows) == len(files), "rendered %d of %d" % (len(rows), len(files))
        for r in rows:
            cur = gains.get(r["name"], json.load(open(next(f for f in files if f.endswith(r["file"] + ".ohpepreset"))))["params"].get("gain", 0.0))
            delta = min(TARGET - r["active"], PEAK_MAX - r["peak"])
            gains[r["name"]] = round(min(24.0, max(-24.0, cur + delta)), 1)
            print("pass %d  %-18s act %6.1f pk %5.1f  gain %+5.1f -> %+5.1f" % (it + 1, r["name"], r["active"], r["peak"], cur, gains[r["name"]]))
        write_gains(gains)
        if a.write:
            subprocess.run([sys.executable, os.path.join(HERE, "make_factory.py")], check=True)
    print(json.dumps({r["name"]: {"active": r["active"], "peak": r["peak"]} for r in rows}, indent=1))


if __name__ == "__main__":
    main()
