import glob,sys,os,modsamp,romsamp,numpy as np,pickle
from scipy.signal import fftconvolve, resample_poly
from fractions import Fraction
def ncc(a,b):
    if len(a)>len(b) or len(a)<64: return 0,0
    a=a-a.mean(); na=np.sqrt((a*a).sum())
    if na==0: return 0,0
    c=fftconvolve(b,a[::-1],mode='valid')
    b2=np.concatenate([[0],np.cumsum(b*b)]); s1=np.concatenate([[0],np.cumsum(b)]); n=len(a)
    e=b2[n:]-b2[:-n]-(s1[n:]-s1[:-n])**2/n
    floor=max(1e-9, 1e-3*float(np.median(e)) if len(e) else 0, 1e-4*na*na)
    r=np.where(e>floor, c/(na*np.sqrt(np.maximum(e,1e-9))), 0.0)
    i=int(np.argmax(r)); return float(r[i]),i
def rs(x,ratio):
    f=Fraction(ratio).limit_denominator(64); return resample_poly(x,f.denominator,f.numerator)
def load_donors(paths, loader=None):
    out={}
    for f in paths:
        try: out[f]=[s for s in (loader or modsamp.load)(open(f,'rb').read()) if s['len']>32]
        except Exception as e: print('skip',f,e,file=sys.stderr)
    return out
GRID=[1.0,1.0595,1.122,1.26,1.335,1.414,1.498,1.587,1.682,1.782,2.0,2.52,2.667,2.828,3.0,4.0,5.27]
def match(R, donors, grid=True):
    best=[]
    rb=R['raw'].astype(np.int8).tobytes(); rp=R['pcm'][:12000]
    for did,ss in donors.items():
        for j,D in enumerate(ss):
            if D['len'] < 0.4*R['len'] and D['len']<len(rp): continue
            db=(np.asarray(D['raw'])>>8 if D['bits']==16 else np.asarray(D['raw'])).astype(np.int8).tobytes()
            if rb[:64] in db and (rb in db or db in rb): best.append((1.0,'EXACT',did,j,D)); continue
            ratios={round(D['len']/R['len'],4)} | (set(GRID) if grid else {1.0})
            top=(0,None)
            for ratio in ratios:
                if not (0.9<ratio<6.5): continue
                Dr = D['pcm'] if ratio==1.0 else rs(D['pcm'], ratio)
                if len(Dr)<len(rp)*0.95: continue
                sc,lag=ncc(rp[:min(len(rp),len(Dr))],Dr)
                if sc>top[0]: top=(sc,ratio)
            if top[0]>0.85: best.append((top[0],'x%.3f'%top[1],did,j,D))
    best.sort(key=lambda t:-t[0]); return best
