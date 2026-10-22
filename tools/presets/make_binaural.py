#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p71: binaural showcase part presets - headphone-first sounds built around the part's 3D stage ("pN.space.*", HRTF):
orbiting sources, wide pads circling the head, near / far whispers and voices, rain-like grain fields, plucks and bells
placed around the listener, percussion whose hits land at different places, basses with a centred low end ("Mono
below" keeps everything under it mono and unspatialised) and moving tops, arps that travel, plus Key Gravity and Voice
Control combinations.

Every preset carries the "3D" category tag (the library's 3D chip and search; the headphones pictogram in the list)
after its musical category, has an audible mod wheel routing and is level-calibrated with bank_curate.py (analyse /
calibrate / apply, target in bank_curation.json: -11 LUFS short-term max, true peak <= -1 dBTP). Only own synthesis and
the factory's CC0 / own recordings are used.

It writes only its own files (listed in binaural_files.json) and keeps an existing file's calibrated "gain".
  python3 tools/presets/make_binaural.py           # writes the presets
  python3 tools/presets/make_binaural.py --docs    # also rewrites the docs section (FACTORY-SOUNDS.md EN / DE)
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "resources", "presets", "factory")
MANIFEST = os.path.join(HERE, "binaural_files.json")
sys.path.insert(0, HERE)

from factory_parts import an, fm, wt, pl, cl, gr, vo, sm, amp, mod, lfo, arp, pitchenv, MONO, P as _P  # noqa: E402
from factory_parts_r3 import fx, chain, slot_prefix, lfo_n  # noqa: E402

# modulation matrix keys -> indices (src/core/mod/ModMatrix.cpp, append-only)
SRC = ["off", "lfo1", "lfo2", "lfo3", "ampenv", "modenv", "env3", "velocity", "key", "modwheel", "bend", "chpressure",
       "pressure", "slide", "notebend", "random", "macro1", "macro2", "macro3", "macro4", "seqlane", "morphx", "morphy",
       "motion1", "motion2", "motion3", "motion4", "padsweep", "padheld", "chordroot", "chordchange", "tension", "vibrato",
       "keybend", "voicelevel", "vowelx", "vowely", "voicebright", "voicenoise", "voicepitch", "voiceonset"]
DST = (["off", "pitch", "level", "pan", "timbre"] + ["engine%d" % (i + 1) for i in range(24)] +
       ["volume", "lfo1rate", "lfo1depth", "lfo2rate", "lfo2depth", "lfo3rate", "lfo3depth", "modenvamt"] +
       ["fx%dmix" % (k + 1) for k in range(6)] + ["fx%dp%d" % (k + 1, j + 1) for k in range(6) for j in range(4)] +
       ["azimuth", "elevation", "distance", "pathrate", "pathphase", "life", "morphx", "morphy",
        "resotune", "resosize", "resomaterial", "resodecay", "resoposition", "resomix",
        "charbright", "charhard", "charalive", "charwide", "sublevel", "glide", "ampattack", "ampdecay", "amprelease",
        "gravamt", "gravglide", "gravcurve", "gravformant", "gravmix"])
assert DST.index("azimuth") == 67 and DST.index("distance") == 69 and DST.index("gravamt") == 90
assert SRC.index("modwheel") == 9 and SRC.index("voicelevel") == 34 and SRC.index("vowelx") == 35
CURVE = {"lin": 0, "exp": 1, "log": 2, "s": 3, "steps": 4}
EXPR = ["off", "cutoff", "resonance", "level", "timbre", "attack", "decay", "release", "modenv", "lforate", "lfodepth",
        "vibrato", "glide", "fm", "position", "spray", "morphx", "morphy", "drive", "fxmix", "sub", "pan", "pitch", "life"]
PATH = ["off", "circle", "figure8", "flyby", "orbit", "pendulum", "spiral", "pingpong", "swarm", "whisper", "walk",
        "overhead", "weave"]
PATHLEN = ["1/16", "1/8", "1/4", "1/2", "1bar", "2bars", "4bars", "8bars", "16bars", "32bars", "1/4."]
PATH_EN = {"circle": "Circle", "figure8": "Figure-8", "flyby": "Fly-By", "orbit": "Random Orbit", "pendulum": "Pendulum",
           "spiral": "Spiral Up", "pingpong": "Ping-Pong F/B", "swarm": "Swarm", "whisper": "Whisper", "walk": "Distance Walk",
           "overhead": "Over the Head", "weave": "Weave", "off": "fixed position"}
PATH_DE = {"circle": "Kreis", "figure8": "Acht", "flyby": "Vorbeiflug", "orbit": "Zufallsorbit", "pendulum": "Pendel",
           "spiral": "Spirale aufwärts", "pingpong": "Ping-Pong v/h", "swarm": "Schwarm", "whisper": "Flüstern",
           "walk": "Entfernungsgang", "overhead": "Über den Kopf", "weave": "Wellenorbit", "off": "feste Position"}


def space(path="circle", length="2bars", rate=1.0, size=0.5, azimuth=0.0, elevation=0.0, distance=2.0, spread=40.0,
          room=0.3, roomsize=0.5, doppler=0.2, air=0.6, rolloff=0.7, lowkeep=120.0, glide=0.06, phase=0.0):
    """The part's binaural 3D stage, switched on; motion lengths in bars follow the tempo."""
    return {"space.on": 1, "space.path": PATH.index(path), "space.pathlen": PATHLEN.index(length), "space.pathrate": rate,
            "space.pathsize": size, "space.pathphase": phase, "space.azimuth": azimuth, "space.elevation": elevation,
            "space.distance": distance, "space.spread": spread, "space.room": room, "space.roomsize": roomsize,
            "space.doppler": doppler, "space.air": air, "space.rolloff": rolloff, "space.lowkeep": lowkeep,
            "space.glide": glide}


def expr(vel=(), at=()):
    out = {}
    for kind, slots in (("vel", vel), ("at", at)):
        for i, s in enumerate(slots):
            out["expr.%s.dst%d" % (kind, i + 1)] = EXPR.index(s[0])
            out["expr.%s.amt%d" % (kind, i + 1)] = s[1]
            out["expr.%s.curve%d" % (kind, i + 1)] = CURVE[s[2] if len(s) > 2 else "lin"]
    return out


def route(src, dst, amt, via="off", curve="lin"):
    return (src, dst, amt, via, curve)


def grav(amt, target, glide, curve, formant, mix=1.0, quality=1):
    """Key Gravity (p66): pulls the partials towards the key."""
    return {"grav.amt": amt, "grav.target": target, "grav.glide": glide, "grav.curve": curve, "grav.formant": formant,
            "grav.mix": mix, "grav.quality": quality}


def sub(level, oct=-1, shape="sine"):
    return {"sub.level": level, "sub.oct": 0 if oct == -1 else 1, "sub.shape": {"sine": 0, "round": 1, "square": 2}[shape],
            "sub.direct": 1}


def char(bright=0.0, hard=0.0, alive=0.0, wide=0.0):
    return {k: v for k, v in (("char.bright", bright), ("char.hard", hard), ("char.alive", alive), ("char.wide", wide)) if v}


def rev(size=0.6, mix=0.18, damping=0.45, predelay=0.02):
    return fx(5, "reverb", size=size, damping=damping, predelay=predelay, mix=mix)


def echo(sync="1/8.", fb=0.3, mix=0.16):
    return fx(4, "echo", sync=sync, feedback=fb, mix=mix)


def chorus(rate=0.3, depth=0.5, mix=0.3):
    return fx(1, "chorus", rate=rate, depth=depth, mix=mix, voices=3, drift=0.3, spread=0.85, lowcut=320.0)


def drive(amount=0.25, mix=0.5, mode="tube"):
    return fx(0, "distortion", mode=mode, drive=amount, mix=mix, output=0.0)


def comp(thresh=-14.0, ratio=3.0, attack=2.0, release=90.0, makeup=0.0):
    return fx(9, "compressor", thresh=thresh, ratio=ratio, attack=attack, release=release, knee=6.0, makeup=makeup, mix=1.0)


GUARD = {"thresh": -7.0, "ratio": 8.0, "attack": 0.1, "release": 120.0, "knee": 6.0, "makeup": 0.0, "mix": 1.0}
CAT_PREFIX = {"Bass": "bass", "Keys": "keys", "Pads": "pads", "Leads": "leads", "Plucks": "plucks", "Drums": "drums",
              "FX": "fx", "Arp": "arp", "Textures": "textures", "Vocal": "vocal"}
PRESETS = []


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def B(name, cat, engine, en, de, *dicts, wheel, routes=(), arp_block=None, guard=None, extra_tags=(), gain=6.0, **kw):
    """One binaural part preset: tags = [category, "3D", ...]; `wheel` = (target, amount[, curve])."""
    fxs = [d for d in dicts if isinstance(d, dict) and "__fx__" in d]
    rest = [d for d in dicts if not (isinstance(d, dict) and "__fx__" in d)]
    n = 0
    for n, f in enumerate(chain(fxs), 1):
        rest.append({slot_prefix(n - 1) + k: v for k, v in f["params"].items()})
    if guard is not None:  # a fast peak compressor at the end of the rack (near passes, grain bursts)
        g = dict(GUARD, thresh=guard[0], makeup=guard[1]) if isinstance(guard, tuple) else dict(GUARD, thresh=guard)
        rest.append(dict({slot_prefix(n) + "type": 10}, **{slot_prefix(n) + "compressor." + k: v for k, v in g.items()}))
    rr = list(routes)
    # like every 3D factory preset: slide lifts the source, pressure pulls it closer (MPE / aftertouch)
    if not any(r[0] == "slide" for r in rr):
        rr.append(route("slide", "elevation", 0.3))
    if not any(r[0] == "pressure" and r[1] == "distance" for r in rr):
        rr.append(route("pressure", "distance", -0.1))
    rr = rr + [route("modwheel", wheel[0], wheel[1], "off", wheel[2] if len(wheel) > 2 else "lin")]
    m = {}
    for i, (src, dst, amt, via, curve) in enumerate(rr):
        s = "mod.s%d." % (i + 1)
        m.update({s + "src": SRC.index(src), s + "via": SRC.index(via), s + "dst": DST.index(dst), s + "amt": amt,
                  s + "curve": CURVE[curve]})
    rest.append(m)
    tags = [cat, "3D"] + list(extra_tags)
    if cat == "Bass" and not any("monobass" in d for d in rest):
        rest.append({"monobass": 150.0})
    if arp_block is not None:
        rest.append(arp(arp_block[0], arp_block[1], arp_block[2], arp_block[3], latch=0))
    p = _P(name, tags, engine, en, *rest, **kw)
    p["params"].setdefault("glide", 0.0)
    p["params"]["gain"] = gain
    p["de"] = de
    p["file"] = "%s-%s.ohpepreset" % (CAT_PREFIX[cat], slug(name))
    sp = p["params"]
    p["path"] = PATH[sp["space.path"]]
    PRESETS.append(p)
    return p


HP_EN = " Best with headphones."
HP_DE = " Am besten mit Kopfhörern."

# ============================================================================================================ Pads
B("Halo Carousel", "Pads", "wavetable",
  "A glassy unison pad that circles your head once every eight bars at a few steps' distance, its wide image turning "
  "with it. Pressure draws it closer; the mod wheel speeds the carousel up." + HP_EN,
  "Eine gläserne Unisono-Fläche, die alle acht Takte einmal in einigen Schritten Abstand um den Kopf kreist; ihr breites "
  "Bild dreht sich mit. Druck holt sie näher; das Modrad beschleunigt das Karussell." + HP_DE,
  wt(table="glass", pos=0.35, warp=0.12, tone=0.55, unison=0.4, univoices=4, uniwidth=0.85, keytrack=0.35, lfopos=0.18),
  amp(1.2, 2.0, 0.9, 3.0), mod(1.5, 2.0, 0.6, 2.5, 0.1), lfo(0.08, 0.22, "tri", "timbre"),
  space("circle", "8bars", size=0.5, distance=2.6, spread=90.0, room=0.35, roomsize=0.6, doppler=0.1),
  rev(0.8, 0.2), expr(at=[("cutoff", 0.25, "s")]),
  routes=[route("pressure", "distance", -0.08)], wheel=("pathrate", 0.45), voices=10, velsens=0.25)

B("Choir Around You", "Pads", "vocal",
  "A soft 'aah' choir drifting on a random orbit around the listener over sixteen bars, sometimes above, sometimes "
  "behind. The mod wheel turns the vowel towards 'ooh'." + HP_EN,
  "Ein weicher 'Aah'-Chor, der in sechzehn Takten auf einem Zufallsorbit um dich treibt, mal oben, mal hinter dir. Das "
  "Modrad dreht den Vokal Richtung 'Uuh'." + HP_DE,
  vo(vowel=0.4, formant=0.0, breath=0.3, tension=0.4, vibdepth=0.12, vibrate=4.8, jitter=0.2),
  amp(0.9, 2.0, 0.9, 2.5), mod(1.0, 2.0, 0.5, 2.0, 0.0), lfo(0.07, 0.18, "tri", "timbre"),
  space("orbit", "16bars", size=0.55, distance=2.4, spread=70.0, room=0.35, roomsize=0.6, doppler=0.1),
  chorus(0.2, 0.5, 0.3), rev(0.75, 0.18),
  wheel=("engine1", 0.45), voices=10, velsens=0.2)

B("Cathedral Ring", "Pads", "cluster",
  "Major-seventh stacks hanging in a high vault: the pad passes over your head and back every eight bars while a slow "
  "drift keeps the stack shimmering. The mod wheel opens the tone." + HP_EN,
  "Große Septakkord-Schichten in einem hohen Gewölbe: die Fläche zieht alle acht Takte über den Kopf und zurück, ein "
  "langsamer Drift lässt sie schimmern. Das Modrad öffnet den Klang." + HP_DE,
  cl(detune=0.22, spread=0.85, tone=0.45, chord="maj7", stack=4, shape=0.2, side=0.7, reso=0.15, keytrack=0.4, drift=0.35),
  amp(1.5, 2.5, 0.9, 3.5), mod(2.0, 2.0, 0.6, 3.0, 0.15), lfo(0.06, 0.2, "tri", "timbre"),
  space("overhead", "8bars", size=0.6, distance=3.0, spread=80.0, elevation=10.0, room=0.45, roomsize=0.75, doppler=0.1),
  rev(0.85, 0.22),
  wheel=("timbre", 0.4), voices=8, velsens=0.2)

B("Slow Planet", "Pads", "analog",
  "A warm detuned saw pad weaving around the head on a wave-shaped orbit that rises and dips over sixteen bars. Velocity "
  "softens the attack; the mod wheel brings the planet in from far away." + HP_EN,
  "Eine warme, verstimmte Sägezahnfläche, die auf einem wellenförmigen Orbit um den Kopf webt und in sechzehn Takten "
  "steigt und sinkt. Anschlag macht den Einsatz weicher; das Modrad holt den Planeten aus der Ferne heran." + HP_DE,
  an(shape=0.05, detune=0.3, cutoff=1500, reso=0.18, oscmix=0.5, sub=0.15, drive=0.15, keytrack=0.4),
  amp(1.4, 2.0, 0.85, 3.0), mod(2.5, 3.0, 0.5, 3.0, 0.2), lfo(0.11, 0.2, "tri", "timbre"),
  space("weave", "16bars", size=0.55, distance=3.5, spread=75.0, room=0.35, roomsize=0.6, doppler=0.15),
  chorus(0.25, 0.55, 0.32), rev(0.8, 0.18), expr(vel=[("attack", -0.25)]),
  wheel=("distance", -0.18), voices=10, velsens=0.3)

B("Glass Horizon", "Pads", "grain",
  "Long glass grains spread into a wide horizon that walks away and comes back towards you over eight bars. The mod "
  "wheel scatters the grains into a sparkle." + HP_EN,
  "Lange Glas-Grains, zu einem weiten Horizont gefächert, der sich in acht Takten entfernt und wieder auf dich zukommt. "
  "Das Modrad streut die Grains zu einem Funkeln." + HP_DE,
  gr(inst="glass", pos=0.25, size=180, density=26, spray=0.25, pitch=0, reverse=0.2, spread=0.9, scan=0.15, tone=0.6),
  amp(1.0, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.5, 2.0, 0.0), lfo(0.08, 0.15, "tri", "timbre"),
  space("walk", "8bars", size=0.45, distance=2.5, spread=100.0, room=0.4, roomsize=0.7, doppler=0.05, rolloff=0.45),
  rev(0.8, 0.2),
  wheel=("engine4", 0.45), voices=8, velsens=0.2)

B("Gravity Halo", "Pads", "analog",
  "A breathy noise-and-saw pad pulled into the key by Key Gravity, spiralling up around you over sixteen bars. The mod "
  "wheel releases the gravity, so the pad drifts free of the key." + HP_EN,
  "Eine hauchige Fläche aus Rauschen und Sägezahn, die Key Gravity in die Tonart zieht und in sechzehn Takten "
  "spiralförmig um dich steigt. Das Modrad löst die Gravitation, die Fläche treibt aus der Tonart." + HP_DE,
  an(shape=0.1, detune=0.2, cutoff=2200, reso=0.2, oscmix=0.4, sub=0.0, noise=0.35, drive=0.1, keytrack=0.5),
  amp(1.5, 2.0, 0.9, 3.0), mod(2.0, 2.0, 0.6, 2.5, 0.15), lfo(0.09, 0.15, "tri", "timbre"),
  grav(0.85, 1, 0.4, 0.6, 0.35, 1.0, 2),
  space("spiral", "16bars", size=0.55, distance=2.8, spread=70.0, room=0.35, roomsize=0.65, doppler=0.12),
  rev(0.8, 0.2),
  wheel=("gravamt", -0.7), voices=8, velsens=0.2, extra_tags=("Textures",))

B("Morning Mist", "Pads", "fm",
  "Soft two-operator FM tones tracing a slow figure-eight through the space around your head, close in front, further "
  "behind. The mod wheel adds bell-like brightness." + HP_EN,
  "Weiche Zwei-Operator-FM-Töne, die eine langsame Acht um den Kopf ziehen, vorne nah, hinten weiter weg. Das Modrad "
  "gibt glockige Helligkeit dazu." + HP_DE,
  fm(algo=0, modratio=2.0, index=0.12, feedback=0.05, op1ratio=1, op2ratio=2, op1level=1.0, op2level=0.35, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(1.2, 2.0, 0.9, 3.0), mod(1.5, 2.5, 0.5, 2.5, 0.3), lfo(0.1, 0.12, "tri", "timbre"),
  space("figure8", "8bars", size=0.6, distance=2.2, spread=60.0, room=0.35, roomsize=0.55, doppler=0.12),
  chorus(0.22, 0.5, 0.3), rev(0.75, 0.2),
  wheel=("engine3", 0.35), voices=10, velsens=0.25)

B("Bowed Ring", "Pads", "sampler",
  "Bowed metal turning slowly against the clock around the listener (circle, four bars, reversed), with a long ringing "
  "room. The mod wheel darkens the bow." + HP_EN,
  "Gestrichenes Metall, das sich langsam gegen den Uhrzeigersinn um dich dreht (Kreis, vier Takte, rückwärts), in einem "
  "lang klingenden Raum. Das Modrad macht den Strich dunkler." + HP_DE,
  sm(inst="bowedmetal", start=0.0, tone=0.6),
  amp(0.8, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.5, 2.0, 0.0), lfo(0.12, 0.15, "sine", "pan"),
  space("circle", "4bars", rate=-1.0, size=0.45, distance=2.2, spread=50.0, room=0.4, roomsize=0.7, doppler=0.15),
  rev(0.85, 0.22),
  wheel=("engine2", -0.4), voices=8, velsens=0.3)

# ========================================================================================================= Textures
B("Rain Canopy", "Textures", "grain",
  "Dense short grains falling like rain on leaves above and around you: a swarm of drops spread over the whole sphere, "
  "slightly overhead. The mod wheel thickens the downpour." + HP_EN,
  "Dichte kurze Grains, die wie Regen auf Blätter über und um dich fallen: ein Schwarm aus Tropfen über die ganze Kugel, "
  "leicht von oben. Das Modrad verdichtet den Guss." + HP_DE,
  gr(inst="glockenspiel", pos=0.05, size=30, density=200, spray=0.95, pitch=0, reverse=0.1, spread=1.0, scan=0.6, tone=0.4),
  amp(0.6, 1.5, 0.9, 2.5), mod(0.5, 1.5, 0.5, 2.0, 0.0), lfo(0.15, 0.2, "sh", "timbre"),
  space("swarm", "8bars", size=0.7, distance=2.0, spread=150.0, elevation=25.0, room=0.3, roomsize=0.55, doppler=0.05),
  rev(0.6, 0.14),
  comp(-30.0, 2.0, 5.0, 120.0, 12.0),
  wheel=("engine3", 0.3), voices=6, velsens=0.2)

B("Night Insects", "Textures", "grain",
  "Tiny bright chirps of plucked strings buzzing around your head on a fast swarm path, near and far at once. The mod "
  "wheel scatters them wider." + HP_EN,
  "Winzige helle Zirplaute gezupfter Saiten, die auf einem schnellen Schwarmpfad um den Kopf schwirren, nah und fern "
  "zugleich. Das Modrad streut sie weiter." + HP_DE,
  gr(inst="harp", pos=0.02, size=14, density=40, spray=0.8, pitch=12, reverse=0.0, spread=1.0, scan=1.0, tone=0.55),
  amp(0.4, 1.0, 0.85, 1.5), mod(0.5, 1.0, 0.5, 1.0, 0.0), lfo(3.5, 0.3, "sh", "amp"),
  space("swarm", "4bars", size=0.8, distance=1.6, spread=120.0, room=0.25, roomsize=0.45, doppler=0.2),
  rev(0.55, 0.14),
  wheel=("engine4", 0.4), guard=-9.0, voices=6, velsens=0.2)

B("Close Whispers", "Textures", "vocal",
  "Breath and whispered vowels right at your ears, slipping from one side to the other like someone leaning in "
  "(motion path Whisper, four bars). Pressure adds breath; the mod wheel changes the vowel." + HP_EN,
  "Atem und geflüsterte Vokale direkt am Ohr, die von einer Seite zur anderen gleiten, als beuge sich jemand zu dir "
  "(Bewegungspfad Flüstern, vier Takte). Druck gibt mehr Atem; das Modrad ändert den Vokal." + HP_DE,
  vo(vowel=1.5, formant=0.6, breath=0.92, tension=0.2, vibdepth=0.0, vibrate=4.5, jitter=0.35),
  amp(0.4, 1.5, 0.85, 1.8), mod(0.5, 2.0, 0.5, 1.5, 0.0), lfo(0.13, 0.25, "tri", "timbre"),
  space("whisper", "4bars", size=0.55, distance=0.45, spread=25.0, room=0.15, roomsize=0.3, doppler=0.05, air=0.3),
  expr(at=[("spray", 0.3, "s")]),
  wheel=("engine1", 0.5), voices=6, velsens=0.3, extra_tags=("Vocal",))

B("Distant Voices", "Textures", "vocal",
  "Hummed voices far off in a large space, wandering nearer and further over sixteen bars. The mod wheel brings them "
  "into the room with you." + HP_EN,
  "Gesummte Stimmen weit weg in einem großen Raum, die in sechzehn Takten näher und ferner wandern. Das Modrad holt sie "
  "zu dir in den Raum." + HP_DE,
  vo(vowel=3.6, formant=-0.3, breath=0.4, tension=0.35, vibdepth=0.1, vibrate=4.6, jitter=0.25),
  amp(1.0, 2.0, 0.9, 3.0), mod(1.0, 2.0, 0.5, 2.0, 0.0), lfo(0.07, 0.2, "tri", "timbre"),
  space("walk", "16bars", size=0.5, distance=4.5, spread=60.0, room=0.55, roomsize=0.85, doppler=0.05, rolloff=0.4),
  rev(0.85, 0.18),
  wheel=("distance", -0.2), voices=8, velsens=0.2, extra_tags=("Vocal",))

B("Ember Field", "Textures", "grain",
  "Short crackling marimba grains like embers, popping up at random places around you on a wandering orbit. The mod wheel "
  "stretches the crackle into glowing tones." + HP_EN,
  "Knisternde Marimba-Grains wie Glut, die auf einem wandernden Orbit an zufälligen Stellen um dich aufflackern. Das "
  "Modrad dehnt das Knistern zu glühenden Tönen." + HP_DE,
  gr(inst="marimba", pos=0.0, size=35, density=40, spray=0.8, pitch=0, reverse=0.4, spread=1.0, scan=0.3, tone=0.5),
  amp(0.5, 1.5, 0.9, 2.0), mod(0.5, 1.5, 0.5, 1.5, 0.0), lfo(0.2, 0.2, "sh", "timbre"),
  space("orbit", "4bars", size=0.7, distance=1.8, spread=130.0, room=0.3, roomsize=0.5, doppler=0.1),
  rev(0.6, 0.15),
  wheel=("engine2", 0.5), voices=6, velsens=0.2)

B("Wind Spiral", "Textures", "analog",
  "Filtered wind that spirals up and over your head every four bars and starts again at your feet, its resonance "
  "whistling softly. The mod wheel opens the gusts." + HP_EN,
  "Gefilterter Wind, der alle vier Takte spiralförmig über den Kopf steigt und unten neu beginnt, die Resonanz pfeift "
  "leise. Das Modrad öffnet die Böen." + HP_DE,
  an(shape=0.0, detune=0.0, cutoff=900, reso=0.55, oscmix=0.0, sub=0.0, noise=1.0, drive=0.0, keytrack=0.8),
  amp(0.8, 1.5, 0.9, 2.5), mod(2.0, 2.0, 0.5, 2.0, 0.25), lfo(0.23, 0.35, "sine", "timbre"),
  space("spiral", "4bars", size=0.6, distance=2.2, spread=90.0, room=0.3, roomsize=0.55, doppler=0.2),
  rev(0.7, 0.16),
  wheel=("engine3", 0.35), voices=6, velsens=0.2, extra_tags=("FX",))

B("Voice Halo", "Textures", "vocal",
  "A sung halo whose place follows your own voice: sing into the input and your vowel turns it around your head, your "
  "level draws it nearer. Without input a gently orbiting choir. The mod wheel speeds the orbit." + HP_EN,
  "Ein gesungener Halo, dessen Ort deiner Stimme folgt: singe in den Eingang, dein Vokal dreht ihn um den Kopf, deine "
  "Lautstärke holt ihn näher. Ohne Eingang ein sanft kreisender Chor. Das Modrad beschleunigt den Orbit." + HP_DE,
  vo(vowel=1.0, formant=0.2, breath=0.35, tension=0.45, vibdepth=0.1, vibrate=5.0, jitter=0.15),
  amp(0.8, 2.0, 0.9, 2.5), mod(1.0, 2.0, 0.5, 2.0, 0.0), lfo(0.08, 0.15, "tri", "timbre"),
  space("circle", "8bars", size=0.5, distance=2.4, spread=60.0, room=0.35, roomsize=0.6, doppler=0.1),
  chorus(0.25, 0.5, 0.28), rev(0.75, 0.18),
  routes=[route("vowelx", "azimuth", 0.35), route("voicelevel", "distance", -0.12)],
  wheel=("pathrate", 0.45), voices=8, velsens=0.2, extra_tags=("Vocal",))

# ============================================================================================================== Keys
B("Orbiting Tines", "Keys", "fm",
  "A bell-tine electric piano drifting in a small circle in front of you every two bars, like the instrument sways as "
  "you play. Velocity adds bite; the mod wheel adds tine brightness." + HP_EN,
  "Ein glockiges Tine-E-Piano, das alle zwei Takte in einem kleinen Kreis vor dir schwebt, als schwanke das Instrument "
  "beim Spielen. Anschlag gibt Biss; das Modrad macht die Tines heller." + HP_DE,
  fm(algo=0, modratio=1.0, index=0.18, feedback=0.0, op1ratio=1, op2ratio=14, op1level=1.0, op2level=0.25, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0, velidx=0.6),
  amp(0.003, 1.6, 0.25, 0.6), mod(0.001, 0.5, 0.0, 0.4, 0.45), lfo(0.12, 0.1, "sine", "pan"),
  space("circle", "2bars", size=0.25, distance=1.6, spread=40.0, room=0.3, roomsize=0.45, doppler=0.1),
  chorus(0.4, 0.4, 0.25), rev(0.55, 0.14), expr(vel=[("fm", 0.35, "exp")]),
  wheel=("engine3", 0.35), voices=10, velsens=0.55)

B("Glass Pendulum", "Keys", "sampler",
  "Glass harmonica tones swinging from one ear to the other on a slow pendulum, each phrase landing somewhere new. The "
  "mod wheel softens the glass." + HP_EN,
  "Glasharmonika-Töne, die auf einem langsamen Pendel von einem Ohr zum anderen schwingen; jede Phrase landet woanders. "
  "Das Modrad macht das Glas weicher." + HP_DE,
  sm(inst="glass", start=0.0, tone=0.62),
  amp(0.01, 2.0, 0.6, 1.2), mod(0.01, 1.0, 0.0, 0.5, 0.0), lfo(0.1, 0.1, "sine", "timbre"),
  space("pendulum", "2bars", size=0.6, distance=1.8, spread=35.0, room=0.35, roomsize=0.5, doppler=0.12),
  rev(0.6, 0.16),
  wheel=("engine2", -0.4), voices=10, velsens=0.5)

B("Vibes in the Round", "Keys", "sampler",
  "Vibraphone bouncing between the front and the back of your head every half bar, the motor-like shimmer becoming "
  "real movement. The mod wheel moves the vibes further away." + HP_EN,
  "Vibraphon, das jeden halben Takt zwischen vorn und hinten springt; das Motor-Flimmern wird zur echten Bewegung. Das "
  "Modrad rückt das Vibraphon weiter weg." + HP_DE,
  sm(inst="vibraphone", start=0.0, tone=0.6),
  amp(0.003, 2.2, 0.0, 1.0), mod(0.01, 1.0, 0.0, 0.5, 0.0), lfo(5.0, 0.12, "sine", "amp"),
  space("pingpong", "1/2", size=0.55, distance=1.7, spread=30.0, room=0.3, roomsize=0.45, doppler=0.1),
  rev(0.55, 0.14),
  wheel=("distance", 0.25), voices=10, velsens=0.55)

B("Ceiling Harpsichord", "Keys", "sampler",
  "A harpsichord that rises over your head and settles behind you every four bars, the room answering from all sides. "
  "The mod wheel darkens the quill." + HP_EN,
  "Ein Cembalo, das alle vier Takte über den Kopf steigt und sich hinter dir niederlässt; der Raum antwortet von allen "
  "Seiten. Das Modrad macht den Kiel dunkler." + HP_DE,
  sm(inst="harpsichord", start=0.0, tone=0.5),
  amp(0.004, 1.8, 0.0, 0.6), mod(0.01, 1.0, 0.0, 0.5, 0.0), lfo(0.1, 0.08, "sine", "pan"),
  space("overhead", "4bars", size=0.55, distance=1.9, spread=40.0, room=0.4, roomsize=0.55, doppler=0.12),
  rev(0.6, 0.16),
  wheel=("engine2", -0.4), voices=10, velsens=0.5)

B("Rotor Organ", "Keys", "cluster",
  "A drawbar-style organ whose sound really spins around your head (circle, one bar). The mod wheel throws the rotor "
  "into the fast speed." + HP_EN,
  "Eine Zugriegel-artige Orgel, deren Klang wirklich um den Kopf rotiert (Kreis, ein Takt). Das Modrad schaltet den "
  "Rotor in die schnelle Stufe." + HP_DE,
  cl(detune=0.04, spread=0.3, tone=0.55, chord="octave", stack=3, shape=0.0, side=0.3, reso=0.05, keytrack=0.5, drift=0.05),
  amp(0.005, 0.3, 1.0, 0.12), mod(0.001, 0.08, 0.0, 0.05, 0.2), lfo(6.2, 0.04, "sine", "pitch"),
  space("circle", "1bar", size=0.3, distance=1.4, spread=30.0, room=0.3, roomsize=0.4, doppler=0.5),
  drive(0.2, 0.35), rev(0.45, 0.12),
  wheel=("pathrate", 1.0), voices=10, velsens=0.15)

# ============================================================================================================ Plucks
B("Kalimba Circle", "Plucks", "sampler",
  "Kalimba notes placed around a circle by pitch: low notes on your left, high notes behind you to the right, the whole "
  "ring turning once per bar. The mod wheel brightens the tines." + HP_EN,
  "Kalimba-Töne nach Tonhöhe im Kreis verteilt: tiefe links, hohe hinten rechts, der ganze Ring dreht sich einmal pro "
  "Takt. Das Modrad macht die Zungen heller." + HP_DE,
  sm(inst="kalimba", start=0.0, tone=0.5),
  amp(0.002, 1.2, 0.0, 0.7), mod(0.01, 1.0, 0.0, 0.5, 0.0), lfo(0.1, 0.1, "sine", "timbre"),
  space("circle", "1bar", size=0.3, distance=1.5, spread=20.0, room=0.3, roomsize=0.45, doppler=0.1, glide=0.02),
  echo("1/8.", 0.25, 0.12), rev(0.5, 0.12),
  routes=[route("key", "azimuth", 0.45)], wheel=("engine2", 0.4), voices=10, velsens=0.55)

B("Harp Constellation", "Plucks", "sampler",
  "Every harp note appears as a star at its own random place around and above you, then the next one lights up "
  "elsewhere. The mod wheel lifts the constellation higher." + HP_EN,
  "Jeder Harfenton erscheint als Stern an einem eigenen zufälligen Ort um und über dir, dann leuchtet der nächste "
  "woanders auf. Das Modrad hebt das Sternbild höher." + HP_DE,
  sm(inst="harp", start=0.0, tone=0.55),
  amp(0.002, 2.0, 0.0, 1.2), mod(0.01, 1.0, 0.0, 0.5, 0.0), lfo(0.1, 0.06, "sine", "timbre"),
  space("off", "1bar", size=0.0, distance=1.8, spread=15.0, room=0.35, roomsize=0.6, doppler=0.0, elevation=10.0, glide=0.01),
  rev(0.7, 0.16),
  routes=[route("random", "azimuth", 0.5), route("random", "elevation", 0.12)],
  wheel=("elevation", 0.25), voices=12, velsens=0.5)

B("Marimba Ricochet", "Plucks", "sampler",
  "Marimba hits ricocheting between front and back every quarter note, crisp and close. The mod wheel throws them "
  "further out into the room." + HP_EN,
  "Marimbaschläge, die jede Viertel zwischen vorn und hinten abprallen, knackig und nah. Das Modrad wirft sie weiter in "
  "den Raum." + HP_DE,
  sm(inst="marimba", start=0.0, tone=0.55),
  amp(0.002, 1.0, 0.0, 0.5), mod(0.01, 1.0, 0.0, 0.5, 0.0), lfo(0.1, 0.06, "sine", "timbre"),
  space("pingpong", "1/4", size=0.6, distance=1.3, spread=20.0, room=0.25, roomsize=0.4, doppler=0.1),
  rev(0.5, 0.12),
  wheel=("distance", 0.3), voices=10, velsens=0.6, extra_tags=("Drums",))

B("Glass Droplets", "Plucks", "pluck",
  "Bright glassy plucks falling like droplets at random points around your head, a little above. Velocity sets the "
  "splash; the mod wheel lets them ring longer." + HP_EN,
  "Helle, gläserne Zupfer, die wie Tropfen an zufälligen Stellen um den Kopf fallen, etwas von oben. Anschlag bestimmt "
  "den Spritzer; das Modrad lässt sie länger klingen." + HP_DE,
  pl(material=0.85, damping=0.45, bright=0.7, body=0.2, excite=0.35, pickpos=0.12, bodysize=0.25),
  amp(0.002, 1.2, 0.0, 0.8), mod(0.01, 0.5, 0.0, 0.3, 0.0), lfo(0.1, 0.06, "sine", "timbre"),
  space("off", "1bar", size=0.0, distance=1.6, spread=10.0, room=0.3, roomsize=0.5, doppler=0.0, elevation=20.0, glide=0.01),
  echo("1/8.", 0.28, 0.14), rev(0.6, 0.14),
  routes=[route("random", "azimuth", 0.5), route("random", "elevation", 0.1)],
  wheel=("engine2", -0.35), voices=10, velsens=0.65)

B("Zither Drizzle", "Plucks", "sampler",
  "Zither strings scattered by a swarm path, every pluck coming from a slightly different place like a light drizzle. "
  "The mod wheel darkens the strings." + HP_EN,
  "Zitherseiten, von einem Schwarmpfad verstreut; jeder Anschlag kommt von einem etwas anderen Ort wie leichter "
  "Nieselregen. Das Modrad macht die Saiten dunkler." + HP_DE,
  sm(inst="zither", start=0.0, tone=0.5),
  amp(0.004, 1.8, 0.0, 1.0), mod(0.01, 1.0, 0.0, 0.5, 0.0), lfo(0.1, 0.06, "sine", "timbre"),
  space("swarm", "2bars", size=0.6, distance=1.7, spread=30.0, room=0.35, roomsize=0.55, doppler=0.1),
  rev(0.65, 0.15),
  wheel=("engine2", -0.4), voices=12, velsens=0.55)

# =============================================================================================================== Arp
B("Satellite Arp", "Arp", "fm",
  "FM bell arpeggios orbiting your head once per bar like a satellite, just out of reach. The mod wheel pushes the orbit "
  "further out." + HP_EN,
  "FM-Glocken-Arpeggios, die wie ein Satellit einmal pro Takt um den Kopf kreisen, knapp außer Reichweite. Das Modrad "
  "schiebt den Orbit weiter hinaus." + HP_DE,
  fm(algo=0, modratio=3.5, index=0.2, feedback=0.0, op1ratio=1, op2ratio=3.5, op1level=1.0, op2level=0.4, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.002, 0.6, 0.0, 0.4), mod(0.001, 0.3, 0.0, 0.3, 0.45), lfo(0.1, 0.1, "tri", "timbre"),
  space("circle", "1bar", size=0.4, distance=1.7, spread=20.0, room=0.3, roomsize=0.45, doppler=0.15),
  echo("1/8.", 0.3, 0.14), rev(0.5, 0.12),
  wheel=("distance", 0.3), arp_block=("updown", 2, 0.45, "1/16"), voices=8, velsens=0.5)

B("Helix Sequence", "Arp", "wavetable",
  "A glassy wavetable arpeggio climbing a double helix around your head: it spirals up over two bars while the notes "
  "climb. The mod wheel sweeps the wavetable." + HP_EN,
  "Ein gläsernes Wavetable-Arpeggio, das eine Doppelhelix um den Kopf hinaufsteigt: in zwei Takten spiralförmig nach "
  "oben, während die Töne steigen. Das Modrad fährt durch die Wavetable." + HP_DE,
  wt(table="digital", pos=0.3, warp=0.15, tone=0.6, unison=0.15, univoices=2, uniwidth=0.4, keytrack=0.35, envpos=0.3),
  amp(0.003, 0.35, 0.0, 0.25), mod(0.002, 0.25, 0.0, 0.2, 0.4), lfo(0.1, 0.1, "tri", "timbre"),
  space("spiral", "2bars", size=0.5, distance=1.8, spread=25.0, room=0.3, roomsize=0.45, doppler=0.15),
  echo("1/8.", 0.3, 0.14), rev(0.5, 0.12),
  wheel=("engine1", 0.4), arp_block=("up", 3, 0.5, "1/16"), voices=8, velsens=0.5)

B("Bell Ricochet", "Arp", "sampler",
  "Glockenspiel notes in random order bouncing front to back every eighth note, sparkling around the listener. The mod "
  "wheel speeds the bounce." + HP_EN,
  "Glockenspiel-Töne in zufälliger Folge, die jede Achtel zwischen vorn und hinten springen und um dich funkeln. Das "
  "Modrad beschleunigt das Springen." + HP_DE,
  sm(inst="glockenspiel", start=0.0, tone=0.5),
  amp(0.002, 0.8, 0.0, 0.5), mod(0.01, 0.5, 0.0, 0.3, 0.0), lfo(0.1, 0.06, "sine", "timbre"),
  space("pingpong", "1/8", size=0.55, distance=1.6, spread=20.0, room=0.3, roomsize=0.45, doppler=0.1),
  rev(0.55, 0.14),
  wheel=("pathrate", 0.5), arp_block=("random", 2, 0.4, "1/16"), voices=8, velsens=0.5)

B("Pulse Orbit", "Arp", "analog",
  "A resonant square arpeggio tracing a figure-eight through your head space every two bars. The mod wheel opens the "
  "filter." + HP_EN,
  "Ein resonantes Rechteck-Arpeggio, das alle zwei Takte eine Acht durch den Raum um deinen Kopf zieht. Das Modrad "
  "öffnet das Filter." + HP_DE,
  an(shape=0.5, detune=0.04, cutoff=1100, reso=0.15, oscmix=0.3, sub=0.0, drive=0.3, keytrack=0.6),
  amp(0.006, 0.35, 0.2, 0.12), mod(0.002, 0.15, 0.0, 0.1, 0.2), lfo(0.1, 0.1, "tri", "timbre"),
  space("figure8", "2bars", size=0.55, distance=1.6, spread=20.0, room=0.25, roomsize=0.4, doppler=0.15),
  echo("1/8.", 0.3, 0.15),
  wheel=("engine3", 0.35), arp_block=("up", 2, 0.45, "1/16"), voices=8, velsens=0.5)

B("Swarm Sequence", "Arp", "cluster",
  "Detuned cluster stabs arpeggiated in eighths, each note flying off with a swarm that never lands twice in the same "
  "place. The mod wheel widens the stack." + HP_EN,
  "Verstimmte Cluster-Stabs als Achtel-Arpeggio; jeder Ton fliegt mit einem Schwarm davon, der nie zweimal an derselben "
  "Stelle landet. Das Modrad verbreitert die Schichtung." + HP_DE,
  cl(detune=0.3, spread=0.8, tone=0.6, chord="min7", stack=3, shape=0.0, side=0.6, reso=0.15, keytrack=0.5, drift=0.1),
  amp(0.008, 0.5, 0.4, 0.2), mod(0.002, 0.2, 0.0, 0.15, 0.05), lfo(0.1, 0.1, "tri", "timbre"),
  space("swarm", "1bar", size=0.65, distance=1.8, spread=40.0, room=0.3, roomsize=0.45, doppler=0.1),
  echo("1/8.", 0.25, 0.12), rev(0.45, 0.12),
  wheel=("engine1", 0.45), arp_block=("up", 1, 0.5, "1/8"), voices=8, velsens=0.5)

# ============================================================================================================== Bass
B("Orbit Sub", "Bass", "analog",
  "A round saw bass whose low end stays centred and mono while its upper harmonics circle your head every two bars. The "
  "mod wheel opens the filter." + HP_EN,
  "Ein runder Sägezahnbass: der Tiefbass bleibt mittig und mono, die oberen Obertöne kreisen alle zwei Takte um den "
  "Kopf. Das Modrad öffnet das Filter." + HP_DE,
  an(shape=0.1, detune=0.08, cutoff=650, reso=0.25, oscmix=0.35, sub=0.45, drive=0.3, keytrack=0.4),
  amp(0.003, 0.5, 0.8, 0.1), mod(0.001, 0.25, 0.15, 0.1, 0.4), MONO, lfo(0.17, 0.1, "tri", "timbre"),
  space("circle", "2bars", size=0.3, distance=1.5, spread=30.0, room=0.15, roomsize=0.3, doppler=0.05, lowkeep=200.0),
  comp(-14.0, 3.5, 1.5, 80.0),
  wheel=("engine3", 0.3), velsens=0.45)

B("Halo Reese", "Bass", "wavetable",
  "A wide detuned reese bass with a mono sub core; the moving top weaves around you on a four-bar orbit. The mod wheel "
  "sweeps the wavetable for growl." + HP_EN,
  "Ein breiter, verstimmter Reese-Bass mit monoem Sub-Kern; die bewegte Obertonschicht webt auf einem Vier-Takt-Orbit um "
  "dich. Das Modrad fährt für Knurren durch die Wavetable." + HP_DE,
  wt(table="classic", pos=0.45, warp=0.1, tone=0.5, unison=0.55, univoices=3, uniwidth=0.7, sub=0.4, keytrack=0.3),
  amp(0.004, 0.6, 0.95, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.1), MONO, lfo(0.15, 0.12, "tri", "timbre"),
  space("weave", "4bars", size=0.35, distance=1.6, spread=50.0, room=0.15, roomsize=0.3, doppler=0.05, lowkeep=220.0),
  comp(-14.0, 3.0, 2.0, 90.0),
  wheel=("engine1", 0.35), velsens=0.3)

B("Pendulum Growl", "Bass", "fm",
  "A growling FM bass whose bright edge swings from ear to ear every half bar while the fundamental stays planted in "
  "the middle. The mod wheel raises the FM index." + HP_EN,
  "Ein knurrender FM-Bass, dessen helle Kante jeden halben Takt von Ohr zu Ohr schwingt, während der Grundton fest in "
  "der Mitte steht. Das Modrad hebt den FM-Index." + HP_DE,
  fm(algo=0, modratio=1.0, index=0.24, feedback=0.3, op1ratio=1, op2ratio=1, op1level=1.0, op2level=0.6, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.003, 0.5, 0.85, 0.1), mod(0.005, 0.35, 0.4, 0.1, 0.5), MONO, lfo(0.2, 0.1, "tri", "timbre"),
  space("pendulum", "1/2", size=0.5, distance=1.5, spread=25.0, room=0.12, roomsize=0.3, doppler=0.05, lowkeep=180.0),
  comp(-15.0, 4.0, 1.0, 70.0),
  wheel=("engine3", 0.3), velsens=0.4)

B("Spinning Pluck Bass", "Bass", "pluck",
  "A plucked string bass: the woody attack spins once around your head every half bar, the low note stays centred. The "
  "mod wheel lets the string sustain." + HP_EN,
  "Ein gezupfter Saitenbass: der holzige Anschlag dreht sich jeden halben Takt einmal um den Kopf, der tiefe Ton bleibt "
  "in der Mitte. Das Modrad lässt die Saite länger klingen." + HP_DE,
  pl(material=0.35, damping=0.55, bright=0.4, body=0.6, excite=0.55, pickpos=0.25, bodysize=0.6),
  amp(0.002, 0.9, 0.0, 0.2), mod(0.01, 0.3, 0.0, 0.2, 0.0), MONO, lfo(0.15, 0.06, "tri", "timbre"),
  space("circle", "1/2", size=0.3, distance=1.4, spread=20.0, room=0.12, roomsize=0.3, doppler=0.05, lowkeep=200.0),
  sub(0.3, -1, "sine"), comp(-14.0, 3.0, 2.0, 90.0),
  wheel=("engine2", -0.35), guard=(-20.0, 4.0), velsens=0.55)

B("Gravity Bass Halo", "Bass", "analog",
  "A vowel-like analog bass whose partials Key Gravity tunes into the key; the halo of overtones circles in a "
  "figure-eight over the centred sub. The mod wheel deepens the gravity." + HP_EN,
  "Ein vokalartiger Analogbass, dessen Teiltöne Key Gravity in die Tonart stimmt; der Obertonhalo zieht eine Acht über "
  "dem mittigen Sub. Das Modrad vertieft die Gravitation." + HP_DE,
  an(shape=0.15, detune=0.1, cutoff=900, reso=0.35, oscmix=0.4, sub=0.4, drive=0.35, keytrack=0.4),
  amp(0.003, 0.5, 0.85, 0.12), mod(0.002, 0.3, 0.3, 0.1, 0.35), MONO, lfo(0.17, 0.1, "tri", "timbre"),
  grav(0.45, 0, 0.1, 0.6, 0.5, 1.0, 1),
  space("figure8", "1bar", size=0.35, distance=1.5, spread=30.0, room=0.12, roomsize=0.3, doppler=0.05, lowkeep=200.0),
  comp(-14.0, 3.5, 1.5, 80.0),
  wheel=("gravamt", 0.5), velsens=0.4)

# ============================================================================================================= Leads
B("Comet Lead", "Leads", "wavetable",
  "A bright wavetable lead that streaks past you every two bars, rising in pitch as it approaches and falling as it "
  "leaves (Doppler). The mod wheel sweeps the tone." + HP_EN,
  "Ein heller Wavetable-Lead, der alle zwei Takte an dir vorbeizieht, beim Nahen höher, beim Entfernen tiefer (Doppler). "
  "Das Modrad fährt den Klang." + HP_DE,
  wt(table="harmonic", pos=0.55, warp=0.2, tone=0.6, unison=0.25, univoices=2, uniwidth=0.5, keytrack=0.4),
  amp(0.005, 0.6, 0.85, 0.3), mod(0.003, 0.4, 0.4, 0.3, 0.3), lfo(5.2, 0.05, "sine", "pitch"),
  space("flyby", "2bars", size=0.6, distance=1.8, spread=15.0, room=0.25, roomsize=0.45, doppler=0.45),
  echo("1/8.", 0.3, 0.14), rev(0.5, 0.12), expr(at=[("vibrato", 0.4, "s")]),
  wheel=("engine1", 0.35), voices=1, velsens=0.4, glide=0.04)

B("Vowel Orbit Lead", "Leads", "vocal",
  "A singing vowel lead circling you every four bars like a voice moving through the room. Pressure adds vibrato; the "
  "mod wheel walks the vowel from 'ah' to 'ee'." + HP_EN,
  "Ein singender Vokal-Lead, der alle vier Takte um dich kreist wie eine Stimme, die durch den Raum geht. Druck gibt "
  "Vibrato; das Modrad wandert vom 'Ah' zum 'Ii'." + HP_DE,
  vo(vowel=0.2, formant=0.1, breath=0.25, tension=0.55, vibdepth=0.15, vibrate=5.2, jitter=0.1),
  amp(0.03, 0.6, 0.9, 0.4), mod(0.01, 0.5, 0.5, 0.4, 0.0), lfo(0.1, 0.1, "tri", "timbre"),
  space("circle", "4bars", size=0.5, distance=1.8, spread=20.0, room=0.3, roomsize=0.5, doppler=0.15),
  echo("1/4", 0.25, 0.12), rev(0.55, 0.14), expr(at=[("vibrato", 0.45, "s")]),
  wheel=("engine1", 0.5), voices=1, velsens=0.35, glide=0.05, extra_tags=("Vocal",))

B("Ear to Ear Lead", "Leads", "analog",
  "A singing analog lead swinging from one ear to the other and back every two bars, so a melody sweeps across your "
  "head. Velocity opens the filter; the mod wheel opens it further." + HP_EN,
  "Ein singender Analog-Lead, der alle zwei Takte von einem Ohr zum anderen und zurück schwingt; eine Melodie wandert "
  "quer durch den Kopf. Anschlag öffnet das Filter; das Modrad öffnet es weiter." + HP_DE,
  an(shape=0.25, detune=0.12, cutoff=1500, reso=0.15, oscmix=0.5, sub=0.1, drive=0.15, keytrack=0.6),
  amp(0.01, 0.5, 0.8, 0.25), mod(0.002, 0.3, 0.3, 0.2, 0.15), lfo(5.0, 0.04, "sine", "pitch"),
  space("pendulum", "2bars", size=0.6, distance=2.0, spread=20.0, room=0.25, roomsize=0.4, doppler=0.15),
  echo("1/8.", 0.3, 0.14), expr(vel=[("cutoff", 0.3, "exp")]),
  wheel=("engine3", 0.3), voices=1, velsens=0.45, glide=0.03)

B("Overhead Flute", "Leads", "sampler",
  "A breathy recorder melody floating over your head and down behind you every four bars, as if played from a gallery. "
  "The mod wheel brightens the breath." + HP_EN,
  "Eine hauchige Blockflötenmelodie, die alle vier Takte über den Kopf und hinter dir herabschwebt, als spiele jemand "
  "von einer Empore. Das Modrad macht den Atem heller." + HP_DE,
  sm(inst="recorder", start=0.0, tone=0.52),
  amp(0.02, 0.8, 0.9, 0.4), mod(0.01, 0.5, 0.0, 0.3, 0.0), lfo(5.0, 0.06, "sine", "pitch"),
  space("overhead", "4bars", size=0.55, distance=1.9, spread=20.0, room=0.35, roomsize=0.55, doppler=0.12),
  rev(0.6, 0.16),
  wheel=("engine2", 0.35), voices=4, velsens=0.35)

# ============================================================================================================= Drums
B("Round Toms", "Drums", "fm",
  "Tuned FM toms placed around you by pitch, low toms on the left and behind, high toms to the right and in front, so a "
  "fill rolls around your head. The mod wheel moves the kit further away." + HP_EN,
  "Gestimmte FM-Toms nach Tonhöhe um dich verteilt, tiefe links und hinten, hohe rechts und vorn: ein Fill rollt um den "
  "Kopf. Das Modrad rückt das Kit weiter weg." + HP_DE,
  fm(algo=0, modratio=1.4, index=0.15, feedback=0.0, op1ratio=1, op2ratio=1.4, op1level=1.0, op2level=0.4, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.001, 0.45, 0.0, 0.25), mod(0.001, 0.08, 0.0, 0.05, 0.4), pitchenv(0.35, 0.06),
  space("off", "1bar", size=0.0, distance=1.4, spread=15.0, room=0.3, roomsize=0.4, doppler=0.0, glide=0.005),
  rev(0.45, 0.12),
  routes=[route("key", "azimuth", 0.5)], wheel=("distance", 0.3), voices=6, velsens=0.7)

B("Circling Hats", "Drums", "cluster",
  "Metallic cluster hats travelling around your head once every beat, so a straight hat pattern becomes a ring of "
  "ticks. The mod wheel speeds the ring up." + HP_EN,
  "Metallische Cluster-Hats, die jeden Schlag einmal um den Kopf wandern; ein gerades Hat-Muster wird zum Ring aus "
  "Ticks. Das Modrad beschleunigt den Ring." + HP_DE,
  cl(detune=1.0, spread=0.7, tone=0.9, chord="fourths", stack=7, shape=1.0, side=1.0, reso=0.2, keytrack=0.3, drift=0.0),
  amp(0.001, 0.3, 0.0, 0.12), mod(0.001, 0.05, 0.0, 0.05, 0.0), lfo(0.1, 0.05, "sine", "timbre"),
  space("circle", "1/4", size=0.4, distance=1.3, spread=10.0, room=0.25, roomsize=0.35, doppler=0.05),
  drive(0.7, 1.0, "soft"),
  wheel=("pathrate", 0.6), voices=4, velsens=0.7, tune=12)

B("Hand Drum Ring", "Drums", "fm",
  "Hand-drum tones sitting in a ring around you: low strokes behind, slaps in front, each note at its own angle, and "
  "the ring slowly turning. The mod wheel speeds the turn." + HP_EN,
  "Handtrommeltöne in einem Ring um dich: tiefe Schläge hinten, Slaps vorn, jeder Ton in seinem eigenen Winkel, der "
  "Ring dreht sich langsam. Das Modrad beschleunigt die Drehung." + HP_DE,
  fm(algo=0, modratio=2.3, index=0.12, feedback=0.1, op1ratio=1, op2ratio=2.3, op1level=1.0, op2level=0.35, op3level=0.0,
     op4level=0.0, op5level=0.0, op6level=0.0),
  amp(0.001, 0.3, 0.0, 0.15), mod(0.001, 0.05, 0.0, 0.05, 0.5), pitchenv(0.25, 0.04),
  space("circle", "4bars", size=0.0, distance=1.5, spread=15.0, room=0.3, roomsize=0.4, doppler=0.0, glide=0.01),
  rev(0.4, 0.1),
  routes=[route("key", "azimuth", -0.6)], wheel=("pathrate", 0.8), voices=6, velsens=0.7)

# ================================================================================================================ FX
B("Drone Flyover", "FX", "wavetable",
  "A gritty engine drone that approaches from far in front, roars past overhead and fades out behind you (Fly-By with "
  "strong Doppler). Hold a note; the mod wheel speeds up the passes." + HP_EN,
  "Ein körniges Motorbrummen, das von weit vorn naht, über dir vorbeidröhnt und hinter dir verklingt (Vorbeiflug mit "
  "starkem Doppler). Eine Note halten; das Modrad beschleunigt die Überflüge." + HP_DE,
  wt(table="grit", pos=0.3, warp=0.3, tone=0.45, unison=0.4, univoices=4, uniwidth=0.6, keytrack=0.3),
  amp(0.5, 1.0, 1.0, 1.5), mod(1.0, 1.0, 1.0, 1.5, 0.3), lfo(0.3, 0.15, "tri", "timbre"),
  space("flyby", "4bars", size=0.8, distance=2.5, spread=40.0, elevation=25.0, room=0.3, roomsize=0.6, doppler=0.8),
  rev(0.7, 0.16),
  wheel=("pathrate", 0.5), guard=-8.0, voices=4, velsens=0.25)

B("Vortex Riser", "FX", "analog",
  "A noise riser caught in a vortex: it spirals upwards faster and faster around your head over four bars. Hold a note "
  "for the build; the mod wheel tightens the vortex." + HP_EN,
  "Ein Rausch-Riser in einem Wirbel: er schraubt sich in vier Takten immer schneller um den Kopf nach oben. Für den "
  "Aufbau eine Note halten; das Modrad zieht den Wirbel enger." + HP_DE,
  an(shape=0.3, detune=0.25, cutoff=500, reso=0.45, oscmix=0.5, sub=0.0, noise=0.6, drive=0.2, keytrack=0.5),
  amp(1.2, 1.0, 1.0, 1.5), mod(2.5, 1.0, 1.0, 1.5, 0.7), lfo(0.5, 0.2, "tri", "timbre"),
  space("spiral", "4bars", size=0.6, distance=2.2, spread=60.0, room=0.35, roomsize=0.6, doppler=0.3),
  rev(0.75, 0.2),
  routes=[route("modenv", "pathrate", 0.5)], wheel=("pathrate", 0.4), voices=4, velsens=0.25)

B("Grain Vortex", "FX", "grain",
  "Bursts of reversed vibraphone grains whirling around the listener on a swarm path with Doppler swoops. The mod "
  "wheel sprays the grains wider." + HP_EN,
  "Stöße rückwärts laufender Vibraphon-Grains, die auf einem Schwarmpfad mit Doppler-Schwüngen um dich wirbeln. Das "
  "Modrad streut die Grains weiter." + HP_DE,
  gr(inst="vibraphone", pos=0.15, size=60, density=35, spray=0.5, pitch=0, reverse=0.8, spread=1.0, scan=0.8, tone=0.55),
  amp(0.3, 1.0, 0.9, 1.5), mod(0.5, 1.0, 0.5, 1.5, 0.0), lfo(0.3, 0.2, "sh", "timbre"),
  space("swarm", "2bars", size=0.75, distance=2.0, spread=90.0, room=0.3, roomsize=0.55, doppler=0.45),
  rev(0.65, 0.16),
  wheel=("engine4", 0.45), guard=-9.0, voices=6, velsens=0.3, extra_tags=("Textures",))

B("Thunder Walk", "FX", "analog",
  "A low, rolling rumble wandering near and far over eight bars like a storm crossing the valley, the sub kept mono "
  "and steady. The mod wheel brings the storm closer." + HP_EN,
  "Ein tiefes, rollendes Grollen, das in acht Takten nah und fern wandert wie ein Gewitter über dem Tal; der Sub bleibt "
  "mono und ruhig. Das Modrad holt das Gewitter näher." + HP_DE,
  an(shape=0.0, detune=0.0, cutoff=650, reso=0.3, oscmix=0.0, sub=0.2, noise=1.0, drive=0.6, keytrack=0.3),
  amp(1.5, 2.0, 0.9, 3.0), mod(2.0, 2.0, 0.6, 2.0, 0.3), lfo(0.35, 0.4, "sh", "timbre"),
  space("walk", "8bars", size=0.6, distance=3.5, spread=120.0, room=0.45, roomsize=0.8, doppler=0.05, rolloff=0.5,
        lowkeep=150.0),
  rev(0.85, 0.2),
  wheel=("distance", -0.2), voices=4, velsens=0.25, extra_tags=("Textures",))

# ============================================================================================================= Vocal
B("Angel Overhead", "Vocal", "vocal",
  "A pure, high 'ooh' voice floating over your head and down behind you every eight bars, like a singer in the "
  "gallery. The mod wheel opens the vowel." + HP_EN,
  "Eine reine, hohe 'Uuh'-Stimme, die alle acht Takte über den Kopf und hinter dir herabschwebt wie eine Sängerin auf "
  "der Empore. Das Modrad öffnet den Vokal." + HP_DE,
  vo(vowel=3.8, formant=0.3, breath=0.2, tension=0.5, vibdepth=0.14, vibrate=5.4, jitter=0.1),
  amp(0.4, 1.5, 0.9, 2.0), mod(0.5, 1.5, 0.5, 1.5, 0.0), lfo(0.09, 0.12, "tri", "timbre"),
  space("overhead", "8bars", size=0.55, distance=2.2, spread=30.0, room=0.4, roomsize=0.65, doppler=0.1, elevation=10.0),
  rev(0.75, 0.2),
  wheel=("engine1", -0.5), voices=6, velsens=0.3)

B("Choir Weave", "Vocal", "vocal",
  "A dark male-formant choir weaving a wavy orbit around you over eight bars, rising and dipping as it turns. Pressure "
  "pulls it closer; the mod wheel lifts the formant." + HP_EN,
  "Ein dunkler Chor mit tiefen Formanten, der in acht Takten einen wellenförmigen Orbit um dich webt, steigend und "
  "sinkend. Druck holt ihn näher; das Modrad hebt den Formant." + HP_DE,
  vo(vowel=1.2, formant=-0.8, breath=0.3, tension=0.4, vibdepth=0.1, vibrate=4.7, jitter=0.2),
  amp(0.7, 2.0, 0.9, 2.5), mod(1.0, 2.0, 0.5, 2.0, 0.0), lfo(0.08, 0.15, "tri", "timbre"),
  space("weave", "8bars", size=0.5, distance=2.3, spread=70.0, room=0.35, roomsize=0.6, doppler=0.1),
  chorus(0.2, 0.55, 0.3), rev(0.75, 0.18),
  routes=[route("pressure", "distance", -0.1)], wheel=("engine2", 0.4), voices=8, velsens=0.25, extra_tags=("Pads",))

B("Breath Circle", "Vocal", "vocal",
  "Close, rhythmic breaths circling your head every two bars at whispering distance: intimate and slightly uncanny. The "
  "mod wheel adds a sung tone to the breath." + HP_EN,
  "Nahe, rhythmische Atemzüge, die alle zwei Takte in Flüsterabstand um den Kopf kreisen: intim und leicht unheimlich. "
  "Das Modrad gibt dem Atem einen gesungenen Ton." + HP_DE,
  vo(vowel=2.4, formant=0.4, breath=0.95, tension=0.15, vibdepth=0.0, vibrate=4.5, jitter=0.4),
  amp(0.15, 0.6, 0.5, 0.6), mod(0.3, 1.0, 0.5, 1.0, 0.0), lfo(0.12, 0.2, "tri", "timbre"),
  space("circle", "2bars", size=0.3, distance=0.7, spread=20.0, room=0.15, roomsize=0.3, doppler=0.05, air=0.3),
  wheel=("engine3", -0.6), voices=6, velsens=0.35, extra_tags=("Textures",))


# ---------------------------------------------------------------------------------------------------------- writing
def part_doc(p):
    d = {"format": "ohpe-preset", "version": 2, "kind": "part", "name": p["name"], "author": "groovex", "tags": p["tags"],
         "engine": p["engine"], "comment": p["comment"], "params": p["params"]}
    if p.get("arp"):
        d["arp"] = p["arp"]
    return d


def write(path, doc):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write("\n")


CAT_DE = {"Bass": "Bass", "Keys": "Tasten", "Pads": "Flächen", "Leads": "Leads", "Plucks": "Zupfer", "Drums": "Drums",
          "FX": "FX", "Arp": "Arp", "Textures": "Texturen", "Vocal": "Vokal", "3D": "3D"}
ENG = {"fm": "FM", "wavetable": "Wavetable", "analog": "Analog", "pluck": ("Pluck", "Zupfen"), "grain": "Grain",
       "vocal": "Vocal", "cluster": "Cluster", "sampler": "Sampler"}
ORDER = ["Pads", "Textures", "Keys", "Plucks", "Arp", "Bass", "Leads", "Drums", "FX", "Vocal"]
INTRO = {
    "en": "## Binaural presets (p71)\n\n"
          "%d part presets made for headphones, each built around the part's binaural 3D stage: pads circling the head, "
          "near whispers and far-off voices, rain-like grain fields, plucks and bells placed around you, percussion whose "
          "hits land at different places (pitch or chance picks the direction), basses whose low end stays centred and "
          "mono while the overtones move, and arps that travel - some combined with Key Gravity or Voice Control. They "
          "carry the **3D** category: the library's *3D* chip or a search for *3D* / *binaural* lists them, and the "
          "headphones pictogram marks them in the list. Every one is level-matched to the bank target and has an audible "
          "mod wheel.\n",
    "de": "## Binaurale Presets (p71)\n\n"
          "%d Part-Presets für Kopfhörer, jedes um die binaurale 3D-Stufe des Parts gebaut: Flächen, die um den Kopf "
          "kreisen, nahes Flüstern und ferne Stimmen, regenartige Grain-Felder, Zupfer und Glocken rund um dich, "
          "Percussion, deren Schläge an verschiedenen Orten landen (Tonhöhe oder Zufall wählt die Richtung), Bässe, deren "
          "Tiefbass mittig und mono bleibt, während die Obertöne wandern, und Arps, die reisen - manche mit Key Gravity "
          "oder Stimmsteuerung kombiniert. Sie tragen die Kategorie **3D**: der *3D*-Chip der Bibliothek oder die Suche "
          "nach *3D* / *binaural* listet sie, das Kopfhörer-Piktogramm markiert sie in der Liste. Jedes ist auf das "
          "Bank-Ziel eingepegelt und hat ein hörbares Modrad.\n",
}


def docs(parts):
    """The binaural section of docs/en|de/FACTORY-SOUNDS.md (between <!-- binaural:begin / end -->, before the drum kits)."""
    for lang, head in (("en", "## Drum kits"), ("de", "## Drum-Kits")):
        de = lang == "de"
        rows = [INTRO[lang] % len(parts), "<!-- binaural:begin -->"]
        rows.append("| Name | %s | Engine | %s | %s |" % ("Kategorie" if de else "Category", "Bewegung" if de else "Motion",
                                                      "Beschreibung" if de else "Description"))
        rows.append("|---|---|---|---|---|")
        for p in sorted(parts, key=lambda p: (ORDER.index(p["tags"][0]), p["name"])):
            eng = ENG[p["engine"]]
            eng = eng[1 if de else 0] if isinstance(eng, tuple) else eng
            tags = ", ".join(CAT_DE.get(t, t) if de else t for t in p["tags"])
            mot = (PATH_DE if de else PATH_EN)[p["path"]]
            rows.append("| %s | %s | %s | %s | %s |" % (p["name"], tags, eng, mot, p["de"] if de else p["comment"]))
        rows.append("<!-- binaural:end -->\n")
        block = "\n".join(rows)
        path = os.path.join(ROOT, "docs", lang, "FACTORY-SOUNDS.md")
        text = open(path, encoding="utf-8").read()
        if "<!-- binaural:begin -->" in text:
            start = text.index(INTRO[lang][:20])
            end = text.index("<!-- binaural:end -->") + len("<!-- binaural:end -->\n")
            text = text[:start] + block + text[end:]
        else:
            i = text.index(head)
            text = text[:i] + block + "\n" + text[i:]
        open(path, "w", encoding="utf-8").write(text)


def main():
    old = json.load(open(MANIFEST)) if os.path.exists(MANIFEST) else []
    mine = {p["file"] for p in PRESETS}
    assert len(mine) == len(PRESETS), "duplicate file names"
    others = set()
    for f in os.listdir(OUT):
        if f.endswith(".ohpepreset") and f not in mine and f not in old:
            others.add(json.load(open(os.path.join(OUT, f), encoding="utf-8"))["name"].lower())
    for f in old:
        if f not in mine and os.path.exists(os.path.join(OUT, f)):
            os.remove(os.path.join(OUT, f))
    for p in PRESETS:
        assert p["name"].lower() not in others, "name taken: " + p["name"]
        assert len(p["name"]) <= 22, p["name"]
        path = os.path.join(OUT, p["file"])
        if os.path.exists(path):  # keep the calibrated level
            p["params"]["gain"] = json.load(open(path, encoding="utf-8"))["params"].get("gain", p["params"]["gain"])
        write(path, part_doc(p))
    json.dump(sorted(mine), open(MANIFEST, "w"), indent=1)
    if "--docs" in sys.argv:
        docs(PRESETS)
    print("wrote", len(PRESETS), "binaural presets")


if __name__ == "__main__":
    main()
