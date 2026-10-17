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
    parts += showcase_extra.morph_presets(parts)
    parts += showcase_extra.usertable_presets()
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
        fn = "%s-%s.ohpepreset" % (p["tags"][0].lower(), slug(p["name"]))
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
    print("%d showcase part presets, %d projects -> %s" % (len(parts), len(projects), OUT))


if __name__ == "__main__":
    main()
