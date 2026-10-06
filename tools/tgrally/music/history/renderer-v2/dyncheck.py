import numpy as np, soundfile as sf, sys
from mixp import kweight
for p in sys.argv[1:]:
    o=np.fromfile('pieces/%s.xmlog.wav'%p,'<i2',offset=44).reshape(-1,2)/32768.0; sr=48000; r,_=sf.read('pieces/%s_mix.wav'%p); L=min(len(o),len(r)); S=5*sr
    def lu(x): y=kweight(x); return np.array([-0.691+10*np.log10(np.mean(np.sum(y[k:k+S]**2,1))+1e-12) for k in range(0,L-S,S)])
    a,b=lu(o[:L]),lu(r[:L]); a-=np.median(a); b-=np.median(b)
    print(p,'section loudness vs median (5 s): orig range %.1f dB, remaster %.1f dB, corr %.2f'%(np.ptp(a[a>-20]),np.ptp(b[b>-20]),np.corrcoef(a,b)[0,1]))
    print('  orig',' '.join('%+.0f'%v for v in a)); print('  rem ',' '.join('%+.0f'%v for v in b))
