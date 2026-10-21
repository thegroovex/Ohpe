#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p62 (1.0 format freeze): nothing about Ohpe's file formats changes silently. No build needed.

  check-formats.py [--base <git ref>]

Checks
  1. tests/formats/FORMATS.lock lists every constant of src/core/model/FormatVersions.h ("version <key> <n>", same
     order, same value) - a version constant changed without the lock fails.
  2. Every file below tests/formats/fixtures is listed in the lock with its SHA-256, and every listed fixture exists -
     a fixture changed, added or deleted without the lock fails.
  3. docs/FORMATS.md and docs/de/FORMATE.md name every format key with its current version (a table row
     "| `key` | n |").
  4. The golden .ohpepack fixtures pass tools/pack/make_ohpepack.py check.
  5. With --base (the pull request's base commit), against the lock there:
       * no version goes down, no version key disappears, keys are only appended;
       * no fixture line is removed or changed (older files must always load - fixtures are frozen);
       * a version that goes up comes with at least one new fixture;
       * when the lock changes at all, both format documents change in the same pull request.
"""
import argparse
import hashlib
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LOCK = "tests/formats/FORMATS.lock"
FIXTURES = "tests/formats/fixtures"
HEADER = "src/core/model/FormatVersions.h"
DOCS = ["docs/FORMATS.md", "docs/de/FORMATE.md"]

errors = []


def fail(msg):
    errors.append(msg)
    print("ERROR: " + msg)


def parse_lock(text):
    versions, fixtures = [], {}
    for n, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if parts[0] == "version" and len(parts) == 3 and parts[2].isdigit():
            versions.append((parts[1], int(parts[2])))
        elif parts[0] == "fixture" and len(parts) == 3 and re.fullmatch(r"[0-9a-f]{64}", parts[2]):
            if parts[1] in fixtures:
                fail("%s:%d: fixture listed twice: %s" % (LOCK, n, parts[1]))
            fixtures[parts[1]] = parts[2]
        else:
            fail("%s:%d: cannot read the line: %s" % (LOCK, n, line))
    return versions, fixtures


def header_versions():
    text = open(os.path.join(ROOT, HEADER), encoding="utf-8").read()
    consts = dict(re.findall(r"constexpr int (\w+) = (\d+);", text))
    table = text[text.index("all {"):]
    out = []
    for key, name in re.findall(r'\{ "(\w+)", (\w+) \}', table):
        if name not in consts:
            fail("%s: the table names %s, which is not a constant" % (HEADER, name))
            continue
        out.append((key, int(consts[name])))
    size = re.search(r"std::array<Entry, (\d+)> all", text)
    if not size or int(size.group(1)) != len(out):
        fail("%s: the size of 'all' does not match its entries" % HEADER)
    return out


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def git(*args):
    return subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="", help="git ref of the pull request's base (checks what changed against it)")
    a = ap.parse_args()

    lock_text = open(os.path.join(ROOT, LOCK), encoding="utf-8").read()
    versions, fixtures = parse_lock(lock_text)

    # 1. versions == FormatVersions.h
    hv = header_versions()
    if versions != hv:
        fail("the versions in %s differ from %s:\n  lock:   %s\n  header: %s" % (LOCK, HEADER, versions, hv))

    # 2. fixtures == lock
    on_disk = {}
    base_dir = os.path.join(ROOT, FIXTURES)
    for dirpath, _, files in os.walk(base_dir):
        for f in files:
            full = os.path.join(dirpath, f)
            on_disk[os.path.relpath(full, base_dir).replace(os.sep, "/")] = sha256(full)
    for rel, digest in sorted(fixtures.items()):
        if rel not in on_disk:
            fail("fixture %s is in the lock but missing" % rel)
        elif on_disk[rel] != digest:
            fail("fixture %s was changed (fixtures are frozen: add a new file for a new shape)" % rel)
    for rel in sorted(set(on_disk) - set(fixtures)):
        fail("fixture %s is not in %s (add its line: tools/formats/make_fixtures.py --lock)" % (rel, LOCK))

    # 3. docs name every key with its version
    for doc in DOCS:
        path = os.path.join(ROOT, doc)
        if not os.path.exists(path):
            fail("%s is missing" % doc)
            continue
        text = open(path, encoding="utf-8").read()
        for key, v in hv:
            if not re.search(r"\|\s*`%s`\s*\|\s*%d\s*\|" % (re.escape(key), v), text):
                fail("%s has no table row \"| `%s` | %d |\"" % (doc, key, v))

    # 4. golden packs pass the pack tool
    for rel in sorted(on_disk):
        if rel.endswith(".ohpepack"):
            r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "pack", "make_ohpepack.py"), "check",
                                os.path.join(base_dir, rel)], capture_output=True, text=True)
            if r.returncode != 0:
                fail("fixture %s does not pass make_ohpepack.py check: %s" % (rel, (r.stdout + r.stderr).strip()))

    # 5. against the base
    if a.base and set(a.base) != {"0"}:
        if git("cat-file", "-e", a.base + "^{commit}").returncode != 0:
            fail("base %s is not available (fetch it first)" % a.base)
        else:
            old = git("show", "%s:%s" % (a.base, LOCK))
            if old.returncode != 0:
                print("note: %s does not exist at the base - first version of the lock, nothing to compare" % LOCK)
            else:
                ov, of = parse_lock(old.stdout)
                old_map, new_map = dict(ov), dict(versions)
                for i, (key, v) in enumerate(ov):
                    if key not in new_map:
                        fail("version key %s was removed (keys are append-only)" % key)
                    elif i >= len(versions) or versions[i][0] != key:
                        fail("version key %s moved (keys are append-only)" % key)
                    elif new_map[key] < v:
                        fail("version %s went down: %d -> %d" % (key, v, new_map[key]))
                for rel, digest in of.items():
                    if rel not in fixtures:
                        fail("fixture %s was removed (older files must always load)" % rel)
                    elif fixtures[rel] != digest:
                        fail("fixture %s was changed in the lock (fixtures are frozen)" % rel)
                bumped = [k for k, v in versions if k in old_map and v > old_map[k]]
                added = set(fixtures) - set(of)
                if bumped and not added:
                    fail("versions went up (%s) without a new fixture of the old shape" % ", ".join(bumped))
                if old.stdout != lock_text:
                    changed = set(git("diff", "--name-only", a.base, "--").stdout.split())
                    changed |= set(git("diff", "--name-only", "--cached").stdout.split())
                    for doc in DOCS:
                        if doc not in changed:
                            fail("%s changed but %s did not (describe the format change in both documents)" % (LOCK, doc))
                if bumped:
                    print("format versions raised: " + ", ".join("%s %d -> %d" % (k, old_map[k], new_map[k]) for k in bumped))

    if errors:
        print("\n%d format check error(s). Formats are frozen since 1.0: see docs/FORMATS.md." % len(errors))
        return 1
    print("formats ok: %d versions, %d fixtures" % (len(versions), len(fixtures)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
