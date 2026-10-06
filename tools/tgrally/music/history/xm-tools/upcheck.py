"""Collect every higher-rate or 16-bit candidate, fetch it, and test whether it is genuinely better
than the ROM copy and the same recording over the whole ROM sample."""
import json,glob,urllib.request,urllib.parse,numpy as np,modsamp,verify,match2,os
from scipy.signal import resample_poly
from fractions import Fraction
from localscan import samples_of
cands={}
for f in glob.glob('scanout/*.jsonl')+glob.glob('amout/*.jsonl'):
    for l in open(f):
        try: d=json.loads(l)
        except: continue
        if 'tgr' in d['path'].lower(): continue
        for h in d.get('hits',[]):
            if h['kind']=='env' and h['ncc']>=0.97 and (h['bits']==16 or h['ratio']>1.05):
                key=(h['rom'],d['path'],h['j'])
                cands[key]=h
import sys
W,N=int(sys.argv[1]),int(sys.argv[2])
files=sorted({k[1] for k in cands}); mine=set(files[W::N])
cands={k:v for k,v in cands.items() if k[1] in mine}
print(len(cands),'candidates',flush=True)
cache={}
def load(path):
    if path in cache: return cache[path]
    if path.startswith('/'):
        base=path.split('!')[0]; 
        from localscan import items
        ss=None
        for name,dd in items(base):
            if name==path: ss=samples_of(name,dd)
    else:
        u='https://ftp.modland.com/pub/modules/'+urllib.parse.quote(path)
        dd=urllib.request.urlopen(urllib.request.Request(u,headers={'User-Agent':'Mozilla/5.0'}),timeout=60).read()
        ss=modsamp.load(dd)
    ss=[s for s in (ss or []) if s['len']>=64]
    cache[path]=ss; return ss
res=[]
for (rom,path,j),h in sorted(cands.items()):
    try: ss=load(path)
    except Exception as e: print('load fail',path,e); continue
    D=[s for s in ss if s['len']==h['len']]
    if not D: continue
    D=D[0] if len(D)==1 else ss[j] if j<len(ss) else D[0]
    R=verify.rom_sample(rom)
    rt=D['len']/R['len']; f=Fraction(rt).limit_denominator(400)
    y=resample_poly(D['pcm'],f.denominator,f.numerator)
    sc,_=match2.ncc(R['pcm'][:min(len(R['pcm']),len(y))-4],y)
    raw=np.asarray(D['raw']).astype(np.int64)
    real16 = D['bits']==16 and np.mean((raw & 0xFF)!=0)>0.5
    # energy above the ROM copy's band, in the donor's own spectrum
    hf=0.0
    if rt>1.05:
        X=np.abs(np.fft.rfft(D['pcm']-D['pcm'].mean()))**2; cut=int(len(X)/rt)
        hf=float(X[cut:].sum()/max(X.sum(),1e-12))
    better = (real16 or (rt>1.05 and hf>1e-4)) and sc>=0.97
    print(rom,path[-50:],flush=True)
    res.append(dict(rom=rom,path=path,j=j,bits=D['bits'],len=D['len'],rate=round(D['c5'] or 0),ratio=round(rt,4),corr=round(sc,4),real16=bool(real16),hf=round(hf,6),better=bool(better),name=D['name'][:40]))
json.dump(res,open('upcheck_%02d.json'%W,'w'),indent=1)
from collections import defaultdict
best=defaultdict(list)
for r in res:
    if r['better']: best[r['rom']].append(r)
for rom,v in sorted(best.items()):
    v.sort(key=lambda r:(-r['bits'],-r['ratio'],-r['corr']))
    t=v[0]; print(rom,'%d cand; best: %db x%.3f corr %.3f hf %.4f %s #%d'%(len(v),t['bits'],t['ratio'],t['corr'],t['hf'],t['path'][-70:],t['j']))
