# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Factory part presets (0.4, package F). Written to resources/presets/factory by make_factory.py.

Each preset: name, category tags (first = main category), engine, comment, local parameters.
Parameters not listed return to their defaults when a preset loads. The level ("vol") is
calibrated by the render loop (levels.json) unless a preset sets it explicitly.
"""

ENGINES = {"fm": 0, "wavetable": 1, "analog": 2, "pluck": 3, "grain": 4, "vocal": 5, "cluster": 6, "sampler": 7}
LFO_SHAPE = {"sine": 0, "tri": 1, "saw": 2, "square": 3, "sh": 4}
LFO_DEST = {"pitch": 0, "timbre": 1, "amp": 2, "pan": 3}
FX = {"off": 0, "chorus": 1, "phaser": 2, "bitcrush": 3, "filterdrive": 4, "delay": 5, "reverb": 6}
INST = {"builtin": 0, "kalimba": 1, "marimba": 2, "vibraphone": 3, "glockenspiel": 4, "harp": 5, "harpsichord": 6,
        "recorder": 7, "glass": 8, "zither": 9, "bowedmetal": 10}


def amp(a, d, s, r):
    return {"amp.a": a, "amp.d": d, "amp.s": s, "amp.r": r}


def mod(a, d, s, r, amt):
    return {"mod.a": a, "mod.d": d, "mod.s": s, "mod.r": r, "mod.amt": amt}


def lfo(rate, depth, shape="sine", dest="pitch"):
    return {"lfo.rate": rate, "lfo.depth": depth, "lfo.shape": LFO_SHAPE[shape], "lfo.dest": LFO_DEST[dest]}


def chorus(rate=0.6, depth=0.5, mix=0.4):
    return {"fx.type": FX["chorus"], "fx.chorus.rate": rate, "fx.chorus.depth": depth, "fx.chorus.mix": mix}


def phaser(rate=0.3, depth=0.6, feedback=0.4, mix=0.4):
    return {"fx.type": FX["phaser"], "fx.phaser.rate": rate, "fx.phaser.depth": depth, "fx.phaser.feedback": feedback, "fx.phaser.mix": mix}


def delay(time=0.45, feedback=0.3, tone=0.5, pingpong=1, mix=0.22):
    return {"fx.type": FX["delay"], "fx.delay.time": time, "fx.delay.feedback": feedback, "fx.delay.tone": tone,
            "fx.delay.pingpong": pingpong, "fx.delay.mix": mix}


def reverb(size=0.6, damping=0.45, predelay=0.02, mix=0.25):
    return {"fx.type": FX["reverb"], "fx.reverb.size": size, "fx.reverb.damping": damping, "fx.reverb.predelay": predelay, "fx.reverb.mix": mix}


def crush(bits=10, down=2, mix=0.6):
    return {"fx.type": FX["bitcrush"], "fx.bitcrush.bits": bits, "fx.bitcrush.downsample": down, "fx.bitcrush.mix": mix}


def drive(cutoff=6000, reso=0.2, drv=0.3, mode=0, mix=1.0):
    return {"fx.type": FX["filterdrive"], "fx.filterdrive.cutoff": cutoff, "fx.filterdrive.reso": reso,
            "fx.filterdrive.drive": drv, "fx.filterdrive.mode": mode, "fx.filterdrive.mix": mix}


def P(name, tags, engine, comment, *dicts, **kw):
    params = {"engine": ENGINES[engine]}
    for d in dicts:
        params.update(d)
    for k, v in kw.items():
        params[k.replace("__", ".")] = v
    # oscillator engines start their waves at full level: a 3 ms minimum attack keeps note starts
    # free of clicks (drums keep their instant attack on purpose)
    if engine in ("analog", "wavetable", "cluster", "fm") and tags[0] != "Drums":
        params["amp.a"] = max(params.get("amp.a", 0.005), 0.003)
        if params.get("mod.amt", 0.3) != 0.0:
            params["mod.a"] = max(params.get("mod.a", 0.005), 0.002)
    if engine == "pluck" and tags[0] != "Drums":  # soften the excitation click a little
        params["amp.a"] = max(params.get("amp.a", 0.005), 0.002)
    return {"name": name, "tags": tags, "engine": engine, "comment": comment, "params": params}


def an(**kw):  # analog engine parameters
    return {"analog." + k: v for k, v in kw.items()}


def fm(**kw):
    return {"fm." + k: v for k, v in kw.items()}


def wt(**kw):
    out = {"wavetable." + k: v for k, v in kw.items()}
    if "wavetable.table" in out and isinstance(out["wavetable.table"], str):
        out["wavetable.table"] = ["classic", "harmonic", "formant", "digital", "glass", "grit"].index(out["wavetable.table"])
    return out


def pl(**kw):
    return {"pluck." + k: v for k, v in kw.items()}


def cl(**kw):
    out = {"cluster." + k: v for k, v in kw.items()}
    chords = ["unison", "octave", "fifth", "major", "minor", "sus2", "sus4", "maj7", "min7", "dom7", "add9", "fourths"]
    if isinstance(out.get("cluster.chord"), str):
        out["cluster.chord"] = chords.index(out["cluster.chord"])
    return out


def gr(**kw):
    out = {"grain." + k: v for k, v in kw.items()}
    if isinstance(out.get("grain.inst"), str):
        out["grain.inst"] = INST[out["grain.inst"]]
    return out


def vo(**kw):
    return {"vocal." + k: v for k, v in kw.items()}


def sm(**kw):
    out = {"sampler." + k: v for k, v in kw.items()}
    if isinstance(out.get("sampler.inst"), str):
        out["sampler.inst"] = INST[out["sampler.inst"]]
    return out


def pitchenv(amount, time, curve=0.5):
    """0.6 pitch envelope: notes start `amount` semitones away (negative = below: they rise) and arrive after `time`."""
    return {"pitchenv.amt": amount, "pitchenv.time": time, "pitchenv.curve": curve}


GLIDE_MODE = {"auto": 0, "legato": 1, "always": 2}


def porta(time, mode="auto", rate="time"):
    """0.6 portamento: glide time, mode (auto / legato / always), constant time or constant rate (time per octave)."""
    return {"glide": time, "glide.mode": GLIDE_MODE[mode], "glide.rate": 0 if rate == "time" else 1}


MONO = {"voices": 1}

PRESETS = [
    # ================================================================ BASS
    P("Round Sub", ["Bass"], "analog", "Warm square and sub oscillator with a soft filter blip; sits under anything.",
      an(shape=0.5, detune=0.04, cutoff=360, reso=0.12, osc2oct=-1, oscmix=0.35, sub=0.6, drive=0.3, keytrack=0.4),
      amp(0.002, 0.5, 0.85, 0.08), mod(0.001, 0.22, 0.2, 0.1, 0.3), MONO, glide=0.03, velsens=0.4),
    P("Acid Line", ["Bass", "Leads"], "analog", "Squelchy resonant saw with a sub for weight, accent-friendly velocity and a short glide.",
      an(shape=0.0, detune=0.0, cutoff=380, reso=0.7, oscmix=0.0, sub=0.3, drive=0.6, keytrack=0.5),
      amp(0.002, 0.35, 0.75, 0.06), mod(0.001, 0.3, 0.05, 0.1, 0.62), MONO, glide=0.06, velsens=0.45),
    P("Fat Saw Bass", ["Bass"], "analog", "Two detuned saws and a sub through the ladder filter; big but not boomy.",
      an(shape=0.05, detune=0.2, cutoff=380, reso=0.22, osc2oct=0, oscmix=0.5, sub=0.2, drive=0.4, keytrack=0.4),
      amp(0.003, 0.4, 0.8, 0.09), mod(0.001, 0.3, 0.25, 0.15, 0.38), MONO, velsens=0.45),
    P("Rubber Bass", ["Bass", "Plucks"], "analog", "Bouncy plucked bass: fast filter decay, round sub.",
      an(shape=0.3, detune=0.05, cutoff=190, reso=0.35, osc2oct=-1, oscmix=0.3, sub=0.5, drive=0.3, keytrack=0.4),
      amp(0.002, 0.35, 0.3, 0.06), mod(0.001, 0.12, 0.0, 0.1, 0.5), MONO, velsens=0.6),
    P("Reese Drift", ["Bass"], "analog", "Wide detuned saws slowly beating against each other; dark and moving.",
      an(shape=0.0, detune=0.5, cutoff=650, reso=0.1, oscmix=0.5, sub=0.3, drive=0.35, keytrack=0.3),
      amp(0.005, 0.5, 0.9, 0.12), mod(0.01, 0.5, 0.5, 0.2, 0.1), lfo(0.18, 0.18, "tri", "timbre"), MONO, glide=0.04),
    P("House Organ Bass", ["Bass"], "analog", "Hollow square with an octave-down second oscillator; short and punchy.",
      an(shape=0.48, detune=0.03, cutoff=330, reso=0.28, osc2oct=-1, oscmix=0.3, sub=0.25, drive=0.2, keytrack=0.5),
      amp(0.002, 0.42, 0.55, 0.08), mod(0.001, 0.18, 0.1, 0.1, 0.35), MONO),
    P("Wobble Bass", ["Bass", "FX"], "analog", "Tempo-ish filter wobble on a driven saw stack; turn the LFO rate to taste.",
      an(shape=0.1, detune=0.15, cutoff=300, reso=0.45, oscmix=0.5, sub=0.45, drive=0.5, keytrack=0.3),
      amp(0.003, 0.4, 0.95, 0.1), mod(0.001, 0.3, 0.5, 0.1, 0.1), lfo(3.33, 0.55, "sine", "timbre"), MONO),
    P("Slap FM Bass", ["Bass"], "fm", "Percussive FM bass: the index snaps shut like a slapped string.",
      fm(algo=2, modratio=1.0, index=0.42, feedback=0.05, op1ratio=1, op2ratio=1, op3ratio=0.5, op4ratio=1, op1level=1.0,
         op2level=0.8, op3level=0.55, op4level=0.0, op5level=0.0, op6level=0.0),
      amp(0.001, 0.6, 0.45, 0.07), mod(0.004, 0.14, 0.15, 0.1, 1.0), MONO, velsens=0.6),
    P("Punch FM Bass", ["Bass"], "fm", "Solid digital bass with a bright attack and a clean sine body.",
      fm(algo=2, modratio=1.0, index=0.38, op1ratio=1, op2ratio=2, op3ratio=1, op4ratio=1, op1level=1.0, op2level=0.6,
         op3level=0.6, op4level=0.0, op5level=0.0, op6level=0.0),
      amp(0.001, 0.4, 0.7, 0.06), mod(0.001, 0.25, 0.2, 0.1, 0.9), MONO, velsens=0.5),
    P("Growl FM Bass", ["Bass"], "fm", "Feedback-edged FM bass with a slow timbre LFO for movement.",
      fm(algo=0, modratio=1.0, index=0.22, feedback=0.35, op1ratio=1, op2ratio=1, op3ratio=0.5, op4ratio=1, op5ratio=1,
         op6ratio=1, op1level=1.0, op2level=0.7, op3level=0.4, op4level=0.4, op5level=0.4, op6level=0.5),
      amp(0.003, 0.5, 0.85, 0.08), mod(0.005, 0.4, 0.6, 0.1, 0.5), lfo(0.4, 0.3, "tri", "timbre"), MONO),
    P("Table Sub", ["Bass"], "wavetable", "Near-sine wavetable bass with a sub; pure low end with a touch of edge.",
      wt(table="classic", pos=0.12, warp=0.1, tone=0.45, unison=0.0, sub=0.6, reso=0.05, keytrack=0.4, envpos=0.0),
      amp(0.002, 0.4, 0.9, 0.08), mod(0.001, 0.2, 0.3, 0.1, 0.25), MONO, glide=0.02),
    P("Grit Bass", ["Bass"], "wavetable", "Growly Grit table with an envelope sweep; aggressive but controlled.",
      wt(table="grit", pos=0.3, warp=0.25, tone=0.42, unison=0.1, sub=0.45, reso=0.2, keytrack=0.3, envpos=0.25),
      amp(0.002, 0.35, 0.8, 0.08), mod(0.001, 0.25, 0.2, 0.1, 0.35), MONO),
    P("Talking Bass", ["Bass"], "wavetable", "Formant table swept by the envelope: a vowel 'wow' on every note.",
      wt(table="formant", pos=0.1, warp=0.0, tone=0.55, unison=0.0, sub=0.5, reso=0.1, keytrack=0.4, envpos=0.45),
      amp(0.002, 0.4, 0.75, 0.08), mod(0.001, 0.35, 0.1, 0.15, 0.25), MONO, glide=0.03),
    P("Unison Bass", ["Bass"], "cluster", "Octave-stacked saws with gentle detune; thick and wide for drops.",
      cl(detune=0.12, spread=0.4, tone=0.32, chord="octave", stack=3, shape=0.1, side=0.5, reso=0.15, keytrack=0.4, drift=0.1),
      amp(0.003, 0.5, 0.85, 0.1), mod(0.001, 0.25, 0.3, 0.1, 0.35), MONO),
    P("Upright Pluck", ["Bass", "Plucks"], "pluck", "Plucked string with a big wooden body: an acoustic-style bass.",
      pl(material=0.0, damping=0.3, bright=0.25, body=0.7, excite=0.3, pickpos=0.28, bodysize=0.9),
      amp(0.002, 1.5, 0.0, 0.1), mod(0.001, 0.3, 0.0, 0.2, 0.15), drive(6000, 0.0, 0.08, 0, 0.8), MONO, velsens=0.6),
    P("Log Drum Bass", ["Bass"], "sampler", "Low marimba bars played as a bass: woody, round and short.",
      sm(inst="marimba", tone=0.62), amp(0.001, 0.9, 0.0, 0.2), mod(0.001, 0.3, 0.0, 0.2, 0.2), drive(5000, 0.0, 0.06, 0, 0.7), velsens=0.6, voices=4),
    # 0.5: deep sub basses
    P("Long Sub", ["Bass", "Boom"], "analog", "Deep, long-decaying sub that slides between overlapping notes (glide); a soft click on the attack.",
      an(shape=0.5, detune=0.0, cutoff=430, reso=0.05, oscmix=0.0, sub=0.15, drive=0.35, keytrack=1.0),
      amp(0.002, 2.6, 0.0, 0.35), mod(0.001, 0.06, 0.0, 0.1, 0.3), MONO, glide=0.11, velsens=0.35),
    P("Dirty Sub", ["Bass", "Boom"], "analog", "Sub driven into the ladder filter: growling harmonics that stay audible on small speakers.",
      an(shape=0.3, detune=0.04, cutoff=620, reso=0.2, oscmix=0.0, sub=0.4, drive=0.9, keytrack=1.0),
      amp(0.002, 1.8, 0.4, 0.2), mod(0.001, 0.3, 0.15, 0.15, 0.3), MONO, glide=0.07, velsens=0.4),
    P("Punch Sub", ["Bass", "Boom"], "analog", "Short, punchy sub with a snappy filter click; locks in with the kick.",
      an(shape=0.5, detune=0.0, cutoff=380, reso=0.1, oscmix=0.0, sub=0.35, drive=0.5, keytrack=1.0),
      amp(0.001, 0.32, 0.0, 0.07), mod(0.001, 0.045, 0.0, 0.05, 0.75), MONO, velsens=0.5),
    P("Sine Slide", ["Bass", "Boom"], "fm", "Nearly pure sine sub with a long glide for sliding bass lines.",
      fm(algo=2, modratio=1.0, index=0.05, op1ratio=1, op2ratio=1, op1level=1.0, op2level=0.5, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      amp(0.004, 0.6, 0.92, 0.18), mod(0.001, 0.12, 0.2, 0.1, 0.6), MONO, glide=0.16, velsens=0.3),
    P("Lofi Bass", ["Bass"], "analog", "Round, warm bass: a soft square body over a strong sine sub, gently driven.",
      an(shape=0.35, detune=0.03, cutoff=420, reso=0.08, osc2oct=0, oscmix=0.0, sub=0.25, drive=0.45, keytrack=0.5),
      amp(0.004, 0.7, 0.65, 0.12), mod(0.001, 0.16, 0.15, 0.1, 0.22), MONO, glide=0.03, velsens=0.4),
    P("Rolling Bass", ["Bass"], "analog", "Tight, driven techno bass for rolling sixteenths: short filter snap, solid sub.",
      an(shape=0.2, detune=0.06, cutoff=330, reso=0.12, oscmix=0.3, sub=0.2, drive=0.55, keytrack=0.5),
      amp(0.001, 0.18, 0.15, 0.05), mod(0.001, 0.05, 0.0, 0.05, 0.28), MONO, velsens=0.5),
    P("Bounce Bass", ["Bass"], "analog", "Bright, punchy saw bass for octave-bouncing eighths.",
      an(shape=0.1, detune=0.12, cutoff=520, reso=0.2, oscmix=0.5, sub=0.1, drive=0.35, keytrack=0.5),
      amp(0.002, 0.25, 0.5, 0.06), mod(0.001, 0.12, 0.1, 0.08, 0.42), MONO, velsens=0.5),

    # ================================================================ KEYS
    P("Tine Piano", ["Keys"], "fm", "Electric piano: soft body plus a bell-like tine that fades with the key. Chorus in the FX slot.",
      fm(algo=2, modratio=1.0, index=0.3, op1ratio=1, op2ratio=1, op3ratio=1, op4ratio=14, op5ratio=1, op6ratio=1,
         op1level=1.0, op2level=0.55, op3level=0.7, op4level=0.14, op5level=0.0, op6level=0.0),
      amp(0.001, 2.6, 0.12, 0.45), mod(0.001, 1.1, 0.2, 0.4, 0.8), chorus(0.5, 0.4, 0.35), voices=14, velsens=0.8),
    P("Digital EP", ["Keys"], "fm", "Brighter electric piano with more bark when played hard.",
      fm(algo=2, modratio=1.0, index=0.42, op1ratio=1, op2ratio=1, op3ratio=1, op4ratio=7, op1level=1.0, op2level=0.6,
         op3level=0.7, op4level=0.22, op5level=0.0, op6level=0.0),
      amp(0.001, 2.0, 0.25, 0.4), mod(0.001, 0.8, 0.25, 0.4, 0.85), chorus(0.7, 0.3, 0.3), voices=14, velsens=0.7),
    P("Dusty EP", ["Keys"], "fm", "Warm electric piano with tape wow and flutter (slow pitch LFO) and chorus; made for lo-fi chords.",
      fm(algo=2, modratio=1.0, index=0.24, op1ratio=1, op2ratio=1, op3ratio=1, op4ratio=14, op5ratio=1, op6ratio=1,
         op1level=1.0, op2level=0.5, op3level=0.65, op4level=0.08, op5level=0.0, op6level=0.0),
      amp(0.002, 3.0, 0.2, 0.5), mod(0.001, 1.3, 0.25, 0.4, 0.7), lfo(0.7, 0.035, "tri", "pitch"), chorus(0.6, 0.55, 0.45), voices=14, velsens=0.7),
    P("Dub Stab", ["Keys", "Pads"], "analog", "Dark detuned chord stab whose filter drifts slowly open and shut; give it delay and reverb.",
      an(shape=0.05, detune=0.22, cutoff=700, reso=0.3, oscmix=0.5, sub=0.0, drive=0.35, keytrack=0.5),
      amp(0.003, 0.5, 0.25, 0.45), mod(0.001, 0.25, 0.0, 0.2, 0.32), lfo(0.06, 0.4, "tri", "timbre"), voices=12, velsens=0.5),
    P("Pop Brass", ["Keys", "Pads"], "analog", "Lush, detuned poly brass with a quick swell and chorus: classic synth-pop chords.",
      an(shape=0.0, detune=0.3, cutoff=1300, reso=0.12, osc2oct=0, oscmix=0.5, sub=0.05, drive=0.25, keytrack=0.6),
      amp(0.025, 0.6, 0.85, 0.35), mod(0.04, 0.5, 0.55, 0.3, 0.36), lfo(5.2, 0.015), chorus(0.5, 0.6, 0.45), voices=12, velsens=0.4),
    P("Funk Clav", ["Keys", "Plucks"], "fm", "Snappy clavinet-style keys; play short and syncopated.",
      fm(algo=2, modratio=1.0, index=0.55, op1ratio=1, op2ratio=3, op3ratio=2, op4ratio=1, op1level=1.0, op2level=0.7,
         op3level=0.35, op4level=0.3, op5level=0.0, op6level=0.0),
      amp(0.001, 0.5, 0.25, 0.07), mod(0.001, 0.18, 0.2, 0.1, 0.9), voices=12, velsens=0.7),
    P("Drawbar Organ", ["Keys"], "fm", "Additive organ: six sine drawbars, gentle vibrato and a rotary-style chorus.",
      fm(algo=7, modratio=1.0, index=0.0, op1ratio=0.5, op2ratio=1, op3ratio=1.5, op4ratio=2, op5ratio=3, op6ratio=4,
         op1level=0.8, op2level=1.0, op3level=0.45, op4level=0.6, op5level=0.25, op6level=0.35),
      amp(0.004, 0.1, 1.0, 0.07), mod(0.001, 0.1, 1.0, 0.1, 0.0), lfo(6.2, 0.025), chorus(0.9, 0.55, 0.45), voices=16, velsens=0.1),
    P("FM Vibes", ["Keys", "Plucks"], "fm", "Mellow metallic bars with tremolo.",
      fm(algo=2, modratio=1.0, index=0.18, op1ratio=1, op2ratio=4, op3ratio=1, op4ratio=10, op1level=1.0, op2level=0.6,
         op3level=0.4, op4level=0.15, op5level=0.0, op6level=0.0),
      amp(0.001, 2.6, 0.0, 1.0), mod(0.001, 0.35, 0.0, 0.3, 1.0), lfo(5.0, 0.18, "sine", "amp"), voices=14, velsens=0.75),
    P("Harpsichord", ["Keys", "Plucks"], "sampler", "Recorded harpsichord (seven notes, CC0); baroque lines and bright comping.",
      sm(inst="harpsichord", tone=1.0), amp(0.001, 2.5, 0.0, 0.25), mod(0.001, 0.3, 0.0, 0.2, 0.0), drive(15000, 0.0, 0.05, 0, 0.7), voices=14, velsens=0.4),
    P("Vibraphone", ["Keys"], "sampler", "Recorded vibraphone with a gentle motor tremolo.",
      sm(inst="vibraphone", tone=1.0), amp(0.001, 3.0, 0.0, 0.8), mod(0.001, 0.3, 0.0, 0.2, 0.0), lfo(4.8, 0.2, "sine", "amp"), drive(15000, 0.0, 0.05, 0, 0.7),
      voices=14, velsens=0.6),
    P("Marimba", ["Keys", "Plucks"], "sampler", "Recorded marimba with two velocity layers.",
      sm(inst="marimba", tone=1.0), amp(0.001, 1.6, 0.0, 0.3), mod(0.001, 0.3, 0.0, 0.2, 0.0), drive(15000, 0.0, 0.05, 0, 0.7), voices=14, velsens=0.5),
    P("Music Box", ["Keys", "Plucks"], "sampler", "Glockenspiel softened into a music box; lullabies and toy melodies.",
      sm(inst="glockenspiel", tone=0.7), amp(0.001, 1.8, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0), reverb(0.5, 0.5, 0.02, 0.22),
      voices=12, velsens=0.5, tune=0),
    P("Glass Keys", ["Keys"], "wavetable", "Bell-like Glass table that darkens as the note decays.",
      wt(table="glass", pos=0.3, warp=0.1, tone=0.72, unison=0.15, sub=0.0, reso=0.05, keytrack=0.6, envpos=-0.25),
      amp(0.001, 1.8, 0.2, 0.6), mod(0.001, 0.9, 0.2, 0.5, 0.35), reverb(0.55, 0.5, 0.02, 0.2), voices=14),
    P("Harmonic Organ", ["Keys"], "wavetable", "Additive Harmonic table: a soft, slightly breathy organ.",
      wt(table="harmonic", pos=0.55, warp=0.0, tone=0.62, unison=0.2, sub=0.25, reso=0.0, keytrack=0.5, envpos=0.0),
      amp(0.01, 0.2, 1.0, 0.12), mod(0.001, 0.2, 1.0, 0.1, 0.0), lfo(5.8, 0.02), voices=16, velsens=0.2),
    P("Soft Brass", ["Keys", "Pads"], "analog", "Polyphonic brass: a slow filter swell on detuned saws.",
      an(shape=0.0, detune=0.25, cutoff=520, reso=0.12, oscmix=0.5, sub=0.0, drive=0.25, keytrack=0.6),
      amp(0.03, 0.8, 0.8, 0.3), mod(0.06, 0.6, 0.45, 0.3, 0.45), voices=12, velsens=0.6),
    P("Velvet Stab", ["Keys"], "analog", "Short filtered chord stab for house and disco.",
      an(shape=0.4, detune=0.18, cutoff=750, reso=0.2, oscmix=0.5, sub=0.0, drive=0.5, keytrack=0.5),
      amp(0.004, 0.6, 0.4, 0.3), mod(0.001, 0.3, 0.0, 0.2, 0.32), reverb(0.5, 0.5, 0.01, 0.2), voices=14),
    P("Cluster Organ", ["Keys"], "cluster", "Octave stack of soft squares: a cheesy combo organ.",
      cl(detune=0.05, spread=0.5, tone=0.55, chord="octave", stack=3, shape=0.5, side=0.6, reso=0.05, keytrack=0.5, drift=0.05),
      amp(0.005, 0.2, 1.0, 0.08), mod(0.001, 0.2, 1.0, 0.1, 0.0), lfo(6.0, 0.02), voices=16, velsens=0.2),
    P("Minor Stab", ["Keys"], "cluster", "One finger plays a minor-seventh chord stab; detuned and punchy.",
      cl(detune=0.15, spread=0.7, tone=0.48, chord="min7", stack=4, shape=0.05, side=0.7, reso=0.15, keytrack=0.5, drift=0.1),
      amp(0.004, 0.9, 0.45, 0.3), mod(0.001, 0.28, 0.0, 0.2, 0.35), reverb(0.6, 0.5, 0.02, 0.22), voices=6),
    P("Mallet Keys", ["Keys", "Plucks"], "pluck", "Physically modelled wooden bars; soft mallet.",
      pl(material=1.0, damping=0.42, bright=0.6, body=0.35, excite=0.45, pickpos=0.2, bodysize=0.5),
      amp(0.002, 1.6, 0.0, 0.4), mod(0.001, 0.3, 0.0, 0.2, 0.1), drive(15000, 0.0, 0.05, 0, 0.7), voices=14, velsens=0.6),

    # ================================================================ PADS
    P("Vowel Pad", ["Pads"], "wavetable", "Formant table drifting between vowels; slow attack.",
      wt(table="formant", pos=0.3, warp=0.05, tone=0.6, unison=0.45, sub=0.1, reso=0.05, keytrack=0.5, envpos=0.35),
      amp(1.1, 1.5, 0.85, 2.0), mod(1.5, 2.0, 0.6, 2.0, 0.15), lfo(0.12, 0.2, "tri", "timbre"), chorus(0.35, 0.5, 0.35)),
    P("Glass Shimmer", ["Pads", "Textures"], "wavetable", "Glass table with wide unison: airy, glittering pad.",
      wt(table="glass", pos=0.5, warp=0.15, tone=0.62, unison=0.65, sub=0.0, reso=0.0, keytrack=0.6, envpos=0.15),
      amp(0.9, 1.5, 0.8, 2.5), mod(1.0, 2.0, 0.5, 2.0, 0.12), lfo(0.2, 0.15, "sine", "timbre"), reverb(0.8, 0.4, 0.03, 0.3)),
    P("Harmonic Swell", ["Pads"], "wavetable", "Harmonics bloom in as the pad swells.",
      wt(table="harmonic", pos=0.2, warp=0.0, tone=0.5, unison=0.4, sub=0.15, reso=0.1, keytrack=0.5, envpos=0.5),
      amp(1.4, 2.0, 0.9, 2.2), mod(2.0, 2.5, 0.7, 2.0, 0.25)),
    P("Super Stack", ["Pads"], "cluster", "Seven detuned saws per key, wide and lush.",
      cl(detune=0.28, spread=0.9, tone=0.45, chord="unison", stack=7, shape=0.0, side=0.7, reso=0.1, keytrack=0.5, drift=0.3),
      amp(0.7, 1.5, 0.85, 1.8), mod(0.8, 2.0, 0.6, 1.5, 0.15), chorus(0.3, 0.4, 0.25)),
    P("Fifth Cloud", ["Pads"], "cluster", "Open fifths with slow drift: cinematic and neutral.",
      cl(detune=0.18, spread=0.8, tone=0.42, chord="fifth", stack=5, shape=0.2, side=0.6, reso=0.08, keytrack=0.5, drift=0.45),
      amp(1.2, 2.0, 0.9, 2.5), mod(1.5, 2.0, 0.7, 2.0, 0.12), reverb(0.75, 0.5, 0.03, 0.28), voices=10),
    P("Sus Horizon", ["Pads"], "cluster", "Suspended-second stacks that never resolve; ambient beds.",
      cl(detune=0.15, spread=0.85, tone=0.4, chord="sus2", stack=4, shape=0.35, side=0.55, reso=0.05, keytrack=0.5, drift=0.5),
      amp(1.6, 2.0, 0.9, 3.0), mod(2.0, 2.0, 0.6, 2.0, 0.12), lfo(0.15, 0.12, "sine", "timbre"), voices=10),
    P("Warm Strings", ["Pads"], "analog", "Ensemble strings: detuned saws, an octave up, gentle chorus.",
      an(shape=0.0, detune=0.32, cutoff=1700, reso=0.08, osc2oct=1, oscmix=0.4, sub=0.0, drive=0.1, keytrack=0.6),
      amp(0.5, 1.0, 0.9, 1.3), mod(0.5, 1.0, 0.8, 1.0, 0.05), lfo(5.2, 0.03), chorus(0.45, 0.55, 0.45), voices=12),
    P("Analog Dawn", ["Pads"], "analog", "Pulse pad whose filter opens slowly, with a slow phaser.",
      an(shape=0.55, detune=0.22, cutoff=600, reso=0.18, oscmix=0.5, sub=0.1, drive=0.15, keytrack=0.5),
      amp(1.0, 1.5, 0.9, 2.0), mod(2.2, 2.0, 0.6, 2.0, 0.35), phaser(0.12, 0.6, 0.35, 0.35), voices=10),
    P("Night Drive", ["Pads", "Textures"], "analog", "Dark resonant pad breathing with a slow filter LFO.",
      an(shape=0.15, detune=0.28, cutoff=780, reso=0.35, oscmix=0.5, sub=0.1, drive=0.2, keytrack=0.5),
      amp(1.4, 2.0, 0.9, 2.5), mod(1.0, 2.0, 0.6, 2.0, 0.1), lfo(0.08, 0.35, "tri", "timbre"), chorus(0.25, 0.5, 0.3), voices=10),
    P("Frozen Cloud", ["Pads", "Textures"], "grain", "Granular freeze of the built-in harmonic texture.",
      gr(pos=0.4, size=180, density=30, spray=0.2, pitch=0, reverse=0.1, spread=0.8, scan=0.02, tone=0.7, inst=0),
      amp(1.2, 2.0, 0.9, 2.5), mod(0.5, 2.0, 0.6, 2.0, 0.05), voices=10),
    P("Glass Grains", ["Pads", "Textures"], "grain", "Grains of a singing wine glass (CC0 recording), slowly scanned.",
      gr(pos=0.45, size=220, density=24, spray=0.25, pitch=0, reverse=0.2, spread=0.85, scan=0.05, tone=0.75, inst="glass"),
      amp(1.0, 2.0, 0.9, 2.5), mod(0.5, 2.0, 0.6, 2.0, 0.05), reverb(0.7, 0.5, 0.03, 0.25), voices=10),
    P("Bowed Haze", ["Pads"], "grain", "Bowed vibraphone grains: a soft metallic halo.",
      gr(pos=0.5, size=260, density=20, spray=0.3, pitch=0, reverse=0.15, spread=0.8, scan=0.03, tone=0.65, inst="bowedmetal"),
      amp(1.3, 2.0, 0.9, 3.0), mod(0.5, 2.0, 0.6, 2.0, 0.05), voices=10),
    P("Breath Choir Pad", ["Pads", "Vocal"], "vocal", "Airy 'ah' choir with plenty of breath.",
      vo(vowel=0.0, formant=0.0, breath=0.45, tension=0.35, vibdepth=0.15, vibrate=5.0, jitter=0.25),
      amp(0.9, 1.5, 0.9, 1.8), mod(0.5, 2.0, 0.5, 1.5, 0.0), chorus(0.35, 0.5, 0.4), voices=12),
    P("Angel Pad", ["Pads", "Vocal"], "vocal", "High 'oo' voices morphing slowly towards 'ah'.",
      vo(vowel=3.6, formant=2.0, breath=0.3, tension=0.4, vibdepth=0.18, vibrate=5.2, jitter=0.2),
      amp(1.2, 2.0, 0.9, 2.5), mod(2.5, 3.0, 0.3, 2.0, -0.4), reverb(0.8, 0.45, 0.03, 0.3), voices=12),
    P("Bowed Metal", ["Pads", "Textures"], "sampler", "Bowed vibraphone recordings (CC0), looped: glassy and eerie.",
      sm(inst="bowedmetal", tone=0.9, loop=1), amp(0.7, 1.5, 0.9, 2.0), mod(0.5, 1.0, 1.0, 1.0, 0.0), voices=12),
    P("Crystal FM Pad", ["Pads"], "fm", "Five slightly detuned sine carriers sharing one modulator.",
      fm(algo=5, modratio=1.0, index=0.12, op1ratio=1.0, op2ratio=1.003, op3ratio=2.0, op4ratio=0.997, op5ratio=3.0,
         op6ratio=1.0, op1level=1.0, op2level=0.9, op3level=0.45, op4level=0.9, op5level=0.2, op6level=0.8),
      amp(1.0, 2.0, 0.85, 2.2), mod(1.5, 2.0, 0.6, 2.0, 0.3), lfo(0.25, 0.25, "sine", "timbre"), chorus(0.3, 0.45, 0.35), voices=12),

    # 0.5: sliding pads - poly glide: every voice of a new chord slides from the nearest note of the last one
    P("Glide Strings", ["Pads"], "analog", "Ensemble strings whose chords slide into each other (poly glide); legato chord changes.",
      an(shape=0.0, detune=0.3, cutoff=1500, reso=0.08, osc2oct=1, oscmix=0.35, sub=0.0, drive=0.1, keytrack=0.6),
      amp(0.6, 1.0, 0.9, 1.4), mod(0.5, 1.0, 0.8, 1.0, 0.05), lfo(5.0, 0.025), chorus(0.45, 0.55, 0.45), voices=8, glide=0.32, polyglide=1),
    P("Slide Choir", ["Pads", "Vocal"], "vocal", "Airy 'ah' choir; each voice slides to the next chord.",
      vo(vowel=0.0, formant=0.0, breath=0.35, tension=0.4, vibdepth=0.16, vibrate=5.0, jitter=0.25),
      amp(0.7, 1.5, 0.9, 1.6), mod(0.5, 2.0, 0.5, 1.5, 0.0), chorus(0.35, 0.5, 0.4), voices=8, glide=0.28, polyglide=1),
    P("Glass Glide", ["Pads", "Textures"], "wavetable", "Wide glass pad with slow, dreamy slides between chords.",
      wt(table="glass", pos=0.45, warp=0.12, tone=0.6, unison=0.6, sub=0.0, reso=0.0, keytrack=0.6, envpos=0.12),
      amp(0.8, 1.5, 0.85, 2.2), mod(1.0, 2.0, 0.5, 2.0, 0.12), lfo(0.2, 0.12, "sine", "timbre"), reverb(0.8, 0.4, 0.03, 0.3), voices=8,
      glide=0.5, polyglide=1),
    P("Brass Slide", ["Pads", "Keys"], "analog", "Swelling poly brass with quick portamento between chords.",
      an(shape=0.0, detune=0.22, cutoff=600, reso=0.12, oscmix=0.5, sub=0.0, drive=0.3, keytrack=0.6),
      amp(0.08, 0.8, 0.85, 0.5), mod(0.12, 0.7, 0.5, 0.4, 0.4), lfo(5.2, 0.02), voices=8, glide=0.18, polyglide=1),
    P("Drift Stack", ["Pads", "Textures"], "cluster", "Detuned saw stacks drifting slowly from chord to chord.",
      cl(detune=0.25, spread=0.85, tone=0.42, chord="unison", stack=5, shape=0.0, side=0.7, reso=0.08, keytrack=0.5, drift=0.35),
      amp(0.9, 1.5, 0.85, 2.0), mod(0.8, 2.0, 0.6, 1.5, 0.12), chorus(0.3, 0.4, 0.25), voices=8, glide=0.7, polyglide=1),
    P("Crystal Slide", ["Pads"], "fm", "Shimmering FM pad: detuned sine carriers sliding between chords.",
      fm(algo=5, modratio=1.0, index=0.1, op1ratio=1.0, op2ratio=1.003, op3ratio=2.0, op4ratio=0.997, op5ratio=3.0,
         op6ratio=1.0, op1level=1.0, op2level=0.9, op3level=0.4, op4level=0.9, op5level=0.15, op6level=0.8),
      amp(0.8, 2.0, 0.85, 2.0), mod(1.5, 2.0, 0.6, 2.0, 0.25), lfo(0.25, 0.2, "sine", "timbre"), chorus(0.3, 0.45, 0.35), voices=8,
      glide=0.4, polyglide=1),

    # ================================================================ LEADS
    P("Classic Mono Lead", ["Leads"], "analog", "Saw lead with a little vibrato, glide and a dotted delay.",
      an(shape=0.0, detune=0.12, cutoff=1400, reso=0.28, oscmix=0.5, sub=0.1, drive=0.3, keytrack=0.6),
      amp(0.004, 0.4, 0.85, 0.15), mod(0.001, 0.4, 0.4, 0.2, 0.28), lfo(5.4, 0.05), delay(0.45, 0.3, 0.5, 1, 0.2), MONO, glide=0.05),
    P("Square Hero", ["Leads"], "analog", "Hollow square plus an octave: a bright, singing lead.",
      an(shape=0.5, detune=0.06, cutoff=2400, reso=0.15, osc2oct=1, oscmix=0.3, sub=0.0, drive=0.2, keytrack=0.6),
      amp(0.005, 0.3, 0.9, 0.18), mod(0.001, 0.3, 0.6, 0.2, 0.15), lfo(5.6, 0.05), delay(0.6, 0.22, 0.4, 1, 0.16), MONO, glide=0.08),
    P("Soft Flute Lead", ["Leads"], "analog", "Breathy, resonant low-pass on a square: a soft whistle tone.",
      an(shape=0.5, detune=0.0, cutoff=520, reso=0.38, oscmix=0.0, sub=0.0, noise=0.05, ftype=0, drive=0.1, keytrack=0.9),
      amp(0.05, 0.3, 0.9, 0.2), mod(0.05, 0.3, 0.6, 0.2, 0.1), lfo(5.0, 0.06), reverb(0.6, 0.5, 0.03, 0.22), MONO, glide=0.04),
    P("Pop Lead", ["Leads"], "analog", "Bright saw and an octave square with vibrato: a clear, singing synth-pop lead.",
      an(shape=0.2, detune=0.1, cutoff=2400, reso=0.18, osc2oct=1, oscmix=0.25, sub=0.0, drive=0.25, keytrack=0.6),
      amp(0.006, 0.35, 0.85, 0.22), mod(0.001, 0.3, 0.55, 0.2, 0.15), lfo(5.5, 0.04), delay(0.4, 0.28, 0.45, 1, 0.18), MONO, glide=0.035),
    P("Hot Wire", ["Leads"], "analog", "Driven, resonant saw for rock-ish solos.",
      an(shape=0.05, detune=0.18, cutoff=1600, reso=0.45, oscmix=0.5, sub=0.0, drive=0.7, keytrack=0.5),
      amp(0.003, 0.4, 0.85, 0.15), mod(0.001, 0.35, 0.5, 0.2, 0.2), lfo(5.5, 0.06), MONO, glide=0.03),
    P("Digital Lead", ["Leads"], "wavetable", "Warped Digital table with unison: modern and cutting.",
      wt(table="digital", pos=0.35, warp=0.35, tone=0.68, unison=0.45, sub=0.1, reso=0.1, keytrack=0.6, envpos=0.1),
      amp(0.004, 0.4, 0.85, 0.15), mod(0.001, 0.4, 0.5, 0.2, 0.15), lfo(5.5, 0.04), delay(0.45, 0.25, 0.5, 1, 0.18), MONO, glide=0.04),
    P("Saw Unison Lead", ["Leads"], "wavetable", "Classic table at the saw position with wide unison.",
      wt(table="classic", pos=0.5, warp=0.0, tone=0.62, unison=0.8, sub=0.15, reso=0.1, keytrack=0.6, envpos=0.0),
      amp(0.004, 0.4, 0.9, 0.18), mod(0.001, 0.4, 0.6, 0.2, 0.12), delay(0.45, 0.25, 0.5, 1, 0.16), voices=4, glide=0.0),
    P("Formant Lead", ["Leads", "Vocal"], "wavetable", "Vowel-like lead that opens up with the envelope.",
      wt(table="formant", pos=0.2, warp=0.1, tone=0.7, unison=0.2, sub=0.0, reso=0.05, keytrack=0.6, envpos=0.4),
      amp(0.01, 0.5, 0.85, 0.2), mod(0.05, 0.6, 0.4, 0.3, 0.2), lfo(5.2, 0.05), MONO, glide=0.06),
    P("Super Lead", ["Leads"], "cluster", "Seven-saw unison lead: festival size.",
      cl(detune=0.32, spread=0.75, tone=0.58, chord="unison", stack=7, shape=0.0, side=0.7, reso=0.1, keytrack=0.6, drift=0.15),
      amp(0.004, 0.4, 0.9, 0.2), mod(0.001, 0.4, 0.6, 0.2, 0.15), delay(0.45, 0.28, 0.5, 1, 0.18), MONO, glide=0.03),
    P("Octave Lead", ["Leads"], "cluster", "Octave-stacked pulses with a quick filter blip.",
      cl(detune=0.08, spread=0.5, tone=0.55, chord="octave", stack=3, shape=0.6, side=0.6, reso=0.15, keytrack=0.6, drift=0.1),
      amp(0.003, 0.35, 0.85, 0.15), mod(0.001, 0.25, 0.5, 0.2, 0.25), lfo(5.6, 0.05), MONO, glide=0.05),
    P("FM Brass Lead", ["Leads"], "fm", "Brassy FM lead: the index rises with a short swell.",
      fm(algo=2, modratio=1.0, index=0.42, op1ratio=1, op2ratio=1, op3ratio=1, op4ratio=2, op1level=1.0, op2level=0.75,
         op3level=0.3, op4level=0.3, op5level=0.0, op6level=0.0),
      amp(0.03, 0.4, 0.85, 0.15), mod(0.06, 0.4, 0.55, 0.2, 0.7), lfo(5.4, 0.05), MONO, glide=0.03),
    P("Sine Whistle", ["Leads"], "fm", "Almost pure sine with a breath of modulation; delicate melodies.",
      fm(algo=2, modratio=1.0, index=0.06, op1ratio=1, op2ratio=2, op1level=1.0, op2level=0.5, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      amp(0.03, 0.3, 0.9, 0.25), mod(0.01, 0.3, 0.5, 0.2, 0.5), lfo(5.0, 0.07), reverb(0.6, 0.5, 0.03, 0.25), MONO, glide=0.05),
    P("Vox Lead", ["Leads", "Vocal"], "vocal", "Solo synthetic voice with expressive vibrato and glide.",
      vo(vowel=0.5, formant=1.0, breath=0.12, tension=0.6, vibdepth=0.32, vibrate=5.6, jitter=0.15),
      amp(0.04, 0.5, 0.9, 0.25), mod(0.1, 0.6, 0.3, 0.3, 0.2), reverb(0.6, 0.5, 0.03, 0.22), MONO, glide=0.07),
    P("Recorder", ["Leads"], "sampler", "Recorded alto recorder, looped, with light vibrato.",
      sm(inst="recorder", tone=1.0, loop=1), amp(0.03, 0.3, 1.0, 0.18), mod(0.001, 0.3, 1.0, 0.2, 0.0), lfo(5.2, 0.05),
      reverb(0.55, 0.5, 0.02, 0.2), voices=4, velsens=0.4),

    # ================================================================ PLUCKS
    P("Nylon Guitar", ["Plucks"], "pluck", "Warm nylon string plucked near the soundhole.",
      pl(material=0.0, damping=0.3, bright=0.3, body=0.5, excite=0.42, pickpos=0.25, bodysize=0.6),
      amp(0.001, 2.5, 0.0, 0.35), mod(0.001, 0.4, 0.0, 0.3, 0.1), drive(9000, 0.0, 0.04, 0, 1.0), voices=12, velsens=0.65),
    P("Steel String", ["Plucks"], "pluck", "Bright steel string plucked near the bridge.",
      pl(material=0.0, damping=0.22, bright=0.48, body=0.35, excite=0.45, pickpos=0.14, bodysize=0.5),
      amp(0.001, 2.8, 0.0, 0.35), mod(0.001, 0.4, 0.0, 0.3, 0.1), drive(12000, 0.0, 0.04, 0, 1.0), voices=12, velsens=0.65),
    P("Harp Pluck", ["Plucks"], "pluck", "Long-ringing harp-like string with a soft attack.",
      pl(material=0.0, damping=0.18, bright=0.32, body=0.3, excite=0.28, pickpos=0.3, bodysize=0.4),
      amp(0.001, 3.0, 0.0, 1.2), mod(0.001, 0.4, 0.0, 0.3, 0.1), reverb(0.65, 0.45, 0.02, 0.22), voices=14, velsens=0.6),
    P("Koto Pluck", ["Plucks"], "pluck", "Bright, twangy string with a resonant body.",
      pl(material=0.15, damping=0.32, bright=0.46, body=0.6, excite=0.45, pickpos=0.1, bodysize=0.3),
      amp(0.001, 2.0, 0.0, 0.4), mod(0.001, 0.4, 0.0, 0.3, 0.1), drive(11000, 0.0, 0.04, 0, 1.0), voices=12, velsens=0.6),
    P("FM Koto", ["Plucks"], "fm", "Plucked FM: a bright attack that quickly mellows.",
      fm(algo=2, modratio=1.0, index=0.48, op1ratio=1, op2ratio=3, op3ratio=1, op4ratio=5, op1level=1.0, op2level=0.6,
         op3level=0.4, op4level=0.2, op5level=0.0, op6level=0.0),
      amp(0.001, 1.4, 0.0, 0.35), mod(0.001, 0.22, 0.0, 0.2, 1.0), reverb(0.55, 0.5, 0.02, 0.2), voices=12),
    P("Steel Drum FM", ["Plucks", "Keys"], "fm", "Tropical pan tones from inharmonic FM.",
      fm(algo=2, modratio=1.0, index=0.3, op1ratio=1, op2ratio=2.5, op3ratio=2, op4ratio=3.5, op1level=1.0, op2level=0.55,
         op3level=0.3, op4level=0.25, op5level=0.0, op6level=0.0),
      amp(0.001, 1.2, 0.0, 0.4), mod(0.001, 0.3, 0.1, 0.2, 0.8), voices=12),
    P("Poly Pluck", ["Plucks"], "analog", "Classic synth pluck: snappy filter on a detuned saw pair.",
      an(shape=0.05, detune=0.18, cutoff=420, reso=0.28, oscmix=0.5, sub=0.1, drive=0.5, keytrack=0.6),
      amp(0.003, 0.45, 0.1, 0.3), mod(0.001, 0.17, 0.0, 0.15, 0.58), delay(0.45, 0.28, 0.45, 1, 0.2), voices=12),
    P("Muted Pick", ["Plucks"], "analog", "Short palm-muted square pluck for fast lines.",
      an(shape=0.48, detune=0.05, cutoff=450, reso=0.2, oscmix=0.3, sub=0.15, drive=0.5, keytrack=0.6),
      amp(0.003, 0.26, 0.08, 0.12), mod(0.001, 0.08, 0.0, 0.1, 0.42), voices=10),
    P("Concert Harp", ["Plucks"], "sampler", "Recorded concert harp (eight notes, CC0).",
      sm(inst="harp", tone=1.0), amp(0.001, 2.6, 0.0, 1.0), mod(0.001, 0.3, 0.0, 0.2, 0.0), reverb(0.6, 0.5, 0.02, 0.2), voices=14),
    P("Zither", ["Plucks"], "sampler", "Recorded zither: shimmering plucked strings (CC0).",
      sm(inst="zither", tone=1.0), amp(0.001, 2.6, 0.0, 0.8), mod(0.001, 0.3, 0.0, 0.2, 0.0), drive(15000, 0.0, 0.05, 0, 0.7), voices=14),
    P("Kalimba", ["Plucks", "Keys"], "sampler", "Recorded thumb piano with its buzzy, woody tines (CC0).",
      sm(inst="kalimba", tone=0.78), amp(0.001, 2.8, 0.0, 0.6), mod(0.001, 0.3, 0.0, 0.2, 0.0), reverb(0.5, 0.5, 0.02, 0.18), voices=12),
    P("Table Pluck", ["Plucks"], "wavetable", "Wavetable pluck: bright saw frame closing fast.",
      wt(table="classic", pos=0.45, warp=0.1, tone=0.3, unison=0.25, sub=0.0, reso=0.2, keytrack=0.6, envpos=-0.2),
      amp(0.001, 0.5, 0.0, 0.3), mod(0.001, 0.15, 0.0, 0.15, 0.55), delay(0.45, 0.25, 0.5, 1, 0.18), voices=12),
    P("Glass Pluck", ["Plucks"], "wavetable", "Glass table pluck with a bell-like sparkle.",
      wt(table="glass", pos=0.6, warp=0.0, tone=0.55, unison=0.1, sub=0.0, reso=0.05, keytrack=0.6, envpos=-0.4),
      amp(0.001, 0.9, 0.0, 0.5), mod(0.001, 0.3, 0.0, 0.3, 0.3), reverb(0.6, 0.5, 0.02, 0.22), voices=12),

    # ================================================================ DRUMS (synth percussion on a part)
    P("Filter Kick", ["Drums"], "analog", "Kick from a self-oscillating ladder swept down by the envelope. Play around C1-C2.",
      an(shape=0.5, detune=0.0, cutoff=55, reso=0.97, oscmix=0.0, sub=0.0, noise=0.0, drive=0.4, keytrack=0.0),
      amp(0.001, 0.38, 0.0, 0.1), mod(0.001, 0.07, 0.0, 0.1, 0.7), velsens=0.5, voices=2),
    P("Sub Kick", ["Drums", "Bass", "Boom"], "analog", "Tunable deep kick: a self-oscillating filter tuned to the key, swept down from above; long boom, driven.",
      an(shape=0.5, detune=0.0, cutoff=261.6, reso=0.97, oscmix=0.0, sub=0.0, noise=0.0, drive=0.6, keytrack=1.0),
      amp(0.001, 1.2, 0.0, 0.25), mod(0.001, 0.05, 0.0, 0.1, 0.55), velsens=0.4, voices=2),
    P("Noise Snare", ["Drums"], "analog", "Band-passed noise and tone: an electronic snare.",
      an(shape=0.5, detune=0.0, cutoff=1800, reso=0.25, oscmix=0.0, sub=0.35, noise=1.0, ftype=2, drive=0.55, keytrack=0.2),
      amp(0.001, 0.2, 0.0, 0.12), mod(0.001, 0.05, 0.0, 0.1, 0.25), velsens=0.6, voices=4),
    P("Tick Hat", ["Drums"], "analog", "High-passed noise tick for hi-hat patterns.",
      an(shape=0.0, cutoff=7500, reso=0.15, oscmix=0.0, sub=0.0, noise=1.0, ftype=3, drive=0.1, keytrack=0.0),
      amp(0.001, 0.08, 0.0, 0.05), mod(0.001, 0.05, 0.0, 0.05, 0.0), drive(13000, 0.0, 0.12, 0, 1.0), velsens=0.7, voices=4),
    P("FM Tom", ["Drums"], "fm", "Pitched FM drum with a woody knock.",
      fm(algo=2, modratio=1.0, index=0.35, op1ratio=1, op2ratio=1.5, op1level=1.0, op2level=0.6, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      amp(0.001, 0.4, 0.0, 0.15), mod(0.001, 0.05, 0.0, 0.05, 1.0), velsens=0.6, voices=6),
    P("Metal Hat FM", ["Drums"], "fm", "Inharmonic FM noise: closed and open hats depending on the key length.",
      fm(algo=0, modratio=1.0, index=0.9, feedback=0.6, op1ratio=7.1, op2ratio=3.33, op3ratio=5.21, op4ratio=1.41, op5ratio=2.7,
         op6ratio=4.3, op1level=1.0, op2level=1.0, op3level=1.0, op4level=1.0, op5level=1.0, op6level=1.0),
      amp(0.001, 0.08, 0.0, 0.08), mod(0.001, 0.1, 1.0, 0.1, 0.0), drive(8500, 0.1, 0.0, 1, 1.0), velsens=0.7, voices=4, tune=-12),
    P("FM Snare", ["Drums"], "fm", "Feedback noise plus a body tone: a crisp digital snare.",
      fm(algo=2, modratio=1.0, index=0.7, feedback=1.0, op1ratio=1, op2ratio=2.3, op3ratio=1.6, op4ratio=3.7, op1level=0.8,
         op2level=0.9, op3level=0.6, op4level=0.9, op5level=0.7, op6level=1.0),
      amp(0.001, 0.19, 0.0, 0.12), mod(0.001, 0.06, 0.3, 0.1, 0.6), drive(14000, 0.0, 0.06, 0, 0.8), velsens=0.6, voices=4),
    P("Wood Block", ["Drums"], "pluck", "Hard mallet on a short wooden bar.",
      pl(material=1.0, damping=0.75, bright=0.6, body=0.3, excite=0.85, pickpos=0.15, bodysize=0.2),
      amp(0.001, 0.3, 0.0, 0.1), mod(0.001, 0.1, 0.0, 0.1, 0.0), velsens=0.7, voices=6),
    P("Tuned Tom", ["Drums"], "pluck", "Modelled drum-like bar with a big body; play it in octaves.",
      pl(material=0.8, damping=0.55, bright=0.3, body=0.75, excite=0.6, pickpos=0.4, bodysize=0.95),
      amp(0.001, 0.6, 0.0, 0.2), mod(0.001, 0.1, 0.0, 0.1, 0.0), velsens=0.6, voices=6),
    P("Cluster Hat", ["Drums"], "cluster", "Stacked narrow pulses detuned into a metallic hat.",
      cl(detune=1.0, spread=0.7, tone=0.92, chord="fourths", stack=7, shape=1.0, side=1.0, reso=0.2, keytrack=0.3, drift=0.0),
      amp(0.001, 0.22, 0.0, 0.1), mod(0.001, 0.05, 0.0, 0.05, 0.0), drive(14000, 0.0, 0.08, 0, 1.0), velsens=0.7, voices=4, tune=12),
    P("Zap Perc", ["Drums", "FX"], "wavetable", "Digital table snapped shut: zaps, clicks and lasers.",
      wt(table="digital", pos=0.8, warp=0.5, tone=0.25, unison=0.0, sub=0.3, reso=0.4, keytrack=0.3, envpos=-0.8),
      amp(0.001, 0.25, 0.0, 0.1), mod(0.001, 0.07, 0.0, 0.08, 0.75), drive(15000, 0.0, 0.05, 0, 0.7), velsens=0.6, voices=4),

    # ================================================================ FX
    P("Noise Riser", ["FX"], "analog", "Hold a key: filtered noise climbs for four seconds.",
      an(shape=0.0, cutoff=300, reso=0.55, oscmix=0.0, sub=0.0, noise=1.0, ftype=2, drive=0.45, keytrack=0.2),
      amp(3.5, 1.0, 1.0, 1.2), mod(4.0, 1.0, 1.0, 1.2, 0.65), lfo(0.5, 0.4, "sine", "pan"), voices=4),
    P("Laser Drop", ["FX", "Drums"], "analog", "Resonant falling sweep; short hits become lasers.",
      an(shape=0.0, cutoff=180, reso=0.9, oscmix=0.0, sub=0.0, drive=0.3, keytrack=0.2),
      amp(0.001, 0.9, 0.0, 0.3), mod(0.001, 0.6, 0.0, 0.3, 0.8), delay(0.3, 0.35, 0.5, 1, 0.25), voices=4),
    P("Alien Signal", ["FX"], "fm", "Random-stepping FM timbre: radio chatter from far away.",
      fm(algo=0, modratio=1.37, index=0.18, feedback=0.1, op1ratio=1, op2ratio=1, op3ratio=2, op4ratio=1, op5ratio=2, op6ratio=1,
         op1level=1.0, op2level=0.6, op3level=0.5, op4level=0.5, op5level=0.4, op6level=0.4),
      amp(0.05, 1.0, 0.8, 1.0), mod(0.01, 1.0, 0.6, 1.0, 0.3), lfo(7.0, 0.5, "sh", "timbre"), delay(0.33, 0.45, 0.35, 1, 0.3), voices=4),
    P("Gong Hit", ["FX", "Textures"], "fm", "Inharmonic FM gong with a long shimmer.",
      fm(algo=2, modratio=1.0, index=0.4, op1ratio=1, op2ratio=1.4, op3ratio=1.0, op4ratio=2.76, op5ratio=0.5, op6ratio=1.93,
         op1level=1.0, op2level=0.7, op3level=0.6, op4level=0.5, op5level=0.5, op6level=0.4),
      amp(0.002, 5.0, 0.0, 3.0), mod(0.001, 2.5, 0.2, 2.0, 0.6), reverb(0.85, 0.4, 0.03, 0.3), voices=6),
    P("Table Scanner", ["FX", "Textures"], "wavetable", "Digital table swept by a slow LFO: robotic movement.",
      wt(table="digital", pos=0.5, warp=0.3, tone=0.55, unison=0.3, sub=0.0, reso=0.3, keytrack=0.5, envpos=0.6),
      amp(0.2, 1.0, 0.9, 1.0), mod(2.0, 2.0, 0.0, 1.0, 0.2), lfo(0.7, 0.45, "tri", "timbre"), voices=6),
    P("Grit Sweep", ["FX"], "wavetable", "Noisy Grit table opening over two seconds.",
      wt(table="grit", pos=0.7, warp=0.4, tone=0.35, unison=0.5, sub=0.0, reso=0.35, keytrack=0.3, envpos=0.5),
      amp(1.5, 1.0, 0.9, 1.5), mod(2.0, 1.0, 1.0, 1.5, 0.45), drive(15000, 0.0, 0.05, 0, 0.7), voices=6),
    P("Detune Swarm", ["FX", "Textures"], "cluster", "Seven wildly drifting oscillators: a swarm.",
      cl(detune=1.0, spread=1.0, tone=0.45, chord="unison", stack=7, shape=0.3, side=0.9, reso=0.2, keytrack=0.4, drift=1.0),
      amp(1.0, 1.0, 0.9, 2.0), mod(1.0, 1.0, 1.0, 1.0, 0.0), lfo(0.3, 0.3, "sine", "timbre"), voices=6),
    P("Granular Rise", ["FX"], "grain", "Grains scan forward and climb while the key is held.",
      gr(pos=0.1, size=90, density=40, spray=0.35, pitch=0, reverse=0.0, spread=0.9, scan=1.2, tone=0.6, inst=0),
      amp(2.5, 1.0, 1.0, 1.5), mod(3.0, 1.0, 1.0, 1.5, 0.35), lfo(0.25, 0.3, "saw", "pitch"), voices=6),
    P("Reverse Spray", ["FX", "Textures"], "grain", "Mostly reversed harp grains sprayed across the stereo field.",
      gr(pos=0.08, size=200, density=20, spray=0.2, pitch=0, reverse=0.8, spread=1.0, scan=0.0, tone=0.7, inst="harp"),
      amp(0.3, 1.0, 0.9, 1.8), mod(0.5, 1.0, 1.0, 1.0, 0.0), voices=6),
    P("Stutter Glitch", ["FX"], "grain", "Tiny, sparse grains: glitchy stutters from the marimba.",
      gr(pos=0.02, size=45, density=22, spray=0.12, pitch=0, reverse=0.3, spread=1.0, scan=0.0, tone=0.8, inst="marimba"),
      amp(0.005, 1.0, 1.0, 0.4), mod(0.01, 1.0, 1.0, 0.4, 0.0), lfo(8.0, 0.4, "sh", "pan"), voices=6),

    # ================================================================ ARP
    P("Analog Arp", ["Arp", "Plucks"], "analog", "Resonant square blips with a ping-pong delay.",
      an(shape=0.45, detune=0.05, cutoff=900, reso=0.35, oscmix=0.3, sub=0.0, drive=0.5, keytrack=0.6),
      amp(0.003, 0.24, 0.0, 0.1), mod(0.001, 0.12, 0.0, 0.1, 0.45), delay(0.45, 0.35, 0.45, 1, 0.24), voices=8),
    P("Bubble Arp", ["Arp"], "analog", "Narrow pulse through a resonant filter: bubbly and round.",
      an(shape=0.75, detune=0.0, cutoff=1100, reso=0.5, oscmix=0.0, sub=0.15, ftype=1, drive=0.15, keytrack=0.6),
      amp(0.001, 0.15, 0.0, 0.08), mod(0.001, 0.08, 0.0, 0.08, 0.35), delay(0.3, 0.3, 0.5, 1, 0.2), voices=8),
    P("FM Arp Bell", ["Arp", "Keys"], "fm", "Short glassy FM bells for sequences.",
      fm(algo=2, modratio=1.0, index=0.28, op1ratio=1, op2ratio=3.5, op3ratio=2, op4ratio=7, op1level=1.0, op2level=0.6,
         op3level=0.3, op4level=0.2, op5level=0.0, op6level=0.0),
      amp(0.001, 0.6, 0.0, 0.3), mod(0.001, 0.2, 0.0, 0.2, 1.0), delay(0.45, 0.3, 0.5, 1, 0.22), voices=10),
    P("Sparkle Bell", ["Arp", "Plucks"], "fm", "Short, bright FM bell for sparkling arpeggios over chords.",
      fm(algo=2, modratio=1.0, index=0.3, op1ratio=1, op2ratio=4, op3ratio=2, op4ratio=9, op1level=1.0, op2level=0.55,
         op3level=0.25, op4level=0.12, op5level=0.0, op6level=0.0),
      amp(0.001, 0.45, 0.0, 0.3), mod(0.001, 0.15, 0.0, 0.15, 1.0), delay(0.4, 0.3, 0.55, 1, 0.2), voices=10, velsens=0.6),
    P("Digital Pluck Arp", ["Arp"], "fm", "Clicky digital pluck that loves fast sixteenths.",
      fm(algo=2, modratio=1.0, index=0.5, op1ratio=1, op2ratio=2, op1level=1.0, op2level=0.75, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      amp(0.001, 0.25, 0.0, 0.1), mod(0.001, 0.07, 0.0, 0.1, 1.0), delay(0.3, 0.3, 0.5, 1, 0.2), voices=8),
    P("Table Arp", ["Arp"], "wavetable", "Wavetable sequence sound: the position snaps with each note.",
      wt(table="harmonic", pos=0.6, warp=0.2, tone=0.55, unison=0.2, sub=0.0, reso=0.15, keytrack=0.6, envpos=-0.5),
      amp(0.001, 0.25, 0.0, 0.12), mod(0.001, 0.12, 0.0, 0.1, 0.3), delay(0.45, 0.3, 0.5, 1, 0.2), voices=8),
    P("Formant Arp", ["Arp", "Vocal"], "wavetable", "Vowel blips for talking sequences.",
      wt(table="formant", pos=0.7, warp=0.0, tone=0.65, unison=0.1, sub=0.0, reso=0.1, keytrack=0.6, envpos=-0.6),
      amp(0.001, 0.22, 0.0, 0.1), mod(0.001, 0.15, 0.0, 0.1, 0.2), voices=8),
    P("Pizzicato Arp", ["Arp", "Plucks"], "pluck", "Short plucked strings for ostinatos.",
      pl(material=0.0, damping=0.45, bright=0.32, body=0.6, excite=0.4, pickpos=0.2, bodysize=0.5),
      amp(0.002, 0.6, 0.0, 0.15), mod(0.001, 0.2, 0.0, 0.1, 0.1), drive(15000, 0.0, 0.05, 0, 0.7), voices=10),
    P("Glock Arp", ["Arp", "Keys"], "sampler", "Recorded glockenspiel for sparkling arpeggios (CC0).",
      sm(inst="glockenspiel", tone=0.9), amp(0.001, 1.5, 0.0, 0.5), mod(0.001, 0.3, 0.0, 0.2, 0.0), delay(0.45, 0.3, 0.5, 1, 0.18), voices=12),

    # ================================================================ TEXTURES
    P("Kalimba Dust", ["Textures"], "grain", "Tiny kalimba grains scattered like dust.",
      gr(pos=0.03, size=70, density=45, spray=0.12, pitch=0, reverse=0.2, spread=1.0, scan=0.0, tone=0.75, inst="kalimba"),
      amp(0.8, 1.5, 0.9, 2.0), mod(0.5, 1.0, 1.0, 1.0, 0.0), voices=8),
    P("Harp Mist", ["Textures", "Pads"], "grain", "Long, soft harp grains blurred into a mist.",
      gr(pos=0.06, size=320, density=18, spray=0.15, pitch=0, reverse=0.25, spread=0.9, scan=0.0, tone=0.65, inst="harp"),
      amp(1.0, 2.0, 0.9, 2.5), mod(0.5, 1.0, 1.0, 1.0, 0.0), voices=8),
    P("Marimba Rain", ["Textures"], "grain", "Sparse marimba grains falling like rain.",
      gr(pos=0.02, size=110, density=16, spray=0.3, pitch=0, reverse=0.0, spread=1.0, scan=0.0, tone=0.7, inst="marimba"),
      amp(0.4, 1.5, 0.9, 2.0), mod(0.5, 1.0, 1.0, 1.0, 0.0), voices=8),
    P("Recorder Breath", ["Textures", "Vocal"], "grain", "Breathy recorder grains: airy, vocal-like tones.",
      gr(pos=0.5, size=140, density=28, spray=0.45, pitch=0, reverse=0.3, spread=0.9, scan=0.0, tone=0.6, inst="recorder"),
      amp(1.0, 1.5, 0.9, 2.0), mod(0.5, 1.0, 1.0, 1.0, 0.0), voices=8),
    P("Zither Cloud", ["Textures"], "grain", "Shimmering zither grains, octave sprinkles.",
      gr(pos=0.05, size=180, density=24, spray=0.15, pitch=0, reverse=0.15, spread=1.0, scan=0.0, tone=0.7, inst="zither"),
      amp(0.8, 1.5, 0.9, 2.2), mod(0.5, 1.0, 1.0, 1.0, 0.0), lfo(0.1, 0.1, "sine", "pan"), voices=8),
    P("Singing Glass", ["Textures", "Pads"], "sampler", "Rubbed wine-glass recordings (CC0), looped: pure and haunting.",
      sm(inst="glass", tone=1.0, loop=1), amp(0.6, 1.5, 0.9, 2.2), mod(0.5, 1.0, 1.0, 1.0, 0.0), lfo(0.3, 0.12, "sine", "amp"), voices=10),
    P("Backwards Kalimba", ["Textures", "FX"], "sampler", "The kalimba played in reverse: swells that end in a pluck.",
      sm(inst="kalimba", tone=0.85, reverse=1), amp(0.001, 3.0, 1.0, 0.25), mod(0.001, 0.3, 1.0, 0.2, 0.0), reverb(0.7, 0.5, 0.02, 0.25), voices=10),
    P("Fourths Drift", ["Textures", "Pads"], "cluster", "Stacked fourths with heavy drift: an unresolved haze.",
      cl(detune=0.25, spread=0.9, tone=0.38, chord="fourths", stack=4, shape=0.5, side=0.5, reso=0.1, keytrack=0.5, drift=0.8),
      amp(1.5, 2.0, 0.9, 3.0), mod(1.0, 2.0, 1.0, 2.0, 0.0), lfo(0.07, 0.2, "sine", "timbre"), voices=8),
    P("Grit Drone", ["Textures"], "wavetable", "Slowly churning Grit table; hold one low note.",
      wt(table="grit", pos=0.4, warp=0.2, tone=0.5, unison=0.6, sub=0.25, reso=0.2, keytrack=0.4, envpos=0.0),
      amp(1.5, 2.0, 0.9, 2.5), mod(1.0, 2.0, 1.0, 2.0, 0.0), lfo(0.05, 0.4, "tri", "timbre"), voices=6),
    P("Breath Texture", ["Textures", "Vocal"], "vocal", "Nearly unvoiced breath through slowly moving vowels.",
      vo(vowel=2.0, formant=0.0, breath=0.9, tension=0.3, vibdepth=0.0, vibrate=5.0, jitter=0.5),
      amp(1.0, 1.5, 0.9, 2.0), mod(0.5, 1.0, 1.0, 1.0, 0.0), lfo(0.15, 0.5, "tri", "timbre"), voices=8),

    # ================================================================ VOCAL
    P("Aah Choir", ["Vocal", "Pads"], "vocal", "Mixed choir singing 'ah'.",
      vo(vowel=0.0, formant=0.0, breath=0.25, tension=0.45, vibdepth=0.2, vibrate=5.2, jitter=0.25),
      amp(0.35, 1.0, 0.9, 0.9), mod(0.5, 1.0, 1.0, 1.0, 0.0), chorus(0.4, 0.5, 0.4), voices=12),
    P("Ooh Choir", ["Vocal", "Pads"], "vocal", "Soft 'oo' choir, round and dark.",
      vo(vowel=4.0, formant=0.0, breath=0.2, tension=0.35, vibdepth=0.18, vibrate=5.0, jitter=0.2),
      amp(0.4, 1.0, 0.9, 1.0), mod(0.5, 1.0, 1.0, 1.0, 0.0), chorus(0.35, 0.5, 0.4), voices=12),
    P("Solo Soprano", ["Vocal", "Leads"], "vocal", "High solo voice: bright formants, generous vibrato.",
      vo(vowel=0.3, formant=3.0, breath=0.12, tension=0.62, vibdepth=0.35, vibrate=5.8, jitter=0.12),
      amp(0.06, 0.5, 0.9, 0.35), mod(0.1, 0.8, 0.3, 0.3, 0.15), reverb(0.7, 0.45, 0.03, 0.25), MONO, glide=0.06),
    P("Male Ensemble", ["Vocal", "Pads"], "vocal", "Low voices on 'oh': a men's choir.",
      vo(vowel=3.0, formant=-3.0, breath=0.2, tension=0.4, vibdepth=0.15, vibrate=4.8, jitter=0.25),
      amp(0.35, 1.0, 0.9, 1.0), mod(0.5, 1.0, 1.0, 1.0, 0.0), chorus(0.35, 0.5, 0.35), voices=12),
    P("Talking Synth", ["Vocal", "Leads"], "vocal", "The LFO walks through the vowels: a talking lead.",
      vo(vowel=1.5, formant=0.0, breath=0.1, tension=0.6, vibdepth=0.1, vibrate=5.5, jitter=0.1),
      amp(0.01, 0.4, 0.9, 0.2), mod(0.01, 0.3, 0.5, 0.2, 0.0), lfo(1.6, 0.8, "tri", "timbre"), MONO, glide=0.05),
    P("Vowel Morph", ["Vocal"], "vocal", "Each note sings 'ee' and opens to 'ah' with the envelope.",
      vo(vowel=2.0, formant=1.0, breath=0.15, tension=0.5, vibdepth=0.22, vibrate=5.4, jitter=0.15),
      amp(0.05, 1.0, 0.9, 0.5), mod(0.01, 0.6, 0.0, 0.3, -1.0), voices=10),
    P("Gospel Hum", ["Vocal"], "vocal", "Closed-mouth hum: warm, low and intimate.",
      vo(vowel=4.0, formant=-1.5, breath=0.35, tension=0.25, vibdepth=0.12, vibrate=5.0, jitter=0.3),
      amp(0.2, 1.0, 0.9, 0.8), mod(0.5, 1.0, 1.0, 1.0, 0.0), voices=12),
    P("Formant Voices", ["Vocal", "Pads"], "wavetable", "Formant wavetable with unison: synthetic choir.",
      wt(table="formant", pos=0.05, warp=0.0, tone=0.62, unison=0.55, sub=0.0, reso=0.05, keytrack=0.5, envpos=0.0),
      amp(0.4, 1.0, 0.9, 1.2), mod(0.5, 1.0, 1.0, 1.0, 0.0), lfo(5.0, 0.03), chorus(0.4, 0.5, 0.35), voices=12),

    # ================================================================ 0.6 (sound revision 2)
    # Lo-fi
    P("Dusty Keys", ["Keys"], "fm", "Mellow, dark electric piano for lo-fi chords: strong tape wow on the pitch, a bit-reduced edge, soft attack.",
      fm(algo=2, modratio=1.0, index=0.17, op1ratio=1, op2ratio=1, op3ratio=1, op4ratio=14, op5ratio=1, op6ratio=1,
         op1level=1.0, op2level=0.55, op3level=0.6, op4level=0.04, op5level=0.0, op6level=0.0),
      amp(0.006, 3.4, 0.25, 0.6), mod(0.001, 1.6, 0.2, 0.5, 0.45), lfo(0.55, 0.075, "sine", "pitch"), crush(11, 3, 0.4),
      voices=14, velsens=0.55),
    P("Vinyl Crackle", ["Textures", "FX"], "analog", "Record crackle and soft hiss (Analog engine: oscillators off, crackle and noise on); hold a note under a beat.",
      an(shape=0.5, detune=0.0, cutoff=700, reso=0.05, oscmix=0.0, sub=0.0, noise=0.16, ftype=3, drive=0.6, keytrack=0.0,
         osclevel=0.0, crackle=0.75),
      amp(0.08, 0.5, 1.0, 0.6), mod(0.001, 0.5, 1.0, 0.5, 0.0), drive(8000, 0.0, 0.0, 0, 1.0), velsens=0.0, voices=1),
    P("Mellow Lofi Bass", ["Bass"], "analog", "Warm, round bass: a soft, low-passed body over a sine sub with gentle saturation and a hint of glide.",
      an(shape=0.25, detune=0.0, cutoff=300, reso=0.05, osc2oct=0, oscmix=0.0, sub=0.85, drive=0.42, keytrack=0.6, osclevel=0.45),
      amp(0.006, 0.9, 0.7, 0.14), mod(0.001, 0.2, 0.1, 0.1, 0.12), MONO, velsens=0.35, **porta(0.035, "legato")),

    # Rise / fall slides: the 0.6 pitch envelope - every new note slides into its pitch
    P("Rise Strings", ["Pads"], "analog", "Ensemble strings whose notes rise an octave up into the chord on every new note.",
      an(shape=0.0, detune=0.3, cutoff=1500, reso=0.08, osc2oct=1, oscmix=0.35, sub=0.0, drive=0.1, keytrack=0.6),
      amp(0.35, 1.0, 0.9, 1.3), mod(0.4, 1.0, 0.8, 1.0, 0.06), lfo(5.0, 0.02), chorus(0.45, 0.55, 0.45), pitchenv(-12, 0.7, 0.55), voices=10),
    P("Rise Choir", ["Pads", "Vocal"], "vocal", "Airy 'ah' choir that scoops up a fourth into every chord.",
      vo(vowel=0.0, formant=0.0, breath=0.3, tension=0.42, vibdepth=0.14, vibrate=5.0, jitter=0.25),
      amp(0.45, 1.5, 0.9, 1.5), mod(0.5, 2.0, 0.5, 1.5, 0.0), chorus(0.35, 0.5, 0.4), pitchenv(-5, 0.45, 0.5), voices=10),
    P("Rise Glass", ["Pads", "Textures"], "wavetable", "Wide glass pad: each note glides up a fifth into place, dreamy and slow.",
      wt(table="glass", pos=0.45, warp=0.12, tone=0.6, unison=0.6, sub=0.0, reso=0.0, keytrack=0.6, envpos=0.12),
      amp(0.6, 1.5, 0.85, 2.0), mod(1.0, 2.0, 0.5, 2.0, 0.12), lfo(0.2, 0.12, "sine", "timbre"), reverb(0.8, 0.4, 0.03, 0.3),
      pitchenv(-7, 0.9, 0.6), voices=10),
    P("Rise Warmth", ["Pads"], "cluster", "Soft detuned saw stack whose notes lean up a whole tone into the chord.",
      cl(detune=0.22, spread=0.8, tone=0.38, chord="unison", stack=5, shape=0.0, side=0.7, reso=0.08, keytrack=0.5, drift=0.3),
      amp(0.7, 1.5, 0.85, 1.8), mod(0.8, 2.0, 0.6, 1.5, 0.1), chorus(0.3, 0.4, 0.25), pitchenv(-2, 0.6, 0.4), voices=10),
    P("Rise Bloom", ["Pads"], "fm", "Shimmering FM pad blooming up from an octave below; long and bright.",
      fm(algo=5, modratio=1.0, index=0.1, op1ratio=1.0, op2ratio=1.003, op3ratio=2.0, op4ratio=0.997, op5ratio=3.0,
         op6ratio=1.0, op1level=1.0, op2level=0.9, op3level=0.4, op4level=0.9, op5level=0.15, op6level=0.8),
      amp(0.5, 2.0, 0.85, 2.0), mod(1.5, 2.0, 0.6, 2.0, 0.25), lfo(0.25, 0.2, "sine", "timbre"), chorus(0.3, 0.45, 0.35),
      pitchenv(-12, 1.2, 0.7), voices=10),
    P("Fall Haze", ["Pads", "FX"], "wavetable", "Hazy pad whose notes sink a fifth from above into the chord.",
      wt(table="harmonic", pos=0.35, warp=0.1, tone=0.5, unison=0.5, sub=0.0, reso=0.0, keytrack=0.6, envpos=0.1),
      amp(0.4, 1.5, 0.85, 1.8), mod(0.8, 2.0, 0.5, 1.5, 0.1), chorus(0.35, 0.5, 0.35), pitchenv(7, 0.8, 0.6), voices=10),
    P("Rise Lead", ["Leads"], "analog", "Saw lead that swoops up an octave into every note; a dotted delay behind it.",
      an(shape=0.0, detune=0.12, cutoff=1600, reso=0.25, oscmix=0.5, sub=0.1, drive=0.3, keytrack=0.6),
      amp(0.004, 0.4, 0.85, 0.15), mod(0.001, 0.4, 0.4, 0.2, 0.28), lfo(5.4, 0.04), delay(0.45, 0.3, 0.5, 1, 0.2),
      pitchenv(-12, 0.14, 0.7), MONO),
    P("Scoop Keys", ["Keys"], "fm", "Electric piano where every note scoops up a whole tone: soulful, a little lazy.",
      fm(algo=2, modratio=1.0, index=0.26, op1ratio=1, op2ratio=1, op3ratio=1, op4ratio=14, op5ratio=1, op6ratio=1,
         op1level=1.0, op2level=0.55, op3level=0.7, op4level=0.1, op5level=0.0, op6level=0.0),
      amp(0.002, 2.6, 0.15, 0.45), mod(0.001, 1.1, 0.2, 0.4, 0.75), chorus(0.5, 0.4, 0.35), pitchenv(-2, 0.09, 0.6), voices=14, velsens=0.7),
    P("Rise Bass", ["Bass"], "analog", "Driven bass that slides up a fifth into every note; great for syncopated lines.",
      an(shape=0.1, detune=0.06, cutoff=480, reso=0.18, oscmix=0.3, sub=0.4, drive=0.5, keytrack=0.5),
      amp(0.002, 0.45, 0.6, 0.08), mod(0.001, 0.15, 0.1, 0.1, 0.4), pitchenv(-7, 0.08, 0.6), MONO, velsens=0.5),
    P("Zip Pluck", ["Plucks"], "wavetable", "Bright pluck with a fast upward zip into the note.",
      wt(table="digital", pos=0.3, warp=0.2, tone=0.65, unison=0.2, sub=0.0, reso=0.15, keytrack=0.5, envpos=0.3),
      amp(0.002, 0.35, 0.0, 0.25), mod(0.001, 0.2, 0.0, 0.2, 0.5), pitchenv(-12, 0.05, 0.8), delay(0.375, 0.3, 0.5, 1, 0.18), voices=10),
    P("Fall Lead", ["Leads", "FX"], "analog", "Square lead that drops a fifth from above into every note.",
      an(shape=0.48, detune=0.08, cutoff=1900, reso=0.15, osc2oct=1, oscmix=0.3, sub=0.0, drive=0.25, keytrack=0.6),
      amp(0.004, 0.4, 0.85, 0.2), mod(0.001, 0.4, 0.5, 0.2, 0.2), lfo(5.2, 0.04), pitchenv(7, 0.18, 0.6), MONO),

    # Portamento showcase (0.6: glide modes and timing)
    P("Legato Glide Lead", ["Leads"], "analog", "Mono saw lead with legato portamento: overlapping notes glide, detached notes do not retrigger a slide.",
      an(shape=0.0, detune=0.14, cutoff=1500, reso=0.25, oscmix=0.5, sub=0.15, drive=0.3, keytrack=0.6),
      amp(0.005, 0.4, 0.85, 0.18), mod(0.001, 0.5, 0.5, 0.2, 0.25), lfo(5.3, 0.04), delay(0.45, 0.25, 0.5, 1, 0.16),
      MONO, **porta(0.12, "legato")),
    P("Portamento Square", ["Leads"], "analog", "Hollow square that always glides, at a constant rate: big jumps slide longer than small steps.",
      an(shape=0.5, detune=0.06, cutoff=2100, reso=0.15, osc2oct=1, oscmix=0.25, sub=0.0, drive=0.25, keytrack=0.6),
      amp(0.004, 0.4, 0.85, 0.2), mod(0.001, 0.4, 0.5, 0.2, 0.2), lfo(5.5, 0.035), MONO, **porta(0.09, "always", "rate")),
    P("Glide Sine Bass", ["Bass"], "fm", "Pure sine bass with legato glide for sliding bass lines; detached notes stay clean.",
      fm(algo=2, modratio=1.0, index=0.04, op1ratio=1, op2ratio=1, op1level=1.0, op2level=0.45, op3level=0.0, op4level=0.0,
         op5level=0.0, op6level=0.0),
      amp(0.004, 0.7, 0.9, 0.15), mod(0.001, 0.12, 0.2, 0.1, 0.5), MONO, velsens=0.3, **porta(0.09, "legato")),
    P("Glide Poly Synth", ["Keys", "Leads"], "analog", "Poly synth whose chords always slide from the chord before (poly glide).",
      an(shape=0.15, detune=0.2, cutoff=1800, reso=0.15, oscmix=0.5, sub=0.0, drive=0.2, keytrack=0.6),
      amp(0.02, 0.8, 0.8, 0.4), mod(0.01, 0.6, 0.4, 0.3, 0.25), chorus(0.5, 0.5, 0.35), voices=8, polyglide=1, **porta(0.16, "always")),

    # Boom: deep tuned kicks and sub basses (sine sub with a pitch dive)
    P("Boom Long Sub", ["Boom", "Bass"], "analog", "Long, tuned sine sub with a quick pitch dive on the attack; rings out for seconds.",
      an(shape=0.5, detune=0.0, cutoff=500, reso=0.0, oscmix=0.0, sub=1.0, drive=0.3, keytrack=1.0, osclevel=0.0),
      amp(0.001, 3.2, 0.0, 0.5), mod(0.001, 0.05, 0.0, 0.1, 0.2), pitchenv(12, 0.07, 0.8), MONO, velsens=0.3, tune=12,
      **porta(0.06, "legato")),
    P("Boom Distorted", ["Boom", "Bass"], "analog", "Sine sub driven hard into the ladder filter and the FX slot: a growling, distorted boom.",
      an(shape=0.5, detune=0.0, cutoff=900, reso=0.15, oscmix=0.0, sub=1.0, drive=1.0, keytrack=1.0, osclevel=0.15),
      amp(0.001, 2.4, 0.0, 0.35), mod(0.001, 0.08, 0.0, 0.1, 0.25), pitchenv(12, 0.06, 0.8), drive(2600, 0.1, 0.7, 0, 1.0),
      MONO, velsens=0.3, tune=12),
    P("Boom Kick Bass", ["Boom", "Drums", "Bass"], "analog", "Short boom: a two-octave pitch dive into a tuned sine - kick and bass in one.",
      an(shape=0.5, detune=0.0, cutoff=700, reso=0.0, oscmix=0.0, sub=1.0, drive=0.5, keytrack=1.0, osclevel=0.0),
      amp(0.001, 0.55, 0.0, 0.12), mod(0.001, 0.03, 0.0, 0.05, 0.3), pitchenv(24, 0.045, 0.9), MONO, velsens=0.45, tune=12),
    P("Boom Slide", ["Boom", "Bass"], "analog", "Long sine sub made for trap-style slides: overlap notes and it glides (legato, constant time).",
      an(shape=0.5, detune=0.0, cutoff=600, reso=0.0, oscmix=0.0, sub=1.0, drive=0.45, keytrack=1.0, osclevel=0.0),
      amp(0.001, 3.6, 0.0, 0.4), mod(0.001, 0.05, 0.0, 0.1, 0.2), pitchenv(7, 0.05, 0.8), MONO, velsens=0.25, tune=12,
      **porta(0.13, "legato")),
    P("Boom Click", ["Boom", "Bass"], "analog", "Sine sub with a bright click on the attack, so the boom cuts through on small speakers.",
      an(shape=0.5, detune=0.0, cutoff=260, reso=0.05, oscmix=0.0, sub=1.0, noise=0.35, drive=0.45, keytrack=1.0, osclevel=0.0),
      amp(0.001, 2.2, 0.0, 0.3), mod(0.001, 0.012, 0.0, 0.02, 0.9), pitchenv(12, 0.03, 0.8), MONO, velsens=0.3, tune=12),
]
