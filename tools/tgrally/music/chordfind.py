import numpy as np, sys
from classify import inst_samples
from piece_render import RECIPES
from chordtest import spec
def comb_mask(f,midis,cents,maxh=16):
    m=np.zeros(len(f),bool)
    for mm in midis:
        f0=440*2**((mm+cents/100-69)/12)
        for h in range(1,maxh+1):
            if f0*h>f[-1]: break
            m|=np.abs(1200*np.log2(np.maximum(f,1)/(f0*h)))<30
    return m
def find(s):
    f,X=spec(s); band=(f>25)&(f<6000); tot=X[band].sum(); best=None
    for c in range(-50,51,10):
        chord=[]; share=0
        for _ in range(6):
            cand=max(((X[comb_mask(f,chord+[k],c)&band].sum()/tot,k) for k in range(24,85) if k not in chord))
            if cand[0]-share<0.05: break
            chord.append(cand[1]); share=cand[0]
        if best is None or share-0.03*len(chord)>best[0]-0.03*len(best[2]): best=(share,c,sorted(chord))
    return best
for a in sys.argv[1:]:
    p,i=a.split(':'); i=int(i); m=open('pieces/%s.xm'%p,'rb').read(); im,flat=inst_samples(m)
    sh,c,ch=find(flat[im[i][0]]); print(p,i,'recipe',RECIPES[p][i].get('shape'),'-> greedy',ch,'detune %+d c'%c,'explains %.2f'%sh)
