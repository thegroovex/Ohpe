#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p29: gives every factory preset a musically meaningful mod wheel. Adds one routing of the part's modulation
matrix to the first free slot (source or via = Mod Wheel) chosen by the preset's role:

  leads, solo voices, organs, brass  vibrato: LFO 1 (when it already runs at a vibrato rate) or a free LFO 2 / 3
                                     set to 5.3 Hz with a short fade-in, scaled by the wheel, to Pitch (+-0.36 st)
  tines / reeds with an auto-pan     deeper auto-pan / tremolo (wheel -> LFO 1 depth)
  pads, basses, plucks, keys, FX     filter / tone opening (wheel -> Timbre: cutoff, FM index, pluck brightness)
  grain pads, textures and FX        grain spray (wheel -> Spray)
  vocal engine choirs                vowel movement (wheel -> Timbre = vowel)
  subs, booms, kicks                 harder, driven character (wheel -> Character Hard)
  arps with a delay / echo           echo throw (wheel -> the delay slot's mix)
  morph presets                      already drive Morph X from the wheel (morph.xsrc = Mod Wheel): unchanged
  projects                           every melodic part gets the rule of its engine (spray, vowel, timbre)

At wheel 0 every preset sounds exactly as before (the wheel scales the routing). Idempotent: a preset that already
has a wheel routing is left alone. The amounts were checked with internal renders (ohpe_render --wheel 127):
no preset gets louder than +3 dB active RMS or peaks above -0.5 dBFS with the wheel fully up.

  python3 tools/presets/add_modwheel.py [--dry]
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
FACTORY = os.path.join(ROOT, "resources", "presets", "factory")

# matrix tables (src/core/mod/ModMatrix.cpp, append-only)
SRC_OFF, SRC_LFO1, SRC_LFO2, SRC_LFO3, SRC_WHEEL = 0, 1, 2, 3, 9
DST_PITCH, DST_TIMBRE = 1, 4
DST_ENGINE1 = 5            # engine parameter 1 (the engine's own order)
DST_LFO1DEPTH = 31
DST_FXMIX1 = 37            # fx1mix ... fx6mix = 37 ... 42
DST_CHARHARD = 82
NUM_SLOTS = 16
GRAIN_SPRAY = 3            # 0-based engine parameter of the grain engine: pos, size, density, spray
ENGINE_GRAIN, ENGINE_VOCAL = 4, 5
FX_DELAY, FX_ECHO = 5, 14

VIB_LFO1 = 0.03            # LFO 1 (-1..1 raw) x wheel x 12 st -> +-0.36 st
VIB_FREE = 0.03

# Per-preset overrides (file stem -> kind) where the role rule would pick a weaker routing
OVERRIDES = {}


def slot_prefix(k):
    return "fx." if k == 0 else "fx%d." % (k + 1)


class Part:
    """A view of one part's parameters (part preset: plain keys, project: "pN." keys)."""

    def __init__(self, params, prefix=""):
        self.p, self.pre = params, prefix

    def get(self, key, default=None):
        return self.p.get(self.pre + key, default)

    def set(self, key, value):
        self.p[self.pre + key] = value

    def routes(self):
        out = []
        for k in range(1, NUM_SLOTS + 1):
            s = "mod.s%d." % k
            out.append((k, int(self.get(s + "src", 0)), int(self.get(s + "via", 0)), int(self.get(s + "dst", 0)),
                        float(self.get(s + "amt", 0.0))))
        return out

    def has_wheel(self):
        return any((src == SRC_WHEEL or (via == SRC_WHEEL and src != SRC_OFF)) and dst != 0 and amt != 0.0
                   for _, src, via, dst, amt in self.routes())

    def free_slot(self):
        for k, src, via, dst, amt in self.routes():
            if src == SRC_OFF and dst == 0:
                return k
        return None

    def lfo_used(self, src):
        return any(s == src or v == src for _, s, v, d, a in self.routes() if d != 0 and a != 0.0 and s != SRC_OFF)

    def add(self, src, via, dst, amt):
        k = self.free_slot()
        assert k is not None, "matrix full"
        s = "mod.s%d." % k
        self.set(s + "src", src)
        self.set(s + "via", via)
        self.set(s + "dst", dst)
        self.set(s + "amt", round(amt, 3))
        self.set(s + "curve", 0)
        return k

    def engine(self):
        return int(self.get("engine", 0))

    def fx_slot(self, *types):
        for k in range(6):
            if int(self.get(slot_prefix(k) + "type", 0)) in types:
                return k
        return None


def vibrato(part, amount=VIB_LFO1):
    """Wheel-scaled vibrato. Returns a description or None when no LFO is free."""
    rate = part.get("lfo.rate")
    if rate is not None and 3.5 <= float(rate) <= 8.0 and int(part.get("lfo.shape", 0)) in (0, 1):
        part.add(SRC_LFO1, SRC_WHEEL, DST_PITCH, amount)
        return "vibrato (LFO 1)"
    for src, name in ((SRC_LFO2, "lfo2"), (SRC_LFO3, "lfo3")):
        if part.lfo_used(src):
            continue
        part.set(name + ".rate", 5.3)
        part.set(name + ".sync", 0)
        part.set(name + ".shape", 0)
        part.set(name + ".phase", 0.0)
        part.set(name + ".fade", 0.25)
        part.set(name + ".mode", 1)
        part.set(name + ".depth", 1.0)
        part.add(src, SRC_WHEEL, DST_PITCH, VIB_FREE)
        return "vibrato (" + name.upper().replace("LFO", "LFO ") + ")"
    return None


def timbre(part, amt, what="filter / tone opening"):
    part.add(SRC_WHEEL, SRC_OFF, DST_TIMBRE, amt)
    return what


def spray(part, amt):
    part.add(SRC_WHEEL, SRC_OFF, DST_ENGINE1 + GRAIN_SPRAY, amt)
    return "grain spray"


def hard(part, amt):
    part.add(SRC_WHEEL, SRC_OFF, DST_CHARHARD, amt)
    return "harder, driven character"


def echo_throw(part, amt):
    k = part.fx_slot(FX_DELAY, FX_ECHO)
    if k is None:
        return None
    part.add(SRC_WHEEL, SRC_OFF, DST_FXMIX1 + k, amt)
    return "echo throw (delay mix)"


def lfo1_deeper(part, amt):
    part.add(SRC_WHEEL, SRC_OFF, DST_LFO1DEPTH, amt)
    return "deeper auto-pan / tremolo (LFO 1 depth)"


def has_pitch_lfo(part):
    return int(part.get("lfo.dest", -1)) == 0 and float(part.get("lfo.depth", 0.0)) > 0.0


def by_engine(part):
    e = part.engine()
    if e == ENGINE_GRAIN:
        return spray(part, 0.35)
    if e == ENGINE_VOCAL:
        return timbre(part, 0.2, "vowel movement")
    return timbre(part, 0.25)


def rule(stem, doc, part):
    cat = stem.split("-")[0]
    name = stem[len(cat) + 1:]
    tags = doc.get("tags", [])
    e = part.engine()
    kind = OVERRIDES.get(stem)
    if kind:
        return kind(part)
    if cat == "leads" or (cat == "vocal" and "solo" in name):
        return vibrato(part) or timbre(part, 0.25)
    if cat == "vocal":
        return timbre(part, 0.2, "vowel movement") if e == ENGINE_VOCAL else timbre(part, 0.25)
    if cat == "keys":
        if "organ" in name or "brass" in name or has_pitch_lfo(part):
            return vibrato(part, 0.025) or timbre(part, 0.2)
        if (int(part.get("lfo.dest", -1)) in (2, 3) and float(part.get("lfo.depth", 0.0)) > 0.0
                and any(w in name for w in ("tine", "reed", "vib", "ep"))):
            return lfo1_deeper(part, 0.3)
        return timbre(part, 0.2, "brighter, harder tone")
    if cat in ("pads", "textures"):
        if e == ENGINE_GRAIN:
            return spray(part, 0.35 if cat == "pads" else 0.4)
        if e == ENGINE_VOCAL:
            return timbre(part, 0.2, "vowel movement")
        return timbre(part, 0.3)
    if cat == "bass":
        if "sub" in name or "Boom" in tags or "sine" in name:
            return hard(part, 0.2)
        return timbre(part, 0.25)
    if cat == "boom":
        return hard(part, 0.2)
    if cat == "drums":
        if "kick" in name:
            return hard(part, 0.15)
        return timbre(part, 0.2, "brighter, snappier tone")
    if cat == "plucks":
        return timbre(part, 0.25, "brighter pluck")
    if cat == "arp":
        return echo_throw(part, 0.25) or timbre(part, 0.25, "brighter notes")
    if cat == "fx":
        return spray(part, 0.4) if e == ENGINE_GRAIN else timbre(part, 0.35)
    return by_engine(part)


def morph_wheel(params):
    return int(params.get("morph.on", 0)) == 1 and 3 in (int(params.get("morph.xsrc", 0)), int(params.get("morph.ysrc", 0)))


def process(path, dry=False):
    with open(path) as f:
        doc = json.load(f)
    stem = os.path.basename(path)[:-len(".ohpepreset")]
    p = doc["params"]
    done = []
    if doc["kind"] == "part":
        part = Part(p)
        if part.has_wheel() or morph_wheel(p):
            return stem, ["(already)"]
        done.append(rule(stem, doc, part))
    else:
        for i in range(1, 5):
            if "p%d.engine" % i not in p:
                continue
            part = Part(p, "p%d." % i)
            if part.has_wheel():
                done.append("p%d (already)" % i)
                continue
            done.append("p%d %s" % (i, by_engine(part)))
    if not dry:
        with open(path, "w") as f:
            json.dump(doc, f, indent=2, ensure_ascii=False)
            f.write("\n")
    return stem, done


def main():
    dry = "--dry" in sys.argv
    for fn in sorted(os.listdir(FACTORY)):
        if fn.endswith(".ohpepreset"):
            stem, done = process(os.path.join(FACTORY, fn), dry)
            print("%-36s %s" % (stem, "; ".join(done)))


if __name__ == "__main__":
    main()
