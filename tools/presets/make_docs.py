#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Writes the preset lists in docs/en/FACTORY-SOUNDS.md and docs/de/FACTORY-SOUNDS.md between the
markers <!-- presets:begin --> and <!-- presets:end --> from factory_parts.py / factory_projects.py."""
import os, re, sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import factory_parts, factory_projects  # noqa: E402
from comments_de import DE  # noqa: E402

CATS = ["Bass", "Keys", "Pads", "Leads", "Plucks", "Drums", "FX", "Arp", "Textures", "Vocal", "Boom", "Hybrid", "Piano"]
GATHER = ("Boom", "Hybrid", "Piano")  # these sections list every preset carrying the tag, wherever its main category is
CAT_DE = {"Bass": "Bass", "Keys": "Tasten", "Pads": "Flächen", "Leads": "Leads", "Plucks": "Zupfer", "Drums": "Drums",
          "FX": "FX", "Arp": "Arp", "Textures": "Texturen", "Vocal": "Vokal", "Boom": "Boom", "Hybrid": "Hybrid",
          "Piano": "Klavier"}
ENG = {"fm": ("FM", "FM"), "wavetable": ("Wavetable", "Wavetable"), "analog": ("Analog", "Analog"), "pluck": ("Pluck", "Zupfen"),
       "grain": ("Grain", "Grain"), "vocal": ("Vocal", "Vocal"), "cluster": ("Cluster", "Cluster"), "sampler": ("Sampler", "Sampler")}


ARP_MODES = (("Up", "Aufwärts"), ("Down", "Abwärts"), ("Up/Down", "Auf/Ab"), ("Random", "Zufall"), ("As Played", "Wie gespielt"),
             ("Chord", "Akkord"))
ARP_RATES = ["1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/32T", "1/4.", "1/8.", "1/16."]
ARP_NOTE = {
    "en": "Presets marked *Arp:* switch the arpeggiator on for the part they load into (latched: press a key once, "
          "it keeps playing; press a new key or chord to change it). Loading a preset without arp into that part "
          "switches it off again. See [PRESETS.md](PRESETS.md#arp-block).\n",
    "de": "Presets mit *Arp:* schalten den Arpeggiator für den Part ein, in den sie geladen werden (gehalten: eine Taste "
          "einmal drücken, er spielt weiter; eine neue Taste oder ein neuer Akkord wechselt). Ein Preset ohne Arp in "
          "diesem Part schaltet ihn wieder aus. Siehe [PRESETS.md](PRESETS.md#arp-block).\n",
}


def arp_text(a, de):
    i = 1 if de else 0
    parts = [ARP_MODES[a["mode"]][i], ARP_RATES[a["rate"]], "%d %s" % (a["oct"], "Okt." if de else "oct")]
    if a.get("latch"):
        parts.append("Halten" if de else "latch")
    return " *Arp: %s*" % " · ".join(parts)


def table(lang):
    de = lang == "de"
    out = []
    parts = factory_parts.PRESETS
    for cat in CATS:
        rows = [p for p in parts if p["tags"][0] == cat]
        if cat in GATHER:  # 0.6 Boom, 0.8 Hybrid / Piano: every preset with the tag, wherever its main category is
            rows = [p for p in parts if cat in p["tags"]]
        out.append("### %s (%d)\n" % (CAT_DE[cat] if de else cat, len(rows)))
        if cat == "Arp":
            out.append(ARP_NOTE[lang])
        out.append("| Name | Engine | %s | %s |" % ("Weitere Kategorien" if de else "Also in", "Beschreibung" if de else "Description"))
        out.append("|---|---|---|---|")
        for p in sorted(rows, key=lambda p: p["name"]):
            also = ", ".join(CAT_DE.get(t, t) if de else t for t in p["tags"] if t != cat) or "–"
            text = DE[p["name"]] if de else p["comment"]
            if p.get("arp"):
                text += arp_text(p["arp"], de)
            out.append("| %s | %s | %s | %s |" % (p["name"], ENG[p["engine"]][1 if de else 0], also, text))
        out.append("")
    out.append("### %s (%d)\n" % ("Projekte / Demo-Patterns" if de else "Projects / demo patterns", len(factory_projects.PROJECTS)))
    out.append("| Name | %s | %s |" % ("Kategorien" if de else "Categories", "Beschreibung" if de else "Description"))
    out.append("|---|---|---|")
    for p in factory_projects.PROJECTS:
        out.append("| %s | %s | %s |" % (p["name"], ", ".join(CAT_DE[t] if de else t for t in p["tags"]), DE[p["name"]] if de else p["comment"]))
    out.append("")
    eng = Counter(p["engine"] for p in parts)
    out.append(("**Engines:** " if not de else "**Engines:** ") + ", ".join("%s %d" % (ENG[e][1 if de else 0], eng[e]) for e in ENG))
    return "\n".join(out)


def main():
    for lang in ("en", "de"):
        path = os.path.join(ROOT, "docs", lang, "FACTORY-SOUNDS.md")
        text = open(path, encoding="utf-8").read()
        new = re.sub(r"<!-- presets:begin -->.*?<!-- presets:end -->",
                     lambda m: "<!-- presets:begin -->\n" + table(lang) + "\n<!-- presets:end -->", text, flags=re.S)
        open(path, "w", encoding="utf-8").write(new)
        print("updated", path)


if __name__ == "__main__":
    main()
