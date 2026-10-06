import numpy as np, collections, sys, ampfit
from classify import inst_samples
from piece_render import RECIPES
def spec(s):
    x=ampfit.played(s,1.0); x=x[int(0.05*s['c5']):]; N=1<<int(np.ceil(np.log2(len(x)))); X=np.abs(np.fft.rfft(x*np.hanning(len(x)),N))**2; f=np.fft.rfftfreq(N,1/s['c5']); return f,X
def comb_share(f,X,midis,maxh=16):
    m=np.zeros(len(f),bool)
    for mm in midis:
        f0=440*2**((mm-69)/12)
        for h in range(1,maxh+1):
            fh=f0*h
            if fh>f[-1]: break
            m|=np.abs(1200*np.log2(np.maximum(f,1)/fh))<35
    band=(f>25)&(f<6000); return X[m&band].sum()/X[band].sum()
if __name__=='__main__':
  for p in sys.argv[1:]:
      m=open('pieces/%s.xm'%p,'rb').read(); im,flat=inst_samples(m)
      for i,r in sorted(RECIPES[p].items()):
          sh=r.get('shape',[])
          if len(sh)<2: continue
          s=flat[im[i][0]]; f,X=spec(s)
          # the shape is written in XM C-4 terms: midi as heard at C-4 = the sample's own pitch at c5
          full=comb_share(f,X,sh); root=comb_share(f,X,sh[:1]); 
          best=max(((comb_share(f,X,[sh[0],k]),k) for k in sh[1:]),default=(0,0))
          print('%-9s %2d %-40s chord %.2f  root-only %.2f  root+%d %.2f'%(p,i,str(sh),full,root,best[1],best[0]))
