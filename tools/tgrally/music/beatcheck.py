import numpy as np, collections, sys
import engines as E
from classify import inst_samples
from render_remaster import load_events, res
from lagcheck import odf
from piece_render import RECIPES
ROW={'title':.12,'desert':.12,'mountain':.12,'coastline':.12,'stripmine':.06}
for p in sys.argv[1:]:
    m=open('pieces/%s.xm'%p,'rb').read(); im,flat=inst_samples(m); notes,T=load_events('pieces/%s.csv'%p); by=collections.defaultdict(list)
    for n in notes: by[n['instr']].append(n)
    for i,r in RECIPES[p].items():
        if r['t'] not in ('break','beat') or i not in by: continue
        ns=by[i]; s=flat[im[i][collections.Counter(n['sample'] for n in ns).most_common(1)[0][0]]]
        nb=collections.Counter(n['note'] for n in ns).most_common(1)[0][0]; orate=s['c5']*2**((nb-49)/12); o=res(s['pcm'],orate)
        k=np.ones(10); a=np.convolve(odf(o),k,'same')
        y1,h=E.rebuild_beat(s['pcm'],orate,__import__('piece_render').row_points('pieces/%s.csv'%p,[n['start'] for n in ns],len(o)/48000))
        c=lambda y: np.corrcoef(a,np.convolve(odf(y.mean(1)[:len(o)]),k,'same')[:len(a)])[0,1]
        print(p,i,'grid %s'%str(__import__('piece_render').row_points('pieces/%s.csv'%p,[n['start'] for n in ns],len(o)/48000)[:6]),'len %.2f notes %d'%(len(o)/48000,len(ns)),'new %.2f'%c(y1),h)
