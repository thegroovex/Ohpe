#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Builds the factory sample bank (0.4, package F): the drum kits Tight, Room, Lo-Fi and Hybrid and
the Sampler / Grain instruments, from public-domain (CC0) recordings, plus Ohpe's own synthesised
drum layers (hybrid kit). Writes resources/samples/** (16-bit FLAC) and resources/samples/bank.json.

Sources (both CC0 1.0, pinned revisions; see THIRD-PARTY.md):
  VD   = github.com/sfzinstruments/virtuosity_drums @ 9f04cf9a734527edfbb0a4eee1f674e45bbf71bc
  VCSL = github.com/sgossner/VCSL                   @ c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e

Needs python3 with numpy, scipy and soundfile, and ffmpeg (FLAC encoding). Downloads only the
files it uses into --cache (default ./sample-cache).

  python3 tools/samples/build_samples.py [--cache DIR] [--wav-cache DIR] [--only kits|instruments]

--wav-cache also writes 32-bit float WAV copies of every bank file (and of the original studio
kit), which the core-only render tool (ohpe_render --wavdir) reads.
"""
import argparse, json, os, re, subprocess, sys, urllib.parse, urllib.request
from concurrent.futures import ThreadPoolExecutor
import numpy as np
import soundfile as sf
from scipy import signal

VD_REV = "9f04cf9a734527edfbb0a4eee1f674e45bbf71bc"
VCSL_REV = "c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e"
VD_URL = "https://raw.githubusercontent.com/sfzinstruments/virtuosity_drums/%s/" % VD_REV
VCSL_URL = "https://raw.githubusercontent.com/sgossner/VCSL/%s/" % VCSL_REV
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "resources", "samples")
KIT_RATE = 44100

# ------------------------------------------------------------------ sources
def vd(mic, piece, art, vl, rr=None):
    name = "%s_%s_vl%d%s.flac" % (mic, art, vl, "_rr%d" % rr if rr else "")
    return ("VD", "Samples/%s/%s/%s" % (mic, piece, name))

PERC = {  # (VD folder, file stem); the mic suffix is _Close / _Far
    "cowbell": ("cowbell", "Cowbell1_Normal_v2_rr1"), "tambourine": ("tambourine", "Tamb1_Shake_rr1"),
    "shaker": ("shaker", "LShaker_Shake1D_rr1"), "claves": ("claves", "Claves1_Hit_v2_rr1"),
    "conga": ("conga", "Conga_22_HitN_101_127_rr1"), "bongo": ("bongoh", "BongoH_Hit1_v2_rr1"),
}
def perc(name, far=False):
    folder, stem = PERC[name]
    return ("VD", "Samples/perc/%s/%s/%s_%s.wav" % ("room" if far else "close", folder, stem, "Far" if far else "Close"))

def vcsl(path):
    return ("VCSL", path)

CLAPS = "Idiophones/Struck Idiophones/Claps/"

def url(src):
    repo, path = src
    return (VD_URL if repo == "VD" else VCSL_URL) + urllib.parse.quote(path)

def local(cache, src):
    return os.path.join(cache, src[0], src[1])

def fetch_all(cache, sources):
    todo = [s for s in set(sources) if not os.path.exists(local(cache, s))]
    def get(s):
        dst = local(cache, s)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        for attempt in range(4):
            try:
                with urllib.request.urlopen(url(s), timeout=60) as r, open(dst + ".part", "wb") as f:
                    f.write(r.read())
                os.replace(dst + ".part", dst)
                return None
            except Exception as e:  # retry
                err = e
        return "%s: %s" % (s[1], err)
    with ThreadPoolExecutor(8) as ex:
        errors = [e for e in ex.map(get, todo) if e]
    if errors:
        sys.exit("download failed:\n" + "\n".join(errors))
    print("sources: %d files (%d downloaded)" % (len(set(sources)), len(todo)))

# ------------------------------------------------------------------ audio helpers
def load(cache, src, rate):
    x, sr = sf.read(local(cache, src), dtype="float64", always_2d=True)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    x = x[:, :2]
    if sr != rate:
        g = np.gcd(int(sr), int(rate))
        x = signal.resample_poly(x, rate // g, int(sr) // g, axis=0)
    return x

def db(g):
    return 10 ** (g / 20.0)

def onset(x, rel_db=-30.0):
    env = np.max(np.abs(x), axis=1)
    thr = env.max() * db(rel_db)
    idx = np.argmax(env > thr)
    return max(0, idx - int(0.0015 * KIT_RATE))

def fit(x, n):
    if len(x) >= n:
        return x[:n]
    return np.vstack([x, np.zeros((n - len(x), x.shape[1]))])

def finish(x, secs, rate, fade_frac=0.35, start=None):
    if start is None:
        start = onset(x)
    x = x[start:]
    n = int(secs * rate)
    x = fit(x, n)
    fade = max(int(0.05 * rate), int(n * fade_frac))
    env = np.ones(n)
    env[-fade:] = np.cos(np.linspace(0, np.pi / 2, fade)) ** 2
    fin = min(n, int(0.0008 * rate))
    env[:fin] *= np.linspace(0, 1, fin)
    x = x * env[:, None]
    x -= np.mean(x, axis=0)  # no DC
    return x

def normalise(x, peak_db=-1.0):
    p = np.max(np.abs(x))
    return x * (db(peak_db) / p) if p > 0 else x

def mixsrc(cache, srcs, rate):
    """srcs: [(source, gain dB, delay ms)] -> mixed stereo at `rate`, aligned at the first one's start."""
    parts = []
    for s in srcs:
        src, g = s[0], s[1]
        delay = s[2] if len(s) > 2 else 0.0
        x = load(cache, src, rate) * db(g)
        if delay:
            x = np.vstack([np.zeros((int(delay * rate / 1000), 2)), x])
        parts.append(x)
    n = max(len(p) for p in parts)
    return sum(fit(p, n) for p in parts)

def mix(*xs):
    n = max(len(x) for x in xs)
    return sum(fit(x, n) for x in xs)

def hp(x, f, rate, order=2):
    sos = signal.butter(order, f, "highpass", fs=rate, output="sos")
    return signal.sosfilt(sos, x, axis=0)

def lp(x, f, rate, order=2):
    sos = signal.butter(order, f, "lowpass", fs=rate, output="sos")
    return signal.sosfilt(sos, x, axis=0)

def bp(x, lo, hi, rate, order=2):
    sos = signal.butter(order, [lo, hi], "bandpass", fs=rate, output="sos")
    return signal.sosfilt(sos, x, axis=0)

def stereo(m):
    return np.stack([m, m], axis=1)

RNG = np.random.default_rng(20261015)

def noise(n):
    return RNG.standard_normal(n)

def room_tail(x, rate, secs=0.6, level_db=-14.0):
    """A short diffuse tail: x convolved with decaying stereo noise (Ohpe's own processing)."""
    n = int(secs * rate)
    t = np.arange(n) / rate
    ir = np.stack([noise(n), noise(n)], axis=1) * np.exp(-t * 6.9 / secs)[:, None]
    ir = lp(ir, 6000, rate)
    ir /= np.sqrt(np.sum(ir ** 2, axis=0, keepdims=True))
    wet = np.stack([signal.fftconvolve(x[:, c], ir[:, c]) for c in range(2)], axis=1)
    return fit(x, len(wet)) + wet * db(level_db)

def lofi(x, rate, cutoff=7000, bits=10, drive=1.8):
    """Lo-fi treatment: band-limit, soft saturation, coarse quantisation, gentle wobble."""
    y = lp(hp(x, 45, rate), cutoff, rate, order=4)
    y = np.tanh(y * drive / max(1e-9, np.max(np.abs(y)))) / np.tanh(drive)
    q = 2 ** (bits - 1)
    y = np.round(y * q + RNG.uniform(-0.5, 0.5, y.shape)) / q
    return lp(y, cutoff * 1.1, rate)

# ------------------------------------------------------------------ Ohpe's own synthesised layers (hybrid kit)
def env_exp(n, rate, secs):
    return np.exp(-np.arange(n) / rate * 6.9 / secs)

def sweep_sine(rate, f0, f1, pitch_secs, amp_secs, length):
    n = int(length * rate)
    t = np.arange(n) / rate
    f = f1 + (f0 - f1) * np.exp(-t / pitch_secs)
    ph = 2 * np.pi * np.cumsum(f) / rate
    return np.sin(ph) * env_exp(n, rate, amp_secs)

def synth_kick(rate, f0=165, f1=46, pitch=0.035, amp=0.5, length=0.9, click=0.35):
    n = int(length * rate)
    body = sweep_sine(rate, f0, f1, pitch, amp, length)
    c = hp(noise(n) * env_exp(n, rate, 0.004), 1500, rate) * click
    return stereo(np.tanh(1.6 * (body + c)) / np.tanh(1.6))

def synth_tom(rate, f0, f1, amp, length=1.0):
    n = int(length * rate)
    body = sweep_sine(rate, f0, f1, 0.06, amp, length) + 0.35 * sweep_sine(rate, f0 * 1.52, f1 * 1.5, 0.05, amp * 0.6, length)
    c = bp(noise(n), 800, 5000, rate) * env_exp(n, rate, 0.012) * 0.25
    return stereo(body + c)

HAT_FREQS = [263.0, 400.0, 421.0, 474.0, 587.0, 845.0]

def synth_metal(rate, length, decay, lo=6500, hi=15000, freqs=HAT_FREQS):
    n = int(length * rate)
    t = np.arange(n) / rate
    m = sum(signal.square(2 * np.pi * f * t + RNG.uniform(0, 6.28)) for f in freqs)
    m = bp(m, lo, min(hi, rate / 2 - 500), rate, order=3)
    m += 0.25 * hp(noise(n), 9000, rate)
    return stereo(m * env_exp(n, rate, decay))

def synth_snare(rate, length=0.6, tone=190, decay=0.2):
    n = int(length * rate)
    body = sweep_sine(rate, tone * 1.3, tone, 0.02, 0.09, length) + 0.5 * sweep_sine(rate, tone * 2.1, tone * 1.8, 0.02, 0.06, length)
    nz = bp(noise(n), 1200, 9000, rate) * env_exp(n, rate, decay)
    return stereo(0.7 * body + nz)

def synth_cowbell(rate, length=0.6):
    n = int(length * rate)
    t = np.arange(n) / rate
    m = signal.square(2 * np.pi * 562 * t) + signal.square(2 * np.pi * 845 * t)
    m = bp(m, 450, 2500, rate, order=2) * (0.65 * env_exp(n, rate, 0.04) + 0.35 * env_exp(n, rate, 0.35))
    return stereo(m)

def synth_click(rate, f, decay, length=0.3):
    n = int(length * rate)
    return stereo(np.sin(2 * np.pi * f * np.arange(n) / rate) * env_exp(n, rate, decay))

# ------------------------------------------------------------------ kits
# Every kit has the same pad layout as the original kit (patterns play on any kit).
PADS = ["01-kick", "02-kick-snares", "03-snare", "04-rimshot", "05-clap", "06-hat-closed", "07-hat-open", "08-hat-pedal",
        "09-tom-low", "10-tom-high", "11-tom-floor", "12-cross-stick", "13-ride", "14-ride-bell", "15-crash",
        "16-crash-sizzle", "17-hat-half", "18-snare-ghost", "19-cowbell", "20-tambourine", "21-shaker", "22-claves",
        "23-conga", "24-bongo"]

def kit_tight():
    K = lambda art, vl: [(vd("kickmic", "kick", art, vl, 1), 0), (vd("mid", "kick", art, vl, 1), -12)]
    S = lambda art, vl: [(vd("snaremic", "snare", art, vl), 0), (vd("mid", "snare", art, vl), -9)]
    M = lambda piece, art, vl, rr=1: [(vd("mid", piece, art, vl, rr), 0)]
    T = lambda piece, art, vl: [(vd("mid", piece, art, vl), 0)]
    return {
        "01-kick": (K("kick_snoff", 4), 0.55), "02-kick-snares": (K("kick_snon", 3), 0.6),
        "03-snare": (S("snare_center", 24), 0.45), "04-rimshot": (S("snare_rimshot", 10), 0.5),
        "05-clap": ([(vcsl(CLAPS + "SoloClap_vl3.wav"), 0), (vcsl(CLAPS + "SoloClap_vl4.wav"), -3, 7)], 0.35),
        "06-hat-closed": (M("hh", "hh_closed", 3), 0.25), "07-hat-open": (M("hh", "hh_open", 3), 1.1),
        "08-hat-pedal": (M("hh", "hh_pedal", 2), 0.3), "09-tom-low": (T("ltom", "ltom_muted", 12), 0.6),
        "10-tom-high": (T("htom", "htom_center", 11), 0.6), "11-tom-floor": (T("ltom", "ltom_halfopen", 12), 0.75),
        "12-cross-stick": (S("snare_crossstick", 12), 0.3), "13-ride": (M("ride", "ride_ride", 2), 1.8),
        "14-ride-bell": (M("ride", "ride_bell", 2), 1.5), "15-crash": (M("crash", "crash_crash", 2), 2.0),
        "16-crash-sizzle": (M("crash", "crash_sizzle", 2), 2.0), "17-hat-half": (M("hh", "hh_half", 3), 0.6),
        "18-snare-ghost": (S("snare_center", 6), 0.3), "19-cowbell": ([(perc("cowbell"), 0)], 0.5),
        "20-tambourine": ([(perc("tambourine"), 0)], 0.6), "21-shaker": ([(perc("shaker"), 0)], 0.4),
        "22-claves": ([(perc("claves"), 0)], 0.3), "23-conga": ([(perc("conga"), 0)], 0.6),
        "24-bongo": ([(perc("bongo"), 0)], 0.5),
    }

def kit_room():
    K = lambda art, vl: [(vd("room", "kick", art, vl, 1), 0), (vd("kickmic", "kick", art, vl, 1), -7), (vd("oh", "kick", art, vl, 1), -5)]
    S = lambda art, vl: [(vd("room", "snare", art, vl), 0), (vd("oh", "snare", art, vl), -4), (vd("snaremic", "snare", art, vl), -11)]
    R = lambda piece, art, vl, rr=1: [(vd("room", piece, art, vl, rr), 0), (vd("oh", piece, art, vl, rr), -3)]
    T = lambda piece, art, vl: [(vd("room", piece, art, vl), 0), (vd("oh", piece, art, vl), -3)]
    return {
        "01-kick": (K("kick_snoff", 3), 1.4), "02-kick-snares": (K("kick_snon", 4), 1.5),
        "03-snare": (S("snare_center", 22), 1.6), "04-rimshot": (S("snare_rimshot", 10), 1.6),
        "05-clap": ("room-clap", 1.1),
        "06-hat-closed": (R("hh", "hh_closed", 3), 0.6), "07-hat-open": (R("hh", "hh_open", 3), 2.4),
        "08-hat-pedal": (R("hh", "hh_pedal", 2), 0.7), "09-tom-low": (T("ltom", "ltom_center", 12), 2.2),
        "10-tom-high": (T("htom", "htom_center", 12), 2.0), "11-tom-floor": (T("ltom", "ltom_center", 14), 2.4),
        "12-cross-stick": (S("snare_crossstick", 12), 0.9), "13-ride": (R("ride", "ride_ride", 2), 3.2),
        "14-ride-bell": (R("ride", "ride_bell", 2), 2.8), "15-crash": (R("crash", "crash_crash", 2), 3.6),
        "16-crash-sizzle": (R("crash", "crash_sizzle", 2), 3.6), "17-hat-half": (R("hh", "hh_half", 3), 1.2),
        "18-snare-ghost": (S("snare_center", 7), 1.0), "19-cowbell": ([(perc("cowbell", True), 0)], 0.9),
        "20-tambourine": ([(perc("tambourine", True), 0)], 0.9), "21-shaker": ([(perc("shaker", True), 0)], 0.6),
        "22-claves": ([(perc("claves", True), 0)], 0.6), "23-conga": ([(perc("conga", True), 0)], 1.0),
        "24-bongo": ([(perc("bongo", True), 0)], 0.8),
    }

def kit_lofi():
    L = lambda piece, art, vl, rr=None: [(vd("lofi", piece, art, vl, rr), 0)]
    return {
        "01-kick": (L("kick", "kick_snoff", 3, 1), 0.8), "02-kick-snares": (L("kick", "kick_snon", 4, 1), 0.9),
        "03-snare": (L("snare", "snare_center", 20), 0.8), "04-rimshot": (L("snare", "snare_rimshot", 10), 0.8),
        "05-clap": ([(vcsl(CLAPS + "Clap_rr2.wav"), 0), (vcsl(CLAPS + "Clap_rr4.wav"), -2, 11)], 0.6),
        "06-hat-closed": (L("hh", "hh_closed", 3, 1), 0.3), "07-hat-open": (L("hh", "hh_open", 3, 1), 1.4),
        "08-hat-pedal": (L("hh", "hh_pedal", 2, 1), 0.4), "09-tom-low": (L("ltom", "ltom_center", 12), 1.2),
        "10-tom-high": (L("htom", "htom_center", 12), 1.1), "11-tom-floor": (L("ltom", "ltom_center", 14), 1.3),
        "12-cross-stick": (L("snare", "snare_crossstick", 12), 0.5), "13-ride": (L("ride", "ride_ride", 2, 1), 2.0),
        "14-ride-bell": (L("ride", "ride_bell", 2, 1), 1.8), "15-crash": (L("crash", "crash_crash", 2, 1), 2.2),
        "16-crash-sizzle": (L("crash", "crash_sizzle", 2, 1), 2.2), "17-hat-half": (L("hh", "hh_half", 3, 1), 0.7),
        "18-snare-ghost": (L("snare", "snare_center", 7), 0.5), "19-cowbell": ([(perc("cowbell"), 0)], 0.5),
        "20-tambourine": ([(perc("tambourine"), 0)], 0.6), "21-shaker": ([(perc("shaker"), 0)], 0.4),
        "22-claves": ([(perc("claves"), 0)], 0.3), "23-conga": ([(perc("conga"), 0)], 0.6),
        "24-bongo": ([(perc("bongo"), 0)], 0.5),
    }

def kit_hybrid():
    return {
        "01-kick": ("hy-kick", 0.7), "02-kick-snares": ("hy-boom", 1.3), "03-snare": ("hy-snare", 0.5),
        "04-rimshot": ("hy-rim", 0.25), "05-clap": ("hy-clap", 0.7), "06-hat-closed": ("hy-hat-c", 0.15),
        "07-hat-open": ("hy-hat-o", 0.8), "08-hat-pedal": ("hy-hat-p", 0.2), "09-tom-low": ("hy-tom-l", 0.8),
        "10-tom-high": ("hy-tom-h", 0.6), "11-tom-floor": ("hy-tom-f", 1.0),
        "12-cross-stick": ([(vd("snaremic", "snare", "snare_crossstick", 12), 0)], 0.3),
        "13-ride": ([(vd("mid", "ride", "ride_ride", 2, 1), 0)], 2.0),
        "14-ride-bell": ([(vd("mid", "ride", "ride_bell", 2, 1), 0)], 1.8),
        "15-crash": ([(vd("mid", "crash", "crash_crash", 2, 1), 0), (vd("oh", "crash", "crash_crash", 2, 1), -3)], 2.6),
        "16-crash-sizzle": ("hy-noise-crash", 2.0), "17-hat-half": ("hy-hat-h", 0.35),
        "18-snare-ghost": ("hy-ghost", 0.3), "19-cowbell": ("hy-cowbell", 0.5),
        "20-tambourine": ([(perc("tambourine"), 0)], 0.5), "21-shaker": ([(perc("shaker"), 0)], 0.35),
        "22-claves": ("hy-claves", 0.2), "23-conga": ("hy-conga", 0.5), "24-bongo": ("hy-bongo", 0.4),
    }

HYBRID_SOURCES = [vd("kickmic", "kick", "kick_snoff", 4, 1), vd("snaremic", "snare", "snare_center", 24),
                  vd("snaremic", "snare", "snare_center", 8), vd("snaremic", "snare", "snare_rimshot", 10),
                  vd("mid", "hh", "hh_closed", 3, 1), vd("mid", "hh", "hh_open", 3, 1),
                  vcsl(CLAPS + "Clap_rr1.wav"), vcsl(CLAPS + "Clap_rr3.wav"), vcsl(CLAPS + "Clap_rr5.wav")]
ROOM_CLAP = [vcsl(CLAPS + "Clap_rr1.wav"), vcsl(CLAPS + "Clap_rr2.wav"), vcsl(CLAPS + "Clap_rr6.wav")]

def special(cache, key, rate, secs):
    """Pads built from Ohpe's own synthesis (hybrid kit) or with extra processing."""
    def rec(src, secs_, hp_f=None, gain=0.0):
        x = load(cache, src, rate)
        x = finish(x, secs_, rate, 0.4)
        if hp_f: x = hp(x, hp_f, rate)
        return normalise(x, gain)
    n = int(secs * rate)
    if key == "room-clap":
        x = mixsrc(cache, [(ROOM_CLAP[0], 0), (ROOM_CLAP[1], -2, 9), (ROOM_CLAP[2], -4, 17)], rate)
        return room_tail(finish(x, 0.5, rate, 0.5), rate, 0.9, -9.0)
    if key == "hy-kick":
        return mix(synth_kick(rate), rec(HYBRID_SOURCES[0], 0.06, 900, -14.0))
    if key == "hy-boom":
        return synth_kick(rate, 120, 41, 0.05, 1.1, 1.4, 0.2)
    if key == "hy-snare":
        return mix(0.8 * synth_snare(rate), rec(HYBRID_SOURCES[1], 0.3, 150, -3.0))
    if key == "hy-ghost":
        return mix(0.6 * synth_snare(rate, 0.4, 200, 0.1), rec(HYBRID_SOURCES[2], 0.3, 150, -4.0))
    if key == "hy-rim":
        return mix(synth_click(rate, 1650, 0.012) * 0.7, synth_click(rate, 460, 0.02) * 0.6, rec(HYBRID_SOURCES[3], 0.08, 2000, -10.0))
    if key == "hy-clap":
        x = mixsrc(cache, [(HYBRID_SOURCES[6], 0), (HYBRID_SOURCES[7], -1, 8), (HYBRID_SOURCES[8], -2, 19)], rate)
        x = finish(x, 0.4, rate, 0.5)
        x = hp(x, 300, rate)
        return room_tail(x, rate, 0.5, -11.0)
    if key in ("hy-hat-c", "hy-hat-o", "hy-hat-p", "hy-hat-h"):
        decay = {"hy-hat-c": 0.045, "hy-hat-o": 0.5, "hy-hat-p": 0.07, "hy-hat-h": 0.16}[key]
        m = synth_metal(rate, secs, decay)
        acoustic = HYBRID_SOURCES[5] if key == "hy-hat-o" else HYBRID_SOURCES[4]
        return mix(m, rec(acoustic, secs, 3000, -12.0))
    if key == "hy-noise-crash":
        m = mix(synth_metal(rate, secs, 1.1, 4000, 16000), stereo(hp(noise(n), 5000, rate) * env_exp(n, rate, 0.9) * 0.6))
        return m
    if key == "hy-tom-l": return synth_tom(rate, 120, 82, 0.45)
    if key == "hy-tom-h": return synth_tom(rate, 190, 135, 0.35)
    if key == "hy-tom-f": return synth_tom(rate, 95, 62, 0.6, 1.2)
    if key == "hy-cowbell": return synth_cowbell(rate)
    if key == "hy-claves": return synth_click(rate, 2500, 0.03)
    if key == "hy-conga": return mix(stereo(sweep_sine(rate, 260, 205, 0.02, 0.22, 0.6)), 0.3 * synth_click(rate, 900, 0.006, 0.6))
    if key == "hy-bongo": return mix(stereo(sweep_sine(rate, 420, 340, 0.015, 0.15, 0.5)), 0.3 * synth_click(rate, 1400, 0.005, 0.5))
    raise KeyError(key)

KITS = {"tight": kit_tight, "room": kit_room, "lofi": kit_lofi, "hybrid": kit_hybrid}

def kit_sources(defs):
    out = []
    for spec, _ in defs.values():
        if isinstance(spec, list):
            out += [s[0] for s in spec]
    return out

def build_kit(cache, kit, defs):
    files = {}
    d = os.path.join(OUT, "kits", kit)
    os.makedirs(d, exist_ok=True)
    for pad, (spec, secs) in defs.items():
        rate = KIT_RATE
        if isinstance(spec, str):
            x = special(cache, spec, rate, secs)
            x = finish(x, secs, rate, 0.3, start=0 if spec.startswith("hy-") else None)
        else:
            x = mixsrc(cache, spec, rate)
            x = finish(x, secs, rate, 0.35)
        if kit == "lofi":
            x = lofi(x, rate)
            x = finish(x, secs, rate, 0.2, start=0)
        mono = kit in ("lofi",) or pad in ("01-kick", "02-kick-snares") and kit == "hybrid"
        if mono:
            x = stereo(np.mean(x, axis=1))
        x = normalise(x, -1.0)
        name = "%s-%s.flac" % (kit, pad)
        write_flac(os.path.join(d, name), x, rate, mono)
        files[str(int(pad[:2]))] = "kits/%s/%s" % (kit, name)
    return files

# ------------------------------------------------------------------ instruments
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
def midi(name):
    m = re.match(r"([A-Ga-g]#?)(-?\d)", name)
    return 12 * (int(m.group(2)) + 1) + NOTE[m.group(1).upper()]

VC = {
    "kalimba": "Idiophones/Plucked Idiophones/Kalimba, Tanzania/",
    "marimba": "Idiophones/Struck Idiophones/Marimba/",
    "vibraphone": "Idiophones/Struck Idiophones/Vibraphone/",
    "glockenspiel": "Idiophones/Struck Idiophones/Glockenspiel/",
    "harp": "Chordophones/Composite Chordophones/Concert Harp/",
    "harpsichord": "Chordophones/Zithers/Harpsichord, Flemish/",
    "recorder": "Aerophones/Edge-blown Aerophones/Baroque Alto Recorder/",
    "glass": "Idiophones/Friction Idiophones/Wine Glasses/",
    "zither": "Chordophones/Zithers/Dan Tranh/",
    "bowedmetal": "Idiophones/Struck Idiophones/Vibraphone/",
}

# instrument -> (seconds, loop?, [(note, file, vlo, vhi)])
def instrument_defs():
    D = {}
    kal = {"G2": "k15", "C#3": "k16", "G3": "k6", "C#4": "k5", "G4": "k22", "C#5": "k24", "G#5": "k26"}
    D["kalimba"] = (2.8, False, [(n, "MBira3_pluck_Main_%s_%s_50_100_rr2.wav" % (n, k), 1, 127) for n, k in kal.items()])
    mar = []
    for n in ["C2", "G2", "F3", "C4", "G4", "F5", "C6"]:
        mar.append((n, "Marimba_hit_Outrigger_%s_med_01.wav" % n, 1, 92))
        mar.append((n, "Marimba_hit_Outrigger_%s_loud_01.wav" % n, 93, 127))
    D["marimba"] = (1.7, False, mar)
    D["vibraphone"] = (3.2, False, [(n, "Hard Mallets/Vibes_hard_%s_v2_rr1_Main.wav" % n, 1, 127) for n in ["F2", "C3", "G3", "D4", "A4", "E5"]])
    D["glockenspiel"] = (1.8, False, [(n, "glock_medium_%s_01.wav" % n, 1, 127) for n in ["G4", "C5", "G5", "C6", "G6", "C7"]])
    D["harp"] = (2.6, False, [(n, "KSHarp_%s_mf1.wav" % n, 1, 127) for n in ["D2", "A2", "E3", "B3", "F4", "C5", "G5", "D6"]])
    D["harpsichord"] = (2.0, False, [(n, "Sustains/Low/HarpsiRH_Low_Far_%s_rr1.wav" % n, 1, 127) for n in ["C2", "F#2", "C3", "F#3", "C4", "F#4", "C5"]])
    D["recorder"] = (2.2, True, [(n, "Sustain/AltRecorder_Sus_%s_rr1_Main.wav" % n, 1, 127) for n in ["F3", "G#3", "C4", "E4", "G#4", "C5", "E5"]])
    D["glass"] = (2.6, True, [("D#4", "Sustains/Slow/glass1_D#4_Slow_1_Main.wav", 1, 127), ("F#4", "Sustains/Slow/glass2_F#4_Slow_1_Main.wav", 1, 127),
                              ("A#4", "Sustains/Slow/glass3_A#4_Slow_1_Main.wav", 1, 127), ("D5", "Sustains/Slow/glass4_D5_Slow_2_Main.wav", 1, 127)])
    D["zither"] = (2.6, False, [(n, "Normal/%s_mf_1.wav" % n, 1, 127) for n in ["C#2", "G#2", "D#3", "G#3", "C#4", "F#4", "B4"]])
    D["bowedmetal"] = (2.8, True, [(n, "Bowed/Vibes_bowed_%s_rr1_Main.wav" % n, 1, 127) for n in ["A2", "E3", "G3", "D4", "A4", "E5"]])
    return D

def inst_sources(defs):
    return [vcsl(VC[k] + f) for k, (_, _, zones) in defs.items() for (_, f, _, _) in zones]

INST_RATE = 44100

def make_loop(x, rate, root, total):
    """Finds a loop in the sustain (whole periods, crossfaded in place). Returns (x, ls, le)."""
    f0 = 440.0 * 2 ** ((root - 69) / 12.0)
    period = rate / f0
    le = int(total * rate) - int(0.02 * rate)
    target = 0.75 * rate
    k = max(1, round(target / period))
    length = int(round(k * period))
    ls = le - length
    xf = int(min(0.25 * length, 0.12 * rate))
    seg_end = x[le - xf:le].copy()
    seg_start = x[ls - xf:ls]
    t = np.linspace(0.0, 1.0, xf)
    # equal-power crossfade: the samples before the loop end blend into the samples before the loop start
    x[le - xf:le] = seg_end * np.cos(t * np.pi / 2) + seg_start * np.sin(t * np.pi / 2)
    return x[:le + 64], ls, le

def build_instruments(cache):
    defs = instrument_defs()
    bank = {}
    for inst, (secs, loop, zones) in defs.items():
        d = os.path.join(OUT, "instruments", inst)
        os.makedirs(d, exist_ok=True)
        rendered = []
        for note, f, vlo, vhi in zones:
            x = load(cache, vcsl(VC[inst] + f), INST_RATE)
            m = np.mean(x, axis=1)
            m = m - np.mean(m)
            start = max(0, np.argmax(np.abs(m) > np.max(np.abs(m)) * db(-36)) - int(0.002 * INST_RATE))
            if loop:
                m = hp(m[:, None], 30, INST_RATE)[:, 0]
                m = m[start:]
                need = int(secs * INST_RATE)
                m = np.concatenate([m, np.zeros(max(0, need - len(m)))])[:need]
                fin = int(0.003 * INST_RATE)
                m[:fin] *= np.linspace(0, 1, fin)
                m, ls, le = make_loop(m, INST_RATE, midi(note), secs)
            else:
                m = finish(stereo(m), secs, INST_RATE, 0.35, start=start)[:, 0]
                ls = le = -1
            # loudness reference: RMS of the first 400 ms, of the loop for sustained sounds
            ref = np.sqrt(np.mean((m[ls:le] if loop else m[:int(0.4 * INST_RATE)]) ** 2))
            rendered.append([note, f, vlo, vhi, m, ls, le, ref])
        # level-match zones (top velocity layer per note), keep layer differences, then one peak normalisation
        top = {}
        for r in rendered:
            if r[3] == 127: top[r[0]] = r[7]
        med = float(np.median(list(top.values())))
        for r in rendered:
            if r[3] == 127:
                g = min(max(med / top[r[0]], db(-9)), db(9))
            else:  # softer velocity layer: 7 dB below the matched top layer of its note
                g = med * db(-7.0) / max(1e-9, r[7])
            r[4] = r[4] * g
        peak = max(np.max(np.abs(r[4])) for r in rendered)
        roots = sorted(set(midi(r[0]) for r in rendered))
        zones_out = []
        for r in rendered:
            root = midi(r[0])
            i = roots.index(root)
            lo = 0 if i == 0 else (roots[i - 1] + root) // 2 + 1
            hi = 127 if i == len(roots) - 1 else (root + roots[i + 1]) // 2
            name = "%s-%d%s.flac" % (inst, root, "" if r[3] == 127 and r[2] == 1 else "-v%d" % r[3])
            write_flac(os.path.join(d, name), stereo(r[4] * db(-1.0) / peak), INST_RATE, True)
            zones_out.append({"file": "instruments/%s/%s" % (inst, name), "root": root, "lo": lo, "hi": hi,
                              "vlo": r[2], "vhi": r[3], "ls": int(r[5]), "le": int(r[6])})
        bank[inst] = zones_out
    return bank

# ------------------------------------------------------------------ output
WAV_CACHE = None

def write_flac(path, x, rate, mono=False):
    data = x[:, :1] if mono else x
    tmp = path + ".tmp.wav"
    sf.write(tmp, np.clip(data, -1.0, 1.0), rate, subtype="PCM_16")
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", tmp, "-c:a", "flac", "-compression_level", "8", path], check=True)
    os.remove(tmp)
    if WAV_CACHE:
        y, r = sf.read(path, dtype="float32")
        sf.write(os.path.join(WAV_CACHE, os.path.splitext(os.path.basename(path))[0] + ".wav"), y, r, subtype="FLOAT")

def main():
    global WAV_CACHE
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default="sample-cache")
    ap.add_argument("--wav-cache")
    ap.add_argument("--only", choices=["kits", "instruments"])
    a = ap.parse_args()
    if a.wav_cache:
        WAV_CACHE = os.path.abspath(a.wav_cache)
        os.makedirs(WAV_CACHE, exist_ok=True)
        for f in sorted(os.listdir(os.path.join(ROOT, "resources", "drumkit"))):  # the original studio kit
            if f.endswith(".flac"):
                y, r = sf.read(os.path.join(ROOT, "resources", "drumkit", f), dtype="float32")
                sf.write(os.path.join(WAV_CACHE, f[:-5] + ".wav"), y, r, subtype="FLOAT")
    bank_path = os.path.join(OUT, "bank.json")
    bank = json.load(open(bank_path)) if os.path.exists(bank_path) else {"format": "ohpe-factory-bank", "version": 1}
    kit_defs = {k: f() for k, f in KITS.items()}
    sources = []
    if a.only != "instruments":
        sources += sum((kit_sources(d) for d in kit_defs.values()), []) + HYBRID_SOURCES + ROOM_CLAP
    if a.only != "kits":
        sources += inst_sources(instrument_defs())
    fetch_all(a.cache, sources)
    if a.only != "instruments":
        bank["kits"] = {k: build_kit(a.cache, k, d) for k, d in kit_defs.items()}
    if a.only != "kits":
        bank["instruments"] = build_instruments(a.cache)
    with open(bank_path, "w") as f:
        json.dump(bank, f, indent=1, sort_keys=True)
        f.write("\n")
    total = 0
    for dp, _, fs_ in os.walk(OUT):
        total += sum(os.path.getsize(os.path.join(dp, x)) for x in fs_)
    print("resources/samples: %.2f MB" % (total / 1e6))

if __name__ == "__main__":
    main()
