import struct, numpy as np, sys
b=open("build/brally/wasm32/app/extract/disc/tracks/%s.trk"%(sys.argv[1] if len(sys.argv)>1 else "mountain"),"rb").read()
be=lambda o: struct.unpack_from(">I",b,o)[0]
B=0x80025C00
nf,fo,nv,vo,fe=be(8),be(0xC)-B,be(0x10),be(0x14)-B,be(0x94)-B
print(nf,hex(fo),nv,hex(vo),hex(fe))
V=np.frombuffer(b,">f4",nv*3,vo).reshape(-1,3).astype(float)
F=np.frombuffer(b,">u2",nf*4,fo).reshape(-1,4)
fl=np.frombuffer(b,"u1",nf,fe)
print(V.min(0),V.max(0)); print(F[:5]); print(np.unique(fl,return_counts=True))
T=V[F[:,:3]]; n=np.cross(T[:,1]-T[:,0],T[:,2]-T[:,0]); a=np.linalg.norm(n,axis=1); nz=n[:,2]/np.maximum(a,1e-9)
print("nz hist",np.histogram(nz,[-1.01,-0.5,0,0.3,0.5,0.7,0.9,1.01])); print("area", (a/2).sum())
sys.path.insert(0,"ports/brally-wasm/tools")
import remaster_env_place as P
rc,rw=P.road_points(b)
print("road",rc.min(0),rc.max(0),rw.mean())
print("F3",np.unique(F[:,3],return_counts=True))
for f in (0,2,3):
    m=fl==f; print(f, "nz mean",nz[m].mean(), "area",(a[m]/2).sum())
# faces under road points
from collections import Counter
cx=T.mean(1)
# adjacency by shared edge (vertex indices; also by rounded positions)
key=lambda i: tuple(np.round(V[i],2))
edges={}
for fi,(i0,i1,i2,_) in enumerate(F):
    for u,v in ((i0,i1),(i1,i2),(i2,i0)):
        e=tuple(sorted((key(u),key(v)))); edges.setdefault(e,[]).append(fi)
adj=[[] for _ in range(nf)]
for fs in edges.values():
    for x in fs:
        for y in fs:
            if x!=y: adj[x].append(y)
# seed: faces under road points
lo=T[:,:,:2].min(1); hi=T[:,:,:2].max(1)
def under(p):
    m=np.where((lo[:,0]<=p[0])&(hi[:,0]>=p[0])&(lo[:,1]<=p[1])&(hi[:,1]>=p[1]))[0]
    out=[]
    for f in m:
        (ax,ay,az),(bx,by,bz),(cx_,cy,cz)=T[f]
        d=(by-cy)*(ax-cx_)+(cx_-bx)*(ay-cy)
        if abs(d)<1e-9: continue
        l1=((by-cy)*(p[0]-cx_)+(cx_-bx)*(p[1]-cy))/d; l2=((cy-ay)*(p[0]-cx_)+(ax-cx_)*(p[1]-cy))/d
        if min(l1,l2,1-l1-l2)>=-1e-4:
            z=l1*az+l2*bz+(1-l1-l2)*cz
            if abs(z-p[2])<4: out.append(f)
    return out
seed=set()
for p in rc[::5]: seed.update(under(p))
print("seed",len(seed))
for th in (0.15,0.3,0.45):
    R=set(seed); st=list(seed)
    while st:
        f=st.pop()
        for g in adj[f]:
            if g not in R and nz[g]>=th: R.add(g); st.append(g)
    print(th,"reachable faces",len(R),"area",sum(a[f]/2 for f in R))
R=set(seed); st=list(seed)
while st:
    f=st.pop()
    for g in adj[f]:
        if g not in R and nz[g]>=0.15: R.add(g); st.append(g)
W=set(g for f in R for g in adj[f] if g not in R)
hz=np.array([np.ptp(T[g][:,2]) for g in W])
print("boundary walls",len(W),"height pct",np.percentile(hz,[0,5,25,50,75]))
# open edges of reachable region (no neighbour) = mesh border
open_e=0
for e,fs in edges.items():
    if len(fs)==1 and fs[0] in R: open_e+=1
print("open border edges",open_e)
def segd(p,a_,b_):
    ab=b_-a_; t=np.clip(np.dot(p-a_,ab)/max(np.dot(ab,ab),1e-12),0,1); return np.linalg.norm(p-(a_+t*ab))
oe=[(e,fs[0]) for e,fs in edges.items() if len(fs)==1 and fs[0] in R]
ctr=T.mean(1)
near=0; samples=[]
for e,f in oe:
    m=(np.array(e[0])+np.array(e[1]))/2
    cand=np.where(np.linalg.norm(ctr-m,axis=1)<200)[0]
    best=1e9
    for g in cand:
        if g==f: continue
        for u,v in ((0,1),(1,2),(2,0)):
            best=min(best,segd(m,T[g][u],T[g][v]))
    if best<0.1: near+=1
    else: samples.append((m.round(1),round(best,2)))
print("open edges touching another face (T-junction)",near,"of",len(oe)); print(samples[:10])
