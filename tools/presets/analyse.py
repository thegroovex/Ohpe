#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Quality loop for the factory sounds: renders every factory preset with ohpe_render, checks
level, peak, DC, tail, denormals and high-frequency content, and (with --levels) writes the
part gains (dB) that bring each preset to its category's loudness target into levels.json.

  python3 tools/presets/analyse.py --render build-core/tests/core/ohpe_render --wavdir DIR \
      [--out renders/] [--levels] [--spectra]
"""
import argparse, glob, json, math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))

# loudness targets (active RMS, dBFS) per main category; the window allowed is -18 ... -12 dBFS
TARGET = {"Bass": -14.5, "Keys": -16.0, "Pads": -16.5, "Leads": -15.0, "Plucks": -16.0, "Drums": -15.0, "FX": -16.5,
          "Arp": -16.0, "Textures": -17.0, "Vocal": -16.0}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--wavdir", required=True)
    ap.add_argument("--out", default="")
    ap.add_argument("--levels", action="store_true")
    ap.add_argument("--spectra", action="store_true")
    ap.add_argument("files", nargs="*")
    a = ap.parse_args()
    files = a.files or sorted(glob.glob(os.path.join(ROOT, "resources", "presets", "factory", "*.ohpepreset")))
    cmd = [a.render, "--bank", os.path.join(ROOT, "resources", "samples", "bank.json"), "--wavdir", a.wavdir]
    if a.out:
        cmd += ["--out", a.out]
    res = subprocess.run(cmd + files, capture_output=True, text=True)
    sys.stderr.write(res.stderr)
    rows = [json.loads(l) for l in res.stdout.splitlines() if l.startswith("{")]
    levels_path = os.path.join(HERE, "levels.json")
    levels = json.load(open(levels_path)) if os.path.exists(levels_path) else {}
    problems = 0
    for r in rows:
        flags = []
        if r["kind"] == "part":
            target = TARGET.get(r["category"], -16.0)
            delta = target - r["active"]
            limited = r["peak"] > -1.6 and delta > 0  # transient sound held back by the peak limit
            if (abs(delta) > 1.0 and not limited) or not (-21.0 <= r["active"] <= -11.0):
                flags.append("level %+.1f" % delta)
        else:
            delta = 0.0
        if r["peak"] > -1.0: flags.append("PEAK")
        if abs(r["dc"]) > max(3e-4, 0.02 * 10 ** (r["rms"] / 20.0)): flags.append("DC")
        if r["tail"] > -60 and r["kind"] == "part": flags.append("TAIL %.0f" % r["tail"])
        if r["hf"] > -30: flags.append("HF %.0f" % r["hf"])
        if r["nonfinite"] or r["denormals"]: flags.append("NUM")
        if flags: problems += 1
        print("%-26s %-8s %-9s act %6.1f pk %5.1f tail %6.1f hf %6.1f  %s" % (r["name"][:26], r["category"], r["engine"], r["active"], r["peak"], r["tail"], r["hf"], " ".join(flags)))
        if a.levels and r["kind"] == "part":
            # current volume from the preset file
            p = json.load(open(next(f for f in files if os.path.splitext(os.path.basename(f))[0] == r["file"])))
            gain = p["params"].get("gain", 0.0)
            delta = min(delta, -1.2 - r["peak"])  # never push the peak above -1.2 dBFS
            levels[r["name"]] = round(min(24.0, max(-24.0, gain + delta)), 1)
    print("%d presets, %d with remarks" % (len(rows), problems))
    if a.levels:
        with open(levels_path, "w") as f:
            json.dump(levels, f, indent=1, sort_keys=True)
            f.write("\n")
    if a.spectra and a.out:
        spectra(a.out, rows)


def spectra(out, rows):
    import numpy as np, soundfile as sf
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    n = len(rows)
    cols = 4
    for page in range(0, n, 16):
        chunk = rows[page:page + 16]
        fig, axes = plt.subplots(4, cols, figsize=(16, 12))
        for ax, r in zip(axes.flat, chunk):
            x, sr = sf.read(os.path.join(out, r["file"] + ".wav"))
            m = x.mean(axis=1)
            ax.specgram(m, NFFT=2048, Fs=sr, noverlap=1024, cmap="magma", vmin=-100, vmax=-10)
            ax.set_ylim(0, 20000)
            ax.set_title("%s (%s) %.1f" % (r["name"], r["engine"], r["active"]), fontsize=8)
            ax.tick_params(labelsize=6)
        for ax in list(axes.flat)[len(chunk):]:
            ax.axis("off")
        fig.tight_layout()
        fig.savefig(os.path.join(out, "spectra-%02d.png" % (page // 16)), dpi=60)
        plt.close(fig)


if __name__ == "__main__":
    main()
