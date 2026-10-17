# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p44: factory part presets that show off the newer features - Life, the morph pad, audio-to-wavetable user tables,
the resonator body (stage and rack type), the character macros (Bright / Hard / Alive / Wide), the 3D spatialiser and
its binaural motion paths, the SUB layer, the Expression page (velocity / aftertouch), pressure glide and tempo-synced
movement (LFO 2 / 3 locked to the tempo, motion paths measured in bars - they follow the tempo whether the metronome
clicks or not).

Written by make_showcase.py (it writes only these files and never touches the rest of the bank). Each preset lists the
features it is built around ("features"); the docs and the tests read them. Every preset:
  * has a musical mod wheel routing (wheel=...) or drives its morph pad from the wheel,
  * answers velocity and / or aftertouch on its Expression page where that suits the sound,
  * glides only when gliding is its point (exactly one pressure -> glide mechanism then),
  * is level-calibrated (showcase_levels.json, written by showcase_cal.py: part gain to the category target).
German descriptions: "de".
"""
import json, os

from factory_parts import ENGINES, arp, an, fm, wt, pl, cl, gr, vo, sm, amp, mod, lfo, pitchenv, MONO, P as _P
from factory_parts_r3 import fx, chain, slot_prefix, lfo_n, mpe, KEYS

HERE = os.path.dirname(os.path.abspath(__file__))
_lv = os.path.join(HERE, "showcase_levels.json")
LEVELS = json.load(open(_lv)) if os.path.exists(_lv) else {}

# modulation matrix: source / target keys -> indices (src/core/mod/ModMatrix.cpp, append-only)
SRC = ["off", "lfo1", "lfo2", "lfo3", "ampenv", "modenv", "env3", "velocity", "key", "modwheel", "bend", "chpressure",
       "pressure", "slide", "notebend", "random", "macro1", "macro2", "macro3", "macro4", "seqlane", "morphx", "morphy",
       "motion1", "motion2", "motion3", "motion4", "padsweep", "padheld", "chordroot", "chordchange", "tension", "vibrato"]
DST = (["off", "pitch", "level", "pan", "timbre"] + ["engine%d" % (i + 1) for i in range(24)] +
       ["volume", "lfo1rate", "lfo1depth", "lfo2rate", "lfo2depth", "lfo3rate", "lfo3depth", "modenvamt"] +
       ["fx%dmix" % (k + 1) for k in range(6)] + ["fx%dp%d" % (k + 1, j + 1) for k in range(6) for j in range(4)] +
       ["azimuth", "elevation", "distance", "pathrate", "pathphase", "life", "morphx", "morphy",
        "resotune", "resosize", "resomaterial", "resodecay", "resoposition", "resomix",
        "charbright", "charhard", "charalive", "charwide", "sublevel", "glide", "ampattack", "ampdecay", "amprelease"])
assert DST.index("azimuth") == 67 and DST.index("life") == 72 and DST.index("sublevel") == 85 and DST.index("amprelease") == 89
assert SRC.index("modwheel") == 9 and SRC.index("vibrato") == 32
CURVE = {"lin": 0, "exp": 1, "log": 2, "s": 3, "steps": 4}
# Expression page targets (pN.expr.*.dst*, append-only)
EXPR = ["off", "cutoff", "resonance", "level", "timbre", "attack", "decay", "release", "modenv", "lforate", "lfodepth",
        "vibrato", "glide", "fm", "position", "spray", "morphx", "morphy", "drive", "fxmix", "sub", "pan", "pitch", "life"]
PATH = ["off", "circle", "figure8", "flyby", "orbit", "pendulum", "spiral", "pingpong", "swarm", "whisper", "walk",
        "overhead", "weave"]
PATHLEN = ["1/16", "1/8", "1/4", "1/2", "1bar", "2bars", "4bars", "8bars", "16bars", "32bars", "1/4."]
RESO_MODEL = {"string": 0, "plate": 1, "open": 2, "closed": 3}
RESO_FOLLOW = {"fixed": 0, "note": 1, "chord": 2}
SUB_SHAPE = {"sine": 0, "round": 1, "square": 2}


def life(amount):
    return {"life": amount}


def char(bright=0.0, hard=0.0, alive=0.0, wide=0.0):
    return {k: v for k, v in (("char.bright", bright), ("char.hard", hard), ("char.alive", alive), ("char.wide", wide)) if v}


def sub(level, oct=-1, shape="sine", direct=True):
    return {"sub.level": level, "sub.oct": 0 if oct == -1 else 1, "sub.shape": SUB_SHAPE[shape], "sub.direct": 1 if direct else 0}


def reso(model="string", follow="note", tune=0.0, size=0.5, material=0.55, decay=2.0, position=0.3, inharm=0.1,
         sympathetic=0.35, width=0.6, drive=0.0, mix=0.5, press=0.5, slide=0.5):
    """The part's resonator body stage (pN.reso.*), switched on."""
    return {"reso.on": 1, "reso.model": RESO_MODEL[model], "reso.follow": RESO_FOLLOW[follow], "reso.tune": tune,
            "reso.size": size, "reso.material": material, "reso.decay": decay, "reso.position": position,
            "reso.inharm": inharm, "reso.sympathetic": sympathetic, "reso.width": width, "reso.drive": drive,
            "reso.mix": mix, "reso.pressamt": press, "reso.slideamt": slide}


def reso_fx(slot=0, model="string", follow="note", tune=0.0, size=0.5, material=0.55, decay=2.0, position=0.3, inharm=0.1,
            sympathetic=0.35, width=0.6, drive=0.0, mix=0.5):
    """The resonator as a rack effect (type 23, '<slot>resonator.*'), placed with the tone-shaping effects."""
    p = {"type": 23, "resonator.model": RESO_MODEL[model], "resonator.follow": RESO_FOLLOW[follow], "resonator.tune": tune,
         "resonator.size": size, "resonator.material": material, "resonator.decay": decay, "resonator.position": position,
         "resonator.inharm": inharm, "resonator.sympathetic": sympathetic, "resonator.width": width,
         "resonator.drive": drive, "resonator.mix": mix}
    return {"__fx__": "comb", "slot": slot, "params": p}  # ranked like the comb (tone stage)


def space(path="circle", length="2bars", rate=1.0, size=0.5, azimuth=0.0, elevation=0.0, distance=2.0, spread=40.0,
          room=0.3, roomsize=0.5, doppler=0.3, air=0.6, rolloff=0.7, lowkeep=120.0, glide=0.06, phase=0.0):
    """The part's 3D stage with a motion path whose length is measured in bars (it follows the tempo)."""
    return {"space.on": 1, "space.path": PATH.index(path), "space.pathlen": PATHLEN.index(length), "space.pathrate": rate,
            "space.pathsize": size, "space.pathphase": phase, "space.azimuth": azimuth, "space.elevation": elevation,
            "space.distance": distance, "space.spread": spread, "space.room": room, "space.roomsize": roomsize,
            "space.doppler": doppler, "space.air": air, "space.rolloff": rolloff, "space.lowkeep": lowkeep,
            "space.glide": glide}


def expr(vel=(), at=()):
    """Expression page: up to two velocity and two aftertouch slots, each (target, amount -1..1[, curve])."""
    out = {}
    for kind, slots in (("vel", vel), ("at", at)):
        for i, s in enumerate(slots):
            out["expr.%s.dst%d" % (kind, i + 1)] = EXPR.index(s[0])
            out["expr.%s.amt%d" % (kind, i + 1)] = s[1]
            out["expr.%s.curve%d" % (kind, i + 1)] = CURVE[s[2] if len(s) > 2 else "lin"]
    return out


def route(src, dst, amt, via="off", curve="lin"):
    return (src, dst, amt, via, curve)


def comp(slot=9, thresh=-14.0, ratio=3.0, attack=2.0, release=90.0, knee=6.0, makeup=0.0, mix=1.0):
    return fx(slot, "compressor", thresh=thresh, ratio=ratio, attack=attack, release=release, knee=knee, makeup=makeup, mix=mix)


PRESETS_SHOWCASE = []


GUARD = {"thresh": -7.0, "ratio": 8.0, "attack": 0.1, "release": 120.0, "knee": 6.0, "makeup": 0.0, "mix": 1.0}


def S(name, tags, engine, features, en, de, *dicts, wheel=None, routes=(), usertable=None, arp_block=None, guard=None, **kw):
    """One showcase part preset. `features`: the feature keys it is built around (docs, tests). `wheel`: the mod wheel
    routing (target key, amount[, curve]) - every preset has one unless the morph pad takes the wheel. `routes`: further
    matrix routings (src, dst, amt, via, curve)."""
    fxs = [d for d in dicts if isinstance(d, dict) and "__fx__" in d]
    mpes = [d for d in dicts if isinstance(d, dict) and "__mpe__" in d]
    rest = [d for d in dicts if not (isinstance(d, dict) and ("__fx__" in d or "__mpe__" in d))]
    n = 0
    if fxs:
        for n, f in enumerate(chain(fxs), 1):
            rest.append({slot_prefix(n - 1) + k: v for k, v in f["params"].items()})
    if guard is not None:  # a fast peak compressor after the whole chain (peaks of a near pass, grain bursts)
        g = dict(GUARD, thresh=guard[0], makeup=guard[1]) if isinstance(guard, tuple) else dict(GUARD, thresh=guard)
        rest.append(dict({slot_prefix(n) + "type": 10}, **{slot_prefix(n) + "compressor." + k: v for k, v in g.items()}))
    rr = list(routes)
    if wheel is not None:
        rr.append(route("modwheel", wheel[0], wheel[1], "off", wheel[2] if len(wheel) > 2 else "lin"))
    m = {}
    for i, (src, dst, amt, via, curve) in enumerate(rr):
        s = "mod.s%d." % (i + 1)
        m.update({s + "src": SRC.index(src), s + "via": SRC.index(via), s + "dst": DST.index(dst), s + "amt": amt,
                  s + "curve": CURVE[curve]})
    rest.append(m)
    tags = list(tags)
    if mpes:
        rest.append(mpes[0]["params"])
        if "MPE" not in tags:
            tags.append("MPE")
    if ("Bass" in tags or "Boom" in tags) and not any("monobass" in d for d in rest):
        rest.append({"monobass": 150.0})
    if arp_block is not None:  # mode, octaves, gate, rate (latch is the player's choice)
        rest.append(arp(arp_block[0], arp_block[1], arp_block[2], arp_block[3], latch=0))
    p = _P(name, tags, engine, en, *rest, **kw)
    if "glide" not in p["params"]:
        p["params"]["glide"] = 0.0  # glide off unless the preset is about gliding
    p["params"]["gain"] = round(LEVELS.get(name, 0.0), 1)
    p["de"] = de
    p["features"] = list(features)
    if usertable:
        p["usertable"] = usertable
    PRESETS_SHOWCASE.append(p)
    return p


# ======================================================================================================== Bass
S("Sub Pressure Bass", ["Bass"], "analog", ["sub", "expr", "char"],
  "A tight saw bass over a clean sine sub two octaves down that stays full under the closed filter. Velocity bites into the "
  "filter, pressure opens it; the mod wheel hardens the attack and drive.",
  "Ein straffer Sägezahnbass über einem sauberen Sinus-Sub zwei Oktaven tiefer, der unter dem geschlossenen Filter voll "
  "bleibt. Anschlag beißt ins Filter, Druck öffnet es; das Modrad macht Attack und Drive härter.",
  an(shape=0.1, detune=0.06, cutoff=420, reso=0.25, oscmix=0.25, sub=0.0, drive=0.3, keytrack=0.45),
  amp(0.003, 0.6, 0.75, 0.08), mod(0.001, 0.22, 0.1, 0.1, 0.45), MONO,
  sub(0.55, -2, "sine", True), expr(vel=[("cutoff", 0.35, "exp")], at=[("cutoff", 0.4, "s")]),
  comp(9, -14, 3.5, 1.5, 80), lfo(0.17, 0.1, "tri", "timbre"),
  wheel=("charhard", 0.7), velsens=0.45)

S("Round Square Sub", ["Bass", "Boom"], "fm", ["sub", "expr"],
  "A deep FM sine bass whose weight comes from the SUB layer's soft square one octave down: hollow push in the low mids, "
  "clean on big systems. The mod wheel adds FM grit; velocity sets the punch.",
  "Ein tiefer FM-Sinusbass, dessen Gewicht vom weichen Rechteck der SUB-Ebene eine Oktave tiefer kommt: hohler Druck in "
  "den unteren Mitten, sauber auf großen Anlagen. Das Modrad bringt FM-Körnung, der Anschlag bestimmt den Punch.",
  fm(algo=0, modratio=1.0, index=0.08, feedback=0.1, op1ratio=1, op2ratio=1, op1level=1.0, op2level=0.3, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.003, 0.9, 0.6, 0.12), mod(0.001, 0.15, 0.0, 0.1, 0.3), MONO, pitchenv(5, 0.03, 0.7),
  sub(0.45, -1, "square", True), expr(vel=[("drive", 0.25, "exp"), ("level", 0.2)]),
  lfo(0.17, 0.08, "tri", "timbre"),
  wheel=("timbre", 0.45), velsens=0.5)

S("Pressure Glide Bass", ["Bass"], "analog", ["glidepress", "expr"],
  "A legato mono bass that slides between overlapping notes. Pressure stretches the slide (pressure glide): a light touch "
  "keeps it tight, leaning in turns it into a slow swoop. The mod wheel opens the filter.",
  "Ein Legato-Monobass, der zwischen überlappenden Noten gleitet. Druck dehnt das Gleiten (Glide-Druck): leicht gespielt "
  "bleibt es knapp, mit Druck wird es zum langsamen Bogen. Das Modrad öffnet das Filter.",
  an(shape=0.25, detune=0.08, cutoff=520, reso=0.3, oscmix=0.35, osc2oct=-1, sub=0.35, drive=0.35, keytrack=0.5),
  amp(0.003, 0.5, 0.8, 0.09), mod(0.001, 0.3, 0.2, 0.12, 0.35), MONO,
  {"glide": 0.07, "glide.mode": 1, "glide.rate": 0, "glide.press": 0.75},
  expr(vel=[("cutoff", 0.3)]), lfo(0.17, 0.1, "tri", "timbre"), comp(9, -14, 3.0, 2.0, 90),
  wheel=("timbre", 0.5), velsens=0.4)

S("Living Reese", ["Bass"], "wavetable", ["life", "char", "sub"],
  "A wide reese whose unison voices never settle: Life lets every voice drift on its own, Alive and Wide push the stack "
  "apart while a mono sine sub holds the floor. The mod wheel opens the wavetable tone.",
  "Ein breiter Reese, dessen Unisono-Stimmen nie zur Ruhe kommen: Leben lässt jede Stimme eigen driften, Lebendig und Weit "
  "ziehen den Stapel auseinander, ein Mono-Sinus-Sub hält das Fundament. Das Modrad öffnet den Wavetable-Ton.",
  wt(table="classic", pos=0.62, warp=0.1, tone=0.42, unison=0.55, univoices=6, uniwidth=0.8, sub=0.0, keytrack=0.4,
     lfopos=0.1),
  amp(0.004, 0.6, 0.85, 0.12), mod(0.002, 0.4, 0.3, 0.2, 0.2), MONO,
  life(0.45), char(alive=0.3, wide=0.35), sub(0.5, -1, "sine", True),
  fx(1, "chorus", rate=0.3, depth=0.5, mix=0.3, voices=3, drift=0.3, spread=0.85, lowcut=320.0), comp(9, -14, 3.5, 1.5, 80),
  expr(vel=[("cutoff", 0.25)], at=[("fm", 0.3)]),
  wheel=("timbre", 0.5), velsens=0.35)

S("Wood Body Bass", ["Bass"], "pluck", ["reso", "expr"],
  "A plucked string bass ringing a short wooden body tuned to each note: round, woody thumps. Velocity strikes harder and "
  "brighter; the mod wheel changes the body from wood to metal.",
  "Ein gezupfter Saitenbass, der einen kurzen Holzkörper auf jeder Note mitschwingen lässt: runde, holzige Schläge. "
  "Anschlag schlägt härter und heller an; das Modrad macht den Körper von Holz zu Metall.",
  pl(material=0.15, damping=0.55, bright=0.35, body=0.6, excite=0.5, pickpos=0.2, bodysize=0.7),
  amp(0.002, 1.2, 0.0, 0.15), mod(0.001, 0.3, 0.0, 0.1, 0.2), MONO,
  reso("string", "note", tune=-12.0, size=0.6, material=0.3, decay=0.6, position=0.25, inharm=0.05, sympathetic=0.0,
       width=0.3, mix=0.4),
  expr(vel=[("cutoff", 0.4, "exp"), ("drive", 0.25)]), comp(9, -13, 3.5, 1.5, 80), lfo(0.17, 0.08, "tri", "timbre"),
  wheel=("resomaterial", 0.55), velsens=0.6)

S("Tempo Wobble Sub", ["Bass"], "analog", ["tempo", "sub"],
  "A growling filter wobble locked to the tempo (LFO 2 on eighth notes, retriggered with each note) over a steady mono sub "
  "that never wobbles. The mod wheel deepens the wobble.",
  "Ein knurrendes Filter-Wobble, fest im Tempo (LFO 2 in Achteln, mit jeder Note neu gestartet), über einem ruhigen "
  "Mono-Sub, der nie mitwackelt. Das Modrad vertieft das Wobble.",
  an(shape=0.0, detune=0.12, cutoff=300, reso=0.45, oscmix=0.5, sub=0.0, drive=0.5, keytrack=0.4),
  amp(0.003, 0.5, 0.85, 0.1), mod(0.001, 0.2, 0.0, 0.1, 0.0), MONO,
  lfo_n(2, rate=2.0, sync="1/8", shape="sine", mode="retrig", depth=0.55), sub(0.6, -1, "sine", True),
  {"lfo.depth": 0.0}, fx(0, "distortion", mode="tube", drive=0.25, mix=0.5, output=0.0), comp(9, -14, 3.5, 1.5, 80),
  expr(vel=[("cutoff", 0.2)]),
  routes=[route("lfo2", "engine3", 0.45)], wheel=("lfo2depth", 0.45), velsens=0.35)

# ======================================================================================================== Keys
S("Velocity Glass EP", ["Keys"], "fm", ["expr"],
  "An FM electric piano that answers the hands: soft notes are round and bloom in slowly, hard ones bark with a glassy "
  "tine; pressure adds a gentle vibrato. The mod wheel brings in a stereo tremolo.",
  "Ein FM-E-Piano, das den Händen folgt: leise Noten sind rund und blühen langsam auf, harte bellen mit gläserner Zunge; "
  "Druck gibt ein sanftes Vibrato. Das Modrad bringt ein Stereo-Tremolo.",
  fm(algo=2, modratio=1.0, index=0.3, op1ratio=1, op2ratio=1, op3ratio=1, op4ratio=7, op1level=1.0, op2level=0.6,
     op3level=0.7, op4level=0.18, op5level=0.0, op6level=0.0),
  amp(0.002, 2.2, 0.25, 0.45), mod(0.001, 0.9, 0.25, 0.4, 0.7), lfo(4.8, 0.0, "sine", "pan"),
  expr(vel=[("timbre", 0.45, "exp"), ("attack", -0.25)], at=[("vibrato", 0.15, "s")]),
  fx(1, "chorus", rate=0.45, depth=0.5, mix=0.35, voices=3, drift=0.35, spread=0.85, lowcut=160.0),
  fx(5, "reverb", size=0.45, damping=0.5, predelay=0.02, mix=0.16),
  wheel=("lfo1depth", 0.55), voices=14, velsens=0.7)

S("Struck Plate Keys", ["Keys"], "cluster", ["reso", "expr"],
  "Soft mallet keys striking small tuned metal plates: the attack blooms into bell partials that follow every note. "
  "Velocity strikes the plate harder; the mod wheel lengthens its ring.",
  "Weiche Schlegeltasten auf kleinen, gestimmten Metallplatten: der Anschlag blüht zu Glocken-Teiltönen auf, die jeder "
  "Note folgen. Anschlag trifft die Platte härter; das Modrad lässt sie länger klingen.",
  cl(detune=0.04, spread=0.25, tone=0.45, chord="unison", stack=1, shape=0.15, side=0.2, reso=0.1, keytrack=0.6, drift=0.1),
  amp(0.003, 1.2, 0.2, 0.9), mod(0.001, 0.4, 0.0, 0.3, 0.3),
  reso("plate", "note", size=0.3, material=0.85, decay=1.6, position=0.35, inharm=0.45, sympathetic=0.1, width=0.8,
       mix=0.45),
  expr(vel=[("cutoff", 0.35, "exp"), ("level", 0.15)]), fx(5, "reverb", size=0.5, damping=0.45, predelay=0.02, mix=0.18),
  wheel=("resodecay", 0.5), voices=12, velsens=0.7)

S("Breathing Organ", ["Keys"], "analog", ["life", "char"],
  "A drawbar-like organ that is never quite still: Life drifts every voice a few cents and breathes in its tone, like an "
  "old instrument warming up. The mod wheel speeds up the rotating chorus.",
  "Eine zugriegelartige Orgel, die nie ganz stillsteht: Leben lässt jede Stimme ein paar Cent driften und den Ton atmen, "
  "wie ein altes Instrument, das warm wird. Das Modrad beschleunigt den kreisenden Chorus.",
  an(shape=0.85, detune=0.05, cutoff=3200, reso=0.05, osc2oct=1, oscmix=0.45, sub=0.4, drive=0.2, keytrack=0.5),
  amp(0.004, 0.2, 1.0, 0.08), mod(0.002, 0.3, 0.0, 0.1, 0.0), lfo(0.7, 0.0, "sine", "pan"),
  life(0.45), char(alive=0.2, hard=0.15),
  fx(1, "chorus", rate=0.9, depth=0.45, mix=0.45, voices=3, drift=0.3, spread=0.85, lowcut=180.0),
  fx(5, "reverb", size=0.4, damping=0.55, predelay=0.01, mix=0.12),
  expr(vel=[("level", 0.15)], at=[("vibrato", 0.12, "s")]),
  wheel=("fx1p1", 0.6), voices=12, velsens=0.15)

S("Sympathetic Harp Keys", ["Keys", "Plucks"], "pluck", ["reso", "expr"],
  "A bright harp-like pluck that wakes a bank of sympathetic strings tuned to the chord you hold: chords keep shimmering "
  "after you let go. Velocity plucks harder; the mod wheel brings the sympathetic strings forward.",
  "Ein heller, harfenartiger Zupfer weckt Resonanzsaiten, gestimmt auf den gehaltenen Akkord: Akkorde schimmern nach dem "
  "Loslassen weiter. Anschlag zupft härter; das Modrad holt die Resonanzsaiten nach vorn.",
  pl(material=0.05, damping=0.3, bright=0.55, body=0.35, excite=0.45, pickpos=0.18, bodysize=0.45),
  amp(0.002, 3.0, 0.0, 1.0), mod(0.001, 0.4, 0.0, 0.3, 0.1),
  reso("string", "chord", size=0.85, material=0.6, decay=4.0, position=0.3, inharm=0.12, sympathetic=0.7, width=0.85,
       mix=0.35),
  expr(vel=[("cutoff", 0.35, "exp")], at=[("lfodepth", 0.3)]), lfo(0.13, 0.15, "sine", "pan"),
  fx(5, "reverb", size=0.55, damping=0.45, predelay=0.02, mix=0.15),
  wheel=("resomix", 0.15), voices=12, velsens=0.65)

S("Alive Tine Keys", ["Keys", "Piano"], "sampler", ["char", "life", "expr"],
  "The recorded tine electric piano with its character macros turned towards Alive and a little darker: slow drift, a "
  "living tone and a soft stereo bloom. Velocity brightens, the mod wheel deepens the tremolo.",
  "Das aufgenommene Zungen-E-Piano mit den Charakter-Makros Richtung Lebendig und etwas dunkler: langsame Drift, ein "
  "lebendiger Ton und ein weiches Stereo-Aufblühen. Anschlag hellt auf, das Modrad vertieft das Tremolo.",
  sm(start=0.0, tone=0.62), {"sampler.inst2": KEYS["tine"]},
  amp(0.001, 3.0, 0.7, 0.5), mod(0.001, 0.5, 0.2, 0.3, 0.0), lfo(4.5, 0.0, "sine", "pan"),
  char(bright=-0.2, alive=0.55, wide=0.3), life(0.25),
  expr(vel=[("cutoff", 0.3, "exp")]),
  fx(1, "chorus", rate=0.45, depth=0.5, mix=0.3, voices=3, drift=0.35, spread=0.85, lowcut=160.0),
  fx(5, "reverb", size=0.45, damping=0.5, predelay=0.02, mix=0.15),
  wheel=("lfo1depth", 0.5), voices=14, velsens=0.7)

S("Pressure Reed Keys", ["Keys", "Piano"], "sampler", ["expr"],
  "The recorded reed electric piano made for aftertouch: press into a chord and it growls (drive) and wavers (vibrato); "
  "velocity sets the bark. The mod wheel brings in a pulsing auto-pan.",
  "Das aufgenommene Zungen-E-Piano mit Stimmzungen, gemacht für Aftertouch: in einen Akkord drücken, und er knurrt (Drive) "
  "und schwebt (Vibrato); der Anschlag bestimmt das Bellen. Das Modrad bringt ein pulsierendes Auto-Pan.",
  sm(start=0.0, tone=0.7), {"sampler.inst2": KEYS["reed"]},
  amp(0.001, 3.0, 0.7, 0.4), mod(0.001, 0.5, 0.2, 0.3, 0.0), lfo(5.0, 0.0, "sine", "pan"),
  expr(vel=[("cutoff", 0.3, "exp")], at=[("drive", 0.45, "s"), ("vibrato", 0.12, "s")]),
  fx(0, "distortion", mode="tube", drive=0.15, mix=0.35, output=0.0),
  fx(5, "reverb", size=0.4, damping=0.5, predelay=0.02, mix=0.12),
  wheel=("lfo1depth", 0.6), voices=12, velsens=0.7)

# ======================================================================================================== Pads
S("Living Strings", ["Pads"], "analog", ["life", "char"],
  "An ensemble string pad that keeps moving while you hold it: Life gives every voice its own slow drift and breathing "
  "tone, Alive adds detune motion. The mod wheel opens the filter.",
  "Eine Streicherfläche, die sich beim Halten weiterbewegt: Leben gibt jeder Stimme eigene langsame Drift und einen "
  "atmenden Ton, Lebendig bringt Verstimmungsbewegung. Das Modrad öffnet das Filter.",
  an(shape=0.0, detune=0.28, cutoff=1600, reso=0.08, osc2oct=1, oscmix=0.4, sub=0.0, drive=0.1, keytrack=0.6),
  amp(0.6, 1.0, 0.9, 1.6), mod(0.6, 1.2, 0.8, 1.2, 0.08),
  life(0.8), char(alive=0.3, wide=0.2),
  fx(1, "chorus", rate=0.25, depth=0.6, mix=0.38, voices=4, drift=0.45, spread=0.85, lowcut=180.0),
  fx(5, "reverb", size=0.75, damping=0.45, predelay=0.03, mix=0.25),
  expr(vel=[("attack", -0.25)], at=[("cutoff", 0.3, "s")]),
  wheel=("timbre", 0.45), voices=12, velsens=0.3)

S("Halo Spiral Pad", ["Pads", "3D"], "wavetable", ["space", "tempo"],
  "A glassy wavetable pad that spirals upwards around your head over sixteen bars and starts again below (motion path "
  "Spiral Up, measured in bars: it follows the tempo). Best with headphones; the mod wheel speeds up the spiral.",
  "Eine gläserne Wavetable-Fläche, die in sechzehn Takten spiralförmig um den Kopf nach oben steigt und unten neu beginnt "
  "(Bewegungspfad Spirale aufwärts, in Takten: folgt dem Tempo). Am besten mit Kopfhörern; das Modrad beschleunigt die Spirale.",
  wt(table="glass", pos=0.4, warp=0.1, tone=0.55, unison=0.35, univoices=4, uniwidth=0.8, keytrack=0.35, lfopos=0.2),
  amp(1.0, 2.0, 0.9, 3.0), mod(1.2, 2.0, 0.6, 2.5, 0.12), lfo(0.1, 0.25, "tri", "timbre"),
  space("spiral", "16bars", size=0.55, distance=3.0, spread=70.0, room=0.35, roomsize=0.6, doppler=0.15),
  fx(5, "reverb", size=0.8, damping=0.45, predelay=0.04, mix=0.25),
  expr(at=[("cutoff", 0.3, "s")]),
  wheel=("pathrate", 0.35), voices=8, velsens=0.25)

S("Tempo Tide Pad", ["Pads"], "wavetable", ["tempo", "expr"],
  "A warm wavetable pad whose tone rises and falls over two bars and drifts through the stereo field over one bar - both "
  "locked to the tempo, they run with any song speed. The mod wheel deepens the tide.",
  "Eine warme Wavetable-Fläche, deren Ton über zwei Takte steigt und fällt und über einen Takt durch das Stereofeld "
  "wandert – beides im Tempo, passend zu jedem Songtempo. Das Modrad vertieft die Gezeiten.",
  wt(table="harmonic", pos=0.35, warp=0.15, tone=0.48, unison=0.4, univoices=6, uniwidth=0.85, keytrack=0.4, lfopos=0.0),
  amp(0.8, 1.5, 0.9, 2.2), mod(0.8, 1.5, 0.7, 1.8, 0.1), {"lfo.depth": 0.0},
  lfo_n(2, rate=0.25, sync="2/1", shape="sine", mode="free", depth=0.6),
  lfo_n(3, rate=0.5, sync="1/1", shape="tri", mode="free", depth=0.5),
  fx(1, "chorus", rate=0.25, depth=0.6, mix=0.3, voices=4, drift=0.45, spread=0.85, lowcut=180.0),
  fx(5, "reverb", size=0.75, damping=0.45, predelay=0.03, mix=0.25),
  expr(vel=[("attack", -0.2)], at=[("position", 0.3, "s")]),
  routes=[route("lfo2", "engine1", 0.3), route("lfo3", "pan", 0.35)], wheel=("lfo2depth", 0.4), voices=10, velsens=0.25)

S("Sympathy Choir", ["Pads", "Vocal"], "vocal", ["reso", "life"],
  "A soft 'ah' choir that sets a ring of sympathetic strings humming in the chord you hold, with Life drifting each voice. "
  "The strings keep ringing when the choir stops; the mod wheel adds a choir vibrato.",
  "Ein weicher 'Ah'-Chor bringt einen Kranz von Resonanzsaiten im gehaltenen Akkord zum Summen, Leben lässt jede Stimme "
  "driften. Die Saiten klingen nach, wenn der Chor endet; das Modrad gibt ein Chor-Vibrato.",
  vo(vowel=0.0, formant=0.0, breath=0.35, tension=0.35, vibdepth=0.12, vibrate=5.0, jitter=0.2),
  amp(0.8, 1.5, 0.9, 2.0), mod(0.5, 2.0, 0.5, 1.5, 0.0),
  reso("string", "chord", size=1.0, material=0.55, decay=5.0, position=0.35, inharm=0.1, sympathetic=0.85, width=0.9,
       mix=0.35),
  life(0.55),
  fx(5, "reverb", size=0.75, damping=0.45, predelay=0.03, mix=0.22),
  expr(at=[("position", 0.25, "s")]),
  routes=[route("vibrato", "pitch", 0.02, via="modwheel")], voices=10, velsens=0.25)

S("Weave Cluster 3D", ["Pads", "3D"], "cluster", ["space", "tempo"],
  "A wide cluster pad weaving left-right and front-back around the listener over eight bars (motion path Weave, in bars: "
  "it follows the tempo). Pressure lifts it overhead; the mod wheel widens the stack.",
  "Eine breite Cluster-Fläche, die in acht Takten links-rechts und vorn-hinten um den Hörer webt (Bewegungspfad Weben, in "
  "Takten: folgt dem Tempo). Druck hebt sie über den Kopf; das Modrad verbreitert den Stapel.",
  cl(detune=0.16, spread=0.7, tone=0.42, chord="sus2", stack=4, shape=0.3, side=0.5, reso=0.06, keytrack=0.5, drift=0.4),
  amp(1.0, 2.0, 0.9, 2.5), mod(1.0, 2.0, 0.7, 2.0, 0.1), lfo(0.09, 0.2, "tri", "timbre"),
  space("weave", "8bars", size=0.5, distance=2.5, spread=80.0, room=0.3, roomsize=0.55, doppler=0.15),
  fx(5, "reverb", size=0.7, damping=0.5, predelay=0.03, mix=0.2),
  routes=[route("pressure", "elevation", 0.35)],
  wheel=("charwide", 0.6), voices=8, velsens=0.25)

S("Bright Wide Pad", ["Pads"], "wavetable", ["char", "expr"],
  "A pad played with its character macros: pressure turns up Bright (tone, warp and FM open together) and the mod wheel "
  "turns up Wide (unison and layer spread). Soft and narrow at rest, huge when you push.",
  "Eine Fläche, gespielt über ihre Charakter-Makros: Druck dreht Hell auf (Ton, Warp und FM öffnen gemeinsam), das "
  "Modrad dreht Weit auf (Unisono- und Layer-Breite). In Ruhe weich und schmal, riesig, wenn man drückt.",
  wt(table="digital", pos=0.3, warp=0.2, tone=0.4, unison=0.35, univoices=6, uniwidth=0.5, keytrack=0.4, lfopos=0.12),
  amp(0.5, 1.5, 0.85, 1.8), mod(0.6, 1.5, 0.6, 1.5, 0.1), lfo(0.09, 0.2, "tri", "timbre"),
  char(bright=-0.25, wide=-0.2),
  fx(5, "reverb", size=0.7, damping=0.45, predelay=0.03, mix=0.22),
  expr(vel=[("attack", -0.2)]), routes=[route("pressure", "charbright", 0.7, curve="s")],
  wheel=("charwide", 0.7), voices=10, velsens=0.3)

S("Sub Floor Pad", ["Pads"], "analog", ["sub", "expr"],
  "A dark, warm pad with a round SUB layer one octave down that gives chords a floor without a bass part. Velocity brings "
  "in more of the sub; the mod wheel opens the pad above it.",
  "Eine dunkle, warme Fläche mit einer runden SUB-Ebene eine Oktave tiefer, die Akkorden ein Fundament gibt – ohne "
  "Bass-Part. Anschlag bringt mehr Sub; das Modrad öffnet die Fläche darüber.",
  an(shape=0.45, detune=0.2, cutoff=900, reso=0.1, oscmix=0.5, sub=0.0, drive=0.1, keytrack=0.5),
  amp(0.6, 1.2, 0.9, 1.8), mod(0.8, 1.5, 0.6, 1.5, 0.1), lfo(0.09, 0.2, "tri", "timbre"),
  sub(0.22, -1, "round", False), expr(vel=[("sub", 0.25), ("attack", -0.2)]),
  fx(1, "chorus", rate=0.25, depth=0.6, mix=0.35, voices=4, drift=0.45, spread=0.85, lowcut=200.0),
  fx(5, "reverb", size=0.7, damping=0.5, predelay=0.03, mix=0.2),
  wheel=("timbre", 0.45), voices=10, velsens=0.3, monobass=150.0)

S("Overhead Swell", ["Pads", "3D"], "vocal", ["space", "expr"],
  "Breathy voices that rise from in front of you, pass over your head and land behind you (motion path Over the Head, four "
  "bars, in tempo). Pressure swells them; the mod wheel adds a choir vibrato. Best with headphones.",
  "Hauchige Stimmen steigen vor dir auf, ziehen über den Kopf und landen hinter dir (Bewegungspfad Über den Kopf, vier "
  "Takte, im Tempo). Druck lässt sie anschwellen; das Modrad gibt ein Chor-Vibrato. Am besten mit Kopfhörern.",
  vo(vowel=1.0, formant=0.5, breath=0.5, tension=0.35, vibdepth=0.1, vibrate=4.8, jitter=0.2),
  amp(1.0, 1.5, 0.9, 2.5), mod(0.5, 2.0, 0.5, 1.5, 0.0), lfo(0.09, 0.2, "tri", "timbre"),
  space("overhead", "4bars", size=0.6, distance=2.0, spread=60.0, room=0.35, roomsize=0.6, doppler=0.2),
  fx(5, "reverb", size=0.75, damping=0.45, predelay=0.03, mix=0.22),
  expr(at=[("level", 0.25, "s")]),
  routes=[route("vibrato", "pitch", 0.02, via="modwheel")], voices=8, velsens=0.25)

# ======================================================================================================== Leads
S("Pressure Glide Lead", ["Leads"], "analog", ["glidepress", "expr"],
  "A singing mono saw lead built for legato: pressure stretches the glide (pressure glide) and the Expression page adds "
  "vibrato as you lean in. The mod wheel opens the filter.",
  "Ein singender Mono-Sägezahn-Lead für Legato: Druck dehnt das Gleiten (Glide-Druck), die Expression-Seite gibt beim "
  "Hineindrücken Vibrato dazu. Das Modrad öffnet das Filter.",
  an(shape=0.05, detune=0.1, cutoff=1700, reso=0.2, oscmix=0.45, sub=0.1, drive=0.25, keytrack=0.4),
  amp(0.005, 0.4, 0.85, 0.25), mod(0.002, 0.4, 0.4, 0.3, 0.2), MONO,
  {"glide": 0.09, "glide.mode": 1, "glide.rate": 0, "glide.press": 0.8},
  expr(vel=[("cutoff", 0.25)], at=[("vibrato", 0.25, "s")]), lfo(5.2, 0.0, "sine", "pitch"),
  fx(4, "delay", time=0.375, feedback=0.3, tone=0.5, pingpong=1, mix=0.16),
  fx(5, "reverb", size=0.5, damping=0.5, predelay=0.02, mix=0.14), comp(9, -12, 3.0, 2.0, 90),
  wheel=("timbre", 0.4), velsens=0.4)

S("Velocity Bite Lead", ["Leads"], "wavetable", ["expr", "char"],
  "A digital wavetable lead that bites harder the harder you play: velocity opens the tone and drives it (Hard), pressure "
  "adds vibrato. The mod wheel adds wavetable FM.",
  "Ein digitaler Wavetable-Lead, der härter zubeißt, je härter man spielt: Anschlag öffnet den Ton und treibt ihn an "
  "(Hart), Druck gibt Vibrato. Das Modrad fügt Wavetable-FM hinzu.",
  wt(table="digital", pos=0.5, warp=0.35, tone=0.5, unison=0.3, univoices=3, uniwidth=0.5, keytrack=0.5, lfopos=0.08),
  amp(0.004, 0.5, 0.8, 0.2), mod(0.002, 0.3, 0.3, 0.2, 0.25), {"voices": 4},
  expr(vel=[("cutoff", 0.45, "exp"), ("drive", 0.35, "exp")], at=[("vibrato", 0.22, "s")]),
  lfo(0.1, 0.1, "tri", "timbre"), fx(4, "delay", time=0.25, feedback=0.25, tone=0.5, pingpong=1, mix=0.14),
  fx(5, "reverb", size=0.45, damping=0.5, predelay=0.02, mix=0.12), comp(9, -12, 3.0, 2.0, 90),
  wheel=("engine13", 0.35), velsens=0.5)

S("Pendulum Lead", ["Leads", "3D"], "analog", ["space", "tempo"],
  "A bright pulse lead swinging like a pendulum from one side of your head to the other, once every two bars (motion path "
  "Pendulum, in tempo). Pressure adds vibrato; the mod wheel slows the swing. Best with headphones.",
  "Ein heller Puls-Lead, der wie ein Pendel alle zwei Takte von einer Seite des Kopfes zur anderen schwingt "
  "(Bewegungspfad Pendel, im Tempo). Druck gibt Vibrato; das Modrad verlangsamt das Schwingen. Am besten mit Kopfhörern.",
  an(shape=0.6, detune=0.08, cutoff=2000, reso=0.15, oscmix=0.4, sub=0.1, drive=0.2, keytrack=0.4),
  amp(0.005, 0.4, 0.85, 0.3), mod(0.002, 0.3, 0.4, 0.3, 0.15), {"voices": 4},
  space("pendulum", "2bars", size=0.6, distance=2.5, spread=25.0, room=0.25, roomsize=0.45, doppler=0.2, rolloff=0.5),
  expr(vel=[("cutoff", 0.25)], at=[("vibrato", 0.2, "s")]), lfo(0.1, 0.1, "tri", "timbre"),
  fx(5, "reverb", size=0.45, damping=0.5, predelay=0.02, mix=0.12),
  wheel=("pathrate", -0.3), velsens=0.4)

S("Living Flute Lead", ["Leads"], "vocal", ["life", "expr"],
  "A breathy flute-like voice whose pitch and breath drift a little with Life, like a real player. Pressure brings in "
  "vibrato and more breath, the mod wheel a wider vibrato.",
  "Eine hauchige, flötenartige Stimme, deren Tonhöhe und Atem mit Leben leicht driften, wie bei einem echten Spieler. "
  "Druck bringt Vibrato und mehr Atem, das Modrad ein weiteres Vibrato.",
  vo(vowel=3.2, formant=2.5, breath=0.6, tension=0.3, vibdepth=0.0, vibrate=5.2, jitter=0.15),
  amp(0.06, 0.4, 0.85, 0.3), mod(0.05, 0.4, 0.4, 0.3, 0.0), {"voices": 2},
  life(0.5), expr(vel=[("attack", -0.3)], at=[("vibrato", 0.25, "s"), ("spray", 0.3)]),
  lfo(0.1, 0.1, "tri", "timbre"), fx(5, "reverb", size=0.55, damping=0.5, predelay=0.02, mix=0.18),
  routes=[route("vibrato", "pitch", 0.03, via="modwheel")], velsens=0.4)

S("Bowed Body Lead", ["Leads"], "analog", ["reso", "expr"],
  "A soft saw bowing a long open tube tuned to each note: a woody, cello-like lead. Pressure presses the bow (the tube's "
  "material darkens to wood); the mod wheel slides the bowing position.",
  "Ein weicher Sägezahn streicht ein langes, offenes Rohr, auf jede Note gestimmt: ein holziger, celloartiger Lead. Druck "
  "drückt den Bogen (das Material wird holziger); das Modrad verschiebt die Streichposition.",
  an(shape=0.1, detune=0.04, cutoff=3200, reso=0.08, oscmix=0.0, sub=0.0, noise=0.12, drive=0.1, keytrack=0.8),
  amp(0.08, 0.4, 0.9, 0.4), mod(0.1, 0.5, 0.4, 0.4, 0.1), {"voices": 2},
  reso("open", "note", size=0.55, material=0.45, decay=1.4, position=0.2, inharm=0.04, sympathetic=0.0, width=0.5,
       mix=0.7, drive=0.15, press=0.7, slide=0.6),
  expr(vel=[("attack", -0.35)], at=[("vibrato", 0.22, "s")]), lfo(0.1, 0.1, "tri", "timbre"),
  fx(5, "reverb", size=0.55, damping=0.5, predelay=0.02, mix=0.16),
  routes=[route("pressure", "resomaterial", -0.3)], wheel=("resoposition", 0.3), velsens=0.4)

S("Hard Edge FM Lead", ["Leads"], "fm", ["char", "expr"],
  "A cutting FM lead voiced with the Hard macro (shorter attack, more feedback, more edge). Velocity adds bite, pressure "
  "vibrato; the mod wheel pushes Hard further into a snarl.",
  "Ein schneidender FM-Lead, abgestimmt mit dem Makro Hart (kürzerer Attack, mehr Feedback, mehr Kante). Anschlag gibt "
  "Biss, Druck Vibrato; das Modrad treibt Hart weiter bis zum Fauchen.",
  fm(algo=0, modratio=2.0, index=0.3, feedback=0.25, op1ratio=1, op2ratio=2, op1level=1.0, op2level=0.6, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.004, 0.5, 0.8, 0.2), mod(0.002, 0.4, 0.5, 0.2, 0.3), {"voices": 3},
  char(hard=0.35, bright=0.1), expr(vel=[("fm", 0.3, "exp")], at=[("vibrato", 0.22, "s")]),
  lfo(0.1, 0.08, "tri", "timbre"), fx(4, "delay", time=0.3, feedback=0.25, tone=0.45, pingpong=1, mix=0.14),
  fx(5, "reverb", size=0.45, damping=0.5, predelay=0.02, mix=0.12), comp(9, -12, 3.0, 2.0, 90),
  wheel=("charhard", 0.55), velsens=0.5)

# ======================================================================================================== Plucks
S("Glass Plate Pluck", ["Plucks"], "fm", ["reso", "expr"],
  "A short FM pluck striking a thin glass plate tuned to the note: crisp attack, then bell partials that bloom. Velocity "
  "strikes harder; the mod wheel lets the plate ring longer.",
  "Ein kurzer FM-Zupfer schlägt eine dünne Glasplatte an, auf die Note gestimmt: knackiger Anschlag, dann aufblühende "
  "Glocken-Teiltöne. Anschlag trifft härter; das Modrad lässt die Platte länger klingen.",
  fm(algo=0, modratio=3.0, index=0.25, feedback=0.05, op1ratio=1, op2ratio=3, op1level=1.0, op2level=0.5, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.002, 0.6, 0.0, 0.4), mod(0.001, 0.25, 0.0, 0.2, 0.6),
  reso("plate", "note", size=0.25, material=0.95, decay=1.4, position=0.3, inharm=0.4, sympathetic=0.0, width=0.85,
       mix=0.5),
  expr(vel=[("fm", 0.3, "exp"), ("level", 0.15)]), lfo(0.09, 0.15, "tri", "timbre"),
  fx(4, "delay", time=0.375, feedback=0.3, tone=0.45, pingpong=1, mix=0.15),
  fx(5, "reverb", size=0.5, damping=0.45, predelay=0.02, mix=0.15),
  wheel=("resodecay", 0.5), voices=10, velsens=0.7)

S("Ping-Pong Space Pluck", ["Plucks", "3D"], "pluck", ["space", "tempo"],
  "A bright pluck that bounces front to back on every half bar (motion path Ping-Pong front/back, in tempo) inside a small "
  "room. Velocity plucks harder; the mod wheel speeds up the bounce. Best with headphones.",
  "Ein heller Zupfer, der jeden halben Takt von vorn nach hinten springt (Bewegungspfad Ping-Pong vorn/hinten, im Tempo), "
  "in einem kleinen Raum. Anschlag zupft härter; das Modrad beschleunigt das Springen. Am besten mit Kopfhörern.",
  pl(material=0.2, damping=0.4, bright=0.42, body=0.35, excite=0.45, pickpos=0.18, bodysize=0.4),
  amp(0.002, 1.5, 0.0, 0.5), mod(0.001, 0.3, 0.0, 0.2, 0.1), lfo(0.09, 0.15, "tri", "timbre"),
  space("pingpong", "1/2", size=0.6, distance=2.0, spread=20.0, room=0.3, roomsize=0.4, doppler=0.2),
  expr(vel=[("cutoff", 0.35, "exp")]),
  fx(5, "reverb", size=0.4, damping=0.5, predelay=0.02, mix=0.12),
  wheel=("pathrate", 0.35), voices=10, velsens=0.7)

S("Kalimba Body", ["Plucks"], "sampler", ["reso", "expr"],
  "The recorded kalimba over a small wooden box body that rings with every note: warmer, rounder, a little hollow. "
  "Velocity brightens; the mod wheel opens the box (more body).",
  "Die aufgenommene Kalimba über einem kleinen Holzkasten, der mit jeder Note mitschwingt: wärmer, runder, etwas hohl. "
  "Anschlag hellt auf; das Modrad öffnet den Kasten (mehr Körper).",
  sm(start=0.0, tone=0.75, inst="kalimba"),
  amp(0.001, 2.5, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0), lfo(0.13, 0.15, "sine", "pan"),
  reso("closed", "note", size=0.4, material=0.35, decay=0.9, position=0.3, inharm=0.08, sympathetic=0.15, width=0.6,
       mix=0.35),
  expr(vel=[("cutoff", 0.3, "exp")]),
  fx(4, "delay", time=0.5, feedback=0.25, tone=0.4, pingpong=1, mix=0.12),
  fx(5, "reverb", size=0.45, damping=0.5, predelay=0.02, mix=0.14),
  wheel=("resomix", 0.35), voices=10, velsens=0.7)

S("Living Nylon", ["Plucks"], "pluck", ["life", "expr"],
  "A warm nylon string with a little Life: every note drifts and breathes a touch differently, like a hand-played guitar. "
  "Velocity plucks brighter; the mod wheel moves the pluck towards the bridge.",
  "Eine warme Nylonsaite mit etwas Leben: jede Note driftet und atmet ein wenig anders, wie eine von Hand gespielte "
  "Gitarre. Anschlag zupft heller; das Modrad verschiebt den Anschlag Richtung Steg.",
  pl(material=0.0, damping=0.3, bright=0.3, body=0.5, excite=0.42, pickpos=0.25, bodysize=0.6),
  amp(0.002, 2.5, 0.0, 0.35), mod(0.001, 0.4, 0.0, 0.3, 0.1), lfo(0.13, 0.12, "sine", "pan"),
  life(0.35), expr(vel=[("cutoff", 0.35, "exp")]),
  fx(5, "reverb", size=0.45, damping=0.5, predelay=0.02, mix=0.14),
  wheel=("engine6", -0.4), voices=12, velsens=0.65)

# ======================================================================================================== Arp
S("Tempo Gate Arp", ["Arp"], "wavetable", ["tempo", "expr"],
  "A glassy arpeggio chopped by a sixteenth-note gate that is locked to the tempo (LFO 2 on the level, retriggered with each "
  "note). Holds a chord, plays in time at any tempo; the mod wheel lifts the echoes.",
  "Ein gläsernes Arpeggio, zerhackt von einem Sechzehntel-Gate im Tempo (LFO 2 auf den Pegel, mit jeder Note neu "
  "gestartet). Akkord halten, spielt bei jedem Tempo im Takt; das Modrad hebt die Echos an.",
  wt(table="glass", pos=0.45, warp=0.15, tone=0.6, unison=0.25, univoices=3, uniwidth=0.6, keytrack=0.4, lfopos=0.15),
  amp(0.003, 0.4, 0.7, 0.2), mod(0.002, 0.25, 0.2, 0.2, 0.2), lfo(0.09, 0.15, "tri", "timbre"),
  lfo_n(2, rate=4.0, sync="1/16", shape="square", mode="retrig", depth=0.5),
  fx(4, "delay", time=0.375, feedback=0.3, tone=0.45, pingpong=1, mix=0.2),
  fx(5, "reverb", size=0.5, damping=0.45, predelay=0.02, mix=0.14),
  expr(vel=[("cutoff", 0.3)]),
  arp_block=("updown", 2, 0.55, "1/16"),
  routes=[route("lfo2", "level", -0.4)], wheel=("fx1p2", 0.4), voices=8, velsens=0.5)

S("Orbit Space Arp", ["Arp", "3D"], "fm", ["space", "tempo"],
  "Bell arpeggios circling the listener once per bar (motion path Circle, one bar, in tempo) at arm's length, in a soft "
  "room. Pressure pulls them closer; the mod wheel moves the orbit further out. Best with headphones.",
  "Glocken-Arpeggios, die den Hörer einmal pro Takt umkreisen (Bewegungspfad Kreis, ein Takt, im Tempo), auf Armlänge, in "
  "einem weichen Raum. Druck zieht sie näher; das Modrad rückt die Umlaufbahn weiter weg. Am besten mit Kopfhörern.",
  fm(algo=0, modratio=3.0, index=0.16, feedback=0.0, op1ratio=1, op2ratio=3, op1level=1.0, op2level=0.45,
     op3level=0.0, op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.002, 0.8, 0.0, 0.5), mod(0.001, 0.4, 0.0, 0.3, 0.5), lfo(0.09, 0.15, "tri", "timbre"),
  space("circle", "1bar", size=0.4, distance=1.5, spread=20.0, room=0.3, roomsize=0.45, doppler=0.15),
  fx(5, "reverb", size=0.5, damping=0.45, predelay=0.02, mix=0.14),
  expr(vel=[("fm", 0.25)]), arp_block=("up", 2, 0.5, "1/16"),
  routes=[route("pressure", "distance", -0.2)], wheel=("distance", 0.1), voices=8, velsens=0.5)

S("Resonant Comb Arp", ["Arp"], "pluck", ["reso", "tempo"],
  "A plucked-string arpeggio through the Resonator as a rack effect: a string body tuned to the chord you hold rings "
  "under every step. The mod wheel lets the body ring longer, the echo is a synced dotted eighth.",
  "Ein Arpeggio aus gezupften Saiten durch den Resonanzkörper als Rack-Effekt: ein Saitenkörper im gehaltenen Akkord "
  "klingt unter jedem Schritt. Das Modrad lässt den Körper länger klingen, das Echo ist eine synchrone punktierte Achtel.",
  pl(material=0.1, damping=0.45, bright=0.4, body=0.3, excite=0.45, pickpos=0.2, bodysize=0.4),
  amp(0.002, 0.8, 0.0, 0.3), mod(0.001, 0.2, 0.0, 0.1, 0.1), lfo(0.09, 0.15, "tri", "timbre"),
  reso_fx(0, "string", "chord", size=0.7, material=0.45, decay=2.5, position=0.25, inharm=0.1, sympathetic=0.6,
          width=0.8, mix=0.35),
  fx(4, "echo", sync="1/8.", feedback=0.3, mix=0.18),
  fx(5, "reverb", size=0.5, damping=0.5, predelay=0.02, mix=0.12),
  expr(vel=[("cutoff", 0.3)]), arp_block=("up", 2, 0.45, "1/16"),
  wheel=("fx1p3", 0.3), voices=8, velsens=0.5)

S("Living Bell Arp", ["Arp"], "fm", ["life", "char"],
  "Soft FM bells arpeggiated in sixteenths, with Life and Alive keeping every repeat a little different - drift, tone "
  "and pan move on their own. The mod wheel throws the echoes.",
  "Weiche FM-Glocken in Sechzehnteln arpeggiert; Leben und Lebendig machen jede Wiederholung ein wenig anders – Drift, "
  "Ton und Panorama bewegen sich von selbst. Das Modrad wirft die Echos.",
  fm(algo=0, modratio=2.0, index=0.2, feedback=0.0, op1ratio=1, op2ratio=2, op1level=1.0, op2level=0.4, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.002, 0.6, 0.0, 0.4), mod(0.001, 0.3, 0.0, 0.2, 0.45), lfo(0.09, 0.15, "tri", "timbre"),
  life(0.5), char(alive=0.4),
  fx(4, "delay", time=0.375, feedback=0.3, tone=0.45, pingpong=1, mix=0.18),
  fx(5, "reverb", size=0.5, damping=0.45, predelay=0.02, mix=0.14),
  expr(vel=[("fm", 0.25)]), arp_block=("updown", 2, 0.5, "1/16"),
  wheel=("fx1p2", 0.4), voices=8, velsens=0.5)

# ======================================================================================================== Textures
S("Swarm Field 3D", ["Textures", "3D"], "grain", ["space", "life"],
  "A cloud of glassy grains swarming around the listener (motion path Swarm, eight bars, in tempo) while Life scatters "
  "them further. Pressure brings the swarm closer; the mod wheel adds grain spray.",
  "Eine Wolke gläserner Grains, die um den Hörer schwärmt (Bewegungspfad Schwarm, acht Takte, im Tempo), Leben streut sie "
  "weiter. Druck holt den Schwarm näher; das Modrad gibt mehr Grain-Streuung.",
  gr(pos=0.4, size=140, density=40, spray=0.35, pitch=0, reverse=0.25, spread=1.0, scan=0.02, tone=0.6, inst="glass"),
  amp(1.0, 2.0, 0.9, 2.5), mod(1.0, 2.0, 0.6, 2.0, 0.1), lfo(0.09, 0.2, "tri", "timbre"),
  space("swarm", "8bars", size=0.6, distance=2.5, spread=90.0, room=0.35, roomsize=0.6, doppler=0.1),
  life(0.6), fx(5, "reverb", size=0.75, damping=0.45, predelay=0.03, mix=0.22),
  routes=[route("pressure", "distance", -0.2)],
  wheel=("engine4", 0.4), voices=8, velsens=0.25)

S("Bowed Metal Body", ["Textures"], "grain", ["reso", "expr"],
  "The recorded bowed metal, stretched into a slow grain bed and resonating in a closed tube tuned to the chord: dark, "
  "singing, endless. Pressure moves through the recording; the mod wheel changes the tube's material.",
  "Das aufgenommene gestrichene Metall, gedehnt zu einer langsamen Grain-Fläche und in einem gedackten Rohr im Akkord "
  "nachklingend: dunkel, singend, endlos. Druck wandert durch die Aufnahme; das Modrad ändert das Material des Rohrs.",
  gr(pos=0.35, size=320, density=36, spray=0.2, pitch=0, reverse=0.1, spread=0.8, scan=0.01, tone=0.55,
     inst="bowedmetal"),
  amp(1.2, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.6, 2.0, 0.0), lfo(0.09, 0.2, "tri", "timbre"),
  reso("closed", "chord", size=0.7, material=0.5, decay=3.0, position=0.3, inharm=0.1, sympathetic=0.4, width=0.8,
       mix=0.4),
  expr(at=[("position", 0.3, "s")]), fx(5, "reverb", size=0.8, damping=0.45, predelay=0.04, mix=0.25),
  wheel=("resomaterial", 0.6), voices=6, velsens=0.25)

S("Slow Weather", ["Textures"], "cluster", ["life", "char", "tempo"],
  "A cluster drone that keeps changing like weather: full Life, Alive turned up, and a tone swell locked to four-bar "
  "cycles of the tempo. Hold a note and listen; the mod wheel brightens the sky.",
  "Ein Cluster-Drone, der sich ändert wie das Wetter: volles Leben, Lebendig aufgedreht und ein Tonschwellen in "
  "Vier-Takt-Zyklen des Tempos. Eine Note halten und zuhören; das Modrad hellt den Himmel auf.",
  cl(detune=0.2, spread=0.85, tone=0.38, chord="fifth", stack=5, shape=0.25, side=0.6, reso=0.08, keytrack=0.5, drift=0.5),
  amp(1.5, 2.0, 0.9, 3.0), mod(1.5, 2.0, 0.7, 2.0, 0.1), {"lfo.depth": 0.0},
  life(1.0), char(alive=0.6, wide=0.3),
  lfo_n(2, rate=0.125, sync="4/1", shape="sine", mode="free", depth=0.6),
  fx(5, "reverb", size=0.85, damping=0.45, predelay=0.04, mix=0.28),
  expr(at=[("cutoff", 0.3, "s")]),
  routes=[route("lfo2", "timbre", 0.35)], wheel=("charbright", 0.6), voices=8, velsens=0.2)

S("Whisper Room", ["Textures", "3D"], "vocal", ["space", "expr"],
  "Breath and whispered vowels close to your ears, moving like someone walking around you (motion path Whisper, four bars, "
  "in tempo). Pressure adds breath; the mod wheel changes the vowel. Best with headphones.",
  "Atem und geflüsterte Vokale nah am Ohr, die sich bewegen, als ginge jemand um dich herum (Bewegungspfad Flüstern, vier "
  "Takte, im Tempo). Druck gibt mehr Atem; das Modrad ändert den Vokal. Am besten mit Kopfhörern.",
  vo(vowel=2.0, formant=0.5, breath=0.85, tension=0.25, vibdepth=0.05, vibrate=4.5, jitter=0.3),
  amp(0.6, 1.5, 0.85, 2.0), mod(0.5, 2.0, 0.5, 1.5, 0.0), lfo(0.09, 0.2, "tri", "timbre"),
  space("whisper", "4bars", size=0.5, distance=0.6, spread=30.0, room=0.2, roomsize=0.35, doppler=0.1),
  expr(at=[("spray", 0.35, "s")]), fx(5, "reverb", size=0.5, damping=0.5, predelay=0.02, mix=0.15),
  wheel=("engine1", 0.4), voices=6, velsens=0.25)

# ======================================================================================================== FX
S("Rising Halo Sweep", ["FX", "3D"], "wavetable", ["space", "tempo"],
  "A riser that spirals up and over your head in four bars (motion path Spiral Up, in tempo) while its tone opens. Hold a "
  "note for the build; the mod wheel speeds it up. Best with headphones.",
  "Ein Riser, der sich in vier Takten spiralförmig über den Kopf hebt (Bewegungspfad Spirale aufwärts, im Tempo), "
  "während sein Ton aufgeht. Für den Aufbau eine Note halten; das Modrad beschleunigt ihn. Am besten mit Kopfhörern.",
  wt(table="grit", pos=0.2, warp=0.2, tone=0.3, unison=0.5, univoices=6, uniwidth=0.9, keytrack=0.3, envpos=0.6,
     lfopos=0.0),
  amp(2.5, 1.0, 1.0, 1.5), mod(4.0, 1.0, 1.0, 1.5, 0.6), lfo(0.09, 0.15, "tri", "timbre"),
  space("spiral", "4bars", size=0.6, distance=2.5, spread=60.0, room=0.35, roomsize=0.6, doppler=0.25),
  fx(5, "reverb", size=0.8, damping=0.4, predelay=0.04, mix=0.25),
  expr(vel=[("attack", -0.3)]),
  wheel=("pathrate", 0.4), voices=4, velsens=0.25)

S("Resonant Impact", ["FX"], "analog", ["reso", "expr"],
  "A noise burst slamming into a large metal plate tuned to the note: a cinematic hit with a long, metallic ring. "
  "Velocity hits harder; the mod wheel shrinks the plate to a smaller, brighter ring.",
  "Ein Rauschstoß, der in eine große, auf die Note gestimmte Metallplatte kracht: ein filmischer Schlag mit langem, "
  "metallischem Nachklang. Anschlag schlägt härter; das Modrad verkleinert die Platte zu einem kleineren, helleren Klang.",
  an(shape=0.0, detune=0.0, cutoff=2500, reso=0.1, oscmix=0.0, sub=0.0, noise=0.8, drive=0.2, keytrack=0.6,
     osclevel=0.3),
  amp(0.001, 0.25, 0.0, 0.2), mod(0.001, 0.2, 0.0, 0.1, 0.4), lfo(0.09, 0.1, "tri", "timbre"), pitchenv(12, 0.05, 0.7),
  reso("plate", "note", size=0.9, material=0.8, decay=4.0, position=0.4, inharm=0.55, sympathetic=0.1, width=0.9,
       mix=0.7, drive=0.2),
  expr(vel=[("level", 0.25, "exp"), ("cutoff", 0.3)]), fx(5, "reverb", size=0.7, damping=0.5, predelay=0.02, mix=0.2),
  wheel=("resosize", -0.4), voices=4, velsens=0.7, guard=-8.0)

S("Distance Walk Drone", ["FX", "3D"], "cluster", ["space", "life"],
  "A low drone that walks towards you and away again over eight bars (motion path Distance Walk, in tempo): it swells, "
  "darkens in the distance, fills the room up close. The mod wheel lifts it above you.",
  "Ein tiefer Drone, der in acht Takten auf dich zu und wieder weg geht (Bewegungspfad Entfernungsgang, im Tempo): er "
  "schwillt an, wird in der Ferne dunkler, füllt nah den Raum. Das Modrad hebt ihn über dich.",
  cl(detune=0.15, spread=0.6, tone=0.35, chord="fifth", stack=3, shape=0.3, side=0.4, reso=0.1, keytrack=0.5, drift=0.4),
  amp(0.6, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.7, 2.0, 0.1), lfo(0.09, 0.2, "tri", "timbre"),
  space("walk", "8bars", size=0.35, distance=2.5, spread=60.0, room=0.4, roomsize=0.7, doppler=0.1, rolloff=0.35),
  life(0.5), fx(5, "reverb", size=0.8, damping=0.45, predelay=0.04, mix=0.22),
  wheel=("elevation", 0.4), voices=6, velsens=0.2)

# ======================================================================================================== Vocal
S("Living Choir", ["Vocal", "Pads"], "vocal", ["life", "char"],
  "A wide 'ah' choir where every singer drifts on their own (Life full up, Alive and Wide on): it sounds like a group, "
  "not a copy. The mod wheel adds a wider choir vibrato.",
  "Ein breiter 'Ah'-Chor, in dem jede Stimme eigen driftet (Leben voll, Lebendig und Weit an): klingt wie eine Gruppe, "
  "nicht wie eine Kopie. Das Modrad gibt ein weiteres Chor-Vibrato.",
  vo(vowel=0.0, formant=0.0, breath=0.4, tension=0.35, vibdepth=0.14, vibrate=5.0, jitter=0.25),
  amp(0.7, 1.5, 0.9, 1.8), mod(0.5, 2.0, 0.5, 1.5, 0.0), lfo(0.09, 0.2, "tri", "timbre"),
  life(0.9), char(alive=0.35, wide=0.4),
  fx(1, "chorus", rate=0.3, depth=0.5, mix=0.3, voices=3, drift=0.35, spread=0.85, lowcut=200.0),
  fx(5, "reverb", size=0.75, damping=0.45, predelay=0.03, mix=0.22),
  expr(at=[("vibrato", 0.15, "s")]),
  routes=[route("vibrato", "pitch", 0.02, via="modwheel")], voices=12, velsens=0.3)

S("Pressure Vowel", ["Vocal", "Leads"], "vocal", ["expr", "glidepress"],
  "A solo voice that opens its mouth as you press: aftertouch moves the vowel from 'oo' to 'ah' and velocity sets the "
  "volume. Overlapping notes slide, longer with pressure. The mod wheel adds vibrato.",
  "Eine Solostimme, die beim Drücken den Mund öffnet: Aftertouch bewegt den Vokal von 'U' nach 'A', der Anschlag "
  "bestimmt die Lautstärke. Überlappende Noten gleiten, mit Druck länger. Das Modrad gibt Vibrato.",
  vo(vowel=4.0, formant=0.5, breath=0.3, tension=0.45, vibdepth=0.0, vibrate=5.2, jitter=0.15),
  amp(0.05, 0.4, 0.9, 0.3), mod(0.05, 0.4, 0.4, 0.3, 0.0), MONO,
  {"glide": 0.06, "glide.mode": 1, "glide.rate": 0, "glide.press": 0.6},
  expr(vel=[("level", 0.3)], at=[("position", -0.5, "s")]), lfo(5.2, 0.0, "sine", "pitch"),
  fx(5, "reverb", size=0.55, damping=0.5, predelay=0.02, mix=0.18),
  wheel=("lfo1depth", 0.12), velsens=0.4)
