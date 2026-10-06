import sys,json,numpy as np,romsamp,modsamp,match2
from scipy.signal import decimate,resample_poly
from fractions import Fraction
D=4
banks=[]
import glob
for nm in sorted(glob.glob('tri/*.npy')):
    s=np.load(nm).astype(np.float64); L=len(s); seg=1<<20
    for a in range(0,L,seg): banks.append(('%s@%x'%(nm[4:-4],a*D), s[a:a+seg+8192]))
R=[('%s#%d'%(mo,k),s) for mo,m in romsamp.rom_modules() for k,s in enumerate(modsamp.xm(m)) if s['len']>32]
keys=sys.argv[1].split(',')
out=open(sys.argv[2],'a')
ratios=[2**(k/24) for k in range(0,73)]
for key,s in R:
    if key not in keys: continue
    best=[]
    for r in ratios:
        f=Fraction(r).limit_denominator(48)
        t=resample_poly(s['pcm'][:16000],f.numerator,f.denominator)
        if len(t)<D*256: continue
        t=decimate(t,D,ftype='fir')[:4096]
        if len(t)<64 or np.std(t)==0: continue
        for name,b in banks:
            sc,pos=match2.ncc(t,b)
            best.append((sc,r,name,pos*D))
    best.sort(reverse=True)
    out.write(json.dumps({'rom':key,'name':s['name'],'top':best[:5]})+'\n'); out.flush()
