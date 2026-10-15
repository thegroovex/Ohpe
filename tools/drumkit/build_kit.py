# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
# Builds resources/drumkit/*.flac from a checkout of github.com/sfzinstruments/virtuosity_drums
# (CC0, rev 9f04cf9a734527edfbb0a4eee1f674e45bbf71bc) placed in ./vd. Needs ffmpeg.
# Factory drum kit for Ohpe, built from Virtuosity Drums (CC0), rev 9f04cf9a734527edfbb0a4eee1f674e45bbf71bc
S="Samples/"
def m(art, mics, vl, rr=True):
    out=[]
    for mic,g in mics:
        name=f"{mic}_{art}_{vl}" + ("_rr1" if rr else "")
        out.append((f"{S}{mic}/{art.split('_')[0]}/{name}.flac", g))
    return out
K=[("kickmic",0),("mid",-4),("oh",-10)]
SN=[("snaremic",0),("mid",-3),("oh",-9)]
T=[("mid",0),("oh",-4)]
C=[("mid",0),("oh",-2)]
PC="Samples/perc/close/"
kit=[ # (file name, sources, seconds)
 ("01-kick",       m("kick_snoff",K,"vl3"), 1.2),
 ("02-kick-snares",m("kick_snon",K,"vl4"), 1.2),
 ("03-snare",      m("snare_center",SN,"vl20",False), 1.2),
 ("04-rimshot",    m("snare_rimshot",SN,"vl10",False), 1.2),
 ("05-clap",       [], 0),   # synthetic clap model (no CC0 recording in the source)
 ("06-hat-closed", m("hh_closed",C,"vl3"), 0.6),
 ("07-hat-open",   m("hh_open",C,"vl3"), 2.5),
 ("08-hat-pedal",  m("hh_pedal",C,"vl2"), 0.6),
 ("09-tom-low",    m("ltom_center",T,"vl12",False), 2.0),
 ("10-tom-high",   m("htom_center",T,"vl12",False), 2.0),
 ("11-tom-floor",  m("ltom_center",T,"vl14",False), 2.2),
 ("12-cross-stick",m("snare_crossstick",SN,"vl12",False), 0.6),
 ("13-ride",       m("ride_ride",C,"vl2"), 3.5),
 ("14-ride-bell",  m("ride_bell",C,"vl2"), 3.0),
 ("15-crash",      m("crash_crash",C,"vl2"), 4.0),
 ("16-crash-sizzle",m("crash_sizzle",C,"vl2"), 4.0),
 ("17-hat-half",   m("hh_half",C,"vl3"), 1.2),
 ("18-snare-ghost",m("snare_center",SN,"vl6",False), 0.8),
 ("19-cowbell",    [(PC+"cowbell/Cowbell1_Normal_v2_rr1_Close.wav",0)], 1.0),
 ("20-tambourine", [(PC+"tambourine/Tamb1_Shake_rr1_Close.wav",0)], 1.0),
 ("21-shaker",     [(PC+"shaker/LShaker_Shake1D_rr1_Close.wav",0)], 0.6),
 ("22-claves",     [(PC+"claves/Claves1_Hit_v2_rr1_Close.wav",0)], 0.6),
 ("23-conga",      [(PC+"conga/Conga_22_HitN_101_127_rr1_Close.wav",0)], 1.0),
 ("24-bongo",      [(PC+"bongoh/BongoH_Hit1_v2_rr1_Close.wav",0)], 0.8),
]
import subprocess, re, os
os.chdir('vd')
def run(a): subprocess.run(a,check=True)
report=[]
for name, src, secs in kit:
    if not src: continue
    args=["ffmpeg","-v","error","-y"]
    for p,_ in src: args+=["-i",p]
    parts=[f"[{i}:a]aresample=48000,aformat=channel_layouts=stereo,volume={g}dB[a{i}]" for i,(p,g) in enumerate(src)]
    mix="".join(f"[a{i}]" for i in range(len(src)))
    chain=";".join(parts)+f";{mix}amix=inputs={len(src)}:normalize=0[m]"
    raw=f"../out/{name}.raw.wav"; run(args+["-filter_complex",chain,"-map","[m]","-c:a","pcm_f32le",raw])
    vr=subprocess.run(["ffmpeg","-v","info","-i",raw,"-af","volumedetect","-f","null","-"],capture_output=True,text=True).stderr
    rawpk=float(re.search(r"max_volume: (-?[\d.]+) dB",vr).group(1))
    sd=subprocess.run(["ffmpeg","-v","info","-i",raw,"-af",f"silencedetect=n={rawpk-30}dB:d=0.0005","-f","null","-"],capture_output=True,text=True).stderr
    m=re.search(r"silence_start: (-?[\d.e-]+)",sd); e=re.search(r"silence_end: ([\d.]+)",sd)
    onset=float(e.group(1)) if (m and e and float(m.group(1))<=0.0005) else 0.0
    start=max(0.0,onset-0.0015)
    fade=max(0.05,secs*0.35)
    cut=f"../out/{name}.cut.wav"
    run(["ffmpeg","-v","error","-y","-i",raw,"-af",f"atrim=start={start}:duration={secs},asetpts=N/SR/TB,afade=t=in:d=0.0008,afade=t=out:st={secs-fade}:d={fade}","-c:a","pcm_f32le",cut])
    vd=subprocess.run(["ffmpeg","-v","info","-i",cut,"-af","volumedetect","-f","null","-"],capture_output=True,text=True).stderr
    peak=float(re.search(r"max_volume: (-?[\d.]+) dB",vd).group(1))
    out=f"../out/{name}.flac"
    run(["ffmpeg","-v","error","-y","-i",cut,"-af",f"volume={-1.0-peak}dB","-sample_fmt","s16","-c:a","flac","-compression_level","8",out])
    os.remove(raw); os.remove(cut)
    report.append((name, round(onset*1000,1), round(peak,1), os.path.getsize(out)))
for r in report: print(r)
print("total", sum(r[3] for r in report))
