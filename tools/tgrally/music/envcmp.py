import numpy as np, sys
from render_remaster import load_events
p,i,tr,t0=sys.argv[1],int(sys.argv[2]),sys.argv[3],float(sys.argv[4]); span=float(sys.argv[5]) if len(sys.argv)>5 else 1.2
notes,T=load_events('pieces/%s.csv'%p)
O=dict(np.load('pieces/%s_origstems.npz'%p)); R=dict(np.load('pieces/%s_tracks.npz'%p))
ns=sorted([n for n in notes if n['instr']==i and n['start']>=t0*48000],key=lambda n:n['start']); a=ns[0]['start']
print('notes (t ms, note, dur ms, vol0):',[(int((n['start']-a)/48),n['note'],int((n['end']-n['start'])/48),round(n['ticks'][0][2],2)) for n in ns[:14]])
o=O[str(i)][a:a+int(span*48000)].astype(float); r=R[tr][a:a+int(span*48000)].mean(1); H=480
eo=[20*np.log10(np.sqrt(np.mean(o[k:k+H]**2))+1e-6) for k in range(0,len(o)-H,H)]
er=[20*np.log10(np.sqrt(np.mean(r[k:k+H]**2))+1e-6) for k in range(0,len(r)-H,H)]
print('orig', ' '.join('%3d'%(v-max(eo)) for v in eo))
print('rem ', ' '.join('%3d'%(v-max(er)) for v in er))
