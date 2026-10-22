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
Presets with a peak guard (rack compressor, attack 0.1 ms, ratio 8 - make_factory.py) keep it as it is, but their
chord headroom is set so the chord reaches the guard at most GUARD_MARGIN dB hotter than a single note does (measured
with the guard taken out): the guard then works on chords as it was designed to on single notes, instead of squashing
held chords by up to 11 dB (the audible grit of p74). Limited polyphony takes a larger amount (the curve only reaches
log2(voices) / 3 of it); a mono part (or one at 24 dB) moves its level instead.

  python3 tools/presets/poly_cal.py baseline --render OLD_BIN --wavdir DIR     # phrase loudness before p74
  python3 tools/presets/poly_cal.py round --render BIN --wavdir DIR             # one calibration round, then "apply"
  python3 tools/presets/bank_curate.py apply
"""
import argparse, json, math, os, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bank_curate as bc  # noqa: E402
import poly_audit as pa  # noqa: E402

CHORD_TP = -1.5        # dBTP target of the 8-note chords (the regression test allows -1.0)
SINGLE_TP = -1.2       # a single note / the phrase stay below this
ROOM_MIN, ROOM_MAX = 0.0, 24.0
DEFAULT_ROOM = 4.5
GUARD_MARGIN = 3.0     # dB a chord may reach the peak guard above a single note
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


def guard_prefix(params):
    """Rack prefix of the preset's peak guard ("fx4.compressor." ...) or None."""
    for k, v in params.items():
        if k.endswith(".type") and v == 10:
            pre = k[:-4] + "compressor."
            if params.get(pre + "attack") == 0.1 and params.get(pre + "ratio") == 8.0 and params.get(pre + "release", 0) >= 120.0:
                return pre
    return None


def chord_and_single(render, wavdir, files, d):
    chord, single = {}, {}
    for c in ("8wide", "8close"):
        for wheel in (0, 127):
            for f, r in pa.render(render, wavdir, files, pa.CHORDS[c], 127, wheel, d).items():
                chord[f + ".ohpepreset"] = max(chord.get(f + ".ohpepreset", -200.0), r["tp"])
    for f, r in pa.render(render, wavdir, files, pa.CHORDS["1"], 127, 0, d).items():
        single[f + ".ohpepreset"] = r["tp"]
    return chord, single


def round_(args):
    base = json.load(open(BASE))
    bank = bc.load_bank()
    files = parts(bank, args.only)
    phrase = bc.render_rows(A(args.render, args.wavdir), files)
    with tempfile.TemporaryDirectory() as d:
        chord, single = chord_and_single(args.render, args.wavdir, files, d)
        # the peak-guarded presets once more without their guard: how much hotter than a single note a chord reaches it
        gdir = os.path.join(d, "unguarded")
        os.makedirs(gdir)
        gfiles = []
        for path in files:
            f = os.path.basename(path)
            pre = guard_prefix(bank[f]["params"])
            if pre:
                q = json.loads(json.dumps(bank[f]))
                q["params"][pre[:-len("compressor.")] + "type"] = 0
                json.dump(q, open(os.path.join(gdir, f), "w"))
                gfiles.append(os.path.join(gdir, f))
        gchord, gsingle = chord_and_single(args.render, args.wavdir, gfiles, d) if gfiles else ({}, {})
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
        share = math.log2(max(1, min(8, int(p.get("voices", 8))))) / 3.0  # limited polyphony reaches only this share
        if f in gchord and share >= 0.3:                         # the guard sees chords about as it sees single notes
            need = max(need, gchord[f] - gsingle.get(f, gchord[f]) - GUARD_MARGIN)
        nh = h
        if need > 0:
            if share < 0.3:                                      # mono: only the level helps
                ng = max(bc.GAIN_MIN, round(ng - need, 1))
            else:
                nh = h + need / share
                if nh > ROOM_MAX:
                    ng = max(bc.GAIN_MIN, round(ng - (nh - ROOM_MAX) * share, 1))
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
    a.add_argument("cmd", choices=["baseline", "round"])
    a.add_argument("--render")
    a.add_argument("--wavdir")
    a.add_argument("--only", nargs="*")
    args = a.parse_args()
    {"baseline": baseline, "round": round_}[args.cmd](args)


if __name__ == "__main__":
    main()
