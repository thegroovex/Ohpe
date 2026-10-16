# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p15: factory part presets that use LIFE ("pN.life", 0..1): held notes keep evolving (per-voice drift, chaos LFOs,
filter breathing, engine colour, grain scatter). Applied by make_factory.py; level-neutral by design (about
+-0.25 dB breathing), so the calibrated gains stay.
"""
LIFE = {
    "Analog Dawn": 0.30,      # analog: drifting pulse pad
    "Warm Strings": 0.25,     # analog: ensemble-like motion
    "Angel Pad": 0.25,        # vocal: breathing formants
    "Evolving Tide": 0.35,    # wavetable: wandering position
    "Crystal FM Pad": 0.20,   # fm: index breathing
    "Bowed Haze": 0.40,       # grain: extra scatter
    "Warm Horizon": 0.30,     # cluster: drifting stack
    "Dusty EP": 0.15,         # keys: a hint of tape-like wobble
}


def apply(name, params):
    """Adds the life amount of preset `name` to its params (unchanged when it has none)."""
    if name in LIFE and "life" not in params:
        params["life"] = LIFE[name]
    return params
