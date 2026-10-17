#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p44: level calibration of the showcase presets. Renders every showcase part preset with ohpe_render (its category
phrase, with the factory recordings), moves its part gain towards the category target of analyse.py (active RMS) and
keeps the true peak at or below -1 dBTP; writes showcase_levels.json and re-runs make_showcase.py. Also prints the
loudness numbers of the RANDOM key's measure (EBU R128 K-weighting, the loudest 400 ms block = "momentary max") so the
new sounds can be compared with the factory median.

  python3 tools/presets/showcase_cal.py --render build-core/tests/core/ohpe_render --wavdir DIR [--rounds 4] [--report]
"""
import argparse, json, math, os, statistics, subprocess, sys, tempfile, wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "resources", "presets", "factory")
LEVELS = os.path.join(HERE, "showcase_levels.json")
sys.path.insert(0, HERE)
from analyse import TARGET  # noqa: E402

PEAK_MAX = -1.5  # dBTP after calibration (the tests allow -1; a little margin for the platforms)


def read_wav(path):
    """WAV reader for ohpe_render's 32-bit float files (and 16-bit PCM)."""
    import struct
    data = open(path, "rb").read()
    pos, fmt, ch, sr, bits, pcm = 12, 1, 2, 48000, 32, b""
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt, ch, sr = struct.unpack("<HHI", body[:8])
            bits = struct.unpack("<H", body[14:16])[0]
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    if fmt == 3 or (fmt == 0xFFFE and bits == 32):
        a = np.frombuffer(pcm, dtype="<f4").astype(np.float64)
    else:
        a = np.frombuffer(pcm, dtype="<i2").astype(np.float64) / 32768.0
    return a.reshape(-1, ch).T, sr


def biquad(x, b, a):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    _, a1, a2 = a
    for i in range(len(x)):
        v = b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, x[i], y1, v
        y[i] = v
    return y


def kweight(x, sr):
    try:
        from scipy.signal import lfilter
    except ImportError:
        lfilter = None
    # ITU-R BS.1770 pre-filter (high shelf) and RLB high-pass, coefficients for sr via bilinear design
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = math.tan(math.pi * f0 / sr)
    Vh, Vb = 10 ** (G / 20.0), 10 ** (G / 20.0) ** 0.4996667741545416
    a0 = 1.0 + K / Q + K * K
    b1 = [(Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0]
    a1 = [1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0]
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = math.tan(math.pi * f0 / sr)
    a2 = [1.0, 2.0 * (K * K - 1.0) / (1.0 + K / Q + K * K), (1.0 - K / Q + K * K) / (1.0 + K / Q + K * K)]
    b2 = [1.0, -2.0, 1.0]
    if lfilter is not None:
        return lfilter(b2, a2, lfilter(b1, a1, x))
    return biquad(biquad(x, b1, a1), b2, a2)


def loudness(a, sr):
    """(momentary max LUFS = loudest 400 ms block, hop 100 ms; integrated LUFS (gated); true peak dBTP (4x))."""
    k = np.vstack([kweight(c, sr) for c in a])
    p = (k ** 2).sum(axis=0)
    blk, hop = int(0.4 * sr), int(0.1 * sr)
    cs = np.concatenate([[0.0], np.cumsum(p)])
    ms = [(cs[s + blk] - cs[s]) / blk for s in range(0, len(p) - blk, hop)]
    lu = [-0.691 + 10 * math.log10(m) if m > 0 else -200.0 for m in ms]
    mmax = max(lu) if lu else -200.0
    g = [m for m, l in zip(ms, lu) if l > -70.0]
    if g:
        rel = -0.691 + 10 * math.log10(np.mean(g)) - 10.0
        g2 = [m for m, l in zip(ms, lu) if l > rel]
        integ = -0.691 + 10 * math.log10(np.mean(g2)) if g2 else -200.0
    else:
        integ = -200.0
    tp = 0.0
    for c in a:
        n = len(c)
        F = np.fft.rfft(c)
        up = np.fft.irfft(F, 4 * n) * 4
        tp = max(tp, float(np.max(np.abs(up))))
    return mmax, integ, (20 * math.log10(tp) if tp > 0 else -200.0)


def render(args, files, extra=()):
    with tempfile.TemporaryDirectory() as d:
        cmd = [args.render, "--bank", os.path.join(ROOT, "resources", "samples", "bank.json"), "--wavdir", args.wavdir,
               "--out", d] + list(extra) + files
        res = subprocess.run(cmd, capture_output=True, text=True)
        rows = {}
        for line in res.stdout.splitlines():
            if line.startswith("{"):
                r = json.loads(line)
                wavs = [f for f in os.listdir(d) if f.startswith(r["file"]) and f.endswith(".wav")]
                if wavs:
                    a, sr = read_wav(os.path.join(d, wavs[0]))
                    r["mmax"], r["lufs"], r["tp"] = loudness(a, sr)
                rows[r["file"]] = r
        if not rows:
            sys.stderr.write(res.stderr)
        return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--wavdir", required=True)
    ap.add_argument("--rounds", type=int, default=4)
    ap.add_argument("--report", action="store_true", help="measure only")
    ap.add_argument("--only", nargs="*", default=[])
    a = ap.parse_args()
    manifest = json.load(open(os.path.join(HERE, "showcase_files.json")))
    levels = json.load(open(LEVELS)) if os.path.exists(LEVELS) else {}
    for rnd in range(0 if a.report else a.rounds):
        files = [os.path.join(OUT, f) for f in manifest if not f.startswith("project-") and not f.startswith("morph-")
                 and (not a.only or any(o in f for o in a.only))]
        rows = render(a, files)
        changed = 0
        for f in files:
            doc = json.load(open(f))
            r = rows.get(os.path.basename(f)[:-len(".ohpepreset")])
            if r is None:
                print("no render:", f)
                continue
            target = TARGET.get(doc["tags"][0], -16.0)
            g = levels.get(doc["name"], doc["params"].get("gain", 0.0))
            step = target - r["active"]
            step = min(step, PEAK_MAX - r.get("tp", r["peak"]))  # never above the peak ceiling
            if abs(step) > 0.3:
                levels[doc["name"]] = round(max(-24.0, min(24.0, g + 0.9 * step)), 1)
                changed += 1
        json.dump(dict(sorted(levels.items())), open(LEVELS, "w"), indent=1)
        subprocess.run([sys.executable, os.path.join(HERE, "make_showcase.py")], check=True)
        print("round %d: %d gains changed" % (rnd + 1, changed))
        if changed == 0:
            break
    # projects: the master volume brings the mix (limiter on, as heard) to -14 dBFS active RMS over 8 s
    pfiles = [os.path.join(OUT, f) for f in manifest if f.startswith("project-")]
    for rnd in range(0 if a.report else 3):
        rows = render(a, pfiles, ["--seconds", "8", "--limit"])
        changed = 0
        for f in pfiles:
            doc = json.load(open(f))
            r = rows.get(os.path.basename(f)[:-len(".ohpepreset")])
            if r is None:
                continue
            step = -14.0 - r["active"]
            if abs(step) > 0.5:
                levels[doc["name"]] = round(max(-24.0, min(12.0, doc["params"].get("mix.master.vol", 0.0) + step)), 1)
                changed += 1
        json.dump(dict(sorted(levels.items())), open(LEVELS, "w"), indent=1)
        subprocess.run([sys.executable, os.path.join(HERE, "make_showcase.py")], check=True)
        if changed == 0:
            break
    for k, r in sorted(render(a, pfiles, ["--seconds", "4", "--limit"]).items()):
        print("project %-30s active %.1f peak %.1f tp %.1f mmax %.1f lufs %.1f" % (r["name"], r["active"], r["peak"], r.get("tp", 0),
              r.get("mmax", 0), r.get("lufs", 0)))
    # report: category phrase and the plug-in test's window (4 beats + 1 s)
    files = [os.path.join(OUT, f) for f in manifest if not f.startswith("project-")]
    full = render(a, files)
    short = render(a, files, ["--beats", "4", "--tail", "1"])
    print("%-28s %-9s %7s %7s %7s %7s %7s %7s" % ("name", "cat", "active", "short", "peak", "tp", "mmax", "lufs"))
    mm = []
    for f in files:
        k = os.path.basename(f)[:-len(".ohpepreset")]
        r, s = full.get(k), short.get(k)
        if not r:
            continue
        flag = ""
        if s and not (-21.0 < s["active"] < -10.0 and s["peak"] < -0.5 and s["hf"] < -3.0):
            flag = "  <-- outside the plug-in test window"
        if r.get("tp", -200) > -1.0:
            flag += "  <-- true peak"
        mm.append(r.get("mmax", -200))
        print("%-28s %-9s %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f%s" % (r["name"][:28], r["category"], r["active"],
              s["active"] if s else float("nan"), r["peak"], r.get("tp", float("nan")), r.get("mmax", float("nan")),
              r.get("lufs", float("nan")), flag))
    if mm:
        print("momentary max: median %.1f LUFS, min %.1f, max %.1f (n=%d)" % (statistics.median(mm), min(mm), max(mm), len(mm)))


if __name__ == "__main__":
    main()
