import numpy as np,collections,sys
from check_all import chroma
from piece_render import RECIPES
def run(p):
    T=dict(np.load('pieces/%s_tracks.npz'%p)); O=dict(np.load('pieces/%s_origstems.npz'%p))
    groups=collections.defaultdict(list)
    for i,r in RECIPES[p].items(): groups[r.get('track',r['t'])].append(i)
    for g,ins in groups.items():
        if g not in T: continue
        o=sum(O[str(i)].astype(float) for i in ins if str(i) in O); t=T[g].mean(1).astype(float); L=min(len(o),len(t))
        ca=chroma(o[:L]); cb=chroma(t[:L]); act=(np.array([np.sqrt(np.mean(o[i*24000:(i+1)*24000]**2)) for i in range(len(ca))])>1e-3)
        r=[np.corrcoef(a,b)[0,1] for a,b,x in zip(ca,cb,act) if x]; r=np.array(r); r=r[np.isfinite(r)]
        best=max(range(12),key=lambda s:np.nanmean([np.corrcoef(a,np.roll(b,s))[0,1] for a,b,x in zip(ca,cb,act) if x]))
        print('  %-9s ins %-16s agreement %.2f  (best at a shift of %d semitones)'%(g,ins,np.median(r) if len(r) else np.nan,best if best<=6 else best-12))
for p in sys.argv[1:]: print(p); run(p)
