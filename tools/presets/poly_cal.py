#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p74 chord headroom calibration of the factory part presets (one round per call, recorded in bank_curation.json).

The part parameter "chordroom" (dB the part's sum is lowered by at 8 sounding notes, log curve, see Part.h) gives
every preset room for chords; this script sets it per preset so a held 8-note chord at velocity 127 (wide and close
voicing, mod wheel 0 and max, master limiter off) stays at or below CHORD_TP dBTP, and moves the level parameter
"gain" back so the category phrase (the bank loudness measure of bank_curate.py) keeps the loudness it had before
p74 - single notes stay where they were or come closer to the -11 LUFS bank target, chords get the headroom.
The peak guards (rack compressor, attack 0.1 ms, ratio 8 - make_factory.py) become a soft last-resort safety:
attack 1 ms, threshold -2 dB, knee 4 dB (they used to squash held chords by up to 11 dB - audible grit).

  python3 tools/presets/poly_cal.py baseline --render OLD_BIN --wavdir DIR     # phrase loudness before p74
  python3 tools/presets/poly_cal.py guards                                      # relaxes the peak guards (once)
  python3 tools/presets/poly_cal.py round --render BIN --wavdir DIR             # one calibration round, then "apply"
  python3 tools/presets/bank_curate.py apply
"""
import argparse, json, os, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bank_curate as bc  # noqa: E402
import poly_audit as pa  # noqa: E402

CHORD_TP = -1.5        # dBTP target of the 8-note chords (the regression test allows -1.0)
SINGLE_TP = -1.2       # a single note / the phrase stay below this
ROOM_MIN, ROOM_MAX = 0.0, 24.0
DEFAULT_ROOM = 4.5
BASE = os.path.join(HERE, "poly_baseline.json")


class A:  # argument shim for bank_curate.render_rows
    def __init__(self, render, wavdir):
        self.render, self.wavdir, self.keep = render, wavdir, ""


def parts(bank, only):
    return [os.path.join(bc.BANK, f) for f in sorted(bank) if bank[f]["kind"] == "part" and (not only or f in only)]


def baseline(args):
    bank = bc.load_bank()
    rows = bc.render_rows(A(args.render, args.wavdir), parts(bank, args.only))
    json.dump({f: {"st": r["st"], "tp": r["tp"], "active": r["active"]} for f, r in rows.items()}, open(BASE, "w"), indent=0)


def guards(_args):
    cur = json.load(open(bc.CURATION, encoding="utf-8"))
    bank = bc.load_bank()
    n = 0
    for f, p in sorted(bank.items()):
        if p["kind"] != "part":
            continue
        for k, v in p["params"].items():
            if not (k.endswith(".type") and v == 10):
                continue
            pre = k[:-4] + "compressor."
            q = p["params"]
            if q.get(pre + "attack") == 0.1 and q.get(pre + "ratio") == 8.0 and q.get(pre + "release", 0) >= 120.0:
                cur["changed"].setdefault(f, {}).setdefault("params", {}).update(
                    {pre + "attack": 1.0, pre + "thresh": -2.0, pre + "knee": 4.0})
                n += 1
    json.dump(cur, open(bc.CURATION, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print("guards relaxed", n)


def round_(args):
    base = json.load(open(BASE))
    bank = bc.load_bank()
    files = parts(bank, args.only)
    phrase = bc.render_rows(A(args.render, args.wavdir), files)
    chord, single = {}, {}
    with tempfile.TemporaryDirectory() as d:
        for c in ("8wide", "8close"):
            for wheel in (0, 127):
                for f, r in pa.render(args.render, args.wavdir, files, pa.CHORDS[c], 127, wheel, d).items():
                    chord[f + ".ohpepreset"] = max(chord.get(f + ".ohpepreset", -200.0), r["tp"])
        for f, r in pa.render(args.render, args.wavdir, files, pa.CHORDS["1"], 127, 0, d).items():
            single[f + ".ohpepreset"] = r["tp"]
    cur = json.load(open(bc.CURATION, encoding="utf-8"))
    changed = 0
    log = []
    for f in sorted(phrase):
        if f not in bank or f not in chord:
            continue
        p = bank[f]["params"]
        g = float(p.get("gain", 0.0))
        h = float(p.get("chordroom", DEFAULT_ROOM))
        r = phrase[f]
        st0 = base.get(f, {}).get("st", r["st"])
        dg = st0 - r["st"]                                      # phrase loudness back to where it was
        dg = min(dg, SINGLE_TP - r["tp"], SINGLE_TP - single.get(f, -200.0))  # peak-safe (phrase, single note)
        if abs(dg) < 0.3:
            dg = 0.0
        ng = max(bc.GAIN_MIN, min(bc.GAIN_MAX, round(g + dg, 1)))
        dg = ng - g
        need = chord[f] + dg - CHORD_TP                          # chord peak after the gain move
        nh = h + need if need > 0 else h
        if need < -2.0 and h > DEFAULT_ROOM:                     # far below: give some back (not below the default)
            nh = max(DEFAULT_ROOM, h + need + 1.0)
        nh = max(ROOM_MIN, min(ROOM_MAX, round(nh, 1)))
        if "chordroom" not in p or abs(nh - h) >= 0.2 or abs(ng - g) >= 0.1:
            ch = cur["changed"].setdefault(f, {}).setdefault("params", {})
            ch["chordroom"] = nh
            if ng != g:
                cur.setdefault("levels_before", {}).setdefault(f, g)
                ch["gain"] = ng
            changed += 1
        log.append((chord[f], f, r["st"], st0, g, ng, h, nh, single.get(f)))
    json.dump(cur, open(bc.CURATION, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    log.sort(reverse=True)
    for row in log[:15]:
        print("chordTP %6.2f %s phraseST %.1f (was %.1f) gain %.1f->%.1f room %.1f->%.1f singleTP %s" % row)
    over = sum(1 for row in log if row[0] > -1.0)
    print("changed", changed, "| chords above -1 dBTP this round:", over)


def main():
    a = argparse.ArgumentParser()
    a.add_argument("cmd", choices=["baseline", "guards", "round"])
    a.add_argument("--render")
    a.add_argument("--wavdir")
    a.add_argument("--only", nargs="*")
    args = a.parse_args()
    {"baseline": baseline, "guards": guards, "round": round_}[args.cmd](args)


if __name__ == "__main__":
    main()
