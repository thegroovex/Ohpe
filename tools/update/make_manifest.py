#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p27: writes the update manifest (ohpe-update.json) for a release - read by the in-app updater.

    make_manifest.py --tag v1.0.0 --repo owner/Ohpe --dir out [--prerelease] [--body-file notes.md]
                     [--content-dir packaging/content] [--date 2026-11-02]

* every file in --dir becomes an asset (platform / kind / part from its name), with size and SHA-256;
* --content-dir: each sub folder with a pack.json ({"id", "version", "kind", "name": {"en", "de"}, "minApp"}) and a
  files/ folder (free packs only; a "license" of type "paid" is skipped) is zipped to <dir>/Ohpe-content-<id>-<version>.zip and listed under "content". The zip has the
  content pack layout (packaging/content/README.md): pack.json with a SHA-256 for every file, the content in the
  kind's folder (kind "presets": presets/*.ohpepreset, kind "wavetables": wavetables/*.wav, frames of 2048 samples) -
  built by tools/pack/make_ohpepack.py, so both tools give the same pack;
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


def license_type(lic):
    if lic is None:
        return "free"
    if isinstance(lic, str):
        return lic.strip().lower() or "free"
    if isinstance(lic, dict):
        t = lic.get("type", "free")
        return t.strip().lower() if isinstance(t, str) and t.strip() else ("free" if t is None else "invalid")
    return "invalid"


def is_free(meta_path):
    try:
        with open(meta_path, encoding="utf-8") as f:
            raw = json.load(f)
    except (OSError, ValueError):
        return True  # not readable here: make_ohpepack reports the error as before
    return not isinstance(raw, dict) or license_type(raw.get("license")) == "free"


def is_content_file(name):
    """A content pack file (built here or not): never a program asset of the release."""
    low = name.lower()
    return low.endswith(".ohpepack") or low.startswith("ohpe-content-")


SAFE_ID = re.compile(r"^[a-z0-9_\-][a-z0-9_\-.]{0,63}$")


def build_packs(content_dir, out_dir):
    packs = []
    if not content_dir or not os.path.isdir(content_dir):
        return packs
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pack"))
    import make_ohpepack
    for sub in sorted(os.listdir(content_dir)):
        folder = os.path.join(content_dir, sub)
        meta_path = os.path.join(folder, "pack.json")
        if not os.path.isfile(meta_path) or not os.path.isdir(os.path.join(folder, "files")):
            continue
        # p49: the public release lists only free Ohpe content; paid packs are never zipped or listed here.
        # p81: decided from the raw pack.json before anything is read or checked, so a paid pack (license as an
        # object or as a plain string, any type but "free") can neither be built nor stop the release build
        if not is_free(meta_path):
            print("skipped (not free): %s" % sub)
            continue
        meta, data = make_ohpepack.pack_meta(folder, "")
        if license_type(meta.get("license")) != "free":
            print("skipped (not free): %s" % sub)
            continue
        pid, ver = meta["id"], meta["version"]
        if not SAFE_ID.match(pid):
            sys.exit("unsafe pack id: %s" % pid)
        zip_name = "Ohpe-content-%s-%s.zip" % (pid, ver)
        make_ohpepack.write_pack(meta, data, os.path.join(out_dir, zip_name))
        packs.append({"id": pid, "version": ver, "kind": meta["kind"],
                      "name": {"en": meta["name"]["en"], "de": meta["name"]["de"]},
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
        if is_content_file(name):
            # p81: a pack file that was not built from a free pack folder above (e.g. a paid pack copied into the
            # release folder) is never listed
            print("not listed (content pack not built from a free pack folder): %s" % name)
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
