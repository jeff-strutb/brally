import numpy as np, modsamp, romsamp, match2
def rom_sample(key):
    mo,k=key.split('#')
    for o,m in romsamp.rom_modules():
        if o==mo: return modsamp.xm(m)[int(k)]
def donor(path,j): return match2.load_donors([path])[path][j]
def reproduce(R,D):
    """Try simple converters donor->ROM; return best (exact-fraction, method, offset)."""
    r=np.asarray(R['raw']).astype(np.int16); n=len(r)
    d=np.asarray(D['raw']).astype(np.float64)
    if D['bits']==16: d=d/256.0
    best=(0,'',0)
    ratio=D['len']/n
    for rt in sorted({ratio, round(ratio), round(ratio*2)/2, 2**0.5, round(ratio,3)}):
        if rt<=0: continue
        for name,fn in [('nearest',lambda x,i: x[np.minimum(np.floor(i).astype(int),len(x)-1)]),
                        ('round',lambda x,i: x[np.minimum(np.round(i).astype(int),len(x)-1)]),
                        ('linear',lambda x,i: np.interp(i,np.arange(len(x)),x)),
                        ('avg',None)]:
            idx=np.arange(n)*rt
            if name=='avg':
                if abs(rt-round(rt))>1e-6 or rt<1: continue
                k=int(round(rt)); m=len(d)//k; y=d[:m*k].reshape(m,k).mean(1)
            else: y=fn(d,idx)
            for q,qf in [('floor',np.floor),('round',np.round),('trunc',np.trunc)]:
                yy=np.clip(qf(y),-128,127).astype(np.int16)
                L=min(len(yy),n); f=float(np.mean(yy[:L]==r[:L])) if L==n or L>0.98*n else 0
                if f>best[0]: best=(f,'%s/%s x%.4f'%(name,q,rt),0)
    return best

def reproduce2(R,D,ratios=None):
    """Search converter space: decimation method x phase x gain x quantizer. Returns best exact fraction."""
    r=np.asarray(R['raw']).astype(np.int16); n=len(r)
    d=np.asarray(D['raw']).astype(np.float64)
    if D['bits']==16: d=d/256.0
    ratio=D['len']/n
    rts=ratios or sorted({ratio, round(ratio,4), round(ratio*4)/4, 2**0.5, 2**(round(12*np.log2(ratio))/12)})
    best=(0,'')
    for rt in rts:
        for ph in np.linspace(0,rt,9)[:-1] if rt>1 else [0]:
            idx=ph+np.arange(n)*rt
            if idx[-1]>len(d)-1: idx=idx[idx<=len(d)-1]
            L=len(idx)
            if L<0.98*n: continue
            cands={'nearest':d[np.floor(idx).astype(int)],'round':d[np.minimum(np.round(idx).astype(int),len(d)-1)],'linear':np.interp(idx,np.arange(len(d)),d)}
            if abs(rt-round(rt))<1e-9 and rt>=2:
                k=int(round(rt)); m=len(d)//k; cands['avg']=d[:m*k].reshape(m,k).mean(1)[:n]
            for nm,y in cands.items():
                y=y[:L]; rr=r[:len(y)]
                A=np.vstack([y,np.ones_like(y)]).T; g,c=np.linalg.lstsq(A,rr,rcond=None)[0]
                for gg,cc in {(1.0,0.0),(g,c),(round(g*64)/64,0.0),(g,0.0)}:
                    z=y*gg+cc
                    for q,qf in (('floor',np.floor),('round',np.round),('trunc',np.trunc)):
                        f=float(np.mean(np.clip(qf(z),-128,127)==rr))
                        if f>best[0]: best=(f,'%s/%s x%.4f ph%.2f g%.4f c%.2f'%(nm,q,rt,ph,gg,cc))
    return best
