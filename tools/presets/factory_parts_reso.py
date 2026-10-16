# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p19 (resonator body): factory presets that play a physically modelled body ("reso.*", ResonatorEffect).

The part's resonator stage sits right after the engine: plucks strike plates, noise and breath bow tubes, pads wake
sympathetic strings tuned to the held chord. Every preset maps MPE / aftertouch onto the body (pressure rings it longer
and harder, slide moves the excitation point) and carries the free tag MPE.

RESO_GAINS holds the part gain (dB) per preset from the level calibration (ohpe_render, target about -15 dBFS active
RMS, peaks below -1.5 dBFS). Imported by factory_parts.py (written after the other part presets).
"""
from factory_parts import P, an, pl, vo, cl, amp, mod, lfo, reverb, delay  # noqa: F401

MODELS = {"string": 0, "plate": 1, "open": 2, "closed": 3}
FOLLOW = {"fixed": 0, "note": 1, "chord": 2}


def reso(model="string", follow="note", tune=0.0, size=0.5, material=0.55, decay=2.0, position=0.3, inharm=0.1,
         sympathetic=0.35, width=0.6, drive=0.0, mix=0.5, press=0.5, slide=0.5):
    """The part's resonator stage ("pN.reso.*"), switched on."""
    return {"reso.on": 1, "reso.model": MODELS[model], "reso.follow": FOLLOW[follow], "reso.tune": tune, "reso.size": size,
            "reso.material": material, "reso.decay": decay, "reso.position": position, "reso.inharm": inharm,
            "reso.sympathetic": sympathetic, "reso.width": width, "reso.drive": drive, "reso.mix": mix,
            "reso.pressamt": press, "reso.slideamt": slide}


RESO_GAINS = {  # level calibration (ohpe_render): about -15 dBFS active RMS
    "Plucked Plate": 5.9,
    "Gamelan Plate": 2.5,
    "Bowed Tube": 24.0,
    "Breath Bore": 11.6,
    "Sympathetic Pad": 6.3,
    "Drone Strings": 11.2,
    "Glass Body Keys": 2.3,
}


def R(name, tags, engine, comment, *dicts, **kw):
    p = P(name, tags + ["MPE"], engine, comment, *dicts, **kw)
    p["params"]["gain"] = RESO_GAINS.get(name, 0.0)
    return p


PRESETS_RESO = [
    R("Plucked Plate", ["Plucks"], "pluck", "A bright pluck striking a tuned metal plate: bell-like partials that bloom after the attack.",
      pl(material=0.7, damping=0.55, bright=0.7, body=0.2, excite=0.5, pickpos=0.2, bodysize=0.3),
      amp(0.002, 1.2, 0.0, 0.8), reso("plate", "note", size=0.35, material=0.8, decay=3.2, position=0.25, inharm=0.5,
                                       sympathetic=0.0, width=0.8, mix=0.6, drive=0.15),
      reverb(0.55, 0.4, 0.02, 0.18), voices=8, velsens=0.8),
    R("Gamelan Plate", ["Plucks"], "pluck", "Short mallet hits on a long bronze plate, stretched partials and a shimmering tail.",
      pl(material=0.85, damping=0.85, bright=0.55, body=0.1, excite=0.8, pickpos=0.1, bodysize=0.2),
      amp(0.002, 0.4, 0.0, 0.3), reso("plate", "note", size=0.8, material=0.9, decay=6.0, position=0.6, inharm=0.75,
                                       sympathetic=0.2, width=0.9, mix=0.85, drive=0.35),
      delay(0.375, 0.25, 0.45, 1, 0.14), voices=8, velsens=0.9),
    R("Bowed Tube", ["Leads"], "analog", "Breathy noise bowing a closed bore: a hollow, clarinet-like tone that swells with pressure.",
      an(shape=0.15, detune=0.0, cutoff=4200, reso=0.1, oscmix=0.0, sub=0.0, noise=0.45, drive=0.15, keytrack=0.8, osclevel=0.7),
      amp(0.12, 0.4, 0.85, 0.5), mod(0.2, 0.6, 0.4, 0.4, 0.15),
      reso("closed", "note", size=0.4, material=0.45, decay=1.6, position=0.15, inharm=0.05, sympathetic=0.0, width=0.4,
           mix=0.9, drive=0.25, press=0.8, slide=0.6),
      lfo(5.2, 0.04, "sine", "pitch"), reverb(0.5, 0.5, 0.02, 0.15), voices=1, glide=0.06),
    R("Breath Bore", ["Textures"], "vocal", "A whispering voice blown through an open pipe: airy flute tones with a soft edge.",
      vo(vowel=0.3, formant=0.0, breath=0.9, tension=0.3, vibdepth=0.1, vibrate=4.5, jitter=0.2),
      amp(0.25, 0.5, 0.8, 1.2), reso("open", "note", size=0.6, material=0.5, decay=1.2, position=0.35, inharm=0.0,
                                      sympathetic=0.15, width=0.7, mix=0.8, drive=0.0, press=0.7, slide=0.7),
      reverb(0.7, 0.45, 0.03, 0.25), voices=6),
    R("Sympathetic Pad", ["Pads"], "analog", "A warm pad that wakes a set of sympathetic strings tuned to the chord you hold - they keep ringing after release.",
      an(shape=0.6, detune=0.25, cutoff=1800, reso=0.1, oscmix=0.5, sub=0.2, noise=0.05, drive=0.1, keytrack=0.5, osclevel=0.0),
      amp(0.8, 1.0, 0.8, 2.5), mod(1.0, 2.0, 0.5, 2.0, 0.1),
      reso("string", "chord", size=1.0, material=0.6, decay=6.0, position=0.35, inharm=0.15, sympathetic=0.9, width=0.9,
           mix=0.5, press=0.6, slide=0.5),
      reverb(0.75, 0.45, 0.03, 0.22), voices=10, life=0.2),
    R("Drone Strings", ["Plucks"], "pluck", "Plucked steel over sympathetic strings tuned to the chord: buzzing, droning, endlessly resonant.",
      pl(material=0.6, damping=0.4, bright=0.75, body=0.4, excite=0.6, pickpos=0.12, bodysize=0.5),
      amp(0.002, 2.5, 0.0, 1.5), reso("string", "chord", size=0.9, material=0.7, decay=4.5, position=0.08, inharm=0.3,
                                       sympathetic=1.0, width=0.8, mix=0.6, drive=0.45),
      reverb(0.6, 0.5, 0.02, 0.15), voices=8),
    R("Glass Body Keys", ["Keys"], "cluster", "Soft keys through a glass plate tuned to each note: a glassy halo over the chord.",
      cl(detune=0.05, spread=0.3, tone=0.4, chord="unison", stack=1, shape=0.2, side=0.2, reso=0.1, keytrack=0.6, drift=0.1),
      amp(0.004, 1.5, 0.4, 1.2), reso("plate", "note", size=0.5, material=1.0, decay=2.5, position=0.45, inharm=0.4,
                                       sympathetic=0.3, width=0.9, mix=0.45, drive=0.0),
      reverb(0.6, 0.35, 0.02, 0.2), voices=10),
]

DE_RESO = {
    "Plucked Plate": "Ein heller Zupfer schlägt eine gestimmte Metallplatte an: glockenartige Teiltöne, die nach dem Anschlag aufblühen.",
    "Gamelan Plate": "Kurze Schlegelschläge auf eine lange Bronzeplatte, gespreizte Teiltöne und ein schimmernder Nachklang.",
    "Bowed Tube": "Hauchiges Rauschen streicht ein gedacktes Rohr: ein hohler, klarinettenartiger Ton, der mit dem Druck anschwillt.",
    "Breath Bore": "Eine flüsternde Stimme, durch ein offenes Rohr geblasen: luftige Flötentöne mit weicher Kante.",
    "Sympathetic Pad": "Eine warme Fläche weckt Resonanzsaiten, gestimmt auf den gehaltenen Akkord – sie klingen nach dem Loslassen weiter.",
    "Drone Strings": "Gezupfter Stahl über Resonanzsaiten im Akkord: schnarrend, dröhnend, endlos nachklingend.",
    "Glass Body Keys": "Weiche Tasten durch eine Glasplatte, auf jede Note gestimmt: ein gläserner Schimmer über dem Akkord.",
}
