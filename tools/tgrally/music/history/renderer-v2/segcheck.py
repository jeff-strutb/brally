import numpy as np,collections,sys
from piece_render import RECIPES
from lagcheck import odf
SR=48000
p=sys.argv[1]; seg=int(sys.argv[2]) if len(sys.argv)>2 else 5
T=dict(np.load('pieces/%s_tracks.npz'%p)); O=dict(np.load('pieces/%s_origstems.npz'%p))
groups=collections.defaultdict(list)
for i,r in RECIPES[p].items(): groups[r.get('track',r['t'])].append(i)
print('seg(s) '+' '.join('%9s'%g[:9] for g in groups))
rows=collections.defaultdict(list)
for g,ins in groups.items():
    o=sum(O[str(i)].astype(float) for i in ins if str(i) in O); t=T[g].mean(1).astype(float); L=min(len(o),len(t))
    a,b=odf(o[:L]),odf(t[:L]); S=seg*1000
    for s in range(0,len(a),S):
        x,y=a[s:s+S],b[s:s+S]; rms=np.sqrt(np.mean(o[s*48:(s+S)*48]**2))
        if rms<3e-3: rows[s].append('    -    '); continue
        # onset agreement: correlation of 1 ms onset-strength curves smoothed by 10 ms
        k=np.ones(10); xs,ys=np.convolve(x,k,'same'),np.convolve(y,k,'same')
        rows[s].append('%5.2f %3d'%(np.corrcoef(xs,ys)[0,1] if ys.std()>0 else 0, 20*np.log10(rms)))
for s in sorted(rows): print('%5d  '%(s//1000)+' '.join(rows[s]))
