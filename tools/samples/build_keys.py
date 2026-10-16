#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
0.8 (sound revision 3): builds the keyboard instruments of the factory sample bank and merges them
into resources/samples/bank.json (the other instruments and kits stay untouched):

  grandpiano    VCSL grand piano recordings (CC0 1.0), close microphones, no sustain pedal:
                every fourth semitone C1 .. C7, two velocity layers (soft vl2 / hard vl4)
  uprightpiano  VCSL upright piano recordings (CC0 1.0), "Sustains", every C and G, one layer (vl3)
  tinepiano     Ohpe's own modelled tine electric piano (two velocity layers, every sixth semitone)
  reedpiano     Ohpe's own modelled reed electric piano (two velocity layers, every sixth semitone)

The modelled pianos are generated here (part of Ohpe, AGPL-3.0-or-later); the generated audio is
additionally dedicated to the public domain under CC0 1.0, so the bank stays uniformly CC0.
Root keys of the recordings are measured from the audio (pitch), not taken from the file names.

  python3 tools/samples/build_keys.py [--cache DIR] [--wav-cache DIR]

Needs what build_samples.py needs (numpy, scipy, soundfile, ffmpeg).
"""
import argparse, json, os, sys, urllib.request

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_samples as bs  # noqa: E402

RATE = 44100
TREE_URL = "https://api.github.com/repos/sgossner/VCSL/git/trees/%s?recursive=1" % bs.VCSL_REV
GRAND_FILES = "JHPiano_NoSus_Close_"   # VCSL grand piano, close microphones, no sustain pedal
UPRIGHT_FILES = "Upright1_Sus_"         # VCSL upright piano, sustained notes


def vcsl_tree(cache):
    """The file list of the pinned VCSL revision (cached): the recordings are found by file name."""
    path = os.path.join(cache, "vcsl-tree-%s.json" % bs.VCSL_REV[:12])
    if not os.path.exists(path):
        os.makedirs(cache, exist_ok=True)
        with urllib.request.urlopen(TREE_URL, timeout=120) as r, open(path, "wb") as f:
            f.write(r.read())
    return [e["path"] for e in json.load(open(path))["tree"] if e["path"].endswith(".wav")]


def find(tree, file_name):
    hits = [p for p in tree if p.rsplit("/", 1)[-1] == file_name]
    if not hits:
        sys.exit("not in VCSL: " + file_name)
    return hits[0]


def grand_defs(tree):
    zones = []
    for octave in range(1, 8):
        for n in ("C", "E", "G#"):
            name = "%s%d" % (n, octave)
            if bs.midi(name) > bs.midi("C7"):
                continue
            zones.append((name, find(tree, GRAND_FILES + "%s_vl2_rr1.wav" % name), 1, 80))
            zones.append((name, find(tree, GRAND_FILES + "%s_vl4_rr1.wav" % name), 81, 127))
    return zones


def upright_defs(tree):
    zones = []
    for octave in range(0, 7):
        for n in ("C", "G"):
            name = "%s%d" % (n, octave)
            if name in ("C0", "G0"):
                continue
            if octave == 6 and n == "G":
                continue
            zones.append((name, find(tree, UPRIGHT_FILES + "%s_vl3_rr1.wav" % name), 1, 127))
    return zones


def measure_root(m, rate, start):
    """Nearest MIDI note of the fundamental (harmonic product spectrum) 50 .. 650 ms after the attack."""
    seg = m[start + int(0.05 * rate): start + int(0.65 * rate)]
    seg = (seg - np.mean(seg)) * np.hanning(len(seg))
    n = 1 << 18
    spec = np.abs(np.fft.rfft(seg, n))
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    hps = np.log(spec + 1e-12)
    for h in (2, 3, 4):
        dec = np.log(spec[::h] + 1e-12)
        hps[: len(dec)] += dec
    band = (freqs > 26.0) & (freqs < 4500.0)
    f = freqs[band][int(np.argmax(hps[band]))]
    return int(round(69 + 12 * np.log2(f / 440.0)))


def piano_secs(root, low, high):
    """Length by register: low notes ring longer."""
    t = np.clip((root - 24) / 72.0, 0.0, 1.0)
    return low + (high - low) * t


# ------------------------------------------------------------------ modelled electric pianos
def ep_note(kind, root, vel, rate, secs):
    """One electric-piano note. kind 'tine' or 'reed', vel 0..1."""
    f0 = 440.0 * 2 ** ((root - 69) / 12.0)
    n = int(secs * rate)
    t = np.arange(n) / rate
    rng = np.random.default_rng(root * 7 + int(vel * 100) + (0 if kind == "tine" else 1000))
    reg = np.clip((root - 28) / 72.0, 0.0, 1.0)  # 0 = low register, 1 = top
    if kind == "tine":
        # tine + tone bar: a strong fundamental, a weak second harmonic and the bright, fast-dying tine
        # partial (inharmonic, about 7x); low notes ring longer
        dec = 4.5 - 3.3 * reg
        x = np.exp(-t / dec) * (0.65 + 0.35 * np.exp(-t / 0.35))
        x = x * np.sin(2 * np.pi * f0 * t)
        x += 0.06 * np.exp(-t / (dec * 0.5)) * np.sin(2 * np.pi * 2 * f0 * t + 0.3)
        bell = min(7.07 * f0, 9000.0)
        x += (0.10 + 0.25 * vel) * (1.0 - 0.6 * reg) * np.exp(-t / (0.06 + 0.05 * (1 - reg))) * np.sin(2 * np.pi * bell * t)
        x += 0.03 * vel * np.exp(-t / 0.02) * np.sin(2 * np.pi * min(13.1 * f0, 12000.0) * t)
        # pickup: displacement -> asymmetric curve (the "bark" grows with the velocity)
        drive = 0.6 + 2.6 * vel ** 1.6 * (1.0 - 0.5 * reg)
        y = np.tanh(drive * x + 0.35 * drive * x * x) / np.tanh(drive)
        # hammer thump
        thump = rng.standard_normal(int(0.012 * rate)) * np.exp(-np.arange(int(0.012 * rate)) / (0.002 * rate))
        y[: len(thump)] += 0.04 * vel * thump
        y = bs.lp(y[:, None], 6500 + 4000 * vel, rate)[:, 0]
        y = bs.hp(y[:, None], 30, rate)[:, 0]
    else:
        # reed (cantilever bar modes 1, 6.27, 17.55), electrostatic pickup: odd-heavy, nasal, square bark
        dec = 3.0 - 2.2 * reg
        x = np.exp(-t / dec) * (0.55 + 0.45 * np.exp(-t / 0.25)) * np.sin(2 * np.pi * f0 * t)
        x += (0.05 + 0.12 * vel) * np.exp(-t / 0.08) * np.sin(2 * np.pi * min(6.27 * f0, 10000.0) * t)
        x += 0.04 * vel * np.exp(-t / 0.03) * np.sin(2 * np.pi * min(17.55 * f0, 14000.0) * t)
        drive = 0.8 + 4.0 * vel ** 1.5 * (1.0 - 0.4 * reg)
        y = np.tanh(drive * (x + 0.12 * x * x)) / np.tanh(drive)
        y = 0.75 * y + 0.25 * bs.bp(y[:, None], 700, 2600, rate)[:, 0] * 2.0  # nasal mid
        thump = rng.standard_normal(int(0.01 * rate)) * np.exp(-np.arange(int(0.01 * rate)) / (0.0015 * rate))
        y[: len(thump)] += 0.05 * vel * thump
        y = bs.lp(y[:, None], 5000 + 3500 * vel, rate)[:, 0]
        y = bs.hp(y[:, None], 40, rate)[:, 0]
    attack = int(0.0015 * rate)
    y[:attack] *= np.linspace(0.0, 1.0, attack)
    fade = int(0.3 * n)
    y[-fade:] *= np.cos(np.linspace(0, np.pi / 2, fade)) ** 2
    return y


def build_recorded(cache, inst, zones, secs_low, secs_high):
    d = os.path.join(bs.OUT, "instruments", inst)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    loaded = []
    for note, f, vlo, vhi in zones:
        x = bs.load(cache, bs.vcsl(f), RATE)
        m = np.mean(x, axis=1)
        m = m - np.mean(m)
        start = max(0, int(np.argmax(np.abs(m) > np.max(np.abs(m)) * bs.db(-36))) - int(0.002 * RATE))
        loaded.append((note, vlo, vhi, m, start, bs.midi(note) - measure_root(m, RATE, start)))
    # the measured pitches decide the octave convention of the file names (the median over all notes:
    # the lowest and highest strings measure unreliably); within it the names give the root keys
    offset = -12 * int(round(np.median([l[5] for l in loaded]) / 12.0))
    print("  %s: octave offset of the file names %+d" % (inst, offset))
    rendered = []
    for note, vlo, vhi, m, start, _ in loaded:
        root = bs.midi(note) + offset
        secs = piano_secs(root, secs_low, secs_high)
        m = bs.finish(bs.stereo(m), secs, RATE, 0.4, start=start)[:, 0]
        ref = np.sqrt(np.mean(m[: int(0.4 * RATE)] ** 2))
        rendered.append([root, vlo, vhi, m, ref])
    return finish_bank(inst, rendered, d)


def build_modelled(inst, kind):
    d = os.path.join(bs.OUT, "instruments", inst)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    rendered = []
    for root in range(28, 101, 6):
        secs = piano_secs(root, 3.2, 1.6)
        for vel, vlo, vhi in ((0.45, 1, 84), (1.0, 85, 127)):
            m = ep_note(kind, root, vel, RATE, secs)
            ref = np.sqrt(np.mean(m[: int(0.4 * RATE)] ** 2))
            rendered.append([root, vlo, vhi, m, ref])
    return finish_bank(inst, rendered, d, soft_db=-8.0)


def finish_bank(inst, rendered, d, soft_db=-7.0):
    # level-match the top layer of every note, keep the soft layers below it, one peak normalisation
    top = {r[0]: r[4] for r in rendered if r[2] == 127}
    med = float(np.median(list(top.values())))
    for r in rendered:
        if r[2] == 127:
            g = min(max(med / top[r[0]], bs.db(-9)), bs.db(9))
        else:
            g = med * bs.db(soft_db) / max(1e-9, r[4])
        r[3] = r[3] * g
    peak = max(np.max(np.abs(r[3])) for r in rendered)
    roots = sorted(set(r[0] for r in rendered))
    out = []
    for r in rendered:
        root = r[0]
        i = roots.index(root)
        lo = 0 if i == 0 else (roots[i - 1] + root) // 2 + 1
        hi = 127 if i == len(roots) - 1 else (root + roots[i + 1]) // 2
        name = "%s-%d%s.flac" % (inst, root, "" if r[2] == 127 and r[1] == 1 else "-v%d" % r[2])
        bs.write_flac(os.path.join(d, name), bs.stereo(r[3] * bs.db(-1.0) / peak), RATE, True)
        out.append({"file": "instruments/%s/%s" % (inst, name), "root": root, "lo": lo, "hi": hi,
                    "vlo": r[1], "vhi": r[2], "ls": -1, "le": -1})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default="sample-cache")
    ap.add_argument("--wav-cache")
    ap.add_argument("--only", choices=["recorded", "modelled"])
    a = ap.parse_args()
    if a.wav_cache:
        bs.WAV_CACHE = os.path.abspath(a.wav_cache)
        os.makedirs(bs.WAV_CACHE, exist_ok=True)
    bank_path = os.path.join(bs.OUT, "bank.json")
    bank = json.load(open(bank_path))
    if a.only != "modelled":
        tree = vcsl_tree(a.cache)
        gz, uz = grand_defs(tree), upright_defs(tree)
        bs.fetch_all(a.cache, [bs.vcsl(z[1]) for z in gz + uz])
        bank["instruments"]["grandpiano"] = build_recorded(a.cache, "grandpiano", gz, 3.6, 1.8)
        bank["instruments"]["uprightpiano"] = build_recorded(a.cache, "uprightpiano", uz, 2.8, 1.6)
    if a.only != "recorded":
        bank["instruments"]["tinepiano"] = build_modelled("tinepiano", "tine")
        bank["instruments"]["reedpiano"] = build_modelled("reedpiano", "reed")
    with open(bank_path, "w") as f:
        json.dump(bank, f, indent=1, sort_keys=True)
        f.write("\n")
    for inst in ("grandpiano", "uprightpiano", "tinepiano", "reedpiano"):
        d = os.path.join(bs.OUT, "instruments", inst)
        if os.path.isdir(d):
            size = sum(os.path.getsize(os.path.join(d, x)) for x in os.listdir(d))
            print("%-13s %2d files %.2f MB" % (inst, len(os.listdir(d)), size / 1e6))


if __name__ == "__main__":
    main()
