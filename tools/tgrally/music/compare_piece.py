import numpy as np,collections,json,sys,re,ast
from piece_render import RECIPES
from mixp import kpow
from check_all import chroma
SR=48000
def env(x,n=int(0.02*SR)):
    m=len(x)//n; return np.sqrt((x[:m*n].reshape(m,n)**2).mean(1))
def onset_times(e,thr=6):
    d=np.diff(20*np.log10(e+1e-6)); return np.nonzero((d>thr)&(e[1:]>e.max()*0.05))[0]+1
def cent(x):
    n=min(len(x),SR*60); X=np.abs(np.fft.rfft(x[:n]))**2; f=np.fft.rfftfreq(n,1/SR); return (X*f).sum()/max(X.sum(),1e-20)
def timing(to,tt):
    # median offset (ms) from each original onset to the nearest remaster onset, and the share that land within 30 ms
    if len(to)==0 or len(tt)==0: return np.nan,np.nan
    d=np.array([tt[np.argmin(abs(tt-t))]-t for t in to])*20
    return float(np.median(d)),float(np.mean(abs(d)<=30))
def run(p,gains):
    T=dict(np.load('pieces/%s_tracks.npz'%p)); O=dict(np.load('pieces/%s_origstems.npz'%p))
    groups=collections.defaultdict(list)
    for i,r in RECIPES[p].items(): groups[r.get('track',r['t'])].append(i)
    rows=[]; tot_o=0; tot_r=0; cache={}
    for g,ins in groups.items():
        if g not in T: continue
        o=sum(O[str(i)].astype(float) for i in ins if str(i) in O); t=T[g].mean(1).astype(float)*10**(gains.get(g,0)/20); L=min(len(o),len(t)); o,t=o[:L],t[:L]
        po=kpow(np.stack([o,o],1)); pr=kpow(np.stack([t,t],1)); tot_o+=po; tot_r+=pr; cache[g]=(o,t,po,pr,ins)
    print('%s\n%-9s %-12s %5s %9s %6s %11s %6s %5s %9s %11s'%(p,'track','inst','act','onsets','ratio','timing ms','in30','harm','cent o/r','share o/r'))
    for g,(o,t,po,pr,ins) in cache.items():
        eo,et=env(o),env(t); act=np.corrcoef(20*np.log10(eo+1e-6),20*np.log10(et+1e-6))[0,1]
        to,tt=onset_times(eo),onset_times(et); md,w30=timing(to,tt)
        ca,cb=chroma(o),chroma(t); ok=[np.sqrt(np.mean(o[k*24000:(k+1)*24000]**2))>1e-3 for k in range(len(ca))]
        h=np.array([np.corrcoef(a,b)[0,1] for a,b,x in zip(ca,cb,ok) if x]); h=np.nanmedian(h) if len(h) else np.nan
        print('%-9s %-12s %5.2f %4d/%-4d %6.2f %+6.0f %6.2f %5.2f %5.0f/%-5.0f %5.1f/%-5.1f%%'%(g,str(ins)[:12],act,len(to),len(tt),len(tt)/max(len(to),1),md,w30,h,cent(o),cent(t),100*po/tot_o,100*pr/tot_r))
for p in sys.argv[1:]:
    last=[l for l in open('pieces/%s.mix.log'%p).read().strip().splitlines() if l.startswith(p+' ')][-1]
    log=re.sub(r'np\.float64\(([^)]*)\)',r'\1',last.split(' ',1)[1])
    run(p,ast.literal_eval(log))
