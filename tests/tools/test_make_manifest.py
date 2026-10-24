#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p81: tools/update/make_manifest.py never puts a paid content pack into the public update manifest.

Run from the repository root (CI: scripts/ci/pack-tools-smoke.sh): python3 tests/tools/test_make_manifest.py
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TOOL = os.path.join(ROOT, "tools", "update", "make_manifest.py")


def preset():
    out = subprocess.run(["git", "ls-files", "*.ohpepreset"], cwd=ROOT, capture_output=True, text=True).stdout.split()
    assert out, "no .ohpepreset in the repo"
    return os.path.join(ROOT, out[0])


def pack(content, sub, meta, with_files=True):
    d = os.path.join(content, sub)
    os.makedirs(os.path.join(d, "files") if with_files else d, exist_ok=True)
    if with_files:
        shutil.copy(preset(), os.path.join(d, "files", "One.ohpepreset"))
    with open(os.path.join(d, "pack.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f)


def run(tmp, tag, extra=()):
    rel = os.path.join(tmp, "rel-" + tag)
    os.makedirs(rel)
    for name in ("Ohpe-%s-Linux-x86_64.tar.gz" % tag, "Ohpe-%s-macOS.dmg" % tag, "Ohpe-%s-Windows.zip" % tag):
        with open(os.path.join(rel, name), "wb") as f:
            f.write(name.encode())
    for name in extra:  # stray files that must never be listed
        with open(os.path.join(rel, name), "wb") as f:
            f.write(b"paid content")
    notes = os.path.join(tmp, "notes.md")
    with open(notes, "w", encoding="utf-8") as f:
        f.write("## English\nnotes\n## Deutsch\nHinweise\n")
    p = subprocess.run([sys.executable, TOOL, "--tag", tag, "--repo", "thegroovex/Ohpe", "--dir", rel, "--body-file", notes,
                        "--content-dir", os.path.join(tmp, "content"), "--date", "2026-01-01"] + (["--prerelease"] if "-" in tag else []),
                       capture_output=True, text=True)
    assert p.returncode == 0, p.stdout + p.stderr
    with open(os.path.join(rel, "ohpe-update.json"), encoding="utf-8") as f:
        return rel, json.load(f), p.stdout


def main():
    tmp = tempfile.mkdtemp()
    try:
        content = os.path.join(tmp, "content")
        base = {"version": "1.0.0", "kind": "presets", "name": {"en": "N", "de": "N"}, "minApp": "0.7.0"}
        pack(content, "free-a", dict(base, id="free-a"))
        pack(content, "free-b", dict(base, id="free-b", license={"type": "free"}))
        pack(content, "paid-a", dict(base, id="paid-a", license={"type": "paid", "productId": "p1"}))
        pack(content, "paid-b", dict(base, id="paid-b", license="paid"))
        pack(content, "paid-c", dict(base, id="paid-c", license={"type": "Paid"}))
        pack(content, "paid-d", dict(base, id="paid-d", license={"type": "commercial"}))
        # a broken paid pack (no content) must be skipped, not stop the release build
        pack(content, "paid-e", dict(base, id="paid-e", license={"type": "paid"}), with_files=False)
        os.makedirs(os.path.join(content, "paid-e", "files"))

        rel, m, log = run(tmp, "v1.0.0", extra=("paid-a-1.0.0.ohpepack", "Ohpe-content-paid-z-1.0.0.zip"))
        ids = sorted(c["id"] for c in m["content"])
        assert ids == ["free-a", "free-b"], ids
        names = [a["name"] for a in m["assets"]]
        assert not any("paid" in n for n in names), names
        assert not any(n.endswith(".ohpepack") or n.lower().startswith("ohpe-content-") for n in names), names
        assert sorted(a["platform"] for a in m["assets"]) == ["linux", "macos", "windows"], names
        built = sorted(f for f in os.listdir(rel) if f.startswith("Ohpe-content-") and "paid" not in f)
        assert built == ["Ohpe-content-free-a-1.0.0.zip", "Ohpe-content-free-b-1.0.0.zip"], built
        assert not any(f.startswith("Ohpe-content-paid-") and f != "Ohpe-content-paid-z-1.0.0.zip" for f in os.listdir(rel))
        assert "paid" not in json.dumps(m["content"]), m["content"]
        for c in m["content"]:
            data = open(os.path.join(rel, c["url"].rsplit("/", 1)[1]), "rb").read()
            assert hashlib.sha256(data).hexdigest() == c["sha256"] and len(data) == c["size"], c["id"]
        for a in m["assets"]:
            data = open(os.path.join(rel, a["name"]), "rb").read()
            assert hashlib.sha256(data).hexdigest() == a["sha256"] and len(data) == a["size"], a["name"]
            assert a["url"] == "https://github.com/thegroovex/Ohpe/releases/download/v1.0.0/" + a["name"], a["url"]
        assert m["channel"] == "stable" and m["version"] == "1.0.0" and m["format"] == 1 and m["app"] == "Ohpe"
        assert m["notes"] == {"en": "notes", "de": "Hinweise"}, m["notes"]
        for sub in ("paid-a", "paid-b", "paid-c", "paid-d", "paid-e"):
            assert "skipped (not free): %s" % sub in log, (sub, log)

        _, m2, _ = run(tmp, "v1.0.0-rc.1")
        assert m2["channel"] == "preview" and m2["version"] == "1.0.0-rc.1", m2["channel"]
        assert sorted(c["id"] for c in m2["content"]) == ["free-a", "free-b"]
        print("make_manifest: ok (paid packs never listed)")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
