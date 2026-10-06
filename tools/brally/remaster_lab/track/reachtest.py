import sys, struct, numpy as np, time, math
sys.path.insert(0,"ports/brally-wasm/tools")
import remaster_env_place as P
tr=sys.argv[1] if len(sys.argv)>1 else "mountain"
b=open(f"build/brally/wasm32/app/extract/disc/tracks/{tr}.trk","rb").read()
rc,rw=P.road_points(b)
t=time.time(); R=P.Reach(b,rc); print("reach",len(R.faces),"faces",round(R.area),"m2", round(time.time()-t,1),"s", len(R.z),"cells")
ia,ci=P.be32(b,0x60),P.be32(b,0x64)
from collections import Counter
A=Counter(); Ar=Counter(); near=Counter()
kinds={}
import random
for i in range(ci):
    r=P.off(ia)+i*0x54
    M=np.array(struct.unpack_from(">16f",b,r),dtype=float).reshape(4,4)
    dl=P.be32(b,r+0x44)
    if not dl: continue
    for _,tex,tris,tf in P.walk(b,P.off(dl)):
        k=(tex,tf)
        if k not in kinds: kinds[k]=P.texture_kind(b,tex,tf)
        for tt in tris:
            w=(np.hstack([np.array(tt,float),np.ones((3,1))])@M)[:,:3]
            n=np.cross(w[1]-w[0],w[2]-w[0]); a=np.linalg.norm(n)/2
            if a<0.05 or n[2]/(2*a)<0.3: continue
            c=w.mean(0); kd=(kinds[k], 'flat' if n[2]/(2*a)>=0.7 else 'slope')
            A[kd]+=a
            if R.reached(c,1.5): Ar[kd]+=a
            else:
                d=np.min(np.linalg.norm(rc[::4,:2]-c[:2],axis=1))
                if d<60: near[kd]+=a
for k in A: print(k, "flat area",round(A[k]),"reached",round(Ar[k]),"unreached within 60m of road",round(near[k]))
