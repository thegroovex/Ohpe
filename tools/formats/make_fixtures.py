#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""p62: writes the golden format fixtures (tests/formats/fixtures) - see docs/FORMATS.md.

Fixtures are FROZEN: this script only writes files that do not exist yet and never changes an existing one
(scripts/ci/check-formats.py compares every fixture with its SHA-256 in tests/formats/FORMATS.lock). To add the shape of
a new format version, add a new file here, run the script, add its line to FORMATS.lock (--lock prints the lines) and
describe the version in docs/FORMATS.md and docs/de/FORMATE.md.

  make_fixtures.py [--out tests/formats/fixtures] [--lock]
"""
import argparse
import hashlib
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def dump(obj):
    return (json.dumps(obj, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


# ------------------------------------------------------------------------------------------------ state
def state_current():
    # the current shape (0.6.4 / 1.0): params by ID (plain values), sections, the ui block with one preset per part
    return dump({
        "format": "ohpe-state", "version": 1, "app": "Ohpe",
        "params": {"tempo": 128.0, "p1.engine": 0.0, "p1.vol": 0.625, "p1.tune": 7.0, "p1.amp.a": 0.25,
                   "p1.amp.r": 1.5, "p2.vol": 0.5, "harmony.kb.layout": 3.0, "harmony.kb.inkey": 0.0,
                   "harmony.kb.fixed": 1.0},
        "sections": {
            "midimap": {"format": "ohpe-midimap", "version": 1, "mappings": [
                {"param": "p1.vol", "type": "cc", "channel": 1, "number": 74, "min": 0.0, "max": 1.0, "mode": "absolute"}]},
            "tape": {"lengthSeconds": 120.0},
            "ui": {"preset": "factory:Glass Keys.ohpepreset",
                   "presets": ["factory:Glass Keys.ohpepreset", "", "user:Deep Bass.ohpepreset", ""]},
        },
    })


def state_legacy_in_key():
    # 0.6.x (before p59 / p50): grid layout 3 = the old "In Key" layout, no harmony.kb.inkey; the ui block has only
    # "preset" (the edit part's uid)
    return dump({
        "format": "ohpe-state", "version": 1, "app": "Ohpe",
        "params": {"tempo": 96.0, "p1.engine": 2.0, "p1.analog.cutoff": 800.0, "harmony.kb.layout": 3.0},
        "sections": {"ui": {"preset": "factory:Glass Keys.ohpepreset"}},
    })


def state_minimal():
    # 0.1: params only, no sections (no MIDI map, no ui block, no tape)
    return dump({"format": "ohpe-state", "version": 1, "app": "Ohpe",
                 "params": {"tempo": 140.0, "p1.engine": 1.0, "p1.wavetable.pos": 0.75}})


def state_future():
    # a state from a newer build: higher version, fields / sections / parameter IDs this build does not know
    return dump({
        "format": "ohpe-state", "version": 7, "app": "Ohpe", "writtenBy": "9.9.0",
        "params": {"tempo": 100.0, "p1.vol": 0.375, "zz.future.param": 3.0},
        "sections": {"futureSection": {"version": 4, "data": [1, 2, 3]}},
    })


# ------------------------------------------------------------------------------------------------ presets
def preset_part_current():
    return dump({
        "format": "ohpe-preset", "version": 2, "kind": "part", "name": "Format Keys", "author": "groovex",
        "tags": ["Keys", "bright"], "engine": "fm", "comment": "p62 fixture",
        "modSchema": 1,
        "params": {"engine": 0.0, "vol": 0.625, "tune": -5.0, "amp.a": 0.25, "amp.r": 1.5,
                   "mod.s1.src": 1.0, "mod.s1.dst": 1.0, "mod.s1.amt": 0.5},
        "arp": {"on": 1, "mode": 2, "oct": 2, "gate": 0.5, "rate": 3},
    })


def preset_part_v1():
    # version 1 (0.1 / 0.2): no kind / tags / engine, "prefix" names the part it was saved from
    return dump({"format": "ohpe-preset", "version": 1, "name": "Old Bass", "prefix": "p1.",
                 "params": {"engine": 2.0, "vol": 0.5, "analog.cutoff": 800.0, "amp.r": 0.75}})


def preset_part_no_modschema():
    # 0.6 era: matrix routings without "modSchema" (= 0, migrated)
    return dump({"format": "ohpe-preset", "version": 2, "kind": "part", "name": "Old Mod", "author": "", "tags": [],
                 "engine": "analog", "comment": "",
                 "params": {"engine": 2.0, "mod.s1.src": 1.0, "mod.s1.dst": 1.0, "mod.s1.amt": 0.25}})


def preset_project_current():
    return dump({
        "format": "ohpe-preset", "version": 2, "kind": "project", "name": "Format Song", "author": "groovex",
        "tags": ["Arp"], "engine": "fm", "comment": "",
        "params": {"tempo": 110.0, "p1.engine": 0.0, "p1.vol": 0.625, "p2.vol": 0.5, "harmony.kb.layout": 3.0,
                   "harmony.kb.inkey": 0.0},
    })


def preset_project_legacy_in_key():
    # before p59: grid layout 3 = the old "In Key" layout, no harmony.kb.inkey
    return dump({"format": "ohpe-preset", "version": 2, "kind": "project", "name": "Old Grid Song", "author": "",
                 "tags": [], "engine": "analog", "comment": "",
                 "params": {"tempo": 90.0, "p1.engine": 2.0, "harmony.kb.layout": 3.0}})


def preset_part_future():
    return dump({"format": "ohpe-preset", "version": 3, "kind": "part", "name": "From The Future", "author": "groovex",
                 "tags": ["Pads"], "engine": "fm", "comment": "", "colour": "#ffcc00", "modSchema": 1,
                 "params": {"engine": 0.0, "vol": 0.375, "zz.future": 1.0},
                 "futureBlock": {"anything": True}})


# ------------------------------------------------------------------------------------------------ tape
FRAMES = 4096
TRACKS = 8


def tape_blob(rate, length_frames, blocks):
    b = bytearray(b"OHPETAPE")
    b += struct.pack("<IIIdQ", 1, TRACKS, FRAMES, float(rate), int(length_frames))
    for track, page, left, right in blocks:
        b += struct.pack("<II", track, page)
        b += struct.pack("<%df" % FRAMES, *left)
        b += struct.pack("<%df" % FRAMES, *right)
    b += struct.pack("<I", 0xFFFFFFFF)
    return bytes(b)


def sine_block(rate, freq, amp, page):
    return [amp * math.sin(2.0 * math.pi * freq * (page * FRAMES + i) / rate) for i in range(FRAMES)]


def tape_file(header, blob):
    h = json.dumps(header, separators=(",", ":")).encode("utf-8")
    return b"OHPETPF1" + struct.pack("<I", len(h)) + h + blob


def tape_current():
    rate = 48000.0
    left = sine_block(rate, 1000.0, 0.5, 1)
    right = sine_block(rate, 500.0, 0.25, 1)
    blob = tape_blob(rate, 10 * 48000, [(2, 1, left, right)])
    return tape_file({"format": "ohpe-tape", "version": 1, "sampleRate": rate, "lengthSeconds": 10.0, "loopIn": 0.5,
                      "loopOut": 2.0, "loop": True, "editTrack": 3}, blob)


def tape_minimal_44k():
    # the smallest valid header (no loop / editTrack: defaults), recorded at 44.1 kHz (resampled on load)
    rate = 44100.0
    left = sine_block(rate, 1000.0, 0.5, 0)
    blob = tape_blob(rate, 20 * 44100, [(0, 0, left, left)])
    return tape_file({"format": "ohpe-tape", "version": 1, "sampleRate": rate, "lengthSeconds": 20.0}, blob)


def tape_future():
    rate = 48000.0
    blob = tape_blob(rate, 10 * 48000, [])
    return tape_file({"format": "ohpe-tape", "version": 2, "sampleRate": rate, "lengthSeconds": 10.0}, blob)


def plugin_state_with_tape():
    # plug-in state (0.2+): <state JSON> '\0' <tape audio blob>
    js = json.dumps({"format": "ohpe-state", "version": 1, "app": "Ohpe",
                     "params": {"tempo": 125.0, "p1.vol": 0.625},
                     "sections": {"tape": {"lengthSeconds": 30.0},
                                  "ui": {"preset": "", "presets": ["", "", "", ""]}}},
                    separators=(",", ":")).encode("utf-8")
    rate = 48000.0
    blob = tape_blob(rate, 30 * 48000, [(0, 2, sine_block(rate, 440.0, 0.5, 2), sine_block(rate, 440.0, 0.5, 2))])
    return js + b"\0" + blob


# ------------------------------------------------------------------------------------------------ wavetable
def ohpewt(name, frames, formants):
    nb = name.encode("utf-8")
    b = bytearray(b"OHPEWT") + struct.pack("<H", 1) + struct.pack("<IIII", 2048, len(frames), len(formants), len(nb)) + nb
    for f in frames:
        b += struct.pack("<2048f", *f)
    for freq, gain, bw in formants:
        b += struct.pack("<5f", *freq) + struct.pack("<5f", *gain) + struct.pack("<5f", *bw)
    return bytes(b)


def wavetable_current():
    sine = [math.sin(2.0 * math.pi * i / 2048.0) for i in range(2048)]
    tri = [(2.0 / math.pi) * math.asin(math.sin(2.0 * math.pi * i / 2048.0)) for i in range(2048)]
    return ohpewt("Format Table", [sine, tri], [])


def wavetable_formants():
    a = ([800.0, 1150.0, 2900.0, 3900.0, 4950.0], [0.0, -6.0, -32.0, -20.0, -50.0], [80.0, 90.0, 120.0, 130.0, 140.0])
    o = ([400.0, 750.0, 2400.0, 2600.0, 2900.0], [0.0, -11.0, -21.0, -20.0, -40.0], [40.0, 80.0, 100.0, 120.0, 120.0])
    return ohpewt("Format Vowels", [], [a, o])


# ------------------------------------------------------------------------------------------------ packs
def preset_for_pack():
    return dump({"format": "ohpe-preset", "version": 2, "kind": "part", "name": "Pack Keys", "author": "groovex",
                 "tags": ["Keys"], "engine": "fm", "comment": "", "params": {"engine": 0.0, "vol": 0.5}})


def pack_current(out_dir):
    with tempfile.TemporaryDirectory() as t:
        os.makedirs(os.path.join(t, "pack", "files"))
        with open(os.path.join(t, "pack", "files", "Pack Keys.ohpepreset"), "wb") as f:
            f.write(preset_for_pack())
        with open(os.path.join(t, "pack", "files", "readme.txt"), "w") as f:
            f.write("Ohpe format fixture pack (p62)\n")
        with open(os.path.join(t, "pack", "pack.json"), "w") as f:
            json.dump({"id": "format-fixture", "version": "1.0.0", "kind": "presets",
                       "name": {"en": "Format fixture", "de": "Format-Testpaket"},
                       "description": {"en": "Golden pack of format 1", "de": "Goldenes Paket im Format 1"},
                       "minApp": "0.5.0", "license": {"type": "free"}}, f)
        subprocess.check_call([sys.executable, os.path.join(ROOT, "tools", "pack", "make_ohpepack.py"), "build",
                               os.path.join(t, "pack"), "--out", t])
        with open(os.path.join(t, "format-fixture-1.0.0.ohpepack"), "rb") as f:
            return f.read()


def pack_legacy_flat():
    # the updater's first packs: a plain zip, *.ohpepreset at the root, no pack.json (name gives id / version)
    import io
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        info = zipfile.ZipInfo("Flat Keys.ohpepreset", date_time=(2026, 1, 1, 0, 0, 0))
        z.writestr(info, preset_for_pack())
    return buf.getvalue()


def pack_json_future():
    return dump({"format": 2, "id": "future-pack", "version": "1.0.0", "kind": "presets",
                 "name": {"en": "Future", "de": "Zukunft"}, "files": {}})


def pack_json_minimal():
    # pack.json without "format" (read as format 1), only the required keys
    return dump({"id": "minimal-pack", "version": "1.0.0", "files": {}})


# ------------------------------------------------------------------------------------------------ settings
def settings_current():
    return dump({"version": 1, "language": "de", "tooltips": False, "helpOverlay": True, "skin": "dark",
                 "brightness": 0.7, "animations": "reduced", "windowScale": 1.25,
                 "controller": {"profile": "off", "in": "", "out": "", "master": "tempo", "strip": "mod", "auto": False,
                                "pattern": "User; MIDIIN2; MIDIOUT2", "chosen": True},
                 "futureKey": {"kept": True}})


def settings_v0_062():
    # 0.6.2 and older: no "version", controller set up before "chosen" existed (migrated to the 0.6.3 defaults)
    return dump({"language": "en", "tooltips": True, "helpOverlay": False, "skin": "light", "brightness": 1.0,
                 "animations": "full", "windowScale": 1.0,
                 "controller": {"profile": "off", "in": "Port A", "out": "Port B", "master": "volume", "strip": "bend"}})


def update_current():
    return dump({"version": 1, "autoCheck": False, "channel": "stable", "lastCheck": 1760000000, "skipped": "0.7.0",
                 "remindVersion": "", "remindAfter": 0})


def update_v0():
    return dump({"autoCheck": True, "channel": "preview", "lastCheck": 1750000000, "skipped": "", "remindVersion": "",
                 "remindAfter": 0})


def midimap_current():
    return dump({"format": "ohpe-midimap", "version": 1, "mappings": [
        {"param": "p1.vol", "type": "cc", "channel": 1, "number": 7, "min": 0.0, "max": 1.0, "mode": "absolute"},
        {"param": "seq.run", "type": "note", "channel": 10, "number": 36, "min": 0.0, "max": 1.0, "mode": "toggle"},
        {"param": "tempo", "type": "cc", "channel": 0, "number": 20, "min": 0.25, "max": 0.75, "mode": "relative"}]})


FIXTURES = {
    "state/state-v1.json": state_current,
    "state/state-v1-0.6-in-key.json": state_legacy_in_key,
    "state/state-v1-0.1-minimal.json": state_minimal,
    "state/state-v7-future.json": state_future,
    "state/plugin-state-v1-with-tape.bin": plugin_state_with_tape,
    "preset/part-v2.ohpepreset": preset_part_current,
    "preset/part-v1-0.1.ohpepreset": preset_part_v1,
    "preset/part-v2-no-modschema.ohpepreset": preset_part_no_modschema,
    "preset/project-v2.ohpepreset": preset_project_current,
    "preset/project-v2-0.6-in-key.ohpepreset": preset_project_legacy_in_key,
    "preset/part-v3-future.ohpepreset": preset_part_future,
    "tape/tape-v1.ohpetape": tape_current,
    "tape/tape-v1-minimal-44k.ohpetape": tape_minimal_44k,
    "tape/tape-v2-future.ohpetape": tape_future,
    "wavetable/table-v1.ohpewt": wavetable_current,
    "wavetable/table-v1-formants.ohpewt": wavetable_formants,
    "pack/format-fixture-1.0.0.ohpepack": None,  # special: built with make_ohpepack.py
    "pack/Ohpe-content-legacy-flat-1.0.0.zip": pack_legacy_flat,
    "pack/pack-json-format2-future.json": pack_json_future,
    "pack/pack-json-minimal.json": pack_json_minimal,
    "settings/settings-v1.json": settings_current,
    "settings/settings-v0-0.6.2.json": settings_v0_062,
    "settings/update-v1.json": update_current,
    "settings/update-v0.json": update_v0,
    "midimap/midimap-v1.ohpemidi": midimap_current,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "tests", "formats", "fixtures"))
    ap.add_argument("--lock", action="store_true", help="print the fixture lines for FORMATS.lock")
    a = ap.parse_args()
    for rel, fn in FIXTURES.items():
        path = os.path.join(a.out, rel)
        if not os.path.exists(path):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            data = pack_current(a.out) if fn is None else fn()
            with open(path, "wb") as f:
                f.write(data)
            print("wrote", rel, file=sys.stderr)
    if a.lock:
        for rel in sorted(FIXTURES):
            with open(os.path.join(a.out, rel), "rb") as f:
                print("fixture %s %s" % (rel, hashlib.sha256(f.read()).hexdigest()))


if __name__ == "__main__":
    main()
