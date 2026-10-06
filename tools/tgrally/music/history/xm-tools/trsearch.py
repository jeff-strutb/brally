import sys,json,glob,wave,numpy as np,romsamp,modsamp,match2
from scipy.signal import decimate,resample_poly
from fractions import Fraction
D=4
src=[]
for f in sorted(glob.glob('trinity/*.wav')):
    w=wave.open(f); x=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,w.getnchannels()).mean(1)
    src.append((f.split('/')[-1],decimate(x.astype(float),D,ftype='fir')))
R=[('%s#%d'%(mo,k),s) for mo,m in romsamp.rom_modules() for k,s in enumerate(modsamp.xm(m)) if s['len']>32]
keys=set(sys.argv[1].split(',')); out=open(sys.argv[2],'a')
ratios=[2**(k/24) for k in range(0,73)]
for key,s in R:
    if key not in keys: continue
    best=[]
    for r in ratios:
        f=Fraction(r).limit_denominator(48)
        t=resample_poly(s['pcm'][:16000],f.numerator,f.denominator)
        if len(t)<D*256: continue
        t=decimate(t,D,ftype='fir')[:4096]
        if np.std(t)==0: continue
        for name,b in src:
            if len(b)<len(t): continue
            sc,pos=match2.ncc(t,b); best.append((sc,r,name,pos*D))
    best.sort(reverse=True)
    out.write(json.dumps({'rom':key,'name':s['name'],'top':best[:5]})+'\n'); out.flush()
