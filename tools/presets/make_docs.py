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

CATS = ["Bass", "Keys", "Pads", "Leads", "Plucks", "Drums", "FX", "Arp", "Textures", "Vocal"]
CAT_DE = {"Bass": "Bass", "Keys": "Tasten", "Pads": "Flächen", "Leads": "Leads", "Plucks": "Zupfer", "Drums": "Drums",
          "FX": "FX", "Arp": "Arp", "Textures": "Texturen", "Vocal": "Vokal"}
ENG = {"fm": ("FM", "FM"), "wavetable": ("Wavetable", "Wavetable"), "analog": ("Analog", "Analog"), "pluck": ("Pluck", "Zupfen"),
       "grain": ("Grain", "Grain"), "vocal": ("Vocal", "Vocal"), "cluster": ("Cluster", "Cluster"), "sampler": ("Sampler", "Sampler")}


def table(lang):
    de = lang == "de"
    out = []
    parts = factory_parts.PRESETS
    for cat in CATS:
        rows = [p for p in parts if p["tags"][0] == cat]
        out.append("### %s (%d)\n" % (CAT_DE[cat] if de else cat, len(rows)))
        out.append("| Name | Engine | %s | %s |" % ("Weitere Kategorien" if de else "Also in", "Beschreibung" if de else "Description"))
        out.append("|---|---|---|---|")
        for p in sorted(rows, key=lambda p: p["name"]):
            also = ", ".join(CAT_DE[t] if de else t for t in p["tags"][1:]) or "–"
            text = DE[p["name"]] if de else p["comment"]
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
