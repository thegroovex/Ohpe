# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""1.0 (3D space): the flagship "3D" factory presets - binaural movement made for headphones.

Every preset switches the part's spatialiser stage on ("space.*", SpaceEffect) with a tempo-synced motion path:
orbiting atmospheres, flying arps, mid-range basses circling the head over a mono sub (the stage keeps everything below
"lowkeep" mono and unspatialised), binaural risers and fly-bys with Doppler. Every preset maps MPE / aftertouch onto the
space too: slide (timbre) lifts the source (elevation), pressure pulls it closer (distance).

SPACE_GAINS holds the part gain (dB) per preset from the level calibration (tools/presets/space_cal.py, target
-15 dBFS active RMS, peaks <= -1.2 dBFS); SPACE_PROJECTS adds 3D motion to two factory projects.
Imported by make_factory.py (written after the other part presets).
"""
import factory_parts_r3 as r3
from factory_parts import an, fm, wt, pl, cl, vo, amp, mod, lfo, pitchenv, arp, MONO  # noqa: F401
from factory_parts_r3 import P, cho, dly, rev, fdrive, shaper, squash, wobble, formant, comb, flanger, comp, motion, lfo_n, mpe

# the matrix targets reserved in 0.7 for the spatialiser (ModMatrix.cpp, after "FX 6 Param 4")
SPACE_DST = ["azimuth", "elevation", "distance", "pathrate", "pathphase"]
if SPACE_DST[0] not in r3.MOD_DST:
    r3.MOD_DST += SPACE_DST

PATHS = {"off": 0, "circle": 1, "figure8": 2, "flyby": 3, "random": 4, "pendulum": 5, "spiral": 6}
LENGTHS = {"1/16": 0, "1/8": 1, "1/4": 2, "1/2": 3, "1 bar": 4, "2 bars": 5, "4 bars": 6, "8 bars": 7, "16 bars": 8, "32 bars": 9,
           "1/4.": 10}


def space(path="off", length="2 bars", rate=1.0, size=0.5, phase=0.0, az=0.0, el=0.0, dist=1.5, spread=40.0, room=0.25,
          roomsize=0.4, doppler=0.5, air=0.6, rolloff=0.7, lowkeep=120.0, glide=0.06):
    """The part's 3D stage ("pN.space.*"), switched on."""
    return {"space.on": 1, "space.path": PATHS[path], "space.pathlen": LENGTHS[length], "space.pathrate": rate,
            "space.pathsize": size, "space.pathphase": phase, "space.azimuth": az, "space.elevation": el, "space.distance": dist,
            "space.spread": spread, "space.room": room, "space.roomsize": roomsize, "space.doppler": doppler, "space.air": air,
            "space.rolloff": rolloff, "space.lowkeep": lowkeep, "space.glide": glide}


def expressive(*extra, slide=0.35, press=-0.12, **settings):
    """MPE / aftertouch in 3D: slide lifts the source (elevation, + = up), pressure pulls it closer (distance, - = nearer)."""
    return motion(("slide", "elevation", slide), ("pressure", "distance", press), *extra, **settings)


SPACE_GAINS = {  # level calibration (space_cal.py): -15 dBFS active RMS, peaks <= -1.2 dBFS
    "Binaural Lead": -6.1,
    "Doppler Fly-By": 17.8,
    "Figure Eight Arp": 22.2,
    "Halo Drift": 10.1,
    "Neuro Swirl": 12.9,
    "Night Swarm": 21.0,
    "Orbit Arp": 22.5,
    "Orbit Atmos": 9.5,
    "Orbit Reese": 9.9,
    "Overhead Choir": 13.9,
    "Pendulum Wobble": 11.7,
    "Spiral Arp": 20.5,
    "Spiral Riser": 22.8,
}


# Spiral Riser and Doppler Fly-By end in a peak compressor after their reverb (slot 4, before the 3D stage): it keeps a
# held note / the close pass in check; their level is set by its makeup (space_cal.py's gain step barely moves them).
# Orbit Arp and Spiral Arp start with a fast-release compressor (slot 1) that takes the edge off the pluck / bell attacks,
# which the binaural stage raises again at the near ear.

# Presets ending in the Multiband (upward + downward compression: the part gain barely changes their level) are
# calibrated on its output (dB, -12 ... +12) instead; filled by space_cal.py as well.
SPACE_OUT = {  # Multiband output (dB) from the same calibration
    "Neuro Swirl": 6.5,
    "Orbit Reese": 6.3,
    "Pendulum Wobble": 10.0,
}


def S(name, tags, engine, comment, *dicts, **kw):
    r3.NO_PUNCH.add(name)  # calibrated with the binaural stage (space_cal.py): no glue / ensemble chorus injected
    p = P(name, tags, engine, comment, *dicts, **kw)
    if name in SPACE_GAINS:
        p["params"]["gain"] = SPACE_GAINS[name]
    if name in SPACE_OUT:
        for k in p["params"]:
            if k.endswith(".multiband.output"):
                p["params"][k] = SPACE_OUT[name]
    return p


PRESETS_SPACE = [
    # ================================================================ ATMOSPHERES (4)
    S("Orbit Atmos", ["Pads", "3D"], "wavetable",
      "Wide wavetable atmosphere circling your head once every eight bars on a gently tilted orbit, in a soft room. "
      "Best with headphones: slide lifts it, pressure pulls it closer.",
      wt(table="harmonic", pos=0.45, warp=0.15, tone=0.62, unison=0.4, univoices=6, uniwidth=0.9, keytrack=0.35, lfopos=0.25),
      amp(1.2, 2.0, 0.9, 3.5), mod(1.5, 2.0, 0.6, 3.0, 0.15), lfo(0.11, 0.3, "tri", "timbre"), cho(1, 0.25, 0.45, 0.25),
      rev(3, 0.86, 0.45, 0.04, 0.3),
      space("circle", "8 bars", size=0.3, dist=3.0, spread=80.0, room=0.35, roomsize=0.6, doppler=0.2),
      expressive(), voices=8),
    S("Halo Drift", ["Pads", "3D"], "cluster",
      "Detuned cluster choir wandering around and above you on a slow random orbit - never the same place twice.",
      cl(detune=0.22, spread=0.85, tone=0.48, chord="unison", stack=6, shape=0.3, side=0.7, reso=0.1, keytrack=0.4, drift=0.35),
      amp(1.5, 2.0, 0.9, 4.0), mod(1.0, 2.5, 0.5, 3.0, 0.1), lfo(0.07, 0.25, "sine", "timbre"), rev(3, 0.9, 0.5, 0.05, 0.32),
      space("random", "16 bars", size=0.85, dist=4.0, spread=70.0, room=0.45, roomsize=0.7, doppler=0.15),
      expressive(), voices=8),
    S("Overhead Choir", ["Pads", "Vocal", "3D"], "vocal",
      "Breathy 'ooh' choir swinging from in front of you, over your head to behind you and back - a pendulum through the room.",
      vo(vowel=0.75, formant=0.0, breath=0.35, tension=0.45, vibdepth=0.2, vibrate=4.8, jitter=0.2),
      amp(0.8, 1.5, 0.9, 3.0), mod(0.5, 2.0, 0.5, 2.0, 0.1), cho(1, 0.3, 0.5, 0.3), rev(3, 0.88, 0.45, 0.04, 0.3),
      space("pendulum", "8 bars", size=0.9, dist=2.5, spread=50.0, room=0.4, roomsize=0.55, doppler=0.1),
      expressive(), voices=8),
    S("Night Swarm", ["Textures", "3D"], "fm",
      "Glassy FM bells scattered on a fast random orbit at different distances: a swarm of lights around your head.",
      fm(algo=2, modratio=1.0, index=0.35, op1ratio=1, op2ratio=3.5, op3ratio=2, op4ratio=7, op1level=1.0, op2level=0.5, op3level=0.4,
         op4level=0.25, op5level=0.0, op6level=0.0, velidx=0.5),
      amp(0.002, 1.2, 0.2, 1.4), mod(0.001, 0.8, 0.2, 1.0, 0.6), dly(2, 0.375, 0.45, 0.4, 1, 0.25), rev(3, 0.9, 0.35, 0.03, 0.35),
      space("random", "1 bar", size=1.0, dist=2.6, spread=30.0, room=0.5, roomsize=0.75, doppler=0.35),
      expressive(), {"vol": 1.0}, voices=10),

    # ================================================================ ARPS (3)
    S("Orbit Arp", ["Arp", "3D"], "pluck",
      "Sixteenth-note pluck arpeggio flying round your head once a bar - close, bright and fast.",
      pl(material=0.35, damping=0.3, bright=0.6, body=0.3, excite=0.25, pickpos=0.3, bodysize=0.4),
      amp(0.001, 0.8, 0.0, 0.4), comp(1, -8.0, 6.0, 0.1, 30.0), dly(2, 0.375, 0.3, 0.5, 1, 0.24), rev(3, 0.6, 0.5, 0.02, 0.18),
      space("circle", "1 bar", size=0.15, dist=1.8, spread=20.0, room=0.36, roomsize=0.35, doppler=0.25),
      arp("up", 2, 0.8, "1/16", latch=1), voices=8),
    S("Figure Eight Arp", ["Arp", "3D"], "wavetable",
      "Digital arp tracing a lying eight around you - left front, across, right behind - over two bars.",
      wt(table="digital", pos=0.35, tone=0.72, unison=0.2, univoices=3, uniwidth=0.6, keytrack=0.5, envpos=0.3),
      amp(0.001, 0.35, 0.2, 0.2), mod(0.001, 0.25, 0.0, 0.2, 0.4), dly(2, 0.25, 0.35, 0.45, 1, 0.2), rev(3, 0.65, 0.5, 0.02, 0.2),
      space("figure8", "2 bars", size=0.85, dist=2.2, spread=25.0, room=0.3, roomsize=0.4, doppler=0.35),
      arp("updown", 2, 0.6, "1/16", latch=1), voices=8),
    S("Spiral Arp", ["Arp", "3D"], "fm",
      "Bell arpeggio spiralling up from ear height to above your head over four bars, then dropping back for the next round.",
      fm(algo=1, modratio=2.0, index=0.3, op1ratio=1, op2ratio=2, op3ratio=4, op4ratio=1, op1level=1.0, op2level=0.5, op3level=0.3,
         op4level=0.2, op5level=0.0, op6level=0.0, velidx=0.3),
      amp(0.004, 0.9, 0.0, 0.5), mod(0.001, 0.6, 0.0, 0.3, 0.35), comp(1, -8.0, 6.0, 0.1, 30.0), dly(2, 0.375, 0.35, 0.45, 1, 0.25), rev(3, 0.75, 0.45, 0.02, 0.24),
      space("spiral", "4 bars", size=0.65, dist=2.4, spread=20.0, room=0.4, roomsize=0.45, doppler=0.2),
      arp("up", 3, 0.75, "1/16", latch=1), voices=8),

    # ================================================================ BASS (3): the mid range moves, the sub stays mono
    S("Orbit Reese", ["Bass", "3D"], "wavetable",
      "Neurofunk reese whose growling mid range circles your head every two bars while the sub below 160 Hz stays dead "
      "centre and mono - club-safe movement.",
      wt(table="classic", pos=0.5, tone=0.72, unison=0.5, univoices=6, uniwidth=0.7, sub=0.5, keytrack=0.2, lfopos=0.2),
      amp(0.003, 0.5, 1.0, 0.12), mod(0.01, 0.4, 0.5, 0.2, 0.05), lfo(0.3, 0.4, "tri", "timbre"), fdrive(1, 5000, 0.2, 0.45),
      squash(3, 0.7, 0.35, 0.6, -2, 1, 1),
      space("circle", "2 bars", size=0.1, dist=1.2, spread=30.0, room=0.12, roomsize=0.3, doppler=0.15, lowkeep=160.0),
      expressive(), MONO),
    S("Neuro Swirl", ["Bass", "3D"], "wavetable",
      "Talking formant growl swirling in a figure eight once a bar - the vowel moves, the space moves, the sub does not.",
      wt(table="formant", pos=0.35, tone=0.7, unison=0.3, univoices=3, fm=0.3, fmratio=1.0, envfm=0.3, sub=0.4, keytrack=0.2,
         lfopos=0.5),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.35, 0.4, 0.2, 0.1), lfo(3.0, 0.7, "tri", "timbre"),
      formant(2, 0.2, 0.0, 0.6, 1.0, "1/8", 0.9, 0.7), squash(3, 0.8, 0.35, 0.7, -2, 2, 2),
      space("figure8", "1 bar", size=0.45, dist=1.3, spread=25.0, room=0.1, roomsize=0.3, doppler=0.2, lowkeep=180.0),
      expressive(), MONO),
    S("Pendulum Wobble", ["Bass", "3D"], "wavetable",
      "Half-time dubstep wobble that swings over your head and back with the bar: the wobble and the space breathe together.",
      wt(table="classic", pos=0.5, tone=0.78, unison=0.4, univoices=5, uniwidth=0.6, sub=0.45, keytrack=0.2),
      amp(0.003, 0.5, 1.0, 0.1), mod(0.01, 0.4, 0.5, 0.2, 0.0), wobble(2, "1/8", "sine", 110, 3000, 0.55, "lp", 0.35),
      squash(3, 0.6, 0.35, 0.5),
      space("pendulum", "1 bar", size=0.6, dist=1.4, spread=30.0, room=0.12, roomsize=0.3, doppler=0.1, lowkeep=170.0),
      expressive(), MONO),

    # ================================================================ FX and LEAD (3)
    S("Spiral Riser", ["FX", "3D"], "analog",
      "Hold a key: noise and saws climb for four seconds while spiralling from below your ears to high above your head.",
      an(shape=0.0, detune=0.4, cutoff=650, reso=0.35, oscmix=0.5, noise=0.6, drive=0.3, keytrack=0.3),
      amp(1.4, 1.0, 1.0, 2.0), mod(3.0, 0.5, 1.0, 1.0, 1.0), pitchenv(-12, 4.0, 0.0), flanger(2, 0.3, 0.7, 0.5, 3.0, 0.4),
      rev(3, 0.9, 0.4, 0.04, 0.35), comp(4, -6.0, 8.0, 0.1, 180.0, makeup=2.0),
      space("spiral", "2 bars", size=1.0, dist=2.2, spread=60.0, room=0.4, roomsize=0.6, doppler=0.1),
      {"vol": 1.0}, voices=2),
    S("Doppler Fly-By", ["FX", "3D"], "wavetable",
      "A siren tone racing past from left to right and back with every note - the pitch bends with the Doppler effect as it passes.",
      wt(table="digital", pos=0.3, tone=0.7, unison=0.3, univoices=3, uniwidth=0.5, keytrack=0.5),
      amp(0.05, 0.5, 1.0, 0.6), mod(0.01, 0.3, 0.5, 0.3, 0.2), lfo(5.5, 0.08, "sine", "pitch"), rev(3, 0.7, 0.4, 0.02, 0.22),
      comp(4, -6.0, 8.0, 0.1, 180.0, makeup=1.8),
      space("flyby", "2 bars", size=0.8, dist=2.0, spread=10.0, room=0.3, roomsize=0.6, doppler=1.0, air=0.8, rolloff=0.4),
      {"vol": 1.0}, voices=2),
    S("Binaural Lead", ["Leads", "3D"], "wavetable",
      "Expressive lead placed in front of you: slide moves it up over your head, pressure brings it right up to your ear, "
      "a slow drift keeps it alive. Made for MPE controllers and headphones.",
      wt(table="classic", pos=0.5, tone=0.72, unison=0.3, univoices=4, uniwidth=0.6, keytrack=0.5, envpos=0.1),
      amp(0.005, 0.5, 0.9, 0.3), mod(0.01, 0.3, 0.6, 0.2, 0.15), lfo(5.2, 0.05, "sine", "pitch"), fdrive(1, 7000, 0.2, 0.3),
      dly(2, 0.375, 0.35, 0.45, 1, 0.2), rev(3, 0.6, 0.5, 0.02, 0.18),
      space("random", "8 bars", size=0.2, az=0.0, dist=1.6, spread=20.0, room=0.25, roomsize=0.4, doppler=0.2),
      expressive(slide=0.5, press=-0.25), MONO, glide=0.02),
]

# 3D motion in two factory projects (applied by make_factory.py on top of factory_projects.py)
SPACE_PROJECTS = {
    "Neuro Engine": {
        "tags": ["3D"],
        "comment": " In 3D: the growl answers swirl around your head, the pad drifts above you (best with headphones).",
        "params": {**{"p1." + k: v for k, v in space("figure8", "1 bar", size=0.45, dist=1.3, spread=25.0, room=0.1, roomsize=0.3,
                                                        doppler=0.2, lowkeep=180.0).items()},
                   **{"p3." + k: v for k, v in space("random", "8 bars", size=0.8, el=15.0, dist=3.5, spread=70.0, room=0.35,
                                                        roomsize=0.6, doppler=0.1).items()}},
    },
    "Evolving Space": {
        "tags": ["3D"],
        "comment": " In 3D: the chords circle you slowly, the starfield bells scatter all around and the drone sways overhead "
                   "(best with headphones).",
        "params": {**{"p1." + k: v for k, v in space("circle", "16 bars", size=0.35, dist=3.0, spread=80.0, room=0.35, roomsize=0.7,
                                                        doppler=0.1).items()},
                   **{"p3." + k: v for k, v in space("random", "2 bars", size=1.0, dist=5.0, spread=30.0, room=0.5, roomsize=0.8,
                                                        doppler=0.3).items()},
                   **{"p4." + k: v for k, v in space("pendulum", "32 bars", size=0.7, dist=4.0, spread=90.0, room=0.4,
                                                        roomsize=0.8, doppler=0.0).items()}},
        # the distance attenuation of the three moving parts (3 - 5 m) costs ~4 dB against the flat mix: made up on the part gains
        "gain_offsets": {"p1": 3.0, "p3": 3.0, "p4": 3.0},
    },
}


def apply_project(p):
    """Adds the 3D motion of SPACE_PROJECTS to a project preset dict (name, tags, comment, params)."""
    extra = SPACE_PROJECTS.get(p["name"])
    if extra is None:
        return p
    p = dict(p, params=dict(p["params"], **extra["params"]), tags=list(p["tags"]) + [t for t in extra["tags"] if t not in p["tags"]])
    for part, db in extra.get("gain_offsets", {}).items():
        p["params"][part + ".gain"] = round(p["params"].get(part + ".gain", 0.0) + db, 1)
    if extra["comment"] not in p["comment"]:
        p["comment"] = p["comment"] + extra["comment"]
    return p
