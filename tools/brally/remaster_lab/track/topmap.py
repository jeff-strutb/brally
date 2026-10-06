import sys, struct, numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0,"ports/brally-wasm/tools")
import remaster_env_place as P
tr=sys.argv[1]; x0,y0,x1,y1=[float(v) for v in sys.argv[2:6]] if len(sys.argv)>5 else (0,200,2050,1700)
out=sys.argv[6] if len(sys.argv)>6 else "build/brally/remaster/lab/top.png"
b=open(f"build/brally/wasm32/app/extract/disc/tracks/{tr}.trk","rb").read()
S=1600/(x1-x0); W,H=int((x1-x0)*S),int((y1-y0)*S)
im=Image.new("RGB",(W,H),(255,255,255)); d=ImageDraw.Draw(im)
T=lambda p:((p[0]-x0)*S,H-(p[1]-y0)*S)
ia,ci=P.be32(b,0x60),P.be32(b,0x64); K={}
col={"grass":(140,200,120),"rock":(170,170,170),"earth":(200,170,120),"foliage":(60,140,60),None:(200,210,240),"sand":(240,230,180)}
for i in range(ci):
    r=P.off(ia)+i*0x54
    M=np.array(struct.unpack_from(">16f",b,r),dtype=float).reshape(4,4)
    dl=P.be32(b,r+0x44)
    if not dl: continue
    for _,tex,tris,tf in P.walk(b,P.off(dl)):
        k=(tex,tf)
        if k not in K: K[k]=P.texture_kind(b,tex,tf)
        for tt in tris:
            w=(np.hstack([np.array(tt,float),np.ones((3,1))])@M)[:,:3]
            n=np.cross(w[1]-w[0],w[2]-w[0]); a=np.linalg.norm(n)
            if a<1e-6 or n[2]/a<0.3: continue
            c=col.get(K[k],(255,0,255))
            if n[2]/a<0.6: c=tuple(int(v*0.75) for v in c)
            d.polygon([T(p) for p in w],fill=c)
rc,rw=P.road_points(b)
for p in rc[::2]: x,y=T(p); d.point((x,y),fill=(0,0,0))
env=open(f"ports/common/models/placements/{tr}.env").read().split("\n")
assets={}
for l in env:
    f=l.split()
    if not f: continue
    if f[0]=="asset": assets[f[1]]=f[2]
    elif f[0]=="put":
        a=assets[f[2]]; m=[float(v) for v in f[3:19]]; x,y=T((m[12],m[13]))
        if (x<0 or y<0 or x>W or y>H): continue
        maxd=len(f)>19
        if a=="guardrail": d.point((x,y),fill=(255,0,0)); d.ellipse((x-1,y-1,x+1,y+1),fill=(255,0,0))
        elif not maxd: d.ellipse((x-1.5,y-1.5,x+1.5,y+1.5),fill=(0,90,0))
        elif a in ("tree_stump_01","tree_stump_02","dead_tree_trunk","dead_tree_trunk_02","boulder_01","rock_moss_set_01","rock_moss_set_02"): d.point((x,y),fill=(120,60,0))
im.save(out); print(W,H)
