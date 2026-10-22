# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p77: 3D presets whose motion was convincing but too slow for normal playing - motion retrigger (space.retrig:
0 Free, 1 First, 2 Note) and a shorter motion length (space.pathlen index). Used by make_binaural.py (its presets) and
recorded in bank_curation.json "changed" (every preset, so "bank_curate.py apply" restores it after a generator run).
file: (retrig, pathlen or None, (EN old, EN new), (DE old, DE new))"""

P77 = {
    "leads-comet-lead.ohpepreset": (2, 3, ("streaks past you every two bars", "streaks past you with every note it plays"),
                                    ("der alle zwei Takte an dir vorbeizieht", "der mit jeder Note an dir vorbeizieht")),
    "leads-ear-to-ear-lead.ohpepreset": (2, 4, ("hopping from one ear to the other every bar",
                                                "hopping from one ear to the other every half bar, starting again with every new note"),
                                         ("der jeden Takt von einem Ohr zum anderen springt",
                                          "der jeden halben Takt von einem Ohr zum anderen springt, mit jeder neuen Note von vorn")),
    "leads-pendulum-lead.ohpepreset": (1, 4, ("once every two bars (motion path Pendulum, in tempo)",
                                              "once per bar from the first note of each phrase (motion path Pendulum, in tempo)"),
                                       ("alle zwei Takte von einer Seite des Kopfes zur anderen schwingt",
                                        "jeden Takt ab der ersten Note einer Phrase von einer Seite des Kopfes zur anderen schwingt")),
    "leads-vowel-orbit-lead.ohpepreset": (1, 5, ("circling you every four bars", "circling you every two bars from the first note of each phrase"),
                                          ("der alle vier Takte um dich kreist", "der ab der ersten Note einer Phrase alle zwei Takte um dich kreist")),
    "leads-overhead-flute.ohpepreset": (1, 5, ("down behind you every four bars", "down behind you every two bars from the first note of each phrase"),
                                        ("die alle vier Takte über den Kopf", "die ab der ersten Note einer Phrase alle zwei Takte über den Kopf")),
    "keys-orbiting-tines.ohpepreset": (2, 4, ("in front of you every two bars", "in front of you once per bar, starting again with every note"),
                                       ("das alle zwei Takte in einem kleinen Kreis vor dir schwebt",
                                        "das jeden Takt in einem kleinen Kreis vor dir schwebt, mit jeder Note von vorn")),
    "keys-glass-pendulum.ohpepreset": (1, 4, ("on a slow pendulum, each phrase landing somewhere new", "on a pendulum that starts again with each phrase"),
                                       ("auf einem langsamen Pendel von einem Ohr zum anderen schwingen; jede Phrase landet woanders",
                                        "auf einem Pendel von einem Ohr zum anderen schwingen, das mit jeder Phrase neu beginnt")),
    "keys-ceiling-harpsichord.ohpepreset": (1, 5, ("settles behind you every four bars", "settles behind you every two bars from the first note of each phrase"),
                                            ("das alle vier Takte über den Kopf steigt", "das ab der ersten Note einer Phrase alle zwei Takte über den Kopf steigt")),
    "keys-vibes-in-the-round.ohpepreset": (1, None, None, None),
    "plucks-ping-pong-space-pluck.ohpepreset": (2, None, ("bounces front to back on every half bar", "bounces front to back every half bar, starting again with every note"),
                                                ("der jeden halben Takt von vorn nach hinten springt",
                                                 "der jeden halben Takt von vorn nach hinten springt, mit jeder Note von vorn")),
    "fx-doppler-fly-by.ohpepreset": (2, 4, ("racing past from left to right and back", "racing past from left to right and back with every note"),
                                     ("rast von links nach rechts und zurück an dir vorbei", "rast mit jeder Note von links nach rechts und zurück an dir vorbei")),
    "vocal-breath-circle.ohpepreset": (1, 4, ("circling your head every two bars", "circling your head every bar from the first breath of each phrase"),
                                       ("die alle zwei Takte in Flüsterabstand um den Kopf kreisen",
                                        "die ab dem ersten Atemzug einer Phrase jeden Takt in Flüsterabstand um den Kopf kreisen")),
}


def apply(p):
    """Applies the p77 change to a make_binaural preset record (params, EN comment, DE text) - in place."""
    ch = P77.get(p.get("file"))
    if ch is None:
        return
    retrig, length, en, de = ch
    p["params"]["space.retrig"] = retrig
    if length is not None:
        p["params"]["space.pathlen"] = length
    if en:
        p["comment"] = p["comment"].replace(en[0], en[1])
    if de:
        p["de"] = p["de"].replace(de[0], de[1])
