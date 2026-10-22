#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p66: the Key Gravity factory presets (part presets built around pN.grav.*). Writes only these files into
resources/presets/factory. Levels ("gain") are calibrated to the bank target (-11 LUFS short-term max, true peak
<= -1 dBTP, see bank_curation.json "loudness") with ohpe_render + bank_curate.py analyse --only <files>.

  python3 tools/presets/factory_parts_gravity.py
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
BANK = os.path.join(HERE, "..", "..", "resources", "presets", "factory")
WHEEL = 9  # mod::SrcModWheel
DST = {"gravamt": 90, "gravglide": 91, "gravcurve": 92, "gravformant": 93, "gravmix": 94}


def grav(amt, target, glide, curve, formant, mix=1.0, quality=1):
    return {"grav.amt": amt, "grav.target": target, "grav.glide": glide, "grav.curve": curve, "grav.formant": formant,
            "grav.mix": mix, "grav.quality": quality}


def wheel(dst, amt):
    return {"mod.s1.src": WHEEL, "mod.s1.via": 0, "mod.s1.dst": DST[dst], "mod.s1.amt": amt, "mod.s1.curve": 0}


def comp(slot, makeup):
    return {slot + ".type": 10, slot + ".compressor.thresh": -10.0, slot + ".compressor.ratio": 3.0, slot + ".compressor.attack": 2.0,
            slot + ".compressor.release": 120.0, slot + ".compressor.knee": 6.0, slot + ".compressor.makeup": makeup,
            slot + ".compressor.mix": 1.0}


def hall(slot, mix, size=0.85, decay=5.0):
    return {slot + ".type": 15, slot + ".hall.mode": 1, slot + ".hall.size": size, slot + ".hall.decay": decay, slot + ".hall.damping": 0.5,
            slot + ".hall.predelay": 30.0, slot + ".hall.width": 1.0, slot + ".hall.mix": mix}


PRESETS = [
    ("pads-gravity-noise-choir", "Gravity Noise Choir", ["Pads", "Textures"], "analog",
     "Pure noise pulled into the chord you hold - Key Gravity at full on Chord. Wheel: from magnetic to gentle.",
     {"engine": 2, "analog.shape": 0.5, "analog.detune": 0.0, "analog.cutoff": 7000, "analog.reso": 0.1, "analog.oscmix": 0.0,
      "analog.sub": 0.0, "analog.noise": 1.0, "analog.ftype": 0, "analog.drive": 0.1, "analog.keytrack": 0.5,
      "amp.a": 0.5, "amp.d": 1.0, "amp.s": 1.0, "amp.r": 1.6, "mod.a": 0.5, "mod.d": 1.0, "mod.s": 1.0, "mod.r": 1.0, "mod.amt": 0.0,
      "voices": 8, **grav(1.0, 1, 0.35, 0.6, 0.3, 1.0, 2), **comp("fx", 3.0), **hall("fx2", 0.3), **wheel("gravcurve", -0.8),
      "gain": 6.0}),
    ("textures-gravity-hat-pad", "Gravity Hat Pad", ["Textures", "Pads"], "cluster",
     "A metallic hat cluster held long and melted into chord and scale - drum to pad. Wheel: less gravity, more metal.",
     {"engine": 6, "cluster.detune": 1.0, "cluster.spread": 0.7, "cluster.tone": 0.85, "cluster.chord": 11, "cluster.stack": 7,
      "cluster.shape": 1.0, "cluster.side": 1.0, "cluster.reso": 0.2, "cluster.keytrack": 0.3, "cluster.drift": 0.2,
      "amp.a": 0.25, "amp.d": 1.5, "amp.s": 0.9, "amp.r": 2.2, "mod.a": 0.001, "mod.d": 0.05, "mod.s": 0.0, "mod.r": 0.05, "mod.amt": 0.0,
      "voices": 8, **grav(1.0, 2, 0.6, 0.3, 0.5, 1.0, 2),
      "fx.type": 1, "fx.chorus.rate": 0.3, "fx.chorus.depth": 0.5, "fx.chorus.mix": 0.3, "fx.chorus.voices": 3, "fx.chorus.drift": 0.3,
      "fx.chorus.spread": 0.85, "fx.chorus.lowcut": 200.0, **comp("fx2", 2.0), **hall("fx3", 0.32), **wheel("gravamt", -0.6),
      "gain": 4.0}),
    ("keys-gravity-bell-lock", "Gravity Bell Lock", ["Keys"], "fm",
     "An inharmonic FM bell whose partials lock into the key - magnetic Key Gravity on Scale. Wheel: lets them drift free.",
     {"engine": 0, "fm.algo": 2, "fm.modratio": 1.41, "fm.index": 0.45, "fm.op1ratio": 1, "fm.op2ratio": 3.5, "fm.op3ratio": 2,
      "fm.op4ratio": 7, "fm.op1level": 1.0, "fm.op2level": 0.7, "fm.op3level": 0.35, "fm.op4level": 0.25, "fm.op5level": 0.0,
      "fm.op6level": 0.0, "amp.a": 0.002, "amp.d": 1.6, "amp.s": 0.0, "amp.r": 1.0, "mod.a": 0.002, "mod.d": 0.6, "mod.s": 0.2,
      "mod.r": 0.5, "mod.amt": 0.8, "voices": 10, **grav(0.75, 0, 0.1, 0.8, 0.7, 1.0, 1), **comp("fx", 4.0),
      "fx2.type": 5, "fx2.delay.time": 0.375, "fx2.delay.feedback": 0.3, "fx2.delay.tone": 0.5, "fx2.delay.pingpong": 1,
      "fx2.delay.mix": 0.2, **wheel("gravamt", -0.7), "gain": 8.0}),
    ("drums-gravity-drum-wash", "Gravity Drum Wash", ["Drums", "Textures"], "analog",
     "A noise snare whose hits ring out as pitched chimes in key - Key Gravity at full, low latency. Wheel: back to the snare.",
     {"engine": 2, "analog.shape": 0.5, "analog.detune": 0.0, "analog.cutoff": 3500, "analog.reso": 0.25, "analog.oscmix": 0.0,
      "analog.sub": 0.2, "analog.noise": 1.0, "analog.ftype": 2, "analog.drive": 0.4, "analog.keytrack": 0.2,
      "amp.a": 0.001, "amp.d": 0.45, "amp.s": 0.0, "amp.r": 0.5, "mod.a": 0.001, "mod.d": 0.05, "mod.s": 0.0, "mod.r": 0.1,
      "mod.amt": 0.25, "velsens": 0.6, "voices": 4, **grav(1.0, 0, 0.05, 0.5, 0.2, 1.0, 0), **comp("fx", 1.0),
      **hall("fx2", 0.25, 0.7, 3.0), **wheel("gravamt", -0.8), "gain": 10.0}),
]


def main():
    lv = {}
    lvf = os.path.join(HERE, "gravity_levels.json")  # calibrated gains (written by hand from the analyse run)
    if os.path.exists(lvf):
        lv = json.load(open(lvf))
    for fname, name, tags, engine, comment, params in PRESETS:
        p = dict(params)
        if fname in lv:
            p["gain"] = lv[fname]
        doc = {"format": "ohpe-preset", "version": 2, "kind": "part", "name": name, "author": "groovex", "tags": tags,
               "engine": engine, "comment": comment, "params": p}
        with open(os.path.join(BANK, fname + ".ohpepreset"), "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print("wrote", fname)


if __name__ == "__main__":
    main()
