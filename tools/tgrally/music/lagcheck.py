import numpy as np,collections,sys
from piece_render import RECIPES
SR=48000;H=48  # 1 ms
def odf(x):
    n=len(x)//H; e=np.sqrt((x[:n*H].reshape(n,H)**2).mean(1)); l=20*np.log10(e+1e-5)
    s=np.convolve(l,np.ones(5)/5,'same'); d=np.maximum(np.diff(s,prepend=s[0]),0); return d
def lag(a,b,maxlag=80):
    c=[np.dot(a[maxlag:-maxlag],np.roll(b,-k)[maxlag:-maxlag]) for k in range(-maxlag,maxlag+1)]; return np.argmax(c)-maxlag
if __name__=='__main__':
    p=sys.argv[1]
    T=dict(np.load('pieces/%s_tracks.npz'%p)); O=dict(np.load('pieces/%s_origstems.npz'%p))
    groups=collections.defaultdict(list)
    for i,r in RECIPES[p].items(): groups[r.get('track',r['t'])].append(i)
    for g,ins in groups.items():
        if g not in T: continue
        o=sum(O[str(i)].astype(float) for i in ins if str(i) in O); t=T[g].mean(1).astype(float); L=min(len(o),len(t))
        a,b=odf(o[:L]),odf(t[:L]); seg=10000; ls=[lag(a[s:s+seg],b[s:s+seg]) for s in range(0,len(a)-seg,seg) if a[s:s+seg].sum()>50]
        print('%-9s lag ms (remaster later +): whole %+d  per 10s %s'%(g,lag(a,b),ls))
