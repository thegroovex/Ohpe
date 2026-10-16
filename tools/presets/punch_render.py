#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Punch-pass helpers: render a (modified) preset with ohpe_render (build-core), level / crest / correlation / loudness /
true-peak measurements. OHPE_WAVDIR: the decoded sample bank (WAV) for the renders."""
import json, os, subprocess, sys, tempfile, glob
import numpy as np, soundfile as sf
WT=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),"..","..")); FAC=WT+"/resources/presets/factory"
R=[WT+"/build-core/tests/core/ohpe_render","--bank",WT+"/resources/samples/bank.json","--wavdir",os.environ.get("OHPE_WAVDIR","/workspace/work/ohpe-r3-wavall")]
SR=48000
def find(name):
    for f in glob.glob(FAC+"/*.ohpepreset"):
        d=json.load(open(f))
        if d["name"]==name: return f,d
def render(d, extra, tag="x"):
    tmp=tempfile.mkdtemp(prefix="rp-"); p=os.path.join(tmp,tag+".ohpepreset"); json.dump(d,open(p,"w"))
    subprocess.run(R+["--out",tmp]+extra+[p],check=True,capture_output=True)
    return sf.read(os.path.join(tmp,tag+".wav"),always_2d=True)[0]
def db(x): return 20*np.log10(max(x,1e-12))
def stats(a):
    pk=np.max(np.abs(a)); m=a.mean(1); 
    # rms over active 50ms windows
    w=2400; n=len(m)//w; r=np.sqrt((a[:n*w]**2).mean(1).reshape(n,w).mean(1)); act=r[r>np.max(r)*0.03]
    rms=np.sqrt((act**2).mean()) if len(act) else 1e-9
    s=a[:,0]-a[:,1]; corr=np.sum(a[:,0]*a[:,1])/np.sqrt(np.sum(a[:,0]**2)*np.sum(a[:,1]**2)+1e-20)
    return dict(peak=db(pk),rms=db(rms),crest=db(pk)-db(rms),corr=corr)
def truepeak(a):
    import numpy.fft as F
    n=len(a); X=F.rfft(a,axis=0); Y=np.zeros((n*4//2+1,a.shape[1]),complex); Y[:X.shape[0]]=X
    y=F.irfft(Y,n*4,axis=0)*4; return db(np.abs(y).max())
def lufs(a):
    p=tempfile.mktemp(suffix=".wav"); sf.write(p,a,SR,subtype="FLOAT")
    r=subprocess.run(["ffmpeg","-nostats","-i",p,"-filter_complex","ebur128","-f","null","-"],capture_output=True,text=True)
    for l in reversed(r.stderr.splitlines()):
        if l.strip().startswith("I:"): return float(l.split()[1])
