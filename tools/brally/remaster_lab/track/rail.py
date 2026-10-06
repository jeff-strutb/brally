import sys, struct, numpy as np
sys.path.insert(0,"ports/brally-wasm/tools")
import remaster_env_place as P
b=open("build/brally/wasm32/app/extract/disc/tracks/mountain.trk","rb").read()
ia,ci=P.be32(b,0x60),P.be32(b,0x64)
G=[]
for i in range(ci):
    r=P.off(ia)+i*0x54
    M=np.array(struct.unpack_from(">16f",b,r),dtype=float).reshape(4,4)
    dl=P.be32(b,r+0x44)
    if not dl: continue
    for o,tex,tris,tf in P.walk(b,P.off(dl)):
        if tex==0x80044D60:
            for t in tris:
                G.append((i,(np.hstack([np.array(t,float),np.ones((3,1))])@M)[:,:3]))
print(len(G))
for i,t in G[:12]: print(i,t.round(2).tolist())
# also uv? print heights
h=[np.ptp(t[:,2]) for _,t in G]; print("tri heights",np.percentile(h,[0,50,100]))
