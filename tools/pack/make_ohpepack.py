#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Builds and checks Ohpe content packs (.ohpepack) - see packaging/content/README.md.

  make_ohpepack.py build <pack folder> [--out DIR] [--min-app 0.5.0] [--signature BASE64 --key-id ID]
  make_ohpepack.py check <file.ohpepack>
  make_ohpepack.py canonical <pack folder | file.ohpepack>     (prints the bytes a signature covers)

A pack folder holds pack.json ({"id", "version", "kind", "name": {"en", "de"}, "description": {"en", "de"},
"minApp", optional "license": {"type": "free" | "paid", "productId"}}) and the content in files/ (or directly in
the kind folder next to pack.json). Content files at the root of files/ (old flat layout) move into the kind folder;
notes (*.txt, *.md) stay at the root; anything else is left out with a warning.

The .ohpepack is a zip: pack.json first (format 1, "files": SHA-256 of every other file), then the files, sorted,
with fixed time stamps - the same input always gives the same bytes. Output: <out>/<id>-<version>.ohpepack.

Signatures are prepared, not enforced by Ohpe yet: "canonical" prints pack.json without "signature", keys sorted,
no white space (UTF-8) - sign these bytes (Ed25519) with your own tool and pass --signature / --key-id to build.
"""
import argparse
import hashlib
import io
import json
import os
import re
import sys
import zipfile

ID = re.compile(r"^[a-z0-9][a-z0-9\-]{0,63}$")
VERSION = re.compile(r"^\d+\.\d+\.\d+(-[0-9A-Za-z.\-]+)?$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
# the fixed folder of each kind inside a pack, and the content files it holds (keep in step with
# src/core/update/PackFormat.cpp: kindFolder / kindExtensions)
KINDS = {"presets": ("presets", (".ohpepreset",))}
# size limits per kind (bytes, unpacked): whole pack, one file, number of files (PackFormat.cpp limitsFor)
LIMITS = {"presets": (64 << 20, 2 << 20, 5000)}
NOTES = (".txt", ".md")
MAX_NOTE = 256 * 1024
ZIP_TIME = (2026, 1, 1, 0, 0, 0)
DEFAULT_MIN_APP = "0.5.0"


def fail(msg):
    sys.exit("make_ohpepack: " + msg)


def warn(msg):
    print("warning: " + msg, file=sys.stderr)


def safe_entry(path):
    parts = path.split("/")
    return (not path.startswith("/") and ":" not in path and "\\" not in path and len(path) <= 512
            and all(p not in ("", ".", "..") for p in parts))


def canonical(meta):
    """pack.json without "signature", keys sorted, no white space - what a signature covers."""
    m = {k: v for k, v in meta.items() if k != "signature"}
    return json.dumps(m, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def check_meta(meta, where):
    if not isinstance(meta, dict):
        fail("%s: pack.json is not an object" % where)
    pid = meta.get("id")
    if not isinstance(pid, str) or not ID.match(pid):
        fail("%s: pack id %r: only a-z, 0-9 and - (1-64 characters)" % (where, pid))
    if not isinstance(meta.get("version"), str) or not VERSION.match(meta["version"]):
        fail("%s: not a version: %r" % (where, meta.get("version")))
    kind = meta.get("kind", "presets")
    if kind not in KINDS:
        fail("%s: unknown kind %r (known: %s)" % (where, kind, ", ".join(sorted(KINDS))))
    min_app = meta.get("minApp", "")
    if min_app and (not isinstance(min_app, str) or not VERSION.match(min_app)):
        fail("%s: minApp is not a version: %r" % (where, min_app))
    lic = meta.get("license")
    if lic is not None:
        if isinstance(lic, str):
            lic = {"type": lic}
        if not isinstance(lic, dict) or lic.get("type", "free") not in ("free", "paid"):
            fail("%s: license type must be free or paid" % where)
    return kind


def normalise_meta(meta, min_app):
    name = meta.get("name") if isinstance(meta.get("name"), dict) else {"en": meta.get("name") or meta["id"]}
    desc = meta.get("description") if isinstance(meta.get("description"), dict) else {"en": meta.get("description") or ""}
    out = {"format": 1, "id": meta["id"], "version": meta["version"], "kind": meta.get("kind", "presets"),
           "name": {"en": name.get("en") or meta["id"], "de": name.get("de") or name.get("en") or meta["id"]},
           "description": {"en": desc.get("en", ""), "de": desc.get("de") or desc.get("en", "")},
           "minApp": meta.get("minApp") or min_app}
    lic = meta.get("license")
    if isinstance(lic, str):
        lic = {"type": lic}
    if isinstance(lic, dict):
        out["license"] = {"type": lic.get("type", "free"), "productId": lic.get("productId", "")}
    return out


def collect(folder, meta):
    """(path in the pack, file on disk), sorted."""
    kind = meta["kind"]
    kfolder, exts = KINDS[kind]
    root = os.path.join(folder, "files")
    if not os.path.isdir(root):
        root = folder
    out = []
    for base, _, names in os.walk(root):
        for n in names:
            full = os.path.join(base, n)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            low = rel.lower()
            if rel == "pack.json" and root == folder:
                continue
            if not safe_entry(rel):
                fail("unsafe path: %s" % rel)
            if "/" not in rel and low.endswith(exts):
                out.append(("%s/%s" % (kfolder, rel), full))
            elif rel.startswith(kfolder + "/") and low.endswith(exts):
                out.append((rel, full))
            elif "/" not in rel and low.endswith(NOTES) and os.path.getsize(full) <= MAX_NOTE:
                out.append((rel, full))
            else:
                warn("%s is not %s content - left out" % (rel, kind))
    out.sort()
    seen = set()
    for rel, _ in out:
        if rel.lower() in seen:
            fail("%s twice (flat and in %s/)" % (rel, kfolder))
        seen.add(rel.lower())
    content = [r for r, _ in out if r.startswith(kfolder + "/")]
    if not content:
        fail("the pack holds no %s" % kind)
    total, per_file, max_files = LIMITS[kind]
    if len(content) > max_files:
        fail("too many files: %d (at most %d)" % (len(content), max_files))
    size = 0
    for rel, full in out:
        s = os.path.getsize(full)
        if rel in content and s > per_file:
            fail("%s is too big (%d bytes, at most %d)" % (rel, s, per_file))
        size += s
    if size > total:
        fail("the pack is too big (%d bytes, at most %d)" % (size, total))
    return out


def pack_meta(folder, min_app):
    """The pack.json an .ohpepack of `folder` gets (with checksums), and the files."""
    with open(os.path.join(folder, "pack.json"), encoding="utf-8") as f:
        src = json.load(f)
    check_meta(src, os.path.join(folder, "pack.json"))
    meta = normalise_meta(src, min_app)
    data = {}
    for rel, full in collect(folder, meta):
        with open(full, "rb") as f:
            data[rel] = f.read()
    meta["files"] = {rel: hashlib.sha256(b).hexdigest() for rel, b in sorted(data.items())}
    return meta, data


def build(a):
    meta, data = pack_meta(a.folder, a.min_app)
    if a.signature:
        meta["signature"] = {"alg": "ed25519", "keyId": a.key_id or "", "value": a.signature}
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "%s-%s.ohpepack" % (meta["id"], meta["version"]))
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        def add(rel, b):
            info = zipfile.ZipInfo(rel, date_time=ZIP_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, b)
        add("pack.json", (json.dumps(meta, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
        for rel in sorted(data):
            add(rel, data[rel])
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    print("%s: %d files, %d bytes, sha256 %s" % (path, len(data), len(buf.getvalue()), hashlib.sha256(buf.getvalue()).hexdigest()))


def check(a):
    try:
        z = zipfile.ZipFile(a.file)
    except (OSError, zipfile.BadZipFile) as err:
        fail("%s: not a zip (%s)" % (a.file, err))
    names = [i.filename for i in z.infolist() if not i.is_dir()]
    for n in names:
        if not safe_entry(n):
            fail("unsafe path: %s" % n)
    if "pack.json" not in names:
        fail("no pack.json")
    meta = json.loads(z.read("pack.json").decode("utf-8"))
    kind = check_meta(meta, "pack.json")
    sums = meta.get("files")
    if not isinstance(sums, dict) or not sums:
        fail("pack.json has no checksums (\"files\")")
    kfolder, exts = KINDS[kind]
    for n in names:
        if n == "pack.json":
            continue
        low = n.lower()
        is_content = n.startswith(kfolder + "/") and low.endswith(exts)
        is_note = "/" not in n and low.endswith(NOTES)
        if not (is_content or is_note):
            warn("%s is not %s content - Ohpe leaves it out" % (n, kind))
            continue
        if n not in sums:
            fail("%s is not in the checksum list" % n)
        digest = hashlib.sha256(z.read(n)).hexdigest()
        if not SHA256.match(str(sums[n]).lower()) or digest != str(sums[n]).lower():
            fail("%s: checksum differs" % n)
    for n in sums:
        if n not in names:
            fail("%s is listed but not in the pack" % n)
    lic = meta.get("license", {"type": "free"})
    print("ok: %s %s (%s), %d files, license %s, %s" % (meta["id"], meta["version"], kind, len(sums),
                                                      (lic if isinstance(lic, str) else lic.get("type", "free")),
                                                      "signed" if meta.get("signature") else "unsigned"))


def show_canonical(a):
    src = a.source
    if os.path.isdir(src):
        meta, _ = pack_meta(src, a.min_app)
    else:
        meta = json.loads(zipfile.ZipFile(src).read("pack.json").decode("utf-8"))
    sys.stdout.buffer.write(canonical(meta))


def main():
    ap = argparse.ArgumentParser(description="Build and check Ohpe content packs (.ohpepack)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("folder")
    b.add_argument("--out", default="out")
    b.add_argument("--min-app", default=DEFAULT_MIN_APP)
    b.add_argument("--signature", help="base64 Ed25519 signature over the canonical pack.json (not enforced yet)")
    b.add_argument("--key-id")
    c = sub.add_parser("check")
    c.add_argument("file")
    k = sub.add_parser("canonical")
    k.add_argument("source")
    k.add_argument("--min-app", default=DEFAULT_MIN_APP)
    a = ap.parse_args()
    {"build": build, "check": check, "canonical": show_canonical}[a.cmd](a)


if __name__ == "__main__":
    main()
