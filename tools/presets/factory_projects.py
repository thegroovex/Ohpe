# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Factory project presets / demo patterns (0.4, package F): whole-instrument set-ups built from
the factory part presets, with the step sequencer, Euclid and Orbit (one of them playing the
drum kit) and the mixer's send effects. Written by make_factory.py."""
import json, os

import factory_parts

HERE = os.path.dirname(os.path.abspath(__file__))
_levels_path = os.path.join(HERE, "levels.json")
LEVELS = json.load(open(_levels_path)) if os.path.exists(_levels_path) else {}
BY_NAME = {p["name"]: p for p in factory_parts.PRESETS}

# drum pads (drum.base 36): note = 35 + pad
KICK, KICK2, SNARE, RIM, CLAP, HAT, OPENHAT, PEDAL, TOMLO, TOMHI, TOMFL, XSTICK, RIDE, BELL, CRASH, SIZZLE, HALFHAT, GHOST, \
    COWBELL, TAMB, SHAKER, CLAVES, CONGA, BONGO = range(36, 60)
KITS = {"studio": 0, "tight": 1, "room": 2, "lofi": 3, "hybrid": 4}
RATE = {"1/4": 0, "1/8": 1, "1/16": 2, "1/32": 3, "1/4T": 4, "1/8T": 5, "1/16T": 6, "1/32T": 7, "1/4.": 8, "1/8.": 9, "1/16.": 10}
STEP_RATE = {"1/4": 0, "1/8": 1, "1/16": 2, "1/32": 3, "1/8T": 4, "1/16T": 5}


def part(n, preset_name, **override):
    """Parameters of factory part preset `preset_name` for part n (1-4), with its calibrated gain."""
    p = BY_NAME[preset_name]
    params = {}
    for k, v in p["params"].items():
        if k.startswith("fx.") and n != 1:
            continue  # the FX slot exists on part 1 only
        params["p%d.%s" % (n, k)] = v
    params.setdefault("p%d.gain" % n, round(LEVELS.get(preset_name, 0.0), 1))
    for k, v in override.items():
        params["p%d.%s" % (n, k.replace("__", "."))] = v
    return params


def steps(pattern, note_map, vel=None, gate=0.5):
    """Step-sequencer steps from a string: one character per step, '.' = rest."""
    out = []
    for i, c in enumerate(pattern):
        if c == "." or c == " ":
            continue
        note, v = note_map[c] if isinstance(note_map[c], tuple) else (note_map[c], 100)
        out.append({"i": i, "on": True, "note": note, "vel": vel or v, "gate": gate, "prob": 100, "ratchet": 1})
    return out


def note_steps(notes, gate=0.5, vel=100):
    """Step-sequencer steps from a list (None = rest, int = note, (note, vel[, gate]) tuple)."""
    out = []
    for i, n in enumerate(notes):
        if n is None:
            continue
        if isinstance(n, tuple):
            note, v = n[0], n[1]
            g = n[2] if len(n) > 2 else gate
        else:
            note, v, g = n, vel, gate
        out.append({"i": i, "on": True, "note": note, "vel": v, "gate": g, "prob": 100, "ratchet": 1})
    return out


def orbit_lane(lane, pattern, note, vel_map=None):
    """Orbit steps of one lane from a string ('x' = hit, 'X' = accent, '.' = rest)."""
    out = []
    for i, c in enumerate(pattern):
        if c in "xXo":
            v = {"x": 96, "X": 120, "o": 70}[c] if vel_map is None else vel_map.get(c, 100)
            out.append({"lane": lane, "i": i, "on": True, "note": note if not isinstance(note, list) else note[i], "vel": v})
    return out


def orbit_notes(lane, notes, vel=100):
    out = []
    for i, n in enumerate(notes):
        if n is not None:
            out.append({"lane": lane, "i": i, "on": True, "note": n, "vel": vel})
    return out


def lanes(lengths, rates, transposes=(0, 0, 0, 0)):
    p = {}
    for i, (l, r, t) in enumerate(zip(lengths, rates, transposes)):
        p["seq.orbit.l%d.len" % (i + 1)] = l
        p["seq.orbit.l%d.rate" % (i + 1)] = RATE[r]
        p["seq.orbit.l%d.transpose" % (i + 1)] = t
    return p


def rings(spec):
    """Euclid rings: [(steps, pulses, rotation, note)] x up to 4."""
    p = {}
    for i, (st, pu, rot, note) in enumerate(spec):
        p.update({"seq.euclid.r%d.steps" % (i + 1): st, "seq.euclid.r%d.pulses" % (i + 1): pu,
                  "seq.euclid.r%d.rot" % (i + 1): rot, "seq.euclid.r%d.note" % (i + 1): note})
    for i in range(len(spec), 4):
        p["seq.euclid.r%d.pulses" % (i + 1)] = 0
    return p


def mixer(**kw):
    """mixer(ch1=(vol dB, send A delay, send B reverb, pan), ..., master=dB)"""
    p = {}
    for k, v in kw.items():
        if k == "master":
            p["mix.master.vol"] = v
            continue
        ch = int(k[2:])
        vol, a, b = v[0], v[1], v[2]
        p["mix.ch%d.vol" % ch] = vol
        p["mix.ch%d.senda" % ch] = a
        p["mix.ch%d.sendb" % ch] = b
        if len(v) > 3:
            p["mix.ch%d.pan" % ch] = v[3]
    return p


def project(name, tags, comment, *dicts, sequencer=None, orbit=None, engine=None):
    params = {}
    for d in dicts:
        params.update(d)
    params["seq.run"] = 0
    first = next((v for k, v in params.items() if k == "p1.engine"), 2)
    eng = engine or [k for k, v in factory_parts.ENGINES.items() if v == first][0]
    sections = {}
    if sequencer is not None:
        sections["sequencer"] = {"format": "ohpe-step-pattern", "version": 1, **sequencer}
    if orbit is not None:
        sections["orbit"] = {"format": "ohpe-orbit", "version": 1, "steps": orbit}
    return {"name": name, "tags": tags, "engine": eng, "comment": comment, "params": params, "sections": sections}


# ---------------------------------------------------------------------------------------------
# 1. Lo-Fi Beat: step sequencer -> drums (swung), Orbit -> electric piano chords, Euclid -> bass
F1, C2, F2, AB2, BB2, C3, EB3, F3, G3, AB3, BB3, C4, D4, EB4 = 29, 36, 41, 44, 46, 48, 51, 53, 55, 56, 58, 60, 62, 63
lofi_drums = note_steps([
    (KICK, 112), None, (HAT, 60), None, (SNARE, 104), None, (HAT, 56), (KICK, 84),
    None, (KICK, 100), (HAT, 62), None, (SNARE, 108), None, (HAT, 52), (GHOST, 46),
    (KICK, 110), None, (HAT, 60), None, (SNARE, 104), None, (HAT, 56), None,
    (KICK, 96), None, (HAT, 64), (KICK, 80), (SNARE, 110), None, (OPENHAT, 58), None], gate=0.3)
# Fm9 (2 bars) -> Bbm9/Ab ... voiced for the Orbit lanes (each lane one chord tone, quarter notes)
CH1 = [AB3, EB4, G3, C4]     # Fm9 upper structure: Ab C Eb G
CH2 = [AB3, C4 + 1 - 1, F3, BB3]  # Bb9 colour: Ab C F Bb
def chord_lane(lane):
    notes = [None] * 16
    for bar, chord in enumerate([CH1, CH1, CH2, CH2]):
        notes[bar * 4] = chord[lane]
        notes[bar * 4 + 2] = chord[lane] if bar % 2 == 0 else None
        notes[bar * 4 + 3] = chord[lane] if bar % 2 == 1 else None
    return notes
LOFI = project(
    "Lo-Fi Beat", ["Drums", "Keys"],
    "Swung boom-bap on the Lo-Fi kit (step sequencer -> drums), electric-piano chords from Orbit, an Euclid bass. Press Play.",
    {"tempo": 84, "drum.kit": KITS["lofi"], "seq.step.drums": 1, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 32,
     "seq.step.swing": 0.3, "key.root": 5, "key.scale": 2},
    part(1, "Tine Piano"), part(2, "Upright Pluck"), part(3, "Music Box"), part(4, "Breath Choir Pad"),
    {"seq.orbit.on": 1, "seq.orbit.part": 1, "seq.orbit.gate": 0.9}, lanes([16] * 4, ["1/4"] * 4),
    {"seq.euclid.on": 1, "seq.euclid.part": 2, "seq.euclid.rate": RATE["1/8"], "seq.euclid.gate": 0.6, "seq.euclid.vel": 104},
    rings([(16, 3, 0, F1), (16, 2, 3, C2), (32, 2, 13, F2)]),
    mixer(ch1=(-3, 0.0, 0.22), ch2=(-1, 0.0, 0.05), ch3=(-6, 0.15, 0.3), ch4=(-8, 0, 0.35), ch5=(0, 0.0, 0.08), master=2),
    {"mix.master.tapesat": 0.35, "fxb.reverb.size": 0.55, "fxb.reverb.damping": 0.6},
    sequencer={"steps": lofi_drums, "lengths": {"1": 32}},
    orbit=sum((orbit_notes(l, chord_lane(l), 86) for l in range(4)), []))

# ---------------------------------------------------------------------------------------------
# 2. Techno Groove: Orbit -> drums (Hybrid kit), Euclid -> acid line, step -> minor stabs
C1, C2_, DS2, G2, C3_ = 24, 36, 39, 43, 48
TECHNO = project(
    "Techno Groove", ["Drums", "Bass"],
    "Four-on-the-floor on the Hybrid kit from Orbit (a 12-step rim lane drifts against it), an Euclid acid line and off-beat stabs.",
    {"tempo": 128, "drum.kit": KITS["hybrid"], "seq.orbit.drums": 1, "seq.orbit.on": 1, "seq.orbit.gate": 0.5,
     "seq.step.channel": 3, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 16, "key.root": 0, "key.scale": 2},
    lanes([16, 16, 16, 12], ["1/16"] * 4),
    part(1, "Noise Riser"), part(2, "Acid Line"), part(3, "Minor Stab"), part(4, "Super Lead"),
    {"seq.euclid.on": 1, "seq.euclid.part": 2, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.45, "seq.euclid.vel": 100},
    rings([(16, 7, 0, C2_), (16, 3, 2, C3_), (13, 3, 5, DS2), (11, 2, 1, G2)]),
    mixer(ch1=(-12, 0.1, 0.3), ch2=(-2, 0.12, 0.05), ch3=(-5, 0.25, 0.2), ch4=(-6, 0.2, 0.2), ch5=(0, 0.0, 0.05), master=-0.5),
    {"fxa.delay.time": 0.352, "fxa.delay.feedback": 0.35, "mix.master.comp": 1, "mix.master.comp.thresh": -14, "mix.master.comp.ratio": 2.5},
    sequencer={"steps": steps("..x...x...x...x.", {"x": (60, 100)}, gate=0.3), "lengths": {"1": 16}},
    orbit=orbit_lane(0, "X...x...X...x...", KICK) + orbit_lane(1, "....X.......X...", CLAP)
    + orbit_lane(2, "..x...x...x...x.", OPENHAT) + orbit_lane(2, "o.o.o.o.o.o.o.o.".replace("o.o", "o.o"), HAT)[0:0]
    + orbit_lane(3, "x..x.x..x.x.", RIM))

# ---------------------------------------------------------------------------------------------
# 3. Ambient Drift: step -> slow fifth pad, Orbit -> generative kalimba, Euclid -> glass tones
AMBIENT = project(
    "Ambient Drift", ["Pads", "Textures"],
    "A slow fifths pad, a generative kalimba from four Orbit lanes of 5, 7, 11 and 16 steps, sparse singing-glass tones from Euclid. No drums.",
    {"tempo": 72, "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/4"], "seq.step.length": 16, "key.root": 0, "key.scale": 11},
    part(1, "Fifth Cloud"), part(2, "Kalimba"), part(3, "Singing Glass"), part(4, "Harp Mist"),
    {"seq.orbit.on": 1, "seq.orbit.part": 2, "seq.orbit.gate": 0.6}, lanes([5, 7, 11, 16], ["1/8", "1/8", "1/8", "1/16"]),
    {"seq.euclid.on": 1, "seq.euclid.part": 3, "seq.euclid.rate": RATE["1/8"], "seq.euclid.gate": 1.0, "seq.euclid.vel": 80},
    rings([(13, 2, 0, 67), (19, 2, 7, 72)]),
    mixer(ch1=(6, 0.0, 0.35), ch2=(-11, 0.25, 0.45), ch3=(-8, 0.1, 0.5), ch4=(-2, 0, 0.5), master=4),
    {"fxb.reverb.size": 0.9, "fxb.reverb.damping": 0.35, "fxb.reverb.predelay": 0.04, "fxa.delay.time": 0.625, "fxa.delay.feedback": 0.45},
    sequencer={"steps": note_steps([(48, 90, 1.0), None, None, None, (44, 86, 1.0), None, None, None,
                                    (51, 88, 1.0), None, None, None, (46, 84, 1.0), None, None, None]), "lengths": {"1": 16}},
    orbit=orbit_notes(0, [72, None, 75, None, 79]) + orbit_notes(1, [None, 67, None, None, 70, None, None], 90)
    + orbit_notes(2, [84, None, None, None, None, 82, None, None, None, None, None], 70)
    + orbit_notes(3, [None] * 6 + [60] + [None] * 7 + [63] + [None], 80))

# ---------------------------------------------------------------------------------------------
# 4. Synth-Pop: Orbit -> drums (Tight kit), step -> octave bass over four chords, Euclid -> glass pluck
A1, F1_, C2b, G1 = 33, 29, 36, 31
def octave_bar(root):
    return [(root, 108), None, (root + 12, 90), None] * 4
SYNTHPOP = project(
    "Synth-Pop", ["Drums", "Plucks"],
    "Tight-kit groove from Orbit, an octave bass over Am-F-C-G (64-step pattern) and an Euclid glass pluck. Part 4 is a string pad to play along.",
    {"tempo": 112, "drum.kit": KITS["tight"], "seq.orbit.drums": 1, "seq.orbit.on": 1, "seq.orbit.gate": 0.5,
     "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 64, "key.root": 9, "key.scale": 2},
    lanes([16, 16, 16, 16], ["1/16"] * 4),
    part(1, "Punch FM Bass"), part(2, "Glass Pluck"), part(3, "Square Hero"), part(4, "Warm Strings"),
    {"seq.euclid.on": 1, "seq.euclid.part": 2, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.5, "seq.euclid.vel": 96},
    rings([(16, 5, 0, 72), (16, 3, 3, 76), (8, 1, 6, 79)]),
    mixer(ch1=(-1, 0.0, 0.05), ch2=(-5, 0.3, 0.25), ch3=(-4, 0.2, 0.2), ch4=(-7, 0.0, 0.3), ch5=(0, 0.0, 0.1), master=-3.5),
    {"fxa.delay.time": 0.402, "fxa.delay.feedback": 0.3},
    sequencer={"steps": note_steps(octave_bar(A1) + octave_bar(F1_ + 12) + octave_bar(C2b) + octave_bar(G1 + 12), gate=0.45),
               "lengths": {"1": 64}},
    orbit=orbit_lane(0, "X.......X.x.....", KICK) + orbit_lane(1, "....X.......X...", SNARE)
    + orbit_lane(2, "x.x.x.x.x.x.x.x.", HAT) + orbit_lane(3, "....x.......x..x", TAMB))

# ---------------------------------------------------------------------------------------------
# 5. Euclid Polyrhythm: Euclid -> drums (Room kit), Orbit -> marimba in 5/6/7/8, step -> pedal bass
POLY = project(
    "Euclid Polyrhythm", ["Drums", "Arp"],
    "Four Euclid rings of 8, 12, 16 and 9 steps on the Room kit, a marimba from Orbit lanes of 5, 6, 7 and 8 steps and a pedal bass.",
    {"tempo": 116, "drum.kit": KITS["room"], "seq.euclid.drums": 1, "seq.euclid.on": 1, "seq.euclid.rate": RATE["1/16"],
     "seq.euclid.gate": 0.5, "seq.euclid.vel": 104, "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/8"], "seq.step.length": 16,
     "key.root": 2, "key.scale": 3},
    rings([(8, 3, 0, KICK), (12, 5, 2, RIM), (16, 7, 1, HAT), (9, 4, 3, CONGA)]),
    part(1, "Upright Pluck"), part(2, "Marimba"), part(3, "Kalimba"), part(4, "Sus Horizon"),
    {"seq.orbit.on": 1, "seq.orbit.part": 2, "seq.orbit.gate": 0.5}, lanes([5, 6, 7, 8], ["1/16"] * 4),
    mixer(ch1=(-3.5, 0.0, 0.08), ch2=(-4, 0.15, 0.25), ch3=(-6, 0.2, 0.3), ch4=(-8, 0, 0.35), ch5=(0, 0.0, 0.1), master=-4.5),
    {"fxa.delay.time": 0.388, "fxa.delay.feedback": 0.3},
    sequencer={"steps": note_steps([(38, 110), None, None, (38, 90), None, None, (45, 96), None,
                                    (38, 110), None, None, (38, 90), None, (36, 96), None, None], gate=0.6), "lengths": {"1": 16}},
    orbit=orbit_notes(0, [62, None, 69, None, 72], 96) + orbit_notes(1, [None, 74, None, 77, None, None], 84)
    + orbit_notes(2, [65, None, None, 67, None, None, None], 78) + orbit_notes(3, [None, None, 81, None, None, None, 79, None], 70))

# ---------------------------------------------------------------------------------------------
# 6. Arp Sequence: step -> 64-step arpeggio, Orbit -> drums (Studio kit), Euclid -> pedal bass;
#    the arpeggiator waits on part 3 (latch) for chords you play
def arp_bar(chord):
    seq = [chord[0], chord[1], chord[2], chord[3], chord[2], chord[1]] * 3
    return [(n, 100 if i % 4 == 0 else 84) for i, n in enumerate(seq[:16])]
ARP = project(
    "Arp Sequence", ["Arp", "Drums"],
    "A 64-step arpeggio over Am, Fmaj7, G and Em on an A pedal, Studio-kit drums from Orbit. The arpeggiator is latched on part 3: play chords.",
    {"tempo": 120, "drum.kit": KITS["studio"], "seq.orbit.drums": 1, "seq.orbit.on": 1, "seq.orbit.gate": 0.5,
     "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 64,
     "seq.arp.on": 1, "seq.arp.part": 3, "seq.arp.mode": 2, "seq.arp.oct": 2, "seq.arp.rate": RATE["1/16"], "seq.arp.gate": 0.5,
     "seq.arp.latch": 1, "key.root": 9, "key.scale": 2},
    lanes([16, 16, 16, 16], ["1/16"] * 4),
    part(1, "Analog Arp"), part(2, "Round Sub"), part(3, "FM Arp Bell"), part(4, "Analog Dawn"),
    {"seq.euclid.on": 1, "seq.euclid.part": 2, "seq.euclid.rate": RATE["1/8"], "seq.euclid.gate": 0.8, "seq.euclid.vel": 100},
    rings([(8, 3, 0, 33), (16, 1, 14, 45)]),
    mixer(ch1=(-3, 0.25, 0.15), ch2=(-1, 0.0, 0.03), ch3=(-5, 0.3, 0.2), ch4=(-8, 0, 0.3), ch5=(0, 0.0, 0.08), master=-3),
    {"fxa.delay.time": 0.375, "fxa.delay.feedback": 0.35},
    sequencer={"steps": note_steps(arp_bar([57, 60, 64, 69]) + arp_bar([57, 60, 65, 69]) + arp_bar([59, 62, 67, 71])
                                   + arp_bar([59, 64, 67, 71]), gate=0.4), "lengths": {"1": 64}},
    orbit=orbit_lane(0, "X...x...X..x.x..", KICK) + orbit_lane(1, "....X.......X...", SNARE)
    + orbit_lane(2, "x.xox.xox.xox.xo".replace("o", "x"), HAT) + orbit_lane(3, "..............x.", OPENHAT))

# ---------------------------------------------------------------------------------------------
# 7. Deep House: Orbit -> drums (Tight kit), step -> organ bass, Euclid -> chord stabs
HOUSE = project(
    "Deep House", ["Drums", "Bass", "Keys"],
    "Shuffled hats and a deep kick on the Tight kit (Orbit), a syncopated organ bass (step sequencer) and Euclid chord stabs.",
    {"tempo": 122, "drum.kit": KITS["tight"], "seq.orbit.drums": 1, "seq.orbit.on": 1, "seq.orbit.gate": 0.5,
     "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 32, "seq.step.swing": 0.15,
     "key.root": 2, "key.scale": 3},
    lanes([16, 16, 16, 16], ["1/16"] * 4),
    part(1, "House Organ Bass"), part(2, "Velvet Stab", **{"amp.d": 0.35}), part(3, "Analog Dawn"), part(4, "Glass Keys"),
    {"seq.euclid.on": 1, "seq.euclid.part": 2, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.35, "seq.euclid.vel": 96},
    rings([(16, 3, 3, 62), (16, 3, 3, 65), (16, 3, 3, 69), (16, 3, 3, 72)]),
    mixer(ch1=(-1, 0.0, 0.03), ch2=(-6, 0.25, 0.25), ch3=(-9, 0, 0.3), ch4=(-6, 0.2, 0.25), ch5=(0, 0.0, 0.06), master=-2),
    {"fxa.delay.time": 0.369, "fxa.delay.feedback": 0.3},
    sequencer={"steps": note_steps([(38, 110), None, None, (38, 80), None, None, (50, 96), None,
                                    None, None, (38, 100), None, (45, 90), None, (48, 96), None] * 2, gate=0.4), "lengths": {"1": 32}},
    orbit=orbit_lane(0, "X...X...X...X...", KICK) + orbit_lane(1, "....X.......X...", CLAP)
    + orbit_lane(2, "..x...x...x...x.", OPENHAT) + orbit_lane(3, "x.ox.oxox.ox.oxo", SHAKER))

PROJECTS = [LOFI, TECHNO, AMBIENT, SYNTHPOP, POLY, ARP, HOUSE]
