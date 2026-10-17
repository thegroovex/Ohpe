#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p27: writes the update manifest (ohpe-update.json) for a release - read by the in-app updater.

    make_manifest.py --tag v1.0.0 --repo owner/Ohpe --dir out [--prerelease] [--body-file notes.md]
                     [--content-dir packaging/content] [--date 2026-11-02]

* every file in --dir becomes an asset (platform / kind / part from its name), with size and SHA-256;
* --content-dir: each sub folder with a pack.json ({"id", "version", "kind", "name": {"en", "de"}, "minApp"}) and a
  files/ folder is zipped to <dir>/Ohpe-content-<id>-<version>.zip and listed under "content";
* the release text is split at its "## English" / "## Deutsch" headings into the EN / DE notes.
The download URLs are the release's asset URLs (https://github.com/<repo>/releases/download/<tag>/<name>).
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import sys
import zipfile

MANIFEST = "ohpe-update.json"
SUMS = "SHA256SUMS.txt"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def classify(name):
    low = name.lower()
    if low.endswith(".dmg") or low.endswith(".pkg"):
        return "macos", "installer", "all"
    if "windows" in low or low.endswith(".exe") or low.endswith(".msi"):
        kind = "installer" if low.endswith((".exe", ".msi")) else "archive"
        part = "plugin" if "vst3" in low else ("app" if "-app" in low else "all")
        return "windows", kind, part
    if "linux" in low:
        return "linux", "archive", "all"
    return None


def split_notes(body):
    text = body.replace("\r", "")
    heads = {m.group(1).strip().lower(): m.start() for m in re.finditer(r"^## (?!#)(.*)$", text, re.M)}
    pe, pd = heads.get("english"), heads.get("deutsch")
    if pe is None and pd is None:
        return text.strip(), text.strip()
    first = min(p for p in (pe, pd) if p is not None)
    pre = text[:first].strip()

    def section(start, other):
        eol = text.find("\n", start)
        if eol < 0:
            return ""
        end = other if other is not None and other > start else len(text)
        return text[eol + 1:end].strip()

    def join(s):
        return "\n\n".join(x for x in (pre, s) if x)

    en = join(section(pe, pd)) if pe is not None else None
    de = join(section(pd, pe)) if pd is not None else None
    return en if en is not None else de, de if de is not None else en


SAFE_ID = re.compile(r"^[a-z0-9_\-][a-z0-9_\-.]{0,63}$")


def build_packs(content_dir, out_dir):
    packs = []
    if not content_dir or not os.path.isdir(content_dir):
        return packs
    for sub in sorted(os.listdir(content_dir)):
        meta_path = os.path.join(content_dir, sub, "pack.json")
        files = os.path.join(content_dir, sub, "files")
        if not os.path.isfile(meta_path) or not os.path.isdir(files):
            continue
        with open(meta_path, encoding="utf-8") as f:
            meta = json.load(f)
        pid, ver = meta["id"], meta["version"]
        if not SAFE_ID.match(pid):
            sys.exit("unsafe pack id: %s" % pid)
        zip_name = "Ohpe-content-%s-%s.zip" % (pid, ver)
        zip_path = os.path.join(out_dir, zip_name)
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
            for root, _, names in os.walk(files):
                for n in sorted(names):
                    full = os.path.join(root, n)
                    z.write(full, os.path.relpath(full, files).replace(os.sep, "/"))
        packs.append({"id": pid, "version": ver, "kind": meta.get("kind", "presets"),
                      "name": {"en": meta.get("name", {}).get("en", pid), "de": meta.get("name", {}).get("de", "")},
                      "minApp": meta.get("minApp", ""), "file": zip_name})
    return packs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True)
    ap.add_argument("--repo", required=True)
    ap.add_argument("--dir", required=True)
    ap.add_argument("--prerelease", action="store_true")
    ap.add_argument("--body-file")
    ap.add_argument("--content-dir")
    ap.add_argument("--date", default=datetime.date.today().isoformat())
    a = ap.parse_args()

    version = a.tag[1:] if a.tag.startswith("v") else a.tag
    if not re.match(r"^\d+\.\d+\.\d+(-[0-9A-Za-z.\-]+)?$", version):
        sys.exit("not a version tag: %s" % a.tag)
    base = "https://github.com/%s/releases/download/%s/" % (a.repo, a.tag)
    body = open(a.body_file, encoding="utf-8").read() if a.body_file and os.path.isfile(a.body_file) else ""
    en, de = split_notes(body)

    packs = build_packs(a.content_dir, a.dir)
    pack_files = {p["file"] for p in packs}
    assets = []
    for name in sorted(os.listdir(a.dir)):
        path = os.path.join(a.dir, name)
        if not os.path.isfile(path) or name in (MANIFEST, SUMS) or name in pack_files:
            continue
        c = classify(name)
        if c is None:
            continue
        assets.append({"platform": c[0], "kind": c[1], "part": c[2], "name": name, "url": base + name,
                       "size": os.path.getsize(path), "sha256": sha256(path)})
    content = []
    for p in packs:
        path = os.path.join(a.dir, p["file"])
        content.append({"id": p["id"], "version": p["version"], "kind": p["kind"], "name": p["name"], "minApp": p["minApp"],
                        "url": base + p["file"], "size": os.path.getsize(path), "sha256": sha256(path)})
    manifest = {"format": 1, "app": "Ohpe", "version": version, "tag": a.tag,
                "channel": "preview" if a.prerelease or "-" in version else "stable", "date": a.date,
                "page": "https://github.com/%s/releases/tag/%s" % (a.repo, a.tag),
                "notes": {"en": en, "de": de}, "assets": assets, "content": content}
    with open(os.path.join(a.dir, MANIFEST), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    print("wrote %s: %d assets, %d content packs" % (MANIFEST, len(assets), len(content)))


if __name__ == "__main__":
    main()
