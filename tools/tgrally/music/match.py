import glob,sys,modsamp,romsamp,numpy as np,pickle
from scipy.signal import fftconvolve, resample
def ncc(a,b):
    # max normalized xcorr of short a within b (both float); returns score, lag
    if len(a)>len(b): a,b=b,a
    if len(a)<64: return 0,0
    a=a-a.mean(); na=np.sqrt((a*a).sum())
    if na==0: return 0,0
    c=fftconvolve(b,a[::-1],mode='valid')
    b2=np.concatenate([[0],np.cumsum(b*b)]); s1=np.concatenate([[0],np.cumsum(b)]); n=len(a)
    e=b2[n:]-b2[:-n]-(s1[n:]-s1[:-n])**2/n
    r=c/(na*np.sqrt(np.maximum(e,1e-12)))
    i=int(np.argmax(r)); return float(r[i]),i
def i8(s):
    r=s['raw']; return (np.asarray(r)>>8).astype(np.int8) if s['bits']==16 else np.asarray(r).astype(np.int8)
donors={}
for f in sorted(glob.glob('donors/*.bin')):
    try: donors[f.split('/')[1][:-4]]=[s for s in modsamp.load(open(f,'rb').read()) if s['len']>32]
    except Exception as e: print('skip',f,e,file=sys.stderr)
res=[]
for mo,m in romsamp.rom_modules():
    for k,R in enumerate(modsamp.xm(m)):
        if R['len']<=32: continue
        rb=R['raw'].astype(np.int8).tobytes(); best=[]
        for did,ss in donors.items():
            if did in ('134660','142976','142098','135802') : pass
            for j,D in enumerate(ss):
                db=i8(D).tobytes()
                if rb[:min(64,len(rb))] in db and (rb in db or db in rb):
                    best.append((1.0,'EXACT',did,j,D)); continue
                # fuzzy: resample donor to rom length ratio variants
                for ratio in {1.0, round(D['len']/R['len'],3)}:
                    if not (0.2<ratio<8): continue
                    Dr = D['pcm'] if ratio==1.0 else resample(D['pcm'], max(64,int(round(D['len']/ratio))))
                    sc,lag=ncc(R['pcm'][:20000],Dr)
                    if sc>0.8: best.append((sc,'x%.3f'%ratio,did,j,D))
        best.sort(key=lambda t:-t[0])
        res.append((mo,k,R,best[:6]))
        print(mo,k,R['name'][:40],R['len'],'|',' ; '.join('%s %.3f %s#%d %s %db %dHz len%d'%(t[1],t[0],t[2],t[3],t[4]['name'][:30],t[4]['bits'],t[4]['c5'] or 0,t[4]['len']) for t in best[:4]),flush=True)
pickle.dump([(mo,k,R['name'],R['len'],[(t[0],t[1],t[2],t[3],t[4]['name'],t[4]['bits'],t[4]['c5'],t[4]['len']) for t in b]) for mo,k,R,b in res],open('match.pkl','wb'))
