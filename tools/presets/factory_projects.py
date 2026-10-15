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
KITS = {"studio": 0, "tight": 1, "room": 2, "lofi": 3, "hybrid": 4, "boom": 5}
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


CHORD = {"off": 0, "triad": 1, "seventh": 2}  # "pN.chord" (one-finger chords, diatonic in the key)
DELAY_SYNC = {"off": 0, "1/16": 1, "1/8T": 2, "1/8": 3, "1/4T": 4, "1/8.": 5, "1/4": 6, "1/4.": 7, "1/2": 8}


def lane_steps(lane, cells):
    """Orbit steps of one lane from a list of None / (note, vel) per step."""
    return [{"lane": lane, "i": i, "on": True, "note": c[0], "vel": c[1]} for i, c in enumerate(cells) if c is not None]


def held(notes_per_step, vel=96, last_gate=0.9, accent=None):
    """Step-sequencer steps for held notes: a list of (note, steps[, vel]) / None for one-step rests.
    Every note is tied over its steps (gate 100 %), its last step ends at `last_gate`."""
    out, i = [], 0
    for n in notes_per_step:
        if n is None:
            i += 1
            continue
        note, length = n[0], n[1]
        v = n[2] if len(n) > 2 else vel
        for k in range(length):
            out.append({"i": i + k, "on": True, "note": note, "vel": v, "gate": 1.0 if k < length - 1 else last_gate,
                        "prob": 100, "ratchet": 1})
        i += length
    return out


def ring_vel(*vels):
    return {"seq.euclid.r%d.vel" % (i + 1): v for i, v in enumerate(vels)}


def lane_parts(*parts):
    return {"seq.orbit.l%d.part" % (i + 1): p for i, p in enumerate(parts)}


# ---------------------------------------------------------------------------------------------
# 1. Lo-Fi Beat (0.5 rework): Orbit lanes 1-3 -> Lo-Fi kit (swung, humanised, ghost notes), Orbit lane 4 ->
#    round bass, step sequencer -> dusty electric piano chords (one-finger sevenths, tied), Euclid -> shaker
#    and a second-bar kick on the kit. Dbmaj7 | Cm7 in F minor.
DB2, C2_L, AB1, EB2, G1_L, BB1 = 37, 36, 32, 39, 31, 34
LOFI_KICK = [(KICK, 118), None, None, None, None, None, (KICK, 82), None, None, None, (KICK, 104), None, None, None, None, None]
LOFI_SNARE = [None, None, None, None, (SNARE, 112), None, None, (GHOST, 34), None, (GHOST, 26), None, None,
              (SNARE, 106), None, None, (GHOST, 40)]
LOFI_HATS = [(HAT, 74), None, (HAT, 46), (HAT, 24), (HAT, 66), None, (HAT, 44), None, (HAT, 70), None, (HAT, 48), None,
             (HAT, 64), (HAT, 28), (OPENHAT, 40), None]
LOFI_BASS = [(DB2, 100), (DB2, 90), (DB2, 90), None, (DB2, 92), None, (AB1, 86), (C2_L, 84),
             (C2_L, 100), (C2_L, 90), (C2_L, 90), None, (C2_L, 90), (EB2, 84), (G1_L, 86), (C2_L, 82)]
LOFI_KEYS = held([(49, 6, 86), None, (49, 2, 70), None, (49, 5, 80), None,
                  (48, 6, 84), None, (48, 2, 68), None, (48, 4, 78), None], last_gate=0.85) + [
    {"i": 31, "on": True, "note": 49, "vel": 72, "gate": 1.0, "prob": 100, "ratchet": 1}]  # pushed into the next bar (tied)
LOFI = project(
    "Lo-Fi Beat", ["Drums", "Keys", "Bass"],
    "Swung, humanised boom-bap with ghost notes on the dry Lo-Fi kit (Orbit lanes 1-3), a round bass on Orbit lane 4, "
    "dusty electric-piano sevenths from the step sequencer (one-finger chords, tied), shaker from Euclid. Press Play.",
    {"tempo": 86, "drum.kit": KITS["lofi"], "key.root": 5, "key.scale": 2,
     "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 32, "seq.step.swing": 0.32},
    part(1, "Dusty EP", chord=CHORD["seventh"]), part(2, "Lofi Bass"), part(3, "Music Box"), part(4, "Breath Choir Pad"),
    {"seq.orbit.on": 1, "seq.orbit.drums": 1, "seq.orbit.gate": 1.0, "seq.orbit.swing": 0.32, "seq.orbit.human": 0.3},
    lanes([16, 16, 16, 16], ["1/16", "1/16", "1/16", "1/8"]), lane_parts(0, 0, 0, 2),
    {"seq.euclid.on": 1, "seq.euclid.drums": 1, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.5, "seq.euclid.vel": 40,
     "seq.euclid.swing": 0.32, "seq.euclid.human": 0.45},
    rings([(16, 4, 2, SHAKER), (32, 1, 30, KICK)]), ring_vel(30, 70),
    mixer(ch1=(0, 0.0, 0.16), ch2=(-5, 0.0, 0.0), ch3=(-8, 0.15, 0.3), ch4=(-10, 0, 0.35), ch5=(1, 0.0, 0.03), master=-3),
    {"mix.master.tapesat": 0.12,
     "fxb.reverb.size": 0.45, "fxb.reverb.damping": 0.7},
    sequencer={"steps": LOFI_KEYS, "lengths": {"1": 32}},
    orbit=lane_steps(0, LOFI_KICK) + lane_steps(1, LOFI_SNARE) + lane_steps(2, LOFI_HATS) + lane_steps(3, LOFI_BASS))

# ---------------------------------------------------------------------------------------------
# 2. Techno Groove (0.5 rework): Orbit -> Hybrid kit (four-on-the-floor, claps and rims, accented 16th hats,
#    off-beat open hat), step sequencer -> rolling bass, Euclid -> dark one-finger seventh stabs with a drifting
#    filter, synced dotted-eighth delay. C minor.
TECH_KICK = [(KICK, 124) if i % 4 == 0 else None for i in range(16)]
TECH_CLAP = [None, None, None, None, (CLAP, 112), None, None, (RIM, 52), None, None, (RIM, 46), None,
             (CLAP, 110), None, None, (RIM, 58)]
TECH_HATS = [((HAT, {0: 56, 1: 42, 3: 88}[i % 4]) if i % 4 != 2 else None) for i in range(16)]
TECH_OPEN = [((OPENHAT, 100) if i % 4 == 2 else None) for i in range(16)]
def rolling(root, alt=None):
    bar = []
    for beat in range(4):
        n = alt[beat] if alt and alt[beat] else root
        bar += [None, (n, 84), (n, 112), (n, 80)]
    return bar
TECHNO = project(
    "Techno Groove", ["Drums", "Bass"],
    "Driving four-on-the-floor on the Hybrid kit from Orbit (claps on 2 and 4, rims, accented 16th hats, off-beat open hat), "
    "a rolling bass from the step sequencer and dark seventh stabs from Euclid with a slowly drifting filter and a synced dotted-eighth delay.",
    {"tempo": 128, "drum.kit": KITS["hybrid"], "seq.orbit.drums": 1, "seq.orbit.on": 1, "seq.orbit.gate": 0.5,
     "seq.step.channel": 2, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 32, "key.root": 0, "key.scale": 2},
    lanes([16, 16, 16, 16], ["1/16"] * 4),
    part(1, "Noise Riser"), part(2, "Rolling Bass"), part(3, "Dub Stab", chord=CHORD["seventh"]), part(4, "Night Drive"),
    {"seq.euclid.on": 1, "seq.euclid.part": 3, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.35, "seq.euclid.vel": 92,
     "seq.euclid.human": 0.15},
    rings([(16, 3, 3, 48), (32, 2, 22, 44)]), ring_vel(96, 84),
    mixer(ch1=(-14, 0.1, 0.3), ch2=(-1, 0.0, 0.0), ch3=(-2, 0.32, 0.3), ch4=(-12, 0.2, 0.3), ch5=(0, 0.0, 0.04), master=-1),
    {"fxa.delay.sync": DELAY_SYNC["1/8."], "fxa.delay.feedback": 0.42, "fxa.delay.tone": 0.4,
     "fxb.reverb.size": 0.75, "fxb.reverb.damping": 0.55,
     "mix.master.comp": 1, "mix.master.comp.thresh": -14, "mix.master.comp.ratio": 2.5},
    sequencer={"steps": note_steps([(n[0], n[1]) if n else None for n in rolling(36) + rolling(36, [None, None, None, 34])], gate=0.55),
               "lengths": {"1": 32}},
    orbit=lane_steps(0, TECH_KICK) + lane_steps(1, TECH_CLAP) + lane_steps(2, TECH_HATS) + lane_steps(3, TECH_OPEN))

# ---------------------------------------------------------------------------------------------
# 3. Ambient Drift: step -> slow fifth pad, Orbit -> generative kalimba, Euclid -> glass tones
AMBIENT = project(
    "Ambient Drift", ["Pads", "Textures"],
    "A slow fifths pad, a generative kalimba from four Orbit lanes of 5, 7, 11 and 16 steps with a tempo-synced dotted-eighth delay, "
    "sparse singing-glass tones from Euclid. No drums.",
    {"tempo": 72, "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/4"], "seq.step.length": 16, "key.root": 0, "key.scale": 11},
    part(1, "Fifth Cloud"), part(2, "Kalimba"), part(3, "Singing Glass"), part(4, "Harp Mist"),
    {"seq.orbit.on": 1, "seq.orbit.part": 2, "seq.orbit.gate": 0.6}, lanes([5, 7, 11, 16], ["1/8", "1/8", "1/8", "1/16"]),
    {"seq.euclid.on": 1, "seq.euclid.part": 3, "seq.euclid.rate": RATE["1/8"], "seq.euclid.gate": 1.0, "seq.euclid.vel": 80},
    rings([(13, 2, 0, 67), (19, 2, 7, 72)]),
    mixer(ch1=(6, 0.0, 0.35), ch2=(-11, 0.5, 0.4), ch3=(-8, 0.15, 0.5), ch4=(-2, 0, 0.5), master=4),
    {"fxb.reverb.size": 0.9, "fxb.reverb.damping": 0.35, "fxb.reverb.predelay": 0.04, "fxa.delay.sync": DELAY_SYNC["1/8."],
     "fxa.delay.time": 0.625, "fxa.delay.feedback": 0.4, "fxa.delay.tone": 0.35},
    sequencer={"steps": note_steps([(48, 90, 1.0), None, None, None, (44, 86, 1.0), None, None, None,
                                    (51, 88, 1.0), None, None, None, (46, 84, 1.0), None, None, None]), "lengths": {"1": 16}},
    orbit=orbit_notes(0, [72, None, 75, None, 79]) + orbit_notes(1, [None, 67, None, None, 70, None, None], 90)
    + orbit_notes(2, [84, None, None, None, None, 82, None, None, None, None, None], 70)
    + orbit_notes(3, [None] * 6 + [60] + [None] * 7 + [63] + [None], 80))

# ---------------------------------------------------------------------------------------------
# 4. Synth-Pop (0.5 rework): Euclid -> Tight kit (four-on-the-floor, snare + clap on 2 and 4, eighth hats),
#    Orbit lane 1 -> octave-bouncing bass, lane 2 -> lush poly brass (one-finger triads, tied), lane 3 ->
#    sparkle arpeggio, step sequencer -> the lead melody. Am - F - C - G (vi-IV-I-V), two beats each.
SP_ROOTS = [(45, 57, 33), (41, 53, 29), (48, 60, 36), (43, 55, 31)]  # (bass high, chord root, bass low)
SP_BASS, SP_CHORD, SP_ARP = [], [], []
_arp = {57: [76, 81, 84, 81], 53: [72, 77, 81, 77], 60: [76, 79, 84, 79], 55: [74, 79, 83, 79]}
for hi, root, lo in SP_ROOTS:
    SP_BASS += [(lo, 112), (lo + 12, 92), (lo, 104), (lo + 12, 90)]
    SP_CHORD += [(root, 88)] * 4
    SP_ARP += [(n, 84 if k == 0 else 70) for k, n in enumerate(_arp[root])]
E5, D5, C5, A4, G5, B4 = 76, 74, 72, 69, 79, 71
SP_LEAD = held([(E5, 4, 110), (D5, 2, 96), (C5, 2, 98), (C5, 4, 104), (A4, 2, 92), (C5, 2, 96),
                (E5, 6, 108), (G5, 2, 100), (D5, 6, 104), None, None,
                (E5, 4, 110), (D5, 2, 96), (C5, 2, 98), (C5, 2, 100), (D5, 2, 96), (E5, 4, 104),
                (G5, 4, 110), (E5, 4, 100), (D5, 6, 104), (B4, 2, 94)], last_gate=0.8)
SYNTHPOP = project(
    "Synth-Pop", ["Drums", "Bass", "Leads"],
    "Bright four-on-the-floor on the Tight kit from Euclid (snare and clap on 2 and 4, eighth hats), an octave-bouncing bass, "
    "lush poly brass and a sparkle arpeggio from three Orbit lanes over Am-F-C-G, and a lead melody from the step sequencer.",
    {"tempo": 116, "drum.kit": KITS["tight"], "key.root": 9, "key.scale": 2,
     "seq.step.channel": 3, "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 64},
    part(1, "Pop Brass", chord=CHORD["triad"]), part(2, "Bounce Bass"), part(3, "Pop Lead"), part(4, "Sparkle Bell"),
    {"seq.euclid.on": 1, "seq.euclid.drums": 1, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.5, "seq.euclid.vel": 100,
     "seq.euclid.human": 0.12},
    rings([(16, 4, 0, KICK), (16, 2, 4, SNARE), (16, 8, 0, HAT), (16, 2, 4, CLAP)]), ring_vel(116, 106, 66, 72),
    {"seq.orbit.on": 1, "seq.orbit.part": 2, "seq.orbit.gate": 1.0}, lanes([16, 16, 16, 1], ["1/8", "1/8", "1/8", "1/4"]),
    lane_parts(2, 1, 4, 0),
    mixer(ch1=(0, 0.1, 0.28), ch2=(-1, 0.0, 0.03), ch3=(-6.5, 0.22, 0.18), ch4=(-7, 0.35, 0.3), ch5=(4, 0.0, 0.14), master=-3),
    {"mix.ch5.comp": 0.5, "fxa.delay.sync": DELAY_SYNC["1/8."], "fxa.delay.feedback": 0.35, "fxa.delay.tone": 0.5,
     "fxb.reverb.size": 0.5, "fxb.reverb.damping": 0.45,
     "mix.master.comp": 1, "mix.master.comp.thresh": -15, "mix.master.comp.ratio": 2.2},
    sequencer={"steps": SP_LEAD, "lengths": {"1": 64}},
    orbit=lane_steps(0, SP_BASS) + lane_steps(1, SP_CHORD) + lane_steps(2, SP_ARP))

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

# ---------------------------------------------------------------------------------------------
# 8. Sub & Glide (0.5): step sequencer -> sliding string chords (poly glide, one-finger sevenths, tied),
#    Orbit lane 1 -> long sliding sub, lanes 2-4 -> half-time beat on the Boom kit, Euclid -> sparkle bell.
SG_CHORDS = held([(48, 4, 90), (44, 4, 88), (51, 4, 90), (46, 4, 86)], last_gate=1.0)
SG_SUB = [(36, 110), (36, 100), (36, 100), (43, 96), (32, 110), (32, 100), (32, 100), (39, 96),
          (39, 110), (39, 100), (39, 100), (34, 96), (34, 110), (34, 100), (46, 96), (34, 100)]
SG_KICK = [(KICK, 120) if i in (0, 10) else None for i in range(16)]
SG_SNARE = [((SNARE, 112) if i == 8 else (CLAP, 70) if i == 15 else None) for i in range(16)]
SG_HATS = [(HAT, (78 if i % 4 == 0 else 52 if i % 2 == 0 else 34)) for i in range(16)]
SUBGLIDE = project(
    "Sub & Glide", ["Pads", "Bass", "Drums"],
    "Seventh chords that slide into each other (Glide Strings, poly glide), a long sliding sub on Orbit lane 1 and a half-time beat "
    "on the Boom kit from Orbit lanes 2-4, sparkle bells from Euclid. Cm7 - Abmaj7 - Ebmaj7 - Bb7.",
    {"tempo": 80, "drum.kit": KITS["boom"], "key.root": 0, "key.scale": 2,
     "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/4"], "seq.step.length": 16},
    part(1, "Glide Strings", chord=CHORD["seventh"]), part(2, "Long Sub"), part(3, "Sparkle Bell"), part(4, "Glass Glide"),
    {"seq.orbit.on": 1, "seq.orbit.drums": 1, "seq.orbit.gate": 1.0, "seq.orbit.human": 0.15},
    lanes([16, 16, 16, 16], ["1/4", "1/16", "1/16", "1/16"]), lane_parts(2, 0, 0, 0),
    {"seq.euclid.on": 1, "seq.euclid.part": 3, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.4, "seq.euclid.vel": 74},
    rings([(16, 3, 2, 79), (32, 2, 9, 84)]),
    mixer(ch1=(-4, 0.1, 0.35), ch2=(2, 0.0, 0.0), ch3=(-8, 0.4, 0.3), ch4=(-8, 0, 0.4), ch5=(-1, 0.0, 0.06), master=-1),
    {"fxa.delay.sync": DELAY_SYNC["1/8."], "fxa.delay.feedback": 0.4, "fxa.delay.tone": 0.45, "fxb.reverb.size": 0.8,
     "fxb.reverb.damping": 0.5, "mix.master.comp": 1, "mix.master.comp.thresh": -15, "mix.master.comp.ratio": 2.0},
    sequencer={"steps": SG_CHORDS, "lengths": {"1": 16}},
    orbit=lane_steps(0, SG_SUB) + lane_steps(1, SG_KICK) + lane_steps(2, SG_SNARE) + lane_steps(3, SG_HATS))

PROJECTS = [LOFI, TECHNO, AMBIENT, SYNTHPOP, POLY, ARP, HOUSE, SUBGLIDE]
