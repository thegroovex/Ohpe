#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p44: writes the showcase factory presets (factory_parts_showcase.py, showcase_morph / showcase_projects below) into
resources/presets/factory. It only writes (and, on a re-run, replaces) its own files - listed in showcase_files.json -
and never touches the rest of the bank, so the passes that edited the bank in place (mod wheel, expression, punch)
stay as they are. Run it after editing the showcase sources; showcase_cal.py calibrates the part gains.

  python3 tools/presets/make_showcase.py
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "resources", "presets", "factory")
MANIFEST = os.path.join(HERE, "showcase_files.json")
sys.path.insert(0, HERE)


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def part_doc(p):
    d = {"format": "ohpe-preset", "version": 2, "kind": "part", "name": p["name"], "author": "groovex", "tags": p["tags"],
         "engine": p["engine"], "comment": p["comment"], "params": p["params"]}
    if p.get("arp"):
        d["arp"] = p["arp"]
    if p.get("morph"):
        d["morph"] = p["morph"]
    if p.get("usertables"):
        d["usertables"] = p["usertables"]
    return d


def project_doc(p):
    return {"format": "ohpe-preset", "version": 2, "kind": "project", "name": p["name"], "author": "groovex",
            "tags": p["tags"], "engine": p["engine"], "comment": p["comment"], "params": p["params"],
            "sections": p["sections"]}


def all_presets():
    import factory_parts_showcase as sc
    parts = list(sc.PRESETS_SHOWCASE)
    import showcase_extra
    parts += showcase_extra.usertable_presets()
    showcase_extra.REG.update({p["name"]: p for p in parts})
    parts += showcase_extra.morph_presets(parts)
    projects = showcase_extra.projects()
    return parts, projects


def existing_names(skip):
    names = set()
    for f in os.listdir(OUT):
        if f.endswith(".ohpepreset") and f not in skip:
            names.add(json.load(open(os.path.join(OUT, f), encoding="utf-8"))["name"])
    return names


def write(path, doc):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write("\n")


FEATURE = {"life": ("Life", "Leben"), "morph": ("Morph pad", "Morph-Pad"), "usertable": ("User wavetable", "Eigene Wavetable"),
           "reso": ("Resonator", "Resonanzkörper"), "char": ("Character macros", "Charakter-Makros"),
           "space": ("3D motion path", "3D-Bewegungspfad"), "sub": ("SUB layer", "SUB-Ebene"),
           "expr": ("Expression", "Ausdruck"), "glidepress": ("Glide pressure", "Glide-Druck"),
           "tempo": ("Tempo-synced", "Tempo-synchron"), "harmony": ("Harmony follow", "Harmonie-Folgen"),
           "motion": ("Motion lanes", "Bewegungsspuren"), "pads": ("Performance pads", "Performance-Pads")}
CAT_DE = {"Bass": "Bass", "Keys": "Tasten", "Pads": "Flächen", "Leads": "Leads", "Plucks": "Zupfer", "Drums": "Drums",
          "FX": "FX", "Arp": "Arp", "Textures": "Texturen", "Vocal": "Vokal", "Boom": "Boom", "Hybrid": "Hybrid",
          "Piano": "Klavier", "3D": "3D", "Morph": "Morph", "MPE": "MPE"}
ENG = {"fm": "FM", "wavetable": "Wavetable", "analog": "Analog", "pluck": ("Pluck", "Zupfen"), "grain": "Grain",
       "vocal": "Vocal", "cluster": "Cluster", "sampler": "Sampler"}
ORDER = ["Bass", "Keys", "Pads", "Leads", "Plucks", "Arp", "Textures", "FX", "Vocal"]
INTRO = {
    "en": "## Showcase presets (p44)\n\n"
          "%d part presets and %d projects built around the newer features: Life, the morph pad, user wavetables analysed "
          "from the factory recordings, the resonator body (stage and rack effect), the character macros, the 3D motion "
          "paths (best with headphones), the SUB layer, the Expression page (velocity / aftertouch), pressure glide and "
          "tempo-synced movement (LFO 2 / 3 and motion paths measured in bars follow the tempo, whether the metronome clicks "
          "or not). The projects show what only a project carries: the harmony brain's Follow, recorded motion lanes, the "
          "performance pads' settings and four 3D parts at once. Every one is level-matched to the factory median, has a "
          "musical mod wheel and answers velocity; only the glide presets glide. They are listed in the library with "
          "the other factory presets (search for a feature, e.g. *3D* or *Morph*).\n",
    "de": "## Showcase-Presets (p44)\n\n"
          "%d Part-Presets und %d Projekte rund um die neueren Funktionen: Leben, das Morph-Pad, eigene Wavetables, analysiert "
          "aus den Werksaufnahmen, der Resonanzkörper (Stufe und Rack-Effekt), die Charakter-Makros, die 3D-Bewegungspfade "
          "(am besten mit Kopfhörern), die SUB-Ebene, die Ausdrucks-Seite (Anschlag / Aftertouch), Glide-Druck und "
          "tempo-synchrone Bewegung (LFO 2 / 3 und in Takten bemessene Bewegungspfade folgen dem Tempo, ob das Metronom "
          "klickt oder nicht). Die Projekte zeigen, was nur ein Projekt trägt: das Folgen des Harmonie-Gehirns, "
          "aufgenommene Bewegungsspuren, die Einstellungen der Performance-Pads und vier 3D-Parts gleichzeitig. Jedes ist "
          "auf den Werks-Median eingepegelt, hat ein musikalisches Modrad und reagiert auf Anschlag; nur die Glide-Presets "
          "gleiten. Sie stehen in der Bibliothek bei den anderen Werks-Presets (Suche nach einer Funktion, z. B. *3D* "
          "oder *Morph*).\n",
}


def docs(parts, projects):
    """The showcase section of docs/en|de/FACTORY-SOUNDS.md (between <!-- showcase:begin / end -->, before the drum kits)."""
    for lang, head in (("en", "## Drum kits"), ("de", "## Drum-Kits")):
        de = lang == "de"
        rows = [INTRO[lang] % (len(parts), len(projects)), "<!-- showcase:begin -->"]
        rows.append("| Name | %s | Engine | %s | %s |" % ("Kategorie" if de else "Category", "Funktionen" if de else "Features",
                                                      "Beschreibung" if de else "Description"))
        rows.append("|---|---|---|---|---|")
        key = lambda p: (ORDER.index(p["tags"][0]) if p["tags"][0] in ORDER else 99, p["name"])
        for p in sorted(parts, key=key):
            eng = ENG[p["engine"]]
            eng = eng[1 if de else 0] if isinstance(eng, tuple) else eng
            tags = ", ".join(CAT_DE.get(t, t) if de else t for t in p["tags"] if t != "MPE")
            feats = ", ".join(FEATURE[f][1 if de else 0] for f in p["features"])
            rows.append("| %s | %s | %s | %s | %s |" % (p["name"], tags, eng, feats, p["de"] if de else p["comment"]))
        for p in projects:
            tags = ", ".join(CAT_DE.get(t, t) if de else t for t in p["tags"])
            feats = ", ".join(FEATURE[f][1 if de else 0] for f in p["features"])
            rows.append("| %s | %s | %s | %s | %s |" % (p["name"], tags, "Projekt" if de else "Project", feats,
                                                       p["de"] if de else p["comment"]))
        rows.append("<!-- showcase:end -->\n")
        block = "\n".join(rows)
        path = os.path.join(ROOT, "docs", lang, "FACTORY-SOUNDS.md")
        text = open(path, encoding="utf-8").read()
        if "<!-- showcase:begin -->" in text:
            start = text.index(INTRO[lang][:20])
            end = text.index("<!-- showcase:end -->") + len("<!-- showcase:end -->\n")
            text = text[:start] + block + text[end:]
        else:
            i = text.index(head)
            text = text[:i] + block + "\n" + text[i:]
        open(path, "w", encoding="utf-8").write(text)


def main():
    old = json.load(open(MANIFEST)) if os.path.exists(MANIFEST) else []
    for f in old:
        if os.path.exists(os.path.join(OUT, f)):
            os.remove(os.path.join(OUT, f))
    taken = existing_names(set(old))
    first = min(taken, key=lambda n: n.casefold())  # the start sound (first factory preset by name) must stay first
    parts, projects = all_presets()
    files = []
    for p in parts:
        assert p["name"] not in taken, "name already in the bank: " + p["name"]
        assert p["name"].casefold() > first.casefold(), p["name"] + " would sort before the start sound " + first
        taken.add(p["name"])
        prefix = p.get("file_prefix", p["tags"][0].lower())
        fn = "%s-%s.ohpepreset" % (prefix, re.sub("^" + prefix + "-", "", slug(p["name"])))
        assert fn not in files and not os.path.exists(os.path.join(OUT, fn)), fn
        write(os.path.join(OUT, fn), part_doc(p))
        files.append(fn)
    for p in projects:
        assert p["name"] not in taken, "name already in the bank: " + p["name"]
        assert p["name"].casefold() > first.casefold(), p["name"]
        taken.add(p["name"])
        fn = "project-%s.ohpepreset" % slug(p["name"])
        assert not os.path.exists(os.path.join(OUT, fn)), fn
        write(os.path.join(OUT, fn), project_doc(p))
        files.append(fn)
    json.dump(sorted(files), open(MANIFEST, "w"), indent=1)
    docs(parts, projects)
    print("%d showcase part presets, %d projects -> %s" % (len(parts), len(projects), OUT))


if __name__ == "__main__":
    main()
