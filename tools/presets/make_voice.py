#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p68 (Voice Control): three factory part presets that listen to the live input (matrix sources Voice Level 34,
Vowel X 35, Vowel Y 36, Voice Brightness 37, Voice Noisiness 38, Voice Pitch 39, Voice Onset 40).

Each one stands on its own without an input: every voice routing adds to a complete sound, and with no input the
sources rest at 0 (the vowel axes at the map's centre), so the preset plays exactly as programmed. Levels are set by
tools/presets/bank_curate.py (analyse / calibrate / apply, target in bank_curation.json) - render them with
--strip-voice so an older ohpe_render (which does not know the voice sources yet) hears the no-input sound.

  python3 tools/presets/make_voice.py               # writes the presets (keeps a calibrated "gain")
  python3 tools/presets/make_voice.py --strip-voice # the same without voice routings (level calibration only)
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
BANK = os.path.join(HERE, "..", "..", "resources", "presets", "factory")

VOICE_LEVEL, VOWEL_X, VOWEL_Y, VOICE_BRIGHT, VOICE_NOISE, VOICE_PITCH, VOICE_ONSET = range(34, 41)
# targets (mod::destTable): Timbre 4, Level 2, Pan 3, Bright 85-?, looked up by key below
DST = {"pitch": 1, "level": 2, "pan": 3, "timbre": 4, "life": 72, "morphx": 73, "morphy": 74}
DST_CHAR = {"charbright": 81, "charhard": 82, "charalive": 83, "charwide": 84}


def route(params, slot, src, dst, amt):
    s = "mod.s%d." % slot
    params.update({s + "src": src, s + "via": 0, s + "dst": dst, s + "amt": amt, s + "curve": 0})


def base(name):
    return json.load(open(os.path.join(BANK, name), encoding="utf-8"))


def presets():
    out = {}

    # 1. a vowel lead whose vowel follows the singer's vowel (the LFO that walked the vowels is gone)
    p = base("vocal-talking-synth.ohpepreset")
    q = dict(p["params"])
    q.update({"vocal.vowel": 2.0, "vocal.formant": 0.15, "vocal.jitter": 0.2, "vocal.breath": 0.18, "vocal.tension": 0.45, "vocal.vibdepth": 0.16, "vocal.vibrate": 5.0,
              "lfo.depth": 0.0, "glide": 0.08, "amp.a": 0.02, "amp.r": 0.35, "fx2.chorus.mix": 0.4, "fx2.chorus.depth": 0.6})
    route(q, 2, VOWEL_X, DST["timbre"], 0.6)        # back .. front vowel moves the synth's vowel
    route(q, 3, VOICE_BRIGHT, DST_CHAR["charbright"], 0.3)
    route(q, 4, VOICE_ONSET, DST["level"], 0.15)     # a syllable leans into the note
    out["vocal-voice-vowel-lead.ohpepreset"] = dict(p, name="Voice Vowel Lead", tags=["Vocal", "Leads", "Voice"], params=q,
        comment="Sing into the input: your vowel moves its vowel, your brightness opens it. Without input a clean vowel lead.")

    # 2. a pad whose filter opens with open vowels and breathes with the voice's level
    p = base("pads-analog-dawn.ohpepreset")
    q = dict(p["params"])
    q.update({"analog.shape": 0.15, "analog.cutoff": 1100, "analog.reso": 0.3, "analog.detune": 0.32, "analog.oscmix": 0.25,
              "analog.sub": 0.25, "analog.drive": 0.3, "mod.amt": 0.15, "mod.a": 1.0, "fx.phaser.mix": 0.4, "fx.phaser.rate": 0.2,
              "fx.phaser.depth": 0.8, "fx.phaser.feedback": 0.5, "amp.a": 0.3, "amp.r": 1.4, "life": 0.45})
    route(q, 2, VOWEL_Y, DST["timbre"], 0.5)          # "aah" opens, "ooh" / "ee" close
    route(q, 3, VOICE_LEVEL, DST_CHAR["charalive"], 0.5)
    route(q, 4, VOWEL_X, DST["pan"], 0.25)
    out["pads-voice-filter-pad.ohpepreset"] = dict(p, name="Voice Filter Pad", tags=["Pads", "Voice"], params=q,
        comment="Talk or hum to it: open vowels open the filter, your level brings it alive. Without input a warm pulse pad.")

    # 3. a bass with a voice-controlled wah: the vowel sweeps the table, consonants bite
    p = base("bass-talking-bass.ohpepreset")
    q = dict(p["params"])
    q.update({"wavetable.envpos": 0.0, "wavetable.pos": 0.35, "wavetable.tone": 0.5, "wavetable.reso": 0.25, "wavetable.warp": 0.3,
              "wavetable.unison": 0.2, "wavetable.sub": 0.7, "wavetable.keytrack": 0.2, "mod.amt": 0.1, "amp.d": 0.6, "amp.s": 0.85,
              "glide": 0.06})
    route(q, 2, VOWEL_Y, DST["timbre"], 0.55)
    route(q, 3, VOICE_NOISE, DST_CHAR["charhard"], 0.45)
    route(q, 4, VOICE_ONSET, DST["timbre"], 0.2)
    out["bass-voice-wah-bass.ohpepreset"] = dict(p, name="Voice Wah Bass", tags=["Bass", "Voice"], params=q,
        comment="A wah you play with your mouth: vowels sweep the formant table, 's' and 't' bite. Without input a round talking bass.")
    return out


def main():
    strip = "--strip-voice" in sys.argv
    for fname, p in presets().items():
        path = os.path.join(BANK, fname)
        if os.path.exists(path):  # keep the calibrated level
            old = json.load(open(path, encoding="utf-8"))
            p["params"]["gain"] = old["params"].get("gain", p["params"]["gain"])
        if strip:
            for k in [k for k in p["params"] if k.startswith("mod.s") and k.endswith(".src") and p["params"][k] >= VOICE_LEVEL]:
                pre = k[: -len("src")]
                for f in ("src", "via", "dst", "amt", "curve"):
                    p["params"].pop(pre + f, None)
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(p, fh, indent=1, ensure_ascii=False)
            fh.write("\n")
        print("wrote", fname)


if __name__ == "__main__":
    main()
