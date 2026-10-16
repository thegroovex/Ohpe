# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""0.8 (sound revision 3): 120 factory part presets - modern bass music (reese, neuro, rollers, wobble and
growl basses), wide leads, pianos and electric pianos, thick granular sounds, full arps, evolving
atmospheres, stereo effects and the Hybrid presets (two engines in one part with cross-modulation).

They use the 0.8 additions: the new FX slot types (shaper, squash, comb, formant, flanger, wobble, tremolo,
width), the layer engine ("layer.*"), the Wavetable unison stack / FM, the keyboard instruments and the
MPE / aftertouch targets. Every part has one FX slot: each preset lists the effects that suit it in
order of importance and the first one that fits (FX_ORDER, or FX_PICK per preset) goes into the slot.
Every preset moves when held (an LFO is added where the sound has none).
German descriptions: comments_de_r3.py. Imported by factory_parts.py.
"""
from factory_parts import (P as _P, ENGINES, an, fm, wt, pl, cl, gr, vo, sm, amp, mod, lfo, chorus, phaser, delay, reverb, crush,
                           drive, pitchenv, porta, arp, MONO)

TYPES = {"off": 0, "chorus": 1, "phaser": 2, "bitcrush": 3, "filterdrive": 4, "delay": 5, "reverb": 6, "shaper": 7, "squash": 8,
         "comb": 9, "formant": 10, "flanger": 11, "wobble": 12, "tremolo": 13, "width": 14}
SYNC = {"free": 0, "1/1": 1, "1/2": 2, "1/4": 3, "1/8": 4, "1/16": 5, "1/32": 6, "1/2T": 7, "1/4T": 8, "1/8T": 9, "1/16T": 10,
        "1/2.": 11, "1/4.": 12, "1/8.": 13, "2/1": 14, "4/1": 15}
CURVE = {"soft": 0, "hard": 1, "fold": 2, "tube": 3, "rectify": 4, "sine": 5, "crush": 6}
WSHAPE = {"sine": 0, "tri": 1, "sawdown": 2, "sawup": 3, "square": 4, "sh": 5}
KEYS = {"grand": 1, "upright": 2, "tine": 3, "reed": 4}  # "inst2" (0.8 keyboard instruments)


def fx(slot, kind, **kw):
    """An effect candidate for the part's FX slot ("fx."). `slot` is kept for readability (the order of a fuller
    chain); P() picks one effect per preset."""
    d = {"fx.type": TYPES[kind]}
    for k, v in kw.items():
        if k == "sync":
            v = SYNC[v]
        elif kind == "shaper" and k == "type":
            v = CURVE[v]
        elif kind == "wobble" and k == "shape":
            v = WSHAPE[v]
        elif kind == "wobble" and k == "mode":
            v = {"lp": 0, "bp": 1}[v]
        elif kind == "filterdrive" and k == "mode":
            v = {"lp": 0, "bp": 1, "hp": 2}[v]
        elif kind == "tremolo" and k == "mode":
            v = {"trem": 0, "pan": 1}[v]
        elif kind == "tremolo" and k == "shape":
            v = {"sine": 0, "tri": 1, "square": 2}[v]
        d["fx." + kind + "." + k] = v
    return {"__fx__": kind, "params": d}


# which effect a preset keeps, by main category (first match wins)
FX_ORDER = {
    "Bass": ["wobble", "formant", "comb", "squash", "shaper", "flanger", "phaser", "filterdrive", "chorus", "width", "delay"],
    "Leads": ["delay", "reverb", "squash", "flanger", "width", "chorus", "shaper", "filterdrive", "bitcrush"],
    "Pads": ["reverb", "flanger", "tremolo", "comb", "width", "chorus", "phaser", "delay", "wobble"],
    "Textures": ["reverb", "comb", "flanger", "delay", "width", "chorus", "formant", "bitcrush"],
    "Vocal": ["reverb", "formant", "width", "chorus", "delay"],
    "Keys": ["tremolo", "reverb", "chorus", "delay", "squash", "filterdrive", "width"],
    "Plucks": ["delay", "reverb", "width", "squash", "comb", "chorus"],
    "Arp": ["delay", "reverb", "phaser", "squash", "width", "chorus", "shaper"],
    "FX": ["flanger", "wobble", "delay", "reverb", "tremolo", "squash", "bitcrush", "width"],
}
# per-preset choice where the category order would pick the wrong one
FX_PICK = {
    "Classic Reese": "squash", "Phase Reese": "phaser", "Neuro Reese": "squash", "Distorted Reese": "shaper",
    "Flanged Reese": "flanger", "Wide Reese": "squash", "Analog Reese": "phaser", "Moving Reese": "phaser",
    "Riddim Saw": "wobble", "Tech Stab Bass": "shaper", "Grind Bass": "shaper", "Talker Bass": "squash", "Screech Bass": "shaper",
    "Hyper Saw Lead": "squash", "Screamer Lead": "delay", "Vox Synth Lead": "delay", "Chip Square Lead": "bitcrush",
    "Concert Grand": "reverb", "Bright Grand": "reverb", "Felt Grand": "reverb", "Upright Keys": "reverb", "Classic Tine": "chorus",
    "Reed Keys": "filterdrive", "Dream Tine": "chorus", "FM Bark Piano": "chorus", "Neuro Arp": "squash",
    "Tape Ghost Keys": "flanger", "Halo Grand": "reverb", "Crystal Organ": "tremolo", "Wide Organ": "tremolo",
    "Bright Poly Wide": "chorus", "Future Chord": "reverb", "Glitch Spray": "delay", "Whoosh": "wobble", "Siren Pan": "tremolo",
    "Wide Riser": "flanger", "Impact Boom": "reverb", "Fold Bass": "squash", "Digital Monster": "wobble", "Neuro Hybrid": "squash",
    "Vox Reese": "formant", "Ring Machine": "delay", "Chop Pad": "reverb", "Resonator Hit": "comb", "Echo Harp": "delay",
}

# MPE / aftertouch mappings (0.8): target names of "pN.mpe.press" / "pN.mpe.timbre"
MPE_T = {"off": 0, "timbre": 1, "level": 2, "lfo": 3, "pan": 4, "cutoff": 5, "position": 6, "index": 7, "density": 8, "drive": 9,
         "vibrato": 10, "formant": 11, "blend": 12}


def mpe(press, press_amt, timbre, timbre_amt, slide=0.06, smooth=0.03, bend=48):
    """Pressure (channel / poly aftertouch, MPE) and timbre (CC 74 / MPE slide) targets with their amounts, per-note bend
    range, slide (per-note bend glide) and smoothing. Marks the preset with the free tag "MPE"."""
    return {"__mpe__": True, "params": {"mpe.press": MPE_T[press], "mpe.pressamt": press_amt, "mpe.timbre": MPE_T[timbre],
                                         "mpe.timbreamt": timbre_amt, "mpe.slide": slide, "mpe.smooth": smooth, "mpe.bend": bend}}


def auto_mpe(name, tags, engine, params):
    """A sensible mapping by role and engine (used where a preset gives none and its role suits expression)."""
    cat = tags[0]
    hybrid = "Hybrid" in tags
    if hybrid:
        second = {"wavetable": "position", "grain": "position", "fm": "index", "vocal": "formant"}.get(engine, "cutoff")
        return mpe("blend", 0.8, second, 0.7, 0.08, 0.04)
    if engine == "grain":
        return mpe("density", 0.7, "position", 0.6, 0.1, 0.06)
    if engine == "vocal":
        return mpe("vibrato", 0.6, "formant", 0.6, 0.08, 0.04)
    if engine == "pluck":
        return mpe("drive", 0.7, "cutoff", 0.7, 0.05, 0.02)
    if engine == "sampler":
        return mpe("level", 0.3, "cutoff", 0.6, 0.06, 0.04)
    if engine == "fm":
        return mpe("index", 0.6, "lfo", 0.5, 0.06, 0.03)
    if engine == "cluster":
        return mpe("cutoff", 0.6, "vibrato", 0.5, 0.08, 0.04)
    if cat == "Bass":
        growl = any(w in name for w in ("Neuro", "Growl", "Grind", "Tear", "Distorted", "Metal", "Screech", "Riddim"))
        if engine == "wavetable":
            return mpe("drive" if growl else "cutoff", 0.7 if growl else 0.6, "position", 0.6, 0.08, 0.03)
        return mpe("drive" if growl else "cutoff", 0.6, "cutoff", 0.5, 0.08, 0.03)
    if cat == "Leads":
        return mpe("vibrato", 0.6, "position" if engine == "wavetable" else "cutoff", 0.7, 0.05, 0.03)
    if engine == "wavetable":
        return mpe("cutoff", 0.5, "position", 0.8, 0.1, 0.06)
    return mpe("cutoff", 0.5, "pan", 0.5, 0.1, 0.06)


# presets without MPE mappings: sub basses (pressure on a sub only muddies it), arps and one-shot effects
NO_MPE = {"Rolling Sub", "Liquid Sub", "Step Sub", "Halftime Sub", "Click Sub", "Deep Pluck Sub", "Roller Bass", "Tech Stab Bass",
          "Warm Roller", "Wide Organ", "Ghost Organ"}


def P(name, tags, engine, comment, *dicts, **kw):
    """0.8 preset: one effect for the FX slot, movement when held, MPE mapping and tag."""
    fxs = [d for d in dicts if isinstance(d, dict) and "__fx__" in d]
    mpes = [d for d in dicts if isinstance(d, dict) and "__mpe__" in d]
    rest = [d for d in dicts if not (isinstance(d, dict) and ("__fx__" in d or "__mpe__" in d))]
    tags = list(tags)
    if fxs:
        want = FX_PICK.get(name)
        order = ([want] if want else []) + FX_ORDER.get(tags[0], [])
        chosen = next((f for k in order for f in fxs if f["__fx__"] == k), fxs[0])
        fxp = dict(chosen["params"])
        if chosen["__fx__"] == "squash" and tags[0] == "Bass":
            fxp["fx.squash.monolow"] = 1  # mono-safe low end under the stack
        rest.append(fxp)
    merged = {}
    for d in rest:
        if "__arp__" not in d:
            merged.update(d)
    # movement: every preset moves when held - a slow LFO where the preset has none
    if merged.get("lfo.depth", 0.0) == 0.0 and not any("lfo.depth" in d for d in rest):
        if tags[0] in ("Keys",) or engine == "sampler":
            rest.append(lfo(0.13, 0.18, "sine", "pan"))
        elif tags[0] == "Bass":
            rest.append(lfo(0.17, 0.12, "tri", "timbre"))
        else:
            rest.append(lfo(0.09, 0.22, "tri", "timbre"))
    if engine == "wavetable" and "wavetable.lfopos" not in merged:
        rest.append({"wavetable.lfopos": 0.12})
    if not mpes and name not in NO_MPE and tags[0] not in ("Arp", "FX"):
        mpes = [auto_mpe(name, tags, engine, merged)]
    if mpes:
        rest.append(mpes[0]["params"])
        if "MPE" not in tags:
            tags.append("MPE")
    return _P(name, tags, engine, comment, *rest, **kw)


def shaper(slot, curve="soft", drv=0.4, bias=0.0, tone=12000, mix=1.0, out=0.0):
    return fx(slot, "shaper", type=curve, drive=drv, bias=bias, tone=tone, mix=mix, output=out)


def squash(slot, depth=0.5, time=0.4, up=0.6, low=0.0, mid=0.0, high=0.0, mix=1.0, out=0.0):
    return fx(slot, "squash", depth=depth, time=time, upward=up, low=low, mid=mid, high=high, mix=mix, output=out)


def width(slot, w=1.3, spread=0.0, haas=12.0, mono=120.0, out=0.0):
    return fx(slot, "width", width=w, spread=spread, haas=haas, mono=mono, output=out)


def wobble(slot, sync="1/8", shape="sine", low=120.0, high=2500.0, reso=0.5, mode="lp", drv=0.3, retrig=1, mix=1.0, rate=2.0):
    return fx(slot, "wobble", rate=rate, sync=sync, shape=shape, low=low, high=high, reso=reso, mode=mode, drive=drv, retrig=retrig, mix=mix)


def comb(slot, freq=300.0, fb=0.6, damp=0.3, rate=0.2, depth=0.3, spread=0.3, mix=0.5):
    return fx(slot, "comb", freq=freq, feedback=fb, damp=damp, rate=rate, depth=depth, spread=spread, mix=mix)


def formant(slot, vowel=0.0, shift=0.0, reso=0.5, rate=1.0, sync="free", depth=0.0, mix=1.0):
    return fx(slot, "formant", vowel=vowel, shift=shift, reso=reso, rate=rate, sync=sync, depth=depth, mix=mix)


def flanger(slot, rate=0.2, depth=0.7, fb=0.5, dly=2.0, mix=0.5):
    return fx(slot, "flanger", rate=rate, depth=depth, feedback=fb, delay=dly, mix=mix)


def tremolo(slot, rate=4.0, sync="free", depth=0.5, mode="trem", shape="sine"):
    return fx(slot, "tremolo", rate=rate, sync=sync, depth=depth, mode=mode, shape=shape)


def dly(slot, time=0.375, fb=0.35, tone=0.5, pingpong=1, mix=0.25):
    return fx(slot, "delay", time=time, feedback=fb, tone=tone, pingpong=pingpong, mix=mix)


def rev(slot, size=0.7, damping=0.45, predelay=0.02, mix=0.3):
    return fx(slot, "reverb", size=size, damping=damping, predelay=predelay, mix=mix)


def cho(slot, rate=0.5, depth=0.5, mix=0.4):
    return fx(slot, "chorus", rate=rate, depth=depth, mix=mix)


def pha(slot, rate=0.3, depth=0.7, fb=0.5, mix=0.5):
    return fx(slot, "phaser", rate=rate, depth=depth, feedback=fb, mix=mix)


def fdrive(slot, cutoff=6000, reso=0.2, drv=0.4, mode="lp", mix=1.0):
    return fx(slot, "filterdrive", cutoff=cutoff, reso=reso, drive=drv, mode=mode, mix=mix)


def layer(engine, level=0.7, tune=0, fine=0.0, xmode="ring", xmod=0.0, w=0.5, env=None):
    """The layer engine: a second engine of the part playing every note (0.8)."""
    d = {"layer.engine": ENGINES[engine] + 1, "layer.level": level, "layer.tune": tune, "layer.fine": fine,
         "layer.xmode": {"ring": 0, "am": 1, "fold": 2}[xmode], "layer.xmod": xmod, "layer.width": w}
    if env is not None:
        d.update({"layer.ownenv": 1, "layer.a": env[0], "layer.d": env[1], "layer.s": env[2], "layer.r": env[3]})
    return d


SAW = 3.0 / 7.0      # Classic table: the saw frame
SQUARE = 4.0 / 7.0   # Classic table: the square frame
WIDE = {"voices": 8}


def B(name, comment, engine, *d, tags=("Bass",), **kw):
    return P(name, list(tags), engine, comment, *d, **kw)


PRESETS_R3 = [
    # ================================================================ REESE / NEURO BASS (14)
    B("Classic Reese", "Two detuned saws beating slowly against each other, a clean sub underneath; mono below 120 Hz.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.52, unison=0.55, univoices=2, uniwidth=0.6, sub=0.35, keytrack=0.3),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.08), cho(1, 0.22, 0.45, 0.25), squash(2, 0.35, 0.5, 0.4),
      width(3, 1.2, 0.0, 12, 120), MONO, glide=0.03),
    B("Phase Reese", "Four-voice saw stack through a slow, deep phaser: the classic sweeping drum & bass reese.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.56, unison=0.45, univoices=4, uniwidth=0.7, sub=0.3, keytrack=0.3),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.06), pha(1, 0.11, 0.85, 0.65, 0.6), squash(2, 0.45, 0.45, 0.5),
      width(3, 1.3, 0.0, 12, 130), MONO, glide=0.03),
    B("Neuro Reese", "Distorted six-voice reese: driven filter, folded harmonics and a hard mono low end - made to be automated.",
      "wavetable", wt(table="classic", pos=0.46, warp=0.22, tone=0.62, unison=0.6, univoices=6, uniwidth=0.8, sub=0.25, keytrack=0.25),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.3, 0.6, 0.2, 0.12), fdrive(1, 3200, 0.3, 0.65, "lp"), shaper(2, "fold", 0.32, 0.0, 9000, 0.55),
      width(3, 1.4, 0.0, 12, 130), MONO, glide=0.02),
    B("Moving Reese", "Reese whose table position and filter breathe with a slow LFO - movement on held notes without automation.",
      "wavetable", wt(table="classic", pos=0.42, tone=0.55, unison=0.5, univoices=4, uniwidth=0.7, sub=0.3, keytrack=0.3, lfopos=0.35),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), lfo(0.21, 0.6, "tri", "timbre"), pha(1, 0.07, 0.7, 0.5, 0.45),
      squash(2, 0.5, 0.45, 0.5), width(3, 1.3, 0.0, 12, 125), MONO, glide=0.03),
    B("Distorted Reese", "Tube-driven reese with a grinding midrange; the squash keeps it dense and upfront.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.5, unison=0.55, univoices=3, sub=0.3, keytrack=0.3),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.4, 0.5, 0.2, 0.1), shaper(2, "tube", 0.55, 0.1, 7000), squash(3, 0.55, 0.4, 0.5, -2, 1, 0),
      cho(1, 0.3, 0.4, 0.3), MONO, glide=0.03),
    B("Hollow Reese", "Reese through a negative-feedback comb that drifts slowly: hollow, metallic and dark.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.58, unison=0.5, univoices=4, uniwidth=0.6, sub=0.35, keytrack=0.3),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), comb(2, 180, -0.7, 0.35, 0.09, 0.45, 0.4, 0.55),
      width(3, 1.25, 0.0, 12, 130), cho(1, 0.18, 0.35, 0.2), MONO, glide=0.03),
    B("Flanged Reese", "Saw stack with a slow jet flanger and chorus: swirling stereo top, solid mono bottom.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.57, unison=0.45, univoices=5, uniwidth=0.75, sub=0.3, keytrack=0.3),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), cho(1, 0.3, 0.5, 0.3), flanger(2, 0.09, 0.8, 0.6, 3.5, 0.55),
      width(3, 1.3, 0.0, 12, 140), MONO, glide=0.03),
    B("Vowel Reese", "Reese that speaks: formant table under an LFO plus a synced vowel filter walking a - o - i.",
      "wavetable", wt(table="formant", pos=0.3, tone=0.62, unison=0.5, univoices=4, uniwidth=0.6, sub=0.4, keytrack=0.3, lfopos=0.4),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), lfo(0.5, 0.7, "tri", "timbre"),
      formant(2, 0.35, 0.0, 0.55, 1.0, "1/2", 0.8, 0.7), squash(3, 0.55, 0.4, 0.6), MONO, glide=0.03),
    B("Sub Reese", "Gentle reese on top of a big sine sub - the sub stays mono and steady, the top moves.",
      "wavetable", wt(table="classic", pos=0.4, tone=0.48, unison=0.5, univoices=2, uniwidth=0.8, sub=0.75, keytrack=0.25),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), cho(1, 0.25, 0.4, 0.25), squash(2, 0.3, 0.5, 0.4, 1, 0, -2),
      width(3, 1.3, 0.0, 12, 160), MONO, glide=0.04),
    B("Tear Reese", "Aggressive neuro reese: FM-edged digital table, heavy multiband squash with lifted highs.",
      "wavetable", wt(table="digital", pos=0.55, tone=0.65, unison=0.55, univoices=5, uniwidth=0.7, fm=0.25, fmratio=2.0, sub=0.3,
                      keytrack=0.25, envfm=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.005, 0.25, 0.4, 0.2, 0.15), fdrive(1, 4500, 0.2, 0.5), squash(2, 0.85, 0.35, 0.8, -3, 1, 3),
      width(3, 1.3, 0.0, 12, 140), MONO, glide=0.02),
    B("Dark Reese", "Low, smoky reese: dark filter, slow chorus, light squash - for liquid and deep tracks.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.44, unison=0.5, univoices=3, sub=0.4, keytrack=0.3),
      amp(0.005, 0.5, 1.0, 0.15), mod(0.01, 0.5, 0.5, 0.2, 0.05), cho(1, 0.15, 0.55, 0.3), squash(2, 0.3, 0.5, 0.5),
      width(3, 1.2, 0.0, 12, 120), MONO, glide=0.04),
    B("Wide Reese", "Eight-voice reese spread across the stereo field, with a Haas spread on top and a mono low end.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.55, unison=0.5, univoices=8, uniwidth=1.0, sub=0.3, keytrack=0.3),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), cho(1, 0.2, 0.4, 0.2), squash(2, 0.45, 0.45, 0.5),
      width(3, 1.6, 0.35, 14, 150), MONO, glide=0.03),
    B("Glide Reese", "Reese with constant-rate portamento: overlap notes and it slides like a fretless rubber band.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.54, unison=0.5, univoices=4, uniwidth=0.6, sub=0.35, keytrack=0.3),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), pha(1, 0.14, 0.7, 0.5, 0.4), squash(2, 0.45, 0.45, 0.5),
      width(3, 1.3, 0.0, 12, 130), MONO, **porta(0.12, "legato", "rate")),
    B("Analog Reese", "Two analog saws detuned wide through the ladder filter, phaser and squash: warm and fat.",
      "analog", an(shape=0.0, detune=0.42, cutoff=1100, reso=0.12, oscmix=0.5, sub=0.35, drive=0.45, keytrack=0.35),
      amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.08), pha(1, 0.12, 0.75, 0.55, 0.45), squash(2, 0.45, 0.45, 0.5),
      width(3, 1.3, 0.0, 12, 130), MONO, glide=0.03),

    # ================================================================ DnB SUBS / ROLLERS (9)
    B("Rolling Sub", "Round sub with a tiny filter blip and gentle saturation for rolling bass lines; mono and steady.",
      "wavetable", wt(table="classic", pos=0.08, tone=0.38, unison=0.0, univoices=1, sub=0.55, keytrack=0.4),
      amp(0.002, 0.4, 0.9, 0.06), mod(0.001, 0.12, 0.0, 0.1, 0.3), fdrive(1, 1800, 0.0, 0.3), squash(2, 0.3, 0.3, 0.3),
      width(3, 0.0, 0.0, 12, 300), MONO, velsens=0.35),
    B("Liquid Sub", "Pure, warm sine sub with a soft second harmonic; slides between overlapping notes.",
      "fm", fm(algo=2, modratio=1.0, index=0.06, op1ratio=1, op2ratio=2, op1level=1.0, op2level=0.5, op3level=0.0, op4level=0.0,
               op5level=0.0, op6level=0.0),
      amp(0.004, 0.6, 0.95, 0.12), mod(0.001, 0.2, 0.3, 0.1, 0.4), shaper(2, "soft", 0.2), width(3, 0.0, 0.0, 12, 300),
      MONO, velsens=0.3, **porta(0.07, "legato")),
    B("Roller Bass", "Punchy square-ish roller: a short filter snap on every note, solid sub, dense with squash.",
      "analog", an(shape=0.42, detune=0.05, cutoff=420, reso=0.25, osc2oct=-1, oscmix=0.35, sub=0.5, drive=0.45, keytrack=0.4),
      amp(0.002, 0.35, 0.75, 0.06), mod(0.001, 0.14, 0.1, 0.1, 0.45), squash(2, 0.5, 0.3, 0.4), width(3, 0.0, 0.0, 12, 300),
      MONO, velsens=0.5),
    B("Tech Stab Bass", "Hard, short tech bass: FM bite, hard clipper and squash for clinical, percussive lines.",
      "fm", fm(algo=2, modratio=1.0, index=0.5, feedback=0.15, op1ratio=1, op2ratio=1, op3ratio=0.5, op1level=1.0, op2level=0.8,
               op3level=0.5, op4level=0.0, op5level=0.0, op6level=0.0),
      amp(0.001, 0.25, 0.4, 0.05), mod(0.001, 0.09, 0.05, 0.08, 0.9), shaper(2, "hard", 0.45, 0.0, 9000), squash(3, 0.55, 0.25, 0.4),
      MONO, velsens=0.5),
    B("Step Sub", "Sub that drops a fifth into every note: a little pitch kick for steppy, half-time lines.",
      "wavetable", wt(table="classic", pos=0.05, tone=0.35, unison=0.0, univoices=1, sub=0.6, keytrack=0.4),
      amp(0.002, 0.6, 0.85, 0.08), mod(0.001, 0.1, 0.0, 0.1, 0.25), pitchenv(7, 0.06, 0.8), shaper(2, "soft", 0.25),
      width(3, 0.0, 0.0, 12, 300), MONO, velsens=0.3),
    B("Deep Pluck Sub", "Plucked sub with a bright attack that fades into a clean sine; tight for fast lines.",
      "analog", an(shape=0.2, detune=0.0, cutoff=260, reso=0.2, oscmix=0.0, sub=0.7, drive=0.3, keytrack=0.5),
      amp(0.002, 0.5, 0.6, 0.07), mod(0.001, 0.08, 0.0, 0.08, 0.6), squash(2, 0.35, 0.3, 0.4), width(3, 0.0, 0.0, 12, 300),
      MONO, velsens=0.5),
    B("Halftime Sub", "Long, heavy sine sub with always-on glide: slow slides for half-time sections.",
      "wavetable", wt(table="classic", pos=0.0, tone=0.3, unison=0.0, univoices=1, sub=0.7, keytrack=0.3),
      amp(0.003, 1.5, 0.9, 0.25), mod(0.001, 0.2, 0.2, 0.1, 0.1), shaper(2, "tube", 0.3, 0.05, 4000), width(3, 0.0, 0.0, 12, 300),
      MONO, velsens=0.25, **porta(0.14, "always")),
    B("Warm Roller", "Ladder-filtered saw with sub for warm, round rolling basslines in liquid tracks.",
      "analog", an(shape=0.1, detune=0.06, cutoff=520, reso=0.18, oscmix=0.3, sub=0.5, drive=0.35, keytrack=0.4),
      amp(0.002, 0.4, 0.85, 0.08), mod(0.001, 0.2, 0.2, 0.1, 0.35), cho(1, 0.3, 0.3, 0.15), squash(2, 0.3, 0.4, 0.4),
      width(3, 1.0, 0.0, 12, 200), MONO, velsens=0.4, glide=0.02),
    B("Click Sub", "Sine sub with a sharp click on top, so the bass line reads on small speakers.",
      "analog", an(shape=0.5, detune=0.0, cutoff=320, reso=0.05, oscmix=0.0, sub=1.0, noise=0.25, drive=0.35, keytrack=1.0, osclevel=0.15),
      amp(0.001, 0.8, 0.8, 0.08), mod(0.001, 0.012, 0.0, 0.02, 0.9), squash(2, 0.3, 0.2, 0.3), width(3, 0.0, 0.0, 12, 300),
      MONO, velsens=0.35),

    # ================================================================ DUBSTEP / GROWL / WOBBLE (12)
    B("Wub Machine", "Saw stack through a tempo-synced eighth-note wobble filter that restarts with every note.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.75, unison=0.4, univoices=5, uniwidth=0.6, sub=0.4, keytrack=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.4, 0.5, 0.2, 0.0), cho(1, 0.3, 0.4, 0.2),
      wobble(2, "1/8", "sine", 110, 3200, 0.55, "lp", 0.35), squash(3, 0.6, 0.35, 0.5), MONO),
    B("Triplet Wobble", "Eighth-triplet wobble on a driven saw stack: the bouncing half-time classic.",
      "wavetable", wt(table="classic", pos=0.45, tone=0.75, unison=0.4, univoices=4, uniwidth=0.6, sub=0.4, keytrack=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.4, 0.5, 0.2, 0.0), fdrive(1, 9000, 0.1, 0.4),
      wobble(2, "1/8T", "sine", 120, 3500, 0.5, "lp", 0.3), squash(3, 0.6, 0.35, 0.5), MONO),
    B("Square Wobble", "Hard sixteenth on / off wobble with a square LFO - choppy and rhythmic.",
      "wavetable", wt(table="classic", pos=SQUARE, tone=0.75, unison=0.35, univoices=3, sub=0.4, keytrack=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.4, 0.5, 0.2, 0.0), wobble(2, "1/16", "square", 140, 2600, 0.45, "lp", 0.4),
      squash(3, 0.6, 0.3, 0.5), MONO),
    B("Riddim Saw", "Hard-clipped saw with a quarter-note band-pass wobble: sparse, heavy and minimal.",
      "wavetable", wt(table="classic", pos=SAW, tone=0.8, unison=0.25, univoices=3, sub=0.5, keytrack=0.2),
      amp(0.002, 0.4, 1.0, 0.08), mod(0.01, 0.4, 0.5, 0.2, 0.0), shaper(1 + 1, "hard", 0.55, 0.0, 10000),
      wobble(3, "1/4", "tri", 160, 2400, 0.6, "bp", 0.3), MONO),
    B("Growl Machine", "Formant table with FM and an LFO on the position, a synced vowel filter and heavy squash: a talking growl.",
      "wavetable", wt(table="formant", pos=0.4, tone=0.7, unison=0.3, univoices=3, fm=0.3, fmratio=1.0, envfm=0.3, sub=0.35,
                      keytrack=0.2, lfopos=0.5),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.4, 0.2, 0.1), lfo(3.0, 0.8, "tri", "timbre"),
      formant(2, 0.2, 0.0, 0.6, 1.0, "1/8", 0.9, 0.75), squash(3, 0.8, 0.35, 0.7, -2, 2, 2), MONO),
    B("Vowel Growl", "Quarter-note vowel sweep (a - e - i - o - u) over a folded saw: the classic 'talking' bass.",
      "wavetable", wt(table="classic", pos=0.5, tone=0.8, unison=0.3, univoices=3, sub=0.4, keytrack=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.4, 0.2, 0.0), shaper(2, "fold", 0.4, 0.0, 9000, 0.7),
      formant(3, 0.0, 0.0, 0.65, 1.0, "1/4", 1.0, 0.85), MONO),
    B("Metal Growl", "Clanging metallic growl: FM table, a resonant comb on top and hard squash.",
      "wavetable", wt(table="grit", pos=0.35, tone=0.7, unison=0.3, univoices=3, fm=0.35, fmratio=1.5, sub=0.35, keytrack=0.2,
                      lfopos=0.3),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.4, 0.2, 0.05), lfo(2.2, 0.6, "sine", "timbre"),
      comb(2, 420, 0.75, 0.25, 0.6, 0.5, 0.3, 0.5), squash(3, 0.75, 0.35, 0.6), MONO),
    B("Screech Bass", "Screaming high-ratio FM with a folded top: for the high, aggressive bass answers.",
      "wavetable", wt(table="classic", pos=0.2, tone=0.75, unison=0.3, univoices=3, fm=0.55, fmratio=3.0, envfm=0.25, sub=0.3,
                      keytrack=0.3, lfopos=0.25),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.005, 0.3, 0.5, 0.2, 0.0), lfo(4.5, 0.5, "tri", "timbre"),
      shaper(2, "fold", 0.35, 0.0, 8000, 0.6), squash(3, 0.7, 0.3, 0.6, -3, 0, 2), MONO, tune=12),
    B("Laser Wobble", "Rising saw LFO on a band-pass wobble at sixteenth notes: zapping, laser-like movement.",
      "wavetable", wt(table="digital", pos=0.45, tone=0.8, unison=0.35, univoices=4, uniwidth=0.6, sub=0.4, keytrack=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.5, 0.2, 0.0), wobble(2, "1/16", "sawup", 200, 4500, 0.65, "bp", 0.3),
      squash(3, 0.65, 0.3, 0.6), MONO),
    B("Grind Bass", "Grinding, folded Grit table squashed hard: a constant, dense wall of mid-range.",
      "wavetable", wt(table="grit", pos=0.5, warp=0.2, tone=0.62, unison=0.4, univoices=4, uniwidth=0.6, sub=0.35, keytrack=0.25,
                      lfopos=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.5, 0.2, 0.05), lfo(0.8, 0.5, "tri", "timbre"),
      shaper(2, "fold", 0.45, 0.05, 7000, 0.7), squash(3, 0.85, 0.35, 0.7, -2, 1, 1), MONO),
    B("Talker Bass", "Formant table walked by a fast LFO and an envelope: each note says something different.",
      "wavetable", wt(table="formant", pos=0.1, tone=0.7, unison=0.25, univoices=3, sub=0.4, keytrack=0.25, envpos=0.5, lfopos=0.3),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.005, 0.45, 0.2, 0.2, 0.0), lfo(5.0, 0.4, "sine", "timbre"),
      shaper(2, "tube", 0.4, 0.05, 9000), squash(3, 0.65, 0.35, 0.6), MONO, glide=0.03),
    B("Half-Step Wobble", "Dotted-quarter wobble that runs across the bar line - heavy and lopsided.",
      "wavetable", wt(table="classic", pos=0.5, tone=0.78, unison=0.4, univoices=5, uniwidth=0.6, sub=0.45, keytrack=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.4, 0.5, 0.2, 0.0), cho(1, 0.25, 0.35, 0.2),
      wobble(2, "1/4.", "sine", 100, 2800, 0.55, "lp", 0.4), squash(3, 0.6, 0.35, 0.5), MONO),

    # ================================================================ LEADS (12)
    P("Super Wide Lead", ["Leads"], "wavetable", "Eight-voice saw stack spread across the stereo field, chorus, ping-pong delay and a big hall.",
      wt(table="classic", pos=SAW, tone=0.78, unison=0.45, univoices=8, uniwidth=1.0, keytrack=0.5),
      amp(0.005, 0.6, 0.85, 0.35), mod(0.01, 0.5, 0.5, 0.3, 0.15), cho(1, 0.4, 0.4, 0.25), dly(2, 0.36, 0.35, 0.45, 1, 0.22),
      rev(3, 0.75, 0.45, 0.02, 0.25), voices=6, glide=0.0),
    P("Hyper Saw Lead", ["Leads"], "cluster", "Huge detuned saw stack squashed upfront, with a stereo delay: festival main-room energy.",
      cl(detune=0.35, spread=0.9, tone=0.82, chord="unison", stack=7, shape=0.0, side=0.8, reso=0.1, keytrack=0.5, drift=0.15),
      amp(0.004, 0.5, 0.9, 0.3), mod(0.01, 0.4, 0.5, 0.3, 0.1), squash(2, 0.55, 0.4, 0.5, -3, 0, 2), dly(3, 0.33, 0.3, 0.5, 1, 0.2),
      voices=6),
    P("Mono Glide Lead", ["Leads"], "wavetable", "Expressive mono lead with legato glide, light drive and a dotted ping-pong delay.",
      wt(table="classic", pos=0.5, tone=0.7, unison=0.3, univoices=3, keytrack=0.5, envpos=0.1),
      amp(0.005, 0.5, 0.9, 0.25), mod(0.01, 0.3, 0.6, 0.2, 0.15), lfo(5.2, 0.05, "sine", "pitch"), fdrive(1, 7000, 0.2, 0.3),
      dly(2, 0.45, 0.4, 0.45, 1, 0.25), rev(3, 0.6, 0.5, 0.02, 0.18), MONO, **porta(0.08, "legato")),
    P("Shimmer Lead", ["Leads", "Keys"], "fm", "Glassy FM lead with a bell edge, chorus and a long bright hall.",
      fm(algo=2, modratio=1.0, index=0.3, op1ratio=1, op2ratio=3, op3ratio=2, op4ratio=4, op1level=1.0, op2level=0.6, op3level=0.5,
         op4level=0.3, op5level=0.0, op6level=0.0, velidx=0.4),
      amp(0.004, 0.8, 0.7, 0.4), mod(0.005, 0.6, 0.4, 0.3, 0.6), cho(1, 0.5, 0.5, 0.35), dly(2, 0.375, 0.3, 0.6, 1, 0.18),
      rev(3, 0.85, 0.3, 0.03, 0.3), voices=6),
    P("Screamer Lead", ["Leads"], "analog", "Resonant, overdriven saw lead that screams on high notes; ping-pong delay keeps it wide.",
      an(shape=0.05, detune=0.12, cutoff=2600, reso=0.55, oscmix=0.5, sub=0.0, drive=0.7, keytrack=0.8),
      amp(0.004, 0.5, 0.9, 0.2), mod(0.01, 0.35, 0.5, 0.2, 0.3), shaper(2, "tube", 0.35, 0.05, 9000), dly(3, 0.3, 0.35, 0.4, 1, 0.22),
      MONO, glide=0.03),
    P("Twist Lead", ["Leads"], "wavetable", "Twisted, sync-like lead: the envelope bends the warp, the stack spreads it wide.",
      wt(table="classic", pos=0.5, warp=0.35, warpmode=1, tone=0.75, unison=0.3, univoices=5, uniwidth=0.8, keytrack=0.5, envpos=0.15),
      amp(0.004, 0.5, 0.85, 0.25), mod(0.005, 0.4, 0.3, 0.2, 0.4), lfo(5.5, 0.04, "sine", "pitch"), dly(2, 0.36, 0.35, 0.45, 1, 0.2),
      rev(3, 0.6, 0.5, 0.02, 0.18), voices=4),
    P("Vox Synth Lead", ["Leads", "Vocal"], "vocal", "Sung synth lead: an 'ah' with vibrato, widened by a Haas spread, with delay.",
      vo(vowel=0.0, formant=0.0, breath=0.2, tension=0.6, vibdepth=0.25, vibrate=5.3, jitter=0.1),
      amp(0.02, 0.6, 0.9, 0.3), mod(0.01, 0.4, 0.5, 0.3, 0.2), width(2, 1.3, 0.35, 11, 150), dly(3, 0.4, 0.35, 0.45, 1, 0.22),
      voices=4, glide=0.02),
    P("Chip Square Lead", ["Leads"], "wavetable", "Narrow pulse lead with a crunchy bit crusher and a quick vibrato - retro, but wide.",
      wt(table="classic", pos=0.86, tone=0.85, unison=0.15, univoices=2, uniwidth=0.6, keytrack=0.5),
      amp(0.003, 0.3, 0.85, 0.12), mod(0.01, 0.3, 0.5, 0.2, 0.0), lfo(6.0, 0.06, "tri", "pitch"), crush(10, 2, 0.4),
      dly(2, 0.25, 0.3, 0.6, 1, 0.2), MONO, glide=0.02),
    P("Dream Lead", ["Leads", "Pads"], "wavetable", "Soft triangle lead with a slow flanger and a long, airy reverb.",
      wt(table="classic", pos=0.16, tone=0.7, unison=0.3, univoices=4, uniwidth=0.8, keytrack=0.5),
      amp(0.03, 0.8, 0.85, 0.6), mod(0.01, 0.5, 0.5, 0.4, 0.1), lfo(4.8, 0.03, "sine", "pitch"), flanger(2, 0.08, 0.6, 0.4, 2.5, 0.4),
      rev(3, 0.9, 0.4, 0.04, 0.38), voices=4),
    P("Growl Lead", ["Leads"], "wavetable", "Biting FM-formant lead that growls with an LFO: a bass-music lead for the drop's answer phrase.",
      wt(table="formant", pos=0.4, tone=0.8, unison=0.3, univoices=3, fm=0.35, fmratio=2.0, keytrack=0.4, lfopos=0.4),
      amp(0.003, 0.5, 0.9, 0.15), mod(0.01, 0.3, 0.5, 0.2, 0.0), lfo(6.0, 0.6, "tri", "timbre"), squash(2, 0.6, 0.3, 0.5),
      dly(3, 0.3, 0.3, 0.45, 1, 0.18), MONO, glide=0.02),
    P("Stereo Saw Lead", ["Leads"], "analog", "Classic two-saw lead made wide: chorus, Haas spread and a short room.",
      an(shape=0.0, detune=0.18, cutoff=3500, reso=0.15, oscmix=0.5, drive=0.3, keytrack=0.7),
      amp(0.004, 0.5, 0.85, 0.22), mod(0.01, 0.35, 0.5, 0.2, 0.2), cho(1, 0.6, 0.5, 0.35), width(2, 1.4, 0.4, 13, 120),
      rev(3, 0.5, 0.5, 0.01, 0.18), voices=4),
    P("Pluck Lead Wide", ["Leads", "Plucks"], "wavetable", "Snappy plucked lead: a quick filter snap, stereo stack and a dotted delay.",
      wt(table="classic", pos=0.45, tone=0.55, unison=0.35, univoices=6, uniwidth=0.9, keytrack=0.6),
      amp(0.002, 0.45, 0.25, 0.3), mod(0.001, 0.18, 0.0, 0.2, 0.55), dly(2, 0.45, 0.4, 0.5, 1, 0.28), rev(3, 0.65, 0.5, 0.02, 0.22),
      voices=6, velsens=0.5),

    # ================================================================ PIANO / ELECTRIC PIANO (10)
    P("Concert Grand", ["Keys", "Piano"], "sampler", "Grand piano recordings, two velocity layers, a touch of hall.",
      sm(tone=1.0, inst2=KEYS["grand"]), amp(0.001, 8.0, 0.0, 0.45), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      rev(3, 0.6, 0.55, 0.015, 0.16), voices=16, velsens=0.75),
    P("Bright Grand", ["Keys", "Piano"], "sampler", "Grand piano opened up for pop: brighter, lightly squashed, short room.",
      sm(tone=1.0, inst2=KEYS["grand"]), amp(0.001, 8.0, 0.0, 0.4), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      squash(2, 0.3, 0.35, 0.4, -1, 0, 3), rev(3, 0.45, 0.5, 0.01, 0.14), voices=16, velsens=0.7),
    P("Felt Grand", ["Keys", "Piano"], "sampler", "Dark, soft grand: the top rolled off, a slow release and a warm hall - intimate.",
      sm(tone=0.55, inst2=KEYS["grand"]), amp(0.003, 8.0, 0.0, 0.8), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      rev(3, 0.75, 0.65, 0.02, 0.24), voices=16, velsens=0.6),
    P("Upright Keys", ["Keys", "Piano"], "sampler", "Upright piano recordings with a little room: honest and close.",
      sm(tone=0.95, inst2=KEYS["upright"]), amp(0.001, 6.0, 0.0, 0.35), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      rev(3, 0.4, 0.5, 0.008, 0.14), voices=16, velsens=0.7),
    P("Classic Tine", ["Keys", "Piano"], "sampler", "Modelled tine electric piano: bell-like when soft, barking when hit hard.",
      sm(tone=1.0, inst2=KEYS["tine"]), amp(0.001, 6.0, 0.0, 0.3), mod(0.001, 0.3, 0.0, 0.2, 0.0), cho(1, 0.35, 0.3, 0.15),
      voices=16, velsens=0.65),
    P("Suitcase Tine", ["Keys", "Piano"], "sampler", "Tine piano with the stereo autopan of a suitcase amp and a small room.",
      sm(tone=0.95, inst2=KEYS["tine"]), amp(0.001, 6.0, 0.0, 0.35), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      tremolo(2, 4.6, "free", 0.65, "pan", "sine"), rev(3, 0.45, 0.55, 0.01, 0.16), voices=16, velsens=0.65),
    P("Reed Keys", ["Keys", "Piano"], "sampler", "Modelled reed electric piano: nasal and woody, with a gritty bark when driven.",
      sm(tone=1.0, inst2=KEYS["reed"]), amp(0.001, 5.0, 0.0, 0.25), mod(0.001, 0.3, 0.0, 0.2, 0.0), fdrive(1, 8000, 0.1, 0.25),
      voices=16, velsens=0.7),
    P("Reed Tremolo", ["Keys", "Piano"], "sampler", "Reed piano through its classic amplitude tremolo - soul and gospel.",
      sm(tone=1.0, inst2=KEYS["reed"]), amp(0.001, 5.0, 0.0, 0.3), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      tremolo(2, 5.5, "free", 0.45, "trem", "sine"), rev(3, 0.4, 0.5, 0.01, 0.12), voices=16, velsens=0.7),
    P("Dream Tine", ["Keys", "Piano", "Pads"], "sampler", "Tine piano drenched in chorus, dotted delay and a long hall.",
      sm(tone=0.9, inst2=KEYS["tine"]), amp(0.001, 6.0, 0.0, 0.8), mod(0.001, 0.3, 0.0, 0.2, 0.0), cho(1, 0.4, 0.6, 0.4),
      dly(2, 0.45, 0.4, 0.4, 1, 0.22), rev(3, 0.85, 0.45, 0.03, 0.3), voices=16, velsens=0.6),
    P("FM Bark Piano", ["Keys", "Piano"], "fm", "Two-pair FM electric piano: the tine pair rings, the bark pair bites with velocity.",
      fm(algo=2, modratio=1.0, index=0.32, op1ratio=1, op2ratio=14, op3ratio=1, op4ratio=1, op1level=1.0, op2level=0.35,
         op3level=0.7, op4level=0.6, op5level=0.0, op6level=0.0, velidx=0.9),
      amp(0.001, 3.5, 0.0, 0.35), mod(0.001, 0.6, 0.1, 0.3, 0.75), cho(1, 0.4, 0.35, 0.2), rev(3, 0.45, 0.5, 0.01, 0.12),
      voices=16, velsens=0.6),

    # ================================================================ GRANULAR (9)
    P("Glass Swarm", ["Textures", "Pads"], "grain", "Dense cloud of singing-glass grains, eight voices deep, chorused into a huge hall.",
      gr(inst="glass", pos=0.35, size=240, density=80, spray=0.45, pitch=0, reverse=0.2, spread=1.0, scan=0.05, tone=0.85),
      amp(0.4, 1.0, 0.9, 2.5), mod(0.01, 1.0, 0.5, 1.0, 0.0), cho(1, 0.3, 0.5, 0.35), width(2, 1.4, 0.0, 12, 120),
      rev(3, 0.9, 0.4, 0.04, 0.4), voices=8),
    P("Metal Choir", ["Textures", "Pads"], "grain", "Bowed-metal grains stacked into a choir; slow scan, shimmering stereo.",
      gr(inst="bowedmetal", pos=0.4, size=320, density=70, spray=0.35, reverse=0.15, spread=1.0, scan=0.04, tone=0.8),
      amp(0.6, 1.0, 0.9, 3.0), mod(0.01, 1.0, 0.5, 1.0, 0.0), flanger(2, 0.05, 0.4, 0.3, 4.0, 0.25),
      rev(3, 0.92, 0.45, 0.04, 0.42), voices=8),
    P("Harp Cloud", ["Textures", "Pads"], "grain", "Harp recordings scattered into a dense cloud with ping-pong echoes.",
      gr(inst="harp", pos=0.15, size=180, density=65, spray=0.5, reverse=0.25, spread=1.0, scan=0.08, tone=0.8),
      amp(0.25, 1.0, 0.85, 2.5), mod(0.01, 1.0, 0.5, 1.0, 0.0), dly(2, 0.42, 0.45, 0.4, 1, 0.28), rev(3, 0.85, 0.45, 0.03, 0.35),
      voices=8),
    P("Kalimba Storm", ["Textures"], "grain", "Kalimba grains at full density with reversed fragments - a rattling, tuned storm.",
      gr(inst="kalimba", pos=0.1, size=90, density=95, spray=0.7, reverse=0.4, spread=1.0, scan=0.15, tone=0.75),
      amp(0.1, 1.0, 0.85, 2.0), mod(0.01, 1.0, 0.5, 1.0, 0.0), cho(1, 0.6, 0.4, 0.3), rev(3, 0.8, 0.5, 0.02, 0.3), voices=8),
    P("Piano Grains", ["Pads", "Textures", "Piano"], "grain", "Grand piano recordings frozen into a slow-moving grain cloud: a piano that breathes.",
      gr(pos=0.12, size=300, density=48, spray=0.25, reverse=0.1, spread=0.9, scan=0.03, tone=0.8, inst2=KEYS["grand"]),
      amp(0.5, 1.0, 0.9, 3.0), mod(0.01, 1.0, 0.5, 1.0, 0.0), cho(1, 0.25, 0.4, 0.25), rev(3, 0.9, 0.4, 0.04, 0.4), voices=8),
    P("Tine Bloom", ["Pads", "Textures", "Piano"], "grain", "Tine-piano grains blooming slowly, chorused, with a dotted echo.",
      gr(pos=0.08, size=260, density=55, spray=0.3, reverse=0.2, spread=1.0, scan=0.05, tone=0.85, inst2=KEYS["tine"]),
      amp(0.7, 1.0, 0.9, 3.0), mod(0.01, 1.0, 0.5, 1.0, 0.0), cho(1, 0.3, 0.5, 0.35), dly(2, 0.45, 0.35, 0.4, 1, 0.2),
      rev(3, 0.88, 0.45, 0.03, 0.35), voices=8),
    P("Frozen Vibes", ["Pads", "Textures"], "grain", "Vibraphone grains held still, with a slow flanger: a frozen, shimmering chord.",
      gr(inst="vibraphone", pos=0.2, size=400, density=40, spray=0.15, spread=1.0, scan=0.0, tone=0.85),
      amp(0.3, 1.0, 0.9, 2.5), mod(0.01, 1.0, 0.5, 1.0, 0.0), flanger(2, 0.06, 0.5, 0.35, 3.0, 0.3), rev(3, 0.9, 0.4, 0.03, 0.38),
      voices=8),
    P("Dust Orchestra", ["Textures"], "grain", "Zither grains thrown backwards and forwards, a stereo delay trail - an orchestra of dust.",
      gr(inst="zither", pos=0.25, size=150, density=60, spray=0.6, reverse=0.5, spread=1.0, scan=-0.06, tone=0.7),
      amp(0.3, 1.0, 0.85, 2.5), mod(0.01, 1.0, 0.5, 1.0, 0.0), dly(2, 0.5, 0.5, 0.35, 1, 0.3), rev(3, 0.85, 0.55, 0.03, 0.35),
      voices=8),
    P("Breath Grains", ["Pads", "Textures"], "grain", "Recorder breath sliced into a soft, wide grain pad.",
      gr(inst="recorder", pos=0.3, size=280, density=60, spray=0.35, reverse=0.1, spread=1.0, scan=0.03, tone=0.75),
      amp(0.5, 1.0, 0.9, 2.5), mod(0.01, 1.0, 0.5, 1.0, 0.0), width(2, 1.5, 0.4, 15, 150), rev(3, 0.88, 0.45, 0.03, 0.35),
      voices=8),

    # ================================================================ ARPS (9)
    P("Trance Gate Arp", ["Arp", "Leads"], "cluster", "Supersaw stack on a sixteenth arp over two octaves, ping-pong delay and hall.",
      cl(detune=0.3, spread=0.9, tone=0.75, chord="unison", stack=7, shape=0.0, side=0.7, reso=0.1, keytrack=0.5, drift=0.1),
      amp(0.003, 0.18, 0.3, 0.12), mod(0.001, 0.12, 0.1, 0.1, 0.3), dly(2, 0.375, 0.35, 0.5, 1, 0.2), rev(3, 0.75, 0.45, 0.02, 0.22),
      arp("up", 2, 0.45, "1/16"), voices=8),
    P("Wide Pluck Arp", ["Arp", "Plucks"], "wavetable", "Six-voice stereo pluck, up / down over three octaves, dotted ping-pong delay.",
      wt(table="classic", pos=0.45, tone=0.55, unison=0.35, univoices=6, uniwidth=1.0, keytrack=0.6),
      amp(0.002, 0.3, 0.0, 0.25), mod(0.001, 0.12, 0.0, 0.1, 0.5), dly(2, 0.45, 0.45, 0.5, 1, 0.25), rev(3, 0.7, 0.5, 0.02, 0.2),
      arp("updown", 3, 0.5, "1/16"), voices=8),
    P("Bell Cascade Arp", ["Arp", "Keys"], "fm", "FM bells cascading down three octaves in triplets, with echoes and a bright hall.",
      fm(algo=2, modratio=1.0, index=0.35, op1ratio=1, op2ratio=3.5, op3ratio=1, op4ratio=7, op1level=1.0, op2level=0.5,
         op3level=0.5, op4level=0.3, op5level=0.0, op6level=0.0, velidx=0.5),
      amp(0.001, 1.2, 0.0, 0.8), mod(0.001, 0.5, 0.0, 0.4, 0.7), dly(2, 0.4, 0.4, 0.6, 1, 0.22), rev(3, 0.85, 0.35, 0.03, 0.3),
      arp("down", 3, 0.6, "1/16T"), voices=10),
    P("Neuro Arp", ["Arp", "Bass"], "wavetable", "Growling FM sixteenths in one octave: a rolling neuro sequence from a single held note.",
      wt(table="formant", pos=0.35, tone=0.68, unison=0.25, univoices=3, fm=0.3, fmratio=1.0, envfm=0.3, sub=0.3, keytrack=0.3, envpos=0.3),
      amp(0.002, 0.2, 0.4, 0.06), mod(0.001, 0.12, 0.1, 0.1, 0.3), shaper(2, "tube", 0.35, 0.05, 8000), squash(3, 0.6, 0.3, 0.5),
      arp("played", 1, 0.55, "1/16"), MONO),
    P("Mallet Echo Arp", ["Arp", "Plucks"], "sampler", "Glockenspiel arpeggio with long dotted echoes - sparkling and wide.",
      sm(inst="glockenspiel", tone=0.85), amp(0.001, 1.5, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0), width(2, 1.3, 0.3, 12, 150),
      dly(3, 0.45, 0.45, 0.5, 1, 0.3), arp("up", 3, 0.4, "1/8"), voices=10),
    P("Dream Tine Arp", ["Arp", "Keys"], "sampler", "Tine piano arpeggio in eighths with chorus, delay and hall: dreamy and full.",
      sm(tone=0.9, inst2=KEYS["tine"]), amp(0.001, 3.0, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0), cho(1, 0.35, 0.5, 0.35),
      dly(2, 0.45, 0.4, 0.45, 1, 0.24), rev(3, 0.85, 0.45, 0.03, 0.28), arp("updown", 2, 0.6, "1/8"), voices=12),
    P("Ripple Arp", ["Arp"], "analog", "Analog saw arp in sixteenth triplets with a sweeping phaser and stereo echo.",
      an(shape=0.1, detune=0.12, cutoff=1400, reso=0.3, oscmix=0.5, drive=0.3, keytrack=0.6),
      amp(0.002, 0.2, 0.2, 0.12), mod(0.001, 0.15, 0.0, 0.1, 0.45), pha(1, 0.15, 0.7, 0.5, 0.45), dly(2, 0.3, 0.35, 0.45, 1, 0.2),
      rev(3, 0.6, 0.5, 0.02, 0.15), arp("up", 2, 0.5, "1/16T"), voices=8),
    P("Chord Stab Arp", ["Arp", "Keys"], "cluster", "Rhythmic chord stabs (chord mode, eighths) from a wide saw stack - instant house groove.",
      cl(detune=0.25, spread=0.8, tone=0.6, chord="min7", stack=3, shape=0.0, side=0.6, reso=0.15, keytrack=0.5, drift=0.1),
      amp(0.002, 0.25, 0.0, 0.15), mod(0.001, 0.15, 0.0, 0.1, 0.45), squash(2, 0.4, 0.35, 0.4), rev(3, 0.6, 0.5, 0.02, 0.2),
      arp("chord", 1, 0.35, "1/8"), voices=8),
    P("Random Spark Arp", ["Arp", "FX"], "fm", "Random thirty-second FM blips over four octaves through a ping-pong delay: sparks.",
      fm(algo=3, modratio=2.0, index=0.4, op1ratio=1, op2ratio=3, op3ratio=2, op4ratio=5, op1level=1.0, op2level=0.6,
         op3level=0.5, op4level=0.4, op5level=0.0, op6level=0.0),
      amp(0.001, 0.12, 0.0, 0.08), mod(0.001, 0.06, 0.0, 0.05, 0.8), dly(2, 0.25, 0.5, 0.6, 1, 0.3), rev(3, 0.7, 0.4, 0.02, 0.2),
      arp("random", 4, 0.3, "1/32"), voices=8),

    # ================================================================ ATMOSPHERES / EVOLVING PADS / TEXTURES (14)
    P("Evolving Tide", ["Pads"], "wavetable", "Harmonic table drifting through its frames on a very slow LFO, eight-voice stack, flanger and hall.",
      wt(table="harmonic", pos=0.4, tone=0.7, unison=0.4, univoices=6, uniwidth=1.0, keytrack=0.4, lfopos=0.45),
      amp(1.2, 2.0, 0.9, 3.5), mod(1.5, 3.0, 0.6, 3.0, 0.2), lfo(0.06, 0.9, "tri", "timbre"), cho(1, 0.2, 0.5, 0.3),
      flanger(2, 0.04, 0.6, 0.4, 4.0, 0.3), rev(3, 0.92, 0.4, 0.04, 0.42), voices=8),
    P("Aurora Scan", ["Pads", "Textures"], "wavetable", "Glass table scanned slowly, circling in stereo with a two-bar autopan.",
      wt(table="glass", pos=0.3, tone=0.75, unison=0.35, univoices=5, uniwidth=1.0, keytrack=0.4, lfopos=0.5, envpos=0.2),
      amp(1.5, 2.0, 0.85, 4.0), mod(2.0, 4.0, 0.5, 3.0, 0.2), lfo(0.05, 0.8, "sine", "timbre"), tremolo(2, 0.25, "2/1", 0.55, "pan", "sine"),
      rev(3, 0.94, 0.35, 0.05, 0.45), voices=8),
    P("Deep Space Drone", ["Textures", "Pads"], "cluster", "Fourths stacked low, a drifting comb on top and an endless hall.",
      cl(detune=0.3, spread=1.0, tone=0.45, chord="fourths", stack=5, shape=0.1, side=0.7, reso=0.25, keytrack=0.3, drift=0.6),
      amp(2.0, 3.0, 0.9, 5.0), mod(2.0, 4.0, 0.5, 4.0, 0.2), lfo(0.04, 0.5, "sine", "timbre"), comb(2, 110, 0.7, 0.5, 0.03, 0.6, 0.6, 0.35),
      rev(3, 0.96, 0.45, 0.06, 0.48), voices=6),
    P("Breathing Machine", ["Textures", "Vocal"], "vocal", "Breathy vocal drone whose vowel opens and closes over two bars.",
      vo(vowel=1.0, formant=-3.0, breath=0.55, tension=0.4, vibdepth=0.05, vibrate=4.0, jitter=0.3),
      amp(1.0, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.6, 2.0, 0.2), formant(2, 0.3, 0.0, 0.6, 0.2, "2/1", 0.9, 0.7),
      rev(3, 0.92, 0.45, 0.05, 0.42), voices=6),
    P("Slow Motion Choir", ["Pads", "Vocal"], "vocal", "Ooh choir that swells in slowly, chorused and wide, in a long cathedral hall.",
      vo(vowel=3.6, formant=0.0, breath=0.25, tension=0.45, vibdepth=0.12, vibrate=4.6, jitter=0.25),
      amp(1.4, 2.0, 0.9, 3.5), mod(1.0, 2.0, 0.6, 2.0, 0.1), cho(1, 0.2, 0.6, 0.4), width(2, 1.5, 0.3, 14, 150),
      rev(3, 0.95, 0.4, 0.05, 0.45), voices=8),
    P("Dark Matter", ["Textures", "Pads"], "wavetable", "Low Grit table with a crawling phaser and a slowly moving position - ominous.",
      wt(table="grit", pos=0.2, tone=0.45, unison=0.4, univoices=5, uniwidth=0.9, keytrack=0.3, lfopos=0.4),
      amp(1.5, 2.0, 0.9, 4.0), mod(2.0, 3.0, 0.5, 3.0, 0.15), lfo(0.03, 0.9, "tri", "timbre"), pha(1, 0.05, 0.9, 0.6, 0.5),
      rev(3, 0.92, 0.55, 0.05, 0.4), voices=6),
    P("Glacier", ["Textures"], "grain", "Glass grains crawling at a glacial scan rate, very wide, endless reverb.",
      gr(inst="glass", pos=0.5, size=450, density=50, spray=0.3, reverse=0.3, spread=1.0, scan=0.01, tone=0.7, pitch=-12),
      amp(2.0, 2.0, 0.9, 5.0), mod(0.01, 1.0, 0.5, 1.0, 0.0), width(2, 1.6, 0.3, 18, 150), rev(3, 0.96, 0.45, 0.06, 0.5), voices=8),
    P("Tidal Pad", ["Pads"], "analog", "Warm saw pad whose filter rolls in and out over two bars like a tide.",
      an(shape=0.05, detune=0.25, cutoff=2400, reso=0.15, oscmix=0.5, drive=0.2, keytrack=0.5),
      amp(0.8, 2.0, 0.9, 3.0), mod(0.5, 2.0, 0.6, 2.0, 0.1), cho(1, 0.3, 0.6, 0.4), wobble(2, "2/1", "sine", 300, 4500, 0.25, "lp", 0.1),
      rev(3, 0.88, 0.45, 0.04, 0.35), voices=8),
    P("Starfield", ["Textures", "Keys"], "fm", "Random high FM bells drifting across the stereo field into delay and hall.",
      fm(algo=5, modratio=1.0, index=0.3, op1ratio=1, op2ratio=2.76, op3ratio=5.4, op4ratio=8.9, op1level=1.0, op2level=0.6,
         op3level=0.4, op4level=0.3, op5level=0.2, op6level=0.3),
      amp(0.005, 2.5, 0.0, 2.5), mod(0.001, 1.0, 0.0, 1.0, 0.5), lfo(0.2, 0.7, "sh", "pan"), dly(2, 0.5, 0.5, 0.6, 1, 0.3),
      rev(3, 0.94, 0.3, 0.05, 0.45), voices=12),
    P("Infinite Hall", ["Pads"], "cluster", "Major-seventh stack in an enormous hall that never seems to end.",
      cl(detune=0.25, spread=1.0, tone=0.55, chord="maj7", stack=3, shape=0.15, side=0.7, reso=0.1, keytrack=0.4, drift=0.3),
      amp(1.0, 2.0, 0.9, 4.0), mod(1.0, 2.0, 0.6, 3.0, 0.1), cho(1, 0.2, 0.5, 0.35), width(2, 1.4, 0.2, 12, 150),
      rev(3, 0.98, 0.35, 0.08, 0.55), voices=6),
    P("Rust Texture", ["Textures", "FX"], "grain", "Bowed metal grains played backwards through a slow comb: rusty, creaking, alive.",
      gr(inst="bowedmetal", pos=0.6, size=200, density=45, spray=0.6, reverse=0.8, spread=1.0, scan=-0.03, tone=0.65),
      amp(1.0, 2.0, 0.85, 3.0), mod(0.01, 1.0, 0.5, 1.0, 0.0), comb(2, 90, 0.8, 0.5, 0.05, 0.5, 0.5, 0.45),
      rev(3, 0.9, 0.5, 0.04, 0.4), voices=8),
    P("Warm Horizon", ["Pads"], "cluster", "Soft, wide add-nine pad with gentle drift: warm and open, sits behind everything.",
      cl(detune=0.2, spread=1.0, tone=0.45, chord="add9", stack=3, shape=0.2, side=0.6, reso=0.05, keytrack=0.4, drift=0.35),
      amp(0.9, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.6, 2.0, 0.1), cho(1, 0.25, 0.55, 0.35), rev(3, 0.88, 0.5, 0.03, 0.34), voices=6),
    P("Night Rain", ["Textures"], "grain", "Random kalimba droplets falling into a ping-pong delay and a dark hall.",
      gr(inst="kalimba", pos=0.3, size=60, density=12, spray=0.9, reverse=0.1, spread=1.0, scan=0.2, tone=0.6, pitch=12),
      amp(0.05, 1.0, 0.85, 3.0), mod(0.01, 1.0, 0.5, 1.0, 0.0), dly(2, 0.36, 0.55, 0.35, 1, 0.35), rev(3, 0.9, 0.6, 0.03, 0.4),
      voices=8),
    P("Ghost Organ", ["Pads", "Keys"], "wavetable", "Hollow harmonic organ with a slow autopan and a cavernous hall.",
      wt(table="harmonic", pos=0.55, tone=0.65, unison=0.2, univoices=3, keytrack=0.4, lfopos=0.15),
      amp(0.3, 2.0, 0.9, 2.5), mod(0.5, 2.0, 0.6, 2.0, 0.1), lfo(0.2, 0.5, "sine", "timbre"), tremolo(2, 0.5, "1/1", 0.35, "pan"),
      rev(3, 0.93, 0.5, 0.05, 0.42), voices=8),

    # ================================================================ STEREO FX (7)
    P("Wide Riser", ["FX"], "analog", "Noise and saws rising over four seconds, flanged and wide, into a long hall.",
      an(shape=0.0, detune=0.4, cutoff=300, reso=0.35, oscmix=0.5, noise=0.6, drive=0.3, keytrack=0.3),
      amp(4.0, 1.0, 1.0, 2.0), mod(4.0, 0.5, 1.0, 1.0, 1.0), pitchenv(-12, 4.0, 0.0), flanger(2, 0.3, 0.8, 0.6, 3.0, 0.5),
      rev(3, 0.9, 0.4, 0.04, 0.4), voices=2),
    P("Impact Boom", ["FX", "Boom"], "fm", "Sub impact with a metallic crack, squashed and thrown into a huge hall.",
      fm(algo=4, modratio=1.0, index=0.6, feedback=0.4, op1ratio=0.5, op2ratio=1.41, op3ratio=2.7, op4ratio=3.9, op1level=1.0,
         op2level=0.7, op3level=0.5, op4level=0.4, op5level=0.0, op6level=0.0),
      amp(0.001, 3.0, 0.0, 2.0), mod(0.001, 0.15, 0.0, 0.2, 1.0), pitchenv(-24, 0.4, 0.8), squash(2, 0.6, 0.3, 0.5),
      rev(3, 0.95, 0.4, 0.02, 0.4), voices=2),
    P("Stereo Zap", ["FX"], "wavetable", "Laser zap diving two octaves, thrown left and right by a ping-pong delay.",
      wt(table="digital", pos=0.6, tone=0.85, unison=0.3, univoices=4, uniwidth=1.0, keytrack=0.5),
      amp(0.001, 0.35, 0.0, 0.2), mod(0.001, 0.2, 0.0, 0.1, 0.5), pitchenv(24, 0.25, 0.9), dly(2, 0.25, 0.55, 0.5, 1, 0.35),
      rev(3, 0.7, 0.5, 0.02, 0.2), voices=4),
    P("Downlifter", ["FX"], "analog", "Falling noise and saw sweep over three seconds with a jet flanger - the end of the build.",
      an(shape=0.0, detune=0.3, cutoff=6000, reso=0.3, oscmix=0.5, noise=0.5, drive=0.2, keytrack=0.3),
      amp(0.01, 3.0, 0.0, 1.0), mod(0.01, 3.0, 0.0, 1.0, -0.8), pitchenv(12, 3.0, 0.0), flanger(2, 0.25, 0.8, 0.7, 2.0, 0.5),
      rev(3, 0.85, 0.45, 0.03, 0.35), voices=2),
    P("Glitch Spray", ["FX", "Textures"], "grain", "Tiny, fast grains crushed and scattered into a stereo delay: digital spray.",
      gr(inst="glockenspiel", pos=0.2, size=12, density=90, spray=0.9, reverse=0.5, spread=1.0, scan=0.5, tone=0.7),
      amp(0.005, 0.8, 0.6, 0.5), mod(0.01, 1.0, 0.5, 1.0, 0.0), crush(9, 3, 0.5), dly(2, 0.19, 0.5, 0.5, 1, 0.3), voices=4),
    P("Whoosh", ["FX"], "analog", "Filtered noise swept by a one-bar band-pass wobble, spread wide: a passing whoosh.",
      an(shape=0.0, detune=0.0, cutoff=12000, reso=0.0, oscmix=0.0, noise=1.0, drive=0.0, keytrack=0.0, osclevel=0.0),
      amp(0.4, 1.0, 0.9, 1.5), mod(0.01, 1.0, 0.5, 1.0, 0.0), wobble(2, "1/1", "tri", 300, 9000, 0.4, "bp", 0.0),
      width(3, 1.6, 0.6, 18, 200), voices=2),
    P("Siren Pan", ["FX"], "fm", "Wailing FM siren whose pitch swings and pans across the speakers.",
      fm(algo=2, modratio=2.0, index=0.25, op1ratio=1, op2ratio=1, op1level=1.0, op2level=0.6, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      amp(0.05, 1.0, 0.9, 0.6), mod(0.01, 1.0, 0.5, 1.0, 0.2), lfo(0.8, 0.9, "tri", "pitch"), tremolo(2, 0.8, "free", 0.7, "pan"),
      rev(3, 0.7, 0.5, 0.02, 0.25), voices=2),

    # ================================================================ WIDE PLUCKS / KEYS (6)
    P("Stereo Pluck", ["Plucks"], "wavetable", "Bright six-voice pluck spread wide with a dotted ping-pong delay and hall.",
      wt(table="classic", pos=0.5, tone=0.5, unison=0.3, univoices=6, uniwidth=1.0, keytrack=0.6),
      amp(0.002, 0.4, 0.0, 0.3), mod(0.001, 0.2, 0.0, 0.2, 0.6), dly(2, 0.45, 0.4, 0.5, 1, 0.24), rev(3, 0.75, 0.5, 0.02, 0.24),
      voices=8, velsens=0.5),
    P("Wide Glass Pluck", ["Plucks", "Keys"], "fm", "FM glass pluck with a Haas spread and a bright, airy reverb.",
      fm(algo=2, modratio=1.0, index=0.35, op1ratio=1, op2ratio=4, op3ratio=1, op4ratio=7, op1level=1.0, op2level=0.5,
         op3level=0.5, op4level=0.3, op5level=0.0, op6level=0.0, velidx=0.6),
      amp(0.001, 0.8, 0.0, 0.5), mod(0.001, 0.3, 0.0, 0.3, 0.7), width(2, 1.4, 0.45, 12, 150), rev(3, 0.8, 0.35, 0.03, 0.28),
      voices=10),
    P("Future Chord", ["Plucks", "Keys"], "cluster", "Wide minor-seventh chord stab with squash and a short shimmer - one key, one chord.",
      cl(detune=0.25, spread=0.9, tone=0.6, chord="min7", stack=3, shape=0.05, side=0.6, reso=0.15, keytrack=0.5, drift=0.1),
      amp(0.002, 0.6, 0.0, 0.35), mod(0.001, 0.3, 0.0, 0.2, 0.5), squash(2, 0.45, 0.35, 0.5), rev(3, 0.7, 0.45, 0.02, 0.26),
      voices=6),
    P("Bright Poly Wide", ["Keys"], "analog", "Classic bright poly synth, chorused and widened - chords that fill the room.",
      an(shape=0.1, detune=0.2, cutoff=3200, reso=0.12, oscmix=0.5, drive=0.2, keytrack=0.6),
      amp(0.01, 0.8, 0.75, 0.4), mod(0.01, 0.5, 0.4, 0.3, 0.25), cho(1, 0.5, 0.6, 0.4), width(2, 1.4, 0.25, 12, 150),
      rev(3, 0.6, 0.5, 0.02, 0.18), voices=8),
    P("Echo Harp", ["Plucks"], "pluck", "Plucked nylon-like string with dotted ping-pong echoes and a wide room.",
      pl(material=0.15, damping=0.35, bright=0.55, body=0.5, excite=0.5, pickpos=0.2, bodysize=0.6),
      amp(0.002, 2.0, 0.0, 0.4), mod(0.001, 0.3, 0.0, 0.2, 0.0), dly(2, 0.45, 0.45, 0.45, 1, 0.28), rev(3, 0.7, 0.5, 0.02, 0.22),
      voices=10, velsens=0.6),
    P("Wide Organ", ["Keys"], "wavetable", "Drawbar-like organ with a rotating stereo pan and a short hall.",
      wt(table="harmonic", pos=0.7, tone=0.8, unison=0.1, univoices=3, keytrack=0.4),
      amp(0.005, 0.5, 1.0, 0.1), mod(0.001, 0.3, 0.0, 0.1, 0.0), tremolo(2, 5.8, "free", 0.4, "pan"), rev(3, 0.55, 0.5, 0.01, 0.18),
      voices=10),

    # ================================================================ HYBRID: two engines in one part (18)
    P("Glass Engine", ["Pads", "Hybrid"], "grain", "Singing-glass grain cloud fused with a harmonic wavetable layer that scans slowly; a touch of ring modulation.",
      gr(inst="glass", pos=0.3, size=260, density=60, spray=0.35, reverse=0.2, spread=1.0, scan=0.04, tone=0.8),
      wt(table="harmonic", pos=0.35, tone=0.65, unison=0.35, univoices=5, uniwidth=1.0, keytrack=0.4, lfopos=0.5),
      layer("wavetable", 0.55, 0, 6.0, "ring", 0.12, 0.8), amp(0.8, 2.0, 0.9, 3.5), mod(1.0, 2.0, 0.6, 2.0, 0.1),
      lfo(0.07, 0.8, "tri", "timbre"), cho(1, 0.25, 0.5, 0.3), rev(3, 0.92, 0.4, 0.04, 0.42), voices=8),
    P("Pluck Nebula", ["Plucks", "Hybrid"], "pluck", "A plucked string that dissolves into a kalimba grain cloud: the string fades, the cloud swells and lingers.",
      pl(material=0.3, damping=0.3, bright=0.6, body=0.4, excite=0.55, pickpos=0.2, bodysize=0.5),
      gr(inst="kalimba", pos=0.15, size=200, density=60, spray=0.5, reverse=0.3, spread=1.0, scan=0.06, tone=0.75),
      layer("grain", 0.65, 12, 0.0, "ring", 0.0, 1.0, (0.5, 2.5, 0.6, 3.0)), amp(0.002, 1.8, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      dly(2, 0.45, 0.4, 0.45, 1, 0.22), rev(3, 0.9, 0.45, 0.04, 0.4), voices=8, velsens=0.5),
    P("Vox Reese", ["Bass", "Hybrid"], "wavetable", "A reese with a singing vocal layer on top: the formants ride the bass, a synced vowel filter makes it talk.",
      wt(table="classic", pos=SAW, tone=0.58, unison=0.5, univoices=4, uniwidth=0.7, sub=0.35, keytrack=0.3),
      vo(vowel=0.0, formant=-2.0, breath=0.15, tension=0.7, vibdepth=0.0, vibrate=5.0, jitter=0.05),
      layer("vocal", 0.6, 12, 0.0, "am", 0.25, 0.6), amp(0.004, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05),
      formant(2, 0.2, 0.0, 0.5, 1.0, "1/2", 0.6, 0.5), width(3, 1.3, 0.0, 12, 130), MONO, glide=0.03),
    P("FM Grains", ["Textures", "Hybrid"], "grain", "Bowed-metal grains ring-modulated by an FM bell layer: grains that clang in tune.",
      gr(inst="bowedmetal", pos=0.4, size=220, density=55, spray=0.4, reverse=0.2, spread=1.0, scan=0.05, tone=0.8),
      fm(algo=2, modratio=1.0, index=0.3, op1ratio=1, op2ratio=3.5, op1level=1.0, op2level=0.6, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      layer("fm", 0.5, 0, 0.0, "ring", 0.45, 0.5), amp(0.5, 2.0, 0.9, 3.0), mod(0.01, 2.0, 0.5, 2.0, 0.3),
      flanger(2, 0.05, 0.5, 0.3, 3.0, 0.25), rev(3, 0.9, 0.45, 0.04, 0.4), voices=8),
    P("Ring Machine", ["Leads", "Hybrid"], "analog", "Saw lead ring-modulated by an FM sine a fifth up: metallic, vocal and aggressive, with delay.",
      an(shape=0.0, detune=0.1, cutoff=4000, reso=0.2, oscmix=0.5, drive=0.35, keytrack=0.7),
      fm(algo=7, modratio=1.0, index=0.0, op1ratio=1, op1level=1.0, op2level=0.0, op3level=0.0, op4level=0.0, op5level=0.0, op6level=0.0),
      layer("fm", 0.3, 7, 0.0, "ring", 0.65, 0.5), amp(0.004, 0.5, 0.9, 0.2), mod(0.01, 0.35, 0.5, 0.2, 0.2),
      dly(2, 0.33, 0.35, 0.45, 1, 0.22), rev(3, 0.55, 0.5, 0.02, 0.15), MONO, glide=0.03),
    P("Fold Bass", ["Bass", "Hybrid"], "wavetable", "Sine sub and an analog saw folded into each other: the fold amount makes it roar.",
      wt(table="classic", pos=0.0, tone=0.4, unison=0.0, univoices=1, sub=0.5, keytrack=0.3),
      an(shape=0.0, detune=0.1, cutoff=1400, reso=0.2, oscmix=0.5, drive=0.3, keytrack=0.4),
      layer("analog", 0.5, 0, 0.0, "fold", 0.55, 0.3), amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.5, 0.2, 0.2),
      squash(2, 0.6, 0.35, 0.5), width(3, 1.2, 0.0, 12, 140), MONO),
    P("Chop Pad", ["Pads", "Hybrid"], "cluster", "A wide chord pad chopped and stuttered by the envelope of a kalimba grain layer.",
      cl(detune=0.25, spread=1.0, tone=0.6, chord="min7", stack=3, shape=0.1, side=0.7, reso=0.1, keytrack=0.4, drift=0.2),
      gr(inst="kalimba", pos=0.2, size=70, density=14, spray=0.3, reverse=0.0, spread=1.0, scan=0.1, tone=0.8),
      layer("grain", 0.35, 12, 0.0, "am", 0.85, 1.0), amp(0.3, 2.0, 0.9, 2.0), mod(0.5, 2.0, 0.6, 2.0, 0.1),
      dly(2, 0.375, 0.35, 0.5, 1, 0.2), rev(3, 0.85, 0.45, 0.03, 0.32), voices=6),
    P("Tape Ghost Keys", ["Keys", "Hybrid", "Piano"], "sampler", "Tine piano with a ghost of itself: a detuned grain copy, crushed and flanged like an old resampled tape.",
      sm(tone=0.85, inst2=KEYS["tine"]),
      gr(pos=0.05, size=180, density=40, spray=0.2, reverse=0.35, spread=1.0, scan=0.02, tone=0.6, inst2=KEYS["tine"]),
      layer("grain", 0.5, 0, -14.0, "ring", 0.0, 0.8, (0.3, 2.0, 0.5, 2.5)), amp(0.001, 4.0, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      crush(11, 2, 0.3), flanger(2, 0.07, 0.5, 0.3, 4.0, 0.3), rev(3, 0.75, 0.6, 0.03, 0.28), voices=12, velsens=0.6),
    P("Spectral Morph", ["Pads", "Hybrid"], "wavetable", "Formant table morphing under a breathy vocal layer, both through a drifting comb: spectra that melt into each other.",
      wt(table="formant", pos=0.2, tone=0.65, unison=0.35, univoices=5, uniwidth=1.0, keytrack=0.4, lfopos=0.6),
      vo(vowel=2.0, formant=0.0, breath=0.5, tension=0.4, vibdepth=0.05, vibrate=4.5, jitter=0.2),
      layer("vocal", 0.5, 0, 0.0, "ring", 0.18, 0.7), amp(1.0, 2.0, 0.9, 3.5), mod(1.0, 2.0, 0.6, 2.0, 0.1),
      lfo(0.05, 0.9, "tri", "timbre"), comb(2, 220, 0.5, 0.5, 0.04, 0.5, 0.6, 0.3), rev(3, 0.93, 0.4, 0.05, 0.42), voices=6),
    P("Choir Grains", ["Vocal", "Hybrid"], "vocal", "Ah choir doubled by a recorder grain cloud an octave up - human and synthetic at once.",
      vo(vowel=0.2, formant=0.0, breath=0.25, tension=0.5, vibdepth=0.15, vibrate=5.0, jitter=0.2),
      gr(inst="recorder", pos=0.3, size=260, density=50, spray=0.3, reverse=0.1, spread=1.0, scan=0.03, tone=0.7),
      layer("grain", 0.45, 12, 0.0, "ring", 0.0, 1.0), amp(0.6, 2.0, 0.9, 2.5), mod(0.5, 2.0, 0.6, 2.0, 0.1),
      cho(1, 0.25, 0.5, 0.3), rev(3, 0.92, 0.4, 0.04, 0.42), voices=8),
    P("Neuro Hybrid", ["Bass", "Hybrid"], "fm", "FM growl and a wavetable reese ring-modulating each other, folded and squashed: a neuro monster.",
      fm(algo=0, modratio=1.0, index=0.35, feedback=0.3, op1ratio=1, op2ratio=1, op3ratio=2, op4ratio=1, op5ratio=0.5, op6ratio=1,
         op1level=1.0, op2level=0.7, op3level=0.4, op4level=0.4, op5level=0.3, op6level=0.4),
      wt(table="classic", pos=SAW, tone=0.6, unison=0.5, univoices=4, uniwidth=0.7, keytrack=0.3),
      layer("wavetable", 0.6, 0, 0.0, "ring", 0.3, 0.6), amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.3, 0.6, 0.2, 0.4),
      lfo(2.5, 0.4, "tri", "timbre"), shaper(2, "fold", 0.3, 0.0, 8000, 0.6), squash(3, 0.75, 0.35, 0.6, -2, 1, 2), MONO),
    P("Halo Grand", ["Keys", "Hybrid", "Piano"], "sampler", "Grand piano with a halo: its own recording frozen into a grain cloud that blooms behind every chord.",
      sm(tone=1.0, inst2=KEYS["grand"]),
      gr(pos=0.12, size=320, density=45, spray=0.25, reverse=0.15, spread=1.0, scan=0.02, tone=0.75, inst2=KEYS["grand"]),
      layer("grain", 0.45, 12, 0.0, "ring", 0.0, 1.0, (0.8, 2.0, 0.6, 3.5)), amp(0.001, 8.0, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      cho(1, 0.2, 0.4, 0.2), rev(3, 0.88, 0.45, 0.04, 0.32), voices=16, velsens=0.7),
    P("Metal Strings", ["Pads", "Hybrid"], "pluck", "Metallic plucked strings folded into a slow cluster pad - an orchestra made of wire.",
      pl(material=0.8, damping=0.2, bright=0.6, body=0.3, excite=0.4, pickpos=0.15, bodysize=0.4),
      cl(detune=0.2, spread=1.0, tone=0.5, chord="unison", stack=5, shape=0.1, side=0.6, reso=0.15, keytrack=0.4, drift=0.3),
      layer("cluster", 0.5, 0, 0.0, "fold", 0.2, 0.8, (0.8, 2.0, 0.9, 2.5)), amp(0.002, 2.5, 0.0, 1.0), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      cho(1, 0.2, 0.5, 0.3), rev(3, 0.9, 0.45, 0.04, 0.38), voices=8),
    P("Digital Monster", ["Bass", "Hybrid"], "wavetable", "Digital FM table gated by a glass-grain layer an octave down, then wobbled: unstable and huge.",
      wt(table="digital", pos=0.5, tone=0.75, unison=0.35, univoices=4, uniwidth=0.6, fm=0.3, fmratio=2.0, sub=0.35, keytrack=0.2),
      gr(inst="glass", pos=0.4, size=60, density=30, spray=0.5, reverse=0.3, spread=0.5, scan=0.2, tone=0.7),
      layer("grain", 0.3, -12, 0.0, "am", 0.5, 0.3), amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.5, 0.2, 0.0),
      wobble(2, "1/8T", "sine", 140, 3500, 0.5, "lp", 0.35), squash(3, 0.7, 0.35, 0.6), MONO),
    P("Crystal Organ", ["Keys", "Hybrid"], "fm", "Additive FM organ with a glass wavetable layer, circling in stereo with a slow autopan.",
      fm(algo=7, modratio=1.0, index=0.0, op1ratio=1, op2ratio=2, op3ratio=3, op4ratio=4, op5ratio=6, op6ratio=8, op1level=1.0,
         op2level=0.7, op3level=0.5, op4level=0.4, op5level=0.25, op6level=0.2),
      wt(table="glass", pos=0.5, tone=0.8, unison=0.25, univoices=3, keytrack=0.4),
      layer("wavetable", 0.45, 12, 4.0, "ring", 0.0, 0.8), amp(0.01, 1.0, 0.95, 0.4), mod(0.01, 0.5, 0.5, 0.3, 0.0),
      tremolo(2, 0.9, "free", 0.4, "pan"), rev(3, 0.8, 0.45, 0.03, 0.3), voices=8),
    P("Breath Lead", ["Leads", "Hybrid"], "vocal", "Breathy sung tone fused with a soft saw stack: a lead between a voice and a synth.",
      vo(vowel=0.6, formant=0.0, breath=0.45, tension=0.55, vibdepth=0.2, vibrate=5.2, jitter=0.1),
      wt(table="classic", pos=0.35, tone=0.65, unison=0.3, univoices=4, uniwidth=0.9, keytrack=0.5),
      layer("wavetable", 0.5, 0, 0.0, "ring", 0.0, 0.8), amp(0.03, 0.6, 0.9, 0.35), mod(0.01, 0.4, 0.5, 0.3, 0.1),
      dly(2, 0.42, 0.35, 0.45, 1, 0.22), rev(3, 0.8, 0.45, 0.03, 0.28), voices=4, glide=0.03),
    P("Orbit Pad", ["Pads", "Hybrid"], "grain", "Harp grains orbiting an FM layer, ring-modulated softly and panning over two bars.",
      gr(inst="harp", pos=0.2, size=240, density=55, spray=0.4, reverse=0.2, spread=1.0, scan=0.04, tone=0.8),
      fm(algo=2, modratio=1.0, index=0.15, op1ratio=1, op2ratio=2, op1level=1.0, op2level=0.5, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      layer("fm", 0.4, 0, 0.0, "ring", 0.25, 0.5, (1.0, 2.0, 0.8, 3.0)), amp(0.6, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.6, 2.0, 0.1),
      tremolo(2, 0.25, "2/1", 0.5, "pan"), rev(3, 0.92, 0.4, 0.04, 0.42), voices=8),
    P("Resonator Hit", ["Plucks", "Hybrid"], "pluck", "A struck metal bar feeding a short grain resonance and a tuned comb: percussive, ringing, strange.",
      pl(material=1.0, damping=0.4, bright=0.7, body=0.2, excite=0.7, pickpos=0.1, bodysize=0.3),
      gr(inst="vibraphone", pos=0.1, size=120, density=50, spray=0.3, reverse=0.1, spread=1.0, scan=0.1, tone=0.8),
      layer("grain", 0.45, 0, 0.0, "fold", 0.15, 1.0, (0.01, 1.2, 0.0, 1.0)), amp(0.002, 1.5, 0.0, 0.5), mod(0.001, 0.3, 0.0, 0.2, 0.0),
      comb(2, 440, 0.6, 0.4, 0.1, 0.1, 0.4, 0.3), rev(3, 0.75, 0.5, 0.02, 0.26), voices=8, velsens=0.6),
]
