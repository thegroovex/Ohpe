#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
p65 (1.0 bank curation): checks and curates the factory preset bank (resources/presets/factory).

  python3 tools/presets/bank_curate.py check                      # static checks (names, tags, banned words)
  python3 tools/presets/bank_curate.py analyse --render BIN --wavdir DIR [--out JSON]
                                                                  # renders every preset, loudness + similarity
  python3 tools/presets/bank_curate.py apply                      # applies tools/presets/bank_curation.json

Rendering: every part preset plays the standard phrase of its main category (ohpe_render, 120 bpm, 8 s, master
limiter off) - the same phrase for every preset of a category, so presets of one category compare 1:1. Project
presets run their sequencers for 8 s.

Loudness target (documented in docs/en/PRESETS-BANK.md / docs/de/PRESETS-BANK.md): the loudest 3 s window of the
render (EBU R128 short-term loudness, K-weighted) at -16 LUFS, tolerance +-1.5 LU, true peak at or below -1.5 dBTP
(peak-safe: a preset that would pass -1.5 dBTP at the target stays quieter). Only the part's level parameter
("gain", -24..24 dB) is changed; the sound itself is untouched.

Near-duplicates: same engine and main category, parameter distance (mean normalised difference over the union of
the numeric parameters) below PARAM_DIST, and audio similarity (band spectrum + envelope, cosine) above AUDIO_SIM.

bank_curation.json is the record of the curation (removed files, display renames, tag fixes, level changes). The
older generators (make_factory.py, make_showcase.py, ...) write the bank as it was before p65; after running one of
them, run "apply" again so the curated bank is restored. Removed presets keep their uid in "retired" (sessions that
name a retired uid keep their sound - it is stored in the session - and show no preset name).
"""
import argparse, glob, json, math, os, re, subprocess, sys, tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BANK = os.path.join(ROOT, "resources", "presets", "factory")
CURATION = os.path.join(HERE, "bank_curation.json")
sys.path.insert(0, HERE)

TARGET_LUFS = -16.0   # short-term max (3 s), K-weighted
TOLERANCE = 1.5       # LU: inside +-TOLERANCE the level stays as it is
PEAK_MAX = -1.5       # dBTP
GAIN_MIN, GAIN_MAX = -24.0, 24.0
PARAM_DIST = 0.06
AUDIO_SIM = 0.985

# no third-party makers, models, brands, artists or host programs in names, tags or comments
BANNED = r"\b(moog|roland|korg|yamaha|oberheim|sequential|prophet|juno|jupiter|minimoog|dx7|dx-7|tb-?303|tr-?808|tr-?909|" \
         r"808|909|303|rhodes|wurli\w*|hammond|leslie|clavinet|mellotron|solina|arp ?2600|odyssey|nord|access virus|" \
         r"serum|massive|sylenth|ableton|logic pro|cubase|fl studio|bitwig|reaper|pro tools|kraftwerk|vangelis|" \
         r"daft punk|eno|zimmer|steinway|bösendorfer|fender|gibson|casio|ensoniq|emu|e-mu|akai|mpc|elektron|" \
         r"teenage engineering|op-1|buchla|eurorack|behringer|arturia|novation|waldorf|ppg|fairlight|synclavier)\b"


def load_bank():
    out = {}
    for f in sorted(glob.glob(os.path.join(BANK, "*.ohpepreset"))):
        out[os.path.basename(f)] = json.load(open(f, encoding="utf-8"))
    return out


def save(fname, p):
    with open(os.path.join(BANK, fname), "w", encoding="utf-8") as f:
        json.dump(p, f, indent=2, ensure_ascii=False)
        f.write("\n")


def main_category(p):
    from_core = ["Bass", "Keys", "Pads", "Leads", "Plucks", "Drums", "FX", "Arp", "Textures", "Vocal", "Boom", "Hybrid",
                 "Piano", "3D"]
    for t in p.get("tags", []):
        if t in from_core:
            return t
    return ""


def check(bank):
    problems = []
    seen = {}
    for f, p in bank.items():
        n = p["name"].strip().lower()
        if n in seen:
            problems.append(("dup-name", f, seen[n], p["name"]))
        seen[n] = f
        text = " ".join([p["name"], p.get("comment", "")] + p.get("tags", []))
        m = re.search(BANNED, text, re.I)
        if m:
            problems.append(("banned-word", f, m.group(0), p["name"]))
        if not main_category(p):
            problems.append(("no-category", f, p.get("tags"), p["name"]))
        if len(p["name"]) > 22:
            problems.append(("long-name", f, len(p["name"]), p["name"]))
        if p["name"] != p["name"].strip() or "  " in p["name"]:
            problems.append(("spaces", f, "", p["name"]))
        pre = f.split("-")[0]
        cats = {"arp": "Arp", "bass": "Bass", "keys": "Keys", "pads": "Pads", "leads": "Leads", "plucks": "Plucks",
                "drums": "Drums", "fx": "FX", "textures": "Textures", "vocal": "Vocal", "boom": "Boom", "piano": "Piano",
                "hybrid": "Hybrid", "3d": "3D"}
        if pre in cats and cats[pre] not in p.get("tags", []):
            problems.append(("file-prefix-tag", f, pre, p.get("tags")))
    return problems


def numeric_params(p):
    d = {}
    for k, v in p.get("params", {}).items():
        if isinstance(v, (int, float)) and k != "gain":
            d[k] = float(v)
    return d


def param_distances(bank):
    parts = {f: numeric_params(p) for f, p in bank.items() if p["kind"] == "part"}
    lo, hi = {}, {}
    for d in parts.values():
        for k, v in d.items():
            lo[k] = min(lo.get(k, v), v)
            hi[k] = max(hi.get(k, v), v)
    files = sorted(parts)
    res = {}
    for i, a in enumerate(files):
        for b in files[i + 1:]:
            pa, pb = bank[a], bank[b]
            if pa.get("engine") != pb.get("engine"):
                continue
            da, db = parts[a], parts[b]
            keys = set(da) | set(db)
            s = 0.0
            for k in keys:
                r = hi[k] - lo[k]
                if k not in da or k not in db:
                    s += 0.5
                elif r > 0:
                    s += abs(da[k] - db[k]) / r
                elif da[k] != db[k]:
                    s += 1.0
            # arp / morph blocks differ = clearly different
            if json.dumps(pa.get("arp"), sort_keys=True) != json.dumps(pb.get("arp"), sort_keys=True):
                s += 2.0
            res[(a, b)] = s / max(1, len(keys))
    return res


# ---------------------------------------------------------------- rendering

def short_term_max(a, sr):
    from showcase_cal import kweight
    k = np.vstack([kweight(c, sr) for c in a])
    p = (k ** 2).sum(axis=0)
    blk, hop = int(3.0 * sr), int(0.1 * sr)
    cs = np.concatenate([[0.0], np.cumsum(p)])
    if len(p) <= blk:
        m = cs[-1] / max(1, len(p))
        return -0.691 + 10 * math.log10(m) if m > 0 else -200.0
    ms = (cs[blk::hop] - cs[:len(cs) - blk:hop][: len(cs[blk::hop])]) / blk
    m = float(np.max(ms))
    return -0.691 + 10 * math.log10(m) if m > 0 else -200.0


def true_peak(a):
    tp = 0.0
    for c in a:
        up = np.fft.irfft(np.fft.rfft(c), 4 * len(c)) * 4
        tp = max(tp, float(np.max(np.abs(up))))
    return 20 * math.log10(tp) if tp > 0 else -200.0


def features(a, sr):
    m = a.mean(axis=0)
    n = 2048
    frames = [m[i:i + n] * np.hanning(n) for i in range(0, len(m) - n, n // 2)]
    if not frames:
        return np.zeros(48)
    spec = np.abs(np.fft.rfft(np.array(frames), axis=1)) ** 2
    freqs = np.fft.rfftfreq(n, 1 / sr)
    edges = np.geomspace(30, 18000, 33)
    bands = np.array([spec[:, (freqs >= edges[i]) & (freqs < edges[i + 1])].sum(axis=1) for i in range(32)]).T
    bspec = 10 * np.log10(bands.mean(axis=0) + 1e-12)
    bspec = bspec - bspec.max()
    env = 10 * np.log10(spec.sum(axis=1) + 1e-12)
    env = np.interp(np.linspace(0, len(env) - 1, 16), np.arange(len(env)), env)
    env = env - env.max()
    v = np.concatenate([np.maximum(bspec, -60) / 60.0, np.maximum(env, -60) / 60.0])
    return v


def read_wav(path):
    from showcase_cal import read_wav as rw
    return rw(path)


def analyse(args):
    bank = load_bank()
    files = [os.path.join(BANK, f) for f in sorted(bank)]
    rows = {}
    with tempfile.TemporaryDirectory() as d:
        for i in range(0, len(files), 20):
            chunk = files[i:i + 20]
            cmd = [args.render, "--bank", os.path.join(ROOT, "resources", "samples", "bank.json"), "--wavdir", args.wavdir,
                   "--out", d, "--seconds", "8", "--bpm", "120"] + chunk
            res = subprocess.run(cmd, capture_output=True, text=True)
            for line in res.stdout.splitlines():
                if not line.startswith("{"):
                    continue
                r = json.loads(line)
                wav = os.path.join(d, r["file"] + ".wav")
                if os.path.exists(wav):
                    a, sr = read_wav(wav)
                    r["st"] = round(short_term_max(a, sr), 2)
                    r["tp"] = round(true_peak(a), 2)
                    r["feat"] = [round(float(x), 4) for x in features(a, sr)]
                    if args.keep:
                        os.makedirs(args.keep, exist_ok=True)
                        os.replace(wav, os.path.join(args.keep, r["file"] + ".wav"))
                rows[r["file"] + ".ohpepreset"] = r
            if res.returncode != 0:
                sys.stderr.write(res.stderr[-2000:])
            print(f"{min(i + 20, len(files))}/{len(files)}", file=sys.stderr, flush=True)
    json.dump(rows, open(args.out, "w"), indent=0)


def report(args):
    bank = load_bank()
    rows = json.load(open(args.data))
    pd = param_distances(bank)
    out = []
    for (a, b), dist in sorted(pd.items(), key=lambda x: x[1]):
        if a not in rows or b not in rows or "feat" not in rows[a] or "feat" not in rows[b]:
            continue
        if main_category(bank[a]) != main_category(bank[b]):
            continue
        fa, fb = np.array(rows[a]["feat"]), np.array(rows[b]["feat"])
        sim = float(fa @ fb / (np.linalg.norm(fa) * np.linalg.norm(fb) + 1e-12))
        if dist < args.pdist or sim > args.asim:
            out.append((a, b, round(dist, 3), round(sim, 4)))
    for o in out:
        print("pair", *o)
    for f, r in sorted(rows.items()):
        flags = []
        if r.get("nonfinite") or r.get("denormals"):
            flags.append("nonfinite")
        if r.get("active", 0) < -40:
            flags.append("silent")
        if r.get("tp", -99) > PEAK_MAX:
            flags.append("peak")
        if r.get("hf", -99) > -3:
            flags.append("hf")
        if flags:
            print("flag", f, flags, r.get("active"), r.get("tp"), r.get("hf"))


def apply_curation(_args):
    cur = json.load(open(CURATION, encoding="utf-8"))
    for f in cur.get("removed", {}):
        path = os.path.join(BANK, f)
        if os.path.exists(path):
            os.remove(path)
    bank = load_bank()
    for f, ch in cur.get("changed", {}).items():
        if f not in bank:
            continue
        p = bank[f]
        if "name" in ch:
            p["name"] = ch["name"]
        if "tags" in ch:
            p["tags"] = ch["tags"]
        if "comment" in ch:
            p["comment"] = ch["comment"]
        if "gain" in ch:
            p["params"]["gain"] = ch["gain"]
        save(f, p)


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    sp.add_parser("check")
    a = sp.add_parser("analyse")
    a.add_argument("--render", required=True)
    a.add_argument("--wavdir", required=True)
    a.add_argument("--out", default="bank-analysis.json")
    a.add_argument("--keep", default="")
    r = sp.add_parser("report")
    r.add_argument("--data", default="bank-analysis.json")
    r.add_argument("--pdist", type=float, default=PARAM_DIST)
    r.add_argument("--asim", type=float, default=AUDIO_SIM)
    sp.add_parser("apply")
    args = ap.parse_args()
    if args.cmd == "check":
        probs = check(load_bank())
        for p in probs:
            print(*p)
        sys.exit(1 if any(p[0] in ("dup-name", "banned-word", "no-category") for p in probs) else 0)
    elif args.cmd == "analyse":
        analyse(args)
    elif args.cmd == "report":
        report(args)
    else:
        apply_curation(args)


if __name__ == "__main__":
    main()
