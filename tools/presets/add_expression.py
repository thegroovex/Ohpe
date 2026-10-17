#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p33: aftertouch -> glide time for every factory part that glides (portamento). Sets the part's Expression page
aftertouch slot 1 ("expr.at.dst1/amt1/curve1") to Glide Time with a positive amount and an S-curve:

  more pressure = a longer, slower glide. A light touch keeps the line tight; leaning into the key while playing
  legato turns the slide into a slow, vocal swoop (the pressure is usually already there when the next note starts,
  so the glide stretches exactly when the player pushes into the phrase).

Amounts by the preset's own glide time (the amount moves the normalised glide time, skew 0.4 over 0 ... 2 s):
  short glides (<= 50 ms, basses / leads)   +30 %   e.g. 30 ms -> about 0.33 s at full pressure
  medium glides (<= 150 ms)                 +25 %   e.g. 120 ms -> about 0.55 s
  long glides (pads)                        +15 %   e.g. 0.5 s -> about 0.9 s

Only the aftertouch part of the Expression page is touched (velocity slots, the mod matrix and the mod wheel routings
stay as they are). Without pressure every preset sounds exactly as before. Idempotent: a part whose aftertouch slot 1
already has an amount is left alone.

  python3 tools/presets/add_expression.py [--dry]
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
FACTORY = os.path.join(ROOT, "resources", "presets", "factory")

TARGET_GLIDE = 12  # mod::expr::Glide (src/core/mod/Expression.h, append-only)
CURVE_S = 3        # mod::CurveS


def amount_for(glide):
    if glide <= 0.05:
        return 0.30
    if glide <= 0.15:
        return 0.25
    return 0.15


def apply(params, pre):
    glide = float(params.get(pre + "glide", 0.0))
    if glide <= 0.0:
        return None
    if float(params.get(pre + "expr.at.amt1", 0.0)) != 0.0:
        return "(already)"
    params[pre + "expr.at.dst1"] = TARGET_GLIDE
    params[pre + "expr.at.amt1"] = amount_for(glide)
    params[pre + "expr.at.curve1"] = CURVE_S
    return "AT -> glide +%d %%" % round(amount_for(glide) * 100)


def process(path, dry=False):
    with open(path) as f:
        doc = json.load(f)
    p = doc["params"]
    done = []
    if doc["kind"] == "part":
        r = apply(p, "")
        if r: done.append(r)
    else:
        for i in range(1, 5):
            if "p%d.engine" % i in p:
                r = apply(p, "p%d." % i)
                if r: done.append("p%d %s" % (i, r))
    if done and not dry:
        with open(path, "w") as f:
            json.dump(doc, f, indent=2, ensure_ascii=False)
            f.write("\n")
    return done


def main():
    dry = "--dry" in sys.argv
    n = 0
    for fn in sorted(os.listdir(FACTORY)):
        if fn.endswith(".ohpepreset"):
            done = process(os.path.join(FACTORY, fn), dry)
            if done:
                n += 1
                print("%-36s %s" % (fn[:-len(".ohpepreset")], "; ".join(done)))
    print("%d presets" % n)


if __name__ == "__main__":
    main()
