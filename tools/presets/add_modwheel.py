#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p29: gives every factory preset a musically meaningful mod wheel. Adds one routing of the part's modulation
matrix to the first free slot (source or via = Mod Wheel) chosen by the preset's role:

  leads, solo voices, organs, brass  vibrato: LFO 1 (when it already runs at a vibrato rate) or a free LFO 2 / 3
                                     set to 5.3 Hz with a short fade-in, scaled by the wheel, to Pitch (+-0.36 st)
  tines / reeds with an auto-pan     deeper auto-pan / tremolo (wheel -> LFO 1 depth)
  pads, basses, plucks, keys, FX     filter / tone opening (wheel -> Timbre: cutoff, FM index, pluck brightness);
                                     closing where the tone is already fully open (sampler at full tone, vowel
                                     at its end); a harder strike for struck pluck bodies (bars, plates)
  grain pads, textures and FX        grain spray (wheel -> Spray)
  vocal engine choirs                vowel movement (wheel -> Timbre = vowel)
  subs, booms, kicks                 harder, driven character (wheel -> Character Hard)
  arps with a delay / echo           echo throw (wheel -> the delay / echo feedback, at most 0.75)
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
DST_FXPARAM1 = 43          # fx1p1 ... fx6p4 = 43 ... 66
DST_CHARHARD = 82
NUM_SLOTS = 16
GRAIN_SPRAY = 3            # 0-based engine parameter of the grain engine: pos, size, density, spray
ENGINE_PLUCK, ENGINE_GRAIN, ENGINE_VOCAL, ENGINE_SAMPLER = 3, 4, 5, 7
FX_DELAY, FX_ECHO = 5, 14

VIB_LFO1 = 0.03            # LFO 1 (-1..1 raw) x wheel x 12 st -> +-0.36 st
VIB_FREE = 0.03

# Level calibration (internal renders, wheel 0 vs 127): amount factors for presets whose peak or loudness rose too
# much with the full amount. Target: at most +1.5 dB active RMS, and the peak below -0.5 dBFS or at most +0.5 dB.
SCALE = {
    "arp-formant-arp": 0.6, "bass-deep-pluck-sub": 0.6, "bass-neuro-swirl": 0.3, "bass-punch-sub": 0.3,
    "bass-rolling-bass": 0.3, "bass-screech-bass": 0.6, "bass-upright-pluck": 0.7, "boom-boom-kick-bass": 0.3,
    "drums-wood-block": 0.6, "keys-dusty-ep": 0.5, "keys-dusty-keys": 0.5, "keys-music-box": 0.3,
    "leads-breath-lead": 0.5, "pads-glass-grains": 0.6, "pads-halo-drift": 0.5, "pads-overhead-choir": 0.4,
    "pads-spectral-morph": 0.5, "pads-super-stack": 0.3, "pads-warm-horizon": 0.3, "textures-breath-bore": 0.3,
    "textures-fourths-drift": 0.8,
}


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

    scale = 1.0

    def add(self, src, via, dst, amt):
        amt *= self.scale
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
    """Wheel -> Timbre. Where the tone is already fully open the wheel closes it instead (a sampler at full tone, a
    vocal vowel at its end), and struck pluck bodies (bars, plates) that ignore the brightness get a harder strike."""
    e = part.engine()
    if e == ENGINE_SAMPLER and float(part.get("sampler.tone", 1.0)) >= 0.95:
        part.add(SRC_WHEEL, SRC_OFF, DST_TIMBRE, -0.45)
        return "softer, darker tone"
    if e == ENGINE_VOCAL and float(part.get("vocal.vowel", 0.0)) + 2.0 * amt > 4.0:
        part.add(SRC_WHEEL, SRC_OFF, DST_TIMBRE, -amt)
        return what
    if e == ENGINE_PLUCK and float(part.get("pluck.material", 0.0)) >= 0.6:
        return hard(part, 0.25, "harder strike")
    part.add(SRC_WHEEL, SRC_OFF, DST_TIMBRE, amt)
    return what


def spray(part, amt):
    part.add(SRC_WHEEL, SRC_OFF, DST_ENGINE1 + GRAIN_SPRAY, amt)
    return "grain spray"


def hard(part, amt, what="harder, driven character"):
    part.add(SRC_WHEEL, SRC_OFF, DST_CHARHARD, amt)
    return what


def echo_throw(part, amt):
    """Longer echo tails: the wheel raises the delay / echo feedback (FX param target of the slot), at most to 0.75.
    (The FX Mix target moves the rack slot's own mix, which is at 100 % in these presets.)"""
    k = part.fx_slot(FX_DELAY, FX_ECHO)
    if k is None:
        return None
    t = int(part.get(slot_prefix(k) + "type"))
    name, j, hi = ("delay", 1, 0.95) if t == FX_DELAY else ("echo", 0, 1.05)  # FX param 2 = Delay feedback, 1 = Echo feedback
    base = float(part.get(slot_prefix(k) + name + ".feedback", 0.4))
    norm = max(0.0, min(0.75, base + amt * hi) - base) / hi
    if norm < 0.05:
        return None
    part.add(SRC_WHEEL, SRC_OFF, DST_FXPARAM1 + 4 * k + j, norm)
    return "echo throw (longer delay feedback)"


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
        part.scale = SCALE.get(stem, 1.0)
        if part.has_wheel() or morph_wheel(p):
            return stem, ["(already)"]
        done.append(rule(stem, doc, part))
    else:
        for i in range(1, 5):
            if "p%d.engine" % i not in p:
                continue
            part = Part(p, "p%d." % i)
            part.scale = SCALE.get(stem, 1.0)
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
