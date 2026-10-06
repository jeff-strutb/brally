import numpy as np, collections, sys
import engines as E
from classify import inst_samples
from render_remaster import load_events
p=sys.argv[1]; ids=[int(a) for a in sys.argv[2:]]
m=open('pieces/%s.xm'%p,'rb').read(); im,flat=inst_samples(m); notes,T=load_events('pieces/%s.csv'%p); by=collections.defaultdict(list)
for n in notes: by[n['instr']].append(n)
for i in ids:
    ns=by[i]; s=flat[im[i][collections.Counter(n['sample'] for n in ns).most_common(1)[0][0]]]
    nb=collections.Counter(n['note'] for n in ns).most_common(1)[0][0]; rate=s['c5']*2**((nb-49)/12)
    Ev,dt=E.band_env(s['pcm'],rate); top={k:v.max() for k,v in Ev.items()}
    print(p,i,s['name'] if 'name' in s else '', 'len %.3f s'%(len(s['pcm'])/rate),'played dur med %.3f'%np.median([(n['end']-n['start'])/48000 for n in ns]),'rate',int(rate))
    # every 10 ms: level vs band max, as a bar row
    for t in range(0,int(len(s['pcm'])/rate*1000),10):
        j=int(t/1000/dt); row=' '.join('%s%3d'%(k[0],Ev[k][j]-top[k]) for k in ('lo','mid','snr','hi'))
        print('  %4d %s'%(t,row))
