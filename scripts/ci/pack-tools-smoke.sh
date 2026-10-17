#!/usr/bin/env bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
# CI smoke test for the content pack tools (Linux, no build needed):
#  * tools/pack/make_ohpepack.py build -> check, same input = same bytes, a tampered pack is refused;
#  * wavetable packs: whole 2048-sample frames pass, anything else is refused;
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

# --- wavetable pack: 2048-sample frames build and check; a WAV that is not whole frames is refused
mkdir -p "$T/wt/files" "$T/wtbad/files"
python3 - "$T/wt/files/Smoke Saw.wav" 4 "$T/wtbad/files/Odd.wav" <<'P'
import math, struct, sys, wave
def write(path, n):
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
        w.writeframes(b"".join(struct.pack("<h", int(12000 * math.sin(2 * math.pi * i / 2048))) for i in range(n)))
write(sys.argv[1], 2048 * int(sys.argv[2]))
write(sys.argv[3], 1000)
P
for d in wt wtbad; do cat > "$T/$d/pack.json" <<'J'
{"id": "ci-smoke-wt", "version": "1.0.0", "kind": "wavetables",
 "name": {"en": "CI tables", "de": "CI-Tabellen"}, "description": {"en": "Test", "de": "Test"}, "minApp": "0.5.0"}
J
done
python3 "$ROOT/tools/pack/make_ohpepack.py" build "$T/wt" --out "$T/wtout"
python3 "$ROOT/tools/pack/make_ohpepack.py" check "$T/wtout/ci-smoke-wt-1.0.0.ohpepack"
if python3 "$ROOT/tools/pack/make_ohpepack.py" build "$T/wtbad" --out "$T/wtbadout" 2>/dev/null \
   && python3 "$ROOT/tools/pack/make_ohpepack.py" check "$T/wtbadout/ci-smoke-wt-1.0.0.ohpepack" 2>/dev/null; then
  echo "a wavetable that is not whole 2048-sample frames was accepted"; exit 1
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
