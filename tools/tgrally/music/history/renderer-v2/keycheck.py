"""Per part: the transposition (semitones) that best aligns the remaster's pitch classes with the
original's, and the fine tuning (cents) of each against A440."""
import numpy as np, collections, sys
from piece_render import RECIPES
SR=48000
def spec_seg(x):
    n=1<<15; out=[]
    for k in range(0,len(x)-n,n):
        s=x[k:k+n]
        if np.sqrt(np.mean(s**2))<1e-3: out.append(None); continue
        out.append(np.abs(np.fft.rfft(s*np.hanning(n)))**2)
    return out
f=np.fft.rfftfreq(1<<15,1/SR); ok=(f>40)&(f<2500); midi=69+12*np.log2(f[ok]/440)
def chroma(X): c=np.zeros(12); np.add.at(c,np.round(midi).astype(int)%12,X[ok]); return c/ (c.sum()+1e-20)
def cents(X):
    w=X[ok]; d=(midi-np.round(midi))*100; top=w>np.percentile(w,99); return np.average(d[top],weights=w[top])
p=sys.argv[1]; tracks=sys.argv[2] if len(sys.argv)>2 else 'pieces/%s_tracks.npz'
T=dict(np.load(tracks%p)); O=dict(np.load('pieces/%s_origstems.npz'%p))
groups=collections.defaultdict(list)
for i,r in RECIPES[p].items(): groups[r.get('track',r['t'])].append(i)
for g,ins in groups.items():
    if g not in T: continue
    o=sum(O[str(i)].astype(float) for i in ins if str(i) in O); t=T[g].mean(1); L=min(len(o),len(t))
    A=spec_seg(o[:L]); B=spec_seg(t[:L]); sc=np.zeros(12); co=[]; cr=[]
    for a,b in zip(A,B):
        if a is None or b is None: continue
        ca,cb=chroma(a),chroma(b); sc+=[np.dot(ca,np.roll(cb,k)) for k in range(12)]; co.append(cents(a)); cr.append(cents(b))
    if not co: continue
    k=int(np.argmax(sc)); print('%-8s %-14s best shift %+d st (0 st score %.2f of best)  cents orig %+.0f remaster %+.0f'%(g,ins,k if k<=6 else k-12,sc[0]/sc.max(),np.median(co),np.median(cr)))
