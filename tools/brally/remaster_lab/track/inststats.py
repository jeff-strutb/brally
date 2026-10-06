import numpy as np, collections, sys, pickle
from trkparse import parse
D='build/brally/wasm32/app/extract/disc/tracks/'
def stats(inst):
    M=inst['M']; sc=np.linalg.norm(M[0,:3])
    A=0; up=0; vert=0; cut=0; xlu=0; keys=collections.Counter()
    pts=[]
    for P,UV,k,rm in inst['tris']:
        n=np.cross(P[1]-P[0],P[2]-P[0]); a=np.linalg.norm(n)/2
        if a==0: continue
        nz=abs(n[2])/(2*a)
        A+=a; up+=a*(nz>0.8); vert+=a*(nz<0.3); cut+=a*bool(rm&0x1000); xlu+=a*bool(rm&0x4000)
        keys[k]+=a; pts.append(P)
    if not A: return None
    pts=np.concatenate(pts); dims=(pts.max(0)-pts.min(0))*sc
    return dict(i=inst['i'],ntri=len(inst['tris']),dims=dims.round(2).tolist(),up=up/A,vert=vert/A,cut=cut/A,xlu=xlu/A,
                keys=sorted(keys,key=lambda k:-keys[k]),pos=(M[3,:3]).round(1).tolist(),sc=sc)
if __name__=='__main__':
    tr=sys.argv[1]
    texs,insts=parse(D+tr+'.trk')
    S=[s for s in (stats(i) for i in insts) if s]
    for s in S[:60]: print(s['i'],s['ntri'],s['dims'],'up%.2f vert%.2f cut%.2f xlu%.2f'%(s['up'],s['vert'],s['cut'],s['xlu']),len(s['keys']),s['pos'])
    d=np.array([max(s['dims']) for s in S]); print('max dim pctiles',np.percentile(d,[10,50,90,99]).round(1))
