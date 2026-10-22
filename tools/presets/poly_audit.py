#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p74 polyphony audit: plays every factory part preset with held chords (1, 4, 6 and 8 notes; close, high and wide
voicings; velocity 100 / 127; mod wheel 0 / max) through ohpe_render and measures, per render:

  tp      true peak (4x oversampled, dBTP) of the part output, master limiter OFF (what the preset itself does)
  peak    sample peak (dBFS), same render
  st      loudest 3 s short-term loudness (LUFS, K-weighted, as bank_curate.py)
  flat    hard-clip signature: runs of >= 3 equal samples above 0.5 (any stage clamping the waveform)
  crackle loudest 1 ms high-band (> 12 kHz) burst over the median of the held part (dB) - clicks / crackle
  gr      gain reduction the master limiter (ceiling -0.3 dBTP) needs for this render (dB, 0 = not working)

  python3 tools/presets/poly_audit.py --render BIN --wavdir DIR --out JSON [--only a.ohpepreset ...] [--quick]
  python3 tools/presets/poly_audit.py --report JSON [--top 25]

Rendering reuses the bank_curate.py render path (same binary, sample bank and measurement helpers). Project presets
are not chord-tested (their sequencers play their own notes); bank_curate.py analyse covers them.
"""
import argparse, json, math, os, subprocess, sys, tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bank_curate as bc  # noqa: E402

CHORDS = {
    "1": [60],
    "4close": [48, 52, 55, 60],
    "4high": [72, 76, 79, 84],
    "6": [48, 52, 55, 60, 64, 67],
    "8wide": [36, 43, 48, 52, 55, 60, 64, 67],
    "8close": [48, 52, 55, 59, 60, 62, 64, 67],
}
PLAY = [(100, 0), (127, 0), (127, 127)]  # (velocity, mod wheel)
HOLD = 3.0
CEILING = -0.3


def flat_runs(a):
    n = 0
    for c in a:
        big = np.abs(c) > 0.5
        same = np.concatenate([[False], np.diff(c) == 0.0]) & big
        # runs of >= 3 equal samples: two consecutive "same" flags
        n += int(np.sum(same[1:] & same[:-1]))
    return n


def crackle(a, sr):
    """Loudest 1 ms burst of the band above 12 kHz over its median in the held part (dB); 0 for a dark sound."""
    from scipy.signal import butter, sosfilt
    m = a.mean(axis=0)
    hp = sosfilt(butter(8, 12000, "hp", fs=sr, output="sos"), m)[int(0.3 * sr):int(HOLD * sr)]  # after the attack
    seg = m[int(0.3 * sr):int(HOLD * sr)]
    if len(hp) < sr // 10:
        return 0.0
    blk = int(0.001 * sr)
    e = (hp[: len(hp) // blk * blk].reshape(-1, blk) ** 2).mean(axis=1)
    med = float(np.median(e))
    total = float((seg ** 2).mean())
    if total <= 1e-10 or float(e.max()) < total * 1e-6:
        return 0.0  # nothing above 12 kHz within 60 dB of the sound: no audible crackle there
    return round(10 * math.log10(float(e.max()) / max(med, total * 1e-9)), 1)


def render(binary, wavdir, files, notes, vel, wheel, d):
    rows = {}
    for i in range(0, len(files), 25):
        cmd = [binary, "--bank", os.path.join(bc.ROOT, "resources", "samples", "bank.json"), "--wavdir", wavdir,
               "--out", d, "--bpm", "120", "--hold", str(HOLD), "--tail", "1.5", "--notes", ",".join(map(str, notes)),
               "--vel", str(vel), "--wheel", str(wheel)] + files[i:i + 25]
        res = subprocess.run(cmd, capture_output=True, text=True)
        for line in res.stdout.splitlines():
            if not line.startswith("{"):
                continue
            r = json.loads(line)
            wav = os.path.join(d, r["file"] + ".wav")
            if not os.path.exists(wav):
                continue
            a, sr = bc.read_wav(wav)
            os.remove(wav)
            tp = bc.true_peak(a)
            rows[r["file"]] = {"name": r["name"], "engine": r["engine"], "tp": round(tp, 2), "peak": r["peak"],
                               "st": round(bc.short_term_max(a, sr), 2), "flat": flat_runs(a), "crackle": crackle(a, sr),
                               "gr": round(max(0.0, tp - CEILING), 2), "nonfinite": r["nonfinite"]}
        if res.returncode != 0:
            sys.stderr.write(res.stderr[-1000:])
    return rows


def audit(args):
    bank = bc.load_bank()
    only = set(args.only or [])
    files = [os.path.join(bc.BANK, f) for f in sorted(bank) if bank[f]["kind"] == "part" and (not only or f in only)]
    chords = ["1", "4close", "8wide"] if args.quick else list(CHORDS)
    play = [(127, 0)] if args.quick else PLAY
    out = {}
    with tempfile.TemporaryDirectory() as d:
        for c in chords:
            for vel, wheel in play:
                key = f"{c}/v{vel}/w{wheel}"
                for f, r in render(args.render, args.wavdir, files, CHORDS[c], vel, wheel, d).items():
                    out.setdefault(f, {})[key] = r
                print(key, file=sys.stderr, flush=True)
    json.dump(out, open(args.out, "w"), indent=0)


def report(args):
    data = json.load(open(args.report))
    rows = []
    for f, cfg in data.items():
        worst = max(cfg.items(), key=lambda kv: kv[1]["tp"])
        single = max((v["st"] for k, v in cfg.items() if k.startswith("1/")), default=None)
        rows.append((worst[1]["tp"], f, worst[0], worst[1], single, max(v["flat"] for v in cfg.values()),
                     max(v["crackle"] for v in cfg.values())))
    rows.sort(reverse=True)
    over = sum(1 for r in rows if r[0] > -1.0)
    print(f"{len(rows)} part presets, {over} above -1 dBTP in some chord render, "
          f"{sum(1 for r in rows if r[5] > 0)} with flat-top runs")
    print("worst | preset | config | tp dBTP | limiter GR dB | single-note ST LUFS | flat | crackle dB")
    for tp, f, k, v, st1, flat, cr in rows[: args.top]:
        print(f"{tp:6.2f} | {v['name']} ({f}) | {k} | {v['tp']} | {v['gr']} | {st1} | {flat} | {cr}")


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--render")
    a.add_argument("--wavdir")
    a.add_argument("--out")
    a.add_argument("--only", nargs="*")
    a.add_argument("--quick", action="store_true", help="1 / 4 / 8 notes at velocity 127 only")
    a.add_argument("--report")
    a.add_argument("--top", type=int, default=25)
    args = a.parse_args()
    if args.report:
        report(args)
    else:
        audit(args)


if __name__ == "__main__":
    main()
