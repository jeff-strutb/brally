"""Pick each source's articulation by comparing candidates with the original sample:
third-octave spectrum (inside the original's band) plus the loudness envelope over the first 0.6 s."""
import numpy as np, itertools, json
import build2 as B
from remaster import *
BANDS=[50*2**(k/3) for k in range(25)]
def bdb(x,rate,secs=0.6):
    x=x[:int(secs*rate)]; X=np.abs(np.fft.rfft(x))**2; f=np.fft.rfftfreq(len(x),1/rate)
    return np.array([10*np.log10(X[(f>=c*2**-(1/6))&(f<c*2**(1/6))].sum()+1e-20) for c in BANDS])
def envdb(x,rate,secs=0.6,step=0.01):
    n=int(step*rate); m=int(secs/step); e=[np.sqrt(np.mean(x[i*n:(i+1)*n]**2)+1e-12) for i in range(m)]
    e=np.array(e); return 20*np.log10(e/e.max())
def score(o,orate,c):
    ob=bdb(o,orate); cb=bdb(c,SR); valid=(ob>ob.max()-35)&(np.array(BANDS)<0.45*orate)
    d=(cb-cb[valid].mean())-(ob-ob[valid].mean()); se=np.sqrt(np.mean(d[valid]**2))
    eo=np.maximum(envdb(o,orate),-40); ec=np.maximum(envdb(c,SR),-40); ee=np.sqrt(np.mean((eo-ec)**2))
    return se+ee,se,ee
res={}
# drums
DR={17:['Kdrum_with_contact','Kdrum_without_contact'],11:['Snare'],9:['Hihat_closed','Hihat_closed_shank'],18:['Hihat_open','Hihat_open_tip','Hihat_semi_open']}
CLOSE={17:{'Kdrum_front':1.0,'Kdrum_back':0.7},11:{'Snare_top':1.0,'Snare_bottom':0.35},9:{'Hihat':1.0},18:{'Hihat':1.0}}
for ins,variants in DR.items():
    s=B.orig[B.SMP[ins]]; orate=s['c5']*2**((B.common(ins)-49)/12); o=s['pcm']
    best=None
    for v,pct,oh,amb in itertools.product(variants,[0.5,0.7,0.85,0.97],[0.0,0.2,0.4,0.7],[0.0,0.3,0.6,1.0]):
        mics=dict(CLOSE[ins]); mics.update({'OHL':oh,'OHR':oh,'AmbL':amb,'AmbR':amb})
        try: y=B.DRUM.hit(v,pct=pct,mics=mics)
        except Exception as e: continue
        if ins in (9,18): y=highpass(y,300)
        sc=score(o,orate,y)
        if best is None or sc[0]<best[0][0]: best=(sc,(v,pct,oh,amb))
    res[ins]={'variant':best[1][0],'pct':best[1][1],'oh':best[1][2],'amb':best[1][3],'score':best[0]}
    print('drum',ins,res[ins],flush=True)
# piano velocity layers
for ins in (3,4,10):
    s=B.orig[B.SMP[ins]]; o=s['pcm']; orate=s['c5']; v=B.voicing(ins); best=None
    for vel in range(6,17):
        parts=[w*fade(B.SAL.note(mid,vel=vel),4.0,0.4) for mid,w in v]; L=max(len(p) for p in parts); y=np.zeros(L)
        for p in parts: y[:len(p)]+=p
        sc=score(o,orate,y)
        if best is None or sc[0]<best[0][0]: best=(sc,vel)
    res[ins]={'vel':best[1],'score':best[0]}; print('piano',ins,res[ins],flush=True)
json.dump(res,open('choices.json','w'),indent=1)
