import os
import json,glob,numpy as np,urllib.request,urllib.parse,modsamp,verify,stload,wave,collections
from localscan import samples_of,items
def load(path,j,ln):
    if path.startswith('/'):
        for name,dd in items(path.split('!')[0]):
            if name==path: ss=samples_of(name,dd)
    elif path.startswith('ref:'):
        w=wave.open(path[4:]); b=w.readframes(w.getnframes())
        a=np.frombuffer(b,'<i2').astype(np.int64) if w.getsampwidth()==2 else np.frombuffer(b,np.uint8).astype(np.int64)-128
        return modsamp.mk('ref',a,8*w.getsampwidth(),c5=w.getframerate())
    else:
        u='https://ftp.modland.com/pub/modules/'+urllib.parse.quote(path)
        ss=modsamp.load(urllib.request.urlopen(urllib.request.Request(u,headers={'User-Agent':'Mozilla/5.0'}),timeout=60).read())
    c=[s for s in ss if s['len']==ln]; return c[0] if len(c)==1 else ss[j]
def tests(D,ratio):
    raw=np.asarray(D['raw']).astype(np.int64)
    on_grid = float(np.mean(raw % 256 == 0)) if D['bits']==16 else None       # ~1/256 if genuine 16-bit
    x=D['pcm']-D['pcm'].mean(); X=np.abs(np.fft.rfft(x*np.hanning(len(x))))**2; n=len(X)
    drop=None
    if ratio>1.05:
        c=int(n/ratio); w=max(4,int(0.08*c))
        below=X[max(1,c-w):c].mean(); above=X[c:min(n,c+w)].mean()
        drop=10*np.log10(max(below,1e-30)/max(above,1e-30))                       # large => band-limited upsample
    return on_grid,drop
rows=[x for f in glob.glob('upcheck_*.json') for x in json.load(open(f)) if x['better']]
best=collections.defaultdict(list)
for x in rows: best[x['rom']].append(x)
out=[]
for rom,v in sorted(best.items()):
    v.sort(key=lambda x:(-x['bits'],-x['ratio'],-x['corr']))
    for x in v[:6]:
        try: D=load(x['path'],x['j'],x['len'])
        except Exception as e: print('fail',x['path'],e); continue
        g,dr=tests(D,x['ratio']); out.append((rom,x['path'][-55:],x['j'],x['bits'],x['ratio'],x['corr'],g,dr))
        print('%-12s %2db x%.3f corr %.3f  on8bitgrid %s  drop@oldlimit %s  %s #%d'%(rom,x['bits'],x['ratio'],x['corr'],'-' if g is None else '%.3f'%g,'-' if dr is None else '%.1fdB'%dr,x['path'][-55:],x['j']),flush=True)
print('--- samples in use (reference folder WAVs)')
man=json.load(open(os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'reference/tgrally/XM/manifest.json')))
for e in man:
    D=load('ref:' + os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'reference/tgrally/XM/')+e['file'],0,0); g,dr=tests(D,e['length_ratio'])
    print('%-34s %2db x%.3f  on8bitgrid %s  drop %s'%(e['file'],D['bits'],e['length_ratio'],'-' if g is None else '%.3f'%g,'-' if dr is None else '%.1fdB'%dr))
