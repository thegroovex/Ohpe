import numpy as np, sys, glob, os
def rd(p):
    d=np.fromfile(p,dtype=np.uint8); return np.frombuffer(d[44:].tobytes(),dtype=np.float32).reshape(-1,2)
a,b=sys.argv[1],sys.argv[2]
for f in sorted(glob.glob(a+'/*.wav')):
    g=os.path.join(b,os.path.basename(f))
    if not os.path.exists(g): continue
    x,y=rd(f),rd(g); n=min(len(x),len(y)); d=np.abs(x[:n]-y[:n]).max(axis=1)
    i=int(d.argmax()); print(f"{os.path.basename(f):50s} maxdiff {d.max():.2e} at {i/48000:.3f}s  first>1e-4 {(np.argmax(d>1e-4)/48000 if (d>1e-4).any() else -1):.3f}s peak {np.abs(x).max():.2f}")
