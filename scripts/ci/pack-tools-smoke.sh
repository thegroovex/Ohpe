#!/usr/bin/env bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
# CI smoke test for the content pack tools (Linux, no build needed):
#  * tools/pack/make_ohpepack.py build -> check, same input = same bytes, a tampered pack is refused;
#  * tools/update/make_manifest.py writes a valid ohpe-update.json (assets with SHA-256, a content pack).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
PRESET=$(cd "$ROOT" && git ls-files '*.ohpepreset' | awk 'NR==1')
[ -n "$PRESET" ] || { echo "no .ohpepreset in the repo"; exit 1; }

# --- .ohpepack: build, check, reproducible, tamper detection
mkdir -p "$T/pack/files"
cp "$ROOT/$PRESET" "$T/pack/files/Smoke One.ohpepreset"
cp "$ROOT/$PRESET" "$T/pack/files/Smoke Two.ohpepreset"
echo "CI smoke pack" > "$T/pack/files/README.txt"
cat > "$T/pack/pack.json" <<'J'
{"id": "ci-smoke", "version": "1.0.0", "kind": "presets",
 "name": {"en": "CI smoke", "de": "CI-Test"}, "description": {"en": "Test pack", "de": "Testpaket"},
 "minApp": "0.5.0", "license": {"type": "free"}}
J
python3 "$ROOT/tools/pack/make_ohpepack.py" build "$T/pack" --out "$T/out1"
python3 "$ROOT/tools/pack/make_ohpepack.py" build "$T/pack" --out "$T/out2"
F="$T/out1/ci-smoke-1.0.0.ohpepack"
python3 "$ROOT/tools/pack/make_ohpepack.py" check "$F"
cmp "$F" "$T/out2/ci-smoke-1.0.0.ohpepack" || { echo "pack build is not reproducible"; exit 1; }
python3 "$ROOT/tools/pack/make_ohpepack.py" canonical "$F" > /dev/null
python3 - "$F" "$T/bad.ohpepack" <<'P'
import sys, zipfile
src, dst = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(src) as zi, zipfile.ZipFile(dst, "w") as zo:
    for n in zi.namelist():
        b = zi.read(n)
        if n.endswith(".ohpepreset"):
            b = b + b" "
        zo.writestr(n, b)
P
if python3 "$ROOT/tools/pack/make_ohpepack.py" check "$T/bad.ohpepack"; then
  echo "tampered pack passed the check"; exit 1
fi

# --- update manifest: fake release assets + one content folder
mkdir -p "$T/rel" "$T/content/smoke/files"
echo app > "$T/rel/Ohpe-0.0.0-Linux-x86_64.tar.gz"
echo dmg > "$T/rel/Ohpe-0.0.0-macOS.dmg"
cp "$ROOT/$PRESET" "$T/content/smoke/files/"
cat > "$T/content/smoke/pack.json" <<'J'
{"id": "smoke", "version": "1.0.0", "kind": "presets", "name": {"en": "Smoke", "de": "Test"}, "minApp": "0.5.0"}
J
printf '## English\nCI smoke\n## Deutsch\nCI-Test\n' > "$T/notes.md"
python3 "$ROOT/tools/update/make_manifest.py" --tag v0.0.0 --repo thegroovex/Ohpe --dir "$T/rel" \
  --body-file "$T/notes.md" --content-dir "$T/content" --date 2026-01-01
python3 - "$T/rel" <<'P'
import hashlib, json, os, re, sys
d = sys.argv[1]
m = json.load(open(os.path.join(d, "ohpe-update.json")))
assert m["assets"], "no assets"
for a in m["assets"]:
    for k in ("name", "size", "sha256", "url"):
        assert k in a, (k, a)
    assert re.fullmatch(r"[0-9a-f]{64}", a["sha256"]), a
    p = os.path.join(d, a["name"])
    if os.path.exists(p):
        assert hashlib.sha256(open(p, "rb").read()).hexdigest() == a["sha256"], a["name"]
assert [c["id"] for c in m["content"]] == ["smoke"], m["content"]
assert m["notes"]["en"].strip() and m["notes"]["de"].strip(), m["notes"]
print("manifest ok: %d assets, %d content" % (len(m["assets"]), len(m["content"])))
P
echo "pack tools smoke: ok"
