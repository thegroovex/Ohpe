# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p44: the showcase morph presets, audio-to-wavetable presets and project presets (written by make_showcase.py).

* Morph: four showcase sounds in the corners of the morph pad; the part's own sound is corner C and the pad starts there,
  the mod wheel moves it right (C -> D), pressure moves it up (C -> A). The corners' own wheel routings are left out (the
  wheel is the morph X then).
* User wavetables: tables analysed from the factory recordings (CC0, resources/samples): at evenly spaced points of the
  recording the harmonic amplitudes of the note are measured and turned into one phase-aligned single cycle (2048 samples)
  each - the same idea as the Wavetable engine's audio-to-wavetable import. The table travels embedded in the preset
  ("usertables" block, .ohpewt data).
* Projects: whole-instrument set-ups that use what only a project carries - the harmony brain's Follow per part, the
  motion recorder's lanes, the performance pads' settings, four 3D parts moving at once.
"""
import base64, json, math, os, struct, sys, wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "resources", "presets", "factory")
sys.path.insert(0, HERE)
from factory_parts_showcase import LEVELS, SRC, DST, CURVE, wt, amp, mod, lfo, life, char, expr, route  # noqa: E402
import factory_parts_showcase as sc  # noqa: E402

CTRL_PRESSURE, CTRL_MODWHEEL = 1, 3
ENGINES = ["fm", "wavetable", "analog", "pluck", "grain", "vocal", "cluster", "sampler"]
DE = {}  # German descriptions of the presets made here


def _load(name):
    for f in os.listdir(OUT):
        if f.endswith(".ohpepreset"):
            d = json.load(open(os.path.join(OUT, f), encoding="utf-8"))
            if d["name"] == name:
                return d
    raise KeyError(name)


def _strip_wheel(params):
    """Removes the matrix routings driven by the mod wheel (source or via) and packs the rest into slots 1, 2 ..."""
    wheel = SRC.index("modwheel")
    keep = []
    for k in range(1, 17):
        s = "mod.s%d." % k
        if s + "src" not in params:
            continue
        r = {x: params.pop(s + x) for x in ("src", "via", "dst", "amt", "curve") if s + x in params}
        if int(r.get("src", 0)) != wheel and int(r.get("via", 0)) != wheel:
            keep.append(r)
    for i, r in enumerate(keep):
        params.update({"mod.s%d.%s" % (i + 1, x): v for x, v in r.items()})
    return params


def _corner(name):
    d = _load(name)
    return {"name": d["name"], "params": _strip_wheel(dict(d["params"]))}


MORPHS = [
    ("Morph Living Keys", ["Keys", "Morph"], ["Velocity Glass EP", "Struck Plate Keys", "Breathing Organ", "Sympathetic Harp Keys"],
     0.15, "Four showcase keyboards on four engines: the breathing organ at home, the harp with its sympathetic strings to the "
     "right, the FM piano above, the struck plates in the far corner. Mod wheel = right, pressure = up.",
     "Vier Showcase-Tasteninstrumente auf vier Engines: die atmende Orgel zu Hause, rechts die Harfe mit ihren "
     "Resonanzsaiten, darüber das FM-Piano, in der fernen Ecke die angeschlagenen Platten. Modrad = rechts, Druck = hoch."),
    ("Morph Pressure Pads", ["Pads", "Morph"], ["Tempo Tide Pad", "Sympathy Choir", "Living Strings", "Sub Floor Pad"],
     0.35, "Living strings at home that turn into a dark pad with a sub floor (mod wheel), a tempo-synced wavetable tide "
     "(pressure) or a choir over sympathetic strings (both). Mod wheel = right, pressure = up.",
     "Lebendige Streicher zu Hause, die zu einer dunklen Fläche mit Sub-Fundament (Modrad), zu einer tempo-synchronen "
     "Wavetable-Gezeit (Druck) oder zu einem Chor über Resonanzsaiten (beides) werden. Modrad = rechts, Druck = hoch."),
    ("Morph Lead Characters", ["Leads", "Morph"], ["Hard Edge FM Lead", "Velocity Bite Lead", "Bowed Body Lead", "Living Flute Lead"],
     0.12, "One melody, four players: a bowed wooden tube at home, a breathy flute to the right, a hard FM lead above and a "
     "biting digital lead in the far corner. Mod wheel = right, pressure = up.",
     "Eine Melodie, vier Spieler: ein gestrichenes Holzrohr zu Hause, rechts eine hauchige Flöte, darüber ein harter "
     "FM-Lead, in der fernen Ecke ein beißender Digital-Lead. Modrad = rechts, Druck = hoch."),
]


def morph_presets(parts):
    out = []
    for name, tags, corners, glide, en, de in MORPHS:
        cs = [_corner(n) for n in corners]
        home = dict(cs[2]["params"])
        home.update({"morph.on": 1, "morph.x": 0.0, "morph.y": 0.0, "morph.glide": glide, "morph.xsrc": CTRL_MODWHEEL,
                     "morph.ysrc": CTRL_PRESSURE})
        # pressure moves the pad: the home sound's own aftertouch slots stay, the matrix pressure routings too
        out.append({"name": name, "tags": tags, "engine": ENGINES[int(home["engine"])], "comment": en, "params": home,
                    "morph": {"corners": cs}, "de": de, "features": ["morph"], "file_prefix": "morph"})
    return out


# ---------------------------------------------------------------------------------------------- user wavetables
WAVDIR = os.environ.get("OHPE_WAVDIR", "/workspace/work/ohpe-r3-wavall")


def _read_wav(path):
    with open(path, "rb") as f:
        data = f.read()
    pos, fmt, ch, sr, bits, pcm = 12, 1, 2, 48000, 16, b""
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt, ch, sr = struct.unpack("<HHI", body[:8])
            bits = struct.unpack("<H", body[14:16])[0]
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    import numpy as np
    if fmt == 3 or bits == 32:
        a = np.frombuffer(pcm, dtype="<f4").astype(np.float64)
    elif bits == 24:
        b = np.frombuffer(pcm, dtype=np.uint8).reshape(-1, 3)
        a = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
        a = np.where(a >= 1 << 23, a - (1 << 24), a).astype(np.float64) / float(1 << 23)
    else:
        a = np.frombuffer(pcm, dtype="<i2").astype(np.float64) / 32768.0
    return a.reshape(-1, ch).mean(axis=1), sr


def analyse_table(wav, root, frames, start, end, harmonics=160, smooth=0.35):
    """Single-cycle frames (2048 samples each) from a recording of MIDI note `root`: harmonic amplitudes measured with
    a Hann-windowed FFT at `frames` evenly spaced points between `start` and `end` seconds, rebuilt as phase-aligned sines
    (each harmonic starts as a rising sine), lightly smoothed across frames, every frame normalised."""
    import numpy as np
    x, sr = _read_wav(os.path.join(WAVDIR, wav))
    f0 = 440.0 * 2.0 ** ((root - 69) / 12.0)
    n = 16384
    win = np.hanning(n)
    amps = []
    for i in range(frames):
        t = start + (end - start) * i / max(1, frames - 1)
        c = int(t * sr)
        seg = x[max(0, c - n // 2):max(0, c - n // 2) + n]
        if len(seg) < n:
            seg = np.pad(seg, (0, n - len(seg)))
        spec = np.abs(np.fft.rfft(seg * win))
        a = []
        for k in range(1, harmonics + 1):
            f = k * f0
            if f > 0.45 * sr:
                a.append(0.0)
                continue
            b = f * n / sr
            lo, hi = int(b * 0.985) - 1, int(b * 1.015) + 2
            a.append(float(spec[max(0, lo):hi].max()) if hi > lo else 0.0)
        amps.append(np.array(a))
    amps = np.array(amps)
    sm = amps.copy()  # smoothing across neighbouring frames
    for i in range(frames):
        nb = [amps[j] for j in (i - 1, i + 1) if 0 <= j < frames]
        if nb:
            sm[i] = (1 - smooth) * amps[i] + smooth * np.mean(nb, axis=0)
    ph = 2.0 * math.pi * np.arange(2048) / 2048.0
    out = []
    for a in sm:
        a = a / (a.max() + 1e-12)
        a[a < 10 ** (-70 / 20.0)] = 0.0  # below -70 dB: dropped (no noise floor in the table)
        cyc = sum(a[k] * np.sin((k + 1) * ph) for k in range(len(a)) if a[k] > 0)
        cyc = cyc - cyc.mean()
        out.append(cyc / (np.abs(cyc).max() + 1e-12))
    return out


def encode_table(name, frames):
    """The .ohpewt format (src/core/wavetable/TableFile.h), little endian, no formant frames."""
    nb = name.encode("utf-8")
    head = b"OHPEWT" + struct.pack("<HIIII", 1, 2048, len(frames), 0, len(nb)) + nb
    body = b"".join(struct.pack("<2048f", *[float(v) for v in f]) for f in frames)
    return head + body


TABLES = [
    # name, preset, file name in the library, source recording, root, frames, start, end (s)
    ("Piano Scan", "Piano Scan Table", "ohpe-factory-piano-scan.ohpewt", "grandpiano-48-v127.wav", 48, 8, 0.03, 2.4),
    ("Recorder Breath", "Recorder Breath Table", "ohpe-factory-recorder-breath.ohpewt", None, None, 8, 0.05, 1.2),
]


def _recorder_file():
    fs = sorted(f for f in os.listdir(WAVDIR) if f.startswith("recorder-"))
    mid = fs[len(fs) // 2]
    return mid, int(mid.split("-")[1].split(".")[0])


def _usertables(i):
    tname, _, file, wav, root, frames, start, end = TABLES[i]
    cache = os.path.join(HERE, "showcase_tables", file)  # the analysed table is kept, so the presets build without the bank
    if os.path.exists(cache):
        data = open(cache, "rb").read()
    else:
        if wav is None:
            wav, root = _recorder_file()
        data = encode_table(tname, analyse_table(wav, root, frames, start, end))
        os.makedirs(os.path.dirname(cache), exist_ok=True)
        open(cache, "wb").write(data)
    n = struct.unpack("<I", data[12:16])[0]
    return {"wavetable": {"name": tname, "file": file, "frames": n, "data": base64.b64encode(data).decode("ascii")}}


def usertable_presets():
    a = sc.S("Piano Scan Table", ["Keys"], "wavetable", ["usertable", "expr"],
             "A wavetable analysed from the recorded grand piano, eight frames from the hammer to the fading tail. The mod "
             "envelope scans through it with every note, so each key starts bright and settles like the piano did; "
             "velocity opens the tone. The mod wheel holds it on the bright frames.",
             "Eine Wavetable, analysiert aus dem aufgenommenen Flügel, acht Frames vom Hammer bis zum Ausklang. Die "
             "Mod-Hüllkurve fährt bei jeder Note hindurch, jede Taste beginnt hell und setzt sich wie beim Flügel; "
             "Anschlag öffnet den Ton. Das Modrad hält sie auf den hellen Frames.",
             wt(table="classic", pos=0.0, warp=0.0, tone=0.62, unison=0.15, univoices=3, uniwidth=0.5, keytrack=0.5,
                envpos=0.8, lfopos=0.0),
             {"usertable.wt": 1}, amp(0.002, 2.5, 0.3, 0.6), mod(0.001, 2.2, 0.15, 0.6, 0.0), lfo(0.13, 0.12, "sine", "pan"),
             expr(vel=[("cutoff", 0.35, "exp")]),
             sc.fx(1, "chorus", rate=0.4, depth=0.4, mix=0.25, voices=3, drift=0.3, spread=0.85, lowcut=180.0),
             sc.fx(5, "reverb", size=0.5, damping=0.5, predelay=0.02, mix=0.16),
             wheel=("engine7", -0.5), voices=12, velsens=0.65)
    b = sc.S("Recorder Breath Table", ["Pads"], "wavetable", ["usertable", "life", "tempo"],
             "A wavetable analysed from the recorded recorder, eight frames from the breathy onset to the singing body, "
             "slowly scanned in two-bar cycles of the tempo and drifting with Life: a soft, woody pad. The mod wheel "
             "deepens the scan.",
             "Eine Wavetable, analysiert aus der aufgenommenen Blockflöte, acht Frames vom hauchigen Einsatz bis zum "
             "singenden Ton, langsam in Zwei-Takt-Zyklen des Tempos durchfahren und mit Leben driftend: eine weiche, "
             "holzige Fläche. Das Modrad vertieft das Durchfahren.",
             wt(table="classic", pos=0.5, warp=0.0, tone=0.5, unison=0.35, univoices=6, uniwidth=0.85, keytrack=0.4,
                lfopos=0.0),
             {"usertable.wt": 1, "lfo.depth": 0.0}, amp(0.8, 1.5, 0.9, 2.0), mod(0.8, 1.5, 0.7, 1.8, 0.1),
             sc.lfo_n(2, rate=0.25, sync="2/1", shape="tri", mode="free", depth=0.5), life(0.4),
             sc.fx(1, "chorus", rate=0.25, depth=0.6, mix=0.3, voices=4, drift=0.45, spread=0.85, lowcut=180.0),
             sc.fx(5, "reverb", size=0.75, damping=0.45, predelay=0.03, mix=0.24),
             expr(vel=[("attack", -0.2)], at=[("cutoff", 0.3, "s")]),
             routes=[route("lfo2", "engine1", 0.35)], wheel=("lfo2depth", 0.4), voices=10, velsens=0.25)
    sc.PRESETS_SHOWCASE.remove(a)
    sc.PRESETS_SHOWCASE.remove(b)
    a["usertables"] = _usertables(0)
    b["usertables"] = _usertables(1)
    return [a, b]


# ---------------------------------------------------------------------------------------------- projects
KICK, KICK2, SNARE, RIM, CLAP, HAT, OPENHAT, PEDAL, TOMLO, TOMHI, TOMFL, XSTICK, RIDE, BELL, CRASH, SIZZLE, HALFHAT, GHOST, \
    COWBELL, TAMB, SHAKER, CLAVES, CONGA, BONGO = range(36, 60)
KITS = {"studio": 0, "tight": 1, "room": 2, "lofi": 3, "hybrid": 4, "boom": 5}
RATE = {"1/4": 0, "1/8": 1, "1/16": 2, "1/32": 3, "1/4T": 4, "1/8T": 5, "1/16T": 6, "1/32T": 7, "1/4.": 8, "1/8.": 9, "1/16.": 10}
STEP_RATE = {"1/4": 0, "1/8": 1, "1/16": 2, "1/32": 3, "1/8T": 4, "1/16T": 5}
FOLLOW = {"off": 0, "scale": 1, "chord": 2, "root": 3}
PROJECT_LEVELS = LEVELS  # the master volumes of the projects are calibrated like the parts (showcase_levels.json)


def part(n, name, **override):
    """Every parameter of showcase / factory part preset `name` (the bank file, with its wheel and expression) for part n."""
    d = _load(name)
    p = {"p%d.%s" % (n, k): v for k, v in d["params"].items()}
    for k, v in override.items():
        p["p%d.%s" % (n, k.replace("__", "."))] = v
    return p


def lanes(lengths, rates, parts=(0, 0, 0, 0)):
    p = {}
    for i, (l, r, pt) in enumerate(zip(lengths, rates, parts)):
        p["seq.orbit.l%d.len" % (i + 1)] = l
        p["seq.orbit.l%d.rate" % (i + 1)] = RATE[r]
        p["seq.orbit.l%d.part" % (i + 1)] = pt
    return p


def lane_steps(lane, cells):
    return [{"lane": lane, "i": i, "on": True, "note": c[0], "vel": c[1]} for i, c in enumerate(cells) if c is not None]


def rings(spec, vels=()):
    p = {}
    for i, (st, pu, rot, note) in enumerate(spec):
        p.update({"seq.euclid.r%d.steps" % (i + 1): st, "seq.euclid.r%d.pulses" % (i + 1): pu,
                  "seq.euclid.r%d.rot" % (i + 1): rot, "seq.euclid.r%d.note" % (i + 1): note})
    for i in range(len(spec), 4):
        p["seq.euclid.r%d.pulses" % (i + 1)] = 0
    for i, v in enumerate(vels):
        p["seq.euclid.r%d.vel" % (i + 1)] = v
    return p


def held(notes, vel=96, last_gate=0.9):
    out, i = [], 0
    for n in notes:
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


def note_steps(notes, gate=0.5):
    out = []
    for i, n in enumerate(notes):
        if n is not None:
            out.append({"i": i, "on": True, "note": n[0], "vel": n[1], "gate": n[2] if len(n) > 2 else gate, "prob": 100,
                        "ratchet": 1})
    return out


def mixer(**kw):
    p = {}
    for k, v in kw.items():
        if k == "master":
            p["mix.master.vol"] = v
            continue
        ch = int(k[2:])
        p["mix.ch%d.vol" % ch], p["mix.ch%d.senda" % ch], p["mix.ch%d.sendb" % ch] = v[0], v[1], v[2]
        if len(v) > 3:
            p["mix.ch%d.pan" % ch] = v[3]
    return p


MASTER = {"mix.master.comp": 1, "mix.master.comp.thresh": -14.0, "mix.master.comp.ratio": 2.5, "mix.master.comp.attack": 0.012,
          "mix.master.comp.release": 0.16, "mix.master.comp.makeup": 0.0, "mix.master.limit": 1}


def motion_lane(target, points):
    return {"target": target, "points": [round(max(0.0, min(1.0, v)), 4) for v in points]}


def curve(length_16ths, fn):
    """Motion points (8 per sixteenth) of fn(phase 0..1)."""
    n = length_16ths * 8
    return [fn(i / n) for i in range(n)]


def project(name, tags, en, de, features, *dicts, sequencer=None, orbit=None, motion=None, extra_sections=None):
    params = {}
    for d in dicts:
        params.update(d)
    params["seq.run"] = 0
    params.update({k: v for k, v in MASTER.items() if k not in params})
    if name in PROJECT_LEVELS:
        params["mix.master.vol"] = PROJECT_LEVELS[name]
    sections = {}
    if sequencer is not None:
        sections["sequencer"] = {"format": "ohpe-step-pattern", "version": 1, **sequencer}
    if orbit is not None:
        sections["orbit"] = {"format": "ohpe-orbit", "version": 1, "steps": orbit}
    if motion is not None:
        sections["motion"] = {"version": 1, "parts": [{"lanes": motion.get(p, [])} for p in range(4)]}
    if extra_sections:
        sections.update(extra_sections)
    first = int(params.get("p1.engine", 2))
    DE[name] = de
    return {"name": name, "tags": tags, "engine": ENGINES[first], "comment": en, "params": params, "sections": sections,
            "de": de, "features": features}


def projects():
    out = []
    # 1. Harmony Follow: Orbit lanes 1-3 play Am - F - C - G voicings on part 1 (the leading part, heard by the brain);
    #    the bass (step sequencer) plays one fixed riff on A and follows the chord ROOT, the bells (Euclid) play fixed notes
    #    and follow the CHORD, the pad (Orbit lane 4) holds one note that follows the chord too - the parts change with the
    #    chords without a single programmed chord change. Tight kit: none (a pure harmony piece, 96 BPM).
    voic = [(57, 60, 64), (57, 60, 65), (55, 60, 64), (55, 59, 62)]  # Am, F, C, G (close voicings around middle C)
    cells = [[(v[k], 84 - 4 * k)] * 4 for v in voic]
    chord_lane = lambda k: [c for bar in cells for c in [bar[0][k]] * 4 if True] if False else \
        [(v[k], 84 - 4 * k) for v in voic for _ in range(4)]
    bass = note_steps([(45, 110, 0.7), None, (45, 84, 0.4), (57, 96, 0.3), None, (45, 90, 0.5), None, (52, 92, 0.4),
                       (45, 108, 0.7), None, (45, 80, 0.4), (57, 96, 0.3), None, (52, 90, 0.5), (55, 88, 0.4), None])
    out.append(project(
        "Harmony Follow Jam", ["Keys", "Bass", "Arp"],
        "Play chords and the band follows: Orbit plays Am-F-C-G on the glass piano (part 1, heard by the harmony brain); the bass "
        "plays one fixed riff and follows the chord root, the bells play fixed notes and snap to the chord tones, the pad "
        "holds one note that moves to the nearest chord tone. Replace the chords with your own on part 1. Press Play.",
        "Akkorde spielen, die Band folgt: Orbit spielt Am-F-C-G auf dem Glas-Piano (Part 1, vom Harmonie-Gehirn gehört); der "
        "Bass spielt ein festes Riff und folgt dem Grundton, die Glocken spielen feste Töne und rasten auf Akkordtöne ein, "
        "die Fläche hält einen Ton, der zum nächsten Akkordton wandert. Eigene Akkorde auf Part 1 spielen. Play drücken.",
        ["harmony", "expr"],
        {"tempo": 96, "key.root": 9, "key.scale": 2, "seq.step.channel": 2, "seq.step.rate": STEP_RATE["1/8"],
         "seq.step.length": 16, "harmony.memory": 4, "harmony.p1.follow": FOLLOW["off"], "harmony.p2.follow": FOLLOW["root"],
         "harmony.p3.follow": FOLLOW["chord"], "harmony.p4.follow": FOLLOW["chord"]},
        part(1, "Velocity Glass EP"), part(2, "Body Slap Bass"), part(3, "Living Bell Arp"), part(4, "Living Strings"),
        {"seq.orbit.on": 1, "seq.orbit.drums": 0, "seq.orbit.part": 1, "seq.orbit.gate": 1.0, "seq.orbit.human": 0.15},
        lanes([16, 16, 16, 16], ["1/4", "1/4", "1/4", "1/4"], (0, 0, 0, 4)),
        {"seq.euclid.on": 1, "seq.euclid.drums": 0, "seq.euclid.part": 3, "seq.euclid.rate": RATE["1/16"],
         "seq.euclid.gate": 0.4, "seq.euclid.vel": 80, "seq.euclid.human": 0.1},
        rings([(16, 5, 0, 76), (16, 3, 2, 81), (12, 2, 5, 72)], (84, 74, 70)),
        mixer(ch1=(-2, 0.1, 0.2), ch2=(-1, 0.0, 0.03), ch3=(-6, 0.3, 0.25), ch4=(-10, 0.0, 0.3), master=0),
        {"fxa.delay.sync": 5, "fxa.delay.feedback": 0.35, "fxa.delay.tone": 0.45, "fxb.reverb.size": 0.6,
         "fxb.reverb.damping": 0.5},
        sequencer={"steps": bass, "lengths": {"1": 16}},
        orbit=lane_steps(0, chord_lane(0)) + lane_steps(1, chord_lane(1)) + lane_steps(2, chord_lane(2))
        + lane_steps(3, [(64, 70)] * 16)))

    # 2. Motion Lanes: the bass's filter (5 sixteenths) and the pad's Bright macro (12 sixteenths) move in recorded lanes of
    #    their own length against the 4/4 bar - polymetric movement that meets again after a few bars; Euclid -> Tight kit.
    bass_cut = curve(5, lambda ph: 0.32 + 0.22 * math.sin(2 * math.pi * ph) ** 2)
    bass_res = curve(5, lambda ph: 0.25 + 0.2 * ph)
    pad_bright = curve(12, lambda ph: 0.5 + 0.35 * math.sin(2 * math.pi * ph))
    pluck_decay = curve(7, lambda ph: 0.35 + 0.3 * (1 - ph))
    mbass = note_steps([(36, 112, 0.5), None, (36, 84, 0.3), (48, 96, 0.3), None, (36, 92, 0.4), (39, 98, 0.4), None,
                        (36, 110, 0.5), None, (43, 90, 0.3), None, (41, 96, 0.4), None, (39, 92, 0.4), (34, 96, 0.4)])
    out.append(project(
        "Motion Lanes Groove", ["Bass", "Drums", "Pads"],
        "A groove whose sounds move by themselves: the motion recorder plays a filter and resonance lane on the bass that "
        "loops every five sixteenths, a twelve-sixteenth Bright lane on the pad and a seven-sixteenth decay lane on the "
        "plates - against the four-four beat on the Tight kit they drift apart and meet again. Record your own: LFO mode, "
        "Motion. Press Play.",
        "Ein Groove, dessen Klänge sich von selbst bewegen: der Bewegungsrekorder spielt auf dem Bass eine Filter- und "
        "Resonanz-Spur, die alle fünf Sechzehntel wiederholt, auf der Fläche eine Hell-Spur über zwölf Sechzehntel und auf "
        "den Platten eine Ausklang-Spur über sieben – gegen den Vierviertel-Beat auf dem Tight-Kit laufen sie auseinander "
        "und treffen sich wieder. Eigene aufnehmen: LFO-Modus, Bewegung. Play drücken.",
        ["motion", "tempo", "char"],
        {"tempo": 118, "drum.kit": KITS["tight"], "key.root": 0, "key.scale": 2, "seq.step.channel": 2,
         "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 16,
         "p2.motion.on": 1, "p2.motion.len": 5, "p2.motion.amt": 1.0,
         "p1.motion.on": 1, "p1.motion.len": 12, "p1.motion.amt": 1.0,
         "p3.motion.on": 1, "p3.motion.len": 7, "p3.motion.amt": 1.0},
        part(1, "Bright Wide Pad"), part(2, "Sub Pressure Bass"), part(3, "Glass Plate Pluck"), part(4, "Tempo Tide Pad"),
        {"seq.orbit.on": 1, "seq.orbit.drums": 0, "seq.orbit.part": 1, "seq.orbit.gate": 1.0},
        lanes([16, 16, 16, 8], ["1/4", "1/4", "1/4", "1/8"], (0, 0, 0, 3)),
        {"seq.euclid.on": 1, "seq.euclid.drums": 1, "seq.euclid.rate": RATE["1/16"], "seq.euclid.gate": 0.5,
         "seq.euclid.vel": 100, "seq.euclid.human": 0.1},
        rings([(16, 4, 0, KICK), (16, 2, 4, CLAP), (16, 8, 1, HAT), (16, 3, 2, SHAKER)], (116, 100, 66, 54)),
        mixer(ch1=(-7, 0.0, 0.25), ch2=(-1, 0.0, 0.0), ch3=(-6, 0.3, 0.2), ch4=(-60, 0.0, 0.0), ch5=(-1, 0.0, 0.06), master=0),
        {"fxa.delay.sync": 5, "fxa.delay.feedback": 0.3, "fxb.reverb.size": 0.6, "fxb.reverb.damping": 0.5,
         "mix.duck.src": 5, "mix.duck.release": 0.14, "mix.ch1.duck": 0.3, "mix.ch2.duck": 0.45, "mix.ch3.duck": 0.15,
         "mix.ch5.comp": 0.45},
        sequencer={"steps": mbass, "lengths": {"1": 16}},
        orbit=lane_steps(0, [(n, 78) for n in (60, 60, 60, 60, 56, 56, 56, 56, 63, 63, 63, 63, 58, 58, 58, 58)])
        + lane_steps(1, [(n, 74) for n in (63, 63, 63, 63, 60, 60, 60, 60, 67, 67, 67, 67, 62, 62, 62, 62)])
        + lane_steps(2, [(n, 72) for n in (67, 67, 67, 67, 63, 63, 63, 63, 70, 70, 70, 70, 65, 65, 65, 65)])
        + lane_steps(3, [(72, 92), None, (75, 80), (79, 86), None, (77, 78), (75, 84), None]),
        motion={1: [motion_lane("analog.cutoff", bass_cut), motion_lane("analog.reso", bass_res)],
                0: [motion_lane("char.bright", pad_bright)], 2: [motion_lane("reso.decay", pluck_decay)]}))

    # 3. Performance Pads: a house loop set up for the four pads (stutter 1/16, tape stop over one bar, low-pass sweep
    #    over two bars, reverb freeze), the pad part opens its own filter against the master sweep (Pad Sweep source) and the
    #    plucks throw their echo while any pad is held (Pads Held source).
    house_bass = note_steps([None, None, (36, 100, 0.4), None, None, None, (36, 92, 0.4), (48, 84, 0.3)] * 2)
    out.append(project(
        "Pads Performance Set", ["Drums", "Bass", "FX"],
        "A house loop set up for the performance pads (Effects mode, Pads): stutter on sixteenths, a one-bar tape stop, a "
        "low-pass sweep over two bars and a reverb freeze, all on the beat. While the sweep closes, the pad opens its own "
        "filter against it; while any pad is held the plucks throw their echo. Press Play, then hold the pads.",
        "Ein House-Loop, eingerichtet für die Performance-Pads (Effekt-Modus, Pads): Stottern in Sechzehnteln, ein Bandstopp "
        "über einen Takt, ein Tiefpass-Sweep über zwei Takte und ein Hall-Freeze, alles im Takt. Während der Sweep "
        "schließt, öffnet die Fläche ihr eigenes Filter dagegen; solange ein Pad gehalten wird, werfen die Zupfer ihr Echo. "
        "Play drücken, dann die Pads halten.",
        ["pads", "tempo"],
        {"tempo": 124, "drum.kit": KITS["tight"], "key.root": 2, "key.scale": 3, "seq.step.channel": 2,
         "seq.step.rate": STEP_RATE["1/16"], "seq.step.length": 16,
         "pads.quant": 1, "pads.stutter.div": 2, "pads.stop.len": 3, "pads.sweep.mode": 0, "pads.sweep.len": 5,
         "pads.sweep.res": 0.5, "pads.freeze.mix": 0.6},
        part(1, "Tempo Tide Pad", **{"mod.s5.src": SRC.index("padsweep"), "mod.s5.via": 0, "mod.s5.dst": DST.index("timbre"),
                                     "mod.s5.amt": 0.5, "mod.s5.curve": 0}),
        part(2, "Round Square Sub"),
        part(3, "Ping-Pong Space Pluck", **{"mod.s3.src": SRC.index("padheld"), "mod.s3.via": 0,
                                            "mod.s3.dst": DST.index("fx1mix"), "mod.s3.amt": 0.0, "mod.s3.curve": 0}),
        part(4, "Resonant Impact"),
        {"seq.orbit.on": 1, "seq.orbit.drums": 1, "seq.orbit.gate": 0.5, "seq.orbit.human": 0.1},
        lanes([16, 16, 16, 16], ["1/16"] * 4),
        {"seq.euclid.on": 1, "seq.euclid.drums": 0, "seq.euclid.part": 3, "seq.euclid.rate": RATE["1/16"],
         "seq.euclid.gate": 0.35, "seq.euclid.vel": 90},
        rings([(16, 3, 3, 62), (16, 3, 3, 69), (16, 2, 6, 74)], (90, 84, 78)),
        mixer(ch1=(-9, 0.0, 0.3), ch2=(0, 0.0, 0.0), ch3=(-6, 0.3, 0.2), ch4=(-60, 0.0, 0.0), ch5=(-1, 0.0, 0.05), master=0),
        {"fxa.delay.sync": 5, "fxa.delay.feedback": 0.35, "fxb.reverb.size": 0.6, "fxb.reverb.damping": 0.5,
         "mix.duck.src": 5, "mix.duck.release": 0.14, "mix.ch1.duck": 0.3, "mix.ch2.duck": 0.45, "mix.ch3.duck": 0.15,
         "mix.ch5.comp": 0.45},
        sequencer={"steps": house_bass, "lengths": {"1": 16}},
        orbit=lane_steps(0, [(KICK, 122) if i % 4 == 0 else None for i in range(16)])
        + lane_steps(1, [(CLAP, 110) if i in (4, 12) else None for i in range(16)])
        + lane_steps(2, [(OPENHAT, 96) if i % 4 == 2 else (HAT, 50) if i % 2 else None for i in range(16)])
        + lane_steps(3, [(SHAKER, 60 if i % 2 else 40) for i in range(16)])))

    # 4. Binaural Scene: four 3D parts moving at once - a spiralling pad, bells circling the head, plucks bouncing front /
    #    back and a grain swarm - every path measured in bars, so the scene breathes with the tempo. No drums.
    out.append(project(
        "Binaural Scene", ["Pads", "3D", "Textures"],
        "Four sounds moving around your head at once: a pad spiralling upwards over sixteen bars, bells circling once per "
        "bar, plucks bouncing front to back on the half bar and a glassy grain swarm. Every path is measured in bars, so "
        "the whole scene follows the tempo. Headphones on, press Play.",
        "Vier Klänge bewegen sich gleichzeitig um den Kopf: eine Fläche steigt in sechzehn Takten spiralförmig auf, Glocken "
        "kreisen einmal pro Takt, Zupfer springen jeden halben Takt von vorn nach hinten, dazu ein gläserner Grain-Schwarm. "
        "Jeder Pfad ist in Takten bemessen, die ganze Szene folgt dem Tempo. Kopfhörer auf, Play drücken.",
        ["space", "tempo"],
        {"tempo": 90, "key.root": 4, "key.scale": 2, "seq.step.channel": 1, "seq.step.rate": STEP_RATE["1/4"],
         "seq.step.length": 16},
        part(1, "Halo Spiral Pad"), part(2, "Orbit Space Arp"), part(3, "Ping-Pong Space Pluck"), part(4, "Swarm Field 3D"),
        {"seq.orbit.on": 1, "seq.orbit.drums": 0, "seq.orbit.part": 2, "seq.orbit.gate": 0.6, "seq.orbit.human": 0.2},
        lanes([5, 7, 11, 16], ["1/8", "1/8", "1/8", "1/4"], (0, 0, 0, 4)),
        {"seq.euclid.on": 1, "seq.euclid.drums": 0, "seq.euclid.part": 3, "seq.euclid.rate": RATE["1/8"],
         "seq.euclid.gate": 0.5, "seq.euclid.vel": 80},
        rings([(13, 3, 0, 71), (19, 4, 5, 76)], (82, 74)),
        mixer(ch1=(-3, 0.0, 0.2), ch2=(-6, 0.2, 0.2), ch3=(-8, 0.2, 0.2), ch4=(-12, 0.0, 0.2), master=0),
        {"fxa.delay.sync": 5, "fxa.delay.feedback": 0.35, "fxa.delay.tone": 0.4, "fxb.reverb.size": 0.8,
         "fxb.reverb.damping": 0.45},
        sequencer={"steps": held([(52, 16, 82), (48, 16, 80), (55, 16, 80), (50, 16, 82)], last_gate=0.95),
                   "lengths": {"1": 64}},
        orbit=lane_steps(0, [(76, 70), None, (79, 62), None, (83, 66)])
        + lane_steps(1, [(71, 60), None, None, (74, 64), None, (78, 58), None])
        + lane_steps(2, [(88, 52), None, None, None, (86, 50), None, None, (91, 48), None, None, None])
        + lane_steps(3, [(64, 60)] * 16)))
    return out
